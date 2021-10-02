#include "subway_led_panel.h"
#include "ledpanel_buffer.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <libopencm3/cm3/cortex.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/stm32/spi.h>

/* NOTE: We assume that rows outputs are bits 0..2! */
#define ROW_IO_BANK GPIOA
#define ROW_PIN_A0 GPIO0
#define ROW_PIN_A1 GPIO1
#define ROW_PIN_A2 GPIO2
#define ROW_PIN_nE1 GPIO3

#define COL_IO_BANK GPIOA
#define COL_PIN_OE GPIO8
#define COL_PIN_LE GPIO4

/*
 *  IO configuration
 *   row A0    => PA0
 *   row A1    => PA1
 *   row A2    => PA2
 *   row ~E1   => PA3
 *   row E2  -> Vcc for on
 *
 *   col DIn   => MOSI1, PA
 *   col Dout  => MISO1, PA4
 *   col CLK   => SCK1, PA3
 *   col OE    => PA8
 *   col LE    => PA4
 */

/* for timer configuration */
static const unsigned int led_cycles = 8; /* 8 row cycles */
static const unsigned int led_refresh = 30; /* Hz */
static const unsigned int tim2_period = 1000;
static unsigned int tim2_prescaler;

static uint8_t curr_col = 0; /* current column */

void tim2_isr()
{
	timer_clear_flag(TIM2, TIM_SR_UIF);

	gpio_set(ROW_IO_BANK, ROW_PIN_nE1);
	gpio_set(ROW_IO_BANK, ROW_PIN_nE1);
	if (curr_col <= 7) {
		gpio_clear(ROW_IO_BANK, 0x07);
		gpio_set(ROW_IO_BANK, curr_col & 0x07);
		curr_col = (curr_col + 1) & 0x07;
	} else {
		curr_col = 0;
	}

	gpio_clear(COL_IO_BANK, COL_PIN_LE);
	gpio_clear(ROW_IO_BANK, ROW_PIN_nE1);

	/* DMA still running? Shouldn't happen. */
	if (DMA1_CCR(3) & DMA_CCR_EN) {
	}

	DMA1_CCR(3) = 0;
	DMA1_CNDTR(3) = 0; /* make it wait ... */

	DMA1_CMAR(3) = (uint32_t) &ledpanel_buffer[curr_col*LEDPANEL_N_ROW_GROUPS*LEDPANEL_N_COL_DRIVERS];
	DMA1_CPAR(3) = (uint32_t)&SPI1_DR; /* peripheral address register */

	DMA1_CCR(3) = DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_EN | DMA_CCR_TCIE;
	DMA1_CNDTR(3) = /*uint16_t!*/ 2*LEDPANEL_N_ROW_GROUPS*LEDPANEL_N_COL_DRIVERS;
}

void dma1_channel3_isr(void)
{
	DMA1_CCR(3) = 0;
	dma_clear_interrupt_flags(DMA1, DMA_CHANNEL3, DMA_TCIF | DMA_GIF);
}

void subway_led_panel_start()
{
}

void subway_led_panel_init()
{
	/* configure GPIO outputs */
	gpio_set_mode(ROW_IO_BANK, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, ROW_PIN_A0);
	gpio_clear(ROW_IO_BANK, ROW_PIN_A0);
	gpio_set_mode(ROW_IO_BANK, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, ROW_PIN_A1);
	gpio_clear(ROW_IO_BANK, ROW_PIN_A1);
	gpio_set_mode(ROW_IO_BANK, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, ROW_PIN_A2);
	gpio_clear(ROW_IO_BANK, ROW_PIN_A2);
	gpio_set_mode(ROW_IO_BANK, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, ROW_PIN_nE1);
	gpio_clear(ROW_IO_BANK, ROW_PIN_nE1);

	gpio_set_mode(COL_IO_BANK, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, COL_PIN_OE);
	gpio_clear(COL_IO_BANK, COL_PIN_OE);

	gpio_set_mode(COL_IO_BANK, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, COL_PIN_LE);
	gpio_clear(COL_IO_BANK, COL_PIN_LE);

	rcc_periph_clock_enable(RCC_SPI1);
	rcc_periph_clock_enable(RCC_DMA1);

	/* === SPI1 init === */
	/* software slave management, internal slave select, spi enable, master mode, baudrate */
	SPI1_CR1 = SPI_CR1_SSM | SPI_CR1_SSI | SPI_CR1_SPE | SPI_CR1_MSTR |
		   (SPI_CR1_BR_FPCLK_DIV_64 << 3) | SPI_CR1_CPHA;
	SPI1_CR2 = SPI_CR2_TXDMAEN; /* enable DMA */

	/* SCL and MOSI are outputs in alternate mode, MISO is input with weak pullup */
	gpio_set_mode(GPIO_BANK_SPI1_SCK, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_SPI1_SCK);
	gpio_set_mode(GPIO_BANK_SPI1_MOSI, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_SPI1_MOSI);
	gpio_set_mode(GPIO_BANK_SPI1_MISO, GPIO_MODE_INPUT,
		      GPIO_CNF_INPUT_PULL_UPDOWN, GPIO_SPI1_MISO);
	gpio_set(GPIO_BANK_SPI1_MISO, GPIO_SPI1_MISO);

	DMA1_CCR(3) = 0;
	DMA1_CNDTR(3) = 0;
	nvic_enable_irq(NVIC_DMA1_CHANNEL3_IRQ);

	/* === Timer2 init === */
	rcc_periph_clock_enable(RCC_TIM2);
	rcc_periph_reset_pulse(RST_TIM2);
	nvic_enable_irq(NVIC_TIM2_IRQ);

	timer_set_mode(TIM2, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE,
		       TIM_CR1_DIR_UP);

	/* APB1 clock is 36MHz, but there is the TIMXCLK which
	   is APB1 clock x2 if the APB prescaler is used,
	   see STM32F1xx ref manual, Low-, medium-, high- and
	   XL-density reset and clock control (RCC),
	   page 93/1134 */

	tim2_prescaler = (rcc_apb1_frequency * 2) /
			 (tim2_period * led_refresh * led_cycles);
	timer_set_prescaler(TIM2,
			    tim2_prescaler); /* 36MHz * 2 / 36'000 = 2kHz  */
	timer_set_period(TIM2, tim2_period); /* 2kHz / 200 = 10 Hz overflow */
	timer_enable_counter(TIM2);
	timer_enable_irq(TIM2, TIM_DIER_UIE);
}

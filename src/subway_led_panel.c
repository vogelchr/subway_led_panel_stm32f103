/*
 */

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

#define COL_IO_BANK GPIOA
#define COL_PIN_OE GPIO8
#define COL_PIN_LE GPIO4

/* for timer configuration */
static const unsigned int led_cycles = 8; /* 8 row cycles */
static const unsigned int led_refresh = 60; /* Hz */
static const unsigned int tim2_period = 1000;
static unsigned int tim2_prescaler;

static uint8_t curr_col = 0; /* current column */

/*
 * Timer2 overflow. Note that we generate ROW_nE1 via PWM,
 * so that nE1 goes high (turns off driver) at the same time
 * that the tim2_isr() fires, and will go low (turns on driver
 * again) depending on the oc_value below in init.
 */
void tim2_isr()
{
	/* disable row and column output drivers */
	gpio_clear(COL_IO_BANK, COL_PIN_OE);

	if (curr_col < 8) {
		/* latch data from shiftregs to column driver out */
		gpio_set(COL_IO_BANK, COL_PIN_LE);

		/* set row select pins for the row that has been
		   transfered before */
		gpio_clear(ROW_IO_BANK, 0x0007 & ~curr_col);
		gpio_set(ROW_IO_BANK, 0x0007 & curr_col);

		/* enable row and column output drivers */
		gpio_set(COL_IO_BANK, COL_PIN_OE);

		/* next row to be transfered: */
		curr_col = (curr_col + 1) & 7;
	} else {
		/* special handling for the first row that's ever
		transfered, there's not yet valid data in the
		column drivers, so don't enable the outputs */
		curr_col = 0;
	}

	/* prepare bits to send to the column driver in correct order */
	ledpanel_buffer_prepare_shiftreg(curr_col);

	/* deassert latch enable pin */
	gpio_clear(COL_IO_BANK, COL_PIN_LE);

	/* restart DMA to transfer SPI data */
	DMA1_CCR(3) = 0;
	DMA1_CNDTR(3) = 0;

	DMA1_CMAR(3) = (uint32_t)&ledpanel_buffer_shiftreg; /* memory */
	DMA1_CPAR(3) = (uint32_t)&SPI1_DR; /* peripheral */

	DMA1_CCR(3) = DMA_CCR_MINC | DMA_CCR_DIR | DMA_CCR_EN;
	DMA1_CNDTR(3) = sizeof(ledpanel_buffer_shiftreg);

	timer_clear_flag(TIM2, TIM_SR_UIF);
}

void subway_led_panel_start()
{
	/* GPIO GPIOA3 is Timer/Counter 2, Channel 4 */
	timer_enable_oc_output(TIM2, TIM_OC4);
	gpio_set_mode(GPIO_BANK_TIM2_CH4, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_TIM2_CH4);
	timer_enable_irq(TIM2, TIM_DIER_UIE);
}

void subway_led_panel_stop()
{
	timer_disable_irq(TIM2, TIM_DIER_UIE);

	/* GPIO GPIOA3 is Timer/Counter 2, Channel 4 */
	gpio_set_mode(GPIO_BANK_TIM2_CH4, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO_TIM2_CH4);
	gpio_set(GPIO_BANK_TIM2_CH4, GPIO_TIM2_CH4);
	timer_disable_oc_output(TIM2, TIM_OC4);
}

/*
 *  MBI5029 datasheet: switching to special mode
 *
 *         1__   2__   3__   4__   5__
 *  CLK ___/  \__/  \__/  \__/  \__/  \___
 *            :     :     :     :     :
 *      ______:     :_____________________
 *  nOE       \_____/     :     :     :
 *            :     :     :_____:     :
 *  LE  __________________/_____\_________
 *                           ^
 *                           |
 *                 1: special, 0: normal
 */

void subway_led_panel_mbi5029_mode(int special)
{
	unsigned int i, u;
	unsigned int nOE_steps[] = { 1, 0, 1, 1, 1 };
	unsigned int LE_steps[] = { 0, 0, 0, 0, 1 };

	LE_steps[3] = !!special;

	/* SCK in bit-banging mode */
	gpio_set_mode(GPIO_BANK_SPI1_SCK, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO_SPI1_SCK);

	gpio_set(COL_IO_BANK, COL_PIN_OE);
	gpio_clear(COL_IO_BANK, COL_PIN_LE);

	for (i=0; i<5; i++) {
		if (nOE_steps[i])
			gpio_set(COL_IO_BANK, COL_PIN_OE);
		else
			gpio_clear(COL_IO_BANK, COL_PIN_OE);

		if (LE_steps[i])
			gpio_set(COL_IO_BANK, COL_PIN_LE);
		else
			gpio_clear(COL_IO_BANK, COL_PIN_LE);

		gpio_clear(GPIO_BANK_SPI1_SCK, GPIO_SPI1_SCK);
		for (u=0; u<256; u++)
			asm volatile ("nop");
		gpio_set(GPIO_BANK_SPI1_SCK, GPIO_SPI1_SCK);
		for (u=0; u<256; u++)
			asm volatile ("nop");
	}

	/* SPI mode again */
	gpio_set_mode(GPIO_BANK_SPI1_SCK, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_SPI1_SCK);
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

	/*
	   depending on whether the panel is refreshed or not
	   we change TIM2_CH4 to either timer compare output
	   (brighness PWM) or "normal" gpio, which is "1" for
	   panel OFF
	*/
	gpio_set_mode(GPIO_BANK_TIM2_CH4, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO_TIM2_CH4);
	gpio_set(GPIO_BANK_TIM2_CH4, GPIO_TIM2_CH4);

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

	/* CLK and MOSI are outputs in alternate mode, MISO is input with weak pullup */
	gpio_set_mode(GPIO_BANK_SPI1_SCK, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_SPI1_SCK);
	gpio_set_mode(GPIO_BANK_SPI1_MOSI, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_SPI1_MOSI);
	gpio_set_mode(GPIO_BANK_SPI1_MISO, GPIO_MODE_INPUT,
		      GPIO_CNF_INPUT_PULL_UPDOWN, GPIO_SPI1_MISO);
	gpio_set(GPIO_BANK_SPI1_MISO, GPIO_SPI1_MISO);

	DMA1_CCR(3) = 0;
	DMA1_CNDTR(3) = 0;

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

	/* TImer2, CH2 on PA4 */
	timer_set_oc_mode(TIM2, TIM_OC4, TIM_OCM_PWM1);
	/* on for only a fifth of the time */
	timer_set_oc_value(TIM2, TIM_OC4, (4 * tim2_period) / 5);
	timer_set_oc_polarity_high(TIM2, TIM_OC4);

	timer_enable_counter(TIM2);
	subway_led_panel_start();
}

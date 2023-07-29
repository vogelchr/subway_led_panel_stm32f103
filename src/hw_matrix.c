/*
 * This file is part of subway_led_panel_stm32f103, originally
 * distributed at https://github.com/vogelchr/subway_led_panel_stm32f103.
 *
 *     Copyright (c) 2021 Christian Vogel <vogelchr@vogel.cx>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "hw_matrix.h"
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
static const unsigned int led_refresh = 250; /* Hz */
static const unsigned int tim2_period = 1000;
static unsigned int tim2_prescaler;

static uint8_t curr_row = 0; /* current row */

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

	if (curr_row < 8) {
		/* latch data from shiftregs to column driver out */
		gpio_set(COL_IO_BANK, COL_PIN_LE);

		/* set row select pins for the row that has been
		   transfered before */
		gpio_clear(ROW_IO_BANK, 0x0007 & ~curr_row);
		gpio_set(ROW_IO_BANK, 0x0007 & curr_row);

		/* enable row and column output drivers */
		gpio_set(COL_IO_BANK, COL_PIN_OE);

		/* next row to be transfered: */
		curr_row = (curr_row + 1) & 7;
	} else {
		/* special handling for the first row that's ever
		transfered, there's not yet valid data in the
		column drivers, so don't enable the outputs */
		curr_row = 0;
	}

	/* prepare bits to send to the column driver in correct order */
	ledpanel_buffer_prepare_shiftreg(curr_row);

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

void hw_matrix_start()
{
	hw_matrix_mbi5029_mode(0);

	/* GPIO GPIOA3 is Timer/Counter 2, Channel 4 */
	timer_enable_oc_output(TIM2, TIM_OC4);
	gpio_set_mode(GPIO_BANK_TIM2_CH4, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_TIM2_CH4);
	timer_enable_irq(TIM2, TIM_DIER_UIE);
}

void hw_matrix_stop()
{
	timer_disable_irq(TIM2, TIM_DIER_UIE);

	/* GPIO GPIOA3 is Timer/Counter 2, Channel 4 */
	gpio_set_mode(GPIO_BANK_TIM2_CH4, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO_TIM2_CH4);
	gpio_set(GPIO_BANK_TIM2_CH4, GPIO_TIM2_CH4);
	timer_disable_oc_output(TIM2, TIM_OC4);

	hw_matrix_mbi5029_mode(1);
}

void hw_matrix_pwm(unsigned char brightness)
{
	/* max 1/257 of tim2_period */
	/* min 256/256 of tim2_period */

	unsigned int upper_edge = (256 - (unsigned int)brightness) * tim2_period / 257;
	timer_set_oc_value(TIM2, TIM_OC4, upper_edge);
}

/*
 *  MBI5029 datasheet: switching to special mode
 *
 *         1__   2__   3__   4__   5__
 *  CLK ___/  \__/  \__/  \__/  \__/  \___
 *      :     :     :     :     :     :
 *      ______:     :_____________________
 *  nOE :     \_____/     :     :     :
 *      :     :     :     :_____:     :
 *  LE  __________________/_____\_________
 *      :     :     :     :  ^  :     :
 *      :     :     :     :  |  :     :
 *      :     :     :     :     :     :
 *       [0]   [1]   [2]   [3]   [4]
 *
 *  During the 4th clock cycle (index [3]), if LE
 *  is high, we are in the special mode, if LE is
 *  low, we are in the normal mode.
 *
 *  Data is latched into the chip on the rising edge.
 *
 *  Note that there is an inverter between our
 *  output pins, and the input to the chip (on the
 *  LED matrix board itself), so we have to set
 *  "nOE_inv" high.
 */

void hw_matrix_mbi5029_mode(int special)
{
	unsigned int i, u;
	unsigned int nOE_inv_steps[] = { 0, 1, 0, 0, 0 };
	unsigned int LE_steps[] = { 0, 0, 0, 1, 0 };

	LE_steps[3] = !!special;

	/* SCK in bit-banging mode */
	gpio_clear(GPIO_BANK_SPI1_SCK, GPIO_SPI1_SCK);
	gpio_set_mode(GPIO_BANK_SPI1_SCK, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO_SPI1_SCK);

	gpio_clear(COL_IO_BANK, COL_PIN_OE);
	gpio_clear(COL_IO_BANK, COL_PIN_LE);

	for (i=0; i<5; i++) {
		if (nOE_inv_steps[i])
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

void hw_matrix_brightness(unsigned int brightness)
{
	unsigned int bitno, chipno, u;

	/* SCK in bit-banging mode */
	gpio_set_mode(GPIO_BANK_SPI1_SCK, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO_SPI1_SCK);
	gpio_set_mode(GPIO_BANK_SPI1_MOSI, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_PUSHPULL, GPIO_SPI1_MOSI);

	gpio_clear(GPIO_BANK_SPI1_SCK, GPIO_SPI1_SCK);
	gpio_clear(GPIO_BANK_SPI1_MOSI, GPIO_SPI1_MOSI);

	for (chipno = 0; chipno < LEDPANEL_SPI_BYTES / 2; chipno++) {
		for (bitno = 0; bitno < 16; bitno++) {
			if (chipno == LEDPANEL_SPI_BYTES / 2 - 1 &&
			    bitno == 15) {
				gpio_set(COL_IO_BANK, COL_PIN_LE);
			}
			if (brightness & (1<<bitno)) {
				gpio_set(GPIO_BANK_SPI1_MOSI, GPIO_SPI1_MOSI);
			} else {
				gpio_clear(GPIO_BANK_SPI1_MOSI, GPIO_SPI1_MOSI);
			}
			for (u=0; u<32; u++)
				asm volatile ("nop");
			gpio_set(GPIO_BANK_SPI1_SCK, GPIO_SPI1_SCK);
			for (u=0; u<32; u++)
				asm volatile ("nop");
			gpio_clear(GPIO_BANK_SPI1_SCK, GPIO_SPI1_SCK);
		}
	}
	gpio_clear(COL_IO_BANK, COL_PIN_LE);

	/* SPI mode again */
	gpio_set_mode(GPIO_BANK_SPI1_SCK, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_SPI1_SCK);
	gpio_set_mode(GPIO_BANK_SPI1_MOSI, GPIO_MODE_OUTPUT_10_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_SPI1_MOSI);
}

void hw_matrix_init()
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
		   (SPI_CR1_BR_FPCLK_DIV_64 << 3) | SPI_CR1_CPHA | SPI_CR1_LSBFIRST;
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
	timer_set_oc_polarity_high(TIM2, TIM_OC4);

	hw_matrix_pwm(64); /* about 25% brightness */

	timer_enable_counter(TIM2);
	hw_matrix_start();
}

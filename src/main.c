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
#include "usb_if.h"

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

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

static uint32_t systick;

void sys_tick_handler()
{
	systick++;
	if (systick >= 9) {
		gpio_clear(GPIOC, GPIO13);
		systick = 0;
	} else if (systick == 2) {
		gpio_set(GPIOC, GPIO13);
	}
}

/* debug LEDs, just to entertain the user... */

static uint16_t debug_ctr;
static uint16_t debug_led_pattern_ctr;
uint16_t debug_led_pattern[] = {
	0x8000,
	0x4000,
	0x2000,
	0x1000,
	0x1000,
	0x2000,
	0x4000,
	0x8000
};

int main(void)
{
	/* === system clock initialization ===
	   external 8MHz XTAL, SYSCLK=9(pll)*8MHz=72MHz, AHB 72MHz(max),
	   ADC 9MHz(14MHz max), APB1=36MHz(max), APB2=72MHz(max),
	   flash has 2 waitstates  */
	rcc_clock_setup_in_hse_8mhz_out_72mhz();

	/* === disable JTAG, enable SWD === */
	rcc_periph_clock_enable(RCC_AFIO);
	AFIO_MAPR |= AFIO_MAPR_SWJ_CFG_JTAG_OFF_SW_ON;

	/* we'll definitely need all three GPIO banks */
	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_GPIOC);

	/* debug LEDs PB12, 13, 14, 15 */
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,
		      0xf000);

	/* systick handler */
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
	systick_set_frequency(10, rcc_ahb_frequency);
	systick_counter_enable();
	systick_interrupt_enable();
	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,
		      GPIO13);
	gpio_clear(GPIOC, GPIO13);

	/* PCB modification, USB D+ Pullup connected to PB11 */
	gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL,
		      0x0800);
	gpio_set(GPIOB, 0x0800); /* PB11 */

	usb_if_init();

	ledpanel_buffer_init();
	hw_matrix_init();
	hw_matrix_start();

	while (1) {
		if(!debug_ctr++) {
			if (++debug_led_pattern_ctr >= ARRAY_SIZE(debug_led_pattern)) {
				debug_led_pattern_ctr=0;
			}
			gpio_set(GPIOB, 0xf000 & debug_led_pattern[debug_led_pattern_ctr]);
			gpio_clear(GPIOB, 0xf000 & ~debug_led_pattern[debug_led_pattern_ctr]);
		};

		usb_if_poll();
	}
}

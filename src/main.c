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


	/* systick handler */
	systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
	systick_set_frequency(10, rcc_ahb_frequency);
	systick_counter_enable();
	systick_interrupt_enable();
	gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_10_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
        gpio_clear(GPIOC, GPIO13);

	ledpanel_buffer_init();
	subway_led_panel_init();
	subway_led_panel_start();

	while (1) {
	}
}

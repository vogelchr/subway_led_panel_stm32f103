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

#include "ledpanel_buffer.h"

#include <string.h>

uint32_t ledpanel_buffer[LEDPANEL_U32_PITCH * LEDPANEL_PIX_HEIGHT];
uint8_t ledpanel_buffer_shiftreg[LEDPANEL_SPI_BYTES];

static void memcpy_reverse(uint8_t *restrict dst, uint8_t *restrict src,
				   size_t len)
{
	uint8_t *p = dst + (len - 1);

	while (len--)
		*p-- = *src++;
}

/*
 * This function is the most panel specific in the whole codebase.
 * It translates from the ...
 *  "normal screen pixel order" framebuffer ledpanel_buffer[] to the
 *  "shiftregister pixel order" ledpanel_buffer_shiftreg[] which gets
 * sent out by the SPI peripheral.
 *
 * I haven't bothered to add constants for the various magic
 * numbers, as they all only should be used here.
 *
 * Rowaddr is the row-address selection A0..A2 for the panel hardware
 * and the function will fill the shiftregister matching this address
 * configuration.
 */

void ledpanel_buffer_prepare_shiftreg(unsigned int rowaddr)
{
	uint8_t *dst = ledpanel_buffer_shiftreg;
	unsigned int s;
#if LEDPANEL_TYPE_TRIPLE
	unsigned int p;
#endif

	/*
	 * write output for the three "strips" consisting of
	 * pixel data for row, row+8 and row+16. As we only
	 * have 20 rows in total, we fill dummy data for the
	 * third strip, if rowaddr > 3.
	 *
	 * As usual, we first write out bits for the "later"
	 * strip, so we count s=2, s=1, s=0.
	 */

	for (s = 2; s != (unsigned int)~0; s--) {
		uint8_t *src;

		if (s == 2 && rowaddr > 3) /* last stripe only has 4 rows */
			src = NULL;
		else
			/* 3 stripes, with 8 pixel rows offset */
			src = (uint8_t *)&ledpanel_buffer[LEDPANEL_U32_PITCH *
							  (rowaddr + s * 8)];

#if LEDPANEL_TYPE_TRIPLE
		/*
		 * Full panel has three modules (p=2, p=1, p=0).
		 * First copy data for the rightmost, then middle, then left.
		 * Copy in 8 dummy pixels first (8 last outputs or 3rd
		 * column driver). Horzontal offset in framebuffer is
		 * 5 bytes (40 pixel) for each module.
		 */
		for (p=2; p != (unsigned int)~0; p--) {
			*dst++ = '\0';
			if (src)
				memcpy_reverse(dst, src + p*5, 5);
			dst += 5;
		}
#else
		/*
		 * Single module for testing, copy in the bits for the
		 * unconnected output first, then 40 bits (5 bytes) in
		 * reverse order.
		 */
		*dst++ = '\0';
		if (src)
			memcpy_reverse(dst, src, 5);
		dst += 5;
#endif
	}
}

void ledpanel_buffer_init()
{
	unsigned int x;
	unsigned int y;

	memset(ledpanel_buffer, '\0', sizeof(ledpanel_buffer));

	for (y = 0; y < LEDPANEL_PIX_HEIGHT; y++) {
		for (x = 0; x < LEDPANEL_PIX_WIDTH; x++) {
			if (((x % 5) == 0) || ((y % 5) == 0))
				LEDPANEL_SET(x, y);
		}
	}
}

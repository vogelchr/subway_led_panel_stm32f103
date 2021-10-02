#include "mtwister.h"
#include "ledpanel_buffer.h"

#include <string.h>

uint32_t ledpanel_buffer[LEDPANEL_U32_PITCH * LEDPANEL_PIX_HEIGHT];
uint8_t ledpanel_buffer_shiftreg[LEDPANEL_SPI_BYTES];

static MTRand ledpanel_buffer_mt;

static uint8_t bitflip(uint8_t v)
{
	v = ((v & 0x0f) << 4) | ((v & 0xf0) >> 4);
	v = ((v & 0x33) << 2) | ((v & 0xcc) >> 2);
	v = ((v & 0x55) << 1) | ((v & 0xaa) >> 1);
	return v;
}

static void memcpy_bitflip_reverse(uint8_t *restrict dst, uint8_t *restrict src,
				   size_t len)
{
	uint8_t *p = dst + (len - 1);

	while (len--)
		*p-- = *src++;
}

void ledpanel_buffer_prepare_shiftreg(unsigned int rowaddr)
{
	uint8_t *dst = ledpanel_buffer_shiftreg;
	unsigned int s, p;

	/* this is now very specific to the single matrix */
	/* we have 3x (16+16+8 pixels, 8 space) */

	s = 3;
	while (s) {
		s--; /* s = 2, 1, 0 */
		uint8_t *src = NULL;

		if (s >= 2 && rowaddr >= 4) /* out of visible range */
			src = NULL;
		else {
			unsigned int src_offs;
			src_offs = LEDPANEL_U32_PITCH * (rowaddr + s * 8);
			src = (uint8_t *)&ledpanel_buffer[src_offs];
		}

		for (p = 0; p < 1; p++) { /* panels, now: only 1 */
			*dst++ = '\0'; /* 8 pixels right of last col */
			/* 40 pixels on 3 column drivers: 5 bytes */
			if (src)
				memcpy_bitflip_reverse(dst, src, 5);
			else
				memset(dst, 0xa5, 5);
			dst += 5;
			src += sizeof(uint32_t) * LEDPANEL_U32_PITCH;
		}
	}

#if 0
	m_seedRand(&ledpanel_buffer_mt, rowaddr+1);

	for (u=0; u<sizeof(ledpanel_buffer_shiftreg); u++) {
		if ((u % sizeof(unsigned long)) == 0)
			r = genRandLong(&ledpanel_buffer_mt);
		ledpanel_buffer_shiftreg[u] = r & 0xff;
		r >>= 8;
	}
#endif
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

void ledpanel_buffer_update(unsigned int rowmode, unsigned int row_or_col)
{
	unsigned int x, y;

	memset(ledpanel_buffer, '\0', sizeof(ledpanel_buffer));

	if (rowmode) {
		if (row_or_col >= LEDPANEL_PIX_HEIGHT)
			return;
		y = row_or_col;
		for (x = 0; x < LEDPANEL_PIX_WIDTH; x++)
			LEDPANEL_SET(x, y);
		return;
	}

	if (row_or_col >= LEDPANEL_PIX_WIDTH)
		return;
	x = row_or_col;
	for (y = 0; y < LEDPANEL_PIX_HEIGHT; y++)
		LEDPANEL_SET(x, y);
}

#ifndef LEDPANEL_BUFFER_H
#define LEDPANEL_BUFFER_H

#include <stdint.h>
#include <stdlib.h>

/*
  select configuration, either one matrix module, or
  the complete sign (using three matrix modules chained).
*/

#ifdef LEDPANEL_TYPE_SINGLE /* see platformio.ini */

#define LEDPANEL_PIX_WIDTH 40
#define LEDPANEL_PIX_HEIGHT 20
#define LEDPANEL_SPI_BYTES 18 // 9 shifregisters in total

#else
#ifdef LEDPANEL_TYPE_TRIPLE

#define LEDPANEL_PIX_WIDTH 120
#define LEDPANEL_PIX_HEIGHT 20
#define LEDPANEL_SPI_BYTES 54 // 27 shifregisters in total

#else
#error You need to define either LEDPANEL_TYPE_SINGLE or LEDPANEL_TYPE_TRIPLE!
#endif
#endif

#define LEDPANEL_U8_PITCH ((LEDPANEL_PIX_WIDTH + 7) / 8)

/*
 * ledpanel_buffer is our framebuffer in memory, consisting of 32 bit words.
 *
 * The first pixel of the first row (upper left) is LSB of word 0.
 * Lines are 32bit aligned, so for a 120 pixel wide framebuffer, we
 * use 4 32-bit words (128 bits), and with 20 lines used, our total
 * framebuffer size is 4 * 20 = 80 words, 320 bytes.
 */

#define LEDPANEL_WORD(x, y)                                                    \
	(ledpanel_buffer[((x) / 8) + LEDPANEL_U8_PITCH * (y)])
#define LEDPANEL_BIT(x) (1 << (7-((x) % 8)))

#define LEDPANEL_GET(x, y) (LEDPANEL_WORD((x), (y)) & LEDPANEL_BIT(x))
#define LEDPANEL_SET(x, y)                                                     \
	do {                                                                   \
		LEDPANEL_WORD((x), (y)) |= LEDPANEL_BIT(x);                    \
	} while (0)
#define LEDPANEL_CLR(x, y)                                                     \
	do {                                                                   \
		LEDPANEL_WORD((x), (y)) &= ~LEDPANEL_BIT(x);                   \
	} while (0)

extern uint8_t ledpanel_buffer[LEDPANEL_U8_PITCH * LEDPANEL_PIX_HEIGHT];

/* hw specific buffer of pixels, to be written out by the SPI hardware */
extern uint8_t ledpanel_buffer_shiftreg[LEDPANEL_SPI_BYTES];

/* copy the pixels corresponding to row-driver address 'rowaddr' from
 * the global ledpanel_buffer to the global ledpanel_buffer_shiftreg,
 * this function is highly hw dependent, and will be called from the
 * ISR for the timer running the panel refresh! */
extern void ledpanel_buffer_prepare_shiftreg(unsigned int rowaddr);

/* clean ledpanel_buffer, will initialize the display with a 5x5 grid */
extern void ledpanel_buffer_init(void);

#endif

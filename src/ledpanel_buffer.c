#include "mtwister.h"
#include "ledpanel_buffer.h"

uint32_t ledpanel_buffer[LEDPANEL_U32_PITCH*LEDPANEL_PIX_HEIGHT];
uint8_t ledpanel_buffer_shiftreg[LEDPANEL_SPI_BYTES];

static MTRand ledpanel_buffer_mt;

void ledpanel_buffer_prepare_shiftreg(unsigned int rowaddr) {
	size_t u;
	unsigned long r = 0xffffffffUL;

	m_seedRand(&ledpanel_buffer_mt, rowaddr+1);

	for (u=0; u<sizeof(ledpanel_buffer_shiftreg); u++) {
		if ((u % sizeof(unsigned long)) == 0)
			r = genRandLong(&ledpanel_buffer_mt);
		ledpanel_buffer_shiftreg[u] = r & 0xff;
		r >>= 8;
	}

}

void
ledpanel_buffer_init()
{
	uint16_t i;

	for (i=0; i<LEDPANEL_U32_PITCH*LEDPANEL_PIX_HEIGHT; i++)
		ledpanel_buffer[i] = i | (0xff00 & ~(i <<8));
}
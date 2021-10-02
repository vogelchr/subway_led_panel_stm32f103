#include "ledpanel_buffer.h"

uint16_t ledpanel_buffer[LEDPANEL_N_ROW_STRIPES*LEDPANEL_N_ROW_GROUPS*LEDPANEL_N_COL_DRIVERS];

void
ledpanel_buffer_init()
{
	uint16_t i;

	for (i=0; i<LEDPANEL_N_ROW_STRIPES*LEDPANEL_N_ROW_GROUPS*LEDPANEL_N_COL_DRIVERS; i++)
		ledpanel_buffer[i] = i | (0xff00 & ~(i <<8));
}
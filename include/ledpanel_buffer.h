#ifndef LEDPANEL_BUFFER_H
#define LEDPANEL_BUFFER_H

#include <stdint.h>
#include <stdlib.h>

#define LEDPANEL_PIX_WIDTH     40
#define LEDPANEL_PIX_HEIGHT    20
#define LEDPANEL_SPI_BYTES     18    // 9 shifregisters in total

#define LEDPANEL_U32_PITCH   ((LEDPANEL_PIX_WIDTH+31)/32)
#define LEDPANEL_U8_PITCH    ((LEDPANEL_PIX_WIDTH+7)/8)

#define LEDPANEL_WORD(x, y)  (ledpanel_buffer[((x) / 32) + LEDPANEL_U32_PITCH * (y)])
#define LEDPANEL_BIT(x)   (1<<((x)%32))

#define LEDPANEL_GET(x,y)  (LEDPANEL_WORD((x), (y)) & LEDPANEL_BIT(x))
#define LEDPANEL_SET(x,y)    do { LEDPANEL_WORD((x),(y)) |= LEDPANEL_BIT(x); } while(0)
#define LEDPANEL_CLR(x,y)    do { LEDPANEL_WORD((x),(y)) &= ~ LEDPANEL_BIT(x); } while(0)

extern uint32_t ledpanel_buffer[LEDPANEL_U32_PITCH*LEDPANEL_PIX_HEIGHT];
extern uint8_t ledpanel_buffer_shiftreg[LEDPANEL_SPI_BYTES];

extern void ledpanel_buffer_prepare_shiftreg(unsigned int rowaddr);
extern void ledpanel_buffer_init(void);
extern void ledpanel_buffer_update(unsigned int rowmode, unsigned int row_or_col);

#endif

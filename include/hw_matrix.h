#ifndef HW_MATRIX_H
#define HW_MATRIX_H

extern void hw_matrix_init(void);  /* initialize GPIOs, setup SPI, DMA, ... */
extern void hw_matrix_stop(void);  /* stop regular scanning (turn off LED matrix) */
extern void hw_matrix_start(void); /* start regular scanning (turn on LED matrix) */
extern void hw_matrix_brightness(unsigned int brightness);

#endif

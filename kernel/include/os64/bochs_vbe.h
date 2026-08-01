#ifndef OS64_BOCHS_VBE_H
#define OS64_BOCHS_VBE_H
#include <stdint.h>
int bochs_vbe_available(void);
int bochs_vbe_set_mode(uint16_t width,uint16_t height,uint16_t bpp);
#endif

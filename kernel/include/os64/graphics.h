#ifndef OS64_GRAPHICS_H
#define OS64_GRAPHICS_H
#include <stddef.h>
int graphics_init(void);
void graphics_poll(void);
void graphics_stop(void);
int graphics_ready(void);
unsigned long graphics_frames(void);
size_t graphics_width(void);
size_t graphics_height(void);
#endif

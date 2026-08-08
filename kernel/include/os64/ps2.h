#ifndef OS64_PS2_H
#define OS64_PS2_H

#include <stdint.h>

/* Experimental i8042/PS/2 controller transport. */
int ps2_controller_init(void);
int ps2_keyboard_initialize(void);
int ps2_keyboard_command(uint8_t command);
int ps2_data_available(void);
uint8_t ps2_read_data(void);
int ps2_mouse_initialize(void);
void ps2_mouse_shutdown(void);
int ps2_mouse_poll(int *delta_x, int *delta_y, unsigned *buttons);
int ps2_controller_ready(void);
int ps2_keyboard_ready(void);
int ps2_mouse_ready(void);

#endif

#ifndef OS64_ANSI_TERMINAL_H
#define OS64_ANSI_TERMINAL_H
#include <stdint.h>
void ansi_init(void);void ansi_reset(void);void ansi_putchar(unsigned char byte);void ansi_write(const char *text);
void ansi_set_fg(uint8_t color);void ansi_set_bg(uint8_t color);void ansi_move_cursor(unsigned row,unsigned column);void ansi_clear(void);
void ansi_draw_box(unsigned x,unsigned y,unsigned width,unsigned height,int double_line);void ansi_draw_panel(unsigned x,unsigned y,unsigned width,unsigned height,const char *title);void ansi_draw_progress(unsigned x,unsigned y,unsigned width,unsigned percent);void ansi_flush(void);
#endif

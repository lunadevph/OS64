#ifndef OS64_TERMINAL_H
#define OS64_TERMINAL_H
#include <stdint.h>
#include "app_abi.h"
typedef struct os_terminal os_terminal_t;
enum os_terminal_color{OS_COLOR_BLACK,OS_COLOR_RED,OS_COLOR_GREEN,OS_COLOR_YELLOW,OS_COLOR_BLUE,OS_COLOR_MAGENTA,OS_COLOR_CYAN,OS_COLOR_WHITE,OS_COLOR_BRIGHT_BLACK,OS_COLOR_BRIGHT_RED,OS_COLOR_BRIGHT_GREEN,OS_COLOR_BRIGHT_YELLOW,OS_COLOR_BRIGHT_BLUE,OS_COLOR_BRIGHT_MAGENTA,OS_COLOR_BRIGHT_CYAN,OS_COLOR_BRIGHT_WHITE};
int os_init(const os64_api_t *api);os_terminal_t *os_terminal_create(void);void os_terminal_destroy(os_terminal_t *terminal);
void os_terminal_clear(os_terminal_t *terminal);void os_terminal_set_color(os_terminal_t *terminal,uint8_t foreground,uint8_t background);void os_terminal_put(os_terminal_t *terminal,unsigned x,unsigned y,uint32_t codepoint);void os_terminal_write(os_terminal_t *terminal,unsigned x,unsigned y,const char *utf8);
void os_terminal_box(os_terminal_t *terminal,unsigned x,unsigned y,unsigned width,unsigned height,int double_line);void os_terminal_panel(os_terminal_t *terminal,unsigned x,unsigned y,unsigned width,unsigned height,const char *title);void os_terminal_progress(os_terminal_t *terminal,unsigned x,unsigned y,unsigned width,unsigned percent);void os_terminal_statusbar(os_terminal_t *terminal,const char *text);void os_terminal_flush(os_terminal_t *terminal);int os_terminal_run(os_terminal_t *terminal);
#endif

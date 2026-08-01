#ifndef OS64_SDK_TUI_H
#define OS64_SDK_TUI_H
#include "terminal.h"
typedef struct os_window os_window_t;typedef struct os_widget os_widget_t;
os_window_t *os_window_create(os_terminal_t *terminal,int x,int y,int width,int height,const char *title);os_widget_t *os_label(os_window_t *window,int x,int y,const char *text);os_widget_t *os_progress(os_window_t *window,int x,int y,int width,unsigned percent);void os_statusbar(os_terminal_t *terminal,const char *text);
#endif

#ifndef OS64_COMPOSITOR_H
#define OS64_COMPOSITOR_H
#include <stdint.h>
typedef struct compositor_rect{int x,y,w,h;}compositor_rect_t;
void compositor_init(unsigned width,unsigned height);
void compositor_desktop(void);
void compositor_panel(unsigned window_count,unsigned focused_id,int tiled,int mouse_ready);
void compositor_task(unsigned position,const char*title,int focused);
compositor_rect_t compositor_window(int x,int y,int width,int height,const char*title,int focused);
void compositor_text(int x,int y,const char*text,unsigned columns,uint32_t foreground,uint32_t background);
void compositor_fill(int x,int y,int width,int height,uint32_t color);
#endif

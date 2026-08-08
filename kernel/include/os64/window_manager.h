#ifndef OS64_WINDOW_MANAGER_H
#define OS64_WINDOW_MANAGER_H
#define WM_MAX_WINDOWS 6
typedef enum wm_layout{WM_LAYOUT_TILED,WM_LAYOUT_FLOATING}wm_layout_t;
typedef struct wm_window{unsigned id;int x,y,width,height;int used,focused;char title[32];void*client;}wm_window_t;
typedef struct window_manager{unsigned screen_width,screen_height,next_id;wm_layout_t layout;wm_window_t windows[WM_MAX_WINDOWS];}window_manager_t;
void wm_init(window_manager_t*wm,unsigned width,unsigned height);
wm_window_t*wm_create(window_manager_t*wm,const char*title,void*client);
void wm_close(window_manager_t*wm,unsigned id);
void wm_focus(window_manager_t*wm,unsigned id);
void wm_focus_next(window_manager_t*wm);
void wm_toggle_layout(window_manager_t*wm);
void wm_move(window_manager_t*wm,unsigned id,int x,int y);
wm_window_t*wm_at(window_manager_t*wm,int x,int y);
wm_window_t*wm_by_id(window_manager_t*wm,unsigned id);
unsigned wm_count(const window_manager_t*wm);
unsigned wm_focused(const window_manager_t*wm);
#endif

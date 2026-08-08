#ifndef OS64_TUI_H
#define OS64_TUI_H
#include "app_abi.h"
#include <stddef.h>
#include <stdint.h>

#define TUI_MAX_WINDOWS 8
#define TUI_MAX_WIDGETS 64
#define TUI_MAX_ITEMS 64
#define TUI_BUTTON_OK 1u
#define TUI_BUTTON_YES 2u
#define TUI_BUTTON_NO 4u
#define TUI_WINDOW_MODAL 1u
#define TUI_WINDOW_CENTER 2u
#define TUI_WINDOW_DOUBLE_BORDER 4u

enum tui_event_type{TUI_EVENT_NONE,TUI_EVENT_KEY,TUI_EVENT_TIMER,TUI_EVENT_RESIZE,TUI_EVENT_COMMAND,TUI_EVENT_CLOSE};
enum tui_widget_type{TUI_LABEL,TUI_BUTTON,TUI_TEXT_INPUT,TUI_PASSWORD_INPUT,TUI_CHECKBOX,TUI_RADIO,TUI_LISTBOX,TUI_TEXT_VIEW,TUI_MENU_BAR,TUI_DROPDOWN,TUI_STATUS_BAR,TUI_PROGRESS,TUI_TABS,TUI_TABLE,TUI_HSCROLL,TUI_VSCROLL};
enum tui_key{TUI_KEY_UP=0x11,TUI_KEY_DOWN=0x12,TUI_KEY_LEFT=0x13,TUI_KEY_RIGHT=0x14,TUI_KEY_F1=0x81,TUI_KEY_F5=0x85,TUI_KEY_F10=0x8a,TUI_KEY_BACKTAB=0x8f,TUI_KEY_HOME=0x91,TUI_KEY_END=0x92,TUI_KEY_PAGEUP=0x93,TUI_KEY_PAGEDOWN=0x94,TUI_KEY_INSERT=0x95,TUI_KEY_DELETE=0x96};

struct tui_theme{
 uint8_t desktop_fg,desktop_bg;
 uint8_t window_fg,window_bg;
 uint8_t title_fg,title_bg;
 uint8_t selected_fg,selected_bg;
 uint8_t disabled_fg,disabled_bg;
 uint8_t status_fg,status_bg;
 uint8_t error_fg,error_bg;
 uint8_t border_fg,border_bg;
 uint8_t highlight_fg,highlight_bg;
 uint8_t success_fg,success_bg;
 uint8_t warning_fg,warning_bg;
};
struct tui_cell{uint32_t character;uint8_t foreground,background,attributes;};
struct tui_screen{size_t width,height;struct tui_cell*front_buffer,*back_buffer;};
struct tui_event{enum tui_event_type type;uint32_t key,modifiers;uint64_t timestamp;};
struct tui_window;
struct tui_widget{
 enum tui_widget_type type;int x,y,width,height;int visible,enabled,focused,checked,value,maximum,scroll,cursor,insert_mode;char text[4096];const char**items;size_t item_count,selected;struct tui_window*parent;void(*activate)(struct tui_widget*);uint8_t custom_fg,custom_bg;
};
struct tui_window{int x,y,width,height;char title[64];unsigned flags;int visible,focused,double_border;struct tui_widget*children[TUI_MAX_WIDGETS];size_t child_count,focus_index;};
struct tui_application{char name[64];int running,redraw;struct tui_window*windows[TUI_MAX_WINDOWS];size_t window_count;};

extern const struct tui_theme tui_theme_dos;
extern const struct tui_theme tui_theme_installer;
int tui_initialize(const os64_api_t*api);
void tui_shutdown(void);
void tui_use_ascii(int enabled);
void tui_set_theme(const struct tui_theme*theme);
struct tui_application*tui_app_create(const char*name);
int tui_app_run(struct tui_application*app);
void tui_app_stop(struct tui_application*app);
struct tui_window*tui_window_create(int x,int y,int width,int height,const char*title,unsigned flags);
void tui_window_destroy(struct tui_window*window);
void tui_window_show(struct tui_window*window);
void tui_window_hide(struct tui_window*window);
void tui_window_focus(struct tui_window*window);
void tui_window_draw(struct tui_window*window);
struct tui_widget*tui_widget_create(enum tui_widget_type type,struct tui_window*parent,int x,int y,int width,int height,const char*text);
struct tui_widget*tui_button_create(struct tui_window*w,int x,int y,int width,const char*text);
struct tui_widget*tui_label_create(struct tui_window*w,int x,int y,const char*text);
struct tui_widget*tui_textbox_create(struct tui_window*w,int x,int y,int width,char*initial,size_t maximum,int password);
struct tui_widget*tui_listbox_create(struct tui_window*w,int x,int y,int width,int height,const char**items,size_t count);
struct tui_widget*tui_table_create(struct tui_window*w,int x,int y,int width,int height,const char**rows,size_t count);
void tui_widget_set_focus(struct tui_widget*widget);
void tui_widget_set_enabled(struct tui_widget*widget,int enabled);
void tui_widget_set_visible(struct tui_widget*widget,int visible);
void tui_invalidate(struct tui_widget*widget);
void tui_widget_repaint(struct tui_widget*widget);
int tui_next_event(struct tui_event*event);
void tui_post_event(const struct tui_event*event);
void tui_dispatch_event(struct tui_application*app,const struct tui_event*event);
void tui_request_redraw(void);
void tui_render(void);
void tui_fill(int x,int y,int width,int height,uint32_t ch,uint8_t fg,uint8_t bg);
void tui_text(int x,int y,const char*text,uint8_t fg,uint8_t bg,int limit);
void tui_box(int x,int y,int width,int height,int double_line,uint8_t fg,uint8_t bg);
int tui_message_box(const char*title,const char*message,unsigned buttons);
int tui_confirm(const char*title,const char*message);
int tui_input_box(const char*title,const char*prompt,char*buffer,size_t size);
int tui_file_selector(const char*title,const char*path,char*out,size_t size,int directories_only);
const os64_api_t*tui_api(void);
void tui_widgets_reset(void);
int tui_window_content_x(const struct tui_window*w);
int tui_window_content_y(const struct tui_window*w);
#endif

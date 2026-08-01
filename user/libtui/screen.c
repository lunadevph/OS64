#include "tui.h"
static const os64_api_t*api;
static struct tui_cell front[80*25],back[80*25];
static struct tui_screen screen={80,25,front,back};
static struct tui_theme theme;
static int ascii_mode,initialized,dirty;
static struct tui_application application;
static struct tui_window windows[TUI_MAX_WINDOWS];
static size_t window_used;
static struct tui_event posted;
static int has_posted;
static void copy_text(char*d,const char*s,size_t cap){size_t n=0;if(s)while(s[n]&&n+1<cap){d[n]=s[n];n++;}d[n]=0;}
static size_t text_len(const char*s){size_t n=0;while(s&&s[n])n++;return n;}
static uint8_t attr(const struct tui_cell*c){return (uint8_t)((c->background<<4)|(c->foreground&15));}
const os64_api_t*tui_api(void){return api;}


int tui_initialize(const os64_api_t*a){
 if(!a||a->version<3||!a->terminal_acquire||!a->terminal_acquire())return 0;
 api=a;screen.width=a->terminal_width();screen.height=a->terminal_height();
 if(screen.width>80)screen.width=80;
 if(screen.height>25)screen.height=25;
 if(screen.width<40||screen.height<15){a->terminal_release();return 0;}
 ascii_mode=a->system_query&&!a->system_query("terminal.unicode");
 theme=tui_theme_dos;
 for(size_t i=0;i<80*25;i++){front[i].character=0xffffffffu;back[i].character=' ';back[i].foreground=theme.desktop_fg;back[i].background=theme.desktop_bg;back[i].attributes=0;}
 window_used=0;tui_widgets_reset();initialized=dirty=1;return 1;
}

void tui_shutdown(void){if(!initialized)return;api->terminal_cursor(0,0,0,1);api->terminal_release();initialized=0;}
void tui_use_ascii(int enabled){ascii_mode=enabled;dirty=1;}
void tui_set_theme(const struct tui_theme*t){if(t)theme=*t;dirty=1;}

void tui_fill(int x,int y,int w,int h,uint32_t ch,uint8_t fg,uint8_t bg){
 for(int yy=0;yy<h;yy++)for(int xx=0;xx<w;xx++){
  int px=x+xx,py=y+yy;
  if(px>=0&&py>=0&&(size_t)px<screen.width&&(size_t)py<screen.height){
   struct tui_cell*c=&back[py*80+px];
   c->character=ch;c->foreground=fg;c->background=bg;c->attributes=0;
  }
 }
 dirty=1;
}

void tui_text(int x,int y,const char*s,uint8_t fg,uint8_t bg,int limit){
 if(!s)return;
 int cells=0;for(size_t i=0;s[i]&&(limit<0||cells<limit);){
  uint32_t cp=(uint8_t)s[i++];
  if((cp&0xe0)==0xc0&&((uint8_t)s[i]&0xc0)==0x80)cp=((cp&31)<<6)|((uint8_t)s[i++]&63);
  else if((cp&0xf0)==0xe0&&((uint8_t)s[i]&0xc0)==0x80&&((uint8_t)s[i+1]&0xc0)==0x80){cp=((cp&15)<<12)|(((uint8_t)s[i]&63)<<6)|((uint8_t)s[i+1]&63);i+=2;}
  int px=x+cells++;
  if(px>=0&&y>=0&&(size_t)px<screen.width&&(size_t)y<screen.height){
   struct tui_cell*c=&back[y*80+px];
   c->character=cp;c->foreground=fg;c->background=bg;
  }
 }
 dirty=1;
}

void tui_box(int x,int y,int w,int h,int dbl,uint8_t fg,uint8_t bg){
 uint32_t tl,tr,bl,br,hz,vt;
 if(ascii_mode){tl=tr=bl=br='+';hz='-';vt='|';}
 else if(dbl){tl=0x2554;tr=0x2557;bl=0x255a;br=0x255d;hz=0x2550;vt=0x2551;}
 else{tl=0x250c;tr=0x2510;bl=0x2514;br=0x2518;hz=0x2500;vt=0x2502;}
 tui_fill(x+1,y+1,w-2,h-2,' ',bg,fg);
 for(int i=1;i<w-1;i++){tui_fill(x+i,y,1,1,hz,fg,bg);tui_fill(x+i,y+h-1,1,1,hz,fg,bg);}
 for(int i=1;i<h-1;i++){tui_fill(x,y+i,1,1,vt,fg,bg);tui_fill(x+w-1,y+i,1,1,vt,fg,bg);}
 tui_fill(x,y,1,1,tl,fg,bg);tui_fill(x+w-1,y,1,1,tr,fg,bg);
 tui_fill(x,y+h-1,1,1,bl,fg,bg);tui_fill(x+w-1,y+h-1,1,1,br,fg,bg);
}

int tui_window_content_x(const struct tui_window*w){return w->x+1;}
int tui_window_content_y(const struct tui_window*w){
 if(w->double_border&&w->title[0])return w->y+3;
 return w->y+1;
}

void tui_window_draw(struct tui_window*w){
 if(!w||!w->visible)return;
 int x=w->x,y=w->y,wi=w->width,he=w->height;
 if(w->double_border){
  tui_fill(x+1,y+1,wi-2,he-2,' ',theme.window_fg,theme.window_bg);
  uint8_t fg=theme.border_fg,bg=theme.border_bg;
  int i;
  for(i=1;i<wi-1;i++){tui_fill(x+i,y,1,1,0x2550,fg,bg);tui_fill(x+i,y+he-1,1,1,0x2550,fg,bg);}
  for(i=1;i<he-1;i++){tui_fill(x,y+i,1,1,0x2551,fg,bg);tui_fill(x+wi-1,y+i,1,1,0x2551,fg,bg);}
  tui_fill(x,y,1,1,0x2554,fg,bg);tui_fill(x+wi-1,y,1,1,0x2557,fg,bg);
  tui_fill(x,y+he-1,1,1,0x255a,fg,bg);tui_fill(x+wi-1,y+he-1,1,1,0x255d,fg,bg);
  if(w->title[0]){
   size_t n=text_len(w->title);
   int tx=x+(int)(wi-(int)n)/2;
   tui_text(tx,y+1,w->title,theme.title_fg,theme.title_bg,(int)n);
   tui_fill(x,y+2,1,1,0x2560,theme.border_fg,theme.border_bg);
   for(i=1;i<wi-1;i++)tui_fill(x+i,y+2,1,1,0x2550,theme.border_fg,theme.border_bg);
   tui_fill(x+wi-1,y+2,1,1,0x2563,theme.border_fg,theme.border_bg);
  }
 }else{
  tui_box(w->x,w->y,w->width,w->height,0,theme.window_fg,theme.window_bg);
  if(w->title[0]){
   size_t n=text_len(w->title);
   int tx=w->x+(w->width-(int)n)/2-1;
   tui_text(tx,w->y," ",theme.title_fg,theme.title_bg,1);
   tui_text(tx+1,w->y,w->title,theme.title_fg,theme.title_bg,w->width-4);
   tui_text(tx+(int)n+1,w->y," ",theme.title_fg,theme.title_bg,1);
  }
 }
}

struct tui_application*tui_app_create(const char*name){
 copy_text(application.name,name,sizeof application.name);
 application.running=1;application.redraw=1;
 application.window_count=0;
 return &application;
}

struct tui_window*tui_window_create(int x,int y,int w,int h,const char*title,unsigned flags){
 if(window_used>=TUI_MAX_WINDOWS||w<4||h<3)return 0;
 if(flags&TUI_WINDOW_CENTER){x=(int)(screen.width-w)/2;y=(int)(screen.height-h)/2;}
 if(x+w<1)x=1-w;
 if(y+h<1)y=1-h;
 if(x>=(int)screen.width)x=(int)screen.width-1;
 if(y>=(int)screen.height)y=(int)screen.height-1;
 struct tui_window*v=&windows[window_used++];
 v->x=x;v->y=y;v->width=w;v->height=h;v->flags=flags;
 v->visible=1;v->focused=1;
 v->double_border=(flags&TUI_WINDOW_DOUBLE_BORDER)?1:(flags&TUI_WINDOW_MODAL)?1:0;
 v->child_count=v->focus_index=0;
 copy_text(v->title,title,sizeof v->title);
 if(application.window_count<TUI_MAX_WINDOWS)application.windows[application.window_count++]=v;
 dirty=1;return v;
}

void tui_window_destroy(struct tui_window*w){if(w)w->visible=0;dirty=1;}
void tui_window_show(struct tui_window*w){if(w)w->visible=1;dirty=1;}
void tui_window_hide(struct tui_window*w){if(w)w->visible=0;dirty=1;}
void tui_window_focus(struct tui_window*w){
 for(size_t i=0;i<application.window_count;i++)application.windows[i]->focused=application.windows[i]==w;
 dirty=1;
}

void tui_request_redraw(void){application.redraw=1;dirty=1;}
void tui_invalidate(struct tui_widget*w){(void)w;tui_request_redraw();}
void tui_widget_repaint(struct tui_widget*w){
 if(!w||!w->visible||!w->parent||!w->parent->visible)return;
 extern void tui_draw_one(struct tui_widget*,const struct tui_theme*);
 tui_draw_one(w,&theme);
 dirty=1;
}

void tui_render(void){
 if(!initialized)return;
 if(application.redraw){
  tui_fill(0,0,(int)screen.width,(int)screen.height,' ',theme.desktop_fg,theme.desktop_bg);
  for(size_t i=0;i<application.window_count;i++)
   if(application.windows[i]->visible){
    tui_window_draw(application.windows[i]);
    extern void tui_draw_widgets(struct tui_window*,const struct tui_theme*);
    tui_draw_widgets(application.windows[i],&theme);
   }
  application.redraw=0;
 }
 if(!dirty)return;
 for(size_t y=0;y<screen.height;y++)
  for(size_t x=0;x<screen.width;x++){
   size_t i=y*80+x;
   struct tui_cell*a=&front[i],*b=&back[i];
   if(a->character!=b->character||a->foreground!=b->foreground||a->background!=b->background||a->attributes!=b->attributes){
    api->terminal_write_cell((unsigned)x,(unsigned)y,b->character,attr(b));
    *a=*b;
   }
  }
 dirty=0;
 extern void tui_update_cursor(struct tui_application*);
 tui_update_cursor(&application);
}

int tui_next_event(struct tui_event*e){
 if(!e)return 0;
 if(has_posted){*e=posted;has_posted=0;return 1;}
 e->type=TUI_EVENT_KEY;
 e->key=api->terminal_read_key();
 e->modifiers=0;e->timestamp=0;
 if(e->key==3)e->type=TUI_EVENT_CLOSE;
 return 1;
}

void tui_post_event(const struct tui_event*e){if(e){posted=*e;has_posted=1;}}
void tui_app_stop(struct tui_application*a){if(a)a->running=0;}
int tui_app_run(struct tui_application*a){
 if(!a)return -1;
 tui_render();
 while(a->running){struct tui_event e;if(!tui_next_event(&e))continue;tui_dispatch_event(a,&e);tui_render();}
 return 0;
}

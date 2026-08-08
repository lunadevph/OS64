#include "desktop.h"
#include "compositor.h"
#include "display.h"
#include "keyboard.h"
#include "image_viewer.h"
#include "ps2.h"
#include "service.h"
#include "shell.h"
#include "tty.h"
#include "window_manager.h"

#define POINTER_W 12u
#define POINTER_H 18u
typedef enum desktop_client_kind{CLIENT_TERMINAL,CLIENT_IMAGE}desktop_client_kind_t;
typedef struct desktop_client{desktop_client_kind_t kind;tty_t tty;image_document_t image;int used;}desktop_client_t;
typedef struct terminal_capture{desktop_client_t*terminal;wm_window_t*window;}terminal_capture_t;
static const uint16_t pointer_bits[POINTER_H]={0x800,0xc00,0xe00,0xf00,0xf80,0xfc0,0xfe0,0xff0,0xff8,0xfc0,0xdc0,0x8e0,0x060,0x070,0x030,0x030,0,0};
static uint32_t pointer_saved[POINTER_W*POINTER_H];
static desktop_client_t clients[WM_MAX_WINDOWS];
static window_manager_t manager;
static unsigned screen_width,screen_height;
static int desktop_mouse_ready;

static int same(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return !*a&&!*b;}
static int same_ci(const char*a,const char*b){while(*a&&*b){char x=*a++,y=*b++;if(x>='A'&&x<='Z')x=(char)(x+32);if(y>='A'&&y<='Z')y=(char)(y+32);if(x!=y)return 0;}return !*a&&!*b;}
static void prompt(char*out,size_t cap){const char*u=kernel_shell_user(),*path=kernel_shell_cwd();size_t n=0;while(*u&&n+1<cap)out[n++]=*u++;if(n+1<cap)out[n++]='@';const char*h="os64:";while(*h&&n+1<cap)out[n++]=*h++;if(same(path,"/home/root"))path="~";while(*path&&n+1<cap)out[n++]=*path++;if(n+2<cap){out[n++]='#';out[n++]=' ';}out[n]=0;}
static void terminal_client_draw(wm_window_t*w){
 desktop_client_t*t=(desktop_client_t*)w->client;compositor_rect_t r={w->x+2,w->y+32,w->width-4,w->height-34};compositor_fill(r.x,r.y,r.w,r.h,0x171b22);
 unsigned columns=r.w>24?(unsigned)(r.w-24)/8:1,rows=r.h>48?(unsigned)(r.h-48)/18:1,first=t->tty.line_count>rows?(unsigned)t->tty.line_count-rows:0,row=0;
 for(unsigned i=first;i<t->tty.line_count;i++,row++)compositor_text(r.x+10,r.y+9+(int)row*18,t->tty.lines[i],columns,0xd7dee8,0x171b22);
 char p[TTY_LINE_LENGTH];prompt(p,sizeof p);unsigned plen=0;while(p[plen])plen++;int iy=r.y+r.h-21;compositor_text(r.x+10,iy,p,columns,0x6ee7a2,0x171b22);unsigned room=columns>plen?columns-plen:0;compositor_text(r.x+10+(int)plen*8,iy,t->tty.input,room,0xf4f7fb,0x171b22);
 if(w->focused){unsigned cursor=t->tty.cursor<room?(unsigned)t->tty.cursor:room;compositor_fill(r.x+10+(int)(plen+cursor)*8,iy,8,16,0xe8edf4);if(t->tty.cursor<t->tty.length){char c[2]={t->tty.input[t->tty.cursor],0};compositor_text(r.x+10+(int)(plen+cursor)*8,iy,c,1,0x171b22,0xe8edf4);}}
}
static void terminal_sink(char c,void*context){terminal_capture_t*capture=(terminal_capture_t*)context;tty_write_char(&capture->terminal->tty,c);if(c=='\n'||c==' ')terminal_client_draw(capture->window);}
static desktop_client_t*client_slot(void){for(unsigned i=0;i<WM_MAX_WINDOWS;i++)if(!clients[i].used)return &clients[i];return 0;}
static wm_window_t*new_terminal(void){desktop_client_t*c=client_slot();if(!c)return 0;c->used=1;c->kind=CLIENT_TERMINAL;image_document_init(&c->image);tty_init(&c->tty);tty_write_line(&c->tty,"OS64 Terminal");tty_write_line(&c->tty,"F2 new | F3 focus | F4 close | F5 layout");wm_window_t*w=wm_create(&manager,"Terminal",c);if(w)return w;c->used=0;return 0;}
static wm_window_t*new_image(const char*path){desktop_client_t*c=client_slot();if(!c)return 0;c->used=1;c->kind=CLIENT_IMAGE;image_document_init(&c->image);if(path&&*path)image_open(&c->image,path);wm_window_t*w=wm_create(&manager,"Image Viewer",c);if(w)return w;image_close(&c->image);c->used=0;return 0;}
static void close_window(unsigned id){wm_window_t*w=wm_by_id(&manager,id);if(!w)return;desktop_client_t*c=(desktop_client_t*)w->client;if(c->kind==CLIENT_IMAGE)image_close(&c->image);c->used=0;wm_close(&manager,id);}
static void image_path(char*out,const char*path){size_t n=0;if(*path=='/'){while(*path&&n<127)out[n++]=*path++;}else{const char*cwd=kernel_shell_cwd();while(*cwd&&n<126)out[n++]=*cwd++;if(n&&out[n-1]!='/')out[n++]='/';while(*path&&n<127)out[n++]=*path++;}out[n]=0;}
static void run_command(wm_window_t*w,const char*command){desktop_client_t*t=(desktop_client_t*)w->client;if(!*command)return;char line[TTY_LINE_LENGTH],shown[TTY_LINE_LENGTH];prompt(shown,sizeof shown);size_t n=0;while(shown[n])n++;size_t i=0;while(command[i]&&n+1<sizeof shown)shown[n++]=command[i++];shown[n]=0;tty_write_line(&t->tty,shown);if(same_ci(command,"clear")){tty_clear(&t->tty);return;}if(same_ci(command,"exit")){close_window(w->id);return;}const char*path=0;if(command[0]=='v'&&command[1]=='i'&&command[2]=='e'&&command[3]=='w'&&command[4]==' ')path=command+5;else if(command[0]=='v'&&command[1]=='i'&&command[2]=='e'&&command[3]=='w'&&command[4]=='e'&&command[5]=='r'&&command[6]==' ')path=command+7;if(path){while(*path==' ')path++;char absolute[128];image_path(absolute,path);if(!new_image(absolute))tty_write_line(&t->tty,"view: no window slot available");return;}i=0;while(command[i]&&i+1<sizeof line){line[i]=command[i];i++;}line[i]=0;terminal_capture_t capture={t,w};display_output_sink(terminal_sink,&capture);kernel_shell_execute(line);display_output_sink(0,0);}
static void menu_draw(void){compositor_fill(8,36,246,156,0xe4e8ec);compositor_text(24,52,"New terminal       F2",24,0x182331,0xe4e8ec);compositor_text(24,82,"Image viewer",24,0x182331,0xe4e8ec);compositor_text(24,112,"Cycle focus        F3",24,0x182331,0xe4e8ec);compositor_text(24,142,"Toggle layout      F5",24,0x182331,0xe4e8ec);compositor_text(24,172,"Close menu        Esc",24,0x52606c,0xe4e8ec);}
static void render(int menu){unsigned focused_position=0,position=0;compositor_desktop();for(int pass=0;pass<2;pass++)for(unsigned i=0;i<WM_MAX_WINDOWS;i++){wm_window_t*w=&manager.windows[i];if(w->used&&w->focused==(pass!=0)){compositor_window(w->x,w->y,w->width,w->height,w->title,w->focused);desktop_client_t*c=(desktop_client_t*)w->client;if(c->kind==CLIENT_TERMINAL)terminal_client_draw(w);else image_render(&c->image,w->x+2,w->y+32,w->width-4,w->height-34);}}for(unsigned i=0;i<WM_MAX_WINDOWS;i++)if(manager.windows[i].used){position++;if(manager.windows[i].focused)focused_position=position;}compositor_panel(wm_count(&manager),focused_position,manager.layout==WM_LAYOUT_TILED,desktop_mouse_ready);position=0;for(unsigned i=0;i<WM_MAX_WINDOWS;i++)if(manager.windows[i].used){compositor_task(position,manager.windows[i].title,manager.windows[i].focused);position++;}if(menu)menu_draw();}
static void pointer_draw(unsigned x,unsigned y){for(unsigned py=0;py<POINTER_H;py++)for(unsigned px=0;px<POINTER_W;px++){pointer_saved[py*POINTER_W+px]=display_graphics_get_pixel(x+px,y+py);if(pointer_bits[py]&(0x800u>>px))display_graphics_put_pixel(x+px,y+py,(px==0||py==0)?0:0xffffff);}}
static void pointer_restore(unsigned x,unsigned y){for(unsigned py=0;py<POINTER_H;py++)for(unsigned px=0;px<POINTER_W;px++)display_graphics_put_pixel(x+px,y+py,pointer_saved[py*POINTER_W+px]);}
static wm_window_t*task_window_at(int x){int slot=(x-10)/122,seen=0;if(x<10||slot<0)return 0;for(unsigned i=0;i<WM_MAX_WINDOWS;i++)if(manager.windows[i].used){if(seen==slot)return &manager.windows[i];seen++;}return 0;}

int desktop_run(void){
 if(!display_framebuffer_active())return 0;
 screen_width=display_pixel_width();screen_height=display_pixel_height();
 if(screen_width<640||screen_height<480)return 0;
 display_cursor_set(0,0,0,1);display_serial_cells(0);compositor_init(screen_width,screen_height);wm_init(&manager,screen_width,screen_height);for(unsigned i=0;i<WM_MAX_WINDOWS;i++)clients[i].used=0;
 desktop_mouse_ready=ps2_mouse_initialize();int x=(int)screen_width/2,y=(int)screen_height/2,menu=0,dragging=0,drag_dx=0,drag_dy=0;unsigned drag_id=0,old_buttons=0;render(menu);pointer_draw((unsigned)x,(unsigned)y);
 for(;;){
  int moved=0,dx=0,dy=0,old_x=x,old_y=y;unsigned buttons=old_buttons;while(ps2_mouse_poll(&dx,&dy,&buttons)){x+=dx;y+=dy;moved=1;}
  if(moved){pointer_restore((unsigned)old_x,(unsigned)old_y);if(x<0)x=0;if(y<0)y=0;if(x>(int)(screen_width-POINTER_W))x=(int)(screen_width-POINTER_W);if(y>(int)(screen_height-POINTER_H))y=(int)(screen_height-POINTER_H);if(dragging&&(buttons&1u)){wm_move(&manager,drag_id,x-drag_dx,y-drag_dy);render(menu);}}
  if((buttons&1u)&&!(old_buttons&1u)){
   pointer_restore((unsigned)x,(unsigned)y);
   if(y<36&&x<135)menu=!menu;
   else if(menu&&y>=36&&y<72){new_terminal();menu=0;}
   else if(menu&&y>=72&&y<102){new_image(0);menu=0;}
   else if(menu&&y>=132&&y<162){wm_toggle_layout(&manager);menu=0;}
   else if(menu){menu=0;}
   else if(y>(int)screen_height-44){wm_window_t*w=task_window_at(x);if(w)wm_focus(&manager,w->id);}
   else {wm_window_t*w=wm_at(&manager,x,y);if(w){wm_focus(&manager,w->id);if(x>=w->x+w->width-30&&y<w->y+32)close_window(w->id);else if(manager.layout==WM_LAYOUT_FLOATING&&y<w->y+32){dragging=1;drag_id=w->id;drag_dx=x-w->x;drag_dy=y-w->y;}}else if(y>=54&&y<132&&x>=160&&x<224)new_image(0);else if(y>=54&&y<132&&x>=16&&x<82)new_terminal();else if(y>=54&&y<132&&x>=88&&x<154){wm_window_t*n=new_terminal();if(n)run_command(n,"ls");}else if(y>=54&&y<132&&x>=232&&x<302){wm_window_t*n=new_terminal();if(n)run_command(n,"status");}}
   render(menu);moved=1;
  }
  if(!(buttons&1u))dragging=0;
  old_buttons=buttons;
  if(moved)pointer_draw((unsigned)x,(unsigned)y);
  uint8_t key=(uint8_t)keyboard_poll();
  if(key==27&&menu){pointer_restore((unsigned)x,(unsigned)y);menu=0;render(menu);pointer_draw((unsigned)x,(unsigned)y);}
  else if(key==0x82){pointer_restore((unsigned)x,(unsigned)y);new_terminal();render(menu);pointer_draw((unsigned)x,(unsigned)y);}
  else if(key==0x83){pointer_restore((unsigned)x,(unsigned)y);wm_focus_next(&manager);render(menu);pointer_draw((unsigned)x,(unsigned)y);}
  else if(key==0x84){unsigned id=wm_focused(&manager);if(id){pointer_restore((unsigned)x,(unsigned)y);close_window(id);render(menu);pointer_draw((unsigned)x,(unsigned)y);}}
  else if(key==0x85){pointer_restore((unsigned)x,(unsigned)y);wm_toggle_layout(&manager);render(menu);pointer_draw((unsigned)x,(unsigned)y);}
  else if((key=='t'||key=='T')&&!wm_count(&manager)){pointer_restore((unsigned)x,(unsigned)y);new_terminal();render(menu);pointer_draw((unsigned)x,(unsigned)y);}
  else if(key&&key!=0x8a){wm_window_t*w=wm_by_id(&manager,wm_focused(&manager));if(w){desktop_client_t*t=(desktop_client_t*)w->client;if(t->kind==CLIENT_TERMINAL){char command[TTY_LINE_LENGTH];if(tty_input_key(&t->tty,key,command,sizeof command))run_command(w,command);}pointer_restore((unsigned)x,(unsigned)y);render(menu);pointer_draw((unsigned)x,(unsigned)y);}}
  service_poll_all();__asm__ volatile("pause");
 }
}

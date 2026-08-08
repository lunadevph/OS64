#include "tui.h"

static void copy(char*d,const char*s,size_t cap){size_t n=0;while(s&&s[n]&&n+1<cap){d[n]=s[n];n++;}d[n]=0;}
static int starts(const char*s,const char*p){while(*p&&*s==*p){s++;p++;}return !*p;}

int _start(const os64_api_t*api,const char*args){
 if(args&&starts(args,"--help")){api->write("Usage: browser [URL]\nInteractive Lynx-style web browser.\n");return 0;}
 if(!tui_initialize(api)){api->write("browser: terminal must be at least 40x15\n");return 1;}
 struct tui_application*a=tui_app_create("browser");
 struct tui_window*w=tui_window_create(0,0,80,25,"OS64 Web Browser",0);
 tui_widget_create(TUI_MENU_BAR,w,1,0,76,1," File  Navigate  View  Help");
 tui_label_create(w,1,2,"URL:");
 char initial[128]="https://example.com";if(args&&*args)copy(initial,args,sizeof initial);
 struct tui_widget*url=tui_textbox_create(w,6,2,68,initial,127,0);
 struct tui_widget*page=tui_widget_create(TUI_TEXT_VIEW,w,1,4,75,15,"Press Enter to load the address.\n\nTab moves between the address bar and document.");
 struct tui_widget*status=tui_widget_create(TUI_STATUS_BAR,w,1,20,76,1,"HTTPS/TLS 1.2 enabled | Enter Open | PgUp/PgDn Scroll | F10 Exit");
 tui_widget_set_focus(url);
 for(;;){
  tui_request_redraw();tui_render();struct tui_event e;tui_next_event(&e);
  if(e.key==TUI_KEY_F10||e.key==27||e.type==TUI_EVENT_CLOSE)break;
  if(e.key==TUI_KEY_F1){tui_message_box("Browser Help","OS64 Browser is a keyboard-driven reader.\nEnter loads a URL. Tab focuses the page.\nPage Up and Page Down scroll. F5 reloads.",TUI_BUTTON_OK);continue;}
  if((e.key=='\n'&&url->focused)||e.key==TUI_KEY_F5){
   copy(status->text,"Loading...",sizeof status->text);tui_request_redraw();tui_render();
   os64_size_t size=0;int result=api->browser_fetch?api->browser_fetch(url->text,page->text,sizeof page->text-1,&size):-9;
   if(result==1){page->text[size]=0;page->scroll=0;copy(status->text,"Document loaded | Tab focuses page | F10 exits",sizeof status->text);}
   else{const char*error=result==-1?"DNS resolution failed.":result==-2?"TCP connection failed.":result==-3?"TLS entropy source unavailable.":result==-4?"TLS validation clock is unavailable or invalid.":result==-5?"HTTPS certificate validation failed.":result==-6?"TLS 1.2 handshake failed.":result==-7?"TLS transport I/O failed.":result==-9?"Refused HTTPS-to-HTTP redirect downgrade.":"Unable to load document.";copy(page->text,error,sizeof page->text);copy(status->text,"HTTPS load failed - see document for details",sizeof status->text);}
   continue;
  }
  if(page->focused&&(e.key==TUI_KEY_PAGEUP||e.key==TUI_KEY_UP)){if(page->scroll)page->scroll--;tui_request_redraw();continue;}
  if(page->focused&&(e.key==TUI_KEY_PAGEDOWN||e.key==TUI_KEY_DOWN)){page->scroll+=e.key==TUI_KEY_PAGEDOWN?10:1;tui_request_redraw();continue;}
  tui_dispatch_event(a,&e);
 }
 tui_shutdown();return 0;
}

#include "tui.h"
static struct tui_widget widgets[TUI_MAX_WIDGETS];static size_t used;
void tui_widgets_reset(void){used=0;}
static void text_copy(char*d,const char*s,size_t cap){size_t n=0;if(s)while(s[n]&&n+1<cap){d[n]=s[n];n++;}d[n]=0;}
static size_t text_len(const char*s){size_t n=0;while(s&&s[n])n++;return n;}
static int focusable(const struct tui_widget*w){return w&&w->visible&&w->enabled&&w->type!=TUI_LABEL&&w->type!=TUI_STATUS_BAR&&w->type!=TUI_PROGRESS&&w->type!=TUI_TABS;}

static int cx(const struct tui_widget*w){return w->parent->x+1+w->x;}
static int cy(const struct tui_widget*w){
 struct tui_window*p=w->parent;
 if(p->double_border&&p->title[0])return p->y+3+w->y;
 return p->y+1+w->y;
}

struct tui_widget*tui_widget_create(enum tui_widget_type type,struct tui_window*p,int x,int y,int width,int height,const char*text){
 if(!p||used>=TUI_MAX_WIDGETS||p->child_count>=TUI_MAX_WIDGETS)return 0;
 struct tui_widget*w=&widgets[used++];
 w->type=type;w->parent=p;w->x=x;w->y=y;w->width=width;w->height=height;
 w->visible=w->enabled=1;w->focused=w->checked=w->value=w->scroll=w->cursor=0;
 w->maximum=127;w->insert_mode=1;w->items=0;w->item_count=w->selected=0;w->activate=0;
 w->custom_fg=w->custom_bg=0;
 text_copy(w->text,text,sizeof w->text);
 p->children[p->child_count++]=w;
 return w;
}
struct tui_widget*tui_button_create(struct tui_window*w,int x,int y,int width,const char*t){return tui_widget_create(TUI_BUTTON,w,x,y,width,1,t);}
struct tui_widget*tui_label_create(struct tui_window*w,int x,int y,const char*t){return tui_widget_create(TUI_LABEL,w,x,y,(int)text_len(t),1,t);}
struct tui_widget*tui_textbox_create(struct tui_window*w,int x,int y,int width,char*initial,size_t maximum,int password){
 struct tui_widget*v=tui_widget_create(password?TUI_PASSWORD_INPUT:TUI_TEXT_INPUT,w,x,y,width,1,initial);
 if(v){v->maximum=(int)(maximum<127?maximum:127);v->cursor=(int)text_len(v->text);}
 return v;
}
struct tui_widget*tui_listbox_create(struct tui_window*w,int x,int y,int width,int height,const char**items,size_t count){
 struct tui_widget*v=tui_widget_create(TUI_LISTBOX,w,x,y,width,height,"");
 if(v){v->items=items;v->item_count=count;}return v;
}
struct tui_widget*tui_table_create(struct tui_window*w,int x,int y,int width,int height,const char**rows,size_t count){
 struct tui_widget*v=tui_listbox_create(w,x,y,width,height,rows,count);
 if(v)v->type=TUI_TABLE;
 return v;
}
void tui_widget_set_enabled(struct tui_widget*w,int value){if(w)w->enabled=value;tui_request_redraw();}
void tui_widget_set_visible(struct tui_widget*w,int value){if(w)w->visible=value;tui_request_redraw();}

void tui_widget_set_focus(struct tui_widget*w){
 if(!w||!w->parent)return;
 int was=w->focused;
 for(size_t i=0;i<w->parent->child_count;i++)w->parent->children[i]->focused=0;
 w->focused=1;
 for(size_t i=0;i<w->parent->child_count;i++)if(w->parent->children[i]==w)w->parent->focus_index=i;
 if(!was)tui_request_redraw();
 else tui_widget_repaint(w);
}

void tui_draw_one(struct tui_widget*w,const struct tui_theme*t){
 if(!w||!w->visible)return;
 int x=cx(w),y=cy(w);
 uint8_t fg,win_bg;
 win_bg=t->window_bg;
 if(w->enabled){
  if(w->focused){fg=t->highlight_fg;win_bg=t->highlight_bg;}
  else fg=t->window_fg;
 }else{fg=t->disabled_fg;win_bg=t->window_bg;}

 if(w->type==TUI_LABEL||w->type==TUI_TABS||w->type==TUI_STATUS_BAR||w->type==TUI_MENU_BAR){
  uint8_t lf,lb;
  if(w->type==TUI_STATUS_BAR){lf=t->status_fg;lb=t->status_bg;}
  else if(w->type==TUI_MENU_BAR){lf=t->title_fg;lb=t->title_bg;}
  else if(w->custom_fg||w->custom_bg){lf=w->custom_fg;lb=w->custom_bg;}
  else{lf=fg;lb=win_bg;}
  tui_fill(x,y,w->width,w->height,' ',lf,lb);
  tui_text(x,y,w->text,lf,lb,w->width);
  return;
 }

 if(w->type==TUI_TEXT_VIEW){
  tui_fill(x,y,w->width,w->height,' ',t->window_fg,t->window_bg);
  int row=0,col=0;size_t i=0,skip=(size_t)(w->scroll<0?0:w->scroll);size_t line=0;
  while(w->text[i]&&row<w->height){
   if(line<skip){if(w->text[i++]=='\n')line++;continue;}
   char ch=w->text[i++];
   if(ch=='\n'){row++;col=0;continue;}
   if(col>=w->width){row++;col=0;if(row>=w->height)break;}
   char one[2]={ch,0};tui_text(x+col,y+row,one,t->window_fg,t->window_bg,1);col++;
  }
  return;
 }

 if(w->type==TUI_BUTTON){
  uint8_t bfg=fg,bbg=win_bg;
  if(w->focused){bfg=t->highlight_bg;bbg=t->highlight_fg;}
  tui_fill(x,y,w->width,1,' ',bbg,bfg);
  int txt=(int)text_len(w->text);
  int inner=txt+4;
  int bx=x+(w->width-inner)/2;
  tui_text(bx,y,"[  ",bfg,bbg,3);
  tui_text(bx+3,y,w->text,bfg,bbg,txt);
  tui_text(bx+3+txt,y,"  ]",bfg,bbg,3);
  return;
 }

 if(w->type==TUI_CHECKBOX||w->type==TUI_RADIO){
  tui_fill(x,y,w->width,1,' ',fg,win_bg);
  tui_text(x,y,w->type==TUI_CHECKBOX?(w->checked?"[x] ":"[ ] "):(w->checked?"(*) ":"( ) "),fg,win_bg,4);
  tui_text(x+4,y,w->text,fg,win_bg,w->width-4);
  return;
 }

 if(w->type==TUI_TEXT_INPUT||w->type==TUI_PASSWORD_INPUT){
  uint8_t bfg=fg,bbg=win_bg;
  if(w->focused){bfg=t->highlight_bg;bbg=t->highlight_fg;}
  tui_fill(x,y,w->width,1,' ',bfg,bbg);
  int length=(int)text_len(w->text),start=w->scroll,cols=w->width;
  if(w->cursor<start)start=w->cursor;
  if(w->cursor>=start+cols)start=w->cursor-cols+1;
  w->scroll=start;
  for(int i=0;i<cols&&start+i<length;i++){
   char c=w->type==TUI_PASSWORD_INPUT?'*':w->text[start+i];
   uint8_t cf=bfg,cb=bbg;
   if(w->focused&&start+i==w->cursor){cf=bbg;cb=bfg;}
   char s[2]={c,0};
   tui_text(x+i,y,s,cf,cb,1);
  }
  return;
 }

 if(w->type==TUI_LISTBOX||w->type==TUI_TABLE||w->type==TUI_DROPDOWN){
  tui_fill(x,y,w->width,w->height,' ',fg,win_bg);
  if(w->selected<(size_t)w->scroll)w->scroll=(int)w->selected;
  if(w->selected>=(size_t)(w->scroll+w->height))w->scroll=(int)w->selected-w->height+1;
  for(int row=0;row<w->height;row++){
   size_t index=(size_t)(w->scroll+row);
   if(index>=w->item_count)break;
   uint8_t rf=fg,rb=win_bg;
   if(index==w->selected){
    if(w->focused){rf=t->selected_fg;rb=t->selected_bg;}
    else{rf=t->highlight_bg;rb=t->highlight_fg;}
   }
   tui_fill(x,y+row,w->width,1,' ',rf,rb);
   tui_text(x,y+row,w->items[index],rf,rb,w->width);
  }
  return;
 }

 if(w->type==TUI_PROGRESS){
  tui_fill(x,y,w->width,1,' ',t->window_fg,t->window_bg);
  int fill=w->maximum?w->value*w->width/w->maximum:0;
  tui_fill(x,y,fill,1,219,t->success_fg,t->success_bg);
  tui_fill(x+fill,y,w->width-fill,1,176,t->disabled_fg,t->window_bg);
  return;
 }

 if(w->type==TUI_HSCROLL||w->type==TUI_VSCROLL){
  int vertical=w->type==TUI_VSCROLL;
  int length=vertical?w->height:w->width;
  tui_fill(x,y,w->width,w->height,176,t->disabled_fg,t->window_bg);
  int position=w->maximum?((length-1)*w->value/w->maximum):0;
  tui_fill(x+(vertical?0:position),y+(vertical?position:0),1,1,219,t->window_fg,t->window_bg);
  return;
 }
}

void tui_draw_widgets(struct tui_window*w,const struct tui_theme*t){
 for(size_t i=0;i<w->child_count;i++)tui_draw_one(w->children[i],t);
}

void tui_update_cursor(struct tui_application*a){
 const os64_api_t*api=tui_api();
 for(size_t wi=a->window_count;wi>0;wi--){
  struct tui_window*w=a->windows[wi-1];
  if(!w->visible||!w->focused)continue;
  for(size_t i=0;i<w->child_count;i++){
   struct tui_widget*v=w->children[i];
   if(v->focused&&(v->type==TUI_TEXT_INPUT||v->type==TUI_PASSWORD_INPUT)){
    int content_x=w->x+1+v->x+v->cursor-v->scroll;
    int content_y=w->double_border&&w->title[0]?w->y+3+v->y:w->y+1+v->y;
    api->terminal_cursor((unsigned)content_x,(unsigned)content_y,1,1);
    return;
   }
  }
 }
 api->terminal_cursor(0,0,0,1);
}

static void focus_move(struct tui_window*w,int direction){
 if(!w||!w->child_count)return;
 size_t at=w->focus_index;
 for(size_t n=0;n<w->child_count;n++){
  at=(at+w->child_count+(size_t)direction)%w->child_count;
  if(focusable(w->children[at])){tui_widget_set_focus(w->children[at]);return;}
 }
}

static void edit(struct tui_widget*w,uint32_t key){
 size_t n=text_len(w->text);
 if(key==TUI_KEY_LEFT&&w->cursor)w->cursor--;
 else if(key==TUI_KEY_RIGHT&&w->cursor<(int)n)w->cursor++;
 else if(key==TUI_KEY_HOME)w->cursor=0;
 else if(key==TUI_KEY_END)w->cursor=(int)n;
 else if(key==TUI_KEY_INSERT)w->insert_mode=!w->insert_mode;
 else if((key==8||key==127)&&w->cursor){
  for(size_t i=(size_t)--w->cursor;i<n;i++)w->text[i]=w->text[i+1];
 }
 else if(key==TUI_KEY_DELETE&&w->cursor<(int)n){
  for(size_t i=(size_t)w->cursor;i<n;i++)w->text[i]=w->text[i+1];
 }
 else if(key>=32&&key<127&&n<(size_t)w->maximum){
  if(w->insert_mode)for(size_t i=n+1;i>(size_t)w->cursor;i--)w->text[i]=w->text[i-1];
  w->text[w->cursor++]=(char)key;
  if(!w->insert_mode&&w->cursor>(int)n)w->text[w->cursor]=0;
 }
 tui_widget_repaint(w);
}

void tui_dispatch_event(struct tui_application*a,const struct tui_event*e){
 if(e->type==TUI_EVENT_CLOSE){a->running=0;return;}
 struct tui_window*w=0;
 for(size_t i=a->window_count;i>0;i--)if(a->windows[i-1]->visible&&a->windows[i-1]->focused){w=a->windows[i-1];break;}
 if(!w)return;
 if(e->key==TUI_KEY_F10||e->key==27){a->running=0;return;}
 if(e->key=='\t'){focus_move(w,1);return;}
 if(e->key==TUI_KEY_BACKTAB){focus_move(w,-1);return;}
 struct tui_widget*v=w->child_count?w->children[w->focus_index]:0;
 if(!v||!focusable(v)){focus_move(w,1);return;}
 if(v->type==TUI_TEXT_INPUT||v->type==TUI_PASSWORD_INPUT){edit(v,e->key);return;}
 if(v->type==TUI_LISTBOX||v->type==TUI_TABLE||v->type==TUI_DROPDOWN){
  if(e->key==TUI_KEY_UP&&v->selected)v->selected--;
  else if(e->key==TUI_KEY_DOWN&&v->selected+1<v->item_count)v->selected++;
  else if(e->key==TUI_KEY_HOME)v->selected=0;
  else if(e->key==TUI_KEY_END&&v->item_count)v->selected=v->item_count-1;
  else if(e->key==TUI_KEY_PAGEUP)v->selected=v->selected>(size_t)v->height?v->selected-v->height:0;
  else if(e->key==TUI_KEY_PAGEDOWN&&v->item_count){v->selected+=v->height;if(v->selected>=v->item_count)v->selected=v->item_count-1;}
  else if(e->key=='\n'&&v->activate)v->activate(v);
  tui_request_redraw();return;
 }
 if((v->type==TUI_CHECKBOX||v->type==TUI_RADIO)&&(e->key==' '||e->key=='\n')){
  v->checked=!v->checked;if(v->activate)v->activate(v);tui_request_redraw();return;
 }
 if(v->type==TUI_BUTTON&&(e->key==' '||e->key=='\n')&&v->activate)v->activate(v);
}

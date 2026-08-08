/* Native OS64 milestone for the Microsoft Edit port. */
#include "app_abi.h"
#define CAPACITY (16u*1024u)
#define K_UP 0x11u
#define K_DOWN 0x12u
#define K_LEFT 0x13u
#define K_RIGHT 0x14u
#define K_HOME 0x91u
#define K_END 0x92u
#define K_PAGEUP 0x93u
#define K_PAGEDOWN 0x94u
#define K_DELETE 0x96u
static unsigned char buffer[CAPACITY];
static os64_size_t length,position;
static unsigned top_line;
static int changed,quit_armed;
static char filename[160];
static os64_size_t copy(char*d,const char*s,os64_size_t cap){os64_size_t n=0;while(s&&s[n]&&n+1<cap){d[n]=s[n];n++;}d[n]=0;return n;}
static os64_size_t line_start(os64_size_t p){while(p&&buffer[p-1]!='\n')p--;return p;}
static os64_size_t line_end(os64_size_t p){while(p<length&&buffer[p]!='\n')p++;return p;}
static unsigned line_at(os64_size_t p){unsigned n=0;for(os64_size_t i=0;i<p;i++)if(buffer[i]=='\n')n++;return n;}
static os64_size_t find_line(unsigned n){os64_size_t p=0;while(n--&&p<length){p=line_end(p);if(p<length)p++;}return p;}
static void fill(const os64_api_t*a,unsigned x,unsigned y,unsigned w,unsigned h,unsigned char attr){for(unsigned py=0;py<h;py++)for(unsigned px=0;px<w;px++)a->terminal_write_cell(x+px,y+py,' ',attr);}
static void print(const os64_api_t*a,unsigned x,unsigned y,const char*s,unsigned w,unsigned char attr){unsigned n=0;while(s&&s[n]&&n<w){a->terminal_write_cell(x+n,y,(unsigned char)s[n],attr);n++;}}
static void number(char*out,unsigned v){char r[12];unsigned n=0,p=0;if(!v)r[n++]='0';while(v){r[n++]=(char)('0'+v%10);v/=10;}while(n)out[p++]=r[--n];out[p]=0;}
static void draw(const os64_api_t*a,const char*message){
 unsigned w=a->terminal_width(),h=a->terminal_height(),line=line_at(position),rows=h-4;os64_size_t start=line_start(position);unsigned column=(unsigned)(position-start);
 if(line<top_line)top_line=line;
 if(line>=top_line+rows)top_line=line-rows+1;
 fill(a,0,0,w,h,0x07);fill(a,0,0,w,1,0x70);print(a,1,0," File  Edit  Selection  View  Help",w-1,0x70);
 fill(a,0,1,w,1,0x1f);print(a,2,1,"Microsoft Edit for OS64",w-2,0x1f);if(w>24)print(a,w-22,1,changed?"[modified]":"[saved]",20,0x1f);
 os64_size_t at=find_line(top_line);for(unsigned row=0;row<rows;row++){unsigned col=0;while(at<length&&buffer[at]!='\n'){unsigned char c=buffer[at++];if(c=='\t'){unsigned spaces=4-col%4;while(spaces--&&col<w)a->terminal_write_cell(col++,row+2,' ',0x07);}else if(col<w)a->terminal_write_cell(col++,row+2,c>=32?c:'?',0x07);}if(at<length)at++;}
 fill(a,0,h-2,w,1,0x70);print(a,1,h-2,message&&*message?message:"Ctrl+S Save   Ctrl+Q Exit   Arrows Navigate",w-1,0x70);
 fill(a,0,h-1,w,1,0x17);char info[100],value[12];os64_size_t n=copy(info,filename,sizeof info);if(n+6<sizeof info){info[n++]=' ';info[n++]=' ';info[n++]='L';info[n++]='n';info[n++]=' ';}number(value,line+1);n+=copy(info+n,value,sizeof info-n);if(n+7<sizeof info){info[n++]=',';info[n++]=' ';info[n++]='C';info[n++]='o';info[n++]='l';info[n++]=' ';}number(value,column+1);copy(info+n,value,sizeof info-n);print(a,1,h-1,info,w-1,0x17);
 a->terminal_cursor(column<w?column:w-1,2+line-top_line,1,1);
}
static void insert(unsigned char c){if(length>=CAPACITY)return;for(os64_size_t i=length;i>position;i--)buffer[i]=buffer[i-1];buffer[position++]=c;length++;changed=1;quit_armed=0;}
static void erase(os64_size_t p){if(p>=length)return;for(os64_size_t i=p+1;i<length;i++)buffer[i-1]=buffer[i];length--;if(position>length)position=length;changed=1;quit_armed=0;}
static void vertical(int direction){unsigned line=line_at(position),column=(unsigned)(position-line_start(position));os64_size_t start;if(direction<0){if(!line)return;start=find_line(line-1);}else{start=line_end(position);if(start>=length)return;start++;}os64_size_t end=line_end(start);position=start+(column<end-start?column:end-start);}
int _start(const os64_api_t*a,const char*args){
 if(!a||a->version!=OS64_ABI_VERSION)return 1;
 while(args&&*args==' ')args++;
 if(args&&args[0]=='-'&&args[1]=='-'&&args[2]=='h'){a->write("Usage: edit [FILE]\nNative OS64 compatibility milestone for Microsoft Edit.\n");return 0;}
 copy(filename,args&&*args?args:"/home/root/untitled.txt",sizeof filename);length=position=0;top_line=0;changed=quit_armed=0;os64_size_t loaded=0;if(a->read_file(filename,buffer,CAPACITY,&loaded))length=loaded;
 if(!a->terminal_acquire||!a->terminal_acquire()){a->write("edit: could not acquire terminal\n");return 1;}if(a->terminal_width()<40||a->terminal_height()<10){a->terminal_release();a->write("edit: terminal must be at least 40x10\n");return 1;}
 const char*message="";for(;;){draw(a,message);message="";unsigned key=a->terminal_read_key();if(key==19){if(a->write_file(filename,buffer,length)){changed=0;quit_armed=0;message="Saved.";}else message="Save failed: permission denied or read-only filesystem.";}else if(key==17||key==3){if(changed&&!quit_armed){quit_armed=1;message="Unsaved changes. Ctrl+Q again discards; Ctrl+S saves.";}else break;}else if(key==K_LEFT&&position)position--;else if(key==K_RIGHT&&position<length)position++;else if(key==K_UP)vertical(-1);else if(key==K_DOWN)vertical(1);else if(key==K_HOME)position=line_start(position);else if(key==K_END)position=line_end(position);else if(key==K_PAGEUP){for(unsigned i=0;i<10;i++)vertical(-1);}else if(key==K_PAGEDOWN){for(unsigned i=0;i<10;i++)vertical(1);}else if(key==K_DELETE)erase(position);else if((key=='\b'||key==127)&&position){position--;erase(position);}else if(key=='\r'||key=='\n')insert('\n');else if(key=='\t')insert('\t');else if(key>=32&&key<=126)insert((unsigned char)key);}
 a->terminal_cursor(0,0,0,1);a->terminal_release();return 0;
}

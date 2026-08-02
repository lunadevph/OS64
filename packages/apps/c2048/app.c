/* OS64 port of mevdschee/2048.c 1.0.3. See LICENSE and README.md. */
#include "app_abi.h"
#include <stdint.h>

#define SIZE 4
#define KEY_UP 0x11u
#define KEY_DOWN 0x12u
#define KEY_LEFT 0x13u
#define KEY_RIGHT 0x14u

static uint8_t board[SIZE][SIZE];
static uint32_t score,random_state=0x2048c0deu;
static const os64_api_t *os;

static void cell(unsigned x,unsigned y,char c,unsigned char color){os->terminal_write_cell(x,y,(unsigned char)c,color);}
static void text(unsigned x,unsigned y,const char*s,unsigned char color){while(*s)cell(x++,y,*s++,color);}
static void number(unsigned x,unsigned y,uint32_t value,unsigned char color){char b[12];unsigned n=0;if(!value)b[n++]='0';while(value){b[n++]=(char)('0'+value%10);value/=10;}while(n)cell(x++,y,b[--n],color);}
static uint32_t random32(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}

static void draw(void){
 unsigned width=os->terminal_width(),height=os->terminal_height();for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)cell(x,y,' ',0x17);
 unsigned ox=width>41?(width-41)/2:0,oy=height>19?(height-19)/2:0;text(ox,oy,"2048.c for OS64",0x1f);text(ox+25,oy,"Score:",0x1f);number(ox+32,oy,score,0x1e);
 for(unsigned y=0;y<SIZE;y++)for(unsigned x=0;x<SIZE;x++){unsigned px=ox+x*10,py=oy+2+y*4;uint8_t v=board[x][y];unsigned char color=(unsigned char)(0x10|((v%6)+2));for(unsigned yy=0;yy<3;yy++)for(unsigned xx=0;xx<9;xx++)cell(px+xx,py+yy,' ',color);if(v){uint32_t value=1u<<v;unsigned digits=1,t=value;while(t>=10){digits++;t/=10;}number(px+(9-digits)/2,py+1,value,(unsigned char)(color|8));}}
 text(ox,oy+19,"Arrow keys: move    R: restart    Q: quit",0x1f);os->terminal_cursor(0,0,0,1);
}
static void get_line(unsigned direction,unsigned line,uint8_t out[SIZE]){for(unsigned i=0;i<SIZE;i++){unsigned x,y;if(direction==0){x=i;y=line;}else if(direction==1){x=SIZE-1-i;y=line;}else if(direction==2){x=line;y=i;}else{x=line;y=SIZE-1-i;}out[i]=board[x][y];}}
static void put_line(unsigned direction,unsigned line,const uint8_t in[SIZE]){for(unsigned i=0;i<SIZE;i++){unsigned x,y;if(direction==0){x=i;y=line;}else if(direction==1){x=SIZE-1-i;y=line;}else if(direction==2){x=line;y=i;}else{x=line;y=SIZE-1-i;}board[x][y]=in[i];}}
static int move(unsigned direction){int changed=0;for(unsigned line=0;line<SIZE;line++){uint8_t old[SIZE],packed[SIZE]={0},merged[SIZE]={0};get_line(direction,line,old);unsigned n=0;for(unsigned i=0;i<SIZE;i++)if(old[i])packed[n++]=old[i];for(unsigned i=0;i+1<n;i++)if(packed[i]&&packed[i]==packed[i+1]){packed[i]++;score+=1u<<packed[i];for(unsigned j=i+1;j+1<SIZE;j++)packed[j]=packed[j+1];packed[SIZE-1]=0;n--;}for(unsigned i=0;i<SIZE;i++){merged[i]=packed[i];if(merged[i]!=old[i])changed=1;}put_line(direction,line,merged);}return changed;}
static int spawn(void){unsigned empty=0;for(unsigned y=0;y<SIZE;y++)for(unsigned x=0;x<SIZE;x++)if(!board[x][y])empty++;if(!empty)return 0;unsigned pick=random32()%empty;for(unsigned y=0;y<SIZE;y++)for(unsigned x=0;x<SIZE;x++)if(!board[x][y]){if(!pick){board[x][y]=(random32()%10)==0?2:1;return 1;}pick--;}return 0;}
static int can_move(void){for(unsigned y=0;y<SIZE;y++)for(unsigned x=0;x<SIZE;x++){if(!board[x][y])return 1;if(x+1<SIZE&&board[x][y]==board[x+1][y])return 1;if(y+1<SIZE&&board[x][y]==board[x][y+1])return 1;}return 0;}
static void reset(void){for(unsigned y=0;y<SIZE;y++)for(unsigned x=0;x<SIZE;x++)board[x][y]=0;score=0;spawn();spawn();}

int _start(const os64_api_t *api,const char *args){(void)args;if(!api||api->version!=OS64_ABI_VERSION)return 126;if(api->terminal_width()<41||api->terminal_height()<21){api->write("c2048: terminal must be at least 41x21\n");return 1;}if(!api->terminal_acquire())return 1;os=api;os64_datetime_t now;if(api->clock_get(&now))random_state^=(uint32_t)now.second|((uint32_t)now.minute<<8);reset();for(;;){draw();unsigned key=api->terminal_read_key();if(key=='q'||key=='Q'||key==3)break;if(key=='r'||key=='R'){reset();continue;}unsigned direction=key==KEY_LEFT?0:key==KEY_RIGHT?1:key==KEY_UP?2:key==KEY_DOWN?3:4;if(direction<4&&move(direction)){spawn();if(!can_move()){draw();text(1,1,"Game over - press R or Q",0x4f);for(;;){key=api->terminal_read_key();if(key=='r'||key=='R'){reset();break;}if(key=='q'||key=='Q'||key==3)goto done;}}}}done:api->terminal_release();return 0;}

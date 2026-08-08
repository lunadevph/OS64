#include "display.h"
#include "io.h"
#include "exceptions.h"
#include "ofp.h"
#include "font8x16.h"
#include "bochs_vbe.h"

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define FONT_WIDTH 8
#define FONT_HEIGHT 16
#define CELL_WIDTH 9
#define CELL_HEIGHT 17
#define MAX_WIDTH 216
#define MAX_HEIGHT 64
#define COM1 0x3f8
static volatile uint16_t *const vga = (uint16_t *)0xb8000;
static size_t row, col;
static size_t width=VGA_WIDTH,height=VGA_HEIGHT;
static uint8_t color = 7, fg = 7, bold;
static int serial_present,serial_cells_enabled=1;
static int initialized;
static unsigned long characters;
static uint16_t cells[MAX_WIDTH*MAX_HEIGHT];
static volatile uint8_t *framebuffer;
static uint32_t fb_pitch,fb_width,fb_height;
static uint8_t fb_bpp,fb_red_position,fb_green_position,fb_blue_position;
static size_t cursor_x,cursor_y;
static uint64_t cursor_blink_ns;
static int cursor_visible,cursor_full_block,cursor_drawn;

static const uint32_t palette[16]={
    0x000000,0x0000aa,0x00aa00,0x00aaaa,0xaa0000,0xaa00aa,0xaa5500,0xaaaaaa,
    0x555555,0x5555ff,0x55ff55,0x55ffff,0xff5555,0xff55ff,0xffff55,0xffffff
};
static uint8_t box_row(uint8_t c,unsigned y){
    int left=0,right=0,up=0,down=0,dbl=0;
    switch(c){
    case 179:up=down=1;break;case 196:left=right=1;break;
    case 218:right=down=1;break;case 191:left=down=1;break;case 192:right=up=1;break;case 217:left=up=1;break;
    case 195:right=up=down=1;break;case 180:left=up=down=1;break;case 194:left=right=down=1;break;case 193:left=right=up=1;break;case 197:left=right=up=down=1;break;
    case 186:dbl=1;up=down=1;break;case 205:dbl=1;left=right=1;break;
    case 201:dbl=1;right=down=1;break;case 187:dbl=1;left=down=1;break;case 200:dbl=1;right=up=1;break;case 188:dbl=1;left=up=1;break;
    case 204:dbl=1;right=up=down=1;break;case 185:dbl=1;left=up=down=1;break;case 203:dbl=1;left=right=down=1;break;case 202:dbl=1;left=right=up=1;break;case 206:dbl=1;left=right=up=down=1;break;
    case 219:return 0xff;case 178:return (y&1)?0xaa:0x55;case 177:return (y&1)?0x88:0x22;case 176:return (y&3)?0:0x22;default:return 0;
    }
    uint8_t bits=0;unsigned mid=7;
    if(dbl){if((up&&y<=mid)||(down&&y>=mid))bits|=0x24;if((left||right)&&(y==6||y==9)){if(left)bits|=0xf0;if(right)bits|=0x1f;}}
    else{if((up&&y<=mid)||(down&&y>=mid))bits|=0x10;if(y==mid){if(left)bits|=0xf0;if(right)bits|=0x1f;}}
    return bits;
}
static uint8_t glyph_row(uint8_t c,unsigned y){
    if(c<128)return os64_font8x16[(size_t)c*FONT_HEIGHT+y];
    return box_row(c,y);
}
uint8_t display_codepoint_glyph(uint32_t u){switch(u){case 0x2502:return 179;case 0x2524:return 180;case 0x2563:return 185;case 0x2551:return 186;case 0x2557:return 187;case 0x255d:return 188;case 0x2510:return 191;case 0x2514:return 192;case 0x2534:return 193;case 0x252c:return 194;case 0x251c:return 195;case 0x2500:return 196;case 0x253c:return 197;case 0x255a:return 200;case 0x2554:return 201;case 0x2569:return 202;case 0x2566:return 203;case 0x2560:return 204;case 0x2550:return 205;case 0x256c:return 206;case 0x2518:return 217;case 0x250c:return 218;case 0x2588:return 219;case 0x2591:return 176;case 0x2592:return 177;case 0x2593:return 178;default:return u<128?(uint8_t)u:'?';}}
static void vga_register(uint16_t port,uint8_t index,uint8_t value){
    outb(port,index);outb((uint16_t)(port+1),value);
}
static void vga_load_font(void){
    volatile uint8_t*font_memory=(volatile uint8_t*)(uintptr_t)0xa0000;
    vga_register(0x3c4,0x00,0x01);
    vga_register(0x3c4,0x02,0x04);
    vga_register(0x3c4,0x04,0x07);
    vga_register(0x3c4,0x00,0x03);
    vga_register(0x3ce,0x04,0x02);
    vga_register(0x3ce,0x05,0x00);
    vga_register(0x3ce,0x06,0x00);
    for(unsigned ch=0;ch<256;ch++){
        unsigned source=ch<128?ch:(unsigned)'?';
        for(unsigned y=0;y<32;y++)
            font_memory[ch*32+y]=y<FONT_HEIGHT?(ch<128?os64_font8x16[source*FONT_HEIGHT+y]:box_row((uint8_t)ch,y)):0;
    }
    vga_register(0x3c4,0x00,0x01);
    vga_register(0x3c4,0x02,0x03);
    vga_register(0x3c4,0x04,0x03);
    vga_register(0x3c4,0x00,0x03);
    vga_register(0x3ce,0x04,0x00);
    vga_register(0x3ce,0x05,0x10);
    vga_register(0x3ce,0x06,0x0e);
}
static void framebuffer_cell(size_t x,size_t y,uint8_t ch,uint8_t attribute){
if(!framebuffer)return;
    uint32_t foreground=palette[attribute&15],background=palette[(attribute>>4)&15];
    for(unsigned py=0;py<CELL_HEIGHT;py++){
        uint8_t bits=py<FONT_HEIGHT?glyph_row(ch,py):0;
        volatile uint32_t*line=(volatile uint32_t*)
            (framebuffer+(y*CELL_HEIGHT+py)*fb_pitch+x*CELL_WIDTH*4);
        for(unsigned px=0;px<CELL_WIDTH;px++)
            line[px]=(px<FONT_WIDTH&&(bits&(0x80u>>px)))?foreground:background;
    }
}
static void render_cell(size_t x,size_t y,uint8_t ch,uint8_t attribute){
    if(x>=width||y>=height)return;
    cells[y*MAX_WIDTH+x]=(uint16_t)((uint16_t)attribute<<8|ch);
    if(framebuffer)framebuffer_cell(x,y,ch,attribute);
    else vga[y*VGA_WIDTH+x]=(uint16_t)((uint16_t)attribute<<8|ch);
}
static void cursor_restore(void){
    if(!framebuffer||!cursor_drawn||cursor_x>=width||cursor_y>=height)return;
    uint16_t cell=cells[cursor_y*MAX_WIDTH+cursor_x];
    framebuffer_cell(cursor_x,cursor_y,(uint8_t)cell,(uint8_t)(cell>>8));
    cursor_drawn=0;
}
static void cursor_draw(void){
    if(!framebuffer||!cursor_visible||cursor_drawn||cursor_x>=width||cursor_y>=height)return;
    uint16_t cell=cells[cursor_y*MAX_WIDTH+cursor_x];
    uint8_t attribute=(uint8_t)(cell>>8);
    uint8_t inverted=(uint8_t)(((attribute&0x0f)<<4)|((attribute>>4)&0x0f));
    if(!cursor_full_block)inverted=(uint8_t)((attribute&0xf0)|15);
    framebuffer_cell(cursor_x,cursor_y,(uint8_t)cell,inverted);
    cursor_drawn=1;
}
static void framebuffer_info(uint64_t address){
    const uint8_t*p=(const uint8_t*)(uintptr_t)address;if(!p)return;uint32_t total=*(const uint32_t*)p;
    for(uint32_t off=8;off+8<=total;){uint32_t type=*(const uint32_t*)(p+off),size=*(const uint32_t*)(p+off+4);if(size<8)return;
        if(type==8&&size>=38){uint64_t base=*(const uint64_t*)(p+off+8);uint8_t kind=p[off+29];uint8_t bpp=p[off+28];uint32_t pitch=*(const uint32_t*)(p+off+16),w=*(const uint32_t*)(p+off+20),h=*(const uint32_t*)(p+off+24);
            if(kind==1&&bpp==32&&base<0x100000000ull&&w>=640&&h>=400){framebuffer=(volatile uint8_t*)(uintptr_t)base;fb_pitch=pitch;fb_width=w;fb_height=h;fb_bpp=bpp;fb_red_position=p[off+32];fb_green_position=p[off+34];fb_blue_position=p[off+36];(void)fb_bpp;(void)fb_red_position;(void)fb_green_position;(void)fb_blue_position;width=w/CELL_WIDTH;height=h/CELL_HEIGHT;if(width>MAX_WIDTH)width=MAX_WIDTH;if(height>MAX_HEIGHT)height=MAX_HEIGHT;return;}}
        off+=(size+7)&~7u;
    }
}

static void serial_put(char c) {
    if(!serial_present)return;
    for(unsigned wait=0;wait<1000000;wait++)if(inb(COM1+5)&0x20){outb(COM1,(uint8_t)c);return;}
}
static void serial_raw(const char *s) { while (*s) serial_put(*s++); }
static void serial_number(size_t n){char b[21];size_t i=0;if(!n){serial_put('0');return;}while(n){b[i++]=(char)('0'+n%10);n/=10;}while(i)serial_put(b[--i]);}
static void cursor_vga(void) { if(framebuffer)return;uint16_t p = (uint16_t)(row * VGA_WIDTH + col); outb(0x3d4,14); outb(0x3d5,p>>8); outb(0x3d4,15); outb(0x3d5,p); }
static void scroll(void) {
    if (row < height) return;
    for (size_t y=1;y<height;y++) for(size_t x=0;x<width;x++){uint16_t v=cells[y*MAX_WIDTH+x];cells[(y-1)*MAX_WIDTH+x]=v;render_cell(x,y-1,(uint8_t)v,(uint8_t)(v>>8));}
    for(size_t x=0;x<width;x++)render_cell(x,height-1,' ',color);
    row=height-1;
}
static void apply(void) { color=(uint8_t)(fg|(bold?8:0)); serial_raw("\033[0;"); if(bold)serial_raw("1;"); serial_raw(fg==1?"34m":fg==2?"32m":fg==3?"36m":fg==4?"31m":fg==6?"33m":"37m"); }
static display_output_sink_t output_sink;
static void *output_sink_context;
void display_output_sink(display_output_sink_t sink,void *context){output_sink=sink;output_sink_context=context;}
static int tag(const unsigned char *p,size_t n,const char *s){size_t i=0;while(s[i]&&i<n&&p[i]==(unsigned char)s[i])i++;return !s[i];}

void display_init(uint64_t multiboot_address) {
    serial_present=0;initialized=0;characters=0;
    output_sink=0;output_sink_context=0;
    cursor_x=cursor_y=0;cursor_blink_ns=0;cursor_visible=cursor_full_block=cursor_drawn=0;
    framebuffer=0;framebuffer_info(multiboot_address);
    if(!framebuffer)vga_load_font();
    outb(COM1+1,0);
    outb(COM1+3,0x80);
    outb(COM1,3);
    outb(COM1+1,0);
    outb(COM1+3,3);
    outb(COM1+2,0xc7);
    outb(COM1+4,0x0b);
    if(inb(COM1+5)!=0xff)serial_present=1;
    display_clear();initialized=1;exceptions_init();ofp_init();
}
int display_serial_available(void){return serial_present;}
void display_color(uint8_t c) { fg=(uint8_t)(c&7);bold=(uint8_t)((c&8)!=0);apply(); }
void display_putc(char c) {
    if(output_sink){characters++;output_sink(c,output_sink_context);serial_put(c=='\n'?'\r':c);if(c=='\n')serial_put('\n');return;}
    cursor_restore();
    characters++;
    if(c=='\n'){col=0;row++;serial_put('\r');serial_put('\n');}
    else if(c=='\b'){if(col){col--;render_cell(col,row,' ',color);serial_raw("\b \b");}}
    else{render_cell(col,row,(uint8_t)c,color);col++;serial_put(c);if(col==width){col=0;row++;}}
    scroll();cursor_x=col;cursor_y=row;cursor_draw();cursor_vga();
}
int display_ready(void){return initialized;}
size_t display_width(void){return width;}
size_t display_height(void){return height;}
unsigned long display_characters(void){return characters;}
void display_cursor_left(void){size_t p=row*width+col;if(p){cursor_restore();p--;row=p/width;col=p%width;cursor_x=col;cursor_y=row;cursor_draw();serial_raw("\033[D");cursor_vga();}}
void display_cursor_right(void){size_t p=row*width+col;if(p+1<width*height){cursor_restore();p++;row=p/width;col=p%width;cursor_x=col;cursor_y=row;cursor_draw();serial_raw("\033[C");cursor_vga();}}
uint16_t display_cell(size_t x,size_t y){return x<width&&y<height?cells[y*MAX_WIDTH+x]:0;}
static void serial_cell(uint8_t ch){
 static const uint16_t unicode[29]={0x2502,0x2524,0x2561,0x2562,0x2556,0x2555,0x2563,0x2551,0x2557,0x255d,0x255c,0x255b,0x2510,0x2514,0x2534,0x252c,0x251c,0x2500,0x253c,0x255e,0x255f,0x255a,0x2554,0x2569,0x2566,0x2560,0x2550,0x256c,0x2567};
 static const uint8_t cp[29]={179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207};
 uint16_t u=0;for(unsigned i=0;i<29;i++)if(ch==cp[i]){u=unicode[i];break;}
 if(ch==217)u=0x2518;else if(ch==218)u=0x250c;else if(ch==219)u=0x2588;else if(ch==176)u=0x2591;else if(ch==177)u=0x2592;else if(ch==178)u=0x2593;
 if(!u){serial_put(ch<128?(char)ch:'?');return;}serial_put((char)(0xe0|(u>>12)));serial_put((char)(0x80|((u>>6)&63)));serial_put((char)(0x80|(u&63)));
}
void display_set_cell(size_t x,size_t y,uint8_t ch,uint8_t attribute){static const uint8_t ansi[]={0,4,2,6,1,5,3,7};if(x>=width||y>=height)return;render_cell(x,y,ch,attribute);if(serial_present&&serial_cells_enabled){serial_raw("\033[");serial_number(y+1);serial_put(';');serial_number(x+1);serial_raw("H\033[");serial_number((attribute&8)?90u+ansi[attribute&7]:30u+ansi[attribute&7]);serial_put(';');serial_number(40u+ansi[(attribute>>4)&7]);serial_put('m');serial_cell(ch);}}
void display_cursor_set(size_t x,size_t y,int visible,int full_block){
    if(x>=width)x=width-1;
    if(y>=height)y=height-1;
    cursor_restore();
    row=y;col=x;cursor_x=x;cursor_y=y;cursor_visible=visible!=0;
    cursor_full_block=full_block!=0;cursor_blink_ns=0;
    if(framebuffer)cursor_draw();
    else{outb(0x3d4,0x0a);outb(0x3d5,visible?(uint8_t)(full_block?0:0x0e):0x20);outb(0x3d4,0x0b);outb(0x3d5,0x0f);cursor_vga();}
    if(serial_present)serial_raw(visible?"\033[?25h":"\033[?25l");
}
void display_cursor_tick(uint64_t monotonic_ns){
    if(!framebuffer||!cursor_visible)return;
    if(!cursor_blink_ns){cursor_blink_ns=monotonic_ns;return;}
    if(monotonic_ns-cursor_blink_ns<500000000ull)return;
    if(cursor_drawn)cursor_restore();else cursor_draw();
    cursor_blink_ns=monotonic_ns;
}
void display_cursor_activity(uint64_t monotonic_ns){
    if(!framebuffer||!cursor_visible)return;
    if(cursor_drawn)cursor_restore();
    cursor_draw();cursor_blink_ns=monotonic_ns;
}
void display_position(size_t*x,size_t*y){if(x)*x=col;if(y)*y=row;}
int display_framebuffer_active(void){return framebuffer!=0;}
int display_mode_supported(void){return framebuffer&&bochs_vbe_available();}
unsigned display_pixel_width(void){return framebuffer?fb_width:720u;}
unsigned display_pixel_height(void){return framebuffer?fb_height:400u;}
uint32_t display_graphics_get_pixel(unsigned x,unsigned y){if(!framebuffer||x>=fb_width||y>=fb_height)return 0;volatile uint32_t*line=(volatile uint32_t*)(framebuffer+y*fb_pitch);return line[x];}
void display_graphics_put_pixel(unsigned x,unsigned y,uint32_t value){if(!framebuffer||x>=fb_width||y>=fb_height)return;volatile uint32_t*line=(volatile uint32_t*)(framebuffer+y*fb_pitch);line[x]=value;}
void display_graphics_fill(unsigned x,unsigned y,unsigned w,unsigned h,uint32_t value){if(!framebuffer||x>=fb_width||y>=fb_height)return;if(w>fb_width-x)w=fb_width-x;if(h>fb_height-y)h=fb_height-y;for(unsigned yy=0;yy<h;yy++){volatile uint32_t*line=(volatile uint32_t*)(framebuffer+(y+yy)*fb_pitch)+x;for(unsigned xx=0;xx<w;xx++)line[xx]=value;}}
void display_graphics_text(unsigned x,unsigned y,const char*text,uint32_t foreground,uint32_t background,unsigned scale){if(!framebuffer||!text)return;if(!scale)scale=1;while(*text){uint8_t ch=(uint8_t)*text++;for(unsigned gy=0;gy<FONT_HEIGHT;gy++){uint8_t bits=glyph_row(ch,gy);for(unsigned gx=0;gx<FONT_WIDTH;gx++){uint32_t pixel=(bits&(0x80u>>gx))?foreground:background;display_graphics_fill(x+gx*scale,y+gy*scale,scale,scale,pixel);}}x+=FONT_WIDTH*scale;}}
int display_set_mode(unsigned new_width,unsigned new_height){
    if(!framebuffer||new_width>1920||new_height>1200||!bochs_vbe_set_mode((uint16_t)new_width,(uint16_t)new_height,32))return 0;
    cursor_restore();fb_width=new_width;fb_height=new_height;fb_pitch=new_width*4u;
    width=new_width/CELL_WIDTH;height=new_height/CELL_HEIGHT;
    if(width>MAX_WIDTH)width=MAX_WIDTH;
    if(height>MAX_HEIGHT)height=MAX_HEIGHT;
    row=col=cursor_x=cursor_y=0;cursor_drawn=0;
    for(unsigned y=0;y<fb_height;y++){
        volatile uint32_t*line=(volatile uint32_t*)(framebuffer+y*fb_pitch);
        for(unsigned x=0;x<fb_width;x++)line[x]=palette[(color>>4)&15];
    }
    display_clear();return 1;
}
void display_puts(const char *s){while(*s)display_putc(*s++);}
void display_clear(void){for(size_t y=0;y<height;y++)for(size_t x=0;x<width;x++)render_cell(x,y,' ',color);row=col=0;cursor_vga();serial_raw("\033[2J\033[H");}
void display_number(uint64_t n){char b[21];size_t i=0;if(!n){display_putc('0');return;}while(n){b[i++]=(char)('0'+n%10);n/=10;}while(i)display_putc(b[--i]);}
void display_serial_cells(int enabled){serial_cells_enabled=enabled!=0;}
void display_serial_clear(void){serial_raw("\033[2J\033[H");}
void display_styled(const unsigned char *t,size_t n){
    for(size_t i=0;i<n;){size_t l=n-i;
        if(tag(t+i,l,"(bold)")){bold=1;apply();i+=6;}else if(tag(t+i,l,"(/bold)")){bold=0;apply();i+=7;}
        else if(tag(t+i,l,"(green)")){fg=2;apply();i+=7;}else if(tag(t+i,l,"(yellow)")){fg=6;apply();i+=8;}
        else if(tag(t+i,l,"(white)")){fg=7;apply();i+=7;}else if(tag(t+i,l,"(reset)")){fg=7;bold=0;apply();i+=7;}
        else display_putc((char)t[i++]);}
    fg=7;bold=0;apply();
}

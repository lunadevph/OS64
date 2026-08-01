#include "bochs_vbe.h"
#include "io.h"

#define VBE_INDEX 0x01ceu
#define VBE_DATA  0x01cfu
#define VBE_ID 0u
#define VBE_XRES 1u
#define VBE_YRES 2u
#define VBE_BPP 3u
#define VBE_ENABLE 4u
#define VBE_VIRT_WIDTH 6u
#define VBE_ENABLED 0x01u
#define VBE_LFB 0x40u
#define VBE_NOCLEARMEM 0x80u

static uint16_t read_reg(uint16_t index){outw(VBE_INDEX,index);return inw(VBE_DATA);}
static void write_reg(uint16_t index,uint16_t value){outw(VBE_INDEX,index);outw(VBE_DATA,value);}

int bochs_vbe_available(void){uint16_t id=read_reg(VBE_ID);return id>=0xb0c0u&&id<=0xb0c5u;}
int bochs_vbe_set_mode(uint16_t width,uint16_t height,uint16_t bpp){
    if(!bochs_vbe_available()||bpp!=32||width<640||height<400||width>1920||height>1200)return 0;
    write_reg(VBE_ENABLE,0);write_reg(VBE_XRES,width);write_reg(VBE_YRES,height);
    write_reg(VBE_BPP,bpp);write_reg(VBE_VIRT_WIDTH,width);
    write_reg(VBE_ENABLE,VBE_ENABLED|VBE_LFB|VBE_NOCLEARMEM);
    return read_reg(VBE_XRES)==width&&read_reg(VBE_YRES)==height&&read_reg(VBE_BPP)==bpp;
}

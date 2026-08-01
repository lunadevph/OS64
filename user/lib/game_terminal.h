#ifndef OS64_GAME_TERMINAL_H
#define OS64_GAME_TERMINAL_H

static const os64_api_t *g_api;
static uint16_t game_terminal_front[W*H];
static uint8_t game_terminal_front_valid;

static void vga_flush(void){
    if(!g_api)return;
    for(unsigned y=0;y<H;y++)for(unsigned x=0;x<W;x++){
        unsigned offset=y*W+x;
        uint16_t cell=vga_buf[offset];
        if(game_terminal_front_valid&&game_terminal_front[offset]==cell)continue;
        game_terminal_front[offset]=cell;
        g_api->terminal_write_cell(x,y,(uint8_t)cell,(uint8_t)(cell>>8));
    }
    game_terminal_front_valid=1;
}

static uint8_t read_key_raw(void){
    if(!g_api||!g_api->terminal_poll_key)return 0;
    unsigned int k=g_api->terminal_poll_key();
    if(!k)return 0;
    if(k==0x11)return 72;
    if(k==0x12)return 80;
    if(k==0x13)return 75;
    if(k==0x14)return 77;
    if(k==27)return 1;
    if(k=='\n')return 28;
    if(k==' ')return 57;
    if(k=='\b'||k==127)return 14;
    if(k>='1'&&k<='9')return (uint8_t)(k-'1'+2);
    if(k=='0')return 11;
    if(k=='-')return 12;
    if(k=='=')return 13;
    if(k=='/')return 53;
    if(k=='*')return 55;
    if(k=='+')return 78;
    if(k==',')return 51;
    if(k=='.')return 52;
    static const char row1[]="qwertyuiop",row2[]="asdfghjkl",row3[]="zxcvbnm";
    for(unsigned i=0;row1[i];i++)if((k|32)==(unsigned)row1[i])return (uint8_t)(16+i);
    for(unsigned i=0;row2[i];i++)if((k|32)==(unsigned)row2[i])return (uint8_t)(30+i);
    for(unsigned i=0;row3[i];i++)if((k|32)==(unsigned)row3[i])return (uint8_t)(44+i);
    return 0;
}

#endif

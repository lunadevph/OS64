#include "ext_variant.h"
#include "ext2.h"
#include "disk.h"
#include <stdint.h>
#define BS 4096u
#define BASE_BLOCK_SECTORS 8u
#define JOURNAL_FIRST 261u
#define JOURNAL_BLOCKS 1024u
#define JOURNAL_INDIRECT 1285u
static unsigned current=2;
static uint16_t r16(const uint8_t*b,unsigned o){return (uint16_t)(b[o]|((uint16_t)b[o+1]<<8));}static uint32_t r32(const uint8_t*b,unsigned o){return r16(b,o)|((uint32_t)r16(b,o+2)<<16);}static void w16(uint8_t*b,unsigned o,uint16_t v){b[o]=(uint8_t)v;b[o+1]=(uint8_t)(v>>8);}static void w32(uint8_t*b,unsigned o,uint32_t v){w16(b,o,(uint16_t)v);w16(b,o+2,(uint16_t)(v>>16));}static void be32(uint8_t*b,unsigned o,uint32_t v){b[o]=(uint8_t)(v>>24);b[o+1]=(uint8_t)(v>>16);b[o+2]=(uint8_t)(v>>8);b[o+3]=(uint8_t)v;}static void zero(uint8_t*b){for(unsigned i=0;i<BS;i++)b[i]=0;}
static int rd(unsigned block,uint8_t*b){uint32_t base=disk_var_start()+block*BASE_BLOCK_SECTORS;for(unsigned i=0;i<8;i++)if(!disk_read(base+i,b+i*512))return 0;return 1;}static int wr(unsigned block,const uint8_t*b){uint32_t base=disk_var_start()+block*BASE_BLOCK_SECTORS;for(unsigned i=0;i<8;i++)if(!disk_write(base+i,b+i*512))return 0;return 1;}
static int add_journal(void){uint8_t b[BS];/* Reserve journal data plus its single-indirect block. */if(!rd(2,b))return 0;for(unsigned n=JOURNAL_FIRST;n<=JOURNAL_INDIRECT;n++)b[n>>3]|=(uint8_t)(1u<<(n&7));if(!wr(2,b))return 0;if(!rd(0,b))return 0;w32(b,1024+12,r32(b,1024+12)-(JOURNAL_BLOCKS+1));w32(b,1024+92,r32(b,1024+92)|4u);w32(b,1024+224,8);if(!wr(0,b)||!rd(1,b))return 0;w16(b,12,(uint16_t)(r16(b,12)-(JOURNAL_BLOCKS+1)));if(!wr(1,b))return 0;/* Reserved inode 8 becomes the standard internal journal inode. */if(!rd(4,b))return 0;uint8_t*i=b+7*128;for(unsigned n=0;n<128;n++)i[n]=0;w16(i,0,0100600);w32(i,4,JOURNAL_BLOCKS*BS);w16(i,26,1);w32(i,28,(JOURNAL_BLOCKS+1)*8);for(unsigned n=0;n<12;n++)w32(i,40+n*4,JOURNAL_FIRST+n);w32(i,88,JOURNAL_INDIRECT);if(!wr(4,b))return 0;zero(b);for(unsigned n=0;n<JOURNAL_BLOCKS-12;n++)w32(b,n*4,JOURNAL_FIRST+12+n);if(!wr(JOURNAL_INDIRECT,b))return 0;zero(b);be32(b,0,0xc03b3998u);be32(b,4,4);be32(b,8,0);be32(b,12,BS);be32(b,16,JOURNAL_BLOCKS);be32(b,20,1);be32(b,24,1);be32(b,28,0);if(!wr(JOURNAL_FIRST,b))return 0;zero(b);for(unsigned n=1;n<JOURNAL_BLOCKS;n++)if(!wr(JOURNAL_FIRST+n,b))return 0;return disk_sync();}
int ext_variant_format(unsigned level){if(level<2||level>4)return 0;if(!ext2_format())return 0;if(level>=3&&!add_journal())return 0;if(level==4){uint8_t b[BS];if(!rd(0,b))return 0;w32(b,1024+96,r32(b,1024+96)|0x40u);if(!wr(0,b)||!disk_sync())return 0;}current=level;return 1;}
unsigned ext_variant_detect(void){uint8_t b[BS];if(!rd(0,b)||r16(b,1024+56)!=0xef53)return 0;uint32_t compat=r32(b,1024+92),incompat=r32(b,1024+96);current=(incompat&0x40)?4:((compat&4)?3:2);return current;}const char*ext_variant_name(void){return current==4?"ext4":current==3?"ext3":"ext2";}

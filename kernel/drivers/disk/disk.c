#include "disk.h"
#include "io.h"

#define DISK_SECTORS 262144u
#define HOME_START 2048u
#define HOME_SECTORS 129024u
#define VAR_START 131072u
#define VAR_SECTORS 131072u

static uint32_t volume_start=HOME_START,reserved=32,fat_sectors=993,data_start;
static uint32_t root_cluster=2,next_cluster=3,free_clusters=126997;
static uint8_t fat_count=2,sectors_per_cluster=1;
static char label[17]="unformatted";static int formatted,mounted,dirty;
static void zero(uint8_t*b){for(int i=0;i<512;i++)b[i]=0;}static void w16(uint8_t*b,int o,uint16_t v){b[o]=v;b[o+1]=v>>8;}static void w32(uint8_t*b,int o,uint32_t v){w16(b,o,v);w16(b,o+2,v>>16);}static uint16_t r16(const uint8_t*b,int o){return b[o]|((uint16_t)b[o+1]<<8);}static uint32_t r32(const uint8_t*b,int o){return r16(b,o)|((uint32_t)r16(b,o+2)<<16);}
static int wait_ready(void){for(unsigned n=0;n<1000000;n++)if(!(inb(0x1f7)&0x80))return 1;return 0;}static int wait_data(void){for(unsigned n=0;n<1000000;n++){uint8_t s=inb(0x1f7);if(s&1)return 0;if(s&8)return 1;}return 0;}
static int select(uint32_t lba,uint8_t cmd){if(!wait_ready())return 0;outb(0x1f6,(uint8_t)(0xe0|((lba>>24)&15)));for(int i=0;i<4;i++)inb(0x3f6);outb(0x1f2,1);outb(0x1f3,lba);outb(0x1f4,lba>>8);outb(0x1f5,lba>>16);outb(0x1f7,cmd);return 1;}
int disk_read(uint32_t lba,uint8_t*b){if(!select(lba,0x20)||!wait_data())return 0;for(int i=0;i<256;i++){uint16_t w=inw(0x1f0);b[i*2]=w;b[i*2+1]=w>>8;}return 1;}int disk_write(uint32_t lba,const uint8_t*b){if(!select(lba,0x30)||!wait_data())return 0;for(int i=0;i<256;i++)outw(0x1f0,(uint16_t)(b[i*2]|((uint16_t)b[i*2+1]<<8)));dirty=1;return wait_ready();}
uint32_t disk_var_start(void){return VAR_START;}uint32_t disk_var_sectors(void){return VAR_SECTORS;}
static uint32_t cluster_lba(uint32_t c){return volume_start+data_start+(c-2)*sectors_per_cluster;}
static int fat_set(uint32_t c,uint32_t value){uint32_t off=c*4,sec=off/512,pos=off%512;uint8_t b[512];for(uint8_t f=0;f<fat_count;f++){uint32_t lba=volume_start+reserved+(uint32_t)f*fat_sectors+sec;if(!disk_read(lba,b))return 0;w32(b,pos,value);if(!disk_write(lba,b))return 0;}return 1;}
static uint32_t fat_get(uint32_t c){uint8_t b[512];uint32_t off=c*4;if(!disk_read(volume_start+reserved+off/512,b))return 0x0fffffff;return r32(b,off%512)&0x0fffffff;}
static int fsinfo_update(void){uint8_t b[512];zero(b);w32(b,0,0x41615252);w32(b,484,0x61417272);w32(b,488,free_clusters);w32(b,492,next_cluster);w32(b,508,0xaa550000);return disk_write(volume_start+1,b)&&disk_write(volume_start+7,b);}
static uint32_t allocate(void){uint32_t max=(HOME_SECTORS-data_start)/sectors_per_cluster+2;for(uint32_t c=next_cluster;c<max;c++)if(fat_get(c)==0){if(!fat_set(c,0x0fffffff))return 0;next_cluster=c+1;if(free_clusters)free_clusters--;if(!fsinfo_update())return 0;uint8_t b[512];zero(b);if(!disk_write(cluster_lba(c),b))return 0;return c;}return 0;}
int disk_sync(void){if(!dirty)return 1;if(!wait_ready())return 0;outb(0x1f7,0xe7);if(!wait_ready())return 0;dirty=0;return 1;}void disk_unmount(void){disk_sync();mounted=0;}
int disk_init(void){uint8_t m[512],b[512];formatted=mounted=dirty=0;if(!disk_read(0,m))return 0;if(m[510]!=0x55||m[511]!=0xaa||m[450]!=0x0c)return 1;volume_start=r32(m,454);if(!disk_read(volume_start,b))return 0;if(b[510]!=0x55||b[511]!=0xaa||r16(b,11)!=512||r32(b,82)!=0x33544146)return 1;sectors_per_cluster=b[13];reserved=r16(b,14);fat_count=b[16];fat_sectors=r32(b,36);root_cluster=r32(b,44);data_start=reserved+(uint32_t)fat_count*fat_sectors;for(int i=0;i<11;i++)label[i]=(char)b[71+i];label[11]=0;if(disk_read(volume_start+1,b)){free_clusters=r32(b,488);next_cluster=r32(b,492);if(next_cluster<3||next_cluster==0xffffffff)next_cluster=3;}formatted=1;return 1;}
int disk_formatted(void){return formatted;}int disk_mounted(void){return mounted;}int disk_mount(void){if(!formatted)return 0;mounted=1;return 1;}const char*disk_label(void){return label;}
int disk_format(void){uint8_t b[512];formatted=mounted=0;volume_start=HOME_START;reserved=32;fat_count=2;fat_sectors=993;sectors_per_cluster=1;root_cluster=2;data_start=2018;next_cluster=3;free_clusters=HOME_SECTORS-data_start-1;
 zero(b);b[446+4]=0x0c;w32(b,446+8,HOME_START);w32(b,446+12,HOME_SECTORS);b[462+4]=0x7f;w32(b,462+8,VAR_START);w32(b,462+12,VAR_SECTORS);b[510]=0x55;b[511]=0xaa;if(!disk_write(0,b))return 0;
 zero(b);b[0]=0xeb;b[1]=0x58;b[2]=0x90;const char*o="OS64FAT ";for(int i=0;i<8;i++)b[3+i]=(uint8_t)o[i];w16(b,11,512);b[13]=1;w16(b,14,32);b[16]=2;b[21]=0xf8;w16(b,24,63);w16(b,26,16);w32(b,28,HOME_START);w32(b,32,HOME_SECTORS);w32(b,36,fat_sectors);w32(b,44,2);w16(b,48,1);w16(b,50,6);b[64]=0x80;b[66]=0x29;w32(b,67,0x64060001);const char*l="OS64 HOME  ";for(int i=0;i<11;i++)b[71+i]=(uint8_t)l[i];const char*t="FAT32   ";for(int i=0;i<8;i++)b[82+i]=(uint8_t)t[i];b[510]=0x55;b[511]=0xaa;if(!disk_write(HOME_START,b)||!disk_write(HOME_START+6,b))return 0;
 zero(b);w32(b,0,0x41615252);w32(b,484,0x61417272);w32(b,488,free_clusters);w32(b,492,next_cluster);w32(b,508,0xaa550000);if(!disk_write(HOME_START+1,b)||!disk_write(HOME_START+7,b))return 0;
 zero(b);for(uint8_t f=0;f<2;f++)for(uint32_t s=0;s<fat_sectors;s++){if(s==0){w32(b,0,0x0ffffff8);w32(b,4,0xffffffff);w32(b,8,0x0fffffff);}if(!disk_write(HOME_START+32+(uint32_t)f*fat_sectors+s,b))return 0;if(s==0)zero(b);}zero(b);for(int i=0;i<11;i++)b[i]=(uint8_t)l[i];b[11]=8;if(!disk_write(cluster_lba(2),b))return 0;formatted=mounted=1;for(int i=0;i<11;i++)label[i]=l[i];label[11]=0;return disk_sync();}
static const char*rel(const char*n){const char*p="/home/";if(n[0]=='/'&&n[1]=='m'){p="/mnt/os64/";}while(*p&&*n==*p){p++;n++;}return n;}static void name83(const char*n,uint8_t*out){for(int i=0;i<11;i++)out[i]=' ';n=rel(n);int i=0,j=8;while(*n&&*n!='.'&&*n!='/'&&i<8){char c=*n++;out[i++]=(uint8_t)(c>='a'&&c<='z'?c-32:c);}while(*n&&*n!='.')n++;if(*n=='.'){n++;while(*n&&j<11){char c=*n++;out[j++]=(uint8_t)(c>='a'&&c<='z'?c-32:c);}}}static int same11(const uint8_t*a,const uint8_t*b){for(int i=0;i<11;i++)if(a[i]!=b[i])return 0;return 1;}static void printable(const uint8_t*n,char*out){int p=0;for(int i=0;i<8&&n[i]!=' ';i++)out[p++]=(char)n[i];if(n[8]!=' '){out[p++]='.';for(int i=8;i<11&&n[i]!=' ';i++)out[p++]=(char)n[i];}out[p]=0;}
static int root_read(uint8_t*b){return disk_read(cluster_lba(root_cluster),b);}
static int dir_read(uint32_t cluster,uint8_t*b){return disk_read(cluster_lba(cluster),b);}
static int find_entry_at(uint32_t cluster,const char*n,uint8_t*r,int*slot){uint8_t key[11];name83(n,key);if(!dir_read(cluster,r))return 0;for(int i=0;i<16;i++){uint8_t*e=r+i*32;if(e[0]&&e[0]!=0xe5&&same11(e,key)){*slot=i;return 1;}}return 0;}
static uint32_t entry_cluster(const uint8_t*e){return ((uint32_t)r16(e,20)<<16)|r16(e,26);}
static int parent_dir(const char*path,uint32_t*parent,const char**leaf){
 const char*p=rel(path);uint32_t cluster=root_cluster;while(*p=='/')p++;
 if(!*p)return 0;
 for(;;){const char*slash=p;while(*slash&&*slash!='/')slash++;if(!*slash){*parent=cluster;*leaf=p;return 1;}
  char component[13];unsigned n=0;while(p<slash&&n+1<sizeof component)component[n++]=*p++;component[n]=0;
  uint8_t dir[512];int slot;if(!find_entry_at(cluster,component,dir,&slot)||!(dir[slot*32+11]&0x10))return 0;
  cluster=entry_cluster(dir+slot*32);p=slash+1;while(*p=='/')p++;if(!*p)return 0;
 }
}
static int find_entry(const char*n,uint8_t*r,int*slot){uint32_t parent;const char*leaf;return parent_dir(n,&parent,&leaf)&&find_entry_at(parent,leaf,r,slot);}
int disk_file_at(unsigned wanted,disk_file_t*f){if(!mounted)return 0;uint8_t r[512];if(!root_read(r))return 0;unsigned seen=0;for(int i=0;i<16;i++){uint8_t*e=r+i*32;if(e[0]&&e[0]!=0xe5&&!(e[11]&8)){if(seen++==wanted){printable(e,f->name);f->type=(e[11]&0x10)?1:0;f->size=r32(e,28);return 1;}}}return 0;}
int disk_create(const char*n,uint8_t type){if(!mounted)return 0;uint32_t parent;const char*leaf;if(!parent_dir(n,&parent,&leaf))return 0;uint8_t r[512];int slot;if(find_entry_at(parent,leaf,r,&slot))return 1;for(slot=0;slot<16;slot++)if(!r[slot*32]||r[slot*32]==0xe5)break;if(slot==16)return 0;uint32_t c=type?allocate():0;if(type&&!c)return 0;uint8_t*e=r+slot*32;for(int i=0;i<32;i++)e[i]=0;name83(leaf,e);e[11]=type?0x10:0x20;w16(e,20,c>>16);w16(e,26,c);if(type){uint8_t d[512];zero(d);for(int i=0;i<11;i++)d[i]=' ';d[0]='.';d[11]=0x10;w16(d,20,c>>16);w16(d,26,c);for(int i=0;i<11;i++)d[32+i]=' ';d[32]='.';d[33]='.';d[43]=0x10;uint32_t dotdot=parent==root_cluster?0:parent;w16(d,52,dotdot>>16);w16(d,58,dotdot);if(!disk_write(cluster_lba(c),d))return 0;}return disk_write(cluster_lba(parent),r);}
int disk_store(const char*n,const unsigned char*d,uint32_t size){if(size>512||!disk_create(n,0))return 0;uint32_t parent;const char*leaf;if(!parent_dir(n,&parent,&leaf))return 0;uint8_t r[512];int slot;if(!find_entry_at(parent,leaf,r,&slot))return 0;uint8_t*e=r+slot*32;if(e[11]&0x10)return 0;uint32_t c=entry_cluster(e);if(size&&!c){c=allocate();if(!c)return 0;w16(e,20,c>>16);w16(e,26,c);}uint8_t b[512];for(unsigned i=0;i<512;i++)b[i]=i<size?d[i]:0;w32(e,28,size);return (!size||disk_write(cluster_lba(c),b))&&disk_write(cluster_lba(parent),r);}
int disk_install_boot_area(const unsigned char*d,size_t size){if(!d||size!=HOME_START*512u)return 0;for(uint32_t s=0;s<HOME_START;s++)if(!disk_write(s,d+(size_t)s*512))return 0;return disk_sync();}
int disk_store_large(const char*n,const unsigned char*d,size_t size){if(!mounted||!d||!size||size>HOME_SECTORS*512u)return 0;uint8_t r[512];int slot;if(find_entry(n,r,&slot)){r[slot*32]=0xe5;if(!disk_write(cluster_lba(root_cluster),r))return 0;}if(!disk_create(n,0)||!find_entry(n,r,&slot))return 0;uint8_t*e=r+slot*32;uint32_t tail=allocate();if(!tail)return 0;w16(e,20,tail>>16);w16(e,26,tail);size_t at=0;uint8_t b[512];for(;;){for(unsigned i=0;i<512;i++)b[i]=at<size?d[at++]:0;if(!disk_write(cluster_lba(tail),b))return 0;if(at>=size)break;uint32_t next=allocate();if(!next||!fat_set(tail,next))return 0;tail=next;}w32(e,28,(uint32_t)size);return disk_write(cluster_lba(root_cluster),r)&&disk_sync();}
int disk_load(const char*n,unsigned char*d,uint32_t*size){uint8_t r[512];int slot;if(!mounted||!find_entry(n,r,&slot))return 0;uint8_t*e=r+slot*32;if(e[11]&0x10)return 0;uint32_t c=entry_cluster(e);*size=r32(e,28);return disk_read(cluster_lba(c),d);}
int disk_remove(const char*n){uint32_t parent;const char*leaf;if(!mounted||!parent_dir(n,&parent,&leaf))return 0;uint8_t r[512];int slot;if(!find_entry_at(parent,leaf,r,&slot))return 0;uint8_t*e=r+slot*32;uint32_t c=entry_cluster(e);e[0]=0xe5;if(c){if(!fat_set(c,0))return 0;if(c<next_cluster)next_cluster=c;free_clusters++;if(!fsinfo_update())return 0;}return disk_write(cluster_lba(parent),r);}
uint32_t disk_free_clusters(void){return free_clusters;}
int disk_check(uint32_t*files,uint32_t*clusters){
 uint8_t m[512],boot[512],info[512],root[512],fat0[512],fat1[512];
 if(!disk_read(0,m)||m[510]!=0x55||m[511]!=0xaa||m[450]!=0x0c
    ||r32(m,454)!=volume_start||!disk_read(volume_start,boot)
    ||boot[510]!=0x55||boot[511]!=0xaa||r16(boot,11)!=512
    ||r32(boot,82)!=0x33544146||!disk_read(volume_start+1,info)
    ||r32(info,0)!=0x41615252||r32(info,484)!=0x61417272
    ||r32(info,508)!=0xaa550000||!root_read(root))return 0;
 for(uint32_t s=0;s<fat_sectors;s++){
  if(!disk_read(volume_start+reserved+s,fat0)
     ||!disk_read(volume_start+reserved+fat_sectors+s,fat1))return 0;
  for(unsigned i=0;i<512;i++)if(fat0[i]!=fat1[i])return 0;
 }
 uint32_t file_count=0,used_clusters=1,max=(HOME_SECTORS-data_start)/sectors_per_cluster+2;
 for(unsigned i=0;i<16;i++){uint8_t*e=root+i*32;if(!e[0]||e[0]==0xe5||(e[11]&8))continue;
  uint32_t c=((uint32_t)r16(e,20)<<16)|r16(e,26),needed=(r32(e,28)+511u)/512u,walked=0;
  if(!c||c>=max)return 0;
  while(c<0x0ffffff8){if(c<2||c>=max||walked++>max)return 0;used_clusters++;uint32_t next=fat_get(c);if(!next)return 0;c=next;}
  if(!(e[11]&0x10)&&walked<needed)return 0;
  file_count++;
 }
 if(files)*files=file_count;
 if(clusters)*clusters=used_clusters;
 return 1;
}
uint32_t disk_fill(uint32_t limit){if(!mounted||!limit)return 0;uint8_t r[512];int slot;if(!find_entry("FILL.BIN",r,&slot)){if(!disk_create("FILL.BIN",0)||!find_entry("FILL.BIN",r,&slot))return 0;}uint8_t*e=r+slot*32;uint32_t first=((uint32_t)r16(e,20)<<16)|r16(e,26),tail=first,used=1;while(fat_get(tail)<0x0ffffff8){tail=fat_get(tail);used++;}uint32_t added=0;while(added<limit){uint32_t c=allocate();if(!c)break;if(!fat_set(tail,c)){fat_set(c,0);break;}tail=c;added++;}w32(e,28,(used+added)*512u);if(!disk_write(cluster_lba(root_cluster),r)||!disk_sync())return 0;return added;}

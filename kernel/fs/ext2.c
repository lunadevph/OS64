#include "ext2.h"
#include "disk.h"

/* A deliberately small, standard ext2 revision-1 implementation.  The OS64
 * data partition is one 4 KiB block group.  Files use the twelve direct
 * blocks; directories use one block.  The resulting volume is readable by
 * ordinary Linux ext2 tools and does not use private on-disk structures. */
#define BS 4096u
#define SECTORS_PER_BLOCK 8u
#define INODES 8192u
#define INODE_SIZE 128u
#define BLOCK_BITMAP 2u
#define INODE_BITMAP 3u
#define INODE_TABLE 4u
#define INODE_TABLE_BLOCKS 256u
#define FIRST_DATA 260u
#define ROOT_INO 2u
#define EXT2_MAGIC 0xef53u
#define EXT2_DIR 0040000u
#define EXT2_FILE 0100000u

typedef struct{uint16_t mode,uid;uint32_t size,atime,ctime,mtime,dtime;uint16_t gid,links;uint32_t blocks,flags,osd1,block[15],generation,file_acl,dir_acl,faddr;uint8_t osd2[12];}__attribute__((packed))inode_t;
static uint32_t base,blocks;static int mounted;
static uint16_t r16(const uint8_t*b,unsigned o){return (uint16_t)(b[o]|((uint16_t)b[o+1]<<8));}
static uint32_t r32(const uint8_t*b,unsigned o){return r16(b,o)|((uint32_t)r16(b,o+2)<<16);}
static void w16(uint8_t*b,unsigned o,uint16_t v){b[o]=(uint8_t)v;b[o+1]=(uint8_t)(v>>8);}
static void w32(uint8_t*b,unsigned o,uint32_t v){w16(b,o,(uint16_t)v);w16(b,o+2,(uint16_t)(v>>16));}
static void zero(void*p,size_t n){uint8_t*b=p;while(n--)*b++=0;}
static int eqn(const char*a,const char*b,unsigned n){unsigned i=0;while(i<n&&a[i]==b[i])i++;return i==n;}
static int bread(uint32_t n,uint8_t*b){if(n>=blocks)return 0;for(unsigned i=0;i<8;i++)if(!disk_read(base+n*8+i,b+i*512))return 0;return 1;}
static int bwrite(uint32_t n,const uint8_t*b){if(n>=blocks)return 0;for(unsigned i=0;i<8;i++)if(!disk_write(base+n*8+i,b+i*512))return 0;return 1;}
static int inode_get(uint32_t ino,inode_t*out){if(!ino||ino>INODES)return 0;uint8_t b[BS];uint32_t off=(ino-1)*INODE_SIZE;if(!bread(INODE_TABLE+off/BS,b))return 0;const uint8_t*p=b+off%BS;for(unsigned i=0;i<sizeof *out;i++)((uint8_t*)out)[i]=p[i];return 1;}
static int inode_put(uint32_t ino,const inode_t*in){uint8_t b[BS];uint32_t off=(ino-1)*INODE_SIZE,lba=INODE_TABLE+off/BS;if(!bread(lba,b))return 0;uint8_t*p=b+off%BS;for(unsigned i=0;i<sizeof *in;i++)p[i]=((const uint8_t*)in)[i];return bwrite(lba,b);}
static int counters(int db,int di,int dd){uint8_t b[BS];if(!bread(0,b))return 0;uint32_t fb=r32(b,1024+12);uint32_t fi=r32(b,1024+16);w32(b,1024+12,(uint32_t)((int)fb+db));w32(b,1024+16,(uint32_t)((int)fi+di));if(!bwrite(0,b)||!bread(1,b))return 0;w16(b,12,(uint16_t)((int)r16(b,12)+db));w16(b,14,(uint16_t)((int)r16(b,14)+di));w16(b,16,(uint16_t)((int)r16(b,16)+dd));return bwrite(1,b);}
static uint32_t alloc_bit(uint32_t bitmap,unsigned first,unsigned limit,int inode){uint8_t b[BS];if(!bread(bitmap,b))return 0;for(unsigned i=first;i<limit;i++)if(!(b[i>>3]&(1u<<(i&7)))){b[i>>3]|=(uint8_t)(1u<<(i&7));if(!bwrite(bitmap,b)||!counters(inode?0:-1,inode?-1:0,0))return 0;return i+1;}return 0;}
static int free_bit(uint32_t bitmap,uint32_t number,int inode){if(!number)return 0;uint8_t b[BS];unsigned i=number-1;if(!bread(bitmap,b))return 0;b[i>>3]&=(uint8_t)~(1u<<(i&7));return bwrite(bitmap,b)&&counters(inode?0:1,inode?1:0,0);}
static uint32_t alloc_block(void){uint32_t n=alloc_bit(BLOCK_BITMAP,FIRST_DATA+1,blocks,0);return n?n-1:0;}
static uint32_t alloc_inode(void){return alloc_bit(INODE_BITMAP,10,INODES,1);}
static unsigned ideal(unsigned n){return (8u+n+3u)&~3u;}
static int dir_find(uint32_t dir,const char*name,unsigned len,uint32_t*ino){inode_t in;uint8_t b[BS];if(!inode_get(dir,&in)||!(in.mode&EXT2_DIR)||!in.block[0]||!bread(in.block[0],b))return 0;for(unsigned p=0;p+8<=BS;){uint32_t n=r32(b,p);uint16_t rec=r16(b,p+4);uint8_t nl=b[p+6];if(rec<8||p+rec>BS)return 0;if(n&&nl==len&&eqn(name,(char*)b+p+8,len)){if(ino)*ino=n;return 1;}p+=rec;}return 0;}
static int dir_add(uint32_t dir,const char*name,unsigned len,uint32_t ino,uint8_t type){if(!len||len>255)return 0;inode_t di;uint8_t b[BS];if(!inode_get(dir,&di)||!di.block[0]||!bread(di.block[0],b))return 0;unsigned need=ideal(len);for(unsigned p=0;p+8<=BS;){uint16_t rec=r16(b,p+4);uint8_t nl=b[p+6];if(rec<8||p+rec>BS)return 0;unsigned used=ideal(nl);if(r32(b,p)&&rec>=used+need){w16(b,p+4,(uint16_t)used);p+=used;w32(b,p,ino);w16(b,p+4,(uint16_t)(rec-used));b[p+6]=(uint8_t)len;b[p+7]=type;for(unsigned i=0;i<len;i++)b[p+8+i]=(uint8_t)name[i];return bwrite(di.block[0],b);}p+=rec;}return 0;}
static int make_dir(uint32_t parent,const char*name,unsigned len,uint32_t*out){uint32_t ino=alloc_inode(),blk=alloc_block();if(!ino||!blk)return 0;inode_t in;zero(&in,sizeof in);in.mode=(uint16_t)(EXT2_DIR|0755);in.links=2;in.size=BS;in.blocks=8;in.block[0]=blk;uint8_t b[BS];zero(b,BS);w32(b,0,ino);w16(b,4,12);b[6]=1;b[7]=2;b[8]='.';w32(b,12,parent);w16(b,16,BS-12);b[18]=2;b[19]=2;b[20]='.';b[21]='.';if(!bwrite(blk,b)||!inode_put(ino,&in)||!dir_add(parent,name,len,ino,2)||!counters(0,0,1))return 0;inode_t pi;if(inode_get(parent,&pi)){pi.links++;inode_put(parent,&pi);}*out=ino;return 1;}
static int walk(const char*path,int create,uint32_t*parent,char*leaf,unsigned*leaf_len,uint32_t*found){const char*p=path;while(*p=='/')p++;uint32_t cur=ROOT_INO;if(!*p){if(found)*found=cur;return 1;}for(;;){const char*s=p;while(*p&&*p!='/')p++;unsigned n=(unsigned)(p-s);while(*p=='/')p++;if(!*p){if(parent)*parent=cur;if(leaf){for(unsigned i=0;i<n;i++)leaf[i]=s[i];leaf[n]=0;}if(leaf_len)*leaf_len=n;if(found){uint32_t q;*found=dir_find(cur,s,n,&q)?q:0;}return 1;}uint32_t next;if(!dir_find(cur,s,n,&next)){if(!create||!make_dir(cur,s,n,&next))return 0;}cur=next;}}

int ext2_probe(void){uint8_t b[BS];base=disk_var_start();blocks=disk_var_sectors()/SECTORS_PER_BLOCK;return bread(0,b)&&r16(b,1024+56)==EXT2_MAGIC&&r32(b,1024+24)==2&&r16(b,1024+88)==INODE_SIZE;}
int ext2_format(void){base=disk_var_start();blocks=disk_var_sectors()/8;if(blocks<=FIRST_DATA+1||blocks>32768)return 0;uint8_t b[BS];zero(b,BS);w32(b,1024,INODES);w32(b,1028,blocks);w32(b,1036,blocks-(FIRST_DATA+1));w32(b,1040,INODES-10);w32(b,1044,0);w32(b,1048,2);w32(b,1052,2);w32(b,1056,32768);w32(b,1060,32768);w32(b,1064,INODES);w16(b,1080,EXT2_MAGIC);w16(b,1082,1);w16(b,1084,1);w32(b,1100,1);w32(b,1108,11);w16(b,1112,INODE_SIZE);w32(b,1120,2);if(!bwrite(0,b))return 0;zero(b,BS);w32(b,0,BLOCK_BITMAP);w32(b,4,INODE_BITMAP);w32(b,8,INODE_TABLE);w16(b,12,(uint16_t)(blocks-FIRST_DATA-1));w16(b,14,INODES-10);w16(b,16,1);if(!bwrite(1,b))return 0;zero(b,BS);for(unsigned i=0;i<=FIRST_DATA;i++)b[i>>3]|=(uint8_t)(1u<<(i&7));for(unsigned i=blocks;i<32768;i++)b[i>>3]|=(uint8_t)(1u<<(i&7));if(!bwrite(BLOCK_BITMAP,b))return 0;zero(b,BS);for(unsigned i=0;i<10;i++)b[i>>3]|=(uint8_t)(1u<<(i&7));for(unsigned i=INODES;i<32768;i++)b[i>>3]|=(uint8_t)(1u<<(i&7));if(!bwrite(INODE_BITMAP,b))return 0;zero(b,BS);for(unsigned i=0;i<INODE_TABLE_BLOCKS;i++)if(!bwrite(INODE_TABLE+i,b))return 0;inode_t root;zero(&root,sizeof root);root.mode=(uint16_t)(EXT2_DIR|0755);root.size=BS;root.links=2;root.blocks=8;root.block[0]=FIRST_DATA;if(!inode_put(ROOT_INO,&root))return 0;zero(b,BS);w32(b,0,ROOT_INO);w16(b,4,12);b[6]=1;b[7]=2;b[8]='.';w32(b,12,ROOT_INO);w16(b,16,BS-12);b[18]=2;b[19]=2;b[20]='.';b[21]='.';mounted=bwrite(FIRST_DATA,b)&&disk_sync();return mounted;}
int ext2_mount(void){mounted=ext2_probe();return mounted;}int ext2_mounted(void){return mounted;}
int ext2_store(const char*path,const unsigned char*data,size_t size,uint16_t mode,uint16_t uid,uint16_t gid){if(!mounted||!path||(!data&&size)||size>12*BS)return 0;uint32_t parent,ino;char name[256];unsigned nl;if(!walk(path,1,&parent,name,&nl,&ino))return 0;inode_t in;int existing=ino!=0;if(existing){if(!inode_get(ino,&in)||(in.mode&EXT2_DIR))return 0;for(unsigned i=0;i<12;i++)if(in.block[i]){free_bit(BLOCK_BITMAP,in.block[i]+1,0);in.block[i]=0;}}else{ino=alloc_inode();if(!ino)return 0;zero(&in,sizeof in);in.mode=(uint16_t)(EXT2_FILE|(mode&0777));in.uid=uid;in.gid=gid;in.links=1;if(!dir_add(parent,name,nl,ino,1))return 0;}in.size=(uint32_t)size;in.blocks=0;size_t at=0;for(unsigned i=0;at<size;i++){uint32_t blk=alloc_block();if(!blk)return 0;in.block[i]=blk;in.blocks+=8;uint8_t b[BS];zero(b,BS);for(unsigned j=0;j<BS&&at<size;j++)b[j]=data[at++];if(!bwrite(blk,b))return 0;}return inode_put(ino,&in)&&disk_sync();}
static int locate(const char*p,uint32_t*ino){uint32_t q;if(!walk(p,0,0,0,0,&q)||!q)return 0;*ino=q;return 1;}
int ext2_load(const char*p,unsigned char*d,size_t cap,size_t*n){uint32_t ino;inode_t in;if(!mounted||!d||!n||!locate(p,&ino)||!inode_get(ino,&in)||(in.mode&EXT2_DIR)||in.size>cap)return 0;size_t at=0;for(unsigned i=0;i<12&&at<in.size;i++){uint8_t b[BS];if(!in.block[i]||!bread(in.block[i],b))return 0;for(unsigned j=0;j<BS&&at<in.size;j++)d[at++]=b[j];}*n=in.size;return 1;}
int ext2_stat(const char*p,size_t*n,uint16_t*m,uint16_t*u,uint16_t*g){uint32_t ino;inode_t in;if(!mounted||!locate(p,&ino)||!inode_get(ino,&in))return 0;if(n)*n=in.size;if(m)*m=(uint16_t)(in.mode&0777);if(u)*u=in.uid;if(g)*g=in.gid;return 1;}
int ext2_chmod(const char*p,uint16_t m){uint32_t ino;inode_t in;if(!locate(p,&ino)||!inode_get(ino,&in))return 0;in.mode=(uint16_t)((in.mode&0170000)|(m&0777));return inode_put(ino,&in)&&disk_sync();}
int ext2_chown(const char*p,uint16_t u,uint16_t g,int su,int sg){uint32_t ino;inode_t in;if(!locate(p,&ino)||!inode_get(ino,&in))return 0;if(su)in.uid=u;if(sg)in.gid=g;return inode_put(ino,&in)&&disk_sync();}
int ext2_remove(const char*p){uint32_t parent,ino;char name[256];unsigned nl;inode_t in;if(!walk(p,0,&parent,name,&nl,&ino)||!ino||!inode_get(ino,&in)||(in.mode&EXT2_DIR))return 0;inode_t di;uint8_t b[BS];if(!inode_get(parent,&di)||!bread(di.block[0],b))return 0;for(unsigned pos=0,prev=0;pos<BS;){uint16_t rec=r16(b,pos+4);if(r32(b,pos)==ino){if(pos)w16(b,prev+4,(uint16_t)(r16(b,prev+4)+rec));else w32(b,pos,0);if(!bwrite(di.block[0],b))return 0;break;}prev=pos;pos+=rec;}for(unsigned i=0;i<12;i++)if(in.block[i])free_bit(BLOCK_BITMAP,in.block[i]+1,0);zero(&in,sizeof in);return inode_put(ino,&in)&&free_bit(INODE_BITMAP,ino,1)&&disk_sync();}
static int enumerate(uint32_t dir,char*prefix,unsigned plen,unsigned*wanted,char*out,size_t cap,size_t*sz){inode_t di;uint8_t b[BS];if(!inode_get(dir,&di)||!bread(di.block[0],b))return 0;for(unsigned p=0;p<BS;){uint32_t ino=r32(b,p);uint16_t rec=r16(b,p+4);uint8_t nl=b[p+6],type=b[p+7];if(rec<8)return 0;if(ino&&nl&&!(nl==1&&b[p+8]=='.')&&!(nl==2&&b[p+8]=='.'&&b[p+9]=='.')){char path[96];unsigned n=0;while(n<plen&&n<95){path[n]=prefix[n];n++;}if(n<95)path[n++]='/';for(unsigned i=0;i<nl&&n<95;i++)path[n++]=(char)b[p+8+i];path[n]=0;if(type==2){if(enumerate(ino,path,n,wanted,out,cap,sz))return 1;}else if((*wanted)--==0){unsigned i=0;while(path[i]&&i+1<cap){out[i]=path[i];i++;}out[i]=0;inode_t in;if(sz&&inode_get(ino,&in))*sz=in.size;return 1;}}p+=rec;}return 0;}
int ext2_at(unsigned index,char*p,size_t cap,size_t*n){if(!mounted||!p||!cap)return 0;char root[2]={0};return enumerate(ROOT_INO,root,0,&index,p,cap,n);}
int ext2_sync(void){return disk_sync();}void ext2_unmount(void){ext2_sync();mounted=0;}
int ext2_check(unsigned*f,unsigned*b){if(!ext2_probe())return 0;uint8_t bm[BS];if(!bread(BLOCK_BITMAP,bm))return 0;unsigned used=0;for(unsigned i=0;i<blocks;i++)if(bm[i>>3]&(1u<<(i&7)))used++;if(f){uint8_t im[BS];if(!bread(INODE_BITMAP,im))return 0;unsigned count=0;for(unsigned i=10;i<INODES;i++)if(im[i>>3]&(1u<<(i&7)))count++;*f=count;}if(b)*b=used;return 1;}

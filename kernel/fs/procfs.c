#include "procfs.h"
#include "memory.h"
#include "timed.h"
#include "disk.h"
#include "varfs.h"
static const char*names[]={"version","uptime","memory","mounts"};
static int eq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return !*a&&!*b;}
static void add(char*b,size_t c,size_t*n,const char*s){while(*s&&*n+1<c)b[(*n)++]=*s++;}
static void num(char*b,size_t c,size_t*n,unsigned long v){char q[24];unsigned z=0;if(!v)q[z++]='0';while(v){q[z++]=(char)('0'+v%10);v/=10;}while(z&&*n+1<c)b[(*n)++]=q[--z];}
int procfs_read(const char*p,unsigned char*d,size_t c,size_t*out){char*b=(char*)d;size_t n=0;if(eq(p,"/proc/version")){add(b,c,&n,"OS64 ");add(b,c,&n,OS64_KERNEL_VERSION);add(b,c,&n," x86_64\n");}else if(eq(p,"/proc/uptime")){num(b,c,&n,timed_uptime());add(b,c,&n,"\n");}else if(eq(p,"/proc/memory")){add(b,c,&n,"MemUsable: ");num(b,c,&n,memory_total_kib());add(b,c,&n," kB\nHeapTotal: ");num(b,c,&n,memory_heap_bytes()/1024);add(b,c,&n," kB\nHeapFree:  ");num(b,c,&n,memory_free_bytes()/1024);add(b,c,&n," kB\n");}else if(eq(p,"/proc/mounts")){add(b,c,&n,"initramfs / initramfs ro\ndevfs /dev devfs rw\ntmpfs /tmp tmpfs rw\nprocfs /proc procfs ro\n");if(disk_mounted())add(b,c,&n,"/dev/sda1 /home fat32 rw\n");if(varfs_mounted()){add(b,c,&n,"/dev/sda2 /var ");add(b,c,&n,varfs_type());add(b,c,&n," rw\n");}}else return 0;*out=n;return 1;}
int procfs_stat(const char*p,size_t*n){unsigned char b[512];return procfs_read(p,b,sizeof b,n);}
int procfs_at(unsigned i,char*p,size_t c){if(i>=4||!c)return 0;size_t n=0;while(names[i][n]&&n+1<c){p[n]=names[i][n];n++;}p[n]=0;return 1;}

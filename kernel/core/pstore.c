#include "pstore.h"
#include "disk.h"
#include <stdint.h>

#define PSTORE_LBA 2039u
#define PSTORE_MAGIC 0x52545350u
static unsigned char record[512];
static int valid;
static uint32_t get32(unsigned o){return (uint32_t)record[o]|((uint32_t)record[o+1]<<8)|((uint32_t)record[o+2]<<16)|((uint32_t)record[o+3]<<24);}
static void put32(unsigned o,uint32_t v){record[o]=(uint8_t)v;record[o+1]=(uint8_t)(v>>8);record[o+2]=(uint8_t)(v>>16);record[o+3]=(uint8_t)(v>>24);}
static uint32_t checksum(const unsigned char*p,size_t n){uint32_t h=2166136261u;while(n--){h^=*p++;h*=16777619u;}return h;}
void pstore_init(void){valid=0;if(!disk_read(PSTORE_LBA,record)||get32(0)!=PSTORE_MAGIC)return;uint32_t n=get32(4);if(n>496||get32(8)!=checksum(record+16,n))return;valid=1;}
int pstore_write_panic(const char*reason,const char*log,size_t log_size){for(unsigned i=0;i<512;i++)record[i]=0;size_t n=0;const char*prefix="OS64 panic: ";while(*prefix&&n<496)record[16+n++]=(unsigned char)*prefix++;while(reason&&*reason&&n<496)record[16+n++]=(unsigned char)*reason++;if(n<496)record[16+n++]='\n';for(size_t i=0;i<log_size&&n<496;i++)record[16+n++]=(unsigned char)log[i];put32(0,PSTORE_MAGIC);put32(4,(uint32_t)n);put32(8,checksum(record+16,n));if(!disk_write(PSTORE_LBA,record)||!disk_sync())return 0;valid=1;if(disk_mounted()){static const unsigned char flag[]="panic\n";(void)disk_store("PANIC.FLG",flag,sizeof flag-1);(void)disk_sync();}return 1;}
int pstore_read(char*out,size_t cap,size_t*size){if(!valid||!out||!cap||!size)return 0;size_t n=get32(4);if(n>cap)n=cap;for(size_t i=0;i<n;i++)out[i]=(char)record[16+i];*size=n;return 1;}
int pstore_clear(void){for(unsigned i=0;i<512;i++)record[i]=0;if(!disk_write(PSTORE_LBA,record)||!disk_sync())return 0;if(disk_mounted())(void)disk_remove("PANIC.FLG");valid=0;return 1;}
int pstore_present(void){return valid;}

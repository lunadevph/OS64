#include "bootopts.h"
#include <stddef.h>
static const char*line="";
static const void *initrd_start,*initrd_end;
static int token(const char*s){for(size_t i=0;line[i];){while(line[i]==' ')i++;size_t j=0;while(s[j]&&line[i+j]==s[j])j++;if(!s[j]&&(line[i+j]==0||line[i+j]==' '))return 1;while(line[i]&&line[i]!=' ')i++;}return 0;}
void bootopts_init(uint64_t address){unsigned char*p=(unsigned char*)(uintptr_t)address;line="";initrd_start=initrd_end=0;if(!p)return;uint32_t total=*(uint32_t*)p;for(uint32_t off=8;off+8<=total;){uint32_t type=*(uint32_t*)(p+off),size=*(uint32_t*)(p+off+4);if(type==1&&size>8)line=(const char*)(p+off+8);else if(type==3&&size>=16&&!initrd_start){initrd_start=(const void*)(uintptr_t)*(uint32_t*)(p+off+8);initrd_end=(const void*)(uintptr_t)*(uint32_t*)(p+off+12);}if(size<8)return;off+=(size+7)&~7u;}}
const char*bootopts_command_line(void){return line;}
const char*bootopts_mode(void){if(token("bootmode=recovery")||token("mode=recovery")||token("single"))return "Recovery";if(token("bootmode=debug")||token("mode=debug"))return "Debug";if(token("bootmode=cli")||token("mode=cli"))return "CLI";return "Normal";}
int bootopts_debug(void){return token("bootmode=debug")||token("mode=debug")||token("loglevel=debug");}
int bootopts_recovery(void){return token("bootmode=recovery")||token("mode=recovery")||token("single");}
int bootopts_cli(void){return token("bootmode=cli")||token("mode=cli");}
int bootopts_serial(void){return token("console=serial");}
const void*bootopts_initrd_start(void){return initrd_start;}const void*bootopts_initrd_end(void){return initrd_end;}

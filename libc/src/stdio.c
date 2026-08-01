#include <stdio.h>
#include <os64/libc.h>
extern const os64_api_t*os64_libc_api(void);int putchar(int c){const os64_api_t*a=os64_libc_api();if(!a||!a->putc)return -1;a->putc((char)c);return (unsigned char)c;}int puts(const char*s){while(*s)if(putchar((unsigned char)*s++)<0)return -1;return putchar('\n');}

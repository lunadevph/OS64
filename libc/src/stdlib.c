#include <stdlib.h>
#include <os64/libc.h>
static const os64_api_t*api;void os64_libc_set_api(const os64_api_t*);
void os64_libc_init(const os64_api_t*a){api=a;os64_libc_set_api(a);}void*malloc(size_t n){return api&&api->allocate?api->allocate(n):0;}void free(void*p){(void)p;}long strtol(const char*s,char**end,int base){long v=0;int neg=0;while(*s==' '||*s=='\t')s++;if(*s=='-'||*s=='+')neg=*s++=='-';if(base!=10){if(end)*end=(char*)s;return 0;}while(*s>='0'&&*s<='9'){v=v*10+(*s-'0');s++;}if(end)*end=(char*)s;return neg?-v:v;}int atoi(const char*s){return (int)strtol(s,0,10);}

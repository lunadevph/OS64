#ifndef OS64_SDK_H
#define OS64_SDK_H
#include "app_abi.h"
#include <os64/security.h>
static inline void os64_write(const os64_api_t*a,const char*s){if(a&&a->write)a->write(s);}
static inline void os64_putc(const os64_api_t*a,char c){if(a&&a->putc)a->putc(c);}
static inline int os64_read_file(const os64_api_t*a,const char*p,unsigned char*d,os64_size_t c,os64_size_t*n){return a&&a->read_file?a->read_file(p,d,c,n):0;}
static inline int os64_write_file(const os64_api_t*a,const char*p,const unsigned char*d,os64_size_t n){return a&&a->write_file?a->write_file(p,d,n):0;}
static inline void*os64_alloc(const os64_api_t*a,os64_size_t n){return a&&a->allocate?a->allocate(n):(void*)0;}
static inline int os64_clock_get(const os64_api_t*a,os64_datetime_t*t){return a&&a->clock_get?a->clock_get(t):0;}
static inline const char*os64_user(const os64_api_t*a){return a&&a->current_user?a->current_user():"";}
static inline const char*os64_cwd(const os64_api_t*a){return a&&a->current_directory?a->current_directory():"";}
static inline int os64_streq(const char*a,const char*b){while(*a&&*a==*b){a++;b++;}return !*a&&!*b;}
static inline os64_size_t os64_strlen(const char*s){os64_size_t n=0;while(s[n])n++;return n;}
#endif

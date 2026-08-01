#include "bearssl.h"
#include <stddef.h>
void*memcpy(void*d,const void*s,size_t n){unsigned char*x=d;const unsigned char*y=s;for(size_t i=0;i<n;i++)x[i]=y[i];return d;}
void*memmove(void*d,const void*s,size_t n){unsigned char*x=d;const unsigned char*y=s;if(x<y)for(size_t i=0;i<n;i++)x[i]=y[i];else for(size_t i=n;i;i--)x[i-1]=y[i-1];return d;}
void*memset(void*d,int c,size_t n){unsigned char*x=d;for(size_t i=0;i<n;i++)x[i]=(unsigned char)c;return d;}
int memcmp(const void*a,const void*b,size_t n){const unsigned char*x=a,*y=b;for(size_t i=0;i<n;i++)if(x[i]!=y[i])return x[i]<y[i]?-1:1;return 0;}
size_t strlen(const char*s){size_t n=0;while(s[n])n++;return n;}
br_prng_seeder br_prng_seeder_system(const char**name){if(name)*name="OS64 explicit entropy";return 0;}

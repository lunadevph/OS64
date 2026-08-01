#include "os64.h"
#include <os64/libc.h>
#include <stdio.h>
#include <stdlib.h>
int _start(const os64_api_t*api,const char*args){
 if(!api||api->version!=OS64_ABI_VERSION)return 1;
 os64_libc_init(api);
 puts("Hello from the OS64 minimal libc.");
 os64_write(api,"User: ");
 os64_write(api,os64_user(api));
 os64_write(api,"  Directory: ");
 os64_write(api,os64_cwd(api));
 os64_write(api,"\nArguments: ");
 os64_write(api,args);
 os64_putc(api,'\n');
 void*p=malloc(32);if(!p)return 2;
 return 0;
}

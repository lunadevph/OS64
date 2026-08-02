#include "app_abi.h"
int _start(const os64_api_t *api,const char *args){
 if(!api||api->version!=OS64_ABI_VERSION)return 126;
 api->write("Hello from an installed OS64 package.\nArguments: ");
 api->write(args&&*args?args:"(none)");api->putc('\n');return 0;
}

#include "app_abi.h"
#ifndef APP_NAME
#error APP_NAME is required
#endif
int _start(const os64_api_t *api,const char *args){if(!api||api->version!=OS64_ABI_VERSION)return 1;return api->dispatch(APP_NAME,args);}

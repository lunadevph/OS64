#include <os64/libc.h>
static const os64_api_t*active;const os64_api_t*os64_libc_api(void){return active;}void os64_libc_set_api(const os64_api_t*a){active=a;}

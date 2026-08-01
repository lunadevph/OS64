#ifndef OS64_LIBC_H
#define OS64_LIBC_H
#include "app_abi.h"
void os64_libc_init(const os64_api_t*);
const os64_api_t *os64_libc_api(void);
#endif

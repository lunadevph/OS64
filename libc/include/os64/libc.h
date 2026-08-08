#ifndef OS64_LIBC_H
#define OS64_LIBC_H
#include "app_abi.h"
#ifdef __cplusplus
extern "C" {
#endif
void os64_libc_init(const os64_api_t*);
const os64_api_t *os64_libc_api(void);
#ifdef __cplusplus
}
#endif
#endif

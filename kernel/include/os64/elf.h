#ifndef OS64_ELF_H
#define OS64_ELF_H
#include <stddef.h>
#include "app_abi.h"
int elf_execute(const unsigned char *image, size_t size, const char *args, const os64_api_t *api);
#endif

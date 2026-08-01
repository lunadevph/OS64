#ifndef OS64_SDK_MEMORY_H
#define OS64_SDK_MEMORY_H
#include <stddef.h>
void *os_allocate(size_t size);void os_release(void *pointer);unsigned long os_memory_free(void);
#endif

#ifndef OS64_RANDOM_H
#define OS64_RANDOM_H

#include <stddef.h>

void random_init(void);
int random_ready(void);
int random_read(void *buffer, size_t size);
int random_mix(const void *buffer, size_t size);

#endif

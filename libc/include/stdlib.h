#ifndef OS64_LIBC_STDLIB_H
#define OS64_LIBC_STDLIB_H
#include <stddef.h>
int atoi(const char*);long strtol(const char*,char**,int);void *malloc(size_t);void *calloc(size_t,size_t);void *realloc(void*,size_t);void free(void*);
#endif

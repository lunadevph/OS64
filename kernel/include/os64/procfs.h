#ifndef OS64_PROCFS_H
#define OS64_PROCFS_H
#include <stddef.h>
int procfs_read(const char*,unsigned char*,size_t,size_t*);int procfs_stat(const char*,size_t*);int procfs_at(unsigned,char*,size_t);
#endif

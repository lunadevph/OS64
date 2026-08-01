#ifndef OS64_TMPFS_H
#define OS64_TMPFS_H
#include <stddef.h>
#include <stdint.h>
void tmpfs_init(void);int tmpfs_store(const char*,const unsigned char*,size_t,uint16_t,uint16_t,uint16_t);int tmpfs_load(const char*,unsigned char*,size_t,size_t*);int tmpfs_stat(const char*,size_t*,uint16_t*,uint16_t*,uint16_t*);int tmpfs_remove(const char*);int tmpfs_at(unsigned,char*,size_t,size_t*);int tmpfs_chmod(const char*,uint16_t);int tmpfs_chown(const char*,uint16_t,uint16_t,int,int);
#endif

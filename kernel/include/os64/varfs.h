#ifndef OS64_VARFS_H
#define OS64_VARFS_H
#include <stddef.h>
#include <stdint.h>
int varfs_probe(void); int varfs_format(void); int varfs_mount(void); int varfs_mounted(void);
int varfs_format_level(unsigned level);const char *varfs_type(void);
int varfs_store(const char*,const unsigned char*,size_t); int varfs_load(const char*,unsigned char*,size_t,size_t*);
int varfs_stat(const char*,size_t*,uint16_t*,uint16_t*,uint16_t*);
int varfs_chmod(const char*,uint16_t);int varfs_chown(const char*,uint16_t,uint16_t,int,int);
int varfs_remove(const char*);
int varfs_at(unsigned index,char *path,size_t capacity,size_t *size);
int varfs_sync(void); void varfs_unmount(void);
int varfs_check(unsigned *files,unsigned *blocks);
#endif

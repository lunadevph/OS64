#ifndef OS64_EXT2_H
#define OS64_EXT2_H
#include <stddef.h>
#include <stdint.h>
int ext2_probe(void);
int ext2_format(void);
int ext2_mount(void);
int ext2_mounted(void);
int ext2_store(const char *path,const unsigned char *data,size_t size,uint16_t mode,uint16_t uid,uint16_t gid);
int ext2_load(const char *path,unsigned char *data,size_t capacity,size_t *size);
int ext2_stat(const char *path,size_t *size,uint16_t *mode,uint16_t *uid,uint16_t *gid);
int ext2_chmod(const char *path,uint16_t mode);
int ext2_chown(const char *path,uint16_t uid,uint16_t gid,int set_uid,int set_gid);
int ext2_remove(const char *path);
int ext2_at(unsigned index,char *path,size_t capacity,size_t *size);
int ext2_sync(void);
void ext2_unmount(void);
int ext2_check(unsigned *files,unsigned *blocks);
#endif

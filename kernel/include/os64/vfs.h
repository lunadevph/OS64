#ifndef OS64_VFS_H
#define OS64_VFS_H
#include <stddef.h>
#include <stdint.h>
typedef enum{VFS_INITRAMFS,VFS_FAT32,VFS_RAMFS,VFS_EXT2,VFS_TMPFS,VFS_PROCFS,VFS_DEVFS}vfs_backend_t;
typedef struct{size_t size;uint16_t mode;uint16_t uid,gid;uint8_t type;vfs_backend_t backend;}vfs_stat_t;
typedef struct{char name[96];uint8_t type;vfs_backend_t backend;}vfs_dirent_t;
#define VFS_TYPE_FILE 0
#define VFS_TYPE_DIRECTORY 1
#define VFS_TYPE_CHAR_DEVICE 2
#define VFS_TYPE_BLOCK_DEVICE 3
#define VFS_WRITE_ERROR 0
#define VFS_WRITE_OK 1
#define VFS_WRITE_NO_SPACE -1
#define VFS_ACCESS_EXECUTE 1
#define VFS_ACCESS_WRITE 2
#define VFS_ACCESS_READ 4
void vfs_init(void);
int vfs_read(const char *path,unsigned char *data,size_t capacity,size_t *size,vfs_stat_t *stat);
int vfs_stat_path(const char *path,vfs_stat_t *stat);
int vfs_check_permission(const char *path,unsigned access);
int vfs_chmod(const char *path,uint16_t mode);
int vfs_chown(const char *path,uint16_t uid,uint16_t gid,int set_uid,int set_gid);
int vfs_write(const char *path,const unsigned char *data,size_t size,size_t *written);
int vfs_remove(const char *path);
int vfs_readdir(const char *path,unsigned index,vfs_dirent_t *entry);
const char *vfs_mount_source(const char *path);
#endif

#ifndef OS64_SDK_FS_H
#define OS64_SDK_FS_H
#include "app_abi.h"
typedef os64_dirent_t os_dirent_t;
int os_file_read(const char *path,unsigned char *data,os64_size_t capacity,os64_size_t *size);int os_file_write(const char *path,const unsigned char *data,os64_size_t size);int os_directory_read(const char *path,unsigned index,os_dirent_t *entry);
#endif

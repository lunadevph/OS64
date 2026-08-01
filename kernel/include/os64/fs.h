#ifndef OS64_FS_H
#define OS64_FS_H
#include <stddef.h>
typedef struct { const char *name; const unsigned char *data; size_t size; char type; } fs_file_t;
void fs_mount(const void *start, const void *end);
size_t fs_count(void);
int fs_at(size_t index, fs_file_t *file);
int fs_find(const char *path, fs_file_t *file);
#endif

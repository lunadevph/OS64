#ifndef OS64_DISK_H
#define OS64_DISK_H
#include <stdint.h>
#include <stddef.h>
int disk_init(void);
int disk_read(uint32_t lba, uint8_t *buffer);
int disk_write(uint32_t lba, const uint8_t *buffer);
uint32_t disk_var_start(void);
uint32_t disk_var_sectors(void);
const char *disk_label(void);
int disk_format(void);
int disk_formatted(void);
int disk_mount(void);
int disk_mounted(void);
int disk_sync(void);
void disk_unmount(void);
typedef struct { char name[96]; uint32_t size; uint8_t type; } disk_file_t;
int disk_file_at(unsigned index,disk_file_t *file);
int disk_create(const char *name,uint8_t type);
int disk_store(const char *name,const unsigned char *data,uint32_t size);
int disk_store_large(const char *name,const unsigned char *data,size_t size);
int disk_install_boot_area(const unsigned char *data,size_t size);
int disk_load(const char *name,unsigned char *data,uint32_t *size);
int disk_remove(const char *name);
uint32_t disk_fill(uint32_t cluster_limit);
uint32_t disk_free_clusters(void);
int disk_check(uint32_t *files,uint32_t *clusters);
#endif

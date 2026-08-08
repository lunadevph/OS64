#ifndef OS64_PSTORE_H
#define OS64_PSTORE_H
#include <stddef.h>
void pstore_init(void);
int pstore_write_panic(const char *reason,const char *log,size_t log_size);
int pstore_read(char *output,size_t capacity,size_t *size);
int pstore_clear(void);
int pstore_present(void);
#endif

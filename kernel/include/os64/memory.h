#ifndef OS64_MEMORY_H
#define OS64_MEMORY_H
#include <stddef.h>
#include <stdint.h>
void memory_init(uint64_t multiboot_address);
uint64_t memory_total_kib(void);
size_t memory_used_bytes(void);
size_t memory_free_bytes(void);
size_t memory_heap_bytes(void);
size_t memory_allocation_count(void);
void *kmalloc(size_t size);
void kfree(void *pointer);
#endif

#include "memory.h"

#define HEAP_MAX_BYTES (2u * 1024u * 1024u)
#define BLOCK_MAGIC 0x4f5336344d454d31ull
#define ALIGNMENT 16u
#define MIN_PAYLOAD 16u

typedef struct __attribute__((aligned(16))) block {
    uint64_t magic;
    size_t size;
    struct block *previous;
    struct block *next;
    int free;
} block_t;

typedef struct {
    uint32_t type;
    uint32_t size;
} mb_tag_t;

typedef struct {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
} mb_mmap_tag_t;

typedef struct {
    uint64_t address;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
} mb_mmap_entry_t;

static uint64_t total_kib;
static unsigned char heap[HEAP_MAX_BYTES] __attribute__((aligned(16)));
static size_t heap_bytes;
static size_t used;
static size_t allocations;
static block_t *first;

static size_t align_up(size_t value)
{
    if (value > (size_t)-1 - (ALIGNMENT - 1u))
        return 0;
    return (value + ALIGNMENT - 1u) & ~(size_t)(ALIGNMENT - 1u);
}

static uint64_t multiboot_available_bytes(const unsigned char *base)
{
    uint32_t total_size;
    uint64_t available = 0;

    if (!base)
        return 0;
    total_size = *(const uint32_t *)base;
    if (total_size < 16u)
        return 0;

    for (uint32_t offset = 8; offset + sizeof(mb_tag_t) <= total_size;) {
        const mb_tag_t *tag = (const mb_tag_t *)(base + offset);
        if (tag->type == 0)
            break;
        if (tag->size < sizeof(*tag) || tag->size > total_size - offset)
            break;
        if (tag->type == 6 && tag->size >= sizeof(mb_mmap_tag_t)) {
            const mb_mmap_tag_t *map = (const mb_mmap_tag_t *)tag;
            uint32_t position = sizeof(*map);
            if (map->entry_size < sizeof(mb_mmap_entry_t))
                break;
            while (position <= tag->size - map->entry_size) {
                const mb_mmap_entry_t *entry =
                    (const mb_mmap_entry_t *)((const unsigned char *)tag + position);
                if (entry->type == 1 && UINT64_MAX - available >= entry->length)
                    available += entry->length;
                position += map->entry_size;
            }
        }
        offset += (tag->size + 7u) & ~7u;
    }
    return available;
}

static uint64_t multiboot_basic_bytes(const unsigned char *base)
{
    uint32_t total_size;

    if (!base)
        return 0;
    total_size = *(const uint32_t *)base;
    for (uint32_t offset = 8; offset + sizeof(mb_tag_t) <= total_size;) {
        const mb_tag_t *tag = (const mb_tag_t *)(base + offset);
        if (tag->type == 0)
            break;
        if (tag->size < sizeof(*tag) || tag->size > total_size - offset)
            break;
        if (tag->type == 4 && tag->size >= 16u) {
            uint32_t upper_kib = *(const uint32_t *)(base + offset + 12u);
            return ((uint64_t)upper_kib + 1024u) * 1024u;
        }
        offset += (tag->size + 7u) & ~7u;
    }
    return 0;
}

void memory_init(uint64_t address)
{
    const unsigned char *info = (const unsigned char *)(uintptr_t)address;
    uint64_t available = multiboot_available_bytes(info);

    if (!available)
        available = multiboot_basic_bytes(info);
    total_kib = available / 1024u;

    /*
     * Paging currently identity-maps physical memory, but the initramfs and
     * boot metadata may occupy otherwise "available" pages. Keep the heap in
     * kernel-owned BSS and use the boot map to size it conservatively.
     */
    heap_bytes = HEAP_MAX_BYTES;
    if (available && available / 8u < heap_bytes)
        heap_bytes = (size_t)(available / 8u);
    heap_bytes &= ~(size_t)(ALIGNMENT - 1u);
    used = 0;
    allocations = 0;
    first = 0;
    if (heap_bytes <= sizeof(block_t) + MIN_PAYLOAD)
        return;

    first = (block_t *)heap;
    first->magic = BLOCK_MAGIC;
    first->size = heap_bytes - sizeof(block_t);
    first->previous = 0;
    first->next = 0;
    first->free = 1;
}

uint64_t memory_total_kib(void)
{
    return total_kib;
}

size_t memory_used_bytes(void)
{
    return used;
}

size_t memory_free_bytes(void)
{
    size_t free_bytes = 0;
    for (block_t *block = first; block; block = block->next) {
        if (block->magic != BLOCK_MAGIC)
            return 0;
        if (block->free)
            free_bytes += block->size;
    }
    return free_bytes;
}

size_t memory_heap_bytes(void)
{
    return heap_bytes > sizeof(block_t) ? heap_bytes - sizeof(block_t) : 0;
}

size_t memory_allocation_count(void)
{
    return allocations;
}

void *kmalloc(size_t size)
{
    block_t *block;
    size_t wanted;

    if (!size || !first)
        return 0;
    wanted = align_up(size);
    if (!wanted)
        return 0;

    for (block = first; block; block = block->next) {
        if (block->magic != BLOCK_MAGIC)
            return 0;
        if (!block->free || block->size < wanted)
            continue;
        if (block->size >= wanted + sizeof(block_t) + MIN_PAYLOAD) {
            block_t *remainder =
                (block_t *)((unsigned char *)(block + 1) + wanted);
            remainder->magic = BLOCK_MAGIC;
            remainder->size = block->size - wanted - sizeof(block_t);
            remainder->previous = block;
            remainder->next = block->next;
            remainder->free = 1;
            if (remainder->next)
                remainder->next->previous = remainder;
            block->next = remainder;
            block->size = wanted;
        }
        block->free = 0;
        used += block->size;
        allocations++;
        return block + 1;
    }
    return 0;
}

void kfree(void *pointer)
{
    block_t *block = first;

    if (!pointer)
        return;
    if ((unsigned char *)pointer < heap + sizeof(block_t) ||
        (unsigned char *)pointer >= heap + heap_bytes)
        return;
    while (block && (void *)(block + 1) != pointer) {
        if (block->magic != BLOCK_MAGIC)
            return;
        block = block->next;
    }
    if (!block || block->magic != BLOCK_MAGIC || block->free)
        return;

    block->free = 1;
    used -= block->size;
    if (allocations)
        allocations--;
    if (block->next && block->next->magic == BLOCK_MAGIC && block->next->free) {
        block_t *next = block->next;
        block->size += sizeof(block_t) + next->size;
        block->next = next->next;
        if (block->next)
            block->next->previous = block;
    }
    if (block->previous && block->previous->magic == BLOCK_MAGIC &&
        block->previous->free) {
        block_t *previous = block->previous;
        previous->size += sizeof(block_t) + block->size;
        previous->next = block->next;
        if (previous->next)
            previous->next->previous = previous;
    }
}

/**
 * @link https://www.gingerbill.org/article/2019/02/08/memory-allocation-strategies-002/
 */
#ifndef MEM_ARENA_H
#define MEM_ARENA_H

#include <string.h> // memset, memmove

#include "allocator.h"

typedef struct Arena Arena;
struct Arena {
    unsigned char *buf;

    // Total of how many bytes can be held in `buf.`
    size_t buf_len;

    // Useful to help free the last alocations.
    size_t prev_offset;

    // Track the current 'top' byte pointer.
    size_t curr_offset;
};

void
arena_init(Arena *a, void *backing_buffer, size_t backing_buffer_length);

void *
arena_alloc(Arena *a, size_t size);

void *
arena_alloc_align(Arena *a, size_t size, size_t align);

void *
arena_resize(Arena *a, void *old_memory, size_t old_size, size_t new_size);

void *
arena_resize_align(Arena *a,
    void                 *old_memory,
    size_t                old_size,
    size_t                new_size,
    size_t                align);

void
arena_free_all(Arena *a);

Allocator
arena_allocator(Arena *a);

#endif // MEM_ARENA_H

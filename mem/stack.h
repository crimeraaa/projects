// https://www.gingerbill.org/article/2019/02/15/memory-allocation-strategies-003/
#ifndef MEM_STACK_H
#define MEM_STACK_H

#include "allocator.h"

typedef struct Stack Stack;
struct Stack {
    unsigned char *buf;

    // The total number of indexable bytes in `buf`.
    size_t buf_len;

    // The index of the previous header of the most recently allocated block.
    size_t prev_offset;

    // The index of the first available byte in `buf`.
    size_t curr_offset;
};

typedef struct Stack_Allocation_Header Stack_Allocation_Header;
struct Stack_Allocation_Header {
    // Index of the previous allocation.
    size_t prev_offset;

    // `maximum_alignment_in_bytes = 2**(8 * sizeof(padding) - 1)`
    size_t padding;
};

void
stack_init(Stack *s, void *backing_buffer, size_t backing_buffer_length);

void *
stack_alloc(Stack *s, size_t size);

void *
stack_resize(Stack *s, void *old_ptr, size_t old_size, size_t new_size);

void
stack_free(Stack *s, void *ptr);

void
stack_free_all(Stack *s);

void *
stack_alloc_align(Stack *s, size_t size, size_t align);

void *
stack_resize_align(Stack *s,
    void                 *old_ptr,
    size_t                old_size,
    size_t                new_size,
    size_t                align);

Allocator
stack_allocator(Stack *s);

#endif /* MEM_STACK_H */

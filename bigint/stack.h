#ifndef BIGINT_STACK_H
#define BIGINT_STACK_H

#include <stdio.h>  // BUFSIZ

#include "common.h"

typedef struct {
    size_t prev_offset;
} Stack_Header;

typedef struct {
    char data[BUFSIZ];

    // Marks active bytes to the left of `data` (exclusive).
    // Marks available bytes to the right of `data` (inclusive).
    size_t offset;
} Stack;

void *
stack_resize(Stack *s, void *ptr, size_t old_size, size_t new_size);

void *
stack_shrink(Stack *s, void *ptr, size_t old_size, size_t new_size);

void
stack_free_all(Stack *s);

void *
stack_allocator_fn(void *ptr, size_t old_size, size_t new_size, void *context);

#endif /* BIGINT_STACK_H */

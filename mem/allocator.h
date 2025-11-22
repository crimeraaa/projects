#ifndef MEM_ALLOCATOR_H
#define MEM_ALLOCATOR_H

#include "../common.h"

#ifndef MEM_DEFAULT_ALIGNMENT
#define MEM_DEFAULT_ALIGNMENT alignof(max_align_t)
#endif // MEM_DEFAULT_ALIGNMENT

typedef enum {
    ALLOCATOR_ALLOC,
    ALLOCATOR_RESIZE,
    ALLOCATOR_FREE,
    ALLOCATOR_FREE_ALL,
} Allocator_Mode;

typedef void *
(*Allocator_Fn)(void *context,
    Allocator_Mode    mode,
    void             *old_memory,
    size_t            old_size,
    size_t            new_size,
    size_t            align);

typedef struct {
    Allocator_Fn fn;
    void        *context;
} Allocator;

#define array_make(T, count, allocator)                                        \
    (T *)mem_alloc_align(/*size=*/sizeof(T) * (count),                         \
        /*align=*/                alignof(T),                                  \
        /*allocator=*/            allocator)

#define array_resize(T, ptr, old_len, new_len, allocator)                      \
    (T *)mem_resize_align(/*old_memory=*/ptr,                                  \
        /*old_size=*/                    sizeof(T) * (old_len),                \
        /*new_size=*/                    sizeof(T) * (new_len),                \
        /*align=*/                       alignof(T),                           \
        /*allocator=*/                   allocator)

#define array_delete(ptr, len, allocator)                                      \
    mem_free(/*memory=*/ptr,                                                   \
        /*size=*/       sizeof(*(ptr)) * (len),                                \
        /*allocator*/   allocator)


/** @brief Allocate `size` bytes using the default alignment. */
void *
mem_alloc(size_t size, Allocator allocator);


/** @brief Reallocate `old_memory` from `old_size` bytes to `new_size` bytes,
 * using the default alignment. */
void *
mem_resize(void *old_memory,
    size_t       old_size,
    size_t       new_size,
    Allocator    allocator);


/** @brief Allocate `size` bytes using the given alignment `align`. */
void *
mem_alloc_align(size_t size, size_t align, Allocator allocator);


/** @brief Reallocate `old_memory` from `old_size` bytes to `new_size` bytes,
 * using the given alignment `align`. */
void *
mem_resize_align(void *old_memory,
    size_t             old_size,
    size_t             new_size,
    size_t             align,
    Allocator          allocator);

void
mem_free(void *memory, size_t size, Allocator allocator);

void
mem_free_all(Allocator allocator);

// Useful for alignment.
bool
mem_is_power_of_two(uintptr_t x);

#endif /* MEM_ALLOCATOR_H */

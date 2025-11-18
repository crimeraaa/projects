#include "allocator.h"

void *
mem_alloc(size_t size, Allocator allocator)
{
    return allocator.fn(allocator.context,
        /*mode=      */ ALLOCATOR_ALLOC,
        /*old_memory=*/ NULL,
        /*old_size=  */ 0,
        /*new_size=  */ size,
        /*align=     */ MEM_DEFAULT_ALIGNMENT);
}

void *
mem_resize(void *old_memory,
    size_t old_size,
    size_t new_size,
    Allocator allocator)
{
    return allocator.fn(allocator.context,
        ALLOCATOR_RESIZE,
        old_memory,
        old_size,
        new_size,
        MEM_DEFAULT_ALIGNMENT);
}

void *
mem_alloc_align(size_t size, size_t align, Allocator allocator)
{
    return allocator.fn(allocator.context,
        /*mode=      */ ALLOCATOR_ALLOC,
        /*old_memory=*/ NULL,
        /*old_size=  */ 0,
        /*new_size=  */ size,
        /*align=     */ align);
}

void *
mem_resize_align(void *old_memory,
    size_t old_size,
    size_t new_size,
    size_t align,
    Allocator allocator)
{
    return allocator.fn(allocator.context,
        ALLOCATOR_RESIZE,
        old_memory,
        old_size,
        new_size,
        align);
}

void
mem_free(void *memory, size_t size, Allocator allocator)
{
    allocator.fn(allocator.context,
        /*mode=      */ ALLOCATOR_FREE,
        /*old_memory=*/ memory,
        /*old_size=  */ size,
        /*new_size=  */ 0,
        /*align=     */ 0);
}

void
mem_free_all(Allocator allocator)
{
    allocator.fn(allocator.context,
        /*mode=      */ ALLOCATOR_FREE_ALL,
        /*old_memory=*/ NULL,
        /*old_size=  */ 0,
        /*new_size=  */ 0,
        /*align=     */ 0);
}

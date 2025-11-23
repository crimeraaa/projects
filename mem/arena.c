#include "arena.h"

static uintptr_t
align_forward(uintptr_t ptr, size_t align)
{
    uintptr_t modulo;

    // Required for modulo via bit-and to work.
    assert(mem_is_power_of_two(align));

    // Same as `p % a` but faster when `a` is a power of 2.
    modulo = ptr & (align - 1);

    // Address is not aligned?
    if (modulo != 0) {
        // `p` is not yet aligned, push it to the next aligned address.
        ptr += align - modulo;
    }
    return ptr;
}

void
arena_init(Arena *a, void *backing_buffer, size_t backing_buffer_length)
{
    a->buf         = cast(unsigned char *)backing_buffer;
    a->buf_len     = backing_buffer_length;
    a->curr_offset = 0;
    a->prev_offset = 0;
}

void *
arena_alloc(Arena *a, size_t size)
{
    return arena_alloc_align(a, size, MEM_DEFAULT_ALIGNMENT);
}

void *
arena_alloc_align(Arena *a, size_t size, size_t align)
{
    // Get the first immediately available pointer.
    uintptr_t curr_ptr = cast(uintptr_t)a->buf + cast(uintptr_t)a->curr_offset;

    // Align the aforementioned address forward to the specified alignment.
    uintptr_t offset = align_forward(curr_ptr, align);

    // Convert said offset from an absolute offset into a relative one.
    offset -= cast(uintptr_t)a->buf;

    // Backing memory has space remaining?
    if (offset + size <= a->buf_len) {
        void *ptr      = &a->buf[offset];
        a->prev_offset = offset;
        a->curr_offset = offset + size;
        // Zero new memory by default
        return memset(ptr, 0, size);
    }
    // Out of memory!
    return NULL;
}

void *
arena_resize(Arena *a, void *old_memory, size_t old_size, size_t new_size)
{
    return arena_resize_align(a, old_memory, old_size, new_size,
        MEM_DEFAULT_ALIGNMENT);
}

void *
arena_resize_align(Arena *a,
    void                 *old_ptr,
    size_t                old_size,
    size_t                new_size,
    size_t                align)
{
    unsigned char *old_addr = cast(unsigned char *)old_ptr;
    assert(mem_is_power_of_two(align));

    // Requesting for a new block?
    if (old_addr == NULL || (old_size == 0 && new_size > 0)) {
        return arena_alloc_align(a, new_size, align);
    // Resizing an existing block?
    } else if (a->buf <= old_addr && old_addr < a->buf + a->buf_len) {
        // `old_mem` is exactly the last allocation?
        // This means it can be resized in-place.
        if (a->buf + a->prev_offset == old_addr) {
            // Growing the allocation?
            if (old_size < new_size) {
                size_t growth = new_size - old_size;
                memset(&a->buf[a->curr_offset], 0, growth);
                a->curr_offset += growth;
            }
            else {
                a->curr_offset -= old_size - new_size;
            }
            return old_ptr;
        }
        // `old_mem` is NOT exactly the last allocation.
        else {
            void *new_ptr = arena_alloc_align(a, new_size, align);
            size_t copy_size = (old_size < new_size) ? old_size : new_size;
            return memmove(new_ptr, old_ptr, copy_size);
        }
    } else {
        assert(0 && "Memory is out of bounds of the buffer in this arena");
        return NULL;
    }
}

void
arena_free_all(Arena *a)
{
    a->curr_offset = 0;
    a->prev_offset = 0;
}

static void *
arena_allocator_fn(void *context,
    Allocator_Mode       mode,
    void                *old_ptr,
    size_t               old_size,
    size_t               new_size,
    size_t               align)
{
    Arena *a = cast(Arena *)context;
    switch (mode) {
    case ALLOCATOR_ALLOC:
        return arena_alloc_align(a, new_size, align);
    case ALLOCATOR_RESIZE:
        return arena_resize_align(a, old_ptr, old_size, new_size, align);
    case ALLOCATOR_FREE:
        break;
    case ALLOCATOR_FREE_ALL:
        arena_free_all(a);
        break;
    }
    return NULL;
}

Allocator
arena_allocator(Arena *a)
{
    Allocator r = {arena_allocator_fn, a};
    return r;
}

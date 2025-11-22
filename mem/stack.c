#include <string.h> // memset

#include "stack.h"

void
stack_init(Stack *s, void *backing_buffer, size_t backing_buffer_length)
{
    s->buf         = cast(unsigned char *)backing_buffer;
    s->buf_len     = backing_buffer_length;
    s->curr_offset = 0;
    s->prev_offset = 0;
}

void *
stack_alloc(Stack *s, size_t size)
{
    return stack_alloc_align(s, size, MEM_DEFAULT_ALIGNMENT);
}

void *
stack_resize(Stack *s, void *old_ptr, size_t old_size, size_t new_size)
{
    size_t align = MEM_DEFAULT_ALIGNMENT;
    return stack_resize_align(s, old_ptr, old_size, new_size, align);
}

static size_t
calc_padding_with_header(uintptr_t ptr, uintptr_t align)
{
    uintptr_t modulo, padding, needed_space;
    assert(mem_is_power_of_two(align));

    // (p % a) assuming `a` is a power of 2.
    modulo       = ptr & (align - 1);
    padding      = 0;
    needed_space = 0;

    // Alignment results in some padding?
    if (modulo != 0) {
        padding = align - modulo;
    }

    needed_space = cast(uintptr_t)sizeof(Stack_Allocation_Header);

    // Would-be padding does yet not account for the header?
    // May occur when `align > sizeof(header)` and `ptr` is not aligned.
    if (padding < needed_space) {
        // User-facing allocation will not include the header size.
        needed_space -= padding;

        // User-facing allocation is not aligned properly?
        if ((needed_space & (align - 1)) != 0) {
            /** @todo(2025-11-22): Figure out what this means */
            padding += align * (1 + (needed_space / align));
        } else {
            // User-facing allocation is aligned properly.
            // padding += align * (needed_space / align);
            padding += needed_space;
        }
    }

    return cast(size_t)padding;
}

void *
stack_alloc_align(Stack *s, size_t size, size_t align)
{
    uintptr_t curr_addr, next_addr;
    size_t padding;
    Stack_Allocation_Header *header;

    assert(mem_is_power_of_two(align));

    // 1st available pointer in the stack.
    curr_addr = cast(uintptr_t)s->buf + cast(uintptr_t)s->curr_offset;
    padding   = calc_padding_with_header(curr_addr, cast(uintptr_t)align);

    // Resulting allocation would overflow the buffer?
    if (s->curr_offset + padding + size > s->buf_len) {
        return NULL;
    }

    next_addr = curr_addr + cast(uintptr_t)padding;
    header = cast(Stack_Allocation_Header *)(next_addr - sizeof(*header));
    // Ensure headers are chained properly by this point.
    header->prev_offset = s->prev_offset;
    header->padding = padding;

    // Save index of the this allocation's header as the top of the chain.
    s->prev_offset  = s->curr_offset;
    s->curr_offset += padding + size;
    return memset(cast(void *)next_addr, 0, size);
}

void *
stack_resize_align(Stack *s,
    void                 *old_ptr,
    size_t                old_size,
    size_t                new_size,
    size_t                align)
{
    if (old_ptr == NULL) {
        return stack_alloc_align(s, new_size, align);
    } else if (new_size == 0) {
        stack_free(s, old_ptr);
        return NULL;
    }

    uintptr_t start, end, curr_addr;
    Stack_Allocation_Header *header;
    size_t prev_offset;
    size_t min_size = (old_size < new_size) ? old_size : new_size;
    void *new_ptr;

    start     = cast(uintptr_t)s->buf;
    end       = start + cast(uintptr_t)s->buf_len;
    curr_addr = cast(uintptr_t)old_ptr;

    if (!(start <= curr_addr && curr_addr < end)) {
        assert(0 && "Out of bounds memory access passed to stack allocator (resize)");
        return NULL;
    }

    if (curr_addr >= start + cast(uintptr_t)s->curr_offset) {
        // Treat as a double free
        return NULL;
    }

    // Nothing to do?
    if (old_size == new_size) {
        return old_ptr;
    }

    header = cast(Stack_Allocation_Header *)(curr_addr - sizeof(*header));

    // Calculate previous offset from header and its address.
    prev_offset = cast(size_t)(curr_addr - cast(uintptr_t)header->padding - start);

    // Can resize in-place?
    if (/* prev_offset == header->prev_offset && */ prev_offset == s->prev_offset) {
        // Resized allocation would be out of bounds in the stack?
        if (prev_offset + header->padding + new_size > s->buf_len) {
            return NULL;
        }

        // Growing the allocation?
        if (old_size < new_size) {
            size_t growth = new_size - old_size;
            memset(cast(void *)(curr_addr + old_size), 0, growth);
            s->curr_offset += growth;
        } else {
            s->curr_offset -= old_size - new_size;
        }
        return old_ptr;
    }

    new_ptr = stack_alloc_align(s, new_size, align);
    return memmove(new_ptr, old_ptr, min_size);
}

void
stack_free(Stack *s, void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    uintptr_t start, end, curr_addr;
    Stack_Allocation_Header *header;
    size_t prev_offset;

    start     = cast(uintptr_t)s->buf;
    end       = start + cast(uintptr_t)s->buf_len;
    curr_addr = cast(uintptr_t)ptr;

    if (!(start <= curr_addr && curr_addr < end)) {
        assert(0 && "Out of bounds memory address passed to stack allocator (free)");
        return;
    }

    if (curr_addr >= start + cast(uintptr_t)s->curr_offset) {
        // Allow double frees
        return;
    }

    header = cast(Stack_Allocation_Header *)(curr_addr - sizeof(*header));

    // Calculate previous offset from header and its address.
    prev_offset = cast(size_t)(curr_addr - cast(uintptr_t)header->padding - start);
    if (prev_offset != s->prev_offset) {
        assert(0 && "Out of order stack allocator free");
        return;
    }

    // Reset the offsets to that of the previous allocation.
    s->curr_offset = s->prev_offset;
    s->prev_offset = header->prev_offset;
}

void
stack_free_all(Stack *s)
{
    s->prev_offset = 0;
    s->curr_offset      = 0;
}

static void *
stack_allocator_fn(void *context,
    Allocator_Mode       mode,
    void                *old_ptr,
    size_t               old_size,
    size_t               new_size,
    size_t               align)
{
    Stack *s = cast(Stack *)context;
    switch (mode) {
    case ALLOCATOR_ALLOC:
        return stack_alloc_align(s, new_size, align);
    case ALLOCATOR_RESIZE:
        return stack_resize_align(s, old_ptr, old_size, new_size, align);
    case ALLOCATOR_FREE:
        stack_free(s, old_ptr);
        break;
    case ALLOCATOR_FREE_ALL:
        stack_free_all(s);
        break;
    }
    return NULL;
}

Allocator
stack_allocator(Stack *s)
{
    Allocator a = {stack_allocator_fn, s};
    return a;
}

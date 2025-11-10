#include <string.h> // memcpy

#include "stack.h"

void *
stack_shrink(Stack *s, void *ptr, size_t old_size, size_t new_size)
{
    assert(old_size >= new_size);
    char *last_alloc = s->data + (s->offset - old_size);

    // 1.) Shrinking OR freeing the last allocation?
    if (ptr == last_alloc) {
        s->offset -= old_size - new_size;
    }

    // 1.1.) If shrinking, just return the old allocation.
    // 1.2.) If freeing, the return value is ignored anyway.
    return ptr;
}

// void
// stack_free(Stack *s, void *ptr, size_t size)
// {
//     (void)stack_shrink(s, ptr, size, /*new_size=*/0);
// }

void *
stack_resize(Stack *s, void *ptr, size_t old_size, size_t new_size)
{
    // 2.) Resize (grow) OR new request?
    char *last_alloc = s->data + (s->offset - old_size);

    // 2.1.) Resize (grow): last allocation is the same as this one?
    if (ptr == last_alloc) {
        if (old_size < new_size) {
            size_t growth = new_size - old_size;
            if (s->offset + growth <= sizeof(s->data)) {
                s->offset += growth;
                return ptr;
            }
            return NULL;
        }
        return stack_shrink(s, ptr, old_size, new_size);
    }

    // 2.2.) New request: Last allocation is NOT the same as this one.
    if (s->offset + new_size <= sizeof(s->data)) {
        char *new_ptr = s->data + s->offset;
        s->offset += new_size;
        // memcpy requires non-NULL arguments but allows zero lengths.
        if (ptr != NULL) {
            memcpy(new_ptr, ptr, old_size);
        }
        return new_ptr;
    }
    return NULL;
}

void
stack_free_all(Stack *s)
{
    s->offset = 0;
}

void *
stack_allocator_fn(void *ptr, size_t old_size, size_t new_size, void *context)
{
    Stack *s = cast(Stack *)context;

    // 1.) Resize (shrink) OR free request?
    if (old_size >= new_size) {
        return stack_shrink(s, ptr, old_size, new_size);
    }
    return stack_resize(s, ptr, old_size, new_size);
}

#+private package
package lulu

import "core:math"
import "core:mem"

MEMORY_ERROR_MESSAGE :: "Out of memory"

new :: proc($T: typeid, L: ^State, count: int, extra := 0) -> ^T {
    g    := L.global_state
    size := size_of(T) * count + extra
    ptr, err := mem.alloc(size, align_of(T), context.allocator)
    if err != nil {
        debug_memory_error(L, "allocate %i bytes", size)
    }
    g.bytes_allocated += size
    return cast(^T)ptr
}

free :: proc(L: ^State, ptr: ^$T, count := 1, extra := 0) {
    g    := L.global_state
    size := size_of(T) * count + extra
    g.bytes_allocated -= size
    mem.free_with_size(ptr, size, context.allocator)
}

/*
Allocates a new slice of type `T` with `count` elements, zero-initialized.

*Allocates using `context.allocator`.

**Assumptions**
- We are in a protected call, so we are able to catch out-of-memory errors.
 */
make_slice :: proc($T: typeid, L: ^State, count: int) -> []T {
    g   := L.global_state
    ptr := new(T, L, count=count)
    s   := (cast([^]T)ptr)[:count]
    return s
}

/*
`s[index] = value` but grows `s` if needed.

*Allocates using `context.allocator`*.

**Assumptions**
- We are in a protected call, so we are able to catch out-of-memory errors.
*/
insert_slice :: proc(L: ^State, s: ^$S/[]$T, #any_int index: int, value: T) {
    if index >= len(s) {
        new_count := max(8, math.next_power_of_two(index + 1))
        resize_slice(L, s, new_count)
    }
    s[index] = value
}

/*
Grows or shrinks `s` to be `count` elements, copying over the old elements
that fit in the new slice.

*Allocates using `context.allocator`*.

**Assumptions**
- We are in a proected call, so we are able to catch out-of-memory errors.
 */
resize_slice :: proc(L: ^State, s: ^$S/[]$T, count: int) {
    // Nothing to do?
    if count == len(s) {
        return
    }
    prev := s^
    next := make_slice(T, L, count)
    copy(next, prev)
    delete_slice(L, prev)
    s^ = next
}

/*
Frees the memory used by the slice `s`.

*Deallocates using `context.allocator`*.

**Assumptions**
- Freeing memory never fails.
 */
delete_slice :: proc(L: ^State, s: $S/[]$T) {
    free(L, raw_data(s), len(s))
}

/*
Find the index of `ptr` in the slice `s`, if it's even in the array to begin
with.
 */
find_ptr_index :: proc(s: $S/[]$T, ptr: ^T) -> (index: int, ok: bool) {
    addr  := cast(uintptr)ptr
    begin := cast(uintptr)raw_data(s)
    end   := begin + cast(uintptr)(len(s) * size_of(T))
    if begin <= addr && addr < end {
        return cast(int)(addr - begin) / size_of(T), true
    }
    return 0, false
}

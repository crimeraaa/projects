#+private
package lulu

import "core:fmt"
import "core:math"
import "core:mem"

MEMORY_ERROR_MESSAGE :: "Out of memory"

/*
Allocates a new pointer of type `T` plus `extra` bytes.

*Allocates using `G(L).backing_allocator`*.
 */
new :: proc($T: typeid, L: ^State, extra := 0, loc := #caller_location) -> ^T {
    g := G(L)
    when DEBUG_STRESS_GC {
        gc_collect(L, g, loc=loc)
    }

    size := size_of(T) + extra
    ptr, err := mem.alloc(size, align_of(T), allocator=g.backing_allocator, loc=loc)
    if err != nil {
        debug_memory_error(L, "allocate %i bytes", size, loc=loc)
    }
    gc_log_alloc(^T, .New, ptr, size, loc=loc)
    g.bytes_allocated += size
    return cast(^T)ptr
}

/*
Allocates a new slice of type `T` with `count` elements, zero-initialized.

*Allocates using `G(L).backing_allocator`.

**Assumptions**
- We are in a protected call, so we are able to catch out-of-memory errors.
 */
make :: proc($T: typeid, L: ^State, count: int, loc := #caller_location) -> []T {
    g := G(L)
    when DEBUG_STRESS_GC {
        gc_collect(L, g, loc=loc)
    }

    array, err := mem.make_slice([]T, count, allocator=g.backing_allocator, loc=loc)
    size := count * size_of(T)
    if err != nil {
        debug_memory_error(L, "allocate %i bytes", size)
    }
    gc_log_alloc([]T, .New, raw_data(array), size, loc=loc)
    g.bytes_allocated += size
    return array
}

/*
Frees a pointer of type `T` of `count` elements plus `extra` bytes.

*Deallocates using `G(L).backing_allocator`.*
 */
free :: proc(L: ^State, ptr: ^$T, extra := 0, loc := #caller_location) {
    g    := G(L)
    size := size_of(T) + extra
    gc_log_alloc(^T, .Free, ptr, size, loc=loc)
    g.bytes_allocated -= size
    mem.free_with_size(ptr, size, allocator=g.backing_allocator, loc=loc)
}

/*
Frees the memory used by the slice `array`.

*Deallocates using `G(L).backing_allocator`.*

**Assumptions**
- Freeing memory never fails.
 */
delete :: proc(L: ^State, array: $S/[]$T, loc := #caller_location) {
    g    := G(L)
    size := size_of(T) * len(array)
    gc_log_alloc([]T, .Free, raw_data(array), size, loc=loc)
    g.bytes_allocated -= size
    mem.delete_slice(array, allocator=g.backing_allocator)
}

/*
`array[index] = value` but grows `array` if needed.

*Allocates using `G(L).backing_allocator`*.

**Assumptions**
- We are in a protected call, so we are able to catch out-of-memory errors.
*/
append :: proc(L: ^State, array: ^$S/[]$T, #any_int index: int, value: T, loc := #caller_location) {
    if index >= len(array) {
        new_count := max(8, math.next_power_of_two(index + 1))
        resize(L, array, new_count, loc=loc)
    }
    array[index] = value
}

/*
Grows or shrinks `array` to be `count` elements, copying over the old elements
that fit in the new slice.

*Allocates using `G(L).backing_allocator`*.

**Assumptions**
- We are in a proected call, so we are able to catch out-of-memory errors.
 */
resize :: proc(L: ^State, array: ^$S/[]$T, count: int, loc := #caller_location) {
    // Nothing to do?
    if count == len(array) {
        return
    }
    prev := array^
    next := make(T, L, count, loc=loc)
    copy(next, prev)
    delete(L, prev)
    array^ = next
}

/*
Find the index of `ptr` in the slice `array`, if it's even in the array to begin
with.
 */
find_ptr_index :: proc(array: $S/[]$T, ptr: ^T) -> (index: int, ok: bool) #no_bounds_check {
    addr  := uintptr(ptr)
    begin := uintptr(raw_data(array))
    end   := uintptr(&array[len(array)])
    if begin <= addr && addr < end {
        return int(addr - begin) / size_of(T), true
    }
    return 0, false
}

find_ptr_index_unsafe :: proc(array: $S/[]$T, ptr: ^T) -> (index: int) #no_bounds_check {
    addr  := uintptr(ptr)
    begin := uintptr(raw_data(array))
    end   := uintptr(&array[len(array)])
    // If the result would be negative you're SOL anyway
    fmt.assertf(begin <= addr && addr < end, "Invalid ptr(%p)", ptr)
    return int(addr - begin) / size_of(T)
}

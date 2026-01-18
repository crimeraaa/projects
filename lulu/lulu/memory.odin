#+private package
package lulu

import "core:fmt"
import "core:math"
import "core:mem"
import os "core:os/os2"
import "core:strings"
import "core:terminal/ansi"

MEMORY_ERROR_MESSAGE :: "Out of memory"

@(disabled=true)
gc_log_alloc :: proc(
    T:      typeid,
    color:  string,
    action: string,
    ptr:    rawptr,
    size:   int,
    loc  := #caller_location
) {
    RESET        :: ansi.ESC + ansi.CSI + ansi.RESET + "m"
    COLOR_ACTION :: ansi.ESC + ansi.CSI + "%sm%-6s" + RESET + " | "

    buf1, buf2: [64]byte
    type_name := fmt.bprint(buf1[:], T)

    file := loc.file_path
    file  = file[strings.last_index_byte(file, os.Path_Separator) + 1:]
    file_line := fmt.bprintf(buf2[:], "%s:%i", file, loc.line)
    fmt.printfln(
        COLOR_ACTION + "%-20s | %-13s | %p (%i bytes)",
        color, action, file_line, type_name, ptr, size)
}

/*
Allocates a new pointer of type `T` of `count` elements plus `extra` bytes.

*Allocates using `L.global_state.backing_allocator`*.
 */
new_ptr :: proc($T: typeid, L: ^State, extra := 0, loc := #caller_location) -> ^T {
    g    := L.global_state
    size := size_of(T) + extra
    ptr, err := mem.alloc(size, align_of(T), allocator=g.backing_allocator)
    if err != nil {
        debug_memory_error(L, "allocate %i bytes", size)
    }
    gc_log_alloc(^T, ansi.FG_GREEN, "[NEW]", ptr, size, loc=loc)
    g.bytes_allocated += size
    return cast(^T)ptr
}

/*
Allocates a new slice of type `T` with `count` elements, zero-initialized.

*Allocates using `L.global_state.backing_allocator`.

**Assumptions**
- We are in a protected call, so we are able to catch out-of-memory errors.
 */
make_slice :: proc($T: typeid, L: ^State, count: int, loc := #caller_location) -> []T {
    g := L.global_state
    array, err := mem.make_slice([]T, count, allocator=g.backing_allocator)
    size := count * size_of(T)
    if err != nil {
        debug_memory_error(L, "allocate %i bytes", size)
    }
    gc_log_alloc([]T, ansi.FG_GREEN, "[NEW]", raw_data(array), size, loc=loc)
    g.bytes_allocated += size
    return array
}

/*
Frees a pointer of type `T` of `count` elements plus `extra` bytes.

*Deallocates using `L.global_state.backing_allocator`.*
 */
free_ptr :: proc(L: ^State, ptr: ^$T, extra := 0, loc := #caller_location) {
    g    := L.global_state
    size := size_of(T) + extra
    gc_log_alloc(^T, ansi.FG_RED, "[FREE]", ptr, size, loc=loc)
    g.bytes_allocated -= size
    mem.free_with_size(ptr, size, allocator=g.backing_allocator)
}

/*
Frees the memory used by the slice `array`.

*Deallocates using `L.global_state.backing_allocator`.*

**Assumptions**
- Freeing memory never fails.
 */
delete_slice :: proc(L: ^State, array: $S/[]$T, loc := #caller_location) {
    g    := L.global_state
    size := size_of(T) * len(array)
    gc_log_alloc([]T, ansi.FG_RED, "[FREE]", raw_data(array), size, loc=loc)
    g.bytes_allocated -= size
    mem.delete_slice(array, allocator=g.backing_allocator)
}

/*
`array[index] = value` but grows `array` if needed.

*Allocates using `L.global_state.backing_allocator`*.

**Assumptions**
- We are in a protected call, so we are able to catch out-of-memory errors.
*/
insert_slice :: proc(L: ^State, array: ^$S/[]$T, #any_int index: int, value: T) {
    if index >= len(array) {
        new_count := max(8, math.next_power_of_two(index + 1))
        resize_slice(L, array, new_count)
    }
    array[index] = value
}

/*
Grows or shrinks `array` to be `count` elements, copying over the old elements
that fit in the new slice.

*Allocates using `L.global_state.backing_allocator`*.

**Assumptions**
- We are in a proected call, so we are able to catch out-of-memory errors.
 */
resize_slice :: proc(L: ^State, array: ^$S/[]$T, count: int) {
    // Nothing to do?
    if count == len(array) {
        return
    }
    prev := array^
    next := make_slice(T, L, count)
    copy(next, prev)
    delete_slice(L, prev)
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

find_ptr_index_unsafe :: proc(array: $S/[]$T, ptr: ^T) -> (index: int) {
    addr  := uintptr(ptr)
    begin := uintptr(raw_data(array))
    // If the result would be negative you're SOL anyway
    fmt.assertf(addr >= begin, "Invalid ptr(%p)", ptr)
    return int(addr - begin) / size_of(T)
}

#+private package
package lulu

import "core:fmt"
import "core:math"
import "core:mem"
import "core:strings"
import "core:terminal/ansi"

MEMORY_ERROR_MESSAGE :: "Out of memory"

@(disabled=!ODIN_DEBUG)
gc_log_object :: proc(
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
    file  = file[strings.last_index_byte(file, '/') + 1:]
    file_line := fmt.bprintf(buf2[:], "%s:%i", file, loc.line)
    fmt.printfln(
        COLOR_ACTION + "%-20s | %-12s | %p (%i bytes)",
        color, action, file_line, type_name, ptr, size)
}

/*
Allocates a new pointer of type `T` of `count` elements plus `extra` bytes.

*Allocates using `context.allocator`*.
 */
new_ptr :: proc($T: typeid, L: ^State, count: int, extra := 0, loc := #caller_location) -> ^T {
    g    := L.global_state
    size := size_of(T) * count + extra
    ptr, err := mem.alloc(size, align_of(T), context.allocator)
    if err != nil {
        debug_memory_error(L, "allocate %i bytes", size)
    }
    gc_log_object(^T, ansi.FG_GREEN, "[NEW]", ptr, size, loc=loc)
    g.bytes_allocated += size
    return cast(^T)ptr
}

/*
Allocates a new slice of type `T` with `count` elements, zero-initialized.

*Allocates using `context.allocator`.

**Assumptions**
- We are in a protected call, so we are able to catch out-of-memory errors.
 */
make_slice :: proc($T: typeid, L: ^State, count: int, loc := #caller_location) -> []T {
    g := L.global_state
    array, err := mem.make_slice([]T, count, context.allocator)
    size := count * size_of(T)
    if err != nil {
        debug_memory_error(L, "allocate %i bytes", size)
    }
    gc_log_object([]T, ansi.FG_GREEN, "[NEW]", raw_data(array), size, loc=loc)
    g.bytes_allocated += size
    return array
}

/*
Frees a pointer of type `T` of `count` elements plus `extra` bytes.

*Deallocates using `context.allocator`.*
 */
free_ptr :: proc(L: ^State, ptr: ^$T, count := 1, extra := 0, loc := #caller_location) {
    g    := L.global_state
    size := size_of(T) * count + extra
    gc_log_object(^T, ansi.FG_RED, "[FREE]", ptr, size, loc=loc)
    g.bytes_allocated -= size
    mem.free_with_size(ptr, size, context.allocator)
}

/*
Frees the memory used by the slice `s`.

*Deallocates using `context.allocator`.*

**Assumptions**
- Freeing memory never fails.
 */
delete_slice :: proc(L: ^State, array: $S/[]$T, loc := #caller_location) {
    g    := L.global_state
    size := size_of(T) * len(array)
    gc_log_object([]T, ansi.FG_RED, "[FREE]", raw_data(array), size, loc=loc)
    g.bytes_allocated -= size
    mem.delete_slice(array, context.allocator)
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

find_ptr_index_unsafe :: proc(array: $S/[]$T, ptr: ^T) -> (index: int) {
    addr  := cast(uintptr)ptr
    begin := cast(uintptr)raw_data(array)
    return cast(int)(addr - begin) / size_of(T)
}

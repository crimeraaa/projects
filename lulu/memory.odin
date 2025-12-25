#+private package
package lulu

// import "core:math"

slice_make :: proc($T: typeid, L: ^VM, count: int) -> []T {
    s := make([]T, count, context.allocator)
    if s == nil {
        vm_error_memory(L)
    }
    return s
}

slice_insert :: proc(L: ^VM, s: ^$S/[]$T, index: int, value: T) {
    if index >= len(s) {
        new_count := max(8, math.next_power_of_two(index + 1))
        slice_resize(L, s, new_count)
    }
    s[index] = value
}

slice_resize :: proc(L: ^VM, s: ^$S/[]$T, count: int) {
    // Nothing to do?
    if count == len(s) {
        return
    }
    prev := s^
    next := slice_make(L, T, count)
    copy(next, prev)
    slice_delete(prev)
    s^ = next
}

slice_delete :: proc(s: $S/[]$T) {
    delete(s, context.allocator)
}

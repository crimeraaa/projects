#+private package
package lulu

// import "core:math"

/*
Allocates a new slice of type `T` with `count` elements, zero-initialized.

*Allocates using `context.allocator`.

**Assumptions**
- We are in a protected call, so we are able to catch out-of-memory errors.
 */
slice_make :: proc($T: typeid, L: ^VM, count: int) -> []T {
    s := make([]T, count, context.allocator)
    if s == nil {
        vm_error_memory(L)
    }
    return s
}

/*
`s[index] = value` but grows `s` if needed.

*Allocates using `context.allocator`*.

**Assumptions**
- We are in a protected call, so we are able to catch out-of-memory errors.
*/
slice_insert :: proc(L: ^VM, s: ^$S/[]$T, index: int, value: T) {
    if index >= len(s) {
        new_count := max(8, math.next_power_of_two(index + 1))
        slice_resize(L, s, new_count)
    }
    s[index] = value
}

/*
Grows or shrinks `s` to be `count` elements.

*Allocates using `context.allocator`*.

**Assumptions**
- We are in a proected call, so we are able to catch out-of-memory errors.
 */
slice_resize :: proc(L: ^VM, s: ^$S/[]$T, count: int) {
    // Nothing to do?
    if count == len(s) {
        return
    }
    prev := s^
    next := slice_make(T, L, count)
    copy(next, prev)
    slice_delete(prev)
    s^ = next
}

/*
Frees the memory used by the slice `s`.

*Deallocates using `context.allocator`*.

**Assumptions**
- Freeing memory never fails.
 */
slice_delete :: proc(s: $S/[]$T) {
    delete(s, context.allocator)
}

package lulu_repl

// standard
import "core:mem"

Fixed_Byte_Buffer :: struct {
    backing: []byte,
    offset:    int,
}

fb_buffer_make :: proc(backing: []byte) -> (b: Fixed_Byte_Buffer) {
    b.backing = backing
    b.offset  = 0
    return b
}

fb_buffer_allocator :: proc(b: ^Fixed_Byte_Buffer) -> mem.Allocator {
    return mem.Allocator{fb_buffer_allocator_proc, b}
}

fb_buffer_allocator_proc :: proc(
    allocator_data: rawptr,
    mode:           mem.Allocator_Mode,
    size:           int,
    alignment:      int,
    old_memory:     rawptr,
    old_size:       int,
    location := #caller_location,
) -> (data: []byte, err: mem.Allocator_Error) {
    // We should only ever used this allocator to request bytes.
    assert(mode == .Free || alignment == align_of(byte), loc=location)

    b := cast(^Fixed_Byte_Buffer)allocator_data
    #partial switch mode {
    case .Alloc:  return fb_buffer_alloc(b, size)
    case .Resize: return fb_buffer_resize(b, old_memory, old_size, size)
    case .Free:
        fb_buffer_free(b, old_memory, old_size)
        return nil, nil

    case:
        break
    }
    return nil, .Mode_Not_Implemented
}

fb_buffer_alloc :: proc(
    b:    ^Fixed_Byte_Buffer,
    size: int,
) -> (data: []byte, err: mem.Allocator_Error) {
    // For our purposes we assume the token stream is the only one who
    // allocates hence its starting offset should always be 0.
    start := b.offset
    assert(start == 0)

    stop := start + size
    return __alloc(b, start, stop)
}

fb_buffer_resize :: proc(
    b:          ^Fixed_Byte_Buffer,
    old_memory: rawptr,
    old_size:   int,
    new_size:   int,
) -> (data: []byte, err: mem.Allocator_Error) {
    // For our purposes we assume the token stream will only ever grow
    // the buffer in-place.
    growth := new_size - old_size
    assert(growth > 0)

    // For our purposes we assume the token stream is the only one who
    // allocates, so the starting offset is always 0 and we are always
    // resizing the exact same pointer.
    start    := b.offset - old_size
    prev_ptr := &b.backing[start]
    assert(start == 0)
    assert(old_memory == prev_ptr)

    stop := b.offset + growth
    return __alloc(b, start, stop)
}

fb_buffer_free :: proc(b: ^Fixed_Byte_Buffer, memory: rawptr, size: int) {
    start := b.offset - size
    assert(start == 0)

    prev_ptr := &b.backing[start]
    assert(memory == prev_ptr)

    b.offset = start
}

@(private="file")
__alloc :: proc(b: ^Fixed_Byte_Buffer, start, stop: int
) -> (data: []byte, err: mem.Allocator_Error) {
    if !(0 <= stop && stop <= len(b.backing)) {
        return nil, .Out_Of_Memory
    }
    data     = b.backing[start:stop]
    b.offset = stop
    return data, nil
}

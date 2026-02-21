package lulu_aux

// standard
import "core:unicode/utf8"
import "core:mem"

// local
import lulu ".."

BUFFER_SIZE :: #config(LULU_BUFFER_SIZE, mem.DEFAULT_PAGE_SIZE)

// When declaring variables of this type, we recommend using the non-zeroed
// initialization `---` to avoid needlessly zeroing out the entire `data`.
Buffer :: struct {
    L:      ^lulu.State,
    pushed: int,
    index:  int,
    data:   [BUFFER_SIZE]byte,
}

@(private="file")
__flush :: proc(b: ^Buffer) {
    lulu.push(b.L, string(b.data[:b.index]))
    b.index   = 0
    b.pushed += 1
}

@(private="file")
__flush_if_over :: proc(b: ^Buffer, count: int) {
    if b.index + count >= len(b.data) {
        __flush(b)
    }
}

buffer_init :: proc(L: ^lulu.State, buffer: ^Buffer) {
    buffer.L      = L
    buffer.index  = 0
    buffer.pushed = 0
}

write :: proc {
    write_byte,
    write_bytes,
    write_rune,
    write_string,
}

write_byte :: proc(b: ^Buffer, data: byte) {
    __flush_if_over(b, 1)
    b.data[b.index] = data
    b.index += 1
}

write_bytes :: proc(b: ^Buffer, data: []byte) {
    n := len(data)
    __flush_if_over(b, n)

    dst := b.data[b.index:b.index + n]
    copy(dst, data)
    b.index += n
}

write_rune :: proc(b: ^Buffer, data: rune) {
    bytes, size := utf8.encode_rune(data)
    write_bytes(b, bytes[:size])
}

write_string :: proc(b: ^Buffer, data: string) {
    write_bytes(b, transmute([]byte)data)
}

push_result :: proc(b: ^Buffer) {
    __flush(b)
    lulu.concat(b.L, b.pushed)
    b.pushed = 1
}

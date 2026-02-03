package lulu_aux

// standard
import "core:unicode/utf8"
import "core:mem"

// local
import lulu ".."

BUFFER_SIZE :: #config(LULU_BUFFER_SIZE, mem.DEFAULT_PAGE_SIZE)

Buffer :: struct {
    pushed: int,
    index:  int,
    data:   [BUFFER_SIZE - size_of(int) * 2]byte,
}

@(private="file")
__flush :: proc(L: ^lulu.State, b: ^Buffer) {
    lulu.push(L, string(b.data[:b.index]))
    b.index   = 0
    b.pushed += 1
}

@(private="file")
__flush_if_over :: proc(L: ^lulu.State, b: ^Buffer, count: int) {
    if b.index + count >= len(b.data) {
        __flush(L, b)
    }
}

write :: proc {
    write_byte,
    write_bytes,
    write_rune,
    write_string,
}

write_byte :: proc(L: ^lulu.State, b: ^Buffer, data: byte) {
    __flush_if_over(L, b, 1)
    b.data[b.index] = data
    b.index += 1
}

write_bytes :: proc(L: ^lulu.State, b: ^Buffer, data: []byte) {
    n := len(data)
    __flush_if_over(L, b, n)

    dst := b.data[b.index:b.index + n]
    copy(dst, data)
    b.index += n
}

write_rune :: proc(L: ^lulu.State, b: ^Buffer, data: rune) {
    bytes, size := utf8.encode_rune(data)
    write_bytes(L, b, bytes[:size])
}

write_string :: proc(L: ^lulu.State, b: ^Buffer, data: string) {
    write_bytes(L, b, transmute([]byte)data)
}

push_result :: proc(L: ^lulu.State, b: ^Buffer) {
    __flush(L, b)
    lulu.concat(L, b.pushed)
    b.pushed = 1
}

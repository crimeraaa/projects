package lulu_aux

// standard
import "core:unicode/utf8"

// local
import lulu ".."

BUFFER_SIZE :: #config(LULU_BUFFER_SIZE, 4096)

Buffer :: struct {
    pushed: int,
    index:  int,
    data:   [BUFFER_SIZE]byte,
}

@(private="file")
_flush :: proc(L: ^lulu.State, b: ^Buffer) {
    lulu.push(L, string(b.data[:b.index]))
    b.index   = 0
    b.pushed += 1
}

@(private="file")
_flush_if_over :: proc(L: ^lulu.State, b: ^Buffer, count: int) {
    if b.index + count >= len(b.data) {
        _flush(L, b)
    }
}

write_byte :: proc(L: ^lulu.State, b: ^Buffer, data: byte) {
    _flush_if_over(L, b, 1)
    b.data[b.index] = data
    b.index += 1
}

write_bytes :: proc(L: ^lulu.State, b: ^Buffer, data: []byte) {
    n := len(data)
    _flush_if_over(L, b, n)

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
    _flush(L, b)
    lulu.concat(L, b.pushed)
    b.pushed = 1
}

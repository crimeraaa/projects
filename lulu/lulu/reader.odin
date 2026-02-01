#+private package
package lulu

import "core:unicode/utf8"

Reader :: struct {
    procedure: Reader_Proc,
    user_data: rawptr,
    current:   []byte,
}

reader_make :: proc(procedure: Reader_Proc, user_data: rawptr) -> Reader {
    z: Reader
    z.procedure = procedure
    z.user_data = user_data
    return z
}

@(private="file")
__peek :: proc(z: ^Reader) -> (r: rune, n: int) {
    return utf8.decode_rune(z.current)
}

@(private="file")
__advance :: proc(z: ^Reader) -> (r: rune, n: int) {
    r, n = __peek(z)
    z.current = z.current[n:]
    return
}

reader_read_rune :: proc(z: ^Reader) -> (r: rune, n: int) {
    // Still have unread bytes?
    if len(z.current) > 0 {
        return __advance(z)
    } else {
        return reader_fill(z)
    }
}

reader_lookahead :: proc(z: ^Reader) -> (r: rune, n: int) {
    // No more unread bytes?
    if len(z.current) == 0 {
        r, n = reader_fill(z)
        if r == utf8.RUNE_EOF {
            return
        }
        // Fill removed the first rune, so return it.
        #no_bounds_check {
            z.current = z.current[-n:]
        }
    }
    return __peek(z)
}

reader_fill :: proc(z: ^Reader) -> (r: rune, n: int) {
    z.current = z.procedure(z.user_data)
    if len(z.current) == 0 {
        return utf8.RUNE_EOF, 0
    }
    return __advance(z)
}

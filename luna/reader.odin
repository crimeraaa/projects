#+private package
package luna

import "core:unicode/utf8"

Reader_Proc :: #type proc(user_data: rawptr) -> []byte

Reader :: struct {
    procedure: Reader_Proc,
    user_data: rawptr,
    iterator:  []byte,
}

reader_make :: proc(procedure: Reader_Proc, user_data: rawptr) -> (r: Reader) {
    r.procedure = procedure
    r.user_data = user_data
    return r
}

@(private="file")
__next :: proc(r: ^Reader) -> (rune, int) {
    display, size := utf8.decode_rune(r.iterator[:])
    r.iterator = r.iterator[size:]
    return display, size
}

reader_fill :: proc(r: ^Reader) -> (rune, int) {
    data := r.procedure(r.user_data)
    if data == nil {
        return utf8.RUNE_EOF, 0
    }
    r.iterator = data
    return __next(r)
}

reader_get_rune :: proc(r: ^Reader) -> (rune, int) {
    if len(r.iterator) == 0 {
        return reader_fill(r)
    }
    return __next(r)
}

reader_lookahead :: proc(r: ^Reader) -> (rune, int) {
    if len(r.iterator) == 0 {
        display, size := reader_fill(r)
        if display == utf8.RUNE_EOF {
            return display, 0
        }
        #no_bounds_check {
            r.iterator = r.iterator[-size:]
        }
    }
    return utf8.decode_rune(r.iterator[:])
}

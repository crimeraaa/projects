#+private
package luna

import "core:mem"

Ostring :: struct {
    using base:   Object_Header,
    keyword_type: Token_Type,
    hash:         u32,
    len:          int,
    data:         [0]byte,
}

ostring_new :: proc(L: ^State, text: string) -> (ostring: ^Ostring) {
    ostring = object_new(Ostring, L, len(text) + 1)
    ostring.len = len(text)
    #no_bounds_check {
        copy(ostring.data[:len(text)], text)
    }
    return
}

ostring_free :: proc(L: ^State, ostring: ^Ostring) {
    mem.free_with_size(ostring, size_of(ostring^) + ostring.len + 1)
}

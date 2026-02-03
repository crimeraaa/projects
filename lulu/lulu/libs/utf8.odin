#+private file
package lulu_libs

// standard
import "core:unicode/utf8"

// local
import lulu ".."
import lulu_aux "../aux"

@(private="package")
utf8_procs := [?]lulu_aux.Library_Entry{
    {"byte",    __byte},
    {"char",    __char},
    {"find",    __find},
    {"len",     __len},
    {"lower",   __lower},
    {"reverse", __reverse},
    {"sub",     __sub},
    {"upper",   __upper},
}

__len :: proc(L: ^lulu.State) -> (ret_count: int) {
    s := lulu_aux.check_string(L, 1)
    n := utf8.rune_count(s)
    lulu.push(L, n)
    return 1
}

__byte    :: proc(L: ^lulu.State) -> int { return string_byte(L,    is_utf8=true) }
__char    :: proc(L: ^lulu.State) -> int { return string_char(L,    rune) }
__lower   :: proc(L: ^lulu.State) -> int { return string_lower(L,   is_utf8=true) }
__find    :: proc(L: ^lulu.State) -> int { return string_find(L,    is_utf8=true) }
__reverse :: proc(L: ^lulu.State) -> int { return string_reverse(L, is_utf8=true) }
__sub     :: proc(L: ^lulu.State) -> int { return string_sub(L,     is_utf8=true) }
__upper   :: proc(L: ^lulu.State) -> int { return string_upper(L,   is_utf8=true) }

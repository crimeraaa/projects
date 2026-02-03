#+private file
package lulu_libs

// local
import lulu ".."
import lulu_aux "../aux"

@(private="package")
string_procs := [?]lulu_aux.Library_Entry{
    {"byte",    __byte},
    {"char",    __char},
    {"find",    __find},
    {"len",     __len},
    {"lower",   __lower},
    {"rep",     __rep},
    {"reverse", __reverse},
    {"sub",     __sub},
    {"upper",   __upper},
}

__len :: proc(L: ^lulu.State) -> (ret_count: int) {
    s := lulu_aux.check_string(L, 1)
    n := len(s)
    lulu.push(L, n)
    return 1
}

/*
**Note(2026-02-03)**
- In Lua, `string.rep` does not need to be UTF-8 aware so it can be used
for any encoding. Hence it is not included in the `utf8` library.
 */
__rep :: proc(L: ^lulu.State) -> (ret_count: int) {
    s := lulu_aux.check_string(L, 1)
    n := lulu_aux.check_integer(L, 2)

    b: lulu_aux.Buffer
    for _ in 0..<n {
        lulu_aux.write(L, &b, s)
    }
    lulu_aux.push_result(L, &b)
    return 1
}

__byte    :: proc(L: ^lulu.State) -> int { return string_byte(L,    is_utf8=false) }
__char    :: proc(L: ^lulu.State) -> int { return string_char(L,    byte) }
__find    :: proc(L: ^lulu.State) -> int { return string_find(L,    is_utf8=false) }
__lower   :: proc(L: ^lulu.State) -> int { return string_lower(L,   is_utf8=false) }
__reverse :: proc(L: ^lulu.State) -> int { return string_reverse(L, is_utf8=false) }
__sub     :: proc(L: ^lulu.State) -> int { return string_sub(L,     is_utf8=false) }
__upper   :: proc(L: ^lulu.State) -> int { return string_upper(L,   is_utf8=false) }

#+private file
package lulu_libs

// standard
import "core:unicode"

// local
import lulu ".."
import lulu_aux "../aux"

@(private="package")
string_procs := [?]lulu_aux.Library_Entry{
    {"byte",    __byte},
    {"char",    __char},
    {"len",     __len},
    {"lower",   __lower},
    {"rep",     __rep},
    {"reverse", __reverse},
    {"sub",     __sub},
    {"upper",   __upper},
}

__char :: proc(L: ^lulu.State) -> (ret_count: int) {
    b: lulu_aux.Buffer
    for i in 1..=lulu.get_top(L) {
        r := rune(lulu_aux.check_integer(L, i))
        lulu_aux.write(L, &b, r)
    }
    lulu_aux.push_result(L, &b)
    return 1
}

__len :: proc(L: ^lulu.State) -> (ret_count: int) {
    s := lulu_aux.check_string(L, 1)
    n := len(s)
    lulu.push(L, n)
    return 1
}

__conv :: proc(L: ^lulu.State, $procedure: $T) -> (ret_count: int) {
    b: lulu_aux.Buffer
    s := lulu_aux.check_string(L, 1)

    for r in s {
        r2 := procedure(r)
        lulu_aux.write(L, &b, r2)
    }
    lulu_aux.push_result(L, &b)
    return 1
}

__lower :: proc(L: ^lulu.State) -> (ret_count: int) {
    return __conv(L, unicode.to_lower)
}

__upper :: proc(L: ^lulu.State) -> (ret_count: int) {
    return __conv(L, unicode.to_upper)
}

__rep :: proc(L: ^lulu.State) -> (ret_count: int) {
    b: lulu_aux.Buffer
    s := lulu_aux.check_string(L, 1)
    n := lulu_aux.check_integer(L, 2)

    for _ in 0..<n {
        lulu_aux.write(L, &b, s)
    }
    lulu_aux.push_result(L, &b)
    return 1
}

__reverse :: proc(L: ^lulu.State) -> (ret_count: int) {
    b: lulu_aux.Buffer
    s := lulu_aux.check_string(L, 1)

    #reverse for r in s {
        lulu_aux.write(L, &b, r)
    }

    lulu_aux.push_result(L, &b)
    return 1
}

__byte :: proc(L: ^lulu.State) -> (ret_count: int) {
    s := lulu_aux.check_string(L, 1)
    i := lulu_aux.opt_integer(L, 2, default=1)
    j := lulu_aux.opt_integer(L, 3, default=i)

    i, j = __resolve_indices(s, i, j)
    view := s[i:j]
    for r in view {
        lulu.push(L, int(r))
    }
    return len(view)
}

__resolve_indices :: proc(s: string, rel_i, rel_j: int) -> (abs_i, abs_j: int) {
    n := len(s)
    // Convert to inclusive 0-based start index.
    if rel_i >= 0 {
        // `rel_i` is originally a 1-based inclusive start index.
        abs_i = max(rel_i - 1, 0)
    } else {
        abs_i = max(n + rel_i, 0)
    }

    // Convert to 0-based exclusive stop index.
    if rel_j > 0 {
        // `rel_j` is originally a 1-based inclusive stop index.
        abs_j = min(rel_j, n)
    } else {
        abs_j = max(n + rel_j + 1, abs_i)
    }
    return
}

__sub :: proc(L: ^lulu.State) -> (ret_count: int) {
    s := lulu_aux.check_string(L, 1)
    i := lulu_aux.check_integer(L, 2)
    j := lulu_aux.opt_integer(L, 3, default=len(s))

    i, j = __resolve_indices(s, i, j)
    lulu.push(L, s[i:j])
    return 1
}

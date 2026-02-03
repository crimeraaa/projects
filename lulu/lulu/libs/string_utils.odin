#+private package
package lulu_libs

// standard
import "core:bytes"
import "core:strings"
import "core:unicode"
import "core:unicode/utf8"

// local
import lulu ".."
import lulu_aux "../aux"

string_char :: proc(L: ^lulu.State, $T: typeid) -> (ret_count: int) {
    b: lulu_aux.Buffer
    for i in 1..=lulu.get_top(L) {
        r := T(lulu_aux.check_integer(L, i))
        lulu_aux.write(L, &b, r)
    }
    lulu_aux.push_result(L, &b)
    return 1
}

string_lower :: proc(L: ^lulu.State, $is_utf8: bool) -> (ret_count: int) {
    return __convert(L, unicode.to_lower, is_utf8)
}

string_upper :: proc(L: ^lulu.State, $is_utf8: bool) -> (ret_count: int) {
    return __convert(L, unicode.to_upper, is_utf8)
}

@(private="file")
__convert :: proc(L: ^lulu.State, $procedure: $T, $is_utf8: bool) -> (ret_count: int) {
    s := lulu_aux.check_string(L, 1)
    b: lulu_aux.Buffer

    // Odin's string iteration is already UTF-8 aware, however the Lua
    // `string` library treats strings as byte sequences.
    for r in s when is_utf8 else transmute([]byte)s {
        converted := procedure(r when is_utf8 else rune(r))
        lulu_aux.write(L, &b, converted)
    }
    lulu_aux.push_result(L, &b)
    return 1
}

string_reverse :: proc(L: ^lulu.State, $is_utf8: bool) -> (ret_count: int) {
    s := lulu_aux.check_string(L, 1)
    b: lulu_aux.Buffer
    #reverse for r in s when is_utf8 else transmute([]byte)s {
        lulu_aux.write(L, &b, r)
    }
    lulu_aux.push_result(L, &b)
    return 1
}


/*
Convert 1-based (byte|rune) indices to 0-based byte indices.

**Parameters**
- rel_i: An inclusive 1-based start (byte|rune) index. May be negative.
- rel_j: An inclusive 1-based stop  (byte|rune) index. May be negative.
- is_utf8: If `true`, then `rel_[ij]` will be treated as rune indices, otherwise
they will be treated as byte indices.

**Returns**
- abs_i: An inclusive 0-based start byte index. Will never be negative.
- abs_j: An exclusive 0-based stop byte index. Will never be negative.
 */
resolve_indices :: proc(s: string, rel_i, rel_j: int, $is_utf8: bool) -> (abs_i, abs_j: int) {
    n := len(s)

    abs_i = resolve_start(s, rel_i, is_utf8)
    // Convert to 0-based exclusive stop index.
    if rel_j > 0 {
        // `rel_j` is originally a 1-based inclusive stop index.
        // If 'too positive' then clamp to the length.
        abs_j = min(rel_j, n)
    } else {
        // If 'too negative' then clamp to `abs_i`, creating a 0-length string.
        abs_j = max(n + rel_j + 1, abs_i)
    }

    when is_utf8 {
        abs_j = utf8.rune_offset(s, abs_j) if abs_j > 0 else 0
    }
    return
}

/*
**Parameters**
- rel_i: An inclusive 1-based start rune index. May be negative.

**Returns**
- abs_i: An inclusive 0-based start byte index. May clamp to 0.
 */
resolve_start :: proc(s: string, rel_i: int, $is_utf8: bool) -> (abs_i: int) {
    if rel_i >= 0 {
        // `rel_i` is originally a 1-based inclusive start index.
        abs_i = max(rel_i - 1, 0)
    } else {
        // `rel_i` was originally a negative offset. The 1-based `-1` is
        // the same as the 1-based `+1`, which is the same as the 0-based `0`.
        //
        // If 'too negative' then just clamp to the starting index.
        abs_i = max(len(s) + rel_i, 0)
    }

    when is_utf8 {
        return utf8.rune_offset(s, abs_i) if abs_i > 0 else 0
    } else {
        return abs_i
    }
}

string_byte :: proc(L: ^lulu.State, $is_utf8: bool) -> (ret_count: int) {
    s := lulu_aux.check_string(L, 1)
    i := lulu_aux.opt_integer(L, 2, default=1)
    j := lulu_aux.opt_integer(L, 3, default=i)

    i, j = resolve_indices(s, i, j, is_utf8)
    view := s[i:j] when is_utf8 else transmute([]byte)s[i:j]
    count := 0
    for r in view {
        lulu.push(L, int(r))
        count += 1
    }
    return count
}


string_find :: proc(L: ^lulu.State, $is_utf8: bool) -> (ret_count: int) {
    s       := lulu_aux.check_string(L, 1)
    pattern := lulu_aux.check_string(L, 2)

    i := lulu_aux.opt_integer(L, 3, default=1)
    i = resolve_start(s, i, is_utf8)
    when is_utf8 {
        sub_i := strings.index(s[i:], pattern)
    } else {
        sub_i := bytes.index(transmute([]byte)s[i:], transmute([]byte)pattern)
    }

    if sub_i == -1 {
        lulu.push_nil(L)
        return 1
    }

    // TODO(2026-02-03): Handle UTF-8 rune offsets...
    // Use resulting byte index as is.
    lulu.push(L, sub_i + 1)
    lulu.push(L, sub_i + len(pattern))
    return 2
}

string_sub :: proc(L: ^lulu.State, $is_utf8: bool) -> (ret_count: int) {
    s := lulu_aux.check_string(L, 1)
    n := utf8.rune_count(s) when is_utf8 else len(s)
    i := lulu_aux.check_integer(L, 2)
    j := lulu_aux.opt_integer(L, 3, default=n)

    // Both results are always 0-based byte offsets.
    i, j = resolve_indices(s, i, j, is_utf8)
    lulu.push(L, s[i:j])
    return 1
}

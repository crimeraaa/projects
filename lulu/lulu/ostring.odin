#+private package
package lulu

import "core:mem"
import "core:strings"

Intern :: struct {
    // Each entry in the string table is actually a linked list.
    table: []^Object_List,

    // Number of non-nil pointers (including list chains) in `table`.
    count: int,
}

Ostring :: struct {
    using base: Object_Header,

    // Keyword type, used to help speed up string comparisons in the lexer.
    // 0 (`nil`) indicates it is not a keyword, otherwise it should be in
    // the range `.And..=.While`.
    kw_type: Token_Type,

    // Hash value of `data[:len]` used for quick comparisons.
    hash: u32,

    // Actual number of `byte` in `data`.
    len: int,

    // Flexible array member. The actual character array is stored right
    // after the previous member in memory.
    data: [0]byte,
}

/*
Convert `s` to an Odin-readable `string`.

**Parameters**
- s: The `^Ostring` instance to be converted.

**Returns**
- The `string` representation of `s`.

**Guarantees**
- If needed, `s` is also nul-terminated. The returned `string` can be safely
converted to `cstring`.
 */
ostring_to_string :: #force_inline proc(s: ^Ostring) -> string #no_bounds_check {
    assert(s.data[s.len] == 0)
    return string(s.data[:s.len])
}

/*
Hashes `data` using the 32-bit FNV-1A hash algorithm.
 */
hash_bytes :: #force_inline proc(data: []byte) -> u32 {
    FNV1A_OFFSET :: 0x811c_9dc5
    FNV1A_PRIME  :: 0x0100_0193

    hash := u32(FNV1A_OFFSET)
    for b in data {
        hash ~= cast(u32)b
        hash *= FNV1A_PRIME
    }
    return hash
}

mod_pow2 :: #force_inline proc(a, n: uint) -> uint {
    return a & (n - 1)
}

/*
Reuse an existing interned copy of `text`, or creates a new one.

*Allocates using `context.allocator`.*

**Parameters**
- L: Holds the global state which in turn holds the interned strings table.
- text: The string to be interned.

**Returns**
- A pointer to the interned string.
 */
ostring_new :: proc(L: ^VM, text: string) -> ^Ostring {
    intern    := &G(L).intern
    table     := intern.table
    table_cap := len(table)
    assert(table_cap >= 2)

    hash  := hash_bytes(transmute([]byte)text)
    index := mod_pow2(cast(uint)hash, cast(uint)table_cap)
    for node := table[index]; node != nil; node = node.next {
        s := &node.string
        if hash == s.hash && text == ostring_to_string(s) {
            return s
        }
    }

    s := object_new(Ostring, L, &table[index], len(text) + 1)
    s.len  = len(text)
    s.hash = hash
    #no_bounds_check {
        data := s.data[:s.len]
        copy(data, text)
        data[s.len] = 0
    }

    // Count refers to the total number of linked list nodes, including chains,
    // rather than occupied array slots. Even so we probably want to rehash
    // anyway to reduce clustering.
    if intern.count + 1 > table_cap {
        intern_resize(L, intern, table_cap * 2)
    }
    intern.count += 1
    return s
}

ostringf_new :: proc(L: ^VM, fmt: string, args: ..any) -> ^Ostring {
    fmt := fmt
    b   := &L.builder
    strings.builder_reset(b)
    arg_i := 0
    for {
        fmt_i := strings.index_byte(fmt, '%')
        if fmt_i == -1 {
            strings.write_string(b, fmt)
            break
        }

        // Write any bits of the string before the format specifier.
        strings.write_string(b, fmt[:fmt_i])

        arg   := args[arg_i]
        arg_i += 1
        spec_i := fmt
        switch spec := fmt[fmt_i + 1]; spec {
        case 'c':
            strings.write_rune(b, arg.(rune))
        case 'd', 'i':
            strings.write_int(b, arg.(int))
        case 'f':
            bit_size := size_of(f64) * 8
            strings.write_float(b, arg.(f64), fmt='g', prec=14, bit_size=bit_size)
        case 's':
            strings.write_string(b, arg.(string))
        case 'p':
            addr := cast(uintptr)arg.(rawptr)
            strings.write_string(b, "0x")
            strings.write_u64(b, cast(u64)addr, base=16)
        case:
            unreachable("Unsupported format specifier '%c'", spec)
        }

        // Move over the format specifier.
        fmt = fmt[fmt_i + 1:]
    }
    return ostring_new(L, strings.to_string(b^))
}

/*
Frees the contents of `s`. Since we are a flexible-array similar to Pascal
strings, we allocated everything in one go and can thus free it in the same way.

*Deallocates using `context.allocator`.*

**Assumptions**
- Freeing memory never fails.
 */
ostring_free :: proc(s: ^Ostring) {
    size := size_of(s^) + s.len + 1
    mem.free_with_size(s, size, context.allocator)
}

/*
Resize the intern table to fit more strings.

*Allocates using `context.allocator`.*

**Assumptions**
- We only ever grow the interned strings table.
- This is first called upon VM startup to ensure that subsequent calls to
`ostring_new` never see a zero-length table.
 */
intern_resize :: proc(L: ^VM, intern: ^Intern, new_cap: int) {
    old_table   := intern.table
    new_table   := slice_make(^Object_List, L, new_cap)
    intern.table = new_table
    defer slice_delete(old_table)

    // Rehash all strings from the old table into the new table.
    for list in old_table {
        this_node := list
        // Rehash all children for this list.
        for this_node != nil {
            ostring := &this_node.string
            index   := mod_pow2(cast(uint)ostring.hash, cast(uint)new_cap)

            // Save because it's about to be replaced.
            next_node := ostring.next

            // Chain this node in the NEW table, using the NEW main index.
            ostring.next     = new_table[index]
            new_table[index] = this_node

            // Next iteration.
            this_node = next_node
        }
    }
}

/*
Free all the memory used by the interned strings table and the strings themselves.

*Deallocates using `context.allocator`.*
 */
intern_destroy :: proc(L: ^VM, intern: ^Intern) {
    for list in intern.table {
        object_free_all(L, list)
    }
    slice_delete(intern.table)
}

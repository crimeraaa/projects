#+private
package lulu

Intern :: struct {
    // Each entry in the string table is actually a linked list.
    // `len(table)` is the capacity, not the number of active elements.
    table: []^Object,

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
        hash ~= u32(b)
        hash *= FNV1A_PRIME
    }
    return hash
}

mod_pow2 :: #force_inline proc(a, n: uint) -> uint {
    return a & (n - 1)
}

/*
Reuse an existing interned copy of `text`, or creates a new one.

*Allocates using `L.global_state.backing_allocator`.*

**Parameters**
- L: Holds the global state which in turn holds the interned strings table.
- text: The string to be interned.

**Returns**
- A pointer to the interned string.
 */
ostring_new :: proc(L: ^State, text: string, loc := #caller_location) -> ^Ostring {
    intern    := &L.global_state.intern
    table     := intern.table
    table_cap := len(table)
    assert(table_cap >= 2)

    hash  := hash_bytes(transmute([]byte)text)
    index := mod_pow2(uint(hash), uint(table_cap))
    list  := table[index]
    for node in object_iterator(&list) {
        s := &node.string
        if hash == s.hash && text == ostring_to_string(s) {
            return s
        }
    }

    ostring := object_new(Ostring, L, &table[index], len(text) + 1, loc=loc)
    ostring.len  = len(text)
    ostring.hash = hash
    #no_bounds_check {
        data := ostring.data[:ostring.len]
        copy(data, text)
        data[ostring.len] = 0
    }

    // Count refers to the total number of linked list nodes, including chains,
    // rather than occupied array slots. Even so we probably want to rehash
    // anyway to reduce clustering.
    if intern.count + 1 > table_cap {
        // GC: Barrier
        object_set_gray(ostring)
        intern_resize(L, intern, table_cap * 2, loc=loc)
        object_set_white(ostring)
    }
    intern.count += 1
    return ostring
}

ostring_size :: proc(ostring: ^Ostring) -> int {
    return size_of(ostring^) + ostring.len + 1
}

/*
Frees the contents of `s`. Since we are a flexible-array similar to Pascal
strings, we allocated everything in one go and can thus free it in the same way.

*Deallocates using `L.global_state.backing_allocator`.*

**Assumptions**
- Freeing memory never fails.
 */
ostring_free :: proc(L: ^State, s: ^Ostring, loc := #caller_location) {
    free(L, s, extra=s.len + 1, loc=loc)
}

/*
Resize the intern table to fit more strings.

*Allocates using `L.global_state.backing_allocator`.*

**Assumptions**
- We only ever grow the interned strings table.
- This is first called upon VM startup to ensure that subsequent calls to
`ostring_new` never see a zero-length table.
 */
intern_resize :: proc(L: ^State, intern: ^Intern, new_cap: int, loc := #caller_location) {
    old_table   := intern.table
    new_table   := make(^Object, L, new_cap, loc=loc)
    intern.table = new_table

    // Rehash all strings from the old table into the new table.
    for list in old_table {
        list := list
        // Rehash all children for this list.
        for node in object_iterator(&list) {
            ostring := &node.string

            // Chain this node in the NEW table, using the NEW main index.
            i := mod_pow2(uint(ostring.hash), uint(new_cap))
            ostring.next = new_table[i]
            new_table[i] = node
        }
    }
    delete(L, old_table, loc=loc)
}

/*
Free all the memory used by the interned strings table and the strings themselves.

*Deallocates using `L.global_state.backing_allocator`.*
 */
intern_destroy :: proc(L: ^State, intern: ^Intern) {
    for list in intern.table {
        object_free_all(L, list)
    }
    delete(L, intern.table)
}

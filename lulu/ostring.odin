#+private package
package lulu

Intern :: struct {
    // Each entry in the string table is actually a linked list.
    table: []^Object_List,

    // Number of non-nil pointers (including list chains) in `table`.
    count: int,
}

OString :: struct {
    using base: Object_Header,

    // Hash value of `data[:len]` used for quick comparisons.
    hash: u32,

    // Actual number of `byte` in `data`.
    len: int,

    // Flexible array member. The actual character array is stored right
    // after the previous member in memory.
    data: [0]byte,
}

/*
Iterate a linked list of interned strings using a `for ... in` loop.

**Parameters**
- list: The address of some `^OString` which will be mutated to help keep
loop state. Do not pass addresses of global state members directly, e.g.
`&G(L).intern.table[i].ostring`. Rather, copy the state member's value to a
stack local and pass the address of said local.

**Returns**
- node: The current node. May be `nil`.
- ok: `true` if `node` is non-`nil`, indicating the loop can continue,
else `false.`, indicating the loop should stop.
 */
ostring_nodes :: proc(list: ^^OString) -> (node: ^OString, ok: bool) {
    node = list^
    ok   = node != nil
    if ok {
        // Prepare next iteration.
        list^ = &node.next.ostring
    }
    return node, ok
}

/*
Convert `s` to an Odin-readable `string`.

**Parameters**
- s: The `^OString` instance to be converted.

**Returns**
- The `string` representation of `s`.

**Guarantees**
- If needed, `s` is also nul-terminated. The returned `string` can be safely
converted to `cstring`.
 */
ostring_to_string :: proc(s: ^OString) -> string #no_bounds_check {
    assert(s.data[s.len] == 0)
    return string(s.data[:s.len])
}

/*
Hashes `text` using the 32-bit FNV-1A hash algorithm.
 */
hash_string :: proc(text: string) -> u32 {
    FNV1A_OFFSET :: 0x811c9dc5
    FNV1A_PRIME  :: 0x01000193

    hash := u32(FNV1A_OFFSET)
    for c in text {
        hash ~= cast(u32)c
        hash *= FNV1A_PRIME
    }
    return hash
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
ostring_new :: proc(L: ^VM, text: string) -> ^OString {
    intern    := &G(L).intern
    table     := intern.table
    table_cap := len(table)
    assert(table_cap >= 2)

    hash   := hash_string(text)
    index  := hash_index(hash, table_cap)
    list   := &table[index].ostring
    for s in ostring_nodes(&list) {
        if hash == s.hash && text == ostring_to_string(s) {
            return s
        }
    }

    s := object_new(OString, L, &table[index], len(text) + 1)
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
        intern_resize(L, intern, table_cap << 1)
    }
    intern.count += 1
    return s
}

/*
Given a string's hash value, return the primary index into the intern table.

**Parameters**
- hash: The value returned from `hash_string()`.
- table_cap: The slice length of the intern table. Must be a power of 2.

**Assumptions**
- `table_cap` is a power of 2. This is necessary to optimize modulo into
a bitwise AND.
 */
@(private="file")
hash_index :: proc(hash: u32, table_cap: int) -> uint {
    a := cast(uint)hash
    n := cast(uint)table_cap
    return a & (n - 1)
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
            ostring := &this_node.ostring
            index   := hash_index(ostring.hash, new_cap)

            // Save because it's about to be replaced.
            next_node := ostring.next

            // Chain this node in the NEW table, using the NEW main index.
            ostring.next     = new_table[index]
            new_table[index] = this_node
            this_node        = next_node
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

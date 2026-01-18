#+private file
package lulu

// This is required because `log2(0)` is undefined, so a zero-sized table
// is unrepresentable with `log2_cap`. So the empty table is actually
// represented by the address of this entry with `log2_cap=0` for an actual
// capacity of 1.
@rodata
_EMPTY_ENTRY: Entry

@(private="package")
Table :: struct {
    using base: Object_Header,

    // Table capacity is stored as a log2 (such that `1 << log2_cap` is
    // equivalent to `2 ** log2_cap`) to help reduce padding.
    log2_cap: u8,

    // List of all key-value pairs ('entries').
    entries: [^]Entry,

    // Number of active entries in `entries`.
    count: int,
}

Entry :: struct {
    key:   Key,
    value: Value,
}

Key :: struct #raw_union {
    // Useful to pass `Key` instances to functions expecting `Value`.
    using v: Value,

    // Optimized to reduce unnecessary padding of `hash` if we know it can
    // safely fit in the would-be padding of `type`.
    h: struct {
        data: Value_Data,
        type: Value_Type,

        // Saving the result of `hash_value()` is useful because it helps us
        // do quick comparisons for early-outs. It also allows to avoid
        // redundant work in `table_resize()`.
        hash: u32,
    }
}

@(private="package")
table_new :: proc(L: ^State, hash_count, array_count: int) -> (t: ^Table) {
    t = object_new(Table, L, &L.global_state.objects)

    // Required so that `table_resize()` below sees the empty table rather
    // than attempting to dereference a nil entries array with 1 element.
    t.entries = &_EMPTY_ENTRY
    cap := hash_count + array_count
    if cap > 0 {
        _resize(L, t, max(cap, 8))
    }
    return t
}

/*
Frees the memory allocated for the table struct itself and its entries array.
Each occupied entry, however, is not deallocated because they may be
referenced in other parts of the program.

**Deallocates using `L.global_state.backing_allocator`.*
 */
@(private="package")
table_free :: proc(L: ^State, t: ^Table) {
    if t.entries != &_EMPTY_ENTRY {
        delete_slice(L, _get_entries(t))
    }
    free_ptr(L, t)
}

_get_entries :: proc(t: ^Table) -> []Entry {
    return t.entries[:_get_cap(t)]
}

_get_cap :: proc(t: ^Table) -> (cap: int) {
    return 1 << t.log2_cap
}

@(private="package")
table_len :: proc(t: ^Table) -> (len: int) {
    for {
        i := value_make_number(f64(len + 1))
        if v, ok := table_get(t, i); !ok || value_is_nil(v) {
            break
        }
        len += 1
    }
    return len
}

/*
Queries the table `t` for key `k`. The value at the corresponding key is
returned.

**Returns**
- v: The value mapped to `k`. It can be `nil`.
- ok: `true` if the key `k` for the corresponding entry was occupied
beforehand (i.e. it was non-nil) else `false`.
 */
@(private="package")
table_get :: proc(t: ^Table, k: Value) -> (v: Value, ok: bool) #optional_ok {
    hash := _hash_value(k)
    entry: ^Entry
    entry, ok = _find_entry(_get_entries(t), k, hash)
    return entry.value, ok
}

/*
Queries the table `t` for key `k`. May resize the table.

**Returns**
- v: A mutable pointer to `t[k]`. It may exist in the table beforehand or
be just inserted.

**Assumptions**
- We do not directly set `v` to allow the caller to handle that. This is so
that , in case they want to handle things like metamethods, they can check if
`v == nil` to determine when to trigger the `__newindex` metamethod.
 */
@(private="package")
table_set :: proc(L: ^State, t: ^Table, k: Value) -> (v: ^Value) {
    // 75% load factor.
    if cap := _get_cap(t); t.count + 1 >= cap * 3 / 2 {
        cap = max(cap * 2, 8)
        _resize(L, t, cap)
    }

    hash := _hash_value(k)
    entry, ok := _find_entry(_get_entries(t), k, hash)
    if !ok {
        t.count += 1
    }

    entry.key.v      = k
    entry.key.h.hash = hash
    return &entry.value
}

_resize :: proc(L: ^State, t: ^Table, new_cap: int) {
    old_entries  := _get_entries(t)
    new_log2_cap := table_log2(new_cap)
    new_entries  := make_slice(Entry, L, 1 << new_log2_cap)

    // We may have nil keys in the old table so we need to ignore them.
    new_count := 0
    for old_entry in old_entries {
        k := old_entry.key
        if value_is_nil(k) {
            continue
        }
        // Non-nil keys must be rehashed in the new table.
        new_entry := _find_entry(new_entries, k, k.h.hash)
        new_entry^ = old_entry
        new_count += 1
    }

    // We actually own our underlying array?
    if t.entries != &_EMPTY_ENTRY {
        delete_slice(L, old_entries)
    }

    t.log2_cap = new_log2_cap
    t.entries  = raw_data(new_entries)
    t.count    = new_count
}


/*
Find the nearest power of 2 to `cap` if it is not one already.

**Returns**
- exp: The exponent of said power of 2.

**Guarantees**
- The actual capacity can be decoded via bitwise left shift.
 */
@(private="package")
table_log2 :: proc(cap: int) -> (exp: u8) {
    assert(cap > 0)
    for {
        pow := 1 << exp
        if pow >= cap {
            break
        }
        exp += 1
    }
    return exp
}

/*
**Assumptions**
- There is at least 1 completely `nil` entry in `entries`.

**Returns**
- entry: Pointer to `entries[k]` or else the first free entry.
- ok: `true` if `entry` was occupied beforehand (i.e. it had a non-nil key)
else `false`.
 */
_find_entry :: proc(entries: []Entry, k: Value, hash: u32) -> (entry: ^Entry, ok: bool) #optional_ok {
    tomb: ^Entry
    cap := uint(len(entries))
    for i := mod_pow2(uint(hash), cap); /* empty */; i = mod_pow2(i + 1, cap) {
        entry = &entries[i]

        // Entry is either partially or completely empty?
        if value_is_nil(entry.key) {
            // Entry is completely empty, so it was never occupied?
            if value_is_nil(entry.value) {
                if tomb != nil {
                    entry = tomb
                }
                return entry, false
            }

            // Otherwise, this entry is partially empty (i.e. it has a nil key
            // with a non-nil value, meaning it was occupied at some point then
            // 'abandoned'. Continue searching but note this finding.
            if tomb == nil {
                tomb = entry
            }
        } else if hash == entry.key.h.hash && value_eq(k, entry.key) {
            return entry, true
        }
    }
    unreachable("How did you even get here?")
}

_hash_value :: proc(v: Value) -> u32 {
    hash_any :: #force_inline proc(v: $T) -> u32 {
        v := v
        return hash_bytes((transmute([^]byte)&v)[:size_of(T)])
    }

    t := value_type(v)
    switch t {
    case .Nil:      return hash_any([0]byte{})
    case .Boolean:  return hash_any(value_get_bool(v))
    case .Number:   return hash_any(value_get_number(v))
    case .String:   return value_get_ostring(v).hash
    case .Light_Userdata, .Table, .Function:
        return hash_any(value_get_pointer(v))

    case .Chunk:
        break
    }
    unreachable("Invalid type '%v'", t)
}

#+private file
package lulu

import "core:slice"

@(private="package")
Table :: struct {
    using base: Object_Header,
    entries: []Entry,
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
        
        // Saving the result of `hash_value` is useful because it helps us
        // do quick comparisons for early-outs.
        hash: u32,
    }
}

@(private="package")
table_new :: proc(L: ^VM, n: int) -> (t: ^Table) {
    g := G(L)
    t = object_new(Table, L, &g.objects)
    if n > 0 {
        resize_and_rehash(L, t, min(n, 8))
    }
    return t
}

/* 
Frees the memory allocated for the table struct itself and its entries array.
Each occupied entry, however, is not deallocated because they may be
referenced in other parts of the program.

**Deallocates using `context.allocator`.*
 */
@(private="package")
table_free :: proc(t: ^Table) {
    slice_delete(t.entries)
    free(t)
}

/* 
Queries the table `t` for key `k`. The value at the corresponding key is
returned.

**Returns**
- v: The value mapped to `k`. It can be `nil`.
- exists: `true` if the key `k` for the corresponding entry was occupied
beforehand (i.e. it was non-nil) else `false`.
 */
@(private="package")
table_get :: proc(t: ^Table, k: Value) -> (v: Value, exists: bool) #optional_ok {
    if len(t.entries) == 0 {
        return value_make(), false
    }
    
    hash := hash_value(k)

    entry: ^Entry
    entry, exists = find_entry(t.entries, k, hash)
    return entry.value, exists
}

@(private="package")
table_set :: proc(L: ^VM, t: ^Table, k, v: Value) {
    if n := len(t.entries); t.count + 1 > n * 3 / 2 {
        n = min(n * 2, 8)
        resize_and_rehash(L, t, n)
    }

    hash := hash_value(k)
    entry, exists := find_entry(t.entries, k, hash)
    if !exists {
        t.count += 1
    }

    entry.key.v      = k
    entry.key.h.hash = hash
    entry.value      = v
}

resize_and_rehash :: proc(L: ^VM, t: ^Table, n: int) {
    old_entries := t.entries
    new_entries := slice_make(Entry, L, n)

    // We may have nil keys in the old table so we need to ignore them.
    count := 0
    for old_entry in old_entries {
        k := old_entry.key
        if value_is_nil(k) {
            continue
        }
        // Non-nil keys must be rehashed in the new table.
        new_entry := find_entry(new_entries, k, hash_key(k))
        new_entry^ = old_entry
        count += 1
    }

    delete(old_entries)
    t.count   = count
    t.entries = new_entries
}

/* 
**Assumptions**
- There is at least 1 completely `nil` entry in `entries`.
 */
find_entry :: proc(entries: []Entry, k: Value, hash: u32) -> (entry: ^Entry, exists: bool) #optional_ok {
    wrap := cast(uint)(len(entries) - 1)
    tomb: ^Entry
    for i := cast(uint)hash & wrap; /* empty */; i = (i + 1) & wrap {
        entry = &entries[i]
        
        // Entry is either partially or completely empty?
        if value_is_nil(entry.key) {
            // Entry is completely empty, so it was never occupied?
            if value_is_nil(entry.value) {
                return tomb if tomb != nil else entry, false
            }

            // Otherwise, this entry is partially empty (i.e. it has a nil key
            // with a non-nil value, meaning it was occupied at some point then
            // 'abandoned'. Continue searching but note this finding.
            if tomb == nil {
                tomb = entry
            }
        } else if hash == hash_key(entry.key) && value_eq(k, entry.key) {
            return entry, true
        }
    }
    unreachable("How did you even get here?")
}

hash_key :: proc(k: Key) -> u32 {
    return k.h.hash
}

hash_value :: proc(v: Value) -> u32 {
    hash_any :: proc(v: $T) -> u32 {
        v    := v
        data := slice.bytes_from_ptr(&v, size_of(v))
        return hash_bytes(data)
    }

    t := value_type(v)
    switch t {
    case .Nil:      return 0
    case .Boolean:  return hash_any(value_to_boolean(v))
    case .Number:   return hash_any(value_to_number(v))
    case .String:   return value_to_ostring(v).hash
    case .Table:    return hash_any(value_to_object(v))
    case .Chunk:
        break
    }
    unreachable("Invalid type '%v'", t)
}

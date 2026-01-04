#+private package
package lulu

import "base:intrinsics"
import "core:fmt"
import "core:terminal/ansi"

RED   :: ansi.ESC + ansi.CSI + ansi.FG_RED   + "m"
GREEN :: ansi.ESC + ansi.CSI + ansi.FG_GREEN + "m"
RESET :: ansi.ESC + ansi.CSI + ansi.RESET    + "m"

Object :: struct #raw_union {
    using base: Object_Header,
    string:     Ostring,
    table:      Table,
    chunk:      Chunk,
}

// Only exists to be 'inherited from'. Do not create lone instances of this type.
Object_Header :: struct #packed {
    next: ^Object,
    type: Value_Type,
    mark: bit_set[Object_Mark; u8],
}

// Flags for the garbage collector.
Object_Mark :: enum u8 {
    // This object has not yet been processed in this particular GC run.
    White,

    // This object has been traversed and all its children have been
    // traversed as well.
    Black,

    // This object is never collectible, e.g. interned keywords.
    Fixed,
}

/*
Create a new object of type `T`, appending it to `list`.

*Allocates using `context.allocator`.*

**Parameters**
- T: The desired type of the resulting object, which must 'inherit' from
`Object_Header`.
- list: Address of some external state member, e.g. `G(L).objects` which
links objects together in a list.
- extra: Number of extra bytes to be allocated in case of flexible array
members, e.g. `Ostring`. If `T` is fixed-size then it should remain zero.

**Returns**
- A pointer to some object `T` which has been linked and initialized.

**Guarantees**
- The returned pointer is never `nil`.

**Assumptions**
- We are in a protected call, so upon allocation failure we are able to
recover to the first protected caller.
 */
object_new :: proc($T: typeid, L: ^State, list: ^^Object, extra := 0) -> ^T
where intrinsics.type_is_subtype_of(T, Object_Header) {
    o := new(T, L, count=1, extra=extra)

    // Chain the new object.
    o.next = list^
    when      T == Ostring do o.type = Value_Type.String \
    else when T == Table   do o.type = Value_Type.Table  \
    else when T == Chunk   do o.type = Value_Type.Chunk  \
    else do #panic("Invalid T")

    // This object is freshly allocated so it has never been traversed.
    o.mark = {.White}
    list^  = cast(^Object)o

    when ODIN_DEBUG {
        ts := value_type_string(o.type)
        fmt.printfln(GREEN + " [NEW] " + RESET + "%8s: %p (%i bytes)", ts, o, size_of(T) + extra)
    }
    return o
}

/*
Free an object and without unlinking it.

*Deallocates using `context.allocator`.*

**Parameters**
- o: The object instance to be freed.

**Guarantees**
- The linked list containing `o` is not (yet) invalidated. It is the duty of
the garbage collector to handle the unlinking for us.
 */
object_free :: proc(L: ^State, o: ^Object) {
    t := o.type
    switch t {
    case .String:   ostring_free(L, &o.string)
    case .Table:    table_free(L, &o.table)
    case .Chunk:    chunk_free(L, &o.chunk)
    case .Nil, .Boolean, .Number:
        unreachable("Invalid object to free: %v", t)
    }

    when ODIN_DEBUG {
        fmt.printfln(RED + "[FREE] " + RESET + "%8s: %p", value_type_string(t), o)
    }
}

object_free_all :: proc(L: ^State, list: ^Object) {
    node := list
    for node != nil {
        // Save here because contents are about to be freed.
        next := node.next
        object_free(L, node)
        node = next
    }
}

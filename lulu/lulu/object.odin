#+private package
package lulu

import "base:intrinsics"
import "core:mem"

Object :: struct #raw_union {
    using base: Object_Header,
    ostring:    Ostring,
    table:      Table,
    chunk:      Chunk,
}

// Aliases to document intention. Generally meant for iteration.
Object_List :: Object

// Aliases to document intention.
GC_List     :: Object

// Only exists to be 'inherited from'. Do not create lone instances of this type.
Object_Header :: struct #packed {
    next: ^Object_List,
    type:  Value_Type,
    mark:  bit_set[Object_Mark; u8],
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
object_new :: proc($T: typeid, L: ^VM, list: ^^Object_List, extra := 0) -> ^T
where intrinsics.type_is_subtype_of(T, Object_Header) {
    size     := size_of(T) + extra
    ptr, err := mem.alloc(size, align_of(T), context.allocator)
    if ptr == nil || err != nil {
        vm_error_memory(L)
    }
    derived := cast(^T)ptr

    // Chain the new object.
    derived.next = list^
    when      T == Ostring do derived.type = Value_Type.String \
    else when T == Table   do derived.type = Value_Type.Table \
    else when T == Chunk   do derived.type = Value_Type.Chunk \
    else do #panic("Invalid T")

    // This object is freshly allocated so it has never been traversed.
    derived.mark = {.White}
    list^ = cast(^Object)derived
    return derived
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
object_free :: proc(o: ^Object) {
    switch o.type {
    case .String:   ostring_free(&o.ostring)
    case .Table:    table_free(&o.table)
    case .Chunk:    chunk_free(&o.chunk)
    case .Nil, .Boolean, .Number:
        unreachable("Invalid object to free: %v", o.type)
    }
}

object_free_all :: proc(L: ^VM, list: ^Object_List) {
    node := list
    for node != nil {
        // Save here because contents are about to be freed.
        next := node.next
        object_free(node)
        node = next
    }
}

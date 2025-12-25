#+private package
package lulu

import "base:intrinsics"
import "core:mem"
import "core:fmt"

assertf :: fmt.assertf
panicf  :: fmt.panicf

Object :: struct #raw_union {
    base:    Object_Header,
    ostring: OString,
}

// Aliases to document intention.
Object_List :: Object
GC_List     :: Object

Object_Header :: struct #packed {
    next: ^Object,
    type:  Value_Type,
    mark:  bit_set[Object_Mark],
}

Object_Mark :: enum u8 {
    // This object has not yet been processed in this particular GC run.
    White,

    // This object has been traversed and all its children have been
    // traversed as well.
    Black,

    // This object is never collectible, e.g. interned keywords.
    Fixed,
}

Value_Type :: enum u8 {
    String,
}

/*
Create a new object of type `T`, appending it to `list`.

*Allocates using `context.allocator`.

**Parameters**
- T: The desired type of the resulting object, which must 'inherit' from
`Object_Header`.
- list: Address of some external state member, e.g. `G(L).objects` which
links objects together in a list.
- extra: Number of extra bytes to be allocated in case of flexible array
members, e.g. `OString`. If `T` is fixed-size then it should remain zero.

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
    o := cast(^T)ptr

    // Chain the new object.
    o.next = list^
    list^  = cast(^Object)o
    when T == OString {
        o.type = Value_Type.String
    } else {
        #panic("Invalid T")
    }
    // This object is freshly allocated so it has never been traversed.
    o.mark = {.White}
    return o
}

/*
Free an object and without unlinking it.

**Parameters**
- o: The object instance to be freed.

**Guarantees**
- The linked list containing `o` is not (yet) invalidated. It is the duty of
the garbage collector to handle the unlinking for us.
 */
object_free :: proc(L: ^VM, o: ^Object) {
    switch o.base.type {
    case .String:
        s    := &o.ostring
        size := size_of(s^) + s.len + 1
        mem.free_with_size(&o.ostring, size, context.allocator)
    case:
        panicf("Invalid object (Value_Type=%s)")
    }
}

/*
Iterate a linked list of objects using `for ... in` syntax.

**Parameters**
- list: The address of some `^Object_List` which will be mutated to help keep
loop state. Do not pass addresses of external state members directly,
e.g. `&G(L).objects`. Rather, copy the pointer value to a stack local and pass
the address of said local.

**Returns**
- node: The current node. May be `nil`.
- ok: `true` if `node` is non-`nil`, indicating the loop can continue,
else `false.`, indicating the loop should stop.
 */
object_nodes :: proc(list: ^^Object_List) -> (node: ^Object, ok: bool) {
    node = list^
    ok   = node != nil
    if ok {
        list^ = node.base.next
    }
    return node, ok
}


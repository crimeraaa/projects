#+private
package lulu

import "base:intrinsics"

Object :: struct #raw_union {
    using base: Object_Header,
    string:     Ostring,
    table:      Table,
    function:   Closure,
    chunk:      Chunk,
}

// Only exists to be 'inherited from'. Do not create lone instances of this type.
Object_Header :: struct #packed {
    next: ^Object,
    type:  Value_Type,
    flags: Object_Flags,
}

// Flags for the garbage collector.
Object_Flags :: bit_set[Object_Flag; u8]

// Flags for the garbage collector.
Object_Flag :: enum u8 {
    // If not set, then this object is White; i.e. it has not yet been
    // processed in this particular GC run. Until otherwise known,we assume
    // this object and all its children are collectible.
    //
    // If set then this object is Gray or Black.
    Marked,

    // This object is never collectible, e.g. interned keywords.
    Fixed,
}

/*
Create a new object of type `T`, appending it to `list`.

*Allocates using `L.global_state.backing_allocator`.*

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
object_new :: proc($T: typeid, L: ^State, list: ^^Object, extra := 0, loc := #caller_location) -> ^T
where intrinsics.type_is_subtype_of(T, Object_Header) {
    // This object is freshly allocated so it has never been traversed.
    // So we leave its flags as 0 to indicate it is color white.
    object := new(T, L, extra=extra, loc=loc)
    base   := cast(^Object)object

    // Chain the new object.
    object.next = list^
    when      T == Ostring     do object.type = Value_Type.String      \
    else when T == Table       do object.type = Value_Type.Table       \
    else when T == Chunk       do object.type = Value_Type.Chunk       \
    else when T == Closure_Api do object.type = Value_Type.Function    \
    else when T == Closure_Lua do object.type = Value_Type.Function    \
    else do #panic("Invalid T")

    list^ = base
    return object
}

// Even if the current object is freed, the iterator state is still valid.
object_iterator :: proc(state: ^^Object) -> (current: ^Object, ok: bool) {
    current = state^
    if ok = current != nil; ok {
        state^ = current.next
    }
    return
}

object_typeid :: proc(object: ^Object) -> typeid {
    closure_typeid :: proc(closure: ^Closure) -> typeid {
        if closure.is_lua {
            return ^Closure_Lua
        }
        return ^Closure_Api
    }

    #partial switch object.type {
    case .String:   return ^Ostring
    case .Table:    return ^Table
    case .Function: return closure_typeid(&object.function)
    case .Chunk:    return ^Chunk
    case:
        break
    }
    unreachable("Invalid object type %v", object.type)
}

object_size :: proc(object: ^Object) -> int {
    #partial switch object.type {
    case .String:   return ostring_size(&object.string)
    case .Table:    return size_of(object.table)
    case .Function: return closure_size(&object.function)
    case .Chunk:    return size_of(object.chunk)
    case:
    }
    unreachable("Invalid object type %v", object.type)
}

/*
Free an object and without unlinking it.

*Deallocates using `L.global_state.backing_allocator`.*

**Parameters**
- o: The object instance to be freed.

**Guarantees**
- The linked list containing `o` is not (yet) invalidated. It is the duty of
the garbage collector to handle the unlinking for us.
 */
object_free :: proc(L: ^State, obj: ^Object, loc := #caller_location) {
    t := obj.type
    switch t {
    case .String:   ostring_free(L, &obj.string, loc=loc)
    case .Table:    table_free(L, &obj.table, loc=loc)
    case .Chunk:    chunk_free(L, &obj.chunk)
    case .Function: closure_free(L, &obj.function)
    case .Nil, .Boolean, .Number, .Light_Userdata, .Integer:
        unreachable("Invalid object to free: %v", t, loc=loc)
    }

}

object_free_all :: proc(L: ^State, list: ^Object) {
    state := list
    for node in object_iterator(&state) {
        object_free(L, node)
    }
}

object_is_marked :: proc(object: ^$T) -> bool
where intrinsics.type_is_subtype_of(T, Object_Header) {
    return .Marked in object.flags
}

object_is_fixed :: proc(object: ^$T) -> bool
where intrinsics.type_is_subtype_of(T, Object_Header) {
    return .Fixed in object.flags
}

object_set_fixed :: proc(object: ^$T) {
    assert(!object_is_fixed(object))
    object.flags += {.Fixed}
}

object_is_reachable :: proc(object: ^$T) -> bool {
    return object_is_marked(object) || object_is_fixed(object)
}

object_set_gray  :: object_set_marked
object_set_white :: object_clear_marked

object_set_marked :: proc(object: ^$T) {
    assert(!object_is_marked(object))
    object.flags += {.Marked}
}

object_clear_marked :: proc(object: ^$T) {
    // assert(object_is_reachable(object), loc=loc)
    object.flags -= {.Marked}
}

object_clear_fixed :: proc(object: ^$T) {
    assert(object_is_fixed(object))
    object.flags -= {.Fixed}
}

#+private
package luna

import "base:intrinsics"
import "core:mem"

Object :: struct #raw_union {
    using base: Object_Header,
    ostring: Ostring,
}

Object_Header :: struct #packed {
    prev: ^Object,
    type: Value_Type,
}

object_new :: proc($T: typeid, L: ^State, extra := 0) -> (object: ^T)
where intrinsics.type_is_subtype_of(T, Object_Header) {
    // Allocation
    ptr, err := mem.alloc(size_of(T) + extra)
    if ptr == nil || err != nil {
        run_throw_error(L, .Memory)
    }

    // Initialization
    object = cast(^T)ptr
    when T == Ostring { object.type = .String }
    else { #panic("Invalid T") }

    // Chaining
    object.prev = L.objects
    L.objects   = cast(^Object)object
    return object
}

object_free :: proc(L: ^State, object: ^Object) {
    #partial switch object.type {
    case .String:
    case:
        unreachable()
    }
}

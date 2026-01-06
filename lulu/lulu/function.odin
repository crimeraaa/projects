#+private package
package lulu

Closure :: struct #raw_union {
    using base: Closure_Header,
    api: Api_Closure,
    lua: Lua_Closure,
}

// Only exists to be 'inherited from'. Do not create lone instances of this type.
Closure_Header :: struct #packed {
    using base_object: Object_Header,
    is_lua: bool,
    upvalue_count: u8,
}

Api_Closure :: struct {
    using base_closure: Closure_Header,
    procedure: Api_Proc,
    upvalues:  [0]Value,
}

Lua_Closure :: struct {
    using base_closure: Closure_Header,
    chunk: ^Chunk,
}

api_closure_new :: proc(L: ^State, procedure: Api_Proc, upvalue_count: u8) -> ^Closure {
    assert(upvalue_count >= 0)

    g     := L.global_state
    extra := size_of(Value) * int(upvalue_count)

    cl := object_new(Api_Closure, L, &g.objects, extra)
    cl.is_lua        = false
    cl.upvalue_count = upvalue_count
    cl.procedure     = procedure
    // Assume that the flexible `upvalues` array is already zero-initialized.
    return cast(^Closure)cl
}

lua_closure_new :: proc(L: ^State, chunk: ^Chunk, upvalue_count: u8) -> ^Closure {
    // Upvalue object not yet implemented.
    assert(upvalue_count == 0)
    g     := L.global_state
    extra := int(upvalue_count)

    cl := object_new(Lua_Closure, L, &g.objects, extra)
    cl.is_lua        = true
    cl.upvalue_count = upvalue_count
    cl.chunk         = chunk
    return cast(^Closure)cl
}

closure_free :: proc(L: ^State, cl: ^Closure) {
    if cl.is_lua {
        free_ptr(L, &cl.lua)
    } else {
        extra := size_of(Value) * int(cl.upvalue_count)
        free_ptr(L, &cl.api, extra=extra)
    }
}

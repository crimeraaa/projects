#+private package
package lulu

Closure :: struct #raw_union {
    using base: Closure_Header,
    api: Closure_Api,
    lua: Closure_Lua,
}

// Only exists to be 'inherited from'. Do not create lone instances of this type.
Closure_Header :: struct #packed {
    using base_object: Object_Header,
    is_lua: bool,
    upvalue_count: u8,
}

Closure_Api :: struct {
    using base_closure: Closure_Header,
    procedure: Api_Proc,
    upvalues:  [0]Value,
}

Closure_Lua :: struct {
    using base_closure: Closure_Header,
    chunk: ^Chunk,
}

closure_api_new :: proc(L: ^State, procedure: Api_Proc, upvalue_count: u8) -> ^Closure {
    assert(upvalue_count >= 0)

    extra := size_of(Value) * int(upvalue_count)

    cl := object_new(Closure_Api, L, &G(L).objects, extra)
    cl.is_lua        = false
    cl.upvalue_count = upvalue_count
    cl.procedure     = procedure
    // Assume that the flexible `upvalues` array is already zero-initialized.
    return cast(^Closure)cl
}

closure_lua_new :: proc(L: ^State, chunk: ^Chunk, upvalue_count: u8) -> ^Closure {
    // Upvalue object not yet implemented.
    assert(upvalue_count == 0)
    g     := L.global_state
    extra := int(upvalue_count)

    cl := object_new(Closure_Lua, L, &g.objects, extra)
    cl.is_lua        = true
    cl.upvalue_count = upvalue_count
    cl.chunk         = chunk
    return cast(^Closure)cl
}

closure_free :: proc(L: ^State, cl: ^Closure) {
    if cl.is_lua {
        free(L, &cl.lua)
    } else {
        extra := size_of(Value) * int(cl.upvalue_count)
        free(L, &cl.api, extra=extra)
    }
}

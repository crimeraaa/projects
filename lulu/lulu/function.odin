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

    // Only using during the Mark and Traverse phases of the garbage collector.
    //
    // This object is always independent, so it can be root during
    // garbage collection.
    gc_list: ^Gc_List,
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

closure_get_chunk :: proc(closure: ^Closure) -> ^Chunk {
    assert(closure.is_lua)
    return closure.lua.chunk
}

closure_api_new :: proc(L: ^State, procedure: Api_Proc, upvalue_count: u8) -> ^Closure {
    assert(upvalue_count >= 0)

    extra := size_of(Value) * int(upvalue_count)

    closure := object_new(Closure_Api, L, &G(L).objects, extra)
    closure.is_lua        = false
    closure.upvalue_count = upvalue_count
    closure.procedure     = procedure
    // Assume that the flexible `upvalues` array is already zero-initialized.
    return cast(^Closure)closure
}

closure_size :: proc(closure: ^Closure) -> int #no_bounds_check {
    slice_size :: proc(slice: $S/[]$T) -> int {
        return size_of(slice[0]) * len(slice)
    }

    if closure.is_lua {
        return size_of(closure.lua)
    }
    upvalues := closure.api.upvalues[:closure.api.upvalue_count]
    return size_of(closure.api) + slice_size(upvalues)
}

closure_lua_new :: proc(L: ^State, chunk: ^Chunk, upvalue_count: u8) -> ^Closure {
    // Upvalue object not yet implemented.
    assert(upvalue_count == 0)
    g     := L.global_state
    extra := int(upvalue_count)

    closure := object_new(Closure_Lua, L, &g.objects, extra)
    closure.is_lua        = true
    closure.upvalue_count = upvalue_count
    closure.chunk         = chunk
    return cast(^Closure)closure
}

closure_free :: proc(L: ^State, closure: ^Closure) {
    if closure.is_lua {
        free(L, &closure.lua)
    } else {
        extra := size_of(Value) * int(closure.upvalue_count)
        free(L, &closure.api, extra=extra)
    }
}

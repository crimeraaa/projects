package lulu

import "core:strings"

Type :: enum {
    None = -1,
    Nil,
    Boolean,
    Number,
    String,
    Table,
}

Error :: enum {
    // No error occured.
    Ok,

    // Invalid token or semantically invalid sequence of tokens, was received.
    // This error is often easy to recover from.
    Syntax,

    // Some operation or a function call failed.
    // This error is slightly difficult to recover from, but manageable.
    Runtime,

    // Failed to allocate or reallocate some memory.
    // This error is often fatal. It is extremely difficult to recover from.
    Memory,
}

@(private="file", rodata)
VALUE_NONE: Value


/*
Retrieve the value at the acceptable index or pseudo-index `index`.

An acceptable index is merely an absolute or relative index which directly
refers to a stack slot in the range `[1..=top]`. If it is positive, then it
is a 1-based index which must be resolved to a 0-based index. If it is negative,
then it is a position relative to the top.

A pseudo-index is an index which can never refer to a valid stack slot. Instead,
it is used to refer to API internals such as the globals table, registry and
current function upvalues.

**Links**
- 3.2 - The Stack:      https://www.lua.org/manual/5.1/manual.html#3.2
- 3.3 - Pseudo-Indices: https://www.lua.org/manual/5.1/manual.html#3.3
 */
@(private="file")
get_index :: proc(L: ^State, index: int) -> ^Value {
    i   := index
    top := get_top(L)
    if i > 0 {
        i -= 1
    } else {
        assert(i != 0)
        i += top
    }

    return &L.registers[i] if 0 <= i && i < top else &VALUE_NONE
}

new_state :: proc() -> (L: ^State, ok: bool) {
    @static
    g: Global_State

    @static
    vm: State
    ok = vm_init(&vm, &g)
    if ok {
        L = &vm
    } else {
        vm_destroy(L)
    }
    return L, ok
}

close :: proc(L: ^State) {
    vm_destroy(L)
}

get_top :: proc(L: ^State) -> int {
    return len(L.registers)
}

set_top :: proc(L: ^State, index: int) {
    if index >= 0 {
        L.registers = L.registers[:index]
    } else {
        vm_pop(L, -index)
    }
}

pop :: proc(L: ^State, count: int) {
    L.registers = L.registers[:get_top(L) - count]
}

type :: proc(L: ^State, index: int) -> Type {
    v := get_index(L, index)
    if v == &VALUE_NONE {
        return .None
    }

    // lulu.Value_Type to lulu.Type
    vt := value_type(v^)
    switch vt {
    case .Nil:      return .Nil
    case .Boolean:  return .Boolean
    case .Number:   return .Number
    case .String:   return .String
    case .Table:    return .Table
    case .Chunk:
        break;
    }
    unreachable("Invalid value type %v", vt)
}

to_boolean :: proc(L: ^State, index: int) -> (b: bool) {
    v := get_index(L, index)^
    return !value_is_falsy(v)
}

to_number :: proc(L: ^State, index: int) -> (n: f64, ok: bool) #optional_ok {
    v := get_index(L, index)^
    ok = value_is_number(v)
    n  = value_get_number(v) if ok else 0.0
    return n, ok
}

to_string :: proc(L: ^State, index: int) -> (s: string, ok: bool) #optional_ok {
    v := get_index(L, index)^
    ok = value_is_string(v)
    s  = value_get_string(v) if ok else ""
    return s, ok
}

load :: proc(L: ^State, name, input: string) -> Error {
    // Must be outside protected call to ensure that we can defer destroy.
    b, _ := strings.builder_make(allocator=context.allocator)
    defer strings.builder_destroy(&b)

    Data :: struct {
        builder:    ^strings.Builder,
        name, input: string,
    }

    parse :: proc(L: ^State, user_data: rawptr) {
        data  := (cast(^Data)user_data)^
        name  := ostring_new(L, data.name)
        program(L, data.builder, name, data.input)
    }
    return vm_raw_pcall(L, parse, &Data{&b, name, input})
}

pcall :: proc(L: ^State, arg_count, ret_count: int) -> Error {
    Data :: struct {
        arg_count, ret_count: int,
    }

    call :: proc(L: ^State, user_data: rawptr) {
        data     := (cast(^Data)user_data)^
        old_base := vm_save_base(L)
        old_top  := vm_save_top(L)

        new_base := old_top - data.arg_count
        callee   := L.stack[new_base - 1]
        chunk    := value_get_chunk(callee)
        new_top  := new_base + chunk.stack_used

        // Push new stack frame.
        registers := L.stack[new_base:new_top]
        frame     := &L.frames[L.frame_index]
        L.frame_index += 1
        L.frame        = frame

        // Initialize the newly pushed stack frame.
        frame.chunk     = chunk
        frame.registers = registers
        frame.saved_pc  = -1

        vm_execute(L)

        // Restore previous stack frame.
        L.frame_index -= 1
        L.frame = &L.frames[L.frame_index]
        set_top(L, new_base - 1 + data.ret_count)
    }
    return vm_raw_pcall(L, call, &Data{arg_count, ret_count})
}

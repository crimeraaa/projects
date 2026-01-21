package lulu

import "core:mem"
import "core:strings"

Type :: enum {
    // API only. Returned by `type()` for values that do not exist in the
    // stack nor the state internals.
    None = -1,
    Nil,
    Boolean,
    Number,
    Light_Userdata,
    String,
    Table,
    Function,
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

/*
API. Lulu passes data to Odin similar to how Lua passes data to C.

**Assumptions**

- The arguments, if any, are pushed to the stack. They can be accessed starting
from index `1`
- Upon return, the previous stack frame (the caller's a.k.a. our parent)
is now able to see the results.

**Guarantees**
- The API procedure shall push its return values in order, if any, to the stack.

 */
Api_Proc :: #type proc(L: ^State) -> (ret_count: int)


/*
Indicates that a function will take an arbitrary number of arguments
(when passed as `arg_count`) and/or return an arbitrary number of values
(when passed as `ret_count`).
 */
VARIADIC :: -1

/*
Pseudo index to access the globals table of the given state.
 */
GLOBALS_INDEX  :: -16_000
UPVALUES_INDEX :: GLOBALS_INDEX - 2

Main_State :: struct {
    global_state: Global_State,
    main_state:   State,
}

new_state :: proc(ms: ^Main_State, allocator: mem.Allocator) -> (L: ^State, ok: bool) {
    ok = vm_init(&ms.main_state, &ms.global_state, allocator)
    if ok {
        L = &ms.main_state
    } else {
        vm_destroy(&ms.main_state)
    }
    return L, ok
}

close :: proc(L: ^State) {
    vm_destroy(L)
}

get_top :: proc(L: ^State) -> int {
    return len(L.registers)
}

/*
**Analogous to**
- `lua_settop(lua_State *L, int index)` in the Lua 5.1.5 API.

**Guarantees**
- If growing the stack view, then the new elements are set to `nil`.
 */
set_top :: proc(L: ^State, index: int) {
    if index >= 0 {
        old_base := vm_save_base(L)
        old_top  := vm_save_top(L)
        new_top  := old_base + index
        values   := L.stack[old_base:new_top]
        if new_top > old_top {
            mem.zero_slice(L.stack[old_top:new_top])
        }
        L.registers = values
    } else {
        vm_pop_value(L, -index)
    }
}

pop :: proc(L: ^State, count: int) {
    L.registers = L.registers[:get_top(L) - count]
}

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
_get_any_index :: proc(L: ^State, index: int) -> ^Value {
    i   := index
    top := get_top(L)
    if i > 0 {
        i -= 1
    } else {
        assert(i != 0)
        i += top
    }

    if 0 <= i && i < top {
        return &L.registers[i]
    }

    // Original value may have been a pseudo index.
    switch index {
    case GLOBALS_INDEX: return &L.globals_table
    }

    if index <= UPVALUES_INDEX {
        cl := value_get_function(L.frame.callee^)
        if !cl.is_lua {
            i = index - UPVALUES_INDEX
            if 1 <= i && i <= int(cl.upvalue_count) {
                return &cl.api.upvalues[i - 1]
            }
        }
    }
    return &_VALUE_NONE
}

@(private="file", rodata)
_VALUE_NONE: Value

@(private="file")
_get_stack_index :: proc(L: ^State, index: int) -> ^Value {
    i := index
    if i > 0 {
        i -= 1
    } else {
        assert(i != 0)
        i += get_top(L)
    }

    // Let slice bounds checking fail if we got an unacceptable index or a
    // pseudo-index.
    return &L.registers[i]
}


type :: proc(L: ^State, index: int) -> Type {
    v := _get_any_index(L, index)
    if v == &_VALUE_NONE {
        return .None
    }
    return cast(Type)value_type(v^)
}

type_name :: proc(L: ^State, type: Type) -> string {
    if type == .None do return "no value"
    return value_type_string(cast(Value_Type)type)
}

type_name_at :: proc(L: ^State, index: int) -> string {
    t := type(L, index)
    return type_name(L, t)
}

is_boolean :: proc(L: ^State, index: int) -> bool {
    v := _get_any_index(L, index)^
    return value_is_boolean(v)
}

is_number :: proc(L: ^State, index: int) -> bool {
    v := _get_any_index(L, index)^
    return value_is_number(v)
}

is_string :: proc(L: ^State, index: int) -> bool {
    v := _get_any_index(L, index)^
    return value_is_string(v)
}

to_boolean :: proc(L: ^State, index: int) -> (b: bool) {
    v := _get_any_index(L, index)^
    return !value_is_falsy(v)
}

to_number :: proc(L: ^State, index: int) -> (n: f64, ok: bool) #optional_ok {
    v := _get_any_index(L, index)^
    ok = value_is_number(v)
    n  = value_get_number(v) if ok else 0.0
    return n, ok
}

to_pointer :: proc(L: ^State, index: int) -> (p: rawptr, ok: bool) #optional_ok {
    v := _get_any_index(L, index)^
    t := value_type(v)
    switch t {
    case .Light_Userdata: return value_get_pointer(v), true
    case .Table:    return value_get_table(v), true
    case .Function: return value_get_function(v), true
    case .Nil, .Boolean, .Number, .String, .Chunk:
        break
    }
    return nil, false
}

to_userdata :: proc(L: ^State, index: int) -> (p: rawptr, ok: bool) #optional_ok {
    v := _get_any_index(L, index)^
    t := value_type(v)
    #partial switch t {
    case .Light_Userdata: return value_get_pointer(v), true
    case:
        break
    }
    return nil, false
}

to_string :: proc(L: ^State, index: int) -> (s: string, ok: bool) #optional_ok {
    v := _get_any_index(L, index)
    if value_is_string(v^) {
        s = value_get_string(v^)
        return s, true
    } else if value_is_number(v^) {
        buf: [VALUE_TO_STRING_BUFFER_SIZE]byte
        repr := number_to_string(value_to_number(v^), buf[:])
        obj := ostring_new(L, repr)
        v^   = value_make_ostring(obj)
        return ostring_to_string(obj), true
    }
    return "", false
}

/*
Push `R[index]` to the top of the stack.

**Parameters**
- index: Either an acceptable index or a pseudo-index.

**Side-effects**
- push: 1
- pop:  0
 */
push_value :: proc(L: ^State, index: int) {
    v := _get_any_index(L, index)^
    vm_push_value(L, v)
}


/*
Moves `R[-1]` to `R[index]`, also moving `R[index:]` to `R[index + 1:]` to
account for this change.
 */
insert :: proc(L: ^State, index: int) {
    index := index - 1 if index > 0 else get_top(L) + index

    // Copy by value as its stack slot is about to replaced.
    value := L.registers[get_top(L) - 1]

    // `len(src)` != `dst(src)` is acceptable.
    src   := L.registers[index:]
    dst   := L.registers[index + 1:]
    copy(dst, src)
    L.registers[index] = value
}

push_nil :: proc(L: ^State, count := 1) {
    for i in 1..=count {
        vm_push_value(L, value_make_nil())
    }
}

push_boolean :: proc(L: ^State, b: bool) {
    v := value_make_boolean(b)
    vm_push_value(L, v)
}

push_number :: proc(L: ^State, n: f64) {
    v := value_make_number(n)
    vm_push_value(L, v)
}

push_lightuserdata :: proc(L: ^State, p: rawptr) {
    v := value_make_lightuserdata(p)
    vm_push_value(L, v)
}

/*
**Side-effects**
- push: 1
- pop: `upvalue_count`
 */
push_api_proc :: proc(L: ^State, p: Api_Proc, upvalue_count: u8 = 0) {
    cl   := closure_api_new(L, p, upvalue_count)
    top  := get_top(L)
    base := top - int(upvalue_count)
    #no_bounds_check {
        src := L.registers[base:top]
        dst := cl.api.upvalues[:upvalue_count]
        copy(dst, src)
    }
    vm_pop_value(L,  int(upvalue_count))
    vm_push_value(L, value_make_function(cl))
}

push_string :: proc(L: ^State, s: string) {
    v := value_make_ostring(ostring_new(L, s))
    vm_push_value(L, v)
}

push_fstring :: proc(L: ^State, format: string, args: ..any) -> string {
    return vm_push_fstring(L, format, ..args)
}

/*
Push `_G[key]` to the top of the stack.

**Side-effects**
- push: 1
- pop:  0
 */
get_global :: proc(L: ^State, name: string) {
    t := value_get_table(L.globals_table)
    k := value_make_ostring(ostring_new(L, name))
    v := table_get(t, k)
    vm_push_value(L, v)
}

/*
`_G[name] = R[-1]`, where `R[-1]` is the most recently pushed value.

**Side-effects**
- push: 0
- pop:  1
 */
set_global :: proc(L: ^State, name: string) {
    t := value_get_table(L.globals_table)
    k := value_make_ostring(ostring_new(L, name))
    v := table_set(L, t, k)
    v^ = _get_stack_index(L, -1)^
    vm_pop_value(L, 1)
}

load :: proc(L: ^State, name, input: string) -> Error {
    // Must be outside protected call to ensure that we can defer destroy.
    b, _ := strings.builder_make(allocator=L.global_state.backing_allocator)
    defer strings.builder_destroy(&b)

    Data :: struct {
        builder:    ^strings.Builder,
        name, input: string,
    }

    data := Data{&b, name, input}

    // Try to push the main function as a closure on the caller's VM stack.
    return run_raw_pcall(L, proc(L: ^State, user_data: rawptr) {
        data  := (cast(^Data)user_data)^
        name  := ostring_new(L, data.name)
        program(L, data.builder, name, data.input)
    }, &data)
}

call :: proc(L: ^State, arg_count, ret_count: int) {
    callee := _get_stack_index(L, -(arg_count + 1))
    run_call(L, callee, arg_count, ret_count)
}

pcall :: proc(L: ^State, arg_count, ret_count: int) -> Error {
    Data :: struct {
        // How many arguments the caller told us they passed.
        // This is assuming the API rules for calling functions were followed.
        arg_count: int,

        // How many return values the caller told us they are expecting.
        // This is used to determine how to restore the caller's VM stack.
        ret_count: int,
    }

    data := Data{arg_count, ret_count}

    return run_raw_pcall(L, proc(L: ^State, user_data: rawptr) {
        data   := (cast(^Data)user_data)^
        callee := _get_stack_index(L, -(data.arg_count + 1))
        run_call(L, callee, data.arg_count, data.ret_count)
    }, &data)
}

/*
Pushes the API procedure `p` and raw pointer `user_data` to the stack
and calls `p` with `user_data` as its sole argument.

**Returns**
- err: An API error code, if any, that was caught. May be `.Ok` (a.k.a. `nil`).
 */
api_pcall :: proc(L: ^State, p: Api_Proc, user_data: rawptr) -> (err: Error) {
    cl := closure_api_new(L, p, 0)
    vm_push_value(L, value_make_function(cl))
    vm_push_value(L, value_make_lightuserdata(user_data))
    return pcall(L, arg_count=1, ret_count=0)
}

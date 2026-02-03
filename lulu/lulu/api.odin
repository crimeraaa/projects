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

Reader_Proc :: #type proc(user_data: rawptr) -> (current: []byte)

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
        vm_pop(L, -index)
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

@(private="file")
_is :: proc(L: ^State, index: int, $procedure: proc(Value) -> bool) -> bool {
    v := _get_any_index(L, index)^
    return procedure(v)
}

is_none :: proc(L: ^State, index: int) -> bool {
    v := _get_any_index(L, index)
    return v == &_VALUE_NONE
}

is_none_or_nil :: proc(L: ^State, index: int) -> bool {
    v := _get_any_index(L, index)
    return v == &_VALUE_NONE || value_is_nil(v^)
}

is_nil      :: proc(L: ^State, index: int) -> bool { return _is(L, index, value_is_nil)      }
is_boolean  :: proc(L: ^State, index: int) -> bool { return _is(L, index, value_is_boolean)  }
is_number   :: proc(L: ^State, index: int) -> bool { return _is(L, index, value_is_number)   }
is_string   :: proc(L: ^State, index: int) -> bool { return _is(L, index, value_is_string)   }
is_table    :: proc(L: ^State, index: int) -> bool { return _is(L, index, value_is_table)    }
is_function :: proc(L: ^State, index: int) -> bool { return _is(L, index, value_is_function) }

to_boolean :: proc(L: ^State, index: int) -> (b: bool) {
    v := _get_any_index(L, index)^
    return !value_is_falsy(v)
}

to_number :: proc(L: ^State, index: int) -> (n: f64, ok: bool) #optional_ok {
    v := _get_any_index(L, index)^
    return value_to_number(v)
}

to_pointer :: proc(L: ^State, index: int) -> (p: rawptr, ok: bool) #optional_ok {
    v := _get_any_index(L, index)^
    t := value_type(v)
    switch t {
    case .Light_Userdata: return value_get_pointer(v),  true
    case .Table:          return value_get_table(v),    true
    case .Function:       return value_get_function(v), true
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
    #partial switch value_type(v^) {
    case .String:
        s = value_get_string(v^)
        return s, true
    case .Number:
        buf: [VALUE_TO_STRING_BUFFER_SIZE]byte
        repr := number_to_string(value_to_number(v^), buf[:])
        obj := ostring_new(L, repr)

        // Restore the pointer in case we reallocated the stack.
        // v = _get_any_index(L, index)
        v^ = value_make(obj)
        return ostring_to_string(obj), true
    case:
        break
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
    vm_push(L, v)
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
    for _ in 1..=count {
        vm_push(L, value_make())
    }
}

push :: proc {
    push_boolean,
    push_number,
    push_integer,
    push_lightuserdata,
    push_string,
    push_api_proc,
}

push_boolean :: proc(L: ^State, b: bool) {
    v := value_make(b)
    vm_push(L, v)
}

push_number :: proc(L: ^State, n: f64) {
    v := value_make(n)
    vm_push(L, v)
}

push_integer :: proc(L: ^State, i: int) {
    push(L, f64(i))
}

push_lightuserdata :: proc(L: ^State, p: rawptr) {
    v := value_make(p)
    vm_push(L, v)
}

/*
**Side-effects**
- push: 1
- pop: `upvalue_count`
 */
push_api_proc :: proc(L: ^State, procedure: Api_Proc, upvalue_count: u8 = 0) {
    cl   := closure_api_new(L, procedure, upvalue_count)
    top  := get_top(L)
    base := top - int(upvalue_count)
    #no_bounds_check {
        src := L.registers[base:top]
        dst := cl.api.upvalues[:upvalue_count]
        copy(dst, src)
    }
    vm_pop(L,  int(upvalue_count))
    vm_push(L, cl)
}

push_string :: proc(L: ^State, s: string) {
    v := value_make(ostring_new(L, s))
    vm_push(L, v)
}

push_fstring :: proc(L: ^State, format: string, args: ..any) -> string {
    return vm_push_fstring(L, format, ..args)
}

/*
Concatenates the top `count` values.

**Side-effects**
- push: 1 if `count > 1` else 0
- pop: `count - 1` if `count >= 2` else 0
*/
concat :: proc(L: ^State, count: int) {
    switch count {
    case 0: push(L, ""); return
    case 1: return
    case:
        break
    }
    assert(count >= 2)
    args := L.registers[get_top(L) - count:]
    vm_concat(L, &args[0], args)
    pop(L, count - 1)
}

/*
Pushes a new table onto the stack with `hash_count` and `array_count` elements
reserved.
 */
new_table :: proc(L: ^State, hash_count := 0, array_count := 0) {
    t := table_new(L, hash_count, array_count)
    vm_push(L, value_make(t))
}

/*
Push `_G[key]` to the top of the stack.

**Side-effects**
- push: 1
- pop:  0
 */
get_global :: proc(L: ^State, name: string) {
    t := &L.globals_table
    k := value_make(ostring_new(L, name))
    v := vm_get_table(L, t, k)
    vm_push(L, v)
}

/*
Push `R[table][R[key]]` to the top of the stack.

**Side-effects**
- push: 1
- pop:  0
 */
get_table :: proc(L: ^State, table, key: int) {
    t := _get_any_index(L, table)
    k := _get_stack_index(L, key)^
    v := vm_get_table(L, t, k)
    vm_push(L, v)
}

/*
`_G[name] = R[-1]`, where `R[-1]` is the most recently pushed value.

**Side-effects**
- push: 0
- pop:  1
 */
set_global :: proc(L: ^State, name: string) {
    t := &L.globals_table
    k := value_make(ostring_new(L, name))
    v := _get_stack_index(L, -1)^

    // Prevent key from being collected.
    vm_push(L, k)
    vm_set_table(L, t, k, v)

    // Pop both key and value.
    vm_pop(L, 2)
}

/*
`R[table][R[key]] := R[-1]` where `R[-1]` is the most recently pushed value.

**Side-effects**
- push: 0
- pop:  1
 */
set_table :: proc(L: ^State, table, key: int) {
    t := _get_any_index(L, table)
    k := _get_stack_index(L, key)^
    v := _get_stack_index(L, -1)^

    vm_set_table(L, t, k, v)

    // Pop only the value.
    vm_pop(L, 1)
}

/*
`R[table][field] := R[-1]` where `R[-1]` is the most recently pushed value.

**Side-effects**
- push: 0
- pop:  1
 */
set_field :: proc(L: ^State, table: int, field: string) {
    t := _get_any_index(L, table)
    k := value_make(ostring_new(L, field))
    v := _get_stack_index(L, -1)^

    // Prevent key from being collected.
    vm_push(L, k)
    vm_set_table(L, t, k, v)

    // Pop both key and value.
    vm_pop(L, 2)
}

/*
**Parameters**
- allocator: Backing allocator for the token stream. You can supply an allocator
with a fixed-size buffer to clamp the maximum length of tokens.
 */
load :: proc(L: ^State, name: string, reader_proc: Reader_Proc, reader_data: rawptr, allocator: mem.Allocator) -> Error {
    // Must be outside protected call to ensure that we can defer destroy.
    b, _ := strings.builder_make(allocator=allocator)
    defer strings.builder_destroy(&b)

    Data :: struct {
        builder: ^strings.Builder,
        name:    string,
        input:   Reader,
    }

    data := Data{&b, name, reader_make(reader_proc, reader_data)}

    // Try to push the main function as a closure on the caller's VM stack.
    return run_raw_pcall(L, proc(L: ^State, user_data: rawptr) {
        data  := cast(^Data)user_data
        name  := ostring_new(L, data.name)
        program(L, data.builder, name, data.input)
    }, &data)
}

/*
**Side-effects**
- push: ret_count
- pop:  arg_count + 1
 */
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
    vm_push(L, cl)
    vm_push(L, value_make(user_data))
    return pcall(L, arg_count=1, ret_count=0)
}

/*
Pushes a string identifying file name and current line/column pair of the
`level`th calling function, starting from the top.

**Parameters**
- level: `0` indicates the current calling function, `1` indicates the function
that called it, etc.
 */
location :: proc(L: ^State, level: int) {
    index := (L.frame_count - 1) - level
    if index >= 0 {
        frame := L.frames[index]
        cl := value_get_function(frame.callee^)
        if cl.is_lua {
            file := chunk_name(cl.lua.chunk)
            loc  := cl.lua.chunk.loc[frame.saved_pc]
            line := int(loc.line)
            col  := int(loc.col)
            push_fstring(L, "%s:%i:%i: ", file, line, col)
        } else {
            push(L, "[Odin]: ")
        }
    } else {
        push(L, "[?]: ")
    }
}

/*
Throws a runtime error, returning immediately to the first protected caller.
 */
error :: proc(L: ^State) -> ! {
    throw_error(L, .Runtime)
}


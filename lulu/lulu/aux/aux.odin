package lulu_aux

// local
import lulu ".."

type_name_at :: proc(L: ^lulu.State, index: int) -> string {
    t := lulu.type(L, index)
    return lulu.type_name(L, t)
}

Library_Entry :: struct {
    name:      string,
    procedure: lulu.Api_Proc,
}

/*
Pushes a new library table, setting each key-value pair to the ones in
`library`.

**Side-effects**
- push: 1
- pop:  0
 */
new_library :: proc(L: ^lulu.State, library: []Library_Entry) {
    lulu.new_table(L, hash_count=len(library))
    set_library(L, library)
}

/*
Sets each field in the library table at `R[-1]` to some associated procedure.

**Side-effects**
- push: 0
- pop:  0
 */
set_library :: proc(L: ^lulu.State, library: []Library_Entry) {
    for field in library {
        lulu.push_api_proc(L, field.procedure)
        lulu.set_field(L, -2, field.name)
    }
}

/*
Pushes an error message and throws a runtime error, returning immediately to
the first protected caller.
 */
errorf :: proc(L: ^lulu.State, format: string, args: ..any) -> ! {
    lulu.location(L, 1)
    lulu.push_fstring(L, format, ..args)
    lulu.concat(L, 2)
    lulu.error(L)
}

type_error :: proc(L: ^lulu.State, index: int, expected: lulu.Type) -> ! {
    type_name_error(L, index, lulu.type_name(L, expected))
}

type_name_error :: proc(L: ^lulu.State, index: int, expected: string) -> ! {
    actual   := type_name_at(L, index)
    message  := lulu.push_fstring(L, "%s expected, got %s", expected, actual)
    arg_error(L, index, message)
}

arg_error :: proc(L: ^lulu.State, index: int, message: string) -> ! {
    errorf(L, "bad argument #%i (%s)", index, message)
}

check_any :: proc(L: ^lulu.State, index: int) {
    if lulu.is_none(L, index) {
        arg_error(L, index, "value expected")
    }
}

check_number :: proc(L: ^lulu.State, index: int) -> f64 {
    n, ok := lulu.to_number(L, index)
    if !ok {
        type_error(L, index, .Number)
    }
    return n
}

check_string :: proc(L: ^lulu.State, index: int) -> string {
    s, ok := lulu.to_string(L, index)
    if !ok {
        type_error(L, index, .String)
    }
    return s
}

opt_number :: proc(L: ^lulu.State, index: int, default: f64) -> f64 {
    if lulu.is_none_or_nil(L, index) {
        return default
    }
    return check_number(L, index)
}

opt_string :: proc(L: ^lulu.State, index: int, default: string) -> string {
    if lulu.is_none_or_nil(L, index) {
        return default
    }
    return check_string(L, index)
}

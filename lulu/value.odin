#+private package
package lulu

import "core:fmt"

Value :: struct {
    type: Value_Type,
    using _: struct #raw_union {
        boolean:  bool,
        number:   f64,
        pointer:  rawptr,
        object:  ^Object,
    },
}

Value_Type :: enum u8 {
    Nil, Boolean, Number,

    // Object Types (User-facing)
    String,

    // Object Types (Internal-use)
    Chunk,
}

@require_results
value_make :: proc {
    value_make_nil,
    value_make_boolean,
    value_make_number,
    value_make_ostring,
}

value_make_nil :: proc() -> Value {
    return Value{type=.Nil}
}

value_make_boolean :: proc(boolean: bool) -> Value {
    return Value{type=.Boolean, boolean=boolean}
}

value_make_number :: proc(number: f64) -> Value {
    return Value{type=.Number, number=number}
}

value_make_ostring :: proc(ostring: ^OString) -> Value {
    return Value{type=.String, object=cast(^Object)ostring}
}

value_eq :: proc(a, b: Value) -> bool {
    if a.type != b.type {
        return false
    }

    switch a.type {
    case .Nil:      return true
    case .Boolean:  return a.boolean == b.boolean
    case .Number:   return a.number == b.number
    case .String:   return a.object == b.object
    case .Chunk:
        break
    }
    unreachable("bruh")
}

value_print :: proc(v: Value, flush := false) {
    switch v.type {
    case .Nil:     fmt.print("nil",     flush=flush)
    case .Boolean: fmt.print(v.boolean, flush=flush)
    case .Number:  fmt.printf("%.14g", v.number,  flush=flush)
    case .String:
        s := ostring_to_string(&v.object.ostring)
        q := '\'' if len(s) == 1 else '\"'
        fmt.printf("%c%s%c", q, s, q, flush=flush)
    case .Chunk:
        unreachable("Invalid Value_Type: %v", v.type)
    }
}

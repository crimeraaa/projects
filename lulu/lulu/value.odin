#+private package
package lulu

import "core:fmt"

Value :: struct {
    type: Value_Type,
    using data: struct #raw_union {
        boolean:  bool,
        number:   f64,
        integer:  int,
        object:  ^Object,
        pointer:  rawptr,
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

value_make_boolean :: proc(b: bool) -> Value {
    return Value{type=.Boolean, boolean=b}
}

value_make_number :: proc(n: f64) -> Value {
    return Value{type=.Number, number=n}
}

value_make_ostring :: proc(s: ^Ostring) -> Value {
    return Value{type=.String, object=cast(^Object)s}
}

value_type :: proc(v: Value) -> Value_Type {
    return v.type
}

value_type_name :: proc(v: Value) -> string {
    t := value_type(v)
    return value_type_string(t)
}

value_type_string :: proc(t: Value_Type) -> string {
    switch t {
    case .Nil:      return "nil"
    case .Boolean:  return "boolean"
    case .Number:   return "number"
    case .String:   return "string"
    case .Chunk:
        break
    }
    unreachable("Invalid type: %v", t)
}

value_is_nil :: proc(v: Value) -> bool {
    return value_type(v) == .Nil
}

value_is_boolean :: proc(v: Value) -> bool {
    return value_type(v) == .Boolean
}

value_is_number :: proc(v: Value) -> bool {
    return value_type(v) == .Number
}

value_is_string :: proc(v: Value) -> bool {
    return value_type(v) == .String
}

value_is_falsy :: proc(v: Value) -> bool {
    return value_is_nil(v) || (value_is_boolean(v) && !value_to_boolean(v))
}

value_to_boolean :: proc(v: Value) -> bool {
    assert(value_is_boolean(v), "Expected 'boolean' but got '%s'", value_type_name(v))
    return v.boolean
}

value_to_number :: proc(v: Value) -> f64 {
    assert(value_is_number(v), "Expected 'boolean' but got '%s'", value_type_name(v))
    return v.number
}

value_to_ostring :: proc(v: Value) -> ^Ostring {
    assert(value_is_string(v), "Expected 'string' but got '%s'", value_type_name(v))
    return &v.object.ostring
}

value_to_string :: proc(v: Value) -> string {
    s := value_to_ostring(v)
    return ostring_to_string(s)
}

value_eq :: proc(a, b: Value) -> bool {
    if a.type != b.type {
        return false
    }

    switch a.type {
    case .Nil:      return true
    case .Boolean:  return value_to_boolean(a) == value_to_boolean(b)
    case .Number:   return value_to_number(a)  == value_to_number(b)
    case .String:   return value_to_ostring(a) == value_to_ostring(b)
    case .Chunk:
        break
    }
    unreachable("Invalid type: %v", a.type)
}

value_print :: proc(v: Value, newline := false) {
    print  := fmt.println  if newline else fmt.print
    printf := fmt.printfln if newline else fmt.printf

    switch v.type {
    case .Nil:     print("nil")
    case .Boolean: print(value_to_boolean(v))
    case .Number:  printf("%.14g", value_to_number(v))
    case .String:
        s := value_to_string(v)
        q := '\'' if len(s) == 1 else '\"'
        printf("%c%s%c", q, s, q)
    case .Chunk:
        unreachable("Invalid type: %v", v.type)
    }
}

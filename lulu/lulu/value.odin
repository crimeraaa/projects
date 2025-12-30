#+private package
package lulu

import "core:fmt"

Value :: struct {
    using data: Value_Data,
    type: Value_Type,
}

Value_Type :: enum u8 {
    Nil, Boolean, Number,

    // Object Types (User-facing)
    String, Table,

    // Object Types (Internal-use)
    Chunk,
}

Value_Data :: struct #raw_union {
    boolean:  bool,
    number:   f64,
    integer:  int,
    object:  ^Object,
    pointer:  rawptr,
}

@require_results
value_make :: proc {
    value_make_nil,
    value_make_boolean,
    value_make_number,
    value_make_ostring,
    value_make_table,
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

value_make_table :: proc(t: ^Table) -> Value {
    return Value{type=.Table, object=cast(^Object)t}
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
    case .Table:    return "table"
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

@(private="file")
__check_type :: proc(v: Value, t: Value_Type) {
    assert(value_type(v) == t, "Expected '%s' but got '%s'",
        value_type_string(t), value_type_name(v))
}

value_to_boolean :: proc(v: Value) -> bool {
    __check_type(v, .Boolean)
    return v.boolean
}

value_to_number :: proc(v: Value) -> f64 {
    __check_type(v, .Number)
    return v.number
}

value_to_object :: proc(v: Value) -> ^Object {
    return v.object
}

value_to_ostring :: proc(v: Value) -> ^Ostring {
    __check_type(v, .String)
    return &v.object.ostring
}

value_to_table :: proc(v: Value) -> ^Table {
    __check_type(v, .Table)
    return &v.object.table
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
    case .Table:    return value_to_table(a) == value_to_table(b)
    case .Chunk:
        break
    }
    unreachable("Invalid type: %v", a.type)
}

value_print :: proc(v: Value) {
    switch v.type {
    case .Nil:     fmt.print("nil")
    case .Boolean: fmt.print(value_to_boolean(v))
    case .Number:  fmt.printf("%.14g", value_to_number(v))
    case .String:
        s := value_to_string(v)
        q := '\'' if len(s) == 1 else '\"'
        fmt.printf("%c%s%c", q, s, q)
    case .Table:
        t := value_to_table(v)
        fmt.printf("table: %p", t)
    case .Chunk:
        unreachable("Invalid type: %v", v.type)
    }
}

value_println :: proc(v: Value) {
    value_print(v)
    fmt.println()
}

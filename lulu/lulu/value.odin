#+private package
package lulu

import "core:fmt"
import "core:math"
import "core:strconv"

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
    pointer:  rawptr,
    object:  ^Object,
}

// === VALUE DATA PAYLOADS ================================================= {{{
// The following can be changed if, for example, you opt to use NaN tagging
// and/or fat pointers instead.


@require_results
value_make :: proc {
    value_make_nil,
    value_make_boolean,
    value_make_number,
    value_make_ostring,
    value_make_table,
}

value_make_nil :: #force_inline proc "contextless" () -> Value {
    v: Value
    v.pointer = nil
    v.type    = .Nil
    return v
}

value_make_boolean :: #force_inline proc "contextless" (b: bool) -> Value {
    v: Value
    v.boolean = b
    v.type    = .Boolean
    return v
}

value_make_number :: #force_inline proc "contextless" (n: f64) -> Value {
    v: Value
    v.number = n
    v.type   = .Number
    return v
}

value_make_object :: proc(o: ^Object, t: Value_Type) -> Value {
    v: Value
    // Check for consistency.
    assert(o.type == t, "Expected '%s' but got '%s'", value_type_string(t), value_type_string(o.type))
    v.object = o
    v.type   = t
    return v
}

value_type :: #force_inline proc "contextless" (v: Value) -> Value_Type {
    return v.type
}

@(private="file")
check_type :: #force_inline proc(v: Value, t: Value_Type) {
    assert(value_type(v) == t, "Expected '%s' but got '%s'",
        value_type_string(t), value_type_name(v))
}

value_get_bool :: #force_inline proc(v: Value) -> (b: bool) {
    check_type(v, .Boolean)
    return v.boolean
}

value_get_number :: #force_inline proc(v: Value) -> (f: f64) {
    check_type(v, .Number)
    return v.number
}

value_get_pointer :: #force_inline proc(v: Value) -> (p: rawptr) {
    return v.pointer
}

value_get_object :: #force_inline proc(v: Value) -> (o: ^Object) {
    o = v.object
    // Ensure consistency.
    check_type(v, o.type)
    return o
}

// === }}} =====================================================================
// Everything below is implemented in terms of the data payload functions.


number_unm :: proc "contextless" (a: f64)    -> f64 {return -a}
number_add :: proc "contextless" (a, b: f64) -> f64 {return a + b}
number_sub :: proc "contextless" (a, b: f64) -> f64 {return a - b}
number_mul :: proc "contextless" (a, b: f64) -> f64 {return a * b}
number_div :: proc "contextless" (a, b: f64) -> f64 {return a / b}
number_mod :: proc "contextless" (a, b: f64) -> f64 {q := a / b; return a - math.floor(q) * b}
number_pow :: math.pow_f64


/* 
Write the number `n` into the caller-supplied byte buffer `buf`.
 */
number_to_string :: proc(n: f64, buf: []byte) -> string {
    return fmt.bprintf(buf, "%.14g", n)
}

value_make_ostring :: proc(s: ^Ostring) -> Value {
    return value_make_object(cast(^Object)s, .String)
}

value_make_table :: proc(t: ^Table) -> Value {
    return value_make_object(cast(^Object)t, .Table)
}

value_make_chunk :: proc(c: ^Chunk) -> Value {
    return value_make_object(cast(^Object)c, .Chunk)
}

value_type_name :: #force_inline proc(v: Value) -> string {
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

value_is_nil :: #force_inline proc(v: Value) -> bool {
    return value_type(v) == .Nil
}

value_is_boolean :: #force_inline proc(v: Value) -> bool {
    return value_type(v) == .Boolean
}

value_is_number :: #force_inline proc(v: Value) -> bool {
    return value_type(v) == .Number
}

value_is_string :: #force_inline proc(v: Value) -> bool {
    return value_type(v) == .String
}

value_is_falsy :: #force_inline proc(v: Value) -> bool {
    return value_is_nil(v) || (value_is_boolean(v) && !value_get_bool(v))
}

value_get_ostring :: #force_inline proc(v: Value) -> ^Ostring {
    check_type(v, .String)
    return &value_get_object(v).string
}

value_get_table :: #force_inline proc(v: Value) -> ^Table {
    check_type(v, .Table)
    return &value_get_object(v).table
}

value_get_string :: #force_inline proc(v: Value) -> string {
    s := value_get_ostring(v)
    return ostring_to_string(s)
}

value_eq :: proc(a, b: Value) -> bool {
    t := value_type(a)
    if t != value_type(b) {
        return false
    }

    switch t {
    case .Nil:      return true
    case .Boolean:  return value_get_bool(a)    == value_get_bool(b)
    case .Number:   return value_get_number(a)  == value_get_number(b)
    case .String:   return value_get_ostring(a) == value_get_ostring(b)
    case .Table:    return value_get_table(a)   == value_get_table(b)
    case .Chunk:
        break
    }
    unreachable("Invalid value to compare: %v", t)
}

// We assume this is large enough to hold all representations of non-string
// values.
VALUE_TO_STRING_BUFFER_SIZE :: 64

/* 
Gets the string representation of `v`, writing it into `buf` only if needed.

**Parameters**
- buf: Used only when string representation is not immediately available
(e.g. `nil` and `boolean` can map to string literals, `string` already has its
data). `number` and non-string object types (e.g. `table`) must be written out
explicitly. We assume that `VALUE_TO_STRING_BUFFER_SIZE` is sufficient.
 */
value_to_string :: proc(v: Value, buf: []byte) -> string {
    t := value_type(v)
    switch t {
    case .Nil:     return "nil"
    case .Boolean: return "true" if value_get_bool(v) else "false"
    case .Number:  return number_to_string(value_get_number(v), buf)
    case .String:  return value_get_string(v)
    case .Table:
        s := value_type_string(t)
        p := cast(u64)cast(uintptr)value_get_pointer(v)
        return fmt.bprintf(buf, "%s: %p", s, p)
    case .Chunk:
        break
    }
    unreachable("Invalid value to write: %v", t)
}

/* 
Get the number representation of `v`. Numbers are returned as-is and strings
are parsed. No other types can have a number representation.
 */
value_to_number :: proc(v: Value) -> (n: f64, ok: bool) {
    if value_is_number(v) {
        return value_get_number(v), true
    } else if !value_is_string(v) {
        return 0, false
    }

    s := value_get_string(v)
    // Maybe an integer?
    try: if len(s) > 2 && s[0] == '0' {
        base := 0
        switch s[1] {
        case 'b', 'B': base = 2
        case 'd', 'D': base = 10
        case 'o', 'O': base = 8
        case 'x', 'X': base = 16
        case 'z', 'Z': base = 12
        case:
            break try
        }

        i: uint
        i, ok = strconv.parse_uint(s[2:], base)
        if ok {
            n = cast(f64)i
        }
        return n, ok
    }
    return strconv.parse_f64(s)
}

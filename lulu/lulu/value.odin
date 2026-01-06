#+private package
package lulu

import "core:fmt"
import "core:math"
import "core:strconv"

Value :: struct {
    using data: Value_Data,
    type: Value_Type,
}

/*
**NOTE(2025-01-05)**
- ORDER: This must match `api.odin:Type`!
 */
Value_Type :: enum u8 {
    // Non-collectible Types
    Nil, Boolean, Number, Light_Userdata,

    // Collectible Types (Object, user-facing)
    String, Table, Function,

    // Collectible Types (Object, internal)
    Chunk,
}

Value_Data :: struct #raw_union {
    boolean: bool,
    number:  f64,
    pointer: rawptr,
    object: ^Object,
}

// === VALUE DATA PAYLOADS ================================================= {{{
// The following can be changed if, for example, you opt to use NaN tagging
// and/or fat pointers instead.


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

value_make_lightuserdata :: #force_inline proc "contextless" (p: rawptr) -> Value {
    v: Value
    v.pointer = p
    v.type    = .Light_Userdata
    return v
}

value_make_object :: proc(o: ^Object, t: Value_Type) -> Value {
    v: Value
    v.object = o
    v.type   = t
    return v
}

value_type :: #force_inline proc "contextless" (v: Value) -> Value_Type {
    return v.type
}

@(private="file")
_check_type :: #force_inline proc(v: Value, t: Value_Type) {
    assert(value_type(v) == t, "Expected '%s' but got '%s'",
        value_type_string(t), value_type_name(v))
}

value_get_bool :: #force_inline proc(v: Value) -> (b: bool) {
    _check_type(v, .Boolean)
    return v.boolean
}

value_get_number :: #force_inline proc(v: Value) -> (f: f64) {
    _check_type(v, .Number)
    return v.number
}

value_get_pointer :: #force_inline proc(v: Value) -> (p: rawptr) {
    return v.pointer
}

value_get_object :: #force_inline proc(v: Value) -> (o: ^Object) {
    o = v.object
    // Ensure consistency.
    _check_type(v, o.type)
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
Parses the string `s` into a number.

**Returns**
- n: The parsed value or `0.0` if failure occured.
- ok: `true` if parsing was succesful else `false`.
 */
number_from_string :: proc(s: string) -> (n: f64, ok: bool) {
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
            n = f64(i)
        }
        return n, ok
    }
    return strconv.parse_f64(s)
}

/*
Write the number `n` into the caller-supplied byte buffer `buf`.

**Returns**
- s: The contents of `buf` that were written.
 */
number_to_string :: proc(n: f64, buf: []byte) -> (s: string) {
    return fmt.bprintf(buf, "%.14g", n)
}

pointer_to_string :: proc(p: any, buf: []byte) -> (s: string) {
    return fmt.bprintf(buf, "%p", p)
}

value_make_ostring :: proc(s: ^Ostring) -> Value {
    return value_make_object(cast(^Object)s, .String)
}

value_make_table :: proc(t: ^Table) -> Value {
    return value_make_object(cast(^Object)t, .Table)
}

value_make_function :: proc(cl: ^Closure) -> Value {
    return value_make_object(cast(^Object)cl, .Function)
}

value_type_name :: #force_inline proc(v: Value) -> string {
    t := value_type(v)
    return value_type_string(t)
}

value_type_string :: proc(t: Value_Type, loc := #caller_location) -> string {
    switch t {
    case .Nil:              return "nil"
    case .Boolean:          return "boolean"
    case .Number:           return "number"
    case .Light_Userdata:   return "lightuserdata"
    case .String:           return "string"
    case .Table:            return "table"
    case .Function:         return "function"
    case .Chunk:
        break
    }
    unreachable("Invalid value to get type name of: %v", t, loc=loc)
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

value_is_function :: #force_inline proc(v: Value) -> bool {
    return value_type(v) == .Function
}

value_is_falsy :: #force_inline proc(v: Value) -> bool {
    return value_is_nil(v) || (value_is_boolean(v) && !value_get_bool(v))
}

value_get_ostring :: #force_inline proc(v: Value) -> ^Ostring {
    _check_type(v, .String)
    return &value_get_object(v).string
}

value_get_function :: #force_inline proc(v: Value) -> ^Closure {
    _check_type(v, .Function)
    return &value_get_object(v).closure
}

value_get_table :: #force_inline proc(v: Value) -> ^Table {
    _check_type(v, .Table)
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
    case .Light_Userdata, .String, .Table, .Function:
        return value_get_pointer(a) == value_get_pointer(b)

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
    case .Light_Userdata, .Table, .Function:
        s := value_type_string(t)
        p := value_get_pointer(v)
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
value_to_number :: proc(v: Value) -> (n: f64, ok: bool) #optional_ok {
    if value_is_number(v) {
        return value_get_number(v), true
    } else if !value_is_string(v) {
        return 0, false
    }
    s := value_get_string(v)
    return number_from_string(s)
}

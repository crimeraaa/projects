#+private package
package luna

Expr :: struct {
    type: Expr_Type,
    using data: Expr_Data,
}


Expr_Type :: enum u8 {
    None,

    // Literals
    Nil, True, False, Number, Integer,

    // Needs a register? Use `Expr.pc` to set `R[A]`.
    Pending,

    // Finalized. Use `Expr.reg` to get `R[A]`.
    Register,
}

Expr_Data :: struct #raw_union {
    number:  f64,
    integer: int,
    pc:      i32,
    reg:     u16,
}

Expr_Op :: enum u8 {
    // Default type
    None,

    // Comparison
    Eq, Neq, Lt, Leq, Gt, Geq,

    // Arithmetic
    Unm, Add, Sub, Mul, Div, Mod,

    // Bitwise
    Bnot, Band, Bor, Bxor,

    // Misc.
    Not,
    Len,
}

expr_is_numerical :: proc(expr: ^Expr) -> bool {
    return expr_is_number(expr) || expr_is_integer(expr)
}

expr_is_number :: proc(expr: ^Expr) -> bool {
    return expr.type == .Number
}

expr_is_integer :: proc(expr: ^Expr) -> bool {
    return expr.type == .Integer
}

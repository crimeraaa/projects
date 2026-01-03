#+private package
package lulu

Expr :: struct {
    type: Expr_Type,
    using data: struct #raw_union {
        // Index of instruction in the current chunk's code array.
        pc: int,

        // Register number.
        reg: u16,

        // Index of the value in the current chunk's constants array.
        // Must fit in SIZE_Bx bits.
        index: u32,

        // Number literal. Useful for constant folding.
        number: f64,

        // Boolean literal.
        boolean: bool,
    }
}

Expr_Type :: enum {
    // Value literals.
    Nil, Boolean, Number,

    // Expression is the index of a constant value as found in the current
    // chunk's constants array? See `Expr.index`.
    Constant,

    // Expression is the name of a global variable. The index of the interned
    // string for the name can be found in `Expr.index`.
    Global,

    // Expression is the name of a local variable. The register of the local
    // can be found in `Expr.reg`.
    Local,

    // Expression is an instruction which needs its destination register
    // (always Register A) to be finalized? See `Expr.pc`.
    Pc_Pending_Register,

    // Expression is 'discharged', i.e. it is now stored in a register?
    // See `Expr.reg`.
    Register,
}

expr_make_reg :: #force_inline proc(t: Expr_Type, reg: u16) -> (e: Expr) {
    e.type = t
    e.reg  = reg
    return e
}

expr_make_pc :: #force_inline proc(t: Expr_Type, pc: int) -> (e: Expr) {
    e.type = t
    e.pc   = pc
    return e
}

expr_make_index :: #force_inline proc(t: Expr_Type, index: u32) -> (e: Expr) {
    e.type  = t
    e.index = index
    return e
}

expr_make_nil :: #force_inline proc() -> (e: Expr) {
    e.type = .Nil
    return e
}

expr_make_boolean :: #force_inline proc(b: bool) -> (e: Expr) {
    e.type    = .Boolean
    e.boolean = b
    return e
}

expr_make_number :: #force_inline proc(n: f64) -> (e: Expr) {
    e.type   = .Number
    e.number = n
    return e
}

expr_is_number :: #force_inline proc(e: ^Expr) -> bool {
    return e.type == .Number
}

expr_is_literal :: #force_inline proc(e: ^Expr) -> bool {
    LITERAL_TYPES :: bit_set[Expr_Type]{.Nil, .Boolean, .Number, .Constant}
    return e.type in LITERAL_TYPES
}

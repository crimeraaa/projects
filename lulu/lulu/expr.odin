#+private package
package lulu

Expr :: struct {
    type: Expr_Type,
    using data: struct #raw_union {
        // Number literal. Useful for constant folding.
        number: f64,

        // Index of instruction in the current chunk's code array.
        pc: int,

        // Index of the value in the current chunk's constants array.
        // Must fit in SIZE_Bx bits.
        index: u32,

        // Register number.
        reg: u16,

        boolean: bool,
    }
}

Expr_Type :: enum u8 {
    // Value literals.
    Nil, Boolean, Number,

    // Expression is the index of a constant value as found in the current
    // chunk's constants array? See `Expr.index`.
    Constant,
    
    // Expression is the name of a global variable. The index of the interned
    // string for the name can be found in `Expr.index`.
    Global,

    // Expression is an instruction which needs its destination register
    // (always Register A) to be finalized? See `Expr.pc`.
    Pc_Pending_Register,

    // Expression is 'discharged', i.e. it is now stored in a register?
    // See `Expr.reg`.
    Register,
}

expr_set_reg :: proc(e: ^Expr, reg: u16) {
    e.type = .Register
    e.reg  = reg
}

expr_set_pc :: proc(e: ^Expr, pc: int) {
    e.type = .Pc_Pending_Register
    e.pc   = pc
}

expr_set_constant :: proc(e: ^Expr, index: u32) {
    e.type  = .Constant
    e.index = index
}

expr_set_global :: proc(e: ^Expr, index: u32) {
    e.type  = .Global
    e.index = index
}

expr_make_nil :: proc() -> (e: Expr) {
    e.type = .Nil
    return e
}

expr_make_boolean :: proc(boolean: bool) -> (e: Expr) {
    e.type    = .Boolean
    e.boolean = boolean
    return e
}

expr_make_number :: proc(number: f64) -> (e: Expr) {
    e.type   = .Number
    e.number = number
    return e
}

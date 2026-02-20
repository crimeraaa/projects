#+private package
package luna

Ast_Node :: union {
    // Expressions
    Literal, ^Unary, ^Binary,

    // Compile-time
    Register, Pending,
}

Ast_Op :: enum u8 {
    // Unary,
    Unm, Not, Len,

    // Arithmetic
    Add, Sub, Mul, Div, Mod,

    // Bitwise
    Bnot, Band, Bor, Bxor,

    // Comparison
    Eq, Neq, Lt, Gt, Leq, Geq,
}

Register :: struct {
    type: Value_Type,
    reg:  u16
}

Pending :: struct {
    type: Value_Type,
    pc:   i32,
}

Literal :: Value

Unary :: struct {
    op:  Ast_Op,
    arg: Ast_Node,
}

Binary :: struct {
    op: Ast_Op,
    left, right: Ast_Node,
}

ast_make :: proc {
    ast_make_unary,
    ast_make_binary,
}

ast_make_unary :: proc(p: ^Parser, op: Ast_Op, arg: Ast_Node) -> Ast_Node {
    unary := new(Unary)
    unary.op  = op
    unary.arg = arg
    p.nodes   = unary
    return unary
}

ast_make_binary :: proc(p: ^Parser, op: Ast_Op, left, right: Ast_Node) -> Ast_Node {
    binary := new(Binary)
    binary.op    = op
    binary.left  = left
    binary.right = right
    p.nodes      = binary
    return binary
}

ast_destroy :: proc(node: Ast_Node) {
    switch data in node {
    case Literal:
    case ^Unary:
        ast_destroy(data.arg)
        free(data)
    case ^Binary:
        ast_destroy(data.left)
        ast_destroy(data.right)
        free(data)
    case Register:
    case Pending:
    }
}

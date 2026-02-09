#+private package
package luna

REG_MAX :: 250

Compiler :: struct {
    // All compilers for the current file share the same parser.
    parser: ^Parser,
    chunk:  ^Chunk,

    pc: i32,
    free_reg: u16,
}

compiler_code_ABC :: proc(c: ^Compiler, op: Opcode, A, B, C: u16) -> (pc: i32) {
    pc = i32(len(c.chunk.code))
    append(&c.chunk.code, Instruction{base={op=op, A=A, B=B, C=C}})
    return
}

compiler_code_ABx :: proc(c: ^Compiler, op: Opcode, A: u16, Bx: u32) -> (pc: i32) {
    pc = i32(len(c.chunk.code))
    append(&c.chunk.code, Instruction{u={op=op, A=A, Bx=Bx}})
    return
}

compiler_code_AsBx :: proc(c: ^Compiler, op: Opcode, A: u16, sBx: i32) -> (pc: i32) {
    pc = i32(len(c.chunk.code))
    append(&c.chunk.code, Instruction{s={op=op, A=A, Bx=sBx}})
    return
}

compiler_code_unary :: proc(c: ^Compiler, op: Expr_Op, expr: ^Expr) {
    #partial switch op {
    case .Not, .Len:
        unimplemented()
    case .Unm:
        #partial switch expr.type {
        case .Integer:  expr.integer = -expr.integer
        case .Number:   expr.number  = -expr.number
        case:
            p := c.parser
            parser_error(p, p.consumed, "Cannot negate a non-number")
        }
    case .Bnot:
        if !expr_is_integer(expr) {
            p := c.parser
            parser_error(p, p.consumed, "Cannot bitwise-not a non-integer")
        }
        expr.integer = ~expr.integer
    }
}

compiler_push_expr_next :: proc(c: ^Compiler, expr: ^Expr) -> (reg: u16) {
    reg = compiler_reg_push(c, 1)
    discharge_to_reg(c, expr, reg)
    return reg
}

compiler_push_expr_any :: proc(c: ^Compiler, expr: ^Expr) -> (reg: u16) {
    if expr.type == .Register {
        return expr.reg
    }
    return compiler_push_expr_next(c, expr)
}

compiler_reg_push :: proc(c: ^Compiler, count: u16) -> (base: u16) {
    base = c.free_reg
    c.free_reg += count
    if c.free_reg > REG_MAX {
        p := c.parser
        parser_error(p, p.current, "Register overflow")
    }
    return base
}

@(private="file")
discharge_to_reg :: proc(c: ^Compiler, expr: ^Expr, reg: u16) {
    pc: i32
    switch expr.type {
    case .None: unreachable()
    case .Nil:      compiler_code_ABC(c, .Load_Nil,  reg, 0, 0)
    case .True:     compiler_code_ABC(c, .Load_Bool, reg, 1, 0)
    case .False:    compiler_code_ABC(c, .Load_Bool, reg, 0, 0)
    case .Number:
        index := chunk_add_constant(c.chunk, expr.number)
        compiler_code_ABx(c, .Load_Const, reg, index)

    case .Integer:
        index := chunk_add_constant(c.chunk, expr.integer)
        compiler_code_ABx(c, .Load_Const, reg, index)

    case .Pending:
        ip := &c.chunk.code[expr.pc]
        assert(OPCODE_INFO[ip.op].a)
        ip.A = reg
    case .Register:
        if reg != expr.reg {
            compiler_code_ABC(c, .Move, reg, expr.reg, 0)
        }
    }
    expr.type = .Register
    expr.reg  = reg
}

compiler_code_binary :: proc(c: ^Compiler, op: Expr_Op, #no_alias left, right: ^Expr) {
    act_op: Opcode
    #partial switch op {
    // number-number OR integer-integer
    case .Eq..=.Geq:
        unimplemented()

    case .Add..=.Div:
        if expr_is_integer(left) && expr_is_integer(right) {
            act_op = .Add_int + Opcode(op - .Add)
        } else if expr_is_number(left) && expr_is_number(right) {
            act_op = .Add_f64 + Opcode(op - .Add)
        } else {
            parser_error(c.parser, c.parser.current, "operands must both be numbers/integers")
        }

    // integer-integer
    case .Mod, .Band..=.Bxor:
        if !expr_is_integer(left) || !expr_is_integer(right) {
            parser_error(c.parser, c.parser.current, "operands must both be integers")
        }

        if op == .Mod {
            act_op = .Mod_int
        } else {
            act_op = .Band + Opcode(op - .Band)
        }
    }
    rb := compiler_push_expr_any(c, left)
    rc := compiler_push_expr_any(c, right)
    pc := compiler_code_ABC(c, act_op, 0, rb, rc)

    left.type = .Pending
    left.pc   = pc
}

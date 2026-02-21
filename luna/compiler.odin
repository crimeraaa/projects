#+private file
package luna

REG_MAX :: 250

@(private="package")
Compiler :: struct {
    // All compilers for the current file share the same parser.
    parser: ^Parser,
    chunk:  ^Chunk,
    // pc: i32,
    free_reg: u16,
}

@(private="package")
compile :: proc(c: ^Compiler, root: ^Ast_Node) {
    visit(c, root)
    code_ABC(c, .Return, 0, 1, 0)
    ast_destroy(root)
}

visit :: proc(c: ^Compiler, node: ^Ast_Node) {
    switch data in node {
    case Literal:   expr_push_next(c, node)
    case ^Unary:    unary(c, node, data)  // free(data)
    case ^Binary:   binary(c, node, data) // free(data)
    case Register, Pending:
        unreachable()
    }
}

code_ABC :: proc(c: ^Compiler, op: Opcode, A, B, C: u16) -> (pc: i32) {
    pc = i32(len(c.chunk.code))
    append(&c.chunk.code, Instruction{base={op=op, A=A, B=B, C=C}})
    return
}

code_ABx :: proc(c: ^Compiler, op: Opcode, A: u16, Bx: u32) -> (pc: i32) {
    pc = i32(len(c.chunk.code))
    append(&c.chunk.code, Instruction{u={op=op, A=A, Bx=Bx}})
    return
}

code_AsBx :: proc(c: ^Compiler, op: Opcode, A: u16, sBx: i32) -> (pc: i32) {
    pc = i32(len(c.chunk.code))
    append(&c.chunk.code, Instruction{s={op=op, A=A, Bx=sBx}})
    return
}

expr_push_next :: proc(c: ^Compiler, expr: ^Ast_Node) -> (reg: u16) {
    reg = reg_push(c)
    discharge_to_reg(c, expr, reg)
    return reg
}

expr_push_any :: proc(c: ^Compiler, expr: ^Ast_Node) -> (reg: u16) {
    if r, ok := expr.(Register); ok {
        return r.reg
    }
    return expr_push_next(c, expr)
}

reg_push :: proc(c: ^Compiler, count: u16 = 1) -> (base: u16) {
    base = c.free_reg
    c.free_reg += count
    if c.free_reg > REG_MAX {
        p := c.parser
        parser_error(p, p.current, "Register overflow")
    }
    return base
}

reg_pop :: proc(c: ^Compiler, reg: u16, count: u16 = 1) {
    prev := c.free_reg - count
    assert(prev == reg)
    c.free_reg = prev
}

discharge_to_reg :: proc(c: ^Compiler, expr: ^Ast_Node, reg: u16) {
    type: Value_Type
    switch node in expr {
    case Literal:
        switch value in node {
        case bool:
            type = .Boolean
            code_ABC(c, .Load_Bool, reg, u16(value), 0)

        case int:
            type = .Integer
            index := chunk_add_constant(c.chunk, value)
            code_ABx(c, .Load_Const, reg, index)

        case f64:
            type = .Number
            index := chunk_add_constant(c.chunk, value)
            code_ABx(c, .Load_Const, reg, index)
        }

    case ^Unary, ^Binary:
        unreachable()

    case Register:
        type = node.type
        if node.reg != reg {
            code_ABC(c, .Move, reg, node.reg, 0)
        }

    case Pending:
        type = node.type
        ip := &c.chunk.code[node.pc]
        ip.A = reg
    }
    expr^ = Register{type, reg}
}

unary :: proc(c: ^Compiler, node: ^Ast_Node, expr: ^Unary) {
    error :: proc(c: ^Compiler, $action, $object: string) -> ! {
        p := c.parser
        parser_error(p, p.consumed, "Cannot " + action + " a non-" + object)
    }

    error_unm :: proc(c: ^Compiler) -> ! {
        error(c, "negate", "(integer|number)")
    }

    visit(c, &expr.arg)
    #partial switch expr.op {
    case .Not, .Len:
        unimplemented()

    case .Unm:
        switch arg in expr.arg {
        case Literal:
            #partial switch value in arg {
            case int: expr.arg = Literal(-value); return
            case f64: expr.arg = Literal(-value); return
            case:
                error_unm(c)
            }

        case ^Unary, ^Binary, Pending: unreachable()
        case Register:
            pc: i32
            #partial switch arg.type {
            case .Integer: pc = code_ABC(c, .Unm_int, 0, arg.reg, 0)
            case .Number:  pc = code_ABC(c, .Unm_f64, 0, arg.reg, 0)
            case:
                error_unm(c)
            }
            node^ = Pending{arg.type, pc}
        }

    case .Bnot:
        if lit, ok := &expr.arg.(Literal); ok {
            if i, ok2 := &lit.(int); ok2 {
                i^ = ~i^
                return
            }
        }
        p := c.parser
        parser_error(p, p.consumed, "Cannot bitwise-not a non-integer")
    }
    expr_push_any(c, node)
}

binary :: proc(c: ^Compiler, node: ^Ast_Node, expr: ^Binary) {
    visit(c, &expr.left)
    visit(c, &expr.right)

    type: Value_Type
    act_op: Opcode
    #partial switch expr.op {
    // number-number OR integer-integer
    case .Eq..=.Geq:
        unimplemented()

    case .Add..=.Div:
        if ast_is_integer(&expr.left) && ast_is_integer(&expr.right) {
            type   = .Integer
            act_op = .Add_int + Opcode(expr.op - .Add)
        } else if ast_is_number(&expr.left) && ast_is_number(&expr.right) {
            type   = .Number
            act_op = .Add_f64 + Opcode(expr.op - .Add)
        } else {
            parser_error(c.parser, c.parser.current, "operands must both be numbers/integers")
        }

    // integer-integer
    case .Mod, .Band..=.Bxor:
        if !ast_is_integer(&expr.left) || !ast_is_integer(&expr.right) {
            parser_error(c.parser, c.parser.current, "operands must both be integers")
        }

        type = .Integer
        if expr.op == .Mod {
            act_op = .Mod_int
        } else {
            act_op = .Band + Opcode(expr.op - .Band)
        }
    }

    rb := expr_push_any(c, &expr.left)
    rc := expr_push_any(c, &expr.right)
    if rb > rc {
        reg_pop(c, rb)
        reg_pop(c, rc)
    } else {
        reg_pop(c, rc)
        reg_pop(c, rb)
    }

    pc := code_ABC(c, act_op, 0, rb, rc)
    node^ = Pending{type, pc}
    expr_push_any(c, node)
}

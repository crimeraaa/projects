#+private file
package luna

REG_MAX :: 250

@(private="package")
Compiler :: struct {
    // All compilers for the current file share the same parser.
    parser: ^Parser,
    chunk:  ^Chunk,
    // nodes:   Ast_Node,

    // pc: i32,
    free_reg: u16,
}

@(private="package")
compile :: proc(c: ^Compiler, root: ^Ast_Node) {
    visit(c, root)
    code_ABC(c, .Return, 0, 1, 0)
}

visit :: proc(c: ^Compiler, node: ^Ast_Node) {
    switch data in node {
    case Literal:
        expr_push_next(c, node)

    case ^Unary:
        visit(c, &data.arg)
        unary(c, data.op, &data.arg)
        free(data)

    case ^Binary:
        visit(c, &data.left)
        visit(c, &data.right)
        type, pc := binary(c, data.op, &data.left, &data.right)
        node^ = Pending{type, pc}
        free(data)
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

unary :: proc(c: ^Compiler, op: Ast_Op, expr: ^Ast_Node) {
    #partial switch op {
    case .Not, .Len:
        unimplemented()
    case .Unm:
        if lit, ok := &expr.(Literal); ok {
            #partial switch v in lit {
            case int: lit^ = -v; return
            case f64: lit^ = -v; return
            case:
                break
            }
        }
        p := c.parser
        parser_error(p, p.consumed, "Cannot negate a non-number")
    case .Bnot:
        if lit, ok := &expr.(Literal); ok {
            if i, ok2 := &lit.(int); ok2 {
                i^ = ~i^
                return
            }
        }
        p := c.parser
        parser_error(p, p.consumed, "Cannot bitwise-not a non-integer")
    }
}

expr_push_next :: proc(c: ^Compiler, expr: ^Ast_Node) -> (reg: u16) {
    reg = reg_push(c, 1)
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
            code_ABC(c, .Load_Bool, reg, u16(value), 0)
            type = .Boolean
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

binary :: proc(c: ^Compiler, op: Ast_Op, #no_alias left, right: ^Ast_Node) -> (type: Value_Type, pc: i32) {
    is_integer :: proc(expr: ^Ast_Node) -> bool {
        #partial switch node in expr {
        case Literal:  _, ok := node.(int); return ok
        case Register: return node.type == .Integer
        case Pending:  return node.type == .Integer
        }
        return false
    }

    is_number :: proc(expr: ^Ast_Node) -> bool {
        #partial switch node in expr {
        case Literal:  _, ok := node.(f64); return ok
        case Register: return node.type == .Number
        case Pending:  return node.type == .Number
        }
        return false
    }

    act_op: Opcode
    #partial switch op {
    // number-number OR integer-integer
    case .Eq..=.Geq:
        unimplemented()

    case .Add..=.Div:
        if is_integer(left) && is_integer(right) {
            type   = .Integer
            act_op = .Add_int + Opcode(op - .Add)
        } else if is_number(left) && is_number(right) {
            type   = .Number
            act_op = .Add_f64 + Opcode(op - .Add)
        } else {
            parser_error(c.parser, c.parser.current, "operands must both be numbers/integers")
        }

    // integer-integer
    case .Mod, .Band..=.Bxor:
        if !is_integer(left) || !is_integer(right) {
            parser_error(c.parser, c.parser.current, "operands must both be integers")
        }

        type = .Integer
        if op == .Mod {
            act_op = .Mod_int
        } else {
            act_op = .Band + Opcode(op - .Band)
        }
    }
    rb := expr_push_any(c, left)
    rc := expr_push_any(c, right)
    if rb > rc {
        reg_pop(c, rb)
        reg_pop(c, rc)
    } else {
        reg_pop(c, rc)
        reg_pop(c, rb)
    }
    pc = code_ABC(c, act_op, 0, rb, rc)
    return type, pc
}

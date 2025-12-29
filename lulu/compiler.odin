#+private package
package lulu

import "core:fmt"

Compiler :: struct {
    // Parent state.
    L: ^VM,

    // Sister state, mainly used for context during error handling.
    parser: ^Parser,

    // Not owned by us, but we are the ones filling in the data.
    chunk: ^Chunk,

    // Index of the next instruction to be written in the current chunk's
    // code array.
    pc: int,

    // Index of the first free register.
    free_reg: u16,
}

compiler_make :: proc(L: ^VM, parser: ^Parser, chunk: ^Chunk) -> Compiler {
    c := Compiler{L=L, parser=parser, chunk=chunk}
    return c
}

compiler_end :: proc(c: ^Compiler, line: int) {
    compiler_code_return(c, 1, line)
    when ODIN_DEBUG {
        chunk_disassemble(c.chunk)
    }
}

compiler_add_constant :: proc(c: ^Compiler, v: Value) -> (index: u32) {
    L := c.L
    chunk := c.chunk
    for constant, index in chunk.constants[:] {
        if value_eq(v, constant) {
            return cast(u32)index
        }
    }
    return chunk_push_constant(L, chunk, v)
}

// === BYTECODE ============================================================ {{{


/*
Appends `i` to the current chunk's code array.
 */
@(private="file")
add_instruction :: proc(c: ^Compiler, i: Instruction, line: int) -> (pc: int) {
    L     := c.L
    chunk := c.chunk
    c.pc += 1
    return chunk_push_code(L, chunk, i, line)
}

compiler_code_abc :: proc(cl: ^Compiler, op: Opcode, a, b, c: u16, line: int) -> (pc: int) {
    i := instruction_make_abc(op, a, b, c)
    return add_instruction(cl, i, line)
}

compiler_code_abx :: proc(c: ^Compiler, op: Opcode, a: u16, bx: u32, line: int) -> (pc: int) {
    i := instruction_make_abx(op, a, bx)
    return add_instruction(c, i, line)
}

compiler_code_return :: proc(c: ^Compiler, count: u16, line: int) {
    compiler_code_abc(c, .Return, 0, count, 0, line)
}

// === }}} =====================================================================
// === EXPRESSIONS ========================================================= {{{


/*
Reserves `count` registers.

**Returns**
- reg: The first free register before the push.
 */
compiler_push_reg :: proc(c: ^Compiler, count: u16 = 1) -> (reg: u16) {
    reg = c.free_reg
    c.free_reg += count
    if c.free_reg > A_MAX {
        buf: [256]byte
        msg := fmt.bprintf(buf[:], "%i registers exceeded", A_MAX)
        parser_error(c.parser, msg)
    }

    if cast(int)c.free_reg > c.chunk.stack_used {
        c.chunk.stack_used = cast(int)c.free_reg
    }
    return reg
}

/*
Pops the topmost register, ensuring that `reg` matches that register.

**Guarantees**
- If we are popping registers out of order then we panic, because that is a
compiler bug that needs to be addressed at the soonest.
 */
compiler_pop_reg :: proc(c: ^Compiler, reg: u16) {
    // Ensure we pop registers in the correct order.
    prev := c.free_reg - 1
    if reg != prev {
        panic("\nBad pop order: expected reg(%i) but got reg(%i)", prev, reg)
    }
    c.free_reg = prev
}

/*
Emits the bytecode necessary to load the value represented by `e` without
yet pushing it to a destination register.

**Guarantees**
- `e` is transformed to type `.Pc_Pending_Register`. Use `e.pc` to manipulate
the instruction that loads the value.
- No register allocation for the result in `e` is performed. Temporary
registers may be allocated, but they are popped immediately.
 */
@(private="file")
load_expr_value :: proc(c: ^Compiler, e: ^Expr, line: int) {
    pc := -1
    switch e.type {
    case .Nil:
        pc = compiler_load_nil(c, 1, line)

    case .Boolean:
        b := cast(u16)e.boolean
        pc = compiler_code_abc(c, .Load_Bool, 0, b, 0, line)

    case .Number:
        value := value_make(e.number)
        index := compiler_add_constant(c, value)
        pc = compiler_code_abx(c, .Load_Const, 0, index, line)

    case .Constant:
        index := e.index
        pc = compiler_code_abx(c, .Load_Const, 0, index, line)

    case .Pc_Pending_Register:
        return

    case .Register:
        return
    }
    assert(pc != -1)
    expr_set_pc(e, pc)
}

compiler_load_nil :: proc(c: ^Compiler, count: u16, line: int) -> (pc: int) {
    // We might be able to fold consecutive nil loads into one?
    if prev_pc := c.pc - 1; prev_pc > 0 {
        if ip := &c.chunk.code[prev_pc]; ip.op == .Load_Nil {
            from_reg := ip.a
            to_reg   := from_reg + count
            // We are definitely increasing the number of nils being loaded?
            if ip.b <= to_reg {
                ip.b = to_reg
                return prev_pc
            }
        }
    }
    return compiler_code_abc(c, .Load_Nil, 0, count, 0, line)
}

/*
Pushes `e` to the first free register if it is not already in a register.
This one of the (if not the) simplest register allocation strategies.

**Parameters**
- e: The expression to be pushed to a register.

**Returns**
- reg: The index of the register where `e` is found in.

**Guarantees**
- `e` is transformed to type `.Register`.
 */
compiler_push_expr_any :: proc(c: ^Compiler, e: ^Expr, line: int) -> (reg: u16) {
    load_expr_value(c, e, line)
    #partial switch e.type {
    case .Pc_Pending_Register:
        pc := e.pc
        reg = compiler_push_reg(c)
        c.chunk.code[pc].a = reg
        expr_set_reg(e, reg)

    case .Register:
        reg = e.reg
    }
    assert(e.type == .Register)
    return reg
}

/*
Pushes `e` to an RK register if possible. That is, if it is a constant value
then we try to directly encode the load of said constant. Otherwise, we must
explicitly load the value to a register beforehand.
 */
compiler_push_expr_rk :: proc(c: ^Compiler, e: ^Expr, line: int) -> (rk: u16) {
    // Helper to transform constants into RK.
    push_rk :: proc(c: ^Compiler, e: ^Expr, v: Value) -> (rk: u16, ok: bool) {
        i := compiler_add_constant(c, v)
        expr_set_constant(e, i)
        return check_rk(i)
    }

    // Constant index fits in the lower 8 bits of the RK register?
    // i.e. when it is masked, we do not lose any of the original bits.
    check_rk :: proc(i: u32) -> (rk: u16, ok: bool) {
        ok = i <= RK_MASK
        if ok {
            rk = reg_to_rk(cast(u16)i)
        }
        return rk, ok
    }

    switch e.type {
    case .Nil:      return push_rk(c, e, value_make())          or_break
    case .Boolean:  return push_rk(c, e, value_make(e.boolean)) or_break
    case .Number:   return push_rk(c, e, value_make(e.number))  or_break
    case .Constant: return check_rk(e.index) or_break

    // Nothing we can do.
    case .Pc_Pending_Register:
        break

    // Already has a register, reuse it.
    case .Register:
        return e.reg
    }
    return compiler_push_expr_any(c, e, line)
}

@(private="file")
pop_expr :: proc(c: ^Compiler, e: ^Expr) {
    if e.type == .Register {
        compiler_pop_reg(c, e.reg)
    }
}

// === }}} =====================================================================


/*
**Parameters**
- e: The argument operand which does not need to be in a register yet.

**Guarantees**
- If `e` is of type `.Number` and we are doing unary minus, then it is negated
in-place. No bytecode is emitted nor does any register allocation occur.

- Otherwise, `e` is transformed to `.Pc_Pending_Register` and is waiting on
register allocation for its result by the caller.
 */
compiler_code_unary :: proc(c: ^Compiler, op: Opcode, e: ^Expr, line: int) {
    // // Constant folding to avoid unnecessary work.
    if op == .Unm && e.type == .Number {
        e.number = -e.number
        return
    }

    r0 := compiler_push_expr_any(c, e, line)
    pc := compiler_code_abc(c, op, 0, r0, 0, line)
    pop_expr(c, e)
    expr_set_pc(e, pc)
}

/*
**Parameters**
- left: The first operand already in a register (i.e. it was pushed beforehand).
- right: The second operand ready to be emitted to a register.

**Guarantees**
- `left` is transformed to `.Pc_Pending_Register` and is waiting on register
allocation for its result by the caller.
 */
compiler_code_arith :: proc(c: ^Compiler, op: Opcode, left, right: ^Expr, line: int) {
    r1 := compiler_push_expr_rk(c, right, line)
    r0 := compiler_push_expr_rk(c, left,  line)

    // Deallocate temporary registers in the correct order.
    if r0 > r1 {
        pop_expr(c, left)
        pop_expr(c, right)
    } else {
        pop_expr(c, right)
        pop_expr(c, left)
    }

    pc := compiler_code_abc(c, op, 0, r0, r1, line)
    expr_set_pc(left, pc)
}

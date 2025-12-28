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

    // Index of the first free register.
    free_reg: u16,
}

compiler_make :: proc(L: ^VM, parser: ^Parser, chunk: ^Chunk) -> Compiler {
    c := Compiler{L=L, parser=parser, chunk=chunk}
    return c
}

compiler_end :: proc(c: ^Compiler, line: int) {
    compiler_code_return(c, 0, line)
    chunk_disassemble(c.chunk)
}

compiler_add_constant :: proc(c: ^Compiler, v: Value) -> u32 {
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
compiler_add_instruction :: proc(c: ^Compiler, i: Instruction, line: int) -> (pc: int) {
    L     := c.L
    chunk := c.chunk
    return chunk_push_code(L, chunk, i, line)
}

compiler_code_abc :: proc(cl: ^Compiler, op: OpCode, a, b, c: u16, line: int) -> (pc: int) {
    i := instruction_make(op, a, b, c)
    return compiler_add_instruction(cl, i, line)
}

compiler_code_abx :: proc(c: ^Compiler, op: OpCode, a: u16, bx: u32, line: int) -> (pc: int) {
    i := instruction_make(op, a, bx)
    return compiler_add_instruction(c, i, line)
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
    if c.free_reg >= MAX_A {
        buf: [256]byte
        msg := fmt.bprintf(buf[:], "%i registers exceeded", MAX_A)
        parser_error(c.parser, msg)
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
pushing it to a register.

**Guarantees**
- `e` is transformed to type `.Pc_Pending_Register`. Use `e.pc` to manipulate
the instruction.
- No register allocation for the result in `e` is performed. Temporary registers
may be allocated, but they are popped immediately.
 */
@(private="file")
compiler_load_expr_value :: proc(c: ^Compiler, e: ^Expr, line: int) {
    pc := -1
    switch e.type {
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

    case .Pc_Pending_Register, .Register:
        return
    }
    assert(pc != -1)
    expr_set_pc(e, pc)
}

/*
Pushes `e` to the first free register if it is not already in one.

**Parameters**
- e: The expression to be pushed too a register.

**Returns**
- reg: The index of the register where `e` was pushed to.
 */
compiler_push_expr :: proc(c: ^Compiler, e: ^Expr, line: int) -> (reg: u16) {
    compiler_load_expr_value(c, e, line)
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

@(private="file")
compiler_pop_expr :: proc(c: ^Compiler, e: ^Expr) {
    if e.type == .Register {
        compiler_pop_reg(c, e.reg)
    }
}

// === }}} =====================================================================


/*
**Parameters**
- e: The argument operand which does not need to be in a register yet.

**Guarantees**
- `e` is transformed to `.Pc_Pending_Register` and is waiting on register
allocation for its result by the caller.
 */
compiler_code_unary :: proc(c: ^Compiler, op: OpCode, e: ^Expr, line: int) {
    r0 := compiler_push_expr(c, e, line)
    pc := compiler_code_abc(c, op, 0, r0, 0, line)
    compiler_pop_expr(c, e)
    expr_set_pc(e, pc)
}

/*
**Parameters**
- left: The first operand already in a register (i.e. it was pushed beforehand).
- right: The second ooperand ready to be emitted to a register.

**Guarantees**
- `left` is transformed to `.Pc_Pending_Register` and is waiting on register
allocation for its result by the caller.
 */
compiler_code_arith :: proc(c: ^Compiler, op: OpCode, left, right: ^Expr, line: int) {
    // Push both expressions to temporary registers (or reuse existing ones).
    r1 := compiler_push_expr(c, right, line)
    r0 := compiler_push_expr(c, left,  line)

    // Deallocate temporary registers in the correct order.
    if r0 > r1 {
        compiler_pop_expr(c, left)
        compiler_pop_expr(c, right)
    } else {
        compiler_pop_expr(c, right)
        compiler_pop_expr(c, left)
    }

    pc := compiler_code_abc(c, op, 0, r0, r1, line)
    expr_set_pc(left, pc)
}

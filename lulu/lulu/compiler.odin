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
    
    // How many array slots are actively occupied in `active_locals`.
    active_count: u16,

    // Indexes are assigned registers. Values are indexes into `chunk.locals`.
    active_locals: [A_MAX]u16,
}

compiler_make :: proc(L: ^VM, parser: ^Parser, chunk: ^Chunk) -> Compiler {
    c := Compiler{L=L, parser=parser, chunk=chunk}
    return c
}

compiler_end :: proc(c: ^Compiler, line: int) {
    L     := c.L
    chunk := c.chunk
    compiler_code_return(c, 0, 0, line)
    chunk_fix(L, chunk, c.pc)
    when ODIN_DEBUG {
        chunk_disassemble(chunk)
    }
}

/*
'Declaring' a local simply means we now know it exists, but we aren't yet able
to reference it in any way until the entire local assignment is done.

**Parameters**
- local_number: The current number of locals that were already declared at the
time of calling this function. E.g. on the first local, this should be 0.
*/
compiler_declare_local :: proc(c: ^Compiler, name: ^Ostring, local_count: u16) {
    if reg, ok := compiler_resolve_local(c, name); ok {
        parser_error(c.parser, "Shadowing of local variable")
    }

    local  := Local{name, -1, -1}
    _, err := append(&c.chunk.locals, local)
    if err != nil {
        vm_error_memory(c.L)
    }

    local_reg   := c.free_reg + local_count
    local_index := cast(u16)len(c.chunk.locals) - 1
    c.active_locals[local_reg] = local_index
}

compiler_resolve_local :: proc(c: ^Compiler, name: ^Ostring) -> (reg: u16, ok: bool) {
    // Iterate from innermost scope going outwards.
    #reverse for i, reg in c.active_locals[:c.active_count] {
        if c.chunk.locals[i].name == name {
            return cast(u16)reg, true
        }
    }
    return 0, false
}

compiler_define_locals :: proc(c: ^Compiler, count: u16) {
    birth_pc := c.pc
    start    := c.active_count
    stop     := start + count
    for i in c.active_locals[start:stop] {
        local := &c.chunk.locals[i]
        assert(local.name != nil && local.birth_pc == -1 && local.death_pc == -1)
        local.birth_pc = birth_pc
    }
    c.active_count = stop
}

compiler_pop_locals :: proc(c: ^Compiler, count: u16) {
    death_pc := c.pc
    start    := c.active_count - count
    stop     := c.active_count
    for i in c.active_locals[start:stop] {
        local := &c.chunk.locals[i]
        // compiler_pop_reg(c, cast(u16)local_reg)
        assert(local.name != nil && local.birth_pc != -1 && local.death_pc == -1)
        local.death_pc = death_pc
    }
    c.active_count = start
    c.free_reg    -= count
}

compiler_add_string :: proc(c: ^Compiler, s: ^Ostring) -> (index: u32) {
    value := value_make(s)
    index = add_constant(c, value)
    return index
}

@(private="file")
add_constant :: proc(c: ^Compiler, v: Value) -> (index: u32) {
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

**Returns**
- pc: The index of the instruction we just emitted.
 */
@(private="file")
add_instruction :: proc(c: ^Compiler, i: Instruction, line: int) -> (pc: int) {
    L     := c.L
    chunk := c.chunk
    // Save because the member will be updated.
    pc    = c.pc
    c.pc += 1
    chunk_push_code(L, chunk, pc, i, line)
    return pc
}

compiler_code_abc :: proc(cl: ^Compiler, op: Opcode, a, b, c: u16, line: int) -> (pc: int) {
    i := instruction_make_abc(op, a, b, c)
    return add_instruction(cl, i, line)
}

compiler_code_abx :: proc(c: ^Compiler, op: Opcode, a: u16, bx: u32, line: int) -> (pc: int) {
    i := instruction_make_abx(op, a, bx)
    return add_instruction(c, i, line)
}

compiler_code_return :: proc(c: ^Compiler, reg, count: u16, line: int) {
    compiler_code_abc(c, .Return, reg, count, 0, line)
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
Pops the topmost `count` registers, ensuring that `reg` matches that register.

**Guarantees**
- If we are popping registers out of order then we panic, because that is a
compiler bug that needs to be addressed at the soonest.
 */
compiler_pop_reg :: proc(c: ^Compiler, reg: u16, count: u16 = 1, loc := #caller_location) {
    // Ensure we pop registers in the correct order.
    prev := c.free_reg - count
    if reg != prev {
        panic("\nBad pop order: expected reg(%i) but got reg(%i)", prev, reg, loc=loc)
    }
    c.free_reg = prev
}

compiler_load_nil :: proc(c: ^Compiler, reg, count: u16, line: int) {
    // At the start of the function? E.g. assigning empty locals
    if c.pc == 0 {
        return
    }

    // We might be able to fold consecutive nil loads into one?
    fold: if prev_pc := c.pc - 1; prev_pc > 0 {
        ip := &c.chunk.code[prev_pc]
        if ip.op != .Load_Nil {
            break fold
        }

        prev_from := ip.a
        prev_to   := ip.b
        // `reg` (our start register for this hypothetical load nil) is not in
        // range of this load nil, so connecting them would be erroneous?
        if !(prev_from <= reg && reg <= prev_to + 1) {
            break fold
        }

        next_to := reg + count
        if next_to > prev_to {
            ip.b = next_to
        }
        return
    }
    // Otherwise, no optimization.
    compiler_code_abc(c, .Load_Nil, reg, reg + count, 0, line)
}

/*
Pushes `e` to the first free register if it is not already in a register.
This is the 2nd simplest register allocation strategy.

**Parameters**
- e: The expression to be pushed to a register.

**Returns**
- reg: The index of the register where `e` is found in.

**Guarantees**
- `e` is transformed to type `.Register` if it was not already in one.

**Analogous to**
- `lcode.c:luaK_exp2anyreg(FuncState *fs, expdesc *e)` in Lua 5.1.5.
 */
compiler_push_expr_any :: proc(c: ^Compiler, e: ^Expr, line: int) -> (reg: u16) {
    if e.type == .Register {
        return e.reg
    }
    return compiler_push_expr_next(c, e, line)
}

/* 
Unconditionally pushes `e` to the first free register even if it already
residing in one. This is the simplest register alloocation strategy, although
care should be taken to ensure needlessly redundant work is avoided.

**Returns**
- reg: The register which `e` now resides in.

**Guarantees**
- `e` is transformed to type `.Register`.

**Analogous to**
- `lcode.c:luaK_exp2nextreg(FuncState *fs, expdesc *e)` in Lua 5.1.5.
 */
compiler_push_expr_next :: proc(c: ^Compiler, e: ^Expr, line: int) -> (reg: u16) {
    discharge_expr_variables(c, e, line)
    // If `e` is the current topmost register, reuse it.
    compiler_pop_expr(c, e)

    reg = compiler_push_reg(c)
    discharge_expr_to_reg(c, e, reg, line)
    return reg

}

/* 
Emits the bytecode needed to retrieve variables represented by `e`.

**Guarantees**
- If `e` did indeed represent a variable of some kind, then it is transformed
to type `.Pc_Pending_Register`.
 */
@(private="file")
discharge_expr_variables :: proc(c: ^Compiler, e: ^Expr, line: int) {
    #partial switch e.type {
    case .Global:
        pc := compiler_code_abx(c, .Get_Global, 0, e.index, line)
        e^  = expr_make_pc(.Pc_Pending_Register, pc)
    
    // Already in a register, no need to emit any bytecode yet. We don't know
    // if an explicit move operation is appropriate. This is because we have
    // no information on the register allocation state here.
    case .Local:
        e.type = .Register
    }
}


/*
Emits the bytecode necessary to load the value represented by `e` into
the register `reg`. This function is mainly used to help implement actual
register allocation strategies.

**Guarantees**
- `e` is transformed to type `.Register`. This is its register allocation for
the destination, hence it is termed 'discharged' (i.e. finalized).

**Analogous to**
- `lcode.c:discharge2reg(FuncState *fs, expdesc *e, int reg)` in Lua 5.1.5.
 */
@(private="file")
discharge_expr_to_reg :: proc(c: ^Compiler, e: ^Expr, reg: u16, line: int) {
    switch e.type {
    case .Nil:
        compiler_load_nil(c, reg, 1, line)

    case .Boolean:
        b := cast(u16)e.boolean
        compiler_code_abc(c, .Load_Bool, reg, b, 0, line)

    case .Number:
        value := value_make(e.number)
        index := add_constant(c, value)
        compiler_code_abx(c, .Load_Const, reg, index, line)

    case .Constant:
        index := e.index
        compiler_code_abx(c, .Load_Const, reg, index, line)
    
    case .Global, .Local:
        unreachable("Invalid expr to discharge: %v", e.type)

    case .Pc_Pending_Register:
        pc := e.pc
        ip := &c.chunk.code[pc]
        ip.a = reg

    case .Register:
        dst := reg
        src := e.reg
        // Differing registers, so we need to explicitly move?
        // Otherwise, they are the same so we don't do anything as that would
        // be redundant.
        if dst != src {
            compiler_code_abc(c, .Move, dst, src, 0, line)
        }
    }
    e^ = expr_make_reg(.Register, reg)
}

/*
Pushes `e` to an RK register if possible. That is, if it is a constant value
then we try to directly encode the load of said constant. Otherwise, we must
explicitly load the value to a register beforehand.
 */
compiler_push_expr_rk :: proc(c: ^Compiler, e: ^Expr, line: int) -> (rk: u16) {
    // Helper to transform constants into RK.
    push_rk :: proc(c: ^Compiler, e: ^Expr, v: Value) -> (rk: u16, ok: bool) {
        index := add_constant(c, v)
        e^     = expr_make_index(.Constant, index)
        return check_rk(index)
    }

    // Constant index fits in the lower 8 bits of the RK register?
    // i.e. when it is masked, we do not lose any of the original bits.
    check_rk :: proc(index: u32) -> (rk: u16, ok: bool) {
        ok = index <= RK_MASK
        if ok {
            rk = reg_to_rk(cast(u16)index)
        }
        return rk, ok
    }

    switch e.type {
    case .Nil:      return push_rk(c, e, value_make())          or_break
    case .Boolean:  return push_rk(c, e, value_make(e.boolean)) or_break
    case .Number:   return push_rk(c, e, value_make(e.number))  or_break
    case .Constant: return check_rk(e.index) or_break

    // Nothing we can do.
    case .Global, .Pc_Pending_Register:
        break

    // Already has a register, reuse it.
    case .Local, .Register:
        return e.reg
    }
    return compiler_push_expr_any(c, e, line)
}

compiler_pop_expr :: proc(c: ^Compiler, e: ^Expr) {
    if e.type == .Register {
        // Is a temporary register, NOT a local?
        if reg := e.reg; reg >= c.active_count {
            compiler_pop_reg(c, reg)
        }
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
    // Constant folding to avoid unnecessary work.
    if op == .Unm && expr_is_number(e) {
        e.number = number_unm(e.number)
        return
    }

    r0 := compiler_push_expr_any(c, e, line)
    pc := compiler_code_abc(c, op, 0, r0, 0, line)
    compiler_pop_expr(c, e)
    e^ = expr_make_pc(.Pc_Pending_Register, pc)
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
    // Can constant fold?
    if expr_is_number(left) && expr_is_number(right) {
        a := left.number
        b := right.number
        c: f64
        #partial switch op {
        case .Add: c = number_add(a, b)
        case .Sub: c = number_sub(a, b)
        case .Mul: c = number_mul(a, b)
        case .Div: c = number_div(a, b) // WARNING: may divide by zero!
        case .Mod: c = number_mod(a, b) // WARNING: may modulo by zero!
        case .Pow: c = number_pow(a, b)
        case:
            unreachable()
        }
        left.number = c
        return
    }

    r1 := compiler_push_expr_rk(c, right, line)
    r0 := compiler_push_expr_rk(c, left,  line)

    // Deallocate temporary registers in the correct order.
    if r0 > r1 {
        compiler_pop_expr(c, left)
        compiler_pop_expr(c, right)
    } else {
        compiler_pop_expr(c, right)
        compiler_pop_expr(c, left)
    }

    // For high precedence recursive calls, remember that we are the
    // right-hand-side of our parent expression. So in those cases, when we're
    // done, the parent's `right` is already of type `.Pc_Needs_Register`.
    pc := compiler_code_abc(c, op, 0, r0, r1, line)
    left^ = expr_make_pc(.Pc_Pending_Register, pc)
}

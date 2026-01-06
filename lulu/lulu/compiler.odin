#+private package
package lulu

import "core:fmt"
import "core:math"

INVALID_PC :: -1

Compiler :: struct {
    // Parent state.
    L: ^State,

    // Sister state, mainly used for context during error handling.
    parser: ^Parser,

    // Not owned by us, but we are the ones filling in the data.
    chunk: ^Chunk,

    // Current counter of how many instructions we have actively written so far
    // to `chunk.code`. Also acts as the index of the next instruction to be
    // written in the current chunk's code array.
    pc: int,

    // Number of all the values we have actively written in `chunk.constants`.
    // Also acts as the index of the next constant to be written.
    constants_count: u32,

    // Number of all the local variable information structs we have actively
    // written in `chunk.locals`. Also acts as the index of the next write.
    locals_count: u16,

    // Index of the first free register.
    free_reg: u16,

    // How many array slots are actively occupied in `active_locals`.
    active_count: u16,

    // Indexes are assigned registers. Since they are registers, they must
    // fit in `MAX_REG`.
    //
    // Values are indexes into `chunk.locals`. This is because we can have
    // more than `MAX_REG` overall local variable information (e.g. many
    // short-lived locals).
    active_locals: [MAX_REG]u16,
}

compiler_make :: proc(L: ^State, parser: ^Parser, chunk: ^Chunk) -> Compiler {
    c := Compiler{L=L, parser=parser, chunk=chunk}
    return c
}

compiler_end :: proc(c: ^Compiler) {
    L     := c.L
    chunk := c.chunk
    compiler_code_return(c, 0, 0)
    chunk_fix(L, chunk, c)
    disassemble(chunk)
}

/*
'Declaring' a local simply means we now know it exists, but we aren't yet able
to reference it in any way until the entire local assignment is done.

**Parameters**
- count: The current number of locals that were already declared at the time
of calling this function. This should not include the local we are about to
declare. E.g. on the first local, this should be 0. On the second local, this
should be 1.
*/
compiler_declare_local :: proc(c: ^Compiler, name: ^Ostring, count: u16) {
    if _, ok := compiler_resolve_local(c, name); ok {
        // For most cases, the shadowed name already in `c.parser.consumed`.
        // For `for` loop state, this is never reached because they contain
        // invalid identifier characters.
        parser_error(c.parser, "Shadowing of local variable")
    }

    info  := Local_Info{name, INVALID_PC, INVALID_PC}
    index := chunk_push_local(c.L, c.chunk, &c.locals_count, info)

    // Don't push (reserve) registers yet, because we don't want to 'see' this
    // local if we use the same name in the assigning expression.
    reg := c.free_reg + count
    c.active_locals[reg] = index
}

compiler_resolve_local :: proc(c: ^Compiler, name: ^Ostring) -> (reg: u16, ok: bool) {
    // Iterate from innermost scope going outwards.
    #reverse for i, reg in c.active_locals[:c.active_count] {
        if c.chunk.locals[i].name == name {
            return u16(reg), true
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
        assert(local.name != nil)
        assert(local.birth_pc == INVALID_PC)
        assert(local.death_pc == INVALID_PC)
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
        assert(local.name != nil)
        assert(local.birth_pc != INVALID_PC)
        assert(local.death_pc == INVALID_PC)
        local.death_pc = death_pc
    }
    compiler_pop_reg(c, start, stop - start)
    c.active_count = start
}

compiler_add_string :: proc(c: ^Compiler, s: ^Ostring) -> (index: u32) {
    index = _add_constant(c, value_make_ostring(s))
    return index
}

@(private="file")
_add_constant :: proc(c: ^Compiler, v: Value) -> (index: u32) {
    L := c.L
    chunk := c.chunk
    for constant, index in chunk.constants[:] {
        if value_eq(v, constant) {
            return u32(index)
        }
    }
    return chunk_push_constant(L, chunk, &c.constants_count, v)
}

// === BYTECODE ============================================================ {{{


/*
Appends `i` to the current chunk's code array.

**Returns**
- pc: The index of the instruction we just emitted.
 */
@(private="file")
_add_instruction :: proc(c: ^Compiler, i: Instruction) -> (pc: int) {
    L := c.L
    p := c.parser

    line := p.consumed.line
    col  := p.consumed.col
    return chunk_push_code(L, c.chunk, &c.pc, i, line, col)
}

compiler_code_abc :: proc(cl: ^Compiler, op: Opcode, a, b, c: u16) -> (pc: int) {
    assert(OP_INFO[op].mode == .ABC)

    i := instruction_make_abc(op, a, b, c)
    return _add_instruction(cl, i)
}

compiler_code_abx :: proc(c: ^Compiler, op: Opcode, a: u16, bx: u32) -> (pc: int) {
    assert(OP_INFO[op].mode == .ABx)
    assert(OP_INFO[op].b != nil)
    assert(OP_INFO[op].c == nil)

    i := instruction_make_abx(op, a, bx)
    return _add_instruction(c, i)
}

compiler_code_return :: proc(c: ^Compiler, reg, count: u16) {
    compiler_code_abc(c, .Return, reg, count - u16(VARIADIC), 0)
}

compiler_set_returns :: proc(c: ^Compiler, call: ^Expr, ret_count: u16) {
    // Expression is an open function call?
    if call.type == .Call {
        // VARIADIC is -1, so we encode 0.
        // Likewise, returning 0 values is actually encoded as C=1.
        ip := &c.chunk.code[call.pc]
        ip.c  = (ret_count - u16(VARIADIC))
        call^ = expr_make_reg(.Register, ip.a)
    }
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
    if c.free_reg > MAX_REG {
        buf: [256]byte
        msg := fmt.bprintf(buf[:], "%i registers exceeded", MAX_REG)
        parser_error(c.parser, msg)
    }

    if int(c.free_reg) > c.chunk.stack_used {
        c.chunk.stack_used = int(c.free_reg)
    }
    return reg
}

/*
Pops the topmost `count` registers, ensuring that `reg` will be the new top
upon done popping.

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

compiler_load_nil :: proc(c: ^Compiler, reg, count: u16) {
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
    compiler_code_abc(c, .Load_Nil, reg, reg + count, 0)
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
compiler_push_expr_any :: proc(c: ^Compiler, e: ^Expr) -> (reg: u16) {
    // Convert `.Local` to `.Register`
    _discharge_expr_variables(c, e)
    if e.type == .Register {
        return e.reg
    }
    return compiler_push_expr_next(c, e)
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
compiler_push_expr_next :: proc(c: ^Compiler, e: ^Expr) -> (reg: u16) {
    _discharge_expr_variables(c, e)
    // If `e` is the current topmost register, reuse it.
    compiler_pop_expr(c, e)

    reg = compiler_push_reg(c)
    _discharge_expr_to_reg(c, e, reg)
    return reg

}

/*
Emits the bytecode needed to retrieve variables represented by `e`.

**Guarantees**
- If `e` did indeed represent a variable of some kind, then it is transformed
to type `.Pc_Pending_Register`.
 */
@(private="file")
_discharge_expr_variables :: proc(c: ^Compiler, e: ^Expr) {
    #partial switch e.type {
    case .Global:
        pc := compiler_code_abx(c, .Get_Global, 0, e.index)
        e^  = expr_make_pc(.Pc_Pending_Register, pc)

    // Already in a register, no need to emit any bytecode yet. We don't know
    // if an explicit move operation is appropriate. This is because we have
    // no information on the register allocation state here.
    case .Local:
        e.type = .Register

    case .Call:
        compiler_set_returns(c, e, 1)
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
_discharge_expr_to_reg :: proc(c: ^Compiler, e: ^Expr, reg: u16, loc := #caller_location) {
    switch e.type {
    case .Nil:
        compiler_load_nil(c, reg, 1)

    case .Boolean:
        compiler_code_abc(c, .Load_Bool, reg, u16(e.boolean), 0)

    case .Number:
        // Can we load it as a positive integer immediately from Bx?
        if n := e.number; 0.0 <= n && n <= MAX_IMM_Bx && n == math.floor(n) {
            imm := u32(n)
            compiler_code_abx(c, .Load_Imm, reg, imm)
        } // Otherwise, we need to load this number in a dedicated instruction.
        else {
            i := _add_constant(c, value_make_number(n))
            compiler_code_abx(c, .Load_Const, reg, i)
        }

    case .Constant:
        compiler_code_abx(c, .Load_Const, reg, e.index)

    // We assume `discharge_expr_variables()` was called beforehand
    case .Global, .Local, .Call:
        unreachable("Invalid expr to discharge: %v", e.type, loc=loc)

    case .Pc_Pending_Register:
        c.chunk.code[e.pc].a = reg

    case .Register:
        dst := reg
        src := e.reg
        // Differing registers, so we need to explicitly move?
        // Otherwise, they are the same so we don't do anything as that would
        // be redundant.
        if dst != src {
            compiler_code_abc(c, .Move, dst, src, 0)
        }
    }
    e^ = expr_make_reg(.Register, reg)
}

/*
Pushes `e` to K register if possible. That is, if it is a constant value
then we try to directly encode the load of said constant into the opcode
arguments.

**Guarantees**
- If `e` represents a literal value then it is transformed to `.Constant`
no matter what.

**Returns**
- k: The index of the constant in the current chunk's constants array.
- ok: `true` if `e` could be transformed to or already was `.Constant` and
the index of the constant fits in `K[C]`. Otherwise `false` if `e` could not
be transformed to a constant or it was a constant but couldn't fit in `K[C]`.
 */
@(private="file")
_push_expr_k :: proc(c: ^Compiler, e: ^Expr) -> (index: u16, ok: bool) {
    // Helper to transform constants into K.
    push_k :: proc(c: ^Compiler, e: ^Expr, v: Value) -> (k: u16, ok: bool) {
        index := _add_constant(c, v)
        e^     = expr_make_index(.Constant, index)
        return check_k(index)
    }

    // Constant index fits in a K register?
    // i.e. when it is masked t fit, we do not lose any of the original bits.
    check_k :: proc(index: u32) -> (k: u16, ok: bool) {
        return u16(index), index <= MAX_K_C
    }

    switch e.type {
    case .Nil:      return push_k(c, e, value_make_nil())
    case .Boolean:  return push_k(c, e, value_make_boolean(e.boolean))
    case .Number:   return push_k(c, e, value_make_number(e.number))
    case .Constant: return check_k(e.index)

    // Nothing we can do.
    case .Global, .Call, .Pc_Pending_Register:
        break

    // Already has a register, reuse it.
    case .Local, .Register:
        return e.reg, true
    }
    return 0, false
}

compiler_pop_expr :: proc(c: ^Compiler, e: ^Expr, loc := #caller_location) {
    if e.type == .Register {
        // Is a temporary register, NOT a local?
        if reg := e.reg; reg >= c.active_count {
            compiler_pop_reg(c, reg, loc=loc)
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
compiler_code_unary :: proc(c: ^Compiler, op: Opcode, e: ^Expr) {
    // Constant folding to avoid unnecessary work.
    if op == .Unm && e.type == .Number {
        e.number = number_unm(e.number)
        return
    }

    r0 := compiler_push_expr_any(c, e)
    pc := compiler_code_abc(c, op, 0, r0, 0)
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
compiler_code_arith :: proc(c: ^Compiler, op: Opcode, left, right: ^Expr) {
    // Helper to avoid division or modulo by zero.
    check_nonzero :: #force_inline proc(a: f64) -> (val: f64, ok: bool) {
        if ok = a == 0; ok {
            val = a
        }
        return val, ok
    }

    // Can constant fold?
    fold: if left.type == .Number && right.type == .Number {
        a := left.number
        b := right.number
        res: f64
        #partial switch op {
        case .Add: res = number_add(a, b)
        case .Sub: res = number_sub(a, b)
        case .Mul: res = number_mul(a, b)
        case .Div: res = number_div(a, check_nonzero(b) or_break fold)
        case .Mod: res = number_mod(a, check_nonzero(b) or_break fold)
        case .Pow: res = number_pow(a, b)
        case:
            unreachable("Invalid arithmetic opcode %v", op)
        }
        left.number = res
        return
    }

    act_op, arg_b, arg_c := _arith(c, op, left, right)

    // For high precedence recursive calls, remember that we are the
    // right-hand-side of our parent expression. So in those cases, when we're
    // done, the parent's `right` is already of type `.Pc_Needs_Register`.
    pc := compiler_code_abc(c, act_op, 0, arg_b, arg_c)
    left^ = expr_make_pc(.Pc_Pending_Register, pc)
}

@(private="file")
_arith :: proc(c: ^Compiler, op: Opcode, left, right: ^Expr) -> (act_op: Opcode, arg_b: u16, arg_c: u16) {
    rb := compiler_push_expr_any(c, left)

    // First try register-immediate.
    try: if imm, neg, ok := _check_imm(right); ok {
        #partial switch op {
        case .Add: act_op = .Add_Imm if !neg else .Sub_Imm
        case .Sub: act_op = .Sub_Imm if !neg else .Add_Imm

        // Have an immediate but we don't have a dedicated instruction for this
        // particular arithmetic operation.
        case .Mul..=.Pow:
            break try
        case:
            unreachable()
        }

        compiler_pop_expr(c, left)
        return act_op, rb, imm
    } else if expr_is_literal(right) {
        // Wasn't register-immediate, try register-constant next.
        #partial switch op {
        case .Add: act_op = .Add_Const
        case .Sub: act_op = .Sub_Const
        case .Mul: act_op = .Mul_Const
        case .Div: act_op = .Div_Const
        case .Mod: act_op = .Mod_Const
        case .Pow: act_op = .Pow_Const
        case:
            unreachable()
        }

        // If it doesn't fit in K[C], then we need to load it in a separate
        // instruction via `compiler_push_expr_any()`.
        kc := _push_expr_k(c, right) or_break try
        compiler_pop_expr(c, left)
        return act_op, rb, kc
    }

    // Neither register-immediate nor register-constant, do register-register.
    rc := compiler_push_expr_any(c, right)

    // Deallocate temporary registers in the correct order.
    // Don't pop the registers directly, because `rb` and/or `rc` may
    // not be poppable!
    if rb > rc {
        compiler_pop_expr(c, left)
        compiler_pop_expr(c, right)
    } else {
        compiler_pop_expr(c, right)
        compiler_pop_expr(c, left)
    }
    return op, rb, rc
}

@(private="file")
_check_imm :: #force_inline proc(e: ^Expr) -> (imm: u16, neg, ok: bool) {
    if e.type == .Number {
        n  := e.number
        neg = n < 0.0
        n   = abs(n)
        // Is an integer in range of the immediate operand?
        ok  = n <= MAX_IMM_C && n == math.floor(n)
        imm = u16(n) if ok else 0
    }
    return imm, neg, ok
}


compiler_code_concat :: proc(c: ^Compiler, left, right: ^Expr) {
    assert(left.type == .Register)
    rb := left.reg

    if right.type == .Pc_Pending_Register {
        ip := &c.chunk.code[right.pc]
        // Recursive case.
        if ip.op == .Concat {
            assert(rb == ip.b - 1)

            // Recursive calls see only 1 temporary register, as our `right`
            // originally held a register but got overwritten in the child
            // recursive call that finished right before us.
            compiler_pop_reg(c, rb)
            ip.b -= 1

            // Ensure parent recursive caller sees the foldable instruction.
            left^ = right^
            return
        }
    }

    // Base case. Most recursive call will push the final argument,
    // all the previous parent's `left` arguments were already pushed in order.
    rc := compiler_push_expr_next(c, right)

    // Base case sees 2 temporary registers. Pop them.
    if rb > rc {
        compiler_pop_reg(c, rb)
        compiler_pop_reg(c, rc)
    } else {
        compiler_pop_reg(c, rc)
        compiler_pop_reg(c, rb)
    }

    // Add 1 to ensure a half-open range for quick slicing.
    pc   := compiler_code_abc(c, .Concat, 0, rb, rc + 1)
    left^ = expr_make_pc(.Pc_Pending_Register, pc)
}

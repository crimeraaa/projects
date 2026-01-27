#+private package
package lulu

import "core:fmt"
import "core:math"

INVALID_PC :: i32(-1)
NO_JUMP    :: i32(-1)

Compiler :: struct {
    // Parent state.
    L: ^State,

    // Sister state, mainly used for context during error handling.
    parser: ^Parser,

    // Current block information for local resolution and shadowing checking.
    block: ^Block,

    // Not owned by us, but we are the ones filling in the data.
    // This must exist in the VM's current stack frame in order to avoid
    // garbage collection midway.
    chunk: ^Chunk,

    // Current counter of how many instructions we have actively written so far
    // to `chunk.code`. Also acts as the index of the next instruction to be
    // written in the current chunk's code array.
    pc: i32,

    // Absolute pc of the last 'jump' target.
    // This is used to determine if we can implicitly use the stack frame
    // itself to load nil. This is because jumps can introduce complications.
    last_target: i32,

    // Number of all the values we have actively written in `chunk.constants`.
    // Also acts as the index of the next constant to be written.
    constants_count: u32,

    // Number of all the local variable information structs we have actively
    // written in `chunk.locals`. Also acts as the index of the next write.
    locals_count: u16,

    // Index of the first free register.
    free_reg: u16,

    // How many active locals are occupying the array slots in `active_locals`.
    active_count: u16,

    // Indexes are assigned registers. Since they are registers, they must
    // fit in `MAX_REG`.
    //
    // Values are indexes into `chunk.locals`. This is because we can have
    // more than `MAX_REG` overall local variable information (e.g. many
    // short-lived locals).
    active_locals: [MAX_REG]u16,
}

Block :: struct {
    // Pointer to the previous block, which exists on the stack.
    // This is a stack-allocated linked list.
    prev: ^Block,

    // Absolute pc of the most recently emitted `.Jump` for a `break` statement.
    // It is a jump list, that is its sBx argument is actually an offset
    // going to the previous `.Jump`. The jump list terminates at `NO_JUMP`.
    break_list: i32,

    // 'Previous local count'.
    //
    // How many active locals are occupying the array slots in `active_locals`
    // at the time the block was pushed. This is used to 'pop' locals
    // upon exiting this block.
    plcount: u16,

    // If we see any `break`, they can be patched if this is `true` otherwise
    // we should throw a syntax error.
    breakable: bool,
}

compiler_make :: proc(L: ^State, parser: ^Parser, chunk: ^Chunk) -> Compiler {
    c := Compiler{L=L, parser=parser, chunk=chunk, last_target=NO_JUMP}
    return c
}

compiler_end :: proc(c: ^Compiler) {
    L     := c.L
    chunk := c.chunk
    compiler_code_return(c, 0, 0)
    chunk_fix(L, chunk, c)
    disassemble(chunk)
}

@(disabled=ODIN_DISABLE_ASSERT)
compiler_assert :: proc(c: ^Compiler, cond: bool, msg := #caller_expression(cond), args: ..any, loc := #caller_location) {
    parser_assert(c.parser, cond, msg, ..args, loc=loc)
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
    if _, scope := compiler_resolve_local(c, name); scope == SCOPE_CURRENT {
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

SCOPE_CURRENT :: 0
SCOPE_GLOBAL  :: -1

/*
**Assumptions**
- `c.block` is non-`nil`. Even the file scope should have its own block.
 */
compiler_resolve_local :: proc(c: ^Compiler, name: ^Ostring) -> (reg: u16, scope: int) {
    stop := c.active_count

    // Iterate from innermost scope going outwards.
    for block := c.block; block != nil; block = block.prev {
        start := block.plcount

        #reverse for i, lreg in c.active_locals[start:stop] {
            if c.chunk.locals[i].name == name {
                // Slices have their own indices, so re-add the offset.
                return u16(lreg) + start, scope
            }
        }

        stop  = start
        scope += 1
    }
    return 0, SCOPE_GLOBAL
}

compiler_define_locals :: proc(c: ^Compiler, count: u16) {
    born  := c.pc
    start := c.active_count
    stop  := start + count
    for local_index in c.active_locals[start:stop] {
        local := &c.chunk.locals[local_index]
        compiler_assert(c, local.name != nil)

        compiler_assert(c, local.born == INVALID_PC,
            "Got locals[%i].pc(%i)",
            local_index, local.born)

        compiler_assert(c, local.died == INVALID_PC,
            "Got locals[%i].pc(%i)",
            local_index, local.died)

        local.born = born
    }
    c.active_count = stop
}

compiler_push_block :: proc(c: ^Compiler, b: ^Block, breakable: bool) {
    b.prev       = c.block
    b.break_list = NO_JUMP
    b.plcount    = c.active_count
    b.breakable  = breakable
    c.block      = b
}

compiler_pop_block :: proc(c: ^Compiler) {
    compiler_assert(c, c.block != nil, "No block to pop with c.active_count(%i)", c.active_count)

    died       := compiler_get_target(c)
    prev_count := c.block.plcount

    // Finalize local information for this scope.
    for local_index in c.active_locals[prev_count:c.active_count] {
        local := &c.chunk.locals[local_index]
        compiler_assert(c, local.name != nil)
        compiler_assert(c, local.born != INVALID_PC)
        compiler_assert(c, local.died == INVALID_PC,
            "Got locals[%i].pc(%i)",
            local_index, local.died)

        local.died = died
    }

    compiler_patch_jump_list(c, c.block.break_list, died)

    // Pop this scope's locals, restoring the previous active count.
    c.block        = c.block.prev
    c.free_reg     = prev_count
    c.active_count = prev_count
}

compiler_add_string :: proc(c: ^Compiler, s: ^Ostring) -> (index: u32) {
    index = _add_constant(c, value_make(s))
    return index
}

@(private="file")
_add_constant :: proc(c: ^Compiler, v: Value) -> (index: u32) {
    L     := c.L
    chunk := c.chunk
    for constant, index in chunk.constants[:c.constants_count] {
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
_add_instruction :: proc(c: ^Compiler, i: Instruction) -> (pc: i32) {
    L := c.L
    p := c.parser

    line := p.consumed.line
    col  := p.consumed.col
    return chunk_push_code(L, c.chunk, &c.pc, i, line, col)
}

compiler_code_ABC :: proc(cl: ^Compiler, op: Opcode, A, B, C: u16) -> (pc: i32) {
    compiler_assert(cl, OP_INFO[op].mode == .ABC)

    i := Instruction{base={op=op, A=A, B=B, C=C}}
    return _add_instruction(cl, i)
}

compiler_code_ABCk :: proc(cl: ^Compiler, op: Opcode, A, B, C: u16, k: bool) -> (pc: i32) {
    compiler_assert(cl, OP_INFO[op].mode == .ABCk)

    i := Instruction{k={op=op, A=A, B=B, C=C, k=k}}
    return _add_instruction(cl, i)
}

compiler_code_ABx :: proc(c: ^Compiler, op: Opcode, A: u16, Bx: u32) -> (pc: i32) {
    compiler_assert(c, OP_INFO[op].mode == .ABx)
    compiler_assert(c, OP_INFO[op].b != nil)
    compiler_assert(c, OP_INFO[op].c == nil)

    i := Instruction{u={op=op, A=A, Bx=Bx}}
    return _add_instruction(c, i)
}

compiler_code_AsBx :: proc(c: ^Compiler, op: Opcode, A: u16, sBx: i32) -> (pc: i32) {
    compiler_assert(c, OP_INFO[op].mode == .AsBx)
    compiler_assert(c, OP_INFO[op].b != nil)
    compiler_assert(c, OP_INFO[op].c == nil)

    i := Instruction{s={op=op, A=A, Bx=sBx}}
    // fmt.printfln(".code[%i] = %v", c.pc, i.s)
    return _add_instruction(c, i)
}

compiler_code_return :: proc(c: ^Compiler, reg, count: u16) {
    compiler_code_ABC(c, .Return, reg, count - u16(VARIADIC), 0)
}

compiler_set_returns :: proc(c: ^Compiler, call: ^Expr, ret_count: u16) {
    // Expression is an open function call?
    if call.type == .Call {
        // VARIADIC is -1, so we encode 0.
        // Likewise, returning 0 values is actually encoded as C=1.
        ip := &c.chunk.code[call.pc]
        ip.C  = ret_count - u16(VARIADIC)
    }
}

compiler_code_jump :: proc(c: ^Compiler, op: Opcode, A: u16) -> (pc: i32) {
    compiler_assert(c, op == .Jump || op == .Jump_Not)
    return compiler_code_AsBx(c, op, A, -1)
}

compiler_add_jump_list :: proc(c: ^Compiler, list: ^i32) -> (next: i32) {
    jump := list^
    compiler_assert(c, jump == NO_JUMP || c.chunk.code[jump].op == .Jump)

    offset := NO_JUMP
    if jump != NO_JUMP {
        // Jump lists are chained together in a series of negative offsets.
        // And yes, the reversed arguments are intentional.
        target := compiler_get_target(c)
        offset = _get_offset(jump=target, target=jump)
    }

    next  = compiler_code_AsBx(c, .Jump, 0, offset)
    list^ = next
    return next
}

compiler_patch_jump :: proc(c: ^Compiler, jump: i32, target := INVALID_PC) {
    if jump == NO_JUMP {
        return
    }
    target := _get_target(c, target)
    prev   := _patch_jump(c, jump, target)
    // `.Jump_If` can never be a jump list.
    compiler_assert(c, prev == NO_JUMP)
}

compiler_patch_jump_list :: proc(c: ^Compiler, list: i32, target := INVALID_PC) {
    target := _get_target(c, target)
    for jump := list; jump != NO_JUMP; {
        prev := _patch_jump(c, jump, target)
        jump = prev
    }
}

@(private="file")
_get_offset :: proc(jump, target: i32) -> (offset: i32) {
    return target - (jump + 1)
}

@(private="file")
_get_target :: proc(c: ^Compiler, target: i32) -> i32 {
    return target if target != NO_JUMP else compiler_get_target(c)
}

/*
Mark the current pc as a jump target, preventing invalid optimizations
relating to implicitly loading `nil`.
 */
compiler_get_target :: proc(c: ^Compiler) -> (target: i32) {
    c.last_target = c.pc
    return c.pc
}

@(private="file")
_patch_jump :: proc(c: ^Compiler, jump, target: i32) -> (prev: i32) {
    offset := _get_offset(jump, target)
    if offset < MIN_sBx || offset > MAX_sBx {
        parser_error(c.parser, "jump too large")
    }
    ip := &c.chunk.code[jump]
    assert(ip.op == .Jump || ip.op == .Jump_Not)

    prev = ip.s.Bx
    if prev != NO_JUMP {
        // If a jump list, then the previous jump can be found at the
        // given negative offset.
        prev = (jump + 1) + prev
    }
    ip.s.Bx = offset
    return prev
}

/*
Sets the return count of the call pc in `call` to `ret_count` and converts
`call` to a `.Register` expression representing the function register.

**Analogous to**
- `lcode.c:luaK_setoneret(FuncState *fs, expdesc *e)` Lua 5.1.5.
 */
compiler_discharge_returns :: proc(c: ^Compiler, call: ^Expr, ret_count: u16) {
    if call.type == .Call {
        ip := &c.chunk.code[call.pc]
        ip.C  = ret_count - u16(VARIADIC)
        call^ = expr_make_reg(.Register, ip.A)
    }
}

compiler_get_table :: proc(c: ^Compiler, #no_alias var, key: ^Expr) {
    var_reg      := compiler_push_expr_any(c, var)
    key_rk, is_k := _push_expr_k(c, key, limit=MAX_C)

    var.type       = .Table
    var.table.reg  = var_reg
    var.table.key  = key_rk
    var.table.is_k = is_k
}

compiler_set_table :: proc(c: ^Compiler, table_reg: u16, #no_alias key, value: ^Expr) {
    key_rk,   key_is_k   := _push_expr_k(c, key,   limit=MAX_B)
    value_rk, value_is_k := _push_expr_k(c, value, limit=MAX_Ck)
    compiler_pop_expr(c, value)
    compiler_pop_expr(c, key)

    op := _get_table_op(key_is_k)
    compiler_code_ABCk(c, op, table_reg, key_rk, value_rk, value_is_k)
}

compiler_set_variable :: proc(c: ^Compiler, #no_alias variable, value: ^Expr) {
    #partial switch variable.type {
    case .Global:
        value_reg := compiler_push_expr_any(c, value)
        compiler_code_ABx(c, .Set_Global, value_reg, variable.index)

    case .Local:
        compiler_pop_expr(c, value)
        _discharge_expr_to_reg(c, value, variable.reg)
        // Avoid the below pop.
        return

    case .Table:
        value_reg, value_is_k := _push_expr_k(c, value, limit=MAX_Ck)
        op := _get_table_op(variable.table.is_k)
        compiler_code_ABCk(c, op, variable.table.reg, variable.table.key, value_reg, value_is_k)

    case:
        unreachable()
    }
    compiler_pop_expr(c, value)
}

_get_table_op :: proc(key_is_k: bool) -> (op: Opcode) {
    if key_is_k {
        return .Set_Field
    } else {
        return .Set_Table
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
        buf: [64]byte
        msg := fmt.bprintf(buf[:], "%i registers exceeded", MAX_REG)
        parser_error(c.parser, msg)
    }

    if c.free_reg > u16(c.chunk.stack_used) {
        c.chunk.stack_used = u8(c.free_reg)
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
    // Don't pop existing locals.
    if reg < c.active_count {
        return
    }

    // Ensure we pop registers in the correct order.
    prev := c.free_reg - count
    compiler_assert(c, reg == prev,
        "Bad pop order, expected reg(%i) but got reg(%i)",
        prev, reg, loc=loc)

    c.free_reg = prev
}

compiler_load_nil :: proc(c: ^Compiler, reg, count: u16) {
    // At the start of the function? E.g. assigning empty locals
    if c.pc == 0 && c.pc < c.last_target {
        return
    }

    // We might be able to fold consecutive nil loads into one?
    fold: if prev_pc := c.pc - 1; prev_pc > 0 && prev_pc < c.last_target {
        ip := &c.chunk.code[prev_pc]
        if ip.op != .Load_Nil {
            break fold
        }

        prev_from := ip.A
        prev_to   := ip.B
        // `reg` (our start register for this hypothetical load nil) is not in
        // range of this load nil, so connecting them would be erroneous?
        if !(prev_from <= reg && reg <= prev_to + 1) {
            break fold
        }

        next_to := reg + count
        if next_to > prev_to {
            ip.B = next_to
        }
        return
    }
    // Otherwise, no optimization.
    compiler_code_ABC(c, .Load_Nil, reg, reg + count, 0)
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
        pc := compiler_code_ABx(c, .Get_Global, 0, e.index)
        e^  = expr_make_pc(.Pc_Pending_Register, pc)

    // Already in a register, no need to emit any bytecode yet. We don't know
    // if an explicit move operation is appropriate. This is because we have
    // no information on the register allocation state here.
    case .Local:
        e.type = .Register

    case .Table:
        op: Opcode
        if e.table.is_k {
            op = .Get_Field
        } else {
            compiler_pop_reg(c, e.table.key)
            op = .Get_Table
        }

        compiler_pop_reg(c, e.table.reg)

        pc := compiler_code_ABC(c, op, 0, e.table.reg, e.table.key)
        e^  = expr_make_pc(.Pc_Pending_Register, pc)

    case .Call:
        compiler_discharge_returns(c, e, 1)
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
    _discharge_expr_variables(c, e)
    switch e.type {
    case .None:    unreachable()
    case .Nil:     compiler_load_nil(c, reg, 1)
    case .Boolean: compiler_code_ABC(c, .Load_Bool, reg, u16(e.boolean), 0)

    case .Number:
        // Can we load it as a positive integer immediately from Bx?
        n := e.number
        if 0.0 <= n && n <= MAX_IMM_Bx && n == math.floor(n) {
            imm := u32(n)
            compiler_code_ABx(c, .Load_Imm, reg, imm)
        } // Otherwise, we need to load this number in a dedicated instruction.
        else {
            i := _add_constant(c, value_make(n))
            compiler_code_ABx(c, .Load_Const, reg, i)
        }

    case .Constant:
        compiler_code_ABx(c, .Load_Const, reg, e.index)

    case .Global, .Local, .Table, .Call:
        unreachable("Invalid expr to discharge: %v", e.type, loc=loc)

    case .Pc_Pending_Register:
        c.chunk.code[e.pc].A = reg

    case .Compare:
        // These instructions may be jump targets by themselves.
        compiler_code_ABC(c, .Load_Bool, reg, u16(true),  1)
        compiler_code_ABC(c, .Load_Bool, reg, u16(false), 0)

    case .Register:
        // Differing registers, so we need to explicitly move?
        // Otherwise, they are the same so we don't do anything as that would
        // be redundant.
        if reg != e.reg {
            compiler_code_ABC(c, .Move, reg, e.reg, 0)
        }
    }
    e^ = expr_make_reg(.Register, reg)
}

/*
Pushes `e` to K register if possible. That is, if it is a constant value
then we try to directly encode the load of said constant into the opcode
arguments. Otherwise, `e` is pushed to the next available register.

**Guarantees**
- If `e` represents a literal value then it is first transformed to
`.Constant` no matter what.
- Then, if `e` fits in a K register, it remains as-is.
- Otherwise, we explicitly load the constant in the next avaiable register,
transforming `e` to `.Register`.

**Returns**
- rk: The index of the constant or the register of the loaded value.
- is_k: `true` if `e` could be transformed to or already was `.Constant` and
the index of the constant fits in `limit`. Otherwise `false` if `e` could not
be transformed to a constant or it was a constant not able to fit in `limit`.
 */
@(private="file")
_push_expr_k :: proc(c: ^Compiler, e: ^Expr, limit: u16) -> (rk: u16, is_k: bool) {
    switch e.type {
    case .None:     unreachable()
    case .Nil:      return _push_k(c, e, value_make(), limit)
    case .Boolean:  return _push_k(c, e, value_make(e.boolean), limit)
    case .Number:   return _push_k(c, e, value_make(e.number), limit)
    case .Constant: return _check_k(c, e, e.index, limit)

    // Nothing we can do.
    case .Global, .Local, .Table, .Pc_Pending_Register, .Call, .Compare, .Register:
        break
    }
    rk = compiler_push_expr_any(c, e)
    return rk, false
}

// Helper to transform constants into K.
_push_k :: proc(c: ^Compiler, e: ^Expr, v: Value, limit: u16) -> (rk: u16, is_k: bool) {
    index := _add_constant(c, v)
    e^     = expr_make_index(.Constant, index)
    return _check_k(c, e, index, limit)
}

// Constant index fits in a K register?
// i.e. when it is masked t fit, we do not lose any of the original bits.
_check_k :: proc(c: ^Compiler, e: ^Expr, index: u32, limit: u16) -> (rk: u16, is_k: bool) {
    is_k = index <= u32(limit)
    rk   = u16(index) if is_k else compiler_push_expr_next(c, e)
    return rk, is_k
}

compiler_pop_expr :: proc(c: ^Compiler, e: ^Expr, loc := #caller_location) {
    if e.type == .Register {
        compiler_pop_reg(c, e.reg, loc=loc)
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
    #partial switch e.type {
    case .Nil:
        if op == .Not {
            e^ = expr_make_boolean(true)
            return
        }
    case .Boolean:
        if op == .Not {
            e.boolean = !e.boolean
            return
        }
    case .Number:
        if op == .Unm {
            e.number = number_unm(e.number)
            return
        }
    case:
        break
    }

    rb := compiler_push_expr_any(c, e)
    pc := compiler_code_ABC(c, op, 0, rb, 0)
    compiler_pop_reg(c, rb)
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
compiler_code_arith :: proc(c: ^Compiler, op: Opcode, #no_alias left, right: ^Expr) {
    // Helper to avoid division or modulo by zero.
    check_nonzero :: proc(a: f64) -> (val: f64, ok: bool) {
        ok = a != 0
        if ok {
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
    pc := compiler_code_ABC(c, act_op, 0, arg_b, arg_c)
    left^ = expr_make_pc(.Pc_Pending_Register, pc)
}

@(private="file")
_arith :: proc(c: ^Compiler, op: Opcode, #no_alias left, right: ^Expr) -> (act_op: Opcode, arg_b: u16, arg_c: u16) {
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

        compiler_pop_reg(c, rb)
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

        // If it doesn't fit in K[C], then we already emitted the instructions
        // needed to load a constant values from the constants table.
        kc := _push_expr_k(c, right, limit=MAX_C) or_break try
        compiler_pop_reg(c, rb)
        return act_op, rb, u16(kc)
    }

    // Neither register-immediate nor register-constant, do register-register.
    rc := compiler_push_expr_any(c, right)

    // Deallocate potenatially temporary registers in the correct order.
    if rb > rc {
        compiler_pop_reg(c, rb)
        compiler_pop_reg(c, rc)
    } else {
        compiler_pop_reg(c, rc)
        compiler_pop_reg(c, rb)
    }
    return op, rb, rc
}

@(private="file")
_check_imm :: proc(e: ^Expr) -> (imm: u16, neg, ok: bool) {
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

compiler_code_compare :: proc(c: ^Compiler, op: Opcode, invert: bool, #no_alias left, right: ^Expr) {
    rb := left.reg
    rc := compiler_push_expr_any(c, right)

    if rb > rc {
        compiler_pop_reg(c, rb)
        compiler_pop_reg(c, rc)
    } else {
        compiler_pop_reg(c, rc)
        compiler_pop_reg(c, rb)
    }

    pc := compiler_code_ABC(c, op, rb, rc, u16(!invert))
    left^ = expr_make_pc(.Compare, pc)
}

compiler_code_concat :: proc(c: ^Compiler, #no_alias left, right: ^Expr) {
    compiler_assert(c, left.type == .Register)
    rb := left.reg

    if right.type == .Pc_Pending_Register {
        ip := &c.chunk.code[right.pc]
        // Recursive case.
        if ip.op == .Concat {
            assert(rb == ip.B - 1)

            // Recursive calls see only 1 temporary register, as our `right`
            // originally held a register but got overwritten in the child
            // recursive call that finished right before us.
            compiler_pop_reg(c, rb)
            ip.B -= 1

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
    pc   := compiler_code_ABC(c, .Concat, 0, rb, rc + 1)
    left^ = expr_make_pc(.Pc_Pending_Register, pc)
}

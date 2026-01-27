#+private package
package lulu

import "core:fmt"
import "core:math"

/*
**Parameters**
- a, b: Must be pointers so that we can check if they are inside the VM
stack or not. This information is useful when reporting errors from local or
global variables.
 */
debug_arith_error :: proc(L: ^State, a, b: ^Value) -> ! {
    culprit := b if value_is_number(a^) else a
    debug_type_error(L, "perform arithmetic on", culprit)
}

/*
Report a runtime error due to attempting `action` on `culprit`.

**Parameters**
- action: `string` representing what operation was supposed to take place.
- culprit: Pointer to register or constant. Pointing to a register is useful
because it allows us to determine the variable type and name, if any.
 */
debug_type_error :: proc(L: ^State, action: string, culprit: ^Value) -> ! {
    frame     := L.frame
    callee    := frame.callee^
    type_name := value_type_name(culprit^)

    // If `culprit` came from the stack, it may come from a variable of some
    // kind. Try to find the variable name if at all possible.
    reg, is_reg := find_ptr_index(frame.registers, culprit);
    with_name: if is_reg && value_is_function(callee) {
        cl := value_get_function(callee)
        if !cl.is_lua {
            break with_name
        }

        buf: [VALUE_TO_STRING_BUFFER_SIZE]byte
        pc := frame.saved_pc
        scope, name, is_var := find_variable(cl.lua.chunk, reg, pc, buf[:]);
        if is_var {
            debug_runtime_error(L, "Attempt to %s %s '%s' (a %s value)",
                action, scope, name, type_name)
        }
    }
    debug_runtime_error(L, "Attempt to %s a %s value", action, type_name)
}

/*
Throws a runtime error and reports an error message.

**Assumptions**
- `L.frame.saved_pc` was set beforehand so we know where to look.
 */
debug_runtime_error :: proc(L: ^State, format := "", args: ..any) -> ! {
    msg     := vm_push_fstring(L, format, ..args)
    frame   := L.frame
    closure := value_get_function(frame.callee^)
    if closure.is_lua {
        chunk := closure.lua.chunk
        file  := chunk_name(chunk)
        // If `frame.saved_pc == -1` then we forgot to protect!
        loc  := chunk.loc[frame.saved_pc]
        line := loc.line
        col  := loc.col
        vm_push_fstring(L, "%s:%i:%i: %s", file, line, col, msg)
    } else {
        vm_push_fstring(L, "[Odin] %s", msg)
    }
    throw_error(L, .Runtime)
}

/*
Throws a runtime error and reports a message in the form `file:line:col: message`.

**Links**
- https://www.gnu.org/prep/standards/standards.html#Errors
 */
debug_syntax_error :: proc(x: ^Lexer, here: Token, msg: string) -> ! {
    L := x.L
    file := ostring_to_string(x.name)
    line := here.line
    col  := here.col
    vm_push_fstring(L, "%s:%i:%i: %s near '%s'", file, line, col, msg, here.lexeme)
    throw_error(L, .Syntax)
}

debug_memory_error :: proc(L: ^State, format: string, args: ..any, loc := #caller_location) -> ! {
    file := loc.file_path
    line := loc.line
    col  := loc.column
    fmt.eprintf("%s:%i:%i: Failed to ", file, line, col, flush=false)
    fmt.eprintfln(format, ..args)
    throw_error(L, .Memory)
}

@(disabled=!DISASSEMBLE)
disassemble :: proc(chunk: ^Chunk) {
    fmt.printfln("[DIASSEMBLY]\n.name: %q", chunk_name(chunk))
    fmt.printfln(".stack_used: %i", chunk.stack_used)

    fmt.println(".locals:")
    if n := len(chunk.locals); n > 0 {
        pad := math.count_digits_of_base(n - 1, base=10)
        for local, i in chunk.locals {
            name := local_name(local)
            born := local.born
            died := local.died
            fmt.printfln("[%0*i] .local '%s' ; .code[%i:%i]", pad, i, name, born, died)
        }
        fmt.println()
    }

    fmt.println(".constants:")
    if n := len(chunk.constants); n > 0 {
        buf: [VALUE_TO_STRING_BUFFER_SIZE]byte
        pad := math.count_digits_of_base(n - 1, base=10)
        for v, i in chunk.constants {
            repr := value_to_string(v, buf[:])
            fmt.printf("[%0*i] .%-8s ", pad, i, value_type_name(v))
            fmt.printfln("%q" if value_is_string(v) else "%s", repr)
        }
        fmt.println()
    }

    fmt.println(".code:")
    pad := math.count_digits_of_base(len(chunk.code) - 1, base=10)
    for i, pc in chunk.code {
        disassemble_at(chunk, i, i32(pc), pad)
    }
    fmt.println()
}

@(disabled=!DISASSEMBLE)
disassemble_at :: proc(chunk: ^Chunk, i: Instruction, pc: i32, pad := 0) {
    _get_reg :: proc(chunk: ^Chunk, reg: u16, pc: i32, buf: []byte) -> string {
        if name, ok := find_local(chunk, reg, pc); ok {
            return name
        } else {
            return fmt.bprintf(buf, "$r%i", reg)
        }
    }

    _get_global :: proc(chunk: ^Chunk, Bx: u32) -> string {
        return value_get_string(chunk.constants[Bx])
    }

    loc := chunk.loc[pc]
    if pc > 0 && chunk.loc[pc - 1].line == loc.line {
        fmt.printf("[%0*i] |--- ", pad, pc)
        // fmt.printf("[%0*i] |------- ", pad, pc)
    } else {
        // left-align
        fmt.printf("[%0*i] %- 4i ", pad, pc, loc.line)

        // buf: [64]byte
        // loc_repr := fmt.bprintf(buf[:], "%i", loc.line)
        // loc_repr := fmt.bprintf(buf[:], "%i:%i", loc.line, loc.col)
        // fmt.printf("[%0*i] %-8s ", pad, pc, loc_repr)
    }

    buf1, buf2, buf3: [VALUE_TO_STRING_BUFFER_SIZE]byte
    op := i.op

    // Argument A is always used for something
    fmt.printf("%-18s % -4i ", op, i.A)

    info := OP_INFO[op]
    switch info.mode {
    case .ABC:  fmt.printf("% -4i % -4i ; ", i.B, i.C)
    case .ABx:  fmt.printf("% -9i ; ", i.u.Bx)
    case .AsBx: fmt.printf("% -9i ; ", i.s.Bx)
    }

    ra := _get_reg(chunk, i.A, pc, buf1[:])
    RA_SLICE_OPS :: bit_set[Opcode]{.Load_Nil, .Call}
    if info.a && op not_in RA_SLICE_OPS {
        fmt.printf("%s := ", ra)
    }

    switch op {
    case .Move:       fmt.print(_get_reg(chunk, i.B, pc, buf1[:]))
    case .Load_Nil:   fmt.printf("$r[%i:%i] := nil", i.A, i.B)
    case .Load_Bool:  fmt.print(bool(i.B))
    case .Load_Imm:   fmt.print(i.u.Bx)
    case .Load_Const:
        v := chunk.constants[i.u.Bx]
        s := value_to_string(v, buf1[:])
        fmt.printf("%q" if value_is_string(v) else "%s", s)

    case .Get_Global: fmt.printf("_G.%s", _get_global(chunk, i.u.Bx))
    case .Set_Global: fmt.printf("_G.%s := %s", _get_global(chunk, i.u.Bx), ra)
    case .New_Table:
        hash_count  := 1 << (i.B - 1) if i.B != 0 else 0
        array_count := 1 << (i.C - 1) if i.C != 0 else 0
        fmt.printf("{{}} ; #hash=%i, #array=%i", hash_count, array_count)
    case .Get_Table:
        // Use separate buffers to avoid aliasing issues
        table_reg := _get_reg(chunk, i.B, pc, buf2[:])
        key_reg   := _get_reg(chunk, i.C, pc, buf3[:])
        fmt.printf("%s[%s]", table_reg, key_reg)

    case .Get_Field:
        // Use separate buffers to avoid aliasing issues
        table_reg := _get_reg(chunk, i.B, pc, buf2[:])
        key       := chunk.constants[i.C]
        key_k     := value_to_string(key, buf3[:])
        form      := "%s[%q]" if value_is_string(key) else "%s[%s]"
        fmt.printf(form, table_reg, key_k)


    case .Set_Table:
        // Use separate buffers to avoid aliasing issues
        key_reg := _get_reg(chunk, i.B, pc, buf2[:])
        val_reg := _get_reg(chunk, i.C, pc, buf3[:])
        fmt.printf("%s[%s] := %s", ra, key_reg, val_reg)


    case .Set_Table_Const:
        key_reg := _get_reg(chunk, i.B, pc, buf2[:])
        val     := chunk.constants[i.C]
        val_reg := value_to_string(val, buf3[:])
        fmt.printf("%s[%s] := ", ra, key_reg)
        fmt.printf("%q" if value_is_string(val) else "%s", val_reg)

    case .Set_Field:
        // Use separate buffers to avoid aliasing issues
        key := chunk.constants[i.B]
        kb  := value_to_string(key, buf2[:])
        rc  := _get_reg(chunk, i.C, pc, buf3[:])
        form := "%s[%q] := %s" if value_is_string(key) else "%s[%s] := %s"
        fmt.printf(form, ra, kb, rc)

    case .Set_Field_Const:
        // Use separate buffers to avoid aliasing issues
        key := chunk.constants[i.B]
        key_reg := value_to_string(key, buf2[:])

        val := chunk.constants[i.C]
        val_reg := value_to_string(val, buf3[:])

        form := "%s[%q] := " if value_is_string(key) else "%s[%s] := "
        fmt.printf(form, ra, key_reg)
        fmt.printf("%q" if value_is_string(val) else "%s", val_reg)

    case .Len..=.Unm:
        rb := _get_reg(chunk, i.B, pc, buf1[:])
        fmt.printf("%s(%s)", _op_string(op), rb)

    case .Add_Imm..=.Sub_Imm:
        rb := _get_reg(chunk, i.B, pc, buf1[:])
        fmt.printf("%s %s %i", rb, _op_string(op), i.C)

    case .Add_Const..=.Pow_Const:
        rb := _get_reg(chunk, i.B, pc, buf1[:])
        kc := value_to_string(chunk.constants[i.C], buf2[:])
        fmt.printf("%s %s %s", rb, _op_string(op), kc)

    case .Add..=.Pow:
        rb := _get_reg(chunk, i.B, pc, buf1[:])
        rc := _get_reg(chunk, i.C, pc, buf2[:])
        fmt.printf("%s %s %s", rb, _op_string(op), rc)

    case .Concat: fmt.printf("concat $r[%i:%i]", i.B, i.C)

    case .Call:
        base_reg  := int(i.A)
        arg_first := base_reg + 1
        arg_count := int(i.B) + VARIADIC
        ret_count := int(i.C) + VARIADIC
        switch ret_count {
        case VARIADIC: fmt.printf("$r[%i:] := ", base_reg)
        case 0:        break
        case 1:        fmt.printf("$r[%i] := ", base_reg)
        case:
            fmt.printf("$r[%i:%i] := ", base_reg, base_reg + ret_count)
        }

        fmt.printf("%s(", ra)
        switch arg_count {
        case VARIADIC: fmt.printf("$r[%i:]", arg_first)
        case 0:        break
        case 1:        fmt.printf("$r[%i]", arg_first)
        case:
            fmt.printf("$r[%i:%i]", arg_first, arg_first + arg_count)
        }
        fmt.print(")")

    case .Jump:
        offset := i.s.Bx
        fmt.printf("goto .code[%i]", pc + 1 + offset)

    case .Jump_If:
        offset := i.s.Bx
        target := pc + 1 + offset
        fmt.printf("if not %s then goto .code[%i]", ra, target)

    case .Return:
        start := int(i.A)
        count := int(i.B) + VARIADIC
        stop  := start + count
        fmt.print("return")
        if count == VARIADIC {
            fmt.printf(" $r[%i:]", start)
        } else if start == stop - 1 {
            fmt.printf(" %s", _get_reg(chunk, u16(start), pc, buf1[:]))
        } else if start != stop {
            fmt.printf(" $r[%i:%i]", start, stop)
        }
    }
    fmt.println()
}

@(private="file")
_op_string :: proc(op: Opcode) -> string {
    #partial switch op {
    // Unary
    case .Len: return "len"
    case .Not: return "not"
    case .Unm: return "-"

    // Arithmetic
    case .Add, .Add_Const, .Add_Imm: return "+"
    case .Sub, .Sub_Const, .Sub_Imm: return "-"
    case .Mul, .Mul_Const:           return "*"
    case .Div, .Div_Const:           return "/"
    case .Mod, .Mod_Const:           return "%"
    case .Pow, .Pow_Const:           return "^"

    // Comparison
    // case .Eq:   return "=="
    // case .Neq:  return "~="
    // case .Lt:   return "<"
    // case .Gt:   return ">"
    // case .Leq:  return "<="
    // case .Geq:  return ">="
    }
    unreachable("Invalid opcode: %v", op)
}

/*
Finds the name of the variable which occupies `reg` during its lifetime
somewhere during `pc`.

**Parameters**
- reg: 0-based index of the register where the desired variable was in.
E.g. the first local variable should always be in register 0.
- pc: The instruction index to check against.

**Analogous to**
- `ldebug.c:getobjname(lua_State *L, CallInfo *ci, int stackpos, const char **name)`
in Lua 5.1.5.
 */
find_variable :: proc(chunk: ^Chunk, #any_int reg, pc: int, buf: []byte) -> (scope, name: string, ok: bool) {
    name, ok = find_local(chunk, reg, pc)
    // Found a local?
    if ok {
        scope = "local"
        return scope, name, ok
    }

    // Probably global, table field or upvalue?
    // Find the instruction which last mutated `reg`.
    i := _symbolic_execute(chunk, reg, pc)
    #partial switch i.op {
    case .Move:
        // R[A] wasn't a local, so maybe R[B] was? e.g `local x = nil; x()`:
        //
        //  Move 1 0 0 ; $r1 = x
        //  Call 1 1 1 ; $r1()
        //
        if i.A > i.B {
            return find_variable(chunk, i.B, pc, buf)
        }

    case .Get_Global:
        scope = "global"
        name  = value_get_string(chunk.constants[i.u.Bx])
        ok    = true

    case .Get_Field:
        scope = "field"
        name  = value_to_string(chunk.constants[i.C], buf)
        ok    = true

    case .Get_Table:
        scope, name, ok = find_variable(chunk, i.C, pc, buf)
        if ok do switch scope {
        case "global": scope = "field from global"
        case "local":  scope = "field from local"
        }
    }
    return scope, name, ok
}

/*
'Symbolic execution' allows us to find the last time `reg` was modified before
the error at `error_pc` was thrown. This, in turn, allows us to determine if the
error occured on a global variable, table field, upvalue, or not a variable.
 */
@(private="file")
_symbolic_execute :: proc(chunk: ^Chunk, reg, error_pc: int) -> (i: Instruction) {
    // Stores index of the last instruction that changed `reg`, initially
    // pointing to the final, neutral return.
    prev_pc := len(chunk.code) - 1

    NEUTRAL_RETURN := Instruction{base={op=.Return, A=0, B=1, C=0}}
    fmt.assertf(instruction_eq(chunk.code[prev_pc], NEUTRAL_RETURN),
        "\nExpected %v but got %v",
        NEUTRAL_RETURN.base, chunk.code[prev_pc].base)

    // TODO(2026-01-03): Verify bytecode correctness?
    for pc := 0; pc < error_pc; pc += 1 {
        i = chunk.code[pc]
        op := i.op

        // This instruction mutates R[A] and `reg` is the destination?
        if OP_INFO[op].a {
            if int(i.A) == reg {
                prev_pc = pc
            }
        }

        #partial switch op {
        case .Jump:
            dst := pc + 1 + int(i.s.Bx)
            // If we perform the jump, we don't skip `error_pc`?
            if pc < dst && dst <= error_pc {
                pc = dst
            }
        }
    }
    return chunk.code[prev_pc]
}


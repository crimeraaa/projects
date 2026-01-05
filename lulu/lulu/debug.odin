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
    if is_reg && value_type(callee) == .Chunk {
        chunk := value_get_chunk(callee)
        if scope, name, is_var := find_variable(chunk, reg, frame.saved_pc); is_var {
            debug_runtime_error(L, "Attempt to %s %s '%s' (a %s value)",
                action, scope, name, type_name)
        }
    }
    debug_runtime_error(L, "Attempt to %s a %s value at reg(%i)", action, type_name, reg)
}

/*
Throws a runtime error and reports an error message.

**Assumptions**
- `L.frame.saved_pc` was set beforehand so we know where to look.
 */
debug_runtime_error :: proc(L: ^State, format := "", args: ..any) -> ! {
    msg      := vm_push_fstring(L, format, ..args)
    frame    := L.frame
    function := frame.callee^
    #partial switch value_type(function) {
    case .Chunk:
        chunk := value_get_chunk(function)
        file := chunk_name(chunk)
        loc  := chunk.loc[frame.saved_pc]
        line := cast(int)loc.line
        col  := cast(int)loc.col
        vm_push_fstring(L, "%s:%i:%i: %s", file, line, col, msg)

    case .Api_Proc:
        vm_push_fstring(L, "[Odin] %s", msg)

    case:
        unreachable()
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
    line := cast(int)here.line
    col  := cast(int)here.col
    vm_push_fstring(L, "%s:%i:%i: %s near '%s'", file, line, col, msg, here.lexeme)
    throw_error(x.L, .Syntax)
}

debug_memory_error :: proc(L: ^State, format: string, args: ..any, loc := #caller_location) -> ! {
    file := loc.file_path
    line := loc.line
    col  := loc.column
    fmt.eprintf("%s:%i:%i Failed to ", file, line, col, flush=false)
    fmt.eprintfln(format, ..args)
    throw_error(L, .Memory)
}

@(disabled=!ODIN_DEBUG)
disassemble :: proc(chunk: ^Chunk) {
    fmt.printfln("[DIASSEMBLY]\n.name: %q", chunk_name(chunk))
    fmt.printfln(".stack_used: %i", chunk.stack_used)

    fmt.println(".locals:")
    if len(chunk.locals) > 0 {
        for local, i in chunk.locals {
            name := local_name(local)
            born := local.birth_pc
            died := local.death_pc
            fmt.printfln("[%i] .local '%s' ; .code[%i:%i]", i, name, born, died)
        }
        fmt.println()
    }

    fmt.println(".constants:")
    if len(chunk.constants) > 0 {
        buf: [VALUE_TO_STRING_BUFFER_SIZE]byte
        for v, i in chunk.constants {
            repr := value_to_string(v, buf[:])
            fmt.printf("[%i] .%-8s ", i, value_type_name(v))
            fmt.printfln("%q" if value_is_string(v) else "%s", repr)
        }
        fmt.println()
    }

    fmt.println(".code:")
    pad := math.count_digits_of_base(len(chunk.code) - 1, base=10)
    for i, pc in chunk.code {
        disassemble_at(chunk, i, pc, pad)
    }
    fmt.println()
}

@(disabled=!ODIN_DEBUG)
disassemble_at :: proc(chunk: ^Chunk, i: Instruction, pc: int, pad := 0) {
    get_reg :: proc(chunk: ^Chunk, reg: u16, pc: int, buf: []byte) -> string {
        if name, ok := find_local(chunk, reg, pc); ok {
            return name
        } else {
            return fmt.bprintf(buf, "$r%i", reg)
        }
    }

    get_global :: proc(chunk: ^Chunk, arg: Arg) -> string {
        return value_get_string(chunk.constants[arg.(Bx)])
    }

    loc := chunk.loc[pc]
    if pc > 0 && chunk.loc[pc - 1] == loc {
        fmt.printf("[%0*i] |------- ", pad, pc)
    } else {
        buf: [64]byte
        loc_repr := fmt.bprintf(buf[:], "%i:%i", loc.line, loc.col)
        fmt.printf("[%0*i] %-8s ", pad, pc, loc_repr)
    }

    buf1, buf2: [VALUE_TO_STRING_BUFFER_SIZE]byte
    op := i.op
    // Register A is always used for something
    fmt.printf("%-12s % -4i ", op, i.a)

    Arg :: union {
        BC, Bx, sBx
    }

    BC  :: struct {b, c: u16}
    Bx  :: u32
    sBx :: i32

    arg: Arg
    info := OP_INFO[op]
    switch info.mode {
    case .ABC:
        arg = BC{i.b, i.c}
        fmt.printf("% -4i % -4i ; ", i.b, i.c)
        // if info.b != nil && info.c == nil {
        //     fmt.printf("% -4i      ; ", i.b)
        // } else {
        //     fmt.printf("% -4i % -4i ; ", i.b, i.c)
        // }

    case .ABx:
        arg = getarg_bx(i)
        fmt.printf("% -9i ; ", arg)

    case .AsBx:
        arg = getarg_bx(i)
        fmt.printf("% -9i ; ", arg)
    }

    ra := get_reg(chunk, i.a, pc, buf1[:])
    if info.a && op != .Call {
        fmt.printf("%s := ", ra)
    }

    switch op {
    case .Move:       fmt.print(get_reg(chunk, arg.(BC).b, pc, buf1[:]))
    case .Load_Nil:   fmt.printf("$r[%i:%i] := nil", i.a, arg.(BC).b)
    case .Load_Bool:  fmt.print(cast(bool)arg.(BC).b)
    case .Load_Imm:   fmt.print(arg.(Bx))
    case .Load_Const:
        v := chunk.constants[arg.(Bx)]
        s := value_to_string(v, buf1[:])
        fmt.printf("%q" if value_is_string(v) else "%s", s)
    case .Get_Global: fmt.printf("_G.%s", get_global(chunk, arg))
    case .Set_Global: fmt.printf("_G.%s := %s", get_global(chunk, arg), ra)
    case .Len..=.Unm:
        rb := get_reg(chunk, arg.(BC).b, pc, buf1[:])
        fmt.printf("%s%s", _op_string(op), rb)

    case .Add_Imm..=.Sub_Imm:
        rb := get_reg(chunk, arg.(BC).b, pc, buf1[:])
        fmt.printf("%s %s %i", rb, _op_string(op), arg.(BC).c)

    case .Add_Const..=.Pow_Const:
        rb := get_reg(chunk, arg.(BC).b, pc, buf1[:])
        kc := value_to_string(chunk.constants[arg.(BC).c], buf2[:])
        fmt.printf("%s %s %s", rb, _op_string(op), kc)

    case .Add..=.Pow:
        rb := get_reg(chunk, arg.(BC).b, pc, buf1[:])
        rc := get_reg(chunk, arg.(BC).c, pc, buf2[:])
        fmt.printf("%s %s %s", rb, _op_string(op), rc)

    case .Concat: fmt.printf("concat $r[%i:%i]", i.b, i.c)

    case .Call:
        base_reg  := cast(int)i.a
        arg_first := base_reg + 1
        arg_count := cast(int)arg.(BC).b
        ret_count := cast(int)arg.(BC).c
        if ret_count > 0 {
            fmt.printf("$r[%i:%i] := ", base_reg, base_reg + ret_count)
        }
        fmt.printf("%s($r[%i:%i])", ra, arg_first, arg_first + arg_count)

    case .Return:
        start := i.a
        stop  := start + arg.(BC).b
        if start == stop - 1 {
            fmt.printf("return %s", get_reg(chunk, start, pc, buf1[:]))
        } else {
            fmt.printf("return $r[%i:%i]", start, stop)
        }
    }
    fmt.println()
}

@(private="file")
_op_string :: proc(op: Opcode) -> string {
    #partial switch op {
    // Unary
    case .Len: return "#"
    case .Not: return "not "
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
find_variable :: proc(chunk: ^Chunk, #any_int reg, pc: int) -> (scope, name: string, ok: bool) {
    name, ok = find_local(chunk, reg, pc)
    // Found a local?
    if ok {
        scope = "local"
    } // Probably global, table field or upvalue?
    else {
        // Find the instruction which last mutated `reg`.
        i := _symbolic_execute(chunk, reg, pc)
        #partial switch i.op {
        case .Move:
            // R[A] wasn't the culprit, so maybe R[B] was?
            if i.a > i.b {
                return find_variable(chunk, i.b, pc)
            }

        case .Get_Global:
            scope = "global"
            name  = value_get_string(chunk.constants[getarg_bx(i)])
            ok    = true
        }
    }
    return scope, name, ok
}

/*
'Symbolic execution' allows us to find the last time `reg` was modified before
the error at `pc` was thrown. This, in turn, allows us to determine if the
error occured on a global variable, table field, upvalue, or not a variable.
 */
@(private="file")
_symbolic_execute :: proc(chunk: ^Chunk, reg, last_pc: int) -> (i: Instruction) {
    // Stores index of the last instruction that changed `reg`, initially
    // pointing to the final, neutral return.
    prev_pc := len(chunk.code) - 1

    NEUTRAL_RETURN := Instruction{op=.Return, a=0, b=0, c=0}
    assert(chunk.code[prev_pc] == NEUTRAL_RETURN, "Expected %v but got %v",
        NEUTRAL_RETURN, chunk.code[prev_pc])

    // TODO(2026-01-03): Verify bytecode correctness? Execute jumps?
    for pc in 0..<last_pc {
        i = chunk.code[pc]
        op := i.op

        // This instruction mutates R[A] and `reg` is the destination?
        if OP_INFO[op].a {
            if cast(int)i.a == reg {
                prev_pc = pc
            }
        }
    }
    return chunk.code[prev_pc]
}


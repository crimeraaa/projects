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
debug_arith_error :: proc(L: ^VM, a, b: ^Value) -> ! {
    culprit := b if value_is_number(a^) else a
    debug_type_error(L, "perform arithmetic on", culprit)
}

debug_type_error :: proc(L: ^VM, action: string, culprit: ^Value) -> ! {
    frame := &L.frame    
    chunk := frame.chunk

    // Check if `culprit` is in the stack
    reg := -1
    for &v, i in frame.stack {
        if &v == culprit {
            reg = i
            break
        }
    }
    
    type_name := value_type_name(culprit^)
    if reg != -1 {
        local_name, ok := find_local_at(chunk, reg, frame.saved_pc)
        if ok {
            debug_runtime_error(L, "Attempt to %s local '%s' (a %s value)",
                action, local_name, type_name)
        }
    }
    debug_runtime_error(L, "Attempt to %s a %s value", action, type_name)
}

/* 
Throws a runtime error and reports an error message.

**Assumptions**
- `L.frame.saved_pc` was set beforehand so we know where to look.
 */
debug_runtime_error :: proc(L: ^VM, format := "", args: ..any) -> ! {
    frame := L.frame
    chunk := frame.chunk
    file  := ostring_to_string(chunk.name)
    line  := chunk.lines[frame.saved_pc]
    fmt.eprintf("%s:%i: ", file, line)
    fmt.eprintfln(format, ..args)
    vm_throw(L, .Runtime)
}

@(disabled=!ODIN_DEBUG)
disassemble :: proc(chunk: ^Chunk) {
    fmt.printfln("[DIASSEMBLY]\n.name: %q", ostring_to_string(chunk.name))
    fmt.printfln(".stack_used: %i\n", chunk.stack_used)

    fmt.println(".locals:")
    if len(chunk.locals) > 0 {
        for v, i in chunk.locals {
            name := ostring_to_string(v.name)
            born := v.birth_pc
            died := v.death_pc
            fmt.printfln("[%i] .local '%s' ; .code[%i:%i]", i, name, born, died)
        }
        fmt.println()
    }

    fmt.println(".constants:")
    if len(chunk.constants) > 0 {
        buf: [VALUE_TO_STRING_BUFFER_SIZE]byte
        for v, i in chunk.constants {
            repr := value_to_string(v, buf[:])
            fmt.printfln("[%i] .%-8s %s ; k%i", i, value_type_name(v), repr, i)
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
        if name, ok := find_local_at(chunk, cast(int)reg, pc); ok {
            return name
        } else {
            return fmt.bprintf(buf, "$r%i", reg)
        }
    }
    
    get_const :: proc(chunk: ^Chunk, arg: Arg, buf: []byte) -> (repr: string) {
        return value_to_string(chunk.constants[arg.(Bx)], buf)
    }
    
    get_global :: proc(chunk: ^Chunk, arg: Arg) -> string {
        return value_get_string(chunk.constants[arg.(Bx)])
    }
    
    line := chunk.lines[pc]
    if pc > 0 && chunk.lines[pc - 1] == line {
        fmt.printf("[%0*i] |--- ", pad, pc)
    } else {
        fmt.printf("[%0*i] % -4i ", pad, pc, line)
    }

    a_buf, b_buf, c_buf: [VALUE_TO_STRING_BUFFER_SIZE]byte
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

    ra := get_reg(chunk, i.a, pc, a_buf[:])
    if info.a {
        fmt.printf("%s := ", ra)
    }

    switch op {
    case .Move:
        rb := get_reg(chunk, arg.(BC).b, pc, b_buf[:])
        fmt.print(rb)

    case .Load_Nil:
        for reg in i.a..<arg.(BC).b {
            // First iteration vs. subsequent iterations.
            form := "%s" if reg == i.a else ", %s"
            fmt.printf(form, get_reg(chunk, reg, pc, a_buf[:]))
        }
        fmt.print(" := nil")

    case .Load_Bool:  fmt.print(cast(bool)arg.(BC).b)
    case .Load_Imm:   fmt.print(arg.(Bx))
    case .Load_Const: fmt.print(get_const(chunk, arg, b_buf[:]))
    case .Get_Global: fmt.printf("_G.%s", get_global(chunk, arg))
    case .Set_Global: fmt.printf("_G.%s := %s", get_global(chunk, arg), ra)

    case .Len..=.Unm:
        rb := get_reg(chunk, arg.(BC).b, pc, b_buf[:])
        fmt.printf("%s%s", op_string(op), rb)
        
    case .Add_Imm..=.Sub_Imm:
        rb := get_reg(chunk, arg.(BC).b, pc, b_buf[:])
        fmt.printf("%s %s %i", rb, op_string(op), arg.(BC).c)
    
    case .Add_Const..=.Pow_Const:
        rb := get_reg(chunk, arg.(BC).b, pc, b_buf[:])
        kc := value_to_string(chunk.constants[arg.(BC).c], c_buf[:])
        fmt.printf("%s %s %s", rb, op_string(op), kc)

    case .Add..=.Pow:
        rb := get_reg(chunk, arg.(BC).b, pc, b_buf[:])
        rc := get_reg(chunk, arg.(BC).c, pc, c_buf[:])
        fmt.printf("%s %s %s", rb, op_string(op), rc)

    case .Return:
        fmt.print("return")
        start := i.a
        stop  := start + arg.(BC).b
        for reg in start..<stop {
            form := " %s" if reg == start else ", %s"
            fmt.printf(form, get_reg(chunk, reg, pc, a_buf[:]))
        }
    }
    fmt.println()
}

/* 
Finds the name of the local which occupies `reg` during its lifetime
somewhere along `pc`.

**Parameters**
- reg: 0-based index. E.g. the first local should have register 0.
- pc: The instruction index to check against.
 */
find_local_at :: proc(c: ^Chunk, reg, pc: int) -> (name: string, ok: bool) {
    // Convert to 1-based index for quick comparison to zero.
    counter := reg + 1
    for local in c.locals {
        // This local, and all locals succeeding it, are all beyond the lifetime
        // of `pc`?
        if local.birth_pc > pc {
            break
        }

        // Local is alive at some point in `pc`?
        if pc < local.death_pc {
            // Correct scope, keep going
            counter -= 1
            
            // Found the exact local, in scope, we are looking for?
            if counter == 0 {
                return ostring_to_string(local.name), true
            }
        }
    }
    return {}, false
}

@(private="file")
op_string :: proc(op: Opcode) -> string {
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


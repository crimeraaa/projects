#+private package
package lulu

import "core:fmt"
import "core:mem"
import "core:math"

Chunk :: struct {
    using base: Object_Header,

    // File name or stream name.
    name: ^Ostring,

    // Up to how many stack slots are actively used at most.
    stack_used: int,

    // List of all possible locals for this chunk.
    locals: [dynamic]Local,

    // Constant values that are referred to within this compiled chunk.
    constants: [dynamic]Value,

    // List of all instructions to be executed.
    code: []Instruction,

    // Maps each index in `code` to its corresponding line.
    lines: []int,
}

/* 
Local variables have predetermined lifetimes. That is, they go into scope
and go out of scope at known points in the program. The lifetime is given
by the half-open range (in terms of program counter indexes)
`[birth_pc, death_pc)`.
 */
Local :: struct {
    name: ^Ostring, 
    
    // Inclusive start index of the instruction in the parent chunk where this
    // local is first valid (i.e. it first comes into scope).
    birth_pc: int,
    
    // Exclusive stop index of the instruction in the parent chunk where this
    // local is last valid (i.e. it finally goes out of scope).
    death_pc: int,
}

/*
Creates a new blank chunk for use when parsing.

*Allocates using `context.allocator`.*

**Assumptions**
- We are in a protected call, so we are able to catch out-of-memory errors
within `object_new()`.
 */
chunk_new :: proc(L: ^VM, name: ^Ostring) -> ^Chunk {
    g := G(L)
    c := object_new(Chunk, L, &g.objects)
    c.name = name
    // Minimum stack usage is 2 to allow all instructions to unconditionally
    // read r0 and r1.
    c.stack_used = 2
    return c
}

/* 
'Fixes' the chunk `c` by shrinking its dynamic arrays to the exact size so that
we can query the last program counter by just getting the length of the code
array for example.

*Allocates using `context.allocator`.*
 */
chunk_fix :: proc(L: ^VM, c: ^Chunk, pc: int) {
    slice_resize(L, &c.code,  pc)
    slice_resize(L, &c.lines, pc)
}

/*
Frees the chunk contents and the chunk pointer itself.

*Deallocates using `context.allocator`.*
 */
chunk_free :: proc(c: ^Chunk) {
    delete(c.locals)
    delete(c.constants)
    delete(c.code)
    delete(c.lines)
    mem.free(c)
}

/*
Adds `i` to the end of the code array.

*Allocates using `context.allocator`.*

**Assumptions**
- We are in a protected call, so failures to append code can be caught
and handled.
 */
chunk_push_code :: proc(L: ^VM, c: ^Chunk, pc: int, i: Instruction, line: int) {
    slice_insert(L, &c.code,  pc, i)
    slice_insert(L, &c.lines, pc, line)
    
}

/*
Adds `v` to the end of the constants array.

*Allocates using `context.allocator`.*

**Assumptions**
- We are in a protected call, so failures to append values can be caught
and handled.
 */
chunk_push_constant :: proc(L: ^VM, c: ^Chunk, v: Value) -> (index: u32) {
    index   = cast(u32)len(c.constants)
    _, err := append(&c.constants, v)
    if err != nil {
        vm_error_memory(L)
    }
    return index
}

chunk_disassemble :: proc(c: ^Chunk) {
    fmt.printfln("[DIASSEMBLY]\n.name: %q", ostring_to_string(c.name))
    fmt.printfln(".stack_used: %i\n", c.stack_used)

    if len(c.locals) > 0 {
        fmt.printfln(".locals:")
        for v, i in c.locals {
            name := ostring_to_string(v.name)
            born := v.birth_pc
            died := v.death_pc
            fmt.printfln("[%i] .local '%s' ; .code[%i:%i]", i, name, born, died)
        }
    }

    if len(c.constants) > 0 {
        fmt.printfln("\n.constants:")
        for v, i in c.constants {
            fmt.printf("[%i] .%-8s ", i, value_type_name(v))
            value_print(v)
            fmt.printfln(" ; k%i", i)
        }
    }

    fmt.println("\n.code:")
    pad := math.count_digits_of_base(len(c.code) - 1, base=10)
    for i, pc in c.code {
        chunk_disassemble_at(c, i, pc, pad)
    }
    fmt.println()
}

chunk_disassemble_at :: proc(c: ^Chunk, i: Instruction, pc: int, pad := 0) {
    print_abc :: proc(i: Instruction, extra := "", args: ..any) {
        fmt.printf("% -4i % -4i % -4i ; ", i.a, i.b, i.c)
        if extra != "" {
            fmt.printf(extra, ..args)
        }
    }

    print_abx :: proc(i: Instruction, extra: string, args: ..any) {
        bx := get_bx(i)
        fmt.printf("% -4i % -9i ; ", i.a, bx)
        if extra != "" {
            fmt.printf(extra, ..args)
        }
    }
    
    get_rk :: proc(c: ^Chunk, reg: u16, pc: int, buf: []byte) -> string {
        if reg_is_k(reg) {
            index    := reg_get_k(reg)
            constant := c.constants[index]
            return value_write_string(constant, buf)
        }
        return get_reg(c, reg, pc, buf)
    }
    
    get_reg :: proc(c: ^Chunk, reg: u16, pc: int, buf: []byte) -> string {
        if name, ok := find_local_by_pc(c, cast(int)reg, pc); ok {
            return name
        }
        return fmt.bprintf(buf, "r%i", reg)
    }
    
    get_kbx :: proc(c: ^Chunk, i: Instruction) -> Value {
        return c.constants[get_bx(i)]
    }
    
    line := c.lines[pc]
    if pc > 0 && c.lines[pc - 1] == line {
        fmt.printf("[%0*i] |--- ", pad, pc)
    } else {
        fmt.printf("[%0*i] % -4i ", pad, pc, line)
    }

    a_buf, b_buf, c_buf: [64]byte
    ra := get_reg(c, i.a, pc, a_buf[:])
    fmt.printf("%-12s", i.op)
    switch i.op {
    case .Move:
        rb := get_reg(c, i.b, pc, b_buf[:])
        print_abc(i, "%s := %s", ra, rb)

    case .Load_Nil:
        print_abc(i)
        for reg in i.a..<i.b {
            if reg > i.a {
                fmt.print(", ")
            }
            s := get_reg(c, reg, pc, a_buf[:])
            fmt.print(s)
        }
        fmt.print(" := nil")

    case .Load_Bool:
        print_abc(i, "%s := %v", ra, cast(bool)i.b)
    
    case .Load_Const:
        kbx := value_write_string(get_kbx(c, i), b_buf[:])
        print_abx(i, "%s := %s", ra, kbx)

    case .Get_Global:
        kbx := value_to_string(get_kbx(c, i))
        print_abx(i, "%s := _G.%s", ra, kbx)

    case .Set_Global:
        kbx := value_to_string(get_kbx(c, i))
        print_abx(i, "_G.%s := %s", kbx, ra)

    case .Len..=.Unm:
        rb := get_reg(c, i.b, pc, b_buf[:])
        print_abc(i, "%s := %s%s", ra, op_string(i.op), rb)

    case .Add..=.Pow, .Eq..=.Geq:
        rkb := get_rk(c, i.b, pc, b_buf[:])
        rkc := get_rk(c, i.c, pc, c_buf[:])
        print_abc(i, "%s := %s %s %s", ra, rkb, op_string(i.op), rkc)

    case .Return:
        print_abc(i, "return r%i..<r%i", i.a, i.a + i.b)
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
find_local_by_pc :: proc(c: ^Chunk, reg, pc: int) -> (name: string, ok: bool) {
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
    case .Len:  return "#"
    case .Not:  return "not "
    case .Unm:  return "-"

    // Arithmetic
    case .Add:  return "+"
    case .Sub:  return "-"
    case .Mul:  return "*"
    case .Div:  return "/"
    case .Mod:  return "%"
    case .Pow:  return "^"

    // Comparison
    case .Eq:   return "=="
    case .Neq:  return "~="
    case .Lt:   return "<"
    case .Gt:   return ">"
    case .Leq:  return "leq"
    case .Geq:  return ">="
    }
    unreachable()
}


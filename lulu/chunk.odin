#+private package
package lulu

import "core:fmt"
import "core:mem"
import "core:math"

Chunk :: struct {
    using base: Object_Header,

    // File name or stream name.
    name: string,

    // Up to how many stack slots are actively used at most.
    stack_used: int,

    // Constant values that are referred to within this compiled chunk.
    constants: [dynamic]Value,

    // List of all instructions to be executed.
    code: [dynamic]Instruction,

    // Maps each index in `code` to its corresponding line.
    lines: [dynamic]int,
}

/*
Creates a new blank chunk for use when parsing.

*Allocates using `context.allocator`.*

**Assumptions**
- We are in a protected call, so we are able to catch out-of-memory errors
within `object_new()`.
 */
chunk_new :: proc(L: ^VM, name: string) -> ^Chunk {
    g := G(L)
    c := object_new(Chunk, L, &g.objects)
    c.name = name
    // Minimum stack usage is 2 to allow all instructions to unconditionally
    // read r0 and r1.
    c.stack_used = 2
    return c
}

/*
Frees the chunk contents and the chunk pointer itself.

*Deallocates using `context.allocator`.*
 */
chunk_free :: proc(c: ^Chunk) {
    delete(c.code)
    delete(c.constants)
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
chunk_push_code :: proc(L: ^VM, c: ^Chunk, i: Instruction, line: int) -> (pc: int) {
    pc = len(c.code)
    _, err1 := append(&c.code,  i)
    _, err2 := append(&c.lines, line)
    if err1 != nil || err2 != nil {
        vm_error_memory(L)
    }
    return pc
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
    fmt.printfln("[DIASSEMBLY]\n.name: %q\n.stack_used: %i\n.constants:", c.name, c.stack_used)
    for v, i in c.constants {
        fmt.printf("[%i] .%-8s ", i, value_type_name(v))
        value_print(v)
        fmt.printfln(" ; k%i", i)
    }
    fmt.println("\n.code:")

    pad := math.count_digits_of_base(len(c.code) - 1, base=10)
    for i, pc in c.code {
        chunk_disassemble_at(c, i, pc, pad)
    }
    fmt.println()
}

chunk_disassemble_at :: proc(c: ^Chunk, i: Instruction, pc: int, pad := 0) {
    line := c.lines[pc]
    if pc > 0 && c.lines[pc - 1] == line {
        fmt.printf("[%0*i] |--- ", pad, pc)
    } else {
        fmt.printf("[%0*i] % -4i ", pad, pc, line)
    }

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

    fmt.printf("%-16s", i.op)
    switch i.op {
    case .Move:      print_abc(i, "r%i := r%i", i.a, i.b)
    case .Load_Nil:  print_abc(i, "r%i..<r%i := nil", i.a, i.b)
    case .Load_Bool: print_abc(i, "r%i := %v", i.a, cast(bool)i.b)
    case .Load_Const:
        print_abx(i, "r%i := ", i.a)
        index := get_bx(i)
        value := c.constants[index]
        value_print(value)

    // Unary
    case .Len..=.Unm: print_abc(i, "r%i := %sr%i", i.a, op_string(i.op), i.b)

    // Binary
    case .Add..=.Pow, .Eq..=.Geq:
        get_rk :: proc(reg: u16, buf: []byte) -> string {
            if reg_is_k(reg) {
                k := reg_get_k(reg)
                return fmt.bprintf(buf, "k%i", k)
            }
            return fmt.bprintf(buf, "r%i", reg)
        }

        // Assumed to be more than enough for the worst cases: r255 or k255.
        b_buf, c_buf: [RK_SIZE]byte
        b := get_rk(i.b, b_buf[:])
        c := get_rk(i.c, c_buf[:])
        print_abc(i, "r%i := %s %s %s", i.a, b, op_string(i.op), c)

    case .Return:
        print_abc(i, "return r%i..<r%i", i.a, i.b)
    }
    fmt.println()
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


#+private package
package lulu

import "core:fmt"
import "core:mem"

Chunk :: struct {
    using base: Object_Header,
    code:      [dynamic]Instruction,
    lines:     [dynamic]int,
    constants: [dynamic]Value,

    // File name or stream name.
    name: string,
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
    fmt.printfln("[DISASSEMBLY] %s", c.name)
    for i, pc in c.code {
        chunk_disassemble_at(c, i, pc)
    }
}

chunk_disassemble_at :: proc(c: ^Chunk, i: Instruction, pc: int) {
    line := c.lines[pc]
    if pc > 0 && c.lines[pc - 1] == line {
        fmt.printf("%4i |--- ", pc)
    } else {
        fmt.printf("%4i % -4i ", pc, line)
    }

    print_abc :: proc(i: Instruction, extra := "", args: ..any) {
        fmt.printf("% -4i % -4i % -4i ; ", i.a, i.b, i.c, flush=false)
        if extra != "" {
            fmt.printf(extra, ..args, flush=false)
        }
    }

    print_abx :: proc(i: Instruction, extra: string, args: ..any) {
        bx := instruction_get_bx(i)
        fmt.printf("% -4i % -9i ; ", i.a, bx, flush=false)
        if extra != "" {
            fmt.printf(extra, ..args, flush=false)
        }
    }

    fmt.printf("%-16s", i.op, flush=false)
    switch i.op {
    case .Move:
        print_abc(i, "r%i := r%i", i.a, i.b)

    case .Load_Bool:
        print_abc(i, "r%i := %v", i.a, cast(bool)i.b)

    case .Load_Const:
        print_abx(i, "r%i := ", i.a)
        index := instruction_get_bx(i)
        v     := c.constants[index]
        value_print(v, flush=false)

    // Unary
    case .Len..=.Unm:
        print_abc(i, "r%i := %v r%i", i.a, i.op, i.b)

    // Binary
    case .Add..=.Pow, .Eq..=.Geq:
        print_abc(i, "r%i := %v r%i, r%i", i.a, i.op, i.b, i.c)

    case .Return:
        print_abc(i, "return r%i, ..., r%i", i.a, i.b)
    }
    fmt.println()
}

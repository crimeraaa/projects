#+private package
package lulu

import "core:mem"

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

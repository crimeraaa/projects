#+private package
package lulu

Chunk :: struct {
    using base: Object_Header,

    // File name or stream name.
    name: ^Ostring,

    // Up to how many stack slots are actively used at most.
    stack_used: int,

    // List of all possible locals for this chunk.
    locals: []Local_Info,

    // Constant values that are referred to within this compiled chunk.
    constants: []Value,

    // List of all instructions to be executed.
    code: []Instruction,

    // Maps each index in `code` to its corresponding line and column.
    loc: []Location_Info,
}

Location_Info :: struct {
    line, col: i32,
}

/*
Local variables have predetermined lifetimes. That is, they go into scope
and go out of scope at known points in the program. The lifetime is given
by the half-open range (in terms of program counter indexes)
`[birth_pc, death_pc)`.
 */
Local_Info :: struct {
    name: ^Ostring,

    // Inclusive start index of the instruction in the parent chunk where this
    // local is first valid (i.e. it first comes into scope).
    birth_pc: int,

    // Exclusive stop index of the instruction in the parent chunk where this
    // local is last valid (i.e. it finally goes out of scope).
    death_pc: int,
}

local_name :: proc(var: Local_Info) -> string {
    return ostring_to_string(var.name)
}

chunk_name :: proc(c: ^Chunk) -> string {
    return ostring_to_string(c.name)
}

/*
Creates a new blank chunk for use when parsing.

*Allocates using `context.allocator`.*

**Assumptions**
- We are in a protected call, so we are able to catch out-of-memory errors
within `object_new()`.
 */
chunk_new :: proc(L: ^State, name: ^Ostring) -> ^Chunk {
    c := object_new(Chunk, L, &L.global_state.objects)
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
chunk_fix :: proc(L: ^State, c: ^Chunk, #any_int pc, local_count, constant_count: int) {
    resize_slice(L, &c.code,      pc)
    resize_slice(L, &c.loc,       pc)
    resize_slice(L, &c.constants, constant_count)
    resize_slice(L, &c.locals,    local_count)
}

/*
Frees the chunk contents and the chunk pointer itself.

*Deallocates using `context.allocator`.*
 */
chunk_free :: proc(L: ^State, c: ^Chunk) {
    delete(c.locals)
    delete(c.constants)
    delete_slice(L, c.code)
    delete_slice(L, c.loc)
    free(L, c)
}

/*
Adds `i` to the end of the code array.

*Allocates using `context.allocator`.*

**Assumptions**
- We are in a protected call, so failures to append code can be caught
and handled.
 */
chunk_push_code :: proc(L: ^State, c: ^Chunk, pc: ^int, i: Instruction, line, col: i32) -> int {
    insert_slice(L, &c.code, pc^, i)
    insert_slice(L, &c.loc, pc^, Location_Info{line=line, col=col})
    pc^ += 1
    return pc^ - 1
}

/*
Adds `v` to the end of the constants array.

*Allocates using `context.allocator`.*

**Assumptions**
- We are in a protected call, so failures to append values can be caught
and handled.
 */
chunk_push_constant :: proc(L: ^State, c: ^Chunk, count: ^u32, v: Value) -> (index: u32) {
    insert_slice(L, &c.constants, count^, v)
    count^ += 1
    return count^ - 1
}

/*
Appends the local variable information `local` to the chunk's locals array.
 */
chunk_push_local :: proc(L: ^State, c: ^Chunk, count: ^u16, local: Local_Info) -> (index: u16) {
    insert_slice(L, &c.locals, count^, local)
    count^ += 1
    return count^ - 1
}

/*
Finds the name of the local which occupies `reg` during its lifetime
somewhere along `pc`.

**Parameters**
- reg: 0-based index. E.g. the first local should have register 0.
- pc: The instruction index to check against.

**Analogous to**
- `luaF_getlocalname(const Proto *f, int local_number, int pc)` in Lua 5.1.5.
 */
find_local :: proc(chunk: ^Chunk, #any_int reg, pc: int) -> (name: string, ok: bool) {
    // Convert to 1-based index for quick comparison to zero.
    counter := reg + 1
    for local in chunk.locals {
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
                return local_name(local), true
            }
        }
    }
    return {}, false
}

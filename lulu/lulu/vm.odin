#+private package
package lulu

import "base:intrinsics"
import "core:fmt"
import "core:c/libc"
import "core:slice"
import "core:strconv"

VM :: struct {
    // Shared state across all VM instances.
    global_state: ^Global_State,
    
    // Hash table of all defined global variables.
    globals_table: ^Table,

    // Stack-allocated linked list of error handlers.
    handler: ^Error_Handler,

    frame: Frame,
    stack: [16]Value,
}

Global_State :: struct {
    // Hash table of all interned strings.
    intern: Intern,

    // Singly linked list of all possibly-collectable objects across all
    // VM states.
    objects: ^Object_List,
}

Frame :: struct {
    chunk:  ^Chunk,

    // Index of instruction where we left off (e.g. if we dispatch a Lua
    // function call).
    saved_pc: int,

    // Window into VM's primary stack.
    stack: []Value,

    // Chunk's constants array, inlined to reduce pointer dereferences.
    constants: []Value,
}

Error_Handler :: struct {
    buffer: libc.jmp_buf,
    code:   Error,
    prev:  ^Error_Handler,
}

Error :: enum {
    // No error occured.
    Ok,

    // Invalid token or semantically invalid sequence of tokens, was received.
    // This error is often easy to recover from.
    Syntax,

    // Some operation or a function call failed.
    // This error is slightly difficult to recover from, but manageable.
    Runtime,

    // Failed to allocate or reallocate some memory.
    // This error is often fatal. It is extremely difficult to recover from.
    Memory,
}

G :: proc(L: ^VM) -> ^Global_State {
    return L.global_state
}

vm_init :: proc(L: ^VM) -> (ok: bool) {
    err := vm_run_protected(L, required_allocations)
    return err == nil
}

@(private="file")
required_allocations :: proc(L: ^VM, _: rawptr) {
    g := G(L)
    
    // Ensure that the globals table is of some non-zero minimum size.
    t := table_new(L, 32)
    L.globals_table = t

    // Ensure that, when we start interning strings, we already have
    // valid indexes.
    intern_resize(L, &g.intern, 32)
    s := ostring_new(L, "out of memory")
    s.mark += {.Fixed}
    for kw_type in Token_Type.And..=Token_Type.While {
        kw := token_string(kw_type)
        s   = ostring_new(L, kw)
        s.kw_type = kw_type
        s.mark   += {.Fixed}
    }
}

vm_destroy :: proc(L: ^VM) {
    g := G(L)
    intern_destroy(L, &g.intern)
    object_free_all(L, g.objects)
}

/*
Run the procedure `p` in "protected mode", i.e. when an error is thrown
we are able to catch it safely.

**Parameters**
- p: The procedure to be run.
- ud: Arbitrary user-defined data for `p`.

**Guarantees**
- `p` is only ever called with the `ud` it was passed with.
 */
vm_run_protected :: proc(L: ^VM, p: proc(^VM, rawptr), ud: rawptr = nil) -> Error {
    // Push new error handler.
    h: Error_Handler
    h.prev    = L.handler
    L.handler = &h

    if libc.setjmp(&L.handler.buffer) == 0 {
        p(L, ud)
    }

    // Restore old error handler.
    L.handler = h.prev
    return intrinsics.volatile_load(&h.code)
}

vm_throw :: proc(L: ^VM, code: Error) -> ! {
    h := L.handler
    // Unprotected call?
    if h == nil {
        panic("lulu panic: unprotected call")
    }
    intrinsics.volatile_store(&h.code, code)
    libc.longjmp(&h.buffer, 1)
}

vm_error_memory :: proc(L: ^VM) -> ! {
    vm_throw(L, .Memory)
}

@(private="file")
get_rk :: #force_inline proc(reg: u16, f: ^Frame) -> ^Value {
    if reg_is_k(reg) {
        i := reg_get_k(reg)
        return &f.constants[i]
    }
    return &f.stack[reg]
}

@(private="file")
get_rkb_rkc :: #force_inline proc(i: Instruction, f: ^Frame) -> (b, c: ^Value) {
    b = get_rk(i.b, f)
    c = get_rk(i.c, f)
    return b, c
}

@(private="file")
vm_to_number :: proc(v: Value) -> (n: f64, ok: bool) {
    if value_is_number(v) {
        return value_to_number(v), true
    } else if !value_is_string(v) {
        return 0, false
    }

    s := value_to_string(v)
    // Maybe an integer?
    try: if len(s) > 2 && s[0] == '0' {
        base := 0
        switch s[1] {
        case 'b', 'B': base = 2
        case 'd', 'D': base = 10
        case 'o', 'O': base = 8
        case 'x', 'X': base = 16
        case 'z', 'Z': base = 12
        case:
            break try
        }

        i: uint
        i, ok = strconv.parse_uint(s[2:], base)
        if ok {
            n = cast(f64)i
        }
        return n, ok
    }
    return strconv.parse_f64(s)
}

@(private="file")
arith :: #force_inline proc(L: ^VM,
    pc: int,
    ra: ^Value,
    op: proc "contextless" (a, b: f64) -> f64,
    rkb, rkc: ^Value)
{
    try: {
        left   := vm_to_number(rkb^) or_break try
        right  := vm_to_number(rkc^) or_break try
        ra^     = value_make(op(left, right))
        return
    }
    protect(L, pc)
    debug_arith_error(L, rkb, rkc)
}

@(private="file")
protect :: proc(L: ^VM, pc: int) {
    L.frame.saved_pc = pc
}

vm_execute :: proc(L: ^VM, chunk: ^Chunk) {
    L.frame = Frame{chunk, 0, L.stack[:chunk.stack_used], chunk.constants[:]}
    code   := raw_data(chunk.code)
    ip     := code
    frame  := &L.frame
    
    // Table of defined global variables.
    globals := L.globals_table

    // Registers array.
    registers := frame.stack
    
    // Constants array.
    constants := frame.constants

    // Zero-initalize the stack frame.
    for &v in registers {
        v = value_make()
    }

    when ODIN_DEBUG do fmt.println("[EXECUTION]")
    for {
        i  := ip[0]
        pc := intrinsics.ptr_sub(ip, code)
        when ODIN_DEBUG {
            for value, reg in registers {
                fmt.printf("\tr%i := ", reg)
                value_print(value)
                name, ok := find_local_by_pc(chunk, reg, pc)
                if ok {
                    fmt.printfln("; local %s", name)
                } else {
                    fmt.println()
                }
            }
            chunk_disassemble_at(chunk, i, pc)
        }

        // ip++
        ip = &ip[1]
        ra := &registers[i.a]
        switch i.op {
        case .Move:
            rb := registers[i.b]
            ra^ = rb

        case .Load_Nil:
            n   := cast(int)(i.b - i.a)
            dst := slice.from_ptr(ra, n)
            for &v in dst {
                v = value_make()
            }

        case .Load_Bool:
            b := cast(bool)i.b
            ra^ = value_make(b)

        case .Load_Const:
            index := get_bx(i)
            value := constants[index]
            ra^ = value

        case .Get_Global:
            index := get_bx(i)
            key   := constants[index]
            value, ok := table_get(globals, key)
            if !ok {
                what := value_to_string(key)
                protect(L, pc)
                debug_runtime_error(L, "Attempt to read undefined global '%s'", what)
            }
            ra^ = value;

        case .Set_Global:
            index := get_bx(i)
            key   := constants[index]
            table_set(L, globals, key)^ = ra^

        // Unary
        case .Len:
            rb := &registers[i.b]
            if !value_is_string(rb^) {
                what := value_type_name(rb^)
                protect(L, pc)
                debug_type_error(L, "get length of", rb)
            }
            n := value_to_ostring(rb^).len
            ra^ = value_make(cast(f64)n)

        case .Not:
            rb    := registers[i.b]
            not_b := value_is_falsy(rb)
            ra^    = value_make(not_b)

        case .Unm:
            rb := &registers[i.b]
            if !value_is_number(rb^) {
                protect(L, pc)
                debug_arith_error(L, rb, rb)
            }
            n := value_to_number(rb^)
            n = number_unm(n)
            ra^ = value_make(n)

        // Arithmetic
        case .Add: arith(L, pc, ra, number_add, get_rkb_rkc(i, frame))
        case .Sub: arith(L, pc, ra, number_sub, get_rkb_rkc(i, frame))
        case .Mul: arith(L, pc, ra, number_mul, get_rkb_rkc(i, frame))
        case .Div: arith(L, pc, ra, number_div, get_rkb_rkc(i, frame))
        case .Mod: arith(L, pc, ra, number_mod, get_rkb_rkc(i, frame))
        case .Pow: arith(L, pc, ra, number_pow, get_rkb_rkc(i, frame))

        // Comparison
        case .Eq:
        case .Neq:
        case .Lt:
        case .Gt:
        case .Leq:
        case .Geq:

        // Control flow
        case .Return:
            n   := cast(int)i.b
            dst := slice.from_ptr(ra, n)
            fmt.printf("%q returned: ", ostring_to_string(chunk.name))
            for v, i in dst {
                if i > 0 {
                    fmt.print(", ")
                }
                value_print(v)
            }
            fmt.println()
            return
        }
    }
}

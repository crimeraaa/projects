#+private package
package lulu

import "base:intrinsics"
import "core:fmt"
import "core:c/libc"
import "core:slice"
import "core:strings"

VM :: struct {
    // Shared state across all VM instances.
    global_state: ^Global_State,
    
    // Hash table of all defined global variables.
    globals_table: ^Table,
    
    // Used for string concatenation and internal string formatting.
    builder: strings.Builder,

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

@(private="file")
Error_Handler :: struct {
    buf:    libc.jmp_buf,
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
    init :: proc(L: ^VM, _: rawptr) {
        g := G(L)

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
        
        // Ensure that the globals table is of some non-zero minimum size.
        t := table_new(L, 32)
        L.globals_table = t
        L.builder       = strings.builder_make(32, context.allocator)
    }

    err := vm_run_protected(L, init)
    return err == nil
}

vm_destroy :: proc(L: ^VM) {
    g := G(L)
    strings.builder_destroy(&L.builder)
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

    if libc.setjmp(&L.handler.buf) == 0 {
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
    libc.longjmp(&h.buf, 1)
}

vm_error_memory :: proc(L: ^VM) -> ! {
    vm_throw(L, .Memory)
}

// @(private="file")
// get_rk :: #force_inline proc(reg: u16, f: ^Frame) -> ^Value {
//     if reg_is_k(reg) {
//         i := reg_get_k(reg)
//         return &f.constants[i]
//     }
//     return &f.stack[reg]
// }

// @(private="file")
// get_rkb_rkc :: #force_inline proc(i: Instruction, f: ^Frame) -> (b, c: ^Value) {
//     b = get_rk(i.b, f)
//     c = get_rk(i.c, f)
//     return b, c
// }


vm_execute :: proc(L: ^VM, chunk: ^Chunk) {
    Op :: #type proc "contextless" (a, b: f64) -> f64
    
    /* 
    Works only for register-immediate encodings.
     */
    arith_imm :: #force_inline proc(L: ^VM, pc: int, ra: ^Value, op: Op, rb: ^Value, imm: u16) {
        if left, ok := value_to_number(rb^); ok {
            right := cast(f64)imm
            ra^    = value_make(op(left, right))
        } else {
            protect(L, pc)
            debug_arith_error(L, rb, rb)
        }
    }

    /*
    Works for both register-register and register-constant encodings.
    */
    arith :: #force_inline proc(L: ^VM, pc: int, ra: ^Value, op: Op, rb, rc: ^Value) {
        try: {
            left   := value_to_number(rb^) or_break try
            right  := value_to_number(rc^) or_break try
            ra^     = value_make(op(left, right))
            return
        }
        protect(L, pc)
        debug_arith_error(L, rb, rc)
    }

    protect :: proc(L: ^VM, pc: int) {
        L.frame.saved_pc = pc
    }

    @(disabled=!ODIN_DEBUG)
    print_stack :: proc(chunk: ^Chunk, i: Instruction, pc: int, R: []Value) {
        buf: [VALUE_TO_STRING_BUFFER_SIZE]byte
        for value, reg in R {
            repr := value_to_string(value, buf[:])
            fmt.printf("\tr%i := %s", reg, repr)
            if name, ok := find_local_at(chunk, reg, pc); ok {
                fmt.printfln("; local %s", name)
            } else {
                fmt.println()
            }
        }
        disassemble_at(chunk, i, pc)
    }

    L.frame = Frame{chunk, 0, L.stack[:chunk.stack_used], chunk.constants[:]}
    code   := raw_data(chunk.code)
    ip     := code
    frame  := &L.frame
    
    // Table of defined global variables.
    _G := L.globals_table

    // Registers array.
    R := frame.stack
    
    // Constants array.
    K := frame.constants

    // Zero-initalize the stack frame.
    for &v in R {
        v = value_make()
    }

    when ODIN_DEBUG do fmt.println("[EXECUTION]")
    for {
        i  := ip[0]
        pc := intrinsics.ptr_sub(ip, code)
        ip = &ip[1] // ip++
        print_stack(chunk, i, pc, R)
        
        ra := &R[i.a]
        switch i.op {
        case .Move:
            ra^ = R[i.b]

        case .Load_Nil:
            for &v in slice.from_ptr(ra, cast(int)(i.b - i.a)) {
                v = value_make()
            }

        case .Load_Bool:
            b  := cast(bool)i.b
            ra^ = value_make(b)
            
        case .Load_Imm:
            imm := cast(f64)getarg_bx(i)
            ra^  = value_make(imm)

        case .Load_Const:
            ra^ = K[getarg_bx(i)]

        case .Get_Global:
            key := K[getarg_bx(i)]
            if value, ok := table_get(_G, key); !ok {
                what := value_get_string(key)
                protect(L, pc)
                debug_runtime_error(L, "Attempt to read undefined global '%s'", what)
            } else {
                ra^ = value;
            }

        case .Set_Global:
            key := K[getarg_bx(i)]
            table_set(L, _G, key)^ = ra^

        // Unary
        case .Len:
            if rb := &R[i.b]; !value_is_string(rb^) {
                what := value_type_name(rb^)
                protect(L, pc)
                debug_type_error(L, "get length of", rb)
            } else {
                n  := value_get_ostring(rb^).len
                ra^ = value_make(cast(f64)n)
            }

        case .Not:
            not_b := value_is_falsy(R[i.b])
            ra^    = value_make(not_b)

        case .Unm:
            if rb := &R[i.b]; !value_is_number(rb^) {
                protect(L, pc)
                debug_arith_error(L, rb, rb)
            } else {
                n  := number_unm(value_get_number(rb^))
                ra^ = value_make(n)
            }
            
        // Arithmetic (register-immediate)
        case .Add_Imm: arith_imm(L, pc, ra, number_add, &R[i.b], i.c)
        case .Sub_Imm: arith_imm(L, pc, ra, number_sub, &R[i.b], i.c)
            
        // Arithmetic (register-constant)
        case .Add_Const: arith(L, pc, ra, number_add, &R[i.b], &K[i.c])
        case .Sub_Const: arith(L, pc, ra, number_sub, &R[i.b], &K[i.c])
        case .Mul_Const: arith(L, pc, ra, number_mul, &R[i.b], &K[i.c])
        case .Div_Const: arith(L, pc, ra, number_div, &R[i.b], &K[i.c])
        case .Mod_Const: arith(L, pc, ra, number_mod, &R[i.b], &K[i.c])
        case .Pow_Const: arith(L, pc, ra, number_pow, &R[i.b], &K[i.c])

        // Arithmetic (register-register)
        case .Add: arith(L, pc, ra, number_add, &R[i.b], &R[i.c])
        case .Sub: arith(L, pc, ra, number_sub, &R[i.b], &R[i.c])
        case .Mul: arith(L, pc, ra, number_mul, &R[i.b], &R[i.c])
        case .Div: arith(L, pc, ra, number_div, &R[i.b], &R[i.c])
        case .Mod: arith(L, pc, ra, number_mod, &R[i.b], &R[i.c])
        case .Pow: arith(L, pc, ra, number_pow, &R[i.b], &R[i.c])

        // Comparison
        // case .Eq..=.Geq:
        //     unreachable("Unimplemented: %v", i.op)

        // Control flow
        case .Return:
            fmt.printf("%q returned: ", ostring_to_string(chunk.name))
            buf: [VALUE_TO_STRING_BUFFER_SIZE]byte
            for v, i in slice.from_ptr(ra, cast(int)i.b) {
                if i > 0 {
                    fmt.print(", ")
                }
                s := value_to_string(v, buf[:])
                fmt.print(s)
            }
            fmt.println()
            return
        }
    }
}

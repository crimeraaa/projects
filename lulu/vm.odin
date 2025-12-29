#+private package
package lulu

import "base:intrinsics"
import "core:fmt"
import "core:c/libc"
import "core:math"
import "core:slice"
import "core:strconv"

VM :: struct {
    // Shared state across all VM instances.
    global: ^Global_State,

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

Error :: enum u8 {
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
    return L.global
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
vm_run_protected :: proc(L: ^VM, p: proc(^VM, rawptr), ud: rawptr) -> Error {
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

vm_error_syntax :: proc(L: ^VM) -> ! {
    vm_throw(L, .Syntax)
}

vm_error_memory :: proc(L: ^VM) -> ! {
    vm_throw(L, .Memory)
}

vm_error_runtime :: proc(L: ^VM, format := "", args: ..any) -> ! {
    frame := L.frame
    chunk := frame.chunk
    file  := chunk.name
    line  := chunk.lines[frame.saved_pc]
    fmt.eprintf("%s:%i: ", file, line)
    fmt.eprintfln(format, ..args)
    vm_throw(L, .Runtime)
}

@(private="file")
get_rk :: #force_inline proc(reg: u16, f: ^Frame) -> Value {
    if reg_is_k(reg) {
        i := reg_get_k(reg)
        return f.constants[i]
    }
    return f.stack[reg]
}

@(private="file")
get_rkb_rkc :: #force_inline proc(i: Instruction, f: ^Frame) -> (b, c: Value) {
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
            return n, ok
        }
    }
    return strconv.parse_f64(s)
}

funm :: proc "contextless" (a: f64)    -> f64 {return -a}
fadd :: proc "contextless" (a, b: f64) -> f64 {return a + b}
fsub :: proc "contextless" (a, b: f64) -> f64 {return a - b}
fmul :: proc "contextless" (a, b: f64) -> f64 {return a * b}
fdiv :: proc "contextless" (a, b: f64) -> f64 {return a / b}
fmod :: proc "contextless" (a, b: f64) -> f64 {return a - math.floor(a / b) * b}
fpow :: math.pow_f64

@(private="file")
arith :: #force_inline proc(L: ^VM,
    i: Instruction,
    pc: int,
    ra: ^Value,
    op: proc "contextless" (a, b: f64) -> f64)
{
    b, c := get_rkb_rkc(i, &L.frame)
    try: {
        left   := vm_to_number(b) or_break try
        right  := vm_to_number(c) or_break try
        result := op(left, right)
        ra^     = value_make(result)
        return
    }
    protect(L, pc)
    arith_error(L, b, c)
}

@(private="file")
protect :: proc(L: ^VM, pc: int) {
    L.frame.saved_pc = pc
}

@(private="file")
arith_error :: proc(L: ^VM, a, b: Value) -> ! {
    what := value_type_name(b if value_is_number(a) else a)
    vm_error_runtime(L, "Attempt to perform arithmetic on a %s value", what)
}

vm_execute :: proc(L: ^VM, chunk: ^Chunk) {
    L.frame = Frame{chunk, 0, L.stack[:chunk.stack_used], chunk.constants[:]}
    code  := raw_data(chunk.code)
    ip: [^]Instruction = code

    frame := &L.frame
    r := frame.stack
    k := frame.constants

    // Zero-initalize the stack frame.
    for &v in r {
        v = value_make()
    }

    when ODIN_DEBUG do fmt.println("[EXECUTION]")
    for {
        i  := ip[0]
        pc := intrinsics.ptr_sub(ip, code)
        when ODIN_DEBUG {
            for v, reg in r {
                fmt.printf("\tr%i := ", reg)
                value_print(v, newline=true)
            }
            chunk_disassemble_at(chunk, i, pc)
        }

        // ip++
        ip = &ip[1]
        ra := &r[i.a]
        switch i.op {
        case .Move:
            rb := r[i.b]
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
            value := k[index]
            ra^ = value

        // Unary
        case .Len:
            rb := r[i.b]
            if !value_is_string(rb) {
                what := value_type_name(rb)
                protect(L, pc)
                vm_error_runtime(L, "Attempt to get length of a %s value", what)
            }
            n := value_to_ostring(rb).len
            ra^ = value_make(cast(f64)n)

        case .Not:
            rb    := r[i.b]
            not_b := value_is_falsy(rb)
            ra^    = value_make(not_b)

        case .Unm:
            rb := r[i.b]
            if !value_is_number(rb) {
                arith_error(L, rb, rb)
            }
            n := value_to_number(rb)
            n = funm(n)
            ra^ = value_make(n)

        // Arithmetic
        case .Add: arith(L, i, pc, ra, fadd)
        case .Sub: arith(L, i, pc, ra, fsub)
        case .Mul: arith(L, i, pc, ra, fmul)
        case .Div: arith(L, i, pc, ra, fdiv)
        case .Mod: arith(L, i, pc, ra, fmod)
        case .Pow: arith(L, i, pc, ra, fpow)

        // Comparison
        case .Eq:
        case .Neq:
        case .Lt:
        case .Gt:
        case .Leq:
        case .Geq:

        // Control flow
        case .Return:
            n := cast(int)(i.b - i.a)
            dst := slice.from_ptr(ra, n)
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

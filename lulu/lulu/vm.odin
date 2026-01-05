package lulu

import "base:intrinsics"
import "base:runtime"
import "core:fmt"
import "core:strings"
import "core:strconv"
import "core:unicode/utf8"


// **Note(2025-01-05)**
//
// Although the fields are public, do not directly access nor modify them.
// Use only the public API to do so.
//
State :: struct {
    // Shared state across all VM instances.
    global_state: ^Global_State,

    // Hash table of all defined global variables.
    globals_table: Value,

    // Used for string concatenation and internal string formatting.
    builder: strings.Builder,

    // Stack-allocated linked list of error handlers.
    handler: ^Error_Handler,

    // Stack window of current function, a 'window' into `stack`.
    registers: []Value,

    // Current frame information.
    frame:       ^Frame,
    frame_index: int,
    frames:      [16]Frame,

    // Stack used across all active call frames.
    stack: [64]Value,
}

@(private="package")
Global_State :: struct {
    // Pointer to the main state we were allocated alongside.
    main_state: ^State,

    // Hash table of all interned strings.
    intern: Intern,

    // Singly linked list of all possibly-collectable objects across all
    // VM states.
    objects: ^Object,

    bytes_allocated: int,
}

@(private="package")
Frame :: struct {
    // The value which represents the function being called. It must be a pointer
    // so that we can try to report the variable name which points to the
    // function.
    callee: ^Value,

    // Index of instruction where we left off (e.g. if we dispatch a Lua
    // function call).
    saved_pc: int,

    // Window into VM's primary stack.
    registers: []Value,
}

@(private="package")
vm_init :: proc(L: ^State, g: ^Global_State) -> (ok: bool) {
    init :: proc(L: ^State, _: rawptr) {
        g := L.global_state

        // Ensure that, when we start interning strings, we already have
        // valid indexes.
        intern_resize(L, &g.intern, 32)
        s := ostring_new(L, MEMORY_ERROR_MESSAGE)
        s.mark += {.Fixed}
        for kw_type in Token_Type.And..=Token_Type.While {
            kw := token_string(kw_type)
            s   = ostring_new(L, kw)
            s.kw_type = kw_type
            s.mark   += {.Fixed}
        }

        // Ensure that the globals table is of some non-zero minimum size.
        t := table_new(L, 17)
        L.globals_table = value_make(t)

        // Initialize concat string builder with some reasonable default size.
        L.builder = strings.builder_make(32, context.allocator)

        // Ensure the pointed-to data is non-nil.
        L.registers = L.stack[:0]
    }

    L.global_state = g
    g.main_state   = L

    // Don't use `vm_raw_pcall()`, because we won't be able to push an error
    // object in case of memory errors.
    err := raw_run_unrestoring(L, init, nil)
    return err == nil
}

@(private="package")
vm_destroy :: proc(L: ^State) {
    g := L.global_state
    strings.builder_destroy(&L.builder)
    intern_destroy(L, &g.intern)
    object_free_all(L, g.objects)
}

@(private="package")
vm_push_fstring :: proc(L: ^State, fmt: string, args: ..any) -> string {
    fmt     := fmt
    pushed  := 0
    arg_i   := 0

    for {
        fmt_i := strings.index_byte(fmt, '%')
        if fmt_i == -1 {
            // Write whatever remains unformatted as-is.
            if len(fmt) > 0 {
                vm_push_string(L, fmt)
                pushed += 1
            }
            break
        }

        // Write any bits of the string before the format specifier.
        if len(fmt[:fmt_i]) > 0 {
            vm_push_string(L, fmt[:fmt_i])
            pushed += 1
        }

        arg    := args[arg_i]
        arg_i  += 1
        spec_i := fmt_i + 1

        buf: [VALUE_TO_STRING_BUFFER_SIZE]byte
        b := strings.builder_from_bytes(buf[:])
        switch spec := fmt[spec_i]; spec {
        case 'c':
            r_buf, size := utf8.encode_rune(arg.(rune))
            copy(buf[:size], r_buf[:size])
            vm_push_string(L, string(buf[:size]))

        case 'd', 'i':
            i := cast(i64)arg.(int)
            vm_push_string(L, strconv.write_int(buf[:], i, base=10))

        case 'f':
            repr := number_to_string(arg.(f64), buf[:])
            vm_push_string(L, repr)

        case 's':
            vm_push_string(L, arg.(string))

        case:
            unreachable("Unsupported format specifier '%c'", spec)
        }
        pushed += 1

        // Move over the format specifier, only if we have characters remaining.
        if next_i := spec_i + 1; next_i < len(fmt) {
            fmt = fmt[next_i:]
        } else {
            break
        }
    }

    stop  := get_top(L)
    start := stop - pushed
    args  := L.registers[start:stop]
    s := vm_concat(L, &args[0], args)
    vm_pop(L, pushed - 1)
    return ostring_to_string(s)
}

@(private="package")
vm_push_string :: proc(L: ^State, s: string) {
    interned := ostring_new(L, s)
    vm_push_value(L, value_make(interned))
}

@(private="package")
vm_push_value :: proc(L: ^State, v: Value) {
    base := vm_save_base(L)
    top  := vm_save_top(L) + 1
    L.registers = L.stack[base:top]
    L.stack[top - 1] = v
}

@(private="package")
vm_concat :: proc(L: ^State, dst: ^Value, args: []Value) -> ^Ostring {
    b := &L.builder
    strings.builder_reset(b)
    for v in args {
        s := value_get_string(v)
        n := strings.write_string(b, s)
        if n != len(s) {
            debug_memory_error(L, "concatenate string '%s'", s)
        }
    }
    text := strings.to_string(b^)
    s    := ostring_new(L, text)
    dst^  = value_make(s)
    return s
}

@(private="package")
vm_pop :: proc(L: ^State, count: int) {
    L.registers = L.registers[:get_top(L) - count]
}

@(private="package")
vm_save_base :: proc(L: ^State) -> (index: int) #no_bounds_check {
    callee_base := &L.registers[0]
    stack_base  := &L.stack[0]
    return intrinsics.ptr_sub(callee_base, stack_base)
}

@(private="package")
vm_save_top :: proc(L: ^State) -> (index: int) #no_bounds_check {
    callee_top := &L.registers[get_top(L)]
    stack_base := &L.stack[0]
    return intrinsics.ptr_sub(callee_top, stack_base)
}

@(private="package")
vm_save_stack :: proc(L: ^State, value: ^Value) -> (index: int) {
    return find_ptr_index_unsafe(L.stack[:], value)
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


@(private="package")
vm_execute :: proc(L: ^State) {
    Op :: #type proc "contextless" (a, b: f64) -> f64

    /*
    Works only for register-immediate encodings.
     */
    arith_imm :: proc(L: ^State, pc: int, ra: ^Value, op: Op, rb: ^Value, imm: u16) {
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
    arith :: proc(L: ^State, pc: int, ra: ^Value, op: Op, rb, rc: ^Value) {
        try: {
            left   := value_to_number(rb^) or_break try
            right  := value_to_number(rc^) or_break try
            ra^     = value_make(op(left, right))
            return
        }
        protect(L, pc)
        debug_arith_error(L, rb, rc)
    }

    protect :: proc(L: ^State, pc: int) {
        L.frame.saved_pc = pc
    }

    @(disabled=!ODIN_DEBUG)
    print_stack :: proc(chunk: ^Chunk, i: Instruction, pc: int, R: []Value) {
        buf: [VALUE_TO_STRING_BUFFER_SIZE]byte
        for value, reg in R {
            repr := value_to_string(value, buf[:])
            fmt.printf("\tr%i := ", reg)
            fmt.printf("%q" if value_is_string(value) else "%s", repr)
            if name, ok := find_local(chunk, reg, pc); ok {
                fmt.printfln(" ; local %s", name)
            } else {
                fmt.println()
            }
        }
        disassemble_at(chunk, i, pc)
    }


    frame := L.frame
    chunk := value_get_chunk(frame.callee^)

    // Registers array.
    R := frame.registers

    // Constants array.
    K := chunk.constants[:]

    code: [^]Instruction = &chunk.code[0]
    ip := code

    // Table of defined global variables.
    _G := L.globals_table

    when ODIN_DEBUG do fmt.println("[EXECUTION]")
    for {
        i  := ip[0] // *ip
        pc := intrinsics.ptr_sub(ip, code)
        ip = &ip[1] // ip++
        print_stack(chunk, i, pc, R)

        a  := i.a
        ra := &R[a]
        op := i.op
        switch op {
        case .Move:
            ra^ = R[i.b]

        case .Load_Nil:
            for &v in R[a:i.b] {
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
            k := K[getarg_bx(i)]
            if v, ok := table_get(value_get_table(_G), k); !ok {
                what := value_get_string(k)
                protect(L, pc)
                debug_runtime_error(L, "Attempt to read undefined global '%s'", what)
            } else {
                ra^ = v;
            }

        case .Set_Global:
            key := K[getarg_bx(i)]
            table_set(L, value_get_table(_G), key)^ = ra^

        // Unary
        case .Len:
            #partial switch rb := &R[i.b]; value_type(rb^) {
            case .String:
                n  := value_get_ostring(rb^).len
                ra^ = value_make(cast(f64)n)
            case:
                protect(L, pc)
                debug_type_error(L, "get length of", rb)
            }

        case .Not:
            ra^ = value_make(value_is_falsy(R[i.b]))

        case .Unm:
            rb := &R[i.b]
            if n, ok := value_to_number(rb^); ok {
                ra^ = value_make(number_unm(n))
            } else {
                protect(L, pc)
                debug_arith_error(L, rb, rb)
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

        case .Concat: vm_concat(L, ra, R[i.b:i.c])

        // Control flow
        case .Call:
            arg_count := cast(int)i.b
            ret_count := cast(int)i.c
            run_call(L, ra, arg_count, ret_count)

        case .Return:
            fmt.printf("%q returned: ", chunk_name(chunk))
            buf: [VALUE_TO_STRING_BUFFER_SIZE]byte

            count := i.b
            for v, i in R[a:a + count] { // slice.from_ptr(ra, cast(int)i.b) {
                if i > 0 {
                    fmt.print(", ")
                }
                s := value_to_string(v, buf[:])
                fmt.print(s)
            }
            fmt.println()
            return
        case:
            unreachable("Invalid opcode %v", op)
        }
    }
}

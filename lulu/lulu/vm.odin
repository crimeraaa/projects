#+private package
package lulu

import "core:fmt"
import "core:mem"
import "core:strings"
import "core:strconv"
import "core:unicode/utf8"

state_push_fstring :: proc(L: ^State, format: string, args: ..any) -> string {
    format  := format
    pushed  := 0
    arg_i   := 0

    for {
        fmt_i := strings.index_byte(format, '%')
        if fmt_i == -1 {
            // Write whatever remains unformatted as-is.
            if len(format) > 0 {
                state_push(L, format)
                pushed += 1
            }
            break
        }

        // Write any bits of the string before the format specifier.
        if unformatted := format[:fmt_i]; len(unformatted) > 0 {
            state_push(L, unformatted)
            pushed += 1
        }

        arg    := args[arg_i]
        arg_i  += 1
        spec_i := fmt_i + 1

        buf: [VALUE_TO_STRING_BUFFER_SIZE]byte
        switch spec := format[spec_i]; spec {
        case 'c':
            r := arg.(rune) or_else rune(arg.(byte))
            r_buf, size := utf8.encode_rune(r)
            copy(buf[:size], r_buf[:size])
            state_push(L, string(buf[:size]))

        case 'd', 'i':
            i := arg.(int) or_else int(arg.(i32))
            state_push(L, strconv.write_int(buf[:], i64(i), base=10))

        case 'f':
            f := arg.(f64) or_else f64(arg.(f32))
            repr := number_to_string(f, buf[:])
            state_push(L, repr)

        case 's':
            state_push(L, arg.(string))

        case 'p':
            repr := pointer_to_string(arg.(rawptr), buf[:])
            state_push(L, repr)

        case:
            unreachable("Unsupported format specifier '%c'", spec)
        }
        pushed += 1

        // Move over the format specifier, only if we have characters remaining.
        if next_i := spec_i + 1; next_i < len(format) {
            format = format[next_i:]
        } else {
            break
        }
    }

    stop  := get_top(L)
    start := stop - pushed
    args  := L.registers[start:stop]
    s := vm_concat(L, &args[0], args)
    state_pop(L, pushed - 1)
    return ostring_to_string(s)
}

state_push :: proc {
    state_push_string,
    state_push_ostring,
    state_push_closure,
    state_push_value,
}

state_push_string :: proc(L: ^State, s: string, loc := #caller_location) {
    interned := ostring_new(L, s, loc=loc)
    state_push(L, value_make(interned))
}

state_push_ostring :: proc(L: ^State, ostring: ^Ostring) {
    state_push(L, value_make(ostring))
}

state_push_closure :: proc(L: ^State, cl: ^Closure) {
    state_push(L, value_make(cl))
}

state_push_value :: proc(L: ^State, v: Value) {
    base := state_save_base(L)
    top  := state_save_top(L) + 1
    assert(top <= len(L.stack), "Lulu value stack overflow")
    L.registers = L.stack[base:top]
    L.stack[top - 1] = v
}

vm_concat :: proc(L: ^State, dst: ^Value, args: []Value) -> ^Ostring {
    b := &L.builder
    strings.builder_reset(b)
    for &v in args {
        #partial switch v.type {
        case .String, .Number:
            break
        case:
            debug_type_error(L, "concatenate", &v)
        }

        buf: [VALUE_TO_STRING_BUFFER_SIZE]byte
        s := value_to_string(v, buf[:])
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

state_pop :: proc(L: ^State, count := 1) {
    L.registers = L.registers[:get_top(L) - count]
}

state_save_base :: proc(L: ^State) -> (index: int) #no_bounds_check {
    callee_base := &L.registers[0]
    return find_ptr_index_unsafe(L.stack[:], callee_base)
}

state_save_top :: proc(L: ^State) -> (index: int) #no_bounds_check {
    callee_top := &L.registers[get_top(L)]
    return find_ptr_index_unsafe(L.stack[:], callee_top)
}

state_save_stack :: proc(L: ^State, value: ^Value) -> (index: int) {
    return find_ptr_index_unsafe(L.stack[:], value)
}

/*
Works only for register-immediate encodings.
 */
@(private="file")
__arith_imm :: proc(L: ^State, pc: i32, ra: ^Value, procedure: $T, rb: ^Value, imm: u16) {
    if left, ok := value_to_number(rb^); ok {
        ra^ = value_make(procedure(left, f64(imm)))
    } else {
        __protect(L, pc)
        debug_arith_error(L, rb, rb)
    }
}

/*
Works for both register-register and register-constant encodings.
*/
@(private="file")
__arith :: proc(L: ^State, pc: i32, ra: ^Value, procedure: $T, rb, rc: ^Value) {
    try: {
        left   := value_to_number(rb^) or_break try
        right  := value_to_number(rc^) or_break try
        ra^     = value_make(procedure(left, right))
        return
    }
    __protect(L, pc)
    debug_arith_error(L, rb, rc)
}

@(private="file")
__compare_imm :: proc(L: ^State, ip: ^[^]Instruction, pc: i32, procedure: $T, ra: ^Value, imm: i32, cond: bool) {
    try: {
        left  := value_to_number(ra^) or_break try
        right := f64(imm)
        if procedure(left, right) != cond {
            ip^ = &ip[1]
        }
        return
    }
    __protect(L, pc)
    tmp := value_make(0.0)
    debug_compare_error(L, ra, &tmp)
}

@(private="file")
__compare_eq :: proc(ip: ^[^]Instruction, ra, rb: Value, cond: bool) {
    res := value_eq(ra, rb)
    if res != cond {
        ip^ = &ip[1]
    }
}

@(private="file")
__compare :: proc(L: ^State, ip: ^[^]Instruction, pc: i32, procedure: $T, ra, rb: ^Value, cond: bool) {
    try: {
        left   := value_to_number(ra^) or_break try
        right  := value_to_number(rb^) or_break try
        if procedure(left, right) != cond {
            ip^ = &ip[1]
        }
        return
    }
    __protect(L, pc)
    debug_compare_error(L, ra, rb)
}

@(private="file")
__protect :: proc(L: ^State, pc: i32) {
    L.frame.saved_pc = pc
}

@(private="file", disabled=!DISASSEMBLE_INLINE)
__print_stack :: proc(chunk: ^Chunk, i: Instruction, pc: i32, R: []Value) {
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

vm_get_table :: proc(L: ^State, t: ^Value, k: Value) -> (v: Value, ok: bool) #optional_ok {
    if !value_is_table(t^) {
        debug_type_error(L, "index", t)
    }
    return table_get(value_get_table(t^), k)
}

vm_set_table :: proc(L: ^State, t: ^Value, k, v: Value) {
    if !value_is_table(t^) {
        debug_type_error(L, "set index of", t)
    }
    dst := table_set(L, value_get_table(t^), k)
    dst^ = v
}

@(private="file")
__get_table :: proc(L: ^State, pc: i32, ra, t: ^Value, k: Value) {
    __protect(L, pc)
    v, _ := vm_get_table(L, t, k)
    ra^  = v
}

@(private="file")
__set_table :: proc(L: ^State, pc: i32, t: ^Value, k, v: Value) {
    __protect(L, pc)
    vm_set_table(L, t, k, v)
}

vm_execute :: proc(L: ^State, ret_expect: int) {
    frame := L.frame
    chunk := value_get_function(frame.callee^).lua.chunk

    // Registers array.
    R := frame.registers

    // Constants array.
    K := chunk.constants[:]

    code := raw_data(chunk.code)
    ip   := code

    // Table of defined global variables.
    _G := &L.globals_table

    when DISASSEMBLE_INLINE do fmt.println("[EXECUTION]")
    for {
        i  := ip[0] // *ip
        pc := i32(mem.ptr_sub(ip, code))
        ip = &ip[1] // ip++
        __print_stack(chunk, i, pc, R)

        A  := i.A
        RA := &R[A]
        op := i.op
        switch op {
        case .Move:       RA^ = R[i.B]
        case .Load_Nil:   mem.zero_slice(R[A:i.B])
        case .Load_Bool:
            RA^ = value_make(bool(i.B))
            if bool(i.C) {
                ip = &ip[1]
            }
        case .Load_Imm:   RA^ = value_make(f64(i.u.Bx))
        case .Load_Const: RA^ = K[i.u.Bx]

        case .Get_Global:
            k := K[i.u.Bx]
            __protect(L, pc)
            if v, ok := vm_get_table(L, _G, k); !ok {
                what := value_get_string(k)
                debug_runtime_error(L, "Attempt to read undefined global '%s'", what)
            } else {
                RA^ = v;
            }

        case .Set_Global:
            __protect(L, pc)
            vm_set_table(L, _G, K[i.u.Bx], RA^)

        case .New_Table:
            hash_count  := 1 << (i.B - 1) if i.B != 0 else 0
            array_count := 1 << (i.C - 1) if i.C != 0 else 0
            t := table_new(L, hash_count, array_count)
            RA^ = value_make(t)

        case .Get_Table: __get_table(L, pc, RA, &R[i.B], R[i.C])
        case .Get_Field: __get_table(L, pc, RA, &R[i.B], K[i.C])
        case .Set_Table: __set_table(L, pc, RA, R[i.B], (K if i.k.k else R)[i.k.C])
        case .Set_Field: __set_table(L, pc, RA, K[i.B], (K if i.k.k else R)[i.k.C])

        // Unary
        case .Len:
            #partial switch rb := &R[i.B]; value_type(rb^) {
            case .String:
                n  := value_get_ostring(rb^).len
                RA^ = value_make(f64(n))

            case .Table:
                n  := table_len(value_get_table(rb^))
                RA^ = value_make(f64(n))
            case:
                __protect(L, pc)
                debug_type_error(L, "get length of", rb)
            }

        case .Not: RA^ = value_make(value_is_falsy(R[i.B]))
        case .Unm:
            rb := &R[i.B]
            if n, ok := value_to_number(rb^); ok {
                RA^ = value_make(number_unm(n))
            } else {
                __protect(L, pc)
                debug_arith_error(L, rb, rb)
            }

        // Arithmetic (register-immediate)
        case .Add_Imm: __arith_imm(L, pc, RA, number_add, &R[i.B], i.C)
        case .Sub_Imm: __arith_imm(L, pc, RA, number_sub, &R[i.B], i.C)

        // Arithmetic (register-constant)
        case .Add_Const: __arith(L, pc, RA, number_add, &R[i.B], &K[i.C])
        case .Sub_Const: __arith(L, pc, RA, number_sub, &R[i.B], &K[i.C])
        case .Mul_Const: __arith(L, pc, RA, number_mul, &R[i.B], &K[i.C])
        case .Div_Const: __arith(L, pc, RA, number_div, &R[i.B], &K[i.C])
        case .Mod_Const: __arith(L, pc, RA, number_mod, &R[i.B], &K[i.C])
        case .Pow_Const: __arith(L, pc, RA, number_pow, &R[i.B], &K[i.C])

        // Arithmetic (register-register)
        case .Add: __arith(L, pc, RA, number_add, &R[i.B], &R[i.C])
        case .Sub: __arith(L, pc, RA, number_sub, &R[i.B], &R[i.C])
        case .Mul: __arith(L, pc, RA, number_mul, &R[i.B], &R[i.C])
        case .Div: __arith(L, pc, RA, number_div, &R[i.B], &R[i.C])
        case .Mod: __arith(L, pc, RA, number_mod, &R[i.B], &R[i.C])
        case .Pow: __arith(L, pc, RA, number_pow, &R[i.B], &R[i.C])

        // Comparison (register-immediate)
        case .Eq_Imm:    __compare_eq(&ip, RA^, value_make(f64(i.vs.Bx)), i.vs.k)
        case .Lt_Imm:    __compare_imm(L, &ip, pc, number_lt,  RA, i.vs.Bx, i.vs.k)
        case .Leq_Imm:   __compare_imm(L, &ip, pc, number_leq, RA, i.vs.Bx, i.vs.k)

        // Comparison (register-constant)
        case .Eq_Const:  __compare_eq(&ip, RA^, K[i.vu.Bx], i.vu.k)
        case .Lt_Const:  __compare(L, &ip, pc, number_lt,  RA, &K[i.vu.Bx], i.vu.k)
        case .Leq_Const: __compare(L, &ip, pc, number_leq, RA, &K[i.vu.Bx], i.vu.k)

        // Comparison (register-register)
        case .Eq:  __compare_eq(&ip, RA^, R[i.B], i.k.k)
        case .Lt:  __compare(L, &ip, pc, number_lt,  RA, &R[i.B], i.k.k)
        case .Leq: __compare(L, &ip, pc, number_leq, RA, &R[i.B], i.k.k)


        // Misc.
        case .Concat:
            args := R[i.B:i.C]
            __protect(L, pc)
            vm_concat(L, RA, args)

        // Control flow
        case .Call:
            arg_first := int(A)   + 1
            arg_count := int(i.B) + VARIADIC
            ret_count := int(i.C) + VARIADIC

            /*
            Resolve the actual number of variadic arguments pushed to the stack.

            **Assumptions**
            - we assume that we can only reach here if we just had a variadic
            return, which previously set the stack top.
            - Otherwise this should be impossible because we don't (yet)
            implement the `...` operator.
            */
            if arg_count == VARIADIC {
                arg_count = get_top(L) - arg_first
            }
            __protect(L, pc)
            run_call(L, RA, arg_count, ret_count)

            // In case varargs pushed more than our current stack frame.
            if len(L.registers) > len(R) {
                R = L.registers
            }

        case .Jump:
            offset := int(i.s.Bx)
            ip = &ip[offset]

        case .Jump_Not:
            if value_is_falsy(RA^) {
                offset := int(i.s.Bx)
                ip = &ip[offset]
            }

        case .Move_If:
            RB   := R[i.B]
            cond := bool(i.C)
            if !value_is_falsy(RB) == cond {
                RA^ = RB
            } else {
                ip = &ip[1]
            }

        case .Return:
            ret_first := int(A)
            ret_count := int(i.B) + VARIADIC
            if ret_count == VARIADIC {
                ret_count = get_top(L) - ret_first
            }

            ret_src := R[ret_first:ret_first + ret_count]
            run_call_return(L, ret_expect, ret_src)
            return

        case:
            unreachable("Invalid opcode %v", op)
        }
    }
}

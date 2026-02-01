#+private package
package lulu

import "base:runtime"
import "core:fmt"
import "core:mem"
import "core:strings"
import "core:strconv"
import "core:unicode/utf8"

vm_init :: proc(L: ^State, g: ^Global_State, allocator: mem.Allocator) -> (ok: bool) {
    L.global_state      = g
    g.main_state        = L
    g.backing_allocator = allocator

    // Don't use `run_raw_pcall()`, because we won't be able to push an
    // error object in case of memory errors.
    err := run_raw_call(L, proc(L: ^State, _: rawptr) {
        g := L.global_state

        // Ensure that, when we start interning strings, we already have
        // valid indexes.
        intern_resize(L, &g.intern, 32)
        s := ostring_new(L, MEMORY_ERROR_MESSAGE)
        s.mark += {.Fixed}
        for kw_type in Token_Type.And..=Token_Type.While {
            kw := token_type_string(kw_type)
            s   = ostring_new(L, kw)
            s.kw_type = kw_type
            s.mark   += {.Fixed}
        }

        // Ensure that the globals table is of some non-zero minimum size.
        t := table_new(L, hash_count=17, array_count=0)
        L.globals_table = value_make(t)

        // Initialize concat string builder with some reasonable default size.
        L.builder = strings.builder_make(32, allocator=g.backing_allocator)

        // Ensure the pointed-to data is non-nil.
        L.registers = L.stack[:0]
    }, nil)

    return err == nil
}

vm_destroy :: proc(L: ^State) {
    g := L.global_state
    strings.builder_destroy(&L.builder)
    intern_destroy(L, &g.intern)
    object_free_all(L, g.objects)
}

vm_push_fstring :: proc(L: ^State, format: string, args: ..any) -> string {
    format  := format
    pushed  := 0
    arg_i   := 0

    for {
        fmt_i := strings.index_byte(format, '%')
        if fmt_i == -1 {
            // Write whatever remains unformatted as-is.
            if len(format) > 0 {
                vm_push(L, format)
                pushed += 1
            }
            break
        }

        // Write any bits of the string before the format specifier.
        if len(format[:fmt_i]) > 0 {
            vm_push(L, format[:fmt_i])
            pushed += 1
        }

        arg    := args[arg_i]
        arg_i  += 1
        spec_i := fmt_i + 1

        buf: [VALUE_TO_STRING_BUFFER_SIZE]byte
        b := strings.builder_from_bytes(buf[:])
        switch spec := format[spec_i]; spec {
        case 'c':
            r := arg.(rune) or_else rune(arg.(byte))
            r_buf, size := utf8.encode_rune(r)
            copy(buf[:size], r_buf[:size])
            vm_push(L, string(buf[:size]))

        case 'd', 'i':
            i := arg.(int) or_else int(arg.(i32))
            vm_push(L, strconv.write_int(buf[:], i64(i), base=10))

        case 'f':
            f := arg.(f64) or_else f64(arg.(f32))
            repr := number_to_string(f, buf[:])
            vm_push(L, repr)

        case 's':
            vm_push(L, arg.(string))

        case 'p':
            repr := pointer_to_string(arg.(rawptr), buf[:])
            vm_push(L, repr)

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
    vm_pop(L, pushed - 1)
    return ostring_to_string(s)
}

vm_push :: proc {
    vm_push_string,
    vm_push_closure,
    vm_push_value,
}

vm_push_string :: proc(L: ^State, s: string) {
    interned := ostring_new(L, s)
    vm_push(L, value_make(interned))
}

vm_push_closure :: proc(L: ^State, cl: ^Closure) {
    vm_push(L, value_make(cl))
}

vm_push_value :: proc(L: ^State, v: Value) {
    base := vm_save_base(L)
    top  := vm_save_top(L) + 1
    assert(top <= len(L.stack), "Lulu value stack overflow")
    L.registers = L.stack[base:top]
    L.stack[top - 1] = v
}

vm_concat :: proc(L: ^State, dst: ^Value, args: []Value) -> ^Ostring {
    b := &L.builder
    strings.builder_reset(b)
    for &v in args {
        if !value_is_string(v) {
            debug_type_error(L, "concatenate", &v)
        }

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

vm_pop :: proc(L: ^State, count := 1) {
    L.registers = L.registers[:get_top(L) - count]
}

vm_save_base :: proc(L: ^State) -> (index: int) #no_bounds_check {
    callee_base := &L.registers[0]
    return find_ptr_index_unsafe(L.stack[:], callee_base)
}

vm_save_top :: proc(L: ^State) -> (index: int) #no_bounds_check {
    callee_top := &L.registers[get_top(L)]
    return find_ptr_index_unsafe(L.stack[:], callee_top)
}

vm_save_stack :: proc(L: ^State, value: ^Value) -> (index: int) {
    return find_ptr_index_unsafe(L.stack[:], value)
}

/*
Works only for register-immediate encodings.
 */
@(private="file")
_arith_imm :: proc(L: ^State, pc: i32, ra: ^Value, procedure: $T, rb: ^Value, imm: u16) {
    if left, ok := value_to_number(rb^); ok {
        ra^ = value_make(procedure(left, f64(imm)))
    } else {
        _protect(L, pc)
        debug_arith_error(L, rb, rb)
    }
}

/*
Works for both register-register and register-constant encodings.
*/
@(private="file")
_arith :: proc(L: ^State, pc: i32, ra: ^Value, procedure: $T, rb, rc: ^Value) {
    try: {
        left   := value_to_number(rb^) or_break try
        right  := value_to_number(rc^) or_break try
        ra^     = value_make(procedure(left, right))
        return
    }
    _protect(L, pc)
    debug_arith_error(L, rb, rc)
}

@(private="file")
_compare :: proc(L: ^State, ip: ^[^]Instruction, pc: i32, procedure: $T, ra, rb: ^Value, cond: bool) {
    try: {
        left   := value_to_number(ra^) or_break try
        right  := value_to_number(rb^) or_break try
        if procedure(left, right) != cond {
            ip^ = &ip[1]
        }
        return
    }
    _protect(L, pc)
    debug_compare_error(L, ra, rb)
}

@(private="file")
_protect :: proc(L: ^State, pc: i32) {
    L.frame.saved_pc = pc
}

@(private="file", disabled=!DISASSEMBLE_INLINE)
_print_stack :: proc(chunk: ^Chunk, i: Instruction, pc: i32, R: []Value) {
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
_get_table :: proc(L: ^State, pc: i32, ra, t: ^Value, k: Value) {
    _protect(L, pc)
    v, _ := vm_get_table(L, t, k)
    ra^  = v
}

@(private="file")
_set_table :: proc(L: ^State, pc: i32, t: ^Value, k, v: Value) {
    _protect(L, pc)
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
        _print_stack(chunk, i, pc, R)

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
            _protect(L, pc)
            if v, ok := vm_get_table(L, _G, k); !ok {
                what := value_get_string(k)
                debug_runtime_error(L, "Attempt to read undefined global '%s'", what)
            } else {
                RA^ = v;
            }

        case .Set_Global:
            _protect(L, pc)
            vm_set_table(L, _G, K[i.u.Bx], RA^)

        case .New_Table:
            hash_count  := 1 << (i.B - 1) if i.B != 0 else 0
            array_count := 1 << (i.C - 1) if i.C != 0 else 0
            t := table_new(L, hash_count=hash_count, array_count=array_count)
            RA^ = value_make(t)

        case .Get_Table: _get_table(L, pc, RA, &R[i.B], R[i.C])
        case .Get_Field: _get_table(L, pc, RA, &R[i.B], K[i.C])
        case .Set_Table: _set_table(L, pc, RA, R[i.B], (K if i.k.k else R)[i.k.C])
        case .Set_Field: _set_table(L, pc, RA, K[i.B], (K if i.k.k else R)[i.k.C])

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
                _protect(L, pc)
                debug_type_error(L, "get length of", rb)
            }

        case .Not: RA^ = value_make(value_is_falsy(R[i.B]))
        case .Unm:
            rb := &R[i.B]
            if n, ok := value_to_number(rb^); ok {
                RA^ = value_make(number_unm(n))
            } else {
                _protect(L, pc)
                debug_arith_error(L, rb, rb)
            }

        // Arithmetic (register-immediate)
        case .Add_Imm: _arith_imm(L, pc, RA, number_add, &R[i.B], i.C)
        case .Sub_Imm: _arith_imm(L, pc, RA, number_sub, &R[i.B], i.C)

        // Arithmetic (register-constant)
        case .Add_Const: _arith(L, pc, RA, number_add, &R[i.B], &K[i.C])
        case .Sub_Const: _arith(L, pc, RA, number_sub, &R[i.B], &K[i.C])
        case .Mul_Const: _arith(L, pc, RA, number_mul, &R[i.B], &K[i.C])
        case .Div_Const: _arith(L, pc, RA, number_div, &R[i.B], &K[i.C])
        case .Mod_Const: _arith(L, pc, RA, number_mod, &R[i.B], &K[i.C])
        case .Pow_Const: _arith(L, pc, RA, number_pow, &R[i.B], &K[i.C])

        // Arithmetic (register-register)
        case .Add: _arith(L, pc, RA, number_add, &R[i.B], &R[i.C])
        case .Sub: _arith(L, pc, RA, number_sub, &R[i.B], &R[i.C])
        case .Mul: _arith(L, pc, RA, number_mul, &R[i.B], &R[i.C])
        case .Div: _arith(L, pc, RA, number_div, &R[i.B], &R[i.C])
        case .Mod: _arith(L, pc, RA, number_mod, &R[i.B], &R[i.C])
        case .Pow: _arith(L, pc, RA, number_pow, &R[i.B], &R[i.C])

        // Comparison
        case .Eq:
            rb   := R[i.B]
            cond := bool(i.C)
            res  := value_eq(RA^, rb)
            if res != cond {
                ip = &ip[1]
            }

        case .Lt:  _compare(L, &ip, pc, number_lt,  RA, &R[i.B], bool(i.C))
        case .Leq: _compare(L, &ip, pc, number_leq, RA, &R[i.B], bool(i.C))

        // Misc.
        case .Concat:
            args := R[i.B:i.C]
            _protect(L, pc)
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
            _protect(L, pc)
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

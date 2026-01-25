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
            kw := token_string(kw_type)
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
            r_buf, size := utf8.encode_rune(arg.(rune))
            copy(buf[:size], r_buf[:size])
            vm_push(L, string(buf[:size]))

        case 'd', 'i':
            i := i64(arg.(int))
            vm_push(L, strconv.write_int(buf[:], i, base=10))

        case 'f':
            repr := number_to_string(arg.(f64), buf[:])
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

@(private="file")
Op :: #type proc "contextless" (a, b: f64) -> f64

/*
Works only for register-immediate encodings.
 */
@(private="file")
_arith_imm :: proc(L: ^State, pc: i32, ra: ^Value, op: Op, rb: ^Value, imm: u16) {
    if left, ok := value_to_number(rb^); ok {
        ra^ = value_make(op(left, f64(imm)))
    } else {
        _protect(L, pc)
        debug_arith_error(L, rb, rb)
    }
}

/*
Works for both register-register and register-constant encodings.
*/
@(private="file")
_arith :: proc(L: ^State, pc: i32, ra: ^Value, op: Op, rb, rc: ^Value) {
    try: {
        left   := value_to_number(rb^) or_break try
        right  := value_to_number(rc^) or_break try
        ra^     = value_make(op(left, right))
        return
    }
    _protect(L, pc)
    debug_arith_error(L, rb, rc)
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

    code := chunk.code
    ip   := raw_data(code)

    // Table of defined global variables.
    _G := &L.globals_table

    when DISASSEMBLE_INLINE do fmt.println("[EXECUTION]")
    for {
        i  := ip[0] // *ip
        pc := i32(find_ptr_index_unsafe(code, ip))
        ip = &ip[1] // ip++
        _print_stack(chunk, i, pc, R)

        a  := i.a
        ra := &R[a]
        op := i.op
        switch op {
        case .Move:       ra^ = R[i.b]
        case .Load_Nil:   mem.zero_slice(R[a:i.b])
        case .Load_Bool:  ra^ = value_make(bool(i.b))
        case .Load_Imm:   ra^ = value_make(f64(i.u.bx))
        case .Load_Const: ra^ = K[i.u.bx]

        case .Get_Global:
            k := K[i.u.bx]
            _protect(L, pc)
            if v, ok := vm_get_table(L, _G, k); !ok {
                what := value_get_string(k)
                debug_runtime_error(L, "Attempt to read undefined global '%s'", what)
            } else {
                ra^ = v;
            }

        case .Set_Global:
            _protect(L, pc)
            vm_set_table(L, _G, K[i.u.bx], ra^)

        case .New_Table:
            hash_count  := 1 << (i.b - 1) if i.b != 0 else 0
            array_count := 1 << (i.c - 1) if i.c != 0 else 0
            t := table_new(L, hash_count=hash_count, array_count=array_count)
            ra^ = value_make(t)

        case .Get_Table:        _get_table(L, pc, ra, &R[i.b], R[i.c])
        case .Get_Field:        _get_table(L, pc, ra, &R[i.b], K[i.c])
        case .Set_Table:        _set_table(L, pc, ra, R[i.b], R[i.c])
        case .Set_Table_Const:  _set_table(L, pc, ra, R[i.b], K[i.c])
        case .Set_Field:        _set_table(L, pc, ra, K[i.b], R[i.c])
        case .Set_Field_Const:  _set_table(L, pc, ra, K[i.b], K[i.c])

        // Unary
        case .Len:
            #partial switch rb := &R[i.b]; value_type(rb^) {
            case .String:
                n  := value_get_ostring(rb^).len
                ra^ = value_make(f64(n))

            case .Table:
                n  := table_len(value_get_table(rb^))
                ra^ = value_make(f64(n))
            case:
                _protect(L, pc)
                debug_type_error(L, "get length of", rb)
            }

        case .Not: ra^ = value_make(value_is_falsy(R[i.b]))
        case .Unm:
            rb := &R[i.b]
            if n, ok := value_to_number(rb^); ok {
                ra^ = value_make(number_unm(n))
            } else {
                _protect(L, pc)
                debug_arith_error(L, rb, rb)
            }

        // Arithmetic (register-immediate)
        case .Add_Imm: _arith_imm(L, pc, ra, number_add, &R[i.b], i.c)
        case .Sub_Imm: _arith_imm(L, pc, ra, number_sub, &R[i.b], i.c)

        // Arithmetic (register-constant)
        case .Add_Const: _arith(L, pc, ra, number_add, &R[i.b], &K[i.c])
        case .Sub_Const: _arith(L, pc, ra, number_sub, &R[i.b], &K[i.c])
        case .Mul_Const: _arith(L, pc, ra, number_mul, &R[i.b], &K[i.c])
        case .Div_Const: _arith(L, pc, ra, number_div, &R[i.b], &K[i.c])
        case .Mod_Const: _arith(L, pc, ra, number_mod, &R[i.b], &K[i.c])
        case .Pow_Const: _arith(L, pc, ra, number_pow, &R[i.b], &K[i.c])

        // Arithmetic (register-register)
        case .Add: _arith(L, pc, ra, number_add, &R[i.b], &R[i.c])
        case .Sub: _arith(L, pc, ra, number_sub, &R[i.b], &R[i.c])
        case .Mul: _arith(L, pc, ra, number_mul, &R[i.b], &R[i.c])
        case .Div: _arith(L, pc, ra, number_div, &R[i.b], &R[i.c])
        case .Mod: _arith(L, pc, ra, number_mod, &R[i.b], &R[i.c])
        case .Pow: _arith(L, pc, ra, number_pow, &R[i.b], &R[i.c])

        // Comparison
        // case .Eq..=.Geq:
        //     unreachable("Unimplemented: %v", i.op)

        case .Concat:
            args := R[i.b:i.c]
            _protect(L, pc)
            vm_concat(L, ra, args)

        // Control flow
        case .Call:
            arg_first := int(a)   + 1
            arg_count := int(i.b) + VARIADIC
            ret_count := int(i.c) + VARIADIC

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
            run_call(L, ra, arg_count, ret_count)

        case .Return:
            ret_first := int(a)
            ret_count := int(i.b) + VARIADIC
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

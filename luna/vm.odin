#+private package
package luna

import "core:fmt"
import "core:mem"

vm_execute :: proc(L: ^State, chunk: ^Chunk) {
    ip := raw_data(chunk.code)
    R  := L.stack[:]
    K  := chunk.constants[:]
    for {
        i  := ip[0]
        pc := mem.ptr_sub(ip, raw_data(chunk.code))
        ip = &ip[1]

        A := i.A

        disassemble(chunk, i, pc)
        switch i.op {
        case .Move:       R[A] = R[i.B]
        case .Load_Nil:   R[A] = nil
        case .Load_Bool:  R[A] = bool(i.B)
        case .Load_Const: R[A] = K[i.u.Bx]

        // Arithmetic (register-register, f64)
        case .Unm_f64:  R[A] = -R[i.B].(f64)
        case .Add_f64:  R[A] = R[i.B].(f64) + R[i.C].(f64)
        case .Sub_f64:  R[A] = R[i.B].(f64) - R[i.C].(f64)
        case .Mul_f64:  R[A] = R[i.B].(f64) * R[i.C].(f64)
        case .Div_f64:  R[A] = R[i.B].(f64) / R[i.C].(f64)

        // Arithmetic (register-register, int)
        case .Unm_int:  R[A] = -R[i.B].(int)
        case .Add_int:  R[A] = R[i.B].(int) + R[i.C].(int)
        case .Sub_int:  R[A] = R[i.B].(int) - R[i.C].(int)
        case .Mul_int:  R[A] = R[i.B].(int) * R[i.C].(int)
        case .Div_int:  R[A] = R[i.B].(int) / R[i.C].(int)
        case .Mod_int:  R[A] = R[i.B].(int) % R[i.C].(int)

        // Bitwise (register-register, int)
        case .Bnot: R[A] = ~R[i.B].(int)
        case .Band: R[A] =  R[i.B].(int) & R[i.C].(int)
        case .Bor:  R[A] =  R[i.B].(int) | R[i.C].(int)
        case .Bxor: R[A] =  R[i.B].(int) ~ R[i.C].(int)

        case .Return:
            for v, i in R[A:A + i.B] {
                if i > 0 {
                    fmt.print(", ", flush=false)
                }
                fmt.print(v, flush=false)
            }
            fmt.println()
            return
        }
    }
}

disassemble :: proc(chunk: ^Chunk, i: Instruction, pc: int) {
    op   := i.op
    info := OPCODE_INFO[op]

    fmt.printf("%-12s ", op, flush=false)
    switch info.form {
    case .ABC:
        fmt.printf("% -3i % -3i", i.A, i.B, flush=false)
        if info.c != nil {
            fmt.printf(" % -3i", i.C, flush=false)
        }
    case .ABCk: fmt.printf("% -3i % -3i % -3i %v", i.A, i.B, i.k.C, i.k.k, flush=false)
    case .ABx:  fmt.printf("% -3i % -8i", i.A, i.u.Bx, flush=false)
    case .AsBx: fmt.printf("% -3i % -8i", i.A, i.s.Bx, flush=false)
    }
    fmt.println()
}

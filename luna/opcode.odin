#+private package
package luna

Opcode :: enum u8 {
    Move,       // A B  | R[A] := R[B]
    Load_Const, // A Bx | R[A] := K[Bx]

    // Arithmetic (register-register, f64)
    Add_f64,    // A B C | R[A] := R[B].(f64) + R[C].(f64)
    Sub_f64,    // A B C | R[A] := R[B].(f64) - R[C].(f64)
    Mul_f64,    // A B C | R[A] := R[B].(f64) * R[C].(f64)
    Div_f64,    // A B C | R[A] := R[B].(f64) / R[C].(f64)

    // Arithmetic (register-register, int)
    Add_int,    // A B C | R[A] := R[B].(int) + R[C].(int)
    Sub_int,    // A B C | R[A] := R[B].(int) - R[C].(int)
    Mul_int,    // A B C | R[A] := R[B].(int) * R[C].(int)
    Div_int,    // A B C | R[A] := R[B].(int) / R[C].(int)
    Mod_int,    // A B C | R[A] := R[B].(int) % R[C].(int)

    // Control Flow
    Return,     // A B   | return R[A:A+B]
}

Instruction :: struct #raw_union {
    using base: Instruction_ABC,
    k: Instruction_ABCk,
    u: Instruction_ABx,
    s: Instruction_AsBx,
}

Instruction_ABC :: bit_field u32 {
    op: Opcode | 6,
    A:  u16    | 8,
    C:  u16    | 9,
    B:  u16    | 9,
}

Instruction_ABCk :: bit_field u32 {
    op: Opcode | 6,
    A:  u16    | 8,
    C:  u16    | 8,
    k:  bool   | 1,
    B:  u16    | 9,
}

Instruction_ABx :: bit_field u32 {
    op: Opcode | 6,
    A:  u16    | 8,
    Bx: u32    | 18,
}

Instruction_AsBx :: bit_field u32 {
    op: Opcode | 6,
    A:  u16    | 8,
    Bx: i32    | 18,
}

Opcode_Form :: enum u8 {
    ABC, ABCk, ABx, AsBx,
}

Opcode_Arg :: enum u8 {
    Unused, Reg, Const, Imm,
}

Opcode_Info :: bit_field u8 {
    form: Opcode_Form | 2,
    a:    bool        | 1,
    c:    Opcode_Arg  | 2,
    b:    Opcode_Arg  | 2,
}

OPCODE_INFO := [Opcode]Opcode_Info{
    .Move       = {form=.ABC, a=true, b=.Reg},
    .Load_Const = {form=.ABx, a=true, b=.Const},

    // Arithmetic (register-register)
    .Add_f64..=.Mod_int = {form=.ABC, a=true, b=.Reg, c=.Reg},
    .Return = {form=.ABC, b=.Imm},
}

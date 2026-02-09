#+private package
package luna

Opcode :: enum u8 {
    Move,       // A B  | R[A] := R[B]
    Load_Nil,   // A    | R[A] := nil
    Load_Bool,  // A B  | R[A] := bool(B)
    Load_Const, // A Bx | R[A] := K[Bx]

    // Arithmetic (register-register, f64)
    Unm_f64,    // A B   | R[A] := R[B].(f64)
    Add_f64,    // A B C | R[A] := R[B].(f64) + R[C].(f64)
    Sub_f64,    // A B C | R[A] := R[B].(f64) - R[C].(f64)
    Mul_f64,    // A B C | R[A] := R[B].(f64) * R[C].(f64)
    Div_f64,    // A B C | R[A] := R[B].(f64) / R[C].(f64)

    // Arithmetic (register-register, int)
    Unm_int,    // A B   | R[A] := R[B].(int)
    Add_int,    // A B C | R[A] := R[B].(int) + R[C].(int)
    Sub_int,    // A B C | R[A] := R[B].(int) - R[C].(int)
    Mul_int,    // A B C | R[A] := R[B].(int) * R[C].(int)
    Div_int,    // A B C | R[A] := R[B].(int) / R[C].(int)
    Mod_int,    // A B C | R[A] := R[B].(int) % R[C].(int)

    // Bitwise (register-register, int)
    Bnot,       // A B   | R[A] := ~R[B].(int)
    Band,       // A B   | R[A] := R[B].(int) & R[C].(int)
    Bor,        // A B   | R[A] := R[B].(int) | R[C].(int)
    Bxor,       // A B   | R[A] := R[B].(int) ^ R[C].(int)

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
    .Load_Nil   = {form=.ABC, a=true},
    .Load_Bool  = {form=.ABC, a=true, b=.Imm},
    .Load_Const = {form=.ABx, a=true, b=.Const},

    // Arithmetic (register-register)
    .Unm_f64            = {form=.ABC, a=true, b=.Reg},
    .Add_f64..=.Div_f64 = {form=.ABC, a=true, b=.Reg, c=.Reg},
    .Unm_int            = {form=.ABC, a=true, b=.Reg},
    .Add_int..=.Mod_int = {form=.ABC, a=true, b=.Reg, c=.Reg},

    // Bitwise (register-register)
    .Bnot         = {form=.ABC, a=true, b=.Reg},
    .Band..=.Bxor = {form=.ABC, a=true, b=.Reg, c=.Reg},

    // Control Flow
    .Return = {form=.ABC, b=.Imm},
}

#+private package
package lulu

Opcode :: enum u8 {
// Op               Args    | Side-effects
Move,           //  A B     | R[A] := R[B]
Load_Nil,       //  A B     | R[A:B] := nil
Load_Bool,      //  A B     | R[A] := (Bool)B
Load_Imm,       //  A Bx    | R[A] := (Number)Bx
Load_Const,     //  A Bx    | R[A] := K[Bx]
Get_Global,     //  A Bx    | R[A] := _G[K[Bx]]
Get_Table,      //  A B C   | R[A] := R[B][R[C]]
Get_Field,      //  A B C   | R[A] := R[B][K[C]]
Set_Global,     //  A Bx    | _G[K[Bx]]  := R[A]
Set_Table,      //  A B C k | R[A][R[B]] := R[C]
Set_Table_Const,//  A B C   | R[A][R[B]] := K[C]
Set_Field,      //  A B C k | R[A][K[B]] := R[C]
Set_Field_Const,//  A B C   | R[A][K[B]] := K[C]
New_Table,      //  A B C   | R[A] := {} ; #hash=B, #array=C

// Unary
Len,    //  A B     | R[A] := #R[B]
Not,    //  A B     | R[A] := not R[B]
Unm,    //  A B     | R[A] := -R[B]

// Arithmetic (register-immediate)
Add_Imm,    //  A B C   | R[A] := R[B] + (Number)C
Sub_Imm,    //  A B C   | R[A] := R[B] - (Number)C

// Arithmetic (register-constant)
Add_Const,  //  A B C   | R[A] := R[B] + K[C]
Sub_Const,  //  A B C   | R[A] := R[B] - K[C]
Mul_Const,  //  A B C   | R[A] := R[B] * K[C]
Div_Const,  //  A B C   | R[A] := R[B] / K[C]
Mod_Const,  //  A B C   | R[A] := R[B] % K[C]
Pow_Const,  //  A B C   | R[A] := R[B] ^ K[C]

// Arithmetic (register-register)
Add,        //  A B C   | R[A] := R[B] + R[C]
Sub,        //  A B C   | R[A] := R[B] - R[C]
Mul,        //  A B C   | R[A] := R[B] * R[C]
Div,        //  A B C   | R[A] := R[B] / R[C]
Mod,        //  A B C   | R[A] := R[B] % R[C]
Pow,        //  A B C   | R[A] := R[B] ^ R[C]

// Comparison
// Eq,         //  A B C
// Neq,        //  A B C
// Lt,         //  A B C
// Gt,         //  A B C
// Leq,        //  A B C
// Geq,        //  A B C

Concat,     //  A B C   | R[A] := concat R[i] for B <= i < C

// Control Flow
Call,           //  A B C   | R[A : A+C-1]:= R[A]( R[A+1 : A+B-1] ) ; (*) See note.
Jump,           //    sBx   | ip += sBx
Jump_If_False,  //  A sBx   | ip += sBx if not R[A] else 1 ; (*) See note.
Return,         //  A B     | return R[A:A+B-1] ; (*) See note.
}

/*
Instruction format inspired by Lua 5.4 and 5.5:
```
tens    | 3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0 |
ones    | 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 |
A B C   |       B(8)     |        C(10)      |      A(8)     |    Op(6)   |
A Bx    |                Bx(18)              |      A(8)     |    Op(6)   |
A sBx   |               sBx(18)              |      A(8)     |    Op(6)   |
```

's' stands for "signed", 'x' stands for "extended".

Unlike Lua 5.1, we do not, for the most part, make use of RK registers in
order to simplify instruction decoding. So we follow Lua 5.4 and 5.5 in
having dedicated instructions for woking with various operand combinations.

Operand B will never function as an RK.
Operand C will only function as RK in table manipulation instructions.

**Notes**
(*) .Call, .Return:
    - Arguments B and C are encoded in terms of `VARIADIC` (a.k.a. `-1`).
    - When encoding we add 1 (or subtract `VARIADIC`), i.e. `B - 1` and `C - 1`,
    in order to reliably represent `-1` in the unsigned operand.
    - When decoding, we first treat the operand as a signed integer and then
    subtract 1 (or add `VARIADIC`) to get the intended count.

(*) .Jump:
    - the `ip += 1` business is assuming that the next instruction is an
    unconditional jump or part of an 'else' block , so we want to skip it.
 */
Instruction :: struct #raw_union {
    using base: Instruction_ABC,

    // Extended, unsigned.
    u: Instruction_ABx,

    // Extended, signed.
    s: Instruction_AsBx,
}

Instruction_ABC :: bit_field u32 {
    op: Opcode | SIZE_OP,
    A:  u16    | SIZE_A,
    C:  u16    | SIZE_C,
    B:  u16    | SIZE_B,
}

Instruction_ABx :: bit_field u32 {
    op: Opcode | SIZE_OP,
    A:  u16    | SIZE_A,
    Bx: u32    | SIZE_Bx,
}

Instruction_AsBx :: bit_field u32 {
    op: Opcode | SIZE_OP,
    A:  u16    | SIZE_A,
    Bx: i32    | SIZE_Bx,
}

SIZE_OP  :: 6
SIZE_A   :: 8
SIZE_C   :: 10
SIZE_B   :: 8
SIZE_Bx  :: SIZE_B + SIZE_C
SIZE_J   :: SIZE_A + SIZE_B + SIZE_C

MAX_A   :: (1 << SIZE_A)  - 1
MAX_B   :: (1 << SIZE_B)  - 1
MAX_C   :: (1 << SIZE_C)  - 1
MAX_Bx  :: (1 << SIZE_Bx) - 1
MAX_sBx :: MAX_Bx >> 1
MIN_sBx :: 0 - MAX_Bx

MAX_REG     :: MAX_A
MAX_IMM_C   :: MAX_C
MAX_IMM_Bx  :: MAX_Bx

Op_Info :: bit_field u8 {
    mode: Op_Format | 2, // What operands are we using?
    a:    bool      | 1, // Operand A: `true` iff used as destination register
    c:    Op_Mode   | 2,
    b:    Op_Mode   | 2,
}

Op_Format :: enum u8 {
    ABC, ABx, AsBx,
}

Op_Mode :: enum {
    None,   // Operand is unused? E.g. operand C given that Bx is active.
    Reg,    // (R): Operand is a (virtual) register in the VM stack?
    Const,  // (K): Operand is the index of a value in the constants array?
    Imm,    // (I): Operand is an immediate positive integer value?
}


OP_INFO := [Opcode]Op_Info{
    .Move            = {mode=.ABC, a=true,  b=.Reg},
    .Load_Nil        = {mode=.ABC, a=true,  b=.Reg},
    .Load_Bool       = {mode=.ABC, a=true,  b=.Imm},
    .Load_Imm        = {mode=.ABx, a=true,  b=.Imm},
    .Load_Const      = {mode=.ABx, a=true,  b=.Const},
    .Get_Global      = {mode=.ABx, a=true,  b=.Const},
    .Get_Table       = {mode=.ABC, a=true,  b=.Reg,   c=.Reg},
    .Get_Field       = {mode=.ABC, a=true,  b=.Reg,   c=.Const},
    .Set_Global      = {mode=.ABx, a=false, b=.Const},
    .Set_Table       = {mode=.ABC, a=false, b=.Reg,   c=.Reg},
    .Set_Table_Const = {mode=.ABC, a=false, b=.Reg,   c=.Const},
    .Set_Field       = {mode=.ABC, a=false, b=.Const, c=.Reg},
    .Set_Field_Const = {mode=.ABC, a=false, b=.Const, c=.Const},
    .New_Table       = {mode=.ABC, a=true,  b=.Imm},

    // Unary
    .Len..=.Unm = {mode=.ABC, a=true, b=.Reg},

    // Arithmetic (register-immediate)
    .Add_Imm..=.Sub_Imm = {mode=.ABC, a=true, b=.Reg, c=.Imm},

    // Arithmetic (register-constant)
    .Add_Const..=.Pow_Const = {mode=.ABC, a=true, b=.Reg, c=.Const},

    // Arithmetic (register-register)
    .Add..=.Pow = {mode=.ABC, a=true, b=.Reg, c=.Reg},
    .Concat     = {mode=.ABC, a=true, b=.Reg, c=.Reg},

    // Control flow
    .Call           = {mode=.ABC,   a=true,  b=.Imm, c=.Imm},
    .Jump           = {mode=.AsBx,  a=false, b=.Imm},
    .Jump_If_False  = {mode=.AsBx,  a=false, b=.Imm},
    .Return         = {mode=.ABC,   a=false, b=.Imm},
}

instruction_eq :: proc(i1, i2: Instruction) -> bool {
    return i1.base == i2.base
}

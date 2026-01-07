#+private package
package lulu

Opcode :: enum u8 {
// Op           Args    | Side-effects
Move,       //  A B     | R[A] := R[B]
Load_Nil,   //  A B     | R[A:B] := nil
Load_Bool,  //  A B     | R[A] := (Bool)R[B]
Load_Imm,   //  A Bx    | R[A] := (Number)Bx
Load_Const, //  A Bx    | R[A] := K[Bx]
Get_Global, //  A Bx    | R[A] := _G[K[Bx]]
Set_Global, //  A Bx    | _G[K[Bx]] := R[A]

// Unary
Len,        //  A B     | R[A] := #R[B]
Not,        //  A B     | R[A] := not R[B]
Unm,        //  A B     | R[A] := -R[B]

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
Call,       //  A B C   | R[A : A+C-1]:= R[A]( R[A+1 : A+B-1] ) ; (*) See note.
Return,     //  A B     | return R[A:A+B-1] ; (*) See note.
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
(*) OP_CALL, OP_RETURN:
    - Arguments B and C are encoded in terms of `VARIADIC` (a.k.a. `-1`).
    - When encoding we add 1 (or subtract `VARIADIC`), i.e. `B - 1` and `C - 1`,
    in order to reliably represent `-1` in the unsigned operand.
    - When decoding, we first treat the operand as a signed integer and then
    subtract 1 (or add `VARIADIC`) to get the intended count.
 */
Instruction :: struct #raw_union {
    using base: Instruction_ABC,

    // Extended
    x: Instruction_ABx,
}

Instruction_ABC :: bit_field u32 {
    op: Opcode | SIZE_OP,
    a:  u16    | SIZE_A,
    c:  u16    | SIZE_C,
    b:  u16    | SIZE_B,
}

Instruction_ABx :: bit_field u32 {
    op: Opcode | SIZE_OP,
    a:  u16    | SIZE_A,
    bx: u32    | SIZE_Bx,
}

SIZE_OP  :: 6
SIZE_A   :: 8
SIZE_C   :: 10
SIZE_B   :: 8
SIZE_Bx  :: SIZE_B + SIZE_C

MAX_A   :: (1 << SIZE_A)  - 1
MAX_B   :: (1 << SIZE_B)  - 1
MAX_C   :: (1 << SIZE_C)  - 1
MAX_Bx  :: (1 << SIZE_Bx) - 1
MAX_sBx :: MAX_Bx >> 1
MIN_sBx :: 0 - MAX_Bx

MAX_REG     :: MAX_A
MAX_K_C     :: MAX_C
MAX_IMM_C   :: MAX_C
MAX_IMM_Bx  :: MAX_Bx

Op_Info :: bit_field u8 {
    mode: Op_Format | 2, // What operands are we using?
    a:    bool      | 1, // Operand A: `true` iff used as destination register
    c:    Op_Mode   | 2,
    b:    Op_Mode   | 2,
}

Op_Format :: enum {
    ABC, ABx, AsBx
}

Op_Mode :: enum {
    None,   // Operand is unused? E.g. operand C given that Bx is active.
    Reg,    // (R): Operand is a (virtual) register in the VM stack?
    Const,  // (K): Operand is the index of a value in the constants array?
    Imm,    // (I): Operand is an immediate positive integer value?
}


OP_INFO := [Opcode]Op_Info{
    .Move       = {mode=.ABC, a=true,  b=.Reg},
    .Load_Nil   = {mode=.ABC, a=true,  b=.Reg},
    .Load_Bool  = {mode=.ABC, a=true,  b=.Imm},
    .Load_Imm   = {mode=.ABx, a=true,  b=.Imm},
    .Load_Const = {mode=.ABx, a=true,  b=.Const},
    .Get_Global = {mode=.ABx, a=true,  b=.Const},
    .Set_Global = {mode=.ABx, a=false, b=.Const},

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
    .Call       = {mode=.ABC, a=true,  b=.Imm, c=.Imm},
    .Return     = {mode=.ABC, a=false, b=.Imm},
}

instruction_make_abc :: proc(op: Opcode, a, b, c: u16) -> Instruction {
    i := Instruction{base={op=op, a=a, b=b, c=c}}
    return i
}

instruction_make_abx :: proc(op: Opcode, a: u16, bx: u32) -> Instruction {
    i := Instruction{x={op=op, a=a, bx=bx}}
    return i
}

instruction_make_asbx :: proc(op: Opcode, a: u16, sbx: i32) -> Instruction {
    return instruction_make_abx(op, a, u32(sbx + MAX_sBx))
}

instruction_eq :: proc(i1, i2: Instruction) -> bool {
    return i1.base == i2.base
}

// // Required for RK registers to work.
// #assert(SIZE_B == SIZE_C)

// SIZE_RK :: SIZE_B
// BIT_RK  :: 1 << (SIZE_RK - 1)
// MASK_RK :: BIT_RK - 1

// reg_is_k :: proc(r: u16) -> (is_k: bool) {
//     return r & BIT_RK != 0
// }

// reg_to_rk :: proc(r: u16) -> (rk: u16) {
//     return r | BIT_RK
// }

// reg_get_k :: proc(r: u16) -> (k: u16) {
//     return r & MASK_RK
// }

// get_rkb :: proc(i: Instruction) -> (b: u16, is_k: bool) {
//     b    = i.b
//     is_k = reg_is_k(b)
//     if is_k {
//         b = reg_get_k(b)
//     }
//     return b, is_k
// }

// get_rkc :: proc(i: Instruction) -> (c: u16, is_k: bool) {
//     c    = i.c
//     is_k = reg_is_k(c)
//     if is_k {
//         c = reg_get_k(c)
//     }
//     return c, is_k
// }

getarg_sbx :: proc(i: Instruction) -> i32 {
    bx  := i.x.bx
    sbx := i32(bx) - MAX_Bx
    return sbx
}

setarg_sbx :: proc(i: ^Instruction, sbx: i32) {
    assert(MIN_sBx <= sbx && sbx <= MAX_sBx)
    i.x.bx = u32(sbx + MAX_Bx)
}

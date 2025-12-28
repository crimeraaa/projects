#+private package
package lulu

OpCode :: enum u8 {
    Move,
    Load_Bool,
    Load_Const,

    // Unary
    Len, Not, Unm,

    // Arithmetic
    Add, Sub, Mul, Div, Mod, Pow,

    // Comparison
    Eq, Lt, Leq, Neq, Gt, Geq,

    Return,
}

SIZE_OP  :: 6
SIZE_A   :: 8
SIZE_B   :: 9
SIZE_C   :: 9
SIZE_Bx  :: SIZE_B + SIZE_C
SIZE_sBx :: SIZE_Bx

MASK_A  :: (1 << SIZE_A)  - 1
MASK_B  :: (1 << SIZE_B)  - 1
MASK_C  :: (1 << SIZE_C)  - 1

MAX_A   :: MASK_A
MAX_B   :: MASK_B
MAX_C   :: MASK_C
MAX_Bx  :: (1 << SIZE_Bx) - 1
MAX_sBx :: MAX_Bx >> 1

/*
Format:

+-----------+-----------+-----------+-----------+
| [31...23] | [22...16] | [15...07] | [06...00] |
+-----------+-----------+-----------+-----------+
| C         | B         | a         | op        |
+-----------+-----------+-----------+-----------+
| Bx                    | a         | op        |
+-----------+-----------+-----------+-----------+
| sBx                   | a         | op        |
+-----------+-----------+-----------+-----------+
 */
Instruction :: bit_field u32 {
    op: OpCode | SIZE_OP,
    a:  u16    | SIZE_A,
    b:  u16    | SIZE_B,
    c:  u16    | SIZE_C,
}

instruction_make :: proc {
    instruction_make_abc,
    instruction_make_abx,
}

instruction_make_abc :: proc(op: OpCode, a, b, c: u16) -> Instruction {
    i := Instruction{op=op, a=a, b=b, c=c}
    return i
}

instruction_make_abx :: proc(op: OpCode, a: u16, bx: u32) -> Instruction {
    hi := cast(u16)(bx >> SIZE_C) & MASK_C
    lo := cast(u16)(bx & MASK_B)
    i  := Instruction{op=op, a=a, b=lo, c=hi}
    return i
}

instruction_get_bx :: proc(i: Instruction) -> u32 {
    hi := cast(u32)i.c << SIZE_C
    lo := cast(u32)i.b
    return hi | lo
}

instruction_set_bx :: proc(i: ^Instruction, bx: u32) {
    hi := cast(u16)(bx >> SIZE_C) & MASK_C
    lo := cast(u16)(bx & MASK_B)

    i.b = lo
    i.c = hi
}

instruction_get_sbx :: proc(i: Instruction) -> i32 {
    bx  := instruction_get_bx(i)
    sbx := cast(i32)(bx - MAX_Bx)
    return sbx
}

instruction_set_sbx :: proc(i: ^Instruction, sbx: i32) {
    bx := cast(u32)(sbx + MAX_Bx)
    instruction_set_bx(i, bx)
}

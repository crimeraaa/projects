#+private package
package lulu

Opcode :: enum u8 {
// Op           Args    | Side-effects
Move,       //  A B     | R(A) := R(B)
Load_Nil,   //  A B     | R(i) := nil for i in [A,B)
Load_Bool,  //  A B     | R(A) := (Bool)R(B)
Load_Const, //  A Bx    | R(A) := K[Bx]
// Unary
Len,        //  A B     | R(A) := #R(B)
Not,        //  A B     | R(A) := not R(B)
Unm,        //  A B     | R(A) := -R(B)
// Arithmetic
Add,        //  A B C   | R(A) := RK(B) + RK(C)
Sub,        //  A B C   | R(A) := RK(B) - RK(C)
Mul,        //  A B C   | R(A) := RK(B) * RK(C)
Div,        //  A B C   | R(A) := RK(B) / RK(C)
Mod,        //  A B C   | R(A) := RK(B) % RK(C)
Pow,        //  A B C   | R(A) := RK(B) ^ RK(C)
// Comparison
Eq,         //  A B C
Neq,        //  A B C
Lt,         //  A B C
Gt,         //  A B C
Leq,        //  A B C
Geq,        //  A B C
// Control Flow
Return,     //  A B     | return R(A..<B)
}

OP_SIZE  :: 6
A_SIZE   :: 8
B_SIZE   :: 9
C_SIZE   :: 9
Bx_SIZE  :: B_SIZE + C_SIZE
sBx_SIZE :: Bx_SIZE

A_MAX   :: (1 << A_SIZE)  - 1
B_MAX   :: (1 << B_SIZE)  - 1
C_MAX   :: (1 << C_SIZE)  - 1
Bx_MAX  :: (1 << Bx_SIZE) - 1
sBx_MAX :: Bx_MAX >> 1
sBx_MIN :: 0 - Bx_MAX

/*
Format:
```
+-----------+-----------+-----------+-----------+
| [31...23] | [22...16] | [15...07] | [06...00] |
+-----------+-----------+-----------+-----------+
| c         | b         | a         | op        |
+-----------+-----------+-----------+-----------+
| Bx                    | a         | op        |
+-----------+-----------+-----------+-----------+
| sBx                   | a         | op        |
+-----------+-----------+-----------+-----------+
```
 */
Instruction :: bit_field u32 {
    op: Opcode | OP_SIZE,
    a:  u16    | A_SIZE,
    b:  u16    | B_SIZE,
    c:  u16    | C_SIZE,
}

instruction_make_abc :: proc(op: Opcode, a, b, c: u16) -> Instruction {
    i := Instruction{op=op, a=a, b=b, c=c}
    return i
}

instruction_make_abx :: proc(op: Opcode, a: u16, bx: u32) -> Instruction {
    i := Instruction{op=op, a=a}
    set_bx(&i, bx)
    return i
}

instruction_make_sbx :: proc(op: Opcode, a: u16, sbx: i32) -> Instruction {
    i := Instruction{op=op, a=a}
    set_sbx(&i, sbx)
    return i
}

// Required for RK registers to work.
#assert(B_SIZE == C_SIZE)

RK_SIZE :: B_SIZE
RK_BIT  :: 1 << (RK_SIZE - 1)
RK_MASK :: RK_BIT - 1

reg_is_k :: proc(r: u16) -> (is_k: bool) {
    return r & RK_BIT != 0
}

reg_to_rk :: proc(r: u16) -> (rk: u16) {
    return r | RK_BIT
}

reg_get_k :: proc(r: u16) -> (k: u16) {
    return r & RK_MASK
}

get_rkb :: proc(i: Instruction) -> (b: u16, is_k: bool) {
    b    = i.b
    is_k = reg_is_k(b)
    if is_k {
        b = reg_get_k(b)
    }
    return b, is_k
}

get_rkc :: proc(i: Instruction) -> (c: u16, is_k: bool) {
    c    = i.c
    is_k = reg_is_k(c)
    if is_k {
        c = reg_get_k(c)
    }
    return c, is_k
}

get_bx :: proc(i: Instruction) -> u32 {
    hi := cast(u32)i.c << C_SIZE
    lo := cast(u32)i.b
    return hi | lo
}

set_bx :: proc(i: ^Instruction, bx: u32) {
    assert(bx <= Bx_MAX)

    // Bit fields already have masking semantics, so no need to do them here.
    hi := cast(u16)(bx >> C_SIZE)
    lo := cast(u16)bx

    i.b = lo
    i.c = hi
}

get_sbx :: proc(i: Instruction) -> i32 {
    bx  := get_bx(i)
    sbx := cast(i32)bx - Bx_MAX
    return sbx
}

set_sbx :: proc(i: ^Instruction, sbx: i32) {
    assert(sBx_MIN <= sbx && sbx <= sBx_MAX)

    bx := cast(u32)(sbx + Bx_MAX)
    set_bx(i, bx)
}

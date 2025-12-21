#include "checked.h"

bool
u64_checked_add(u64 *dst, u64 a, u64 b)
{
    bool carry;

    // Overflow check (64-bit unsigned addition)
    //
    //      a + b > max(u64)
    //
    // Can be rewritten as:
    //      a > max(u64) - b
    //
    // ...and this will never overflow because if `b == max(u64)` then
    // the expression expands to `a > 0`.
    *dst  = a + b;
    carry = a > U64_MAX - b;
    return carry;
}

bool
u64_checked_add_carry(u64 *dst, u64 a, u64 b, u64 carry)
{
    bool c1, c2;

    // Overflow check (64-bit unsigned addition with carry):
    //
    //      a + b + carry > max(u64)
    //
    // However, either the intermediate additions or the full expression
    // itself may overflow. So we only check the full expression if we
    // know that `a + b` doesn't overflow:
    //
    //      ((a + b) > max(u64)) || ((a + b) + carry > max(u64))
    //
    c1 = u64_checked_add(dst, a, b);
    c2 = u64_checked_add(dst, *dst, carry);
    return c1 || c2;
}

bool
u64_checked_sub(u64 *dst, u64 a, u64 b)
{
    bool carry;
    // a - b < 0
    // a < b
    *dst  = a - b;
    carry = a < b;
    return carry;
}

bool
u64_checked_sub_carry(u64 *dst, u64 a, u64 b, u64 carry)
{
    bool c1, c2;

    // Overflow check (64-bit unsigned subtraction with carry)
    //
    //      (a - b) - carry < 0
    //
    // However, either the intermediate subtractions or the full expression
    // may overflow. So we can only check the full expression if we know
    // that `a - b` doesn't overflow:
    //
    //      ((a - b) < 0) || ((a - b) - carry < 0)
    //
    c1 = u64_checked_sub(dst, a, b);
    c2 = u64_checked_sub(dst, *dst, carry);
    return c1 || c2;
}

bool
u64_checked_mul(u64 *dst, u64 a, u64 b)
{
    bool carry;

    // Overflow check (64-bit unsigned multiplication):
    //
    //      a * b > max(u64)
    //
    // Can be rewritten as:
    //      a > max(u64) / b
    //
    // ...except for `b == 0`. If either operand is zero we know the
    // product is automatically zero.
    *dst  = a * b;
    carry = (a > 0 && b > 0) && (a > U64_MAX / b);
    return carry;
}

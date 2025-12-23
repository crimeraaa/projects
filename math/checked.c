#include "checked.h"

bool
u64_checked_add(u64 *dst, u64 a, u64 b)
{
    u64 sum;
    bool carry;

    // Overflow check (64-bit unsigned addition)
    //
    //      a + b > UMAX
    //      a > UMAX - b
    //
    // ...and this will never overflow because if `b == UMAX` then
    // the expression expands to `a > 0`.
    sum   = a + b;
    carry = a > U64_MAX - b;
    *dst  = sum;
    return carry;
}

bool
u64_checked_add_carry(u64 *dst, u64 a, u64 b, u64 carry)
{
    u64 sum;
    bool c1, c2;

    // Overflow check (64-bit unsigned addition with carry):
    //
    //      a + b + carry > MAX
    //
    // However, either the intermediate additions or the full expression
    // itself may overflow. So if `a + b` overflows then we know the full
    // expression overflows already, otherwise check the full expression.
    //
    //      sum := a + b
    //      sum > MAX || sum + carry > MAX
    //      sum > MAX || sum > MAX - carry
    //
    c1   = u64_checked_add(&sum, a, b);
    c2   = u64_checked_add(&sum, sum, carry);
    *dst = sum;
    return c1 || c2;
}

bool
i64_checked_add(i64 *dst, i64 a, i64 b)
{
    i64 sum;
    bool a_sign, b_sign, sum_sign, overflow = false;

    // Overflow check (64-bit signed addition):
    //
    //      a + b < IMIN or a + b > IMAX
    //
    // Conceptually, if the signs do not match then we can never overflow.
    //
    //      0    + IMIN = IMIN
    //      IMAX + IMIN = -1
    //
    // So check only the cases where both signs match:
    //
    //      a + b < 0 when a + b > IMAX
    //
    //      e.g. IMAX + 1 < 0
    //
    //      a + b > 0 when a + b < IMIN
    //
    //      e.g. IMIN + (-1) > 0
    //
    // Thus, the overflow check can be generalized as:
    //      arguments signs match AND result sign does not match
    //
    sum      = a + b;
    a_sign   = a < 0;
    b_sign   = b < 0;
    sum_sign = sum < 0;

    overflow = a_sign == b_sign && sum_sign != a_sign;
    *dst     = sum;
    return overflow;
}

bool
i64_checked_add_carry(i64 *dst, i64 a, i64 b, i64 carry)
{
    i64 sum;
    bool c1, c2;

    // Overflow check (64-bit signed addition with carry)
    //
    //      a + b + carry < min(i64) or a + b + carry > max(i64)
    //
    c1   = i64_checked_add(&sum, a, b);
    c2   = i64_checked_add(&sum, sum, carry);
    *dst = sum;
    return c1 || c2;
}

bool
u64_checked_sub(u64 *dst, u64 a, u64 b)
{
    u64 diff;
    bool carry;
    // a - b < 0
    // a < b
    diff  = a - b;
    carry = a < b;
    *dst  = diff;
    return carry;
}

bool
u64_checked_sub_carry(u64 *dst, u64 a, u64 b, u64 carry)
{
    u64 diff;
    bool c1, c2;

    // Overflow check (64-bit unsigned subtraction with carry)
    //
    //      (a - b) - carry < 0
    //
    // However, either the intermediate subtractions or the full expression
    // may overflow. So if `a - b` overflows then we know the full expression
    // already overflows, otherwise check the full expression:
    //
    //      diff := a - b
    //      diff < 0 || diff - carry < 0
    //
    c1   = u64_checked_sub(&diff, a, b);
    c2   = u64_checked_sub(&diff, diff, carry);
    *dst = diff;
    return c1 || c2;
}

bool
i64_checked_sub(i64 *dst, i64 a, i64 b)
{
    i64 diff;
    bool a_sign, b_sign, diff_sign, overflow;

    // Overflow check (64-bit signed subtraction):
    //
    //      a - b < IMIN or a - b > IMAX
    //
    // Subtraction of the same signs will never overflow.
    //
    //      IMAX - IMAX = 0
    //      0    - IMAX = IMIN + 1
    //      IMIN - IMIN = 0
    //
    // So check only the cases where the signs differ.
    //
    //      a - b > 0 where a - b < IMIN
    //
    //      e.g. IMIN - 1 = IMAX
    //
    //      a - b < 0 where a - b > IMAX
    //
    //      e.g. IMAX - (-1) = IMIN
    //
    diff      = a - b;
    a_sign    = a < 0;
    b_sign    = b < 0;
    diff_sign = diff < 0;
    overflow  = a_sign != b_sign && diff_sign != a_sign;
    *dst      = diff;
    return overflow;
}

bool
i64_checked_sub_carry(i64 *dst, i64 a, i64 b, i64 carry)
{
    i64 diff;
    bool c1, c2;

    // Overflow check (64-bit signed subtraction with carry)
    //
    //      a - b - carry < min(i64) or a - b - carry > max(i64)
    //
    c1   = i64_checked_sub(&diff, a, b);
    c2   = i64_checked_sub(&diff, diff, carry);
    *dst = diff;
    return c1 || c2;
}

bool
u64_checked_mul(u64 *dst, u64 a, u64 b)
{
    u64 prod;
    bool carry;

    // Overflow check (64-bit unsigned multiplication):
    //
    //      a * b > UMAX
    //      a > UMAX / b
    //
    // ...except for `b == 0`. If either operand is zero we know the
    // product is automatically zero.
    prod  = a * b;
    carry = prod > 0 && a > U64_MAX / b;
    *dst  = prod;
    return carry;
}

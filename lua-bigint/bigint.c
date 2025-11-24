#include <stdio.h>  // printf
#include <stdlib.h> // malloc, realloc, free, abort
#include <string.h> // memset

#include "bigint.h"

static bool
internal_is_zero(const BigInt *bi)
{
    return bi->len == 0;
}

static bool
internal_is_neg(const BigInt *bi)
{
    return bi->sign == NEGATIVE;
}

static int
count_digits(uintmax_t value, Digit base)
{
    int count;
    if (value == 0) {
        return 1;
    }

    count = 0;
    while (value > 0) {
        value /= cast(uintmax_t)base;
        count += 1;
    }
    return count;
}


static BigInt *
internal_ensure(lua_State *L, int arg)
{
    return cast(BigInt *)luaL_checkudata(L, arg, BIGINT_MTNAME);
}

static void
internal_swap_ptr(const BigInt **a, const BigInt **b)
{
    const BigInt *tmp = *a;
    *a = *b;
    *b = tmp;
}

static void
internal_neg(BigInt *dst)
{
    Sign sign = NEGATIVE;
    if (internal_is_zero(dst) || internal_is_neg(dst)) {
        sign = POSITIVE;
    }
    dst->sign = sign;
}

// Pushes a new BigInt and returns a pointer to said instance.
static BigInt *
internal_make(lua_State *L, int digit_count)
{
    BigInt *bi;
    size_t  array_size = sizeof(bi->digits[0]) * cast(size_t)digit_count;

    // -> (..., bi: BigInt *)
    bi = cast(BigInt *)lua_newuserdata(L, sizeof(*bi) + array_size);
    bi->sign = POSITIVE;
    bi->len  = cast(size_t)digit_count;
    memset(bi->digits, 0, array_size);

    luaL_getmetatable(L, BIGINT_MTNAME); // -> (..., bi, mt: {})
    lua_setmetatable(L, -2);             // -> (..., bi) ; setmetatable(bi, mt)
    return bi;
}

static BigInt *
internal_make_copy(lua_State *L, const BigInt *src)
{
    BigInt *dst = internal_make(L, src->len);
    dst->sign = src->sign;
    memcpy(dst->digits, src->digits, sizeof(src->digits[0]) * src->len);
    return dst;
}

static BigInt *
internal_make_integer(lua_State *L, lua_Integer value)
{
    Sign sign = (value >= 0) ? POSITIVE : NEGATIVE;
    if (sign == NEGATIVE) {
        value = -value;
    }

    int digit_count = count_digits(cast(uintmax_t)value, BIGINT_DIGIT_BASE);
    BigInt *bi = internal_make(L, digit_count);
    bi->sign = sign;

    // Write `value` from LSD to MSD.
    for (size_t i = 0; i < bi->len; i += 1) {
        bi->digits[i] = cast(Digit)(value % BIGINT_DIGIT_BASE);
        value /= BIGINT_DIGIT_BASE;
    }

    return bi;
}

static Arg
arg_get(lua_State *L, int arg_i)
{
    Arg arg;
    arg.type    = ARG_INVALID;
    arg.integer = 0;
    if (lua_isuserdata(L, arg_i)) {
        // Instance metatable was pushed?
        if (lua_getmetatable(L, arg_i)) {
            luaL_getmetatable(L, BIGINT_MTNAME);
            bool res = lua_rawequal(L, -2, -1);
            // Pop instance metatable and library metatable.
            lua_pop(L, 2);
            if (res) {
                arg.type   = ARG_BIGINT;
                arg.bigint = cast(BigInt *)lua_touserdata(L, arg_i);
                return arg;
            }
        }
    } else {
        lua_Number n = lua_tonumber(L, arg_i);
        if (n != 0 || lua_isnumber(L, arg_i)) {
            lua_Integer i = cast(lua_Integer)n;
            if (cast(lua_Number)i == n) {
                arg.type    = ARG_INTEGER;
                arg.integer = i;
                return arg;
            }
            luaL_typerror(L, arg_i, "integer");
        }
    }
    luaL_typerror(L, arg_i, "BigInt");
    return arg;
}

static bool
arg_is_integer(Arg arg)
{
    return arg.type == ARG_INTEGER;
}

static bool
arg_is_bigint(Arg arg)
{
    return arg.type == ARG_BIGINT;
}

static int
internal_ctor(lua_State *L, int first_arg)
{
    Arg arg = arg_get(L, first_arg);
    if (arg_is_integer(arg)) {
        internal_make_integer(L, arg.integer);
        return 1;
    }
    return luaL_argerror(L, first_arg, "Constructor unimplemented");
}

static int
bigint_ctor(lua_State *L)
{
    return internal_ctor(L, 1);
}


/** @brief `bigint(...) => bigint.__call(bigint, ...)` */
static int
bigint_call(lua_State *L)
{
    return internal_ctor(L, 2);
}

static BigInt *
internal_clamp(BigInt *dst)
{
    // Trim leading zeroes
    while (dst->len > 0 && dst->digits[dst->len - 1] == 0) {
        // Lua tracked the initial size we allocated so this is fine.
        dst->len -= 1;
    }

    // No more digits?
    if (internal_is_zero(dst)) {
        dst->sign = POSITIVE;
    }
    return dst;
}

static Comparison
internal_cmp_bigint(const BigInt *a, const BigInt *b)
{
    bool a_is_negative = internal_is_neg(a);

    // 1.) Differing signs?
    if (a->sign != b->sign) {
        // 1.1.) (-a) <   b
        // 1.2.)   a  > (-b)
        return (a_is_negative) ? LESS : GREATER;
    }

    // 2.) Same signs, but differing lengths?
    if (a->len != b->len) {
        // 2.1.) -a < -b when #a > #b (more negative => less)
        // 2.2.) -a > -b when #a < #b (less negative => greater)
        if (a_is_negative) {
            return (a->len < b->len) ? GREATER : LESS;
        }
        // 2.3.)  a < b when #a < #b
        // 2.4.)  a > b when #a > #b
        else {
            return (a->len < b->len) ? LESS : GREATER;
        }
    }

    // 3.) Same signs and same lengths. Compare digits from MSD to LSD.
    for (size_t i = a->len; i > 0; i -= 1) {
        Digit a_digit = a->digits[i - 1];
        Digit b_digit = b->digits[i - 1];
        if (a_digit < b_digit) {
            return (a_is_negative) ? GREATER : LESS;
        } else if (a_digit > b_digit) {
            return (a_is_negative) ? LESS : GREATER;
        }
    }
    return EQUAL;
}

static bool
internal_eq_bigint(const BigInt *a, const BigInt *b)
{
    return internal_cmp_bigint(a, b) == EQUAL;
}

static bool
internal_lt_bigint(const BigInt *a, const BigInt *b)
{
    return internal_cmp_bigint(a, b) == LESS;
}

static bool
internal_le_bigint(const BigInt *a, const BigInt *b)
{
    return internal_cmp_bigint(a, b) <= EQUAL;
}

static bool
integer_fits_digit(lua_Integer b)
{
    return 0 <= b && b <= BIGINT_DIGIT_MAX;
}

typedef bool (*Compare_Fn)(const BigInt *a, const BigInt *b);

static int
bigint_compare(lua_State *L, Compare_Fn cmp_bigint)
{
    const BigInt *a = internal_ensure(L, 1);
    const BigInt *b = internal_ensure(L, 2);
    bool res = cmp_bigint(a, b);
    lua_pushboolean(L, res);
    return 1;
}

static int
bigint_eq(lua_State *L)
{
    return bigint_compare(L, internal_eq_bigint);
}

static int
bigint_lt(lua_State *L)
{
    return bigint_compare(L, internal_lt_bigint);
}

static int
bigint_le(lua_State *L)
{
    return bigint_compare(L, internal_le_bigint);
}

static Comparison
internal_cmp_bigint_abs(const BigInt *a, const BigInt *b)
{
    if (a->len != b->len) {
        return (a->len < b->len) ? LESS : GREATER;
    }

    for (size_t i = a->len; i > 0; i -= 1) {
        Digit a_digit = a->digits[i - 1];
        Digit b_digit = b->digits[i - 1];
        if (a_digit < b_digit) {
            return LESS;
        } else if (a_digit > b_digit) {
            return GREATER;
        }
    }
    return EQUAL;
}

// static bool
// internal_eq_bigint_abs(const BigInt *a, const BigInt *b)
// {
//     return internal_cmp_bigint_abs(a, b) == EQUAL;
// }

static bool
internal_lt_bigint_abs(const BigInt *a, const BigInt *b)
{
    return internal_cmp_bigint_abs(a, b) == LESS;
}

// static bool
// internal_le_bigint_abs(const BigInt *a, const BigInt *b)
// {
//     return internal_cmp_bigint_abs(a, b) <= EQUAL;
// }

static Comparison
internal_cmp_digit_abs(const BigInt *a, Digit b)
{
    if (internal_is_zero(a)) {
        return (b == 0) ? EQUAL : LESS;
    }

    if (a->len > 1) {
       return GREATER;
    }

    if (a->digits[0] < b) {
        return LESS;
    } else if (a->digits[0] == b) {
        return EQUAL;
    }
    return GREATER;
}

// static bool
// internal_eq_digit_abs(const BigInt *a, Digit b)
// {
//     return internal_cmp_digit_abs(a, b) == EQUAL;
// }

static bool
internal_lt_digit_abs(const BigInt *a, Digit b)
{
    return internal_cmp_digit_abs(a, b) == LESS;
}

// static bool
// internal_le_digit_abs(const BigInt *a, Digit b)
// {
//     return internal_cmp_digit_abs(a, b) <= EQUAL;
// }

/** @brief `|a| + |b|` */
static BigInt *
internal_add_bigint_unsigned(lua_State *L, const BigInt *a, const BigInt *b)
{
    if (a->len < b->len) {
        internal_swap_ptr(&a, &b);
    }
    size_t max_used = a->len;
    size_t min_used = b->len;
    // May overallocate by 1 digit, which is acceptable.
    BigInt *dst = internal_make(L, max_used + 1);

    size_t i = 0;
    Digit carry = 0;
    for (; i < min_used; i += 1) {
        Digit sum = a->digits[i] + b->digits[i] + carry;
        carry = cast(Digit)(sum > BIGINT_DIGIT_MAX);
        if (carry == 1) {
            sum -= BIGINT_DIGIT_BASE;
        }
        dst->digits[i] = sum;
    }

    for (; i < max_used; i += 1) {
        Digit sum = a->digits[i] + carry;
        carry = cast(Digit)(sum > BIGINT_DIGIT_MAX);
        if (carry == 1) {
            sum -= BIGINT_DIGIT_BASE;
        }
        dst->digits[i] = sum;
    }
    dst->digits[i] = carry;
    return internal_clamp(dst);
}


/** @brief `|a| + |b|` where `b >= 0` and `b` fits in a `Digit`. */
static BigInt *
internal_add_digit_unsigned(lua_State *L, const BigInt *a, Digit b)
{
    size_t used = a->len;

    // May overallcoate by 1 digit, which is acceptable.
    BigInt *dst   = internal_make(L, used + 1);
    Digit   carry = b;
    for (size_t i = 0; i < used; i += 1) {
        Digit sum = a->digits[i] + carry;
        carry = cast(Digit)(sum > BIGINT_DIGIT_MAX);
        if (carry == 1) {
            sum -= BIGINT_DIGIT_BASE;
        }
        dst->digits[i] = sum;
    }
    dst->digits[used] = carry;
    return internal_clamp(dst);
}


/** @brief `|a| - |b|` where `|a| >= |b|`. */
static BigInt *
internal_sub_bigint_unsigned(lua_State *L, const BigInt *a, const BigInt *b)
{
    if (a->len < b->len) {
        internal_swap_ptr(&a, &b);
    }

    size_t max_used = a->len;
    size_t min_used = b->len;
    BigInt *dst = internal_make(L, max_used + 1);

    Word borrow = 0;
    size_t i = 0;
    for (; i < min_used; i += 1) {
        Word diff = cast(Word)a->digits[i] - cast(Word)b->digits[i] - borrow;
        borrow = cast(Word)(diff < 0);
        if (borrow == 1) {
            diff += BIGINT_DIGIT_BASE;
        }
        dst->digits[i] = cast(Digit)diff;
    }

    for (; i < max_used; i += 1) {
        Word diff = cast(Word)a->digits[i] - borrow;
        borrow = cast(Word)(diff < 0);
        if (borrow == 1) {
            diff += BIGINT_DIGIT_BASE;
        }
        dst->digits[i] = cast(Digit)diff;
    }
    dst->digits[i] = cast(Digit)borrow;
    return internal_clamp(dst);
}


/** @brief `a - b` where a >= b and b >= 0 */
static BigInt *
internal_sub_digit_unsigned(lua_State *L, const BigInt *a, Digit b)
{
    size_t  used = a->len;
    BigInt *dst  = internal_make(L, used + 1);

    Word borrow = cast(Word)b;
    for (size_t i = 0; i < used; i += 1) {
        Word diff = a->digits[i] - borrow;
        borrow = cast(Word)(diff < 0);
        if (borrow == 1) {
            diff += BIGINT_DIGIT_BASE;
        }
        dst->digits[i] = cast(Digit)diff;
    }
    dst->digits[used] = cast(Digit)borrow;
    return internal_clamp(dst);
}

static int
bigint_add_bigint(lua_State *L, const BigInt *a, const BigInt *b)
{
    // 1.)   a  +   b
    // 2.) (-a) + (-b) == -(a + b)
    if (a->sign == b->sign) {
        bool    both_neg = internal_is_neg(a);
        BigInt *dst      = internal_add_bigint_unsigned(L, a, b);
        if (both_neg) {
            internal_neg(dst);
        }
        return 1;
    }

    // 3.) (-a) +   b  == -(a - b)
    // 4.)   a  + (-b) ==   a - b
    bool a_is_neg  = internal_is_neg(a);
    bool a_is_less = internal_lt_bigint_abs(a, b);
    if (a_is_less) {
        internal_swap_ptr(&a, &b);
    }

    BigInt *dst = internal_sub_bigint_unsigned(L, a, b);
    // 3.1.) (-a) + b > 0 when |a| < |b|
    // 3.2.) (-a) + b < 0 when |a| > |b|
    //
    // 4.1)  a - b <  0 when |a| <  |b|
    // 4.2.) a - b >= 0 when |a| >= |b|
    if ((a_is_neg && !a_is_less) || a_is_less) {
        internal_neg(dst);
    }
    return 1;
}

static int
bigint_add_digit(lua_State *L, const BigInt *a, Digit b)
{
    // 1.) (-a) + b == -(a - b)
    if (internal_is_neg(a)) {
        // 1.1.) -(a - b) >= 0
        //  where |a| < |b|
        if (internal_lt_digit_abs(a, b)) {
            internal_make_integer(L, cast(lua_Integer)(b - a->digits[0]));
        }
        // 1.2.) -(a - b) < 0
        //  where |a| > |b|
        else {
            BigInt *dst = internal_sub_digit_unsigned(L, a, b);
            internal_neg(dst);
        }
    }
    // 2.) a + b
    else {
        internal_add_digit_unsigned(L, a, b);
    }
    return 1;
}

static int
bigint_sub_bigint(lua_State *L, const BigInt *a, const BigInt *b)
{
    bool a_is_neg = internal_is_neg(a);
    if (a->sign != b->sign) {
        // 1.) (-a) -   b  == -(a + b)
        // 2.)   a  - (-b) ==   a + b
        BigInt *dst = internal_add_bigint_unsigned(L, a, b);
        if (a_is_neg) {
            internal_neg(dst);
        }
        return 1;
    }

    Sign sign = a->sign;
    if (internal_lt_bigint_abs(a, b)) {
        sign = (sign == POSITIVE) ? NEGATIVE : POSITIVE;
        internal_swap_ptr(&a, &b);
    }

    BigInt *dst = internal_sub_bigint_unsigned(L, a, b);
    if (!internal_is_zero(dst)) {
        dst->sign = sign;
    }
    return 1;
}

static int
bigint_sub_digit(lua_State *L, const BigInt *a, Digit b)
{
    Sign sign = a->sign;
    // 1.) (-a) - b == -(a + b)
    if (internal_is_neg(a)) {
        BigInt *dst = internal_add_digit_unsigned(L, a, b);
        dst->sign = sign;
        return 1;
    }

    // 2.) a - b < 0 == -(b - a)
    //  where a < b
    //    and a >= 0
    //    and b >= 0
    if (internal_lt_digit_abs(a, b)) {
        BigInt *dst = internal_make(L, 1);
        dst->digits[0] = b - a->digits[0];
        internal_neg(dst);
        internal_clamp(dst);
        return 1;
    }

    // 3.) a - b >= 0
    //  where a >= b
    //    and a >= 0
    //    and b >= 0
    BigInt *dst = internal_sub_digit_unsigned(L, a, b);
    dst->sign = sign;
    return 1;
}

typedef int (*Arith_Fn1)(lua_State *L, const BigInt *a, const BigInt *b);
typedef int (*Arith_Fn2)(lua_State *L, const BigInt *a, Digit b);

static int
bigint_arith(lua_State *L, Arith_Fn1 fn1, Arith_Fn2 fn2)
{
    Arg a = arg_get(L, 1);
    Arg b = arg_get(L, 2);

    // 1.) a: BigInt *
    if (arg_is_bigint(a)) {
        // 1.1.) a: BigInt, b: BigInt
        if (arg_is_bigint(b)) {
            return fn1(L, a.bigint, b.bigint);
        }
        // 1.2.) a: BigInt, b: integer
        else {
            luaL_argcheck(L, integer_fits_digit(b.integer), 2, "bruh");
            return fn2(L, a.bigint, cast(Digit)b.integer);
        }
    }

    // 2.) a: integer
    // 2.1.) a: integer, b: BigInt
    if (arg_is_bigint(b)) {
        luaL_argcheck(L, integer_fits_digit(a.integer), 1, "bruh");
        return fn2(L, b.bigint, cast(Digit)a.integer);
    }

    // Impossible for both to be plain `number`.
    // 2.2.) a: integer, b: integer
    return 1;

}

static int
bigint_add(lua_State *L)
{
    return bigint_arith(L, bigint_add_bigint, bigint_add_digit);
}

static int
bigint_sub(lua_State *L)
{
    return bigint_arith(L, bigint_sub_bigint, bigint_sub_digit);
}

static int
bigint_unm(lua_State *L)
{
    const BigInt *src = internal_ensure(L, 1);
    BigInt *dst = internal_make_copy(L, src);
    internal_neg(dst);
    return 1;
}

static Digit
internal_place_value(Digit digit, Digit base)
{
    if (digit == 0) {
        return 0;
    }

    // Use intermediate type in case of overflow from multiplication.
    Word place = 1;
    while (place * cast(Word)base <= cast(Word)digit) {
        place *= cast(Word)base;
    }
    return cast(Digit)place;
}

static void
internal_write_digit(luaL_Buffer *sb, Digit digit, Digit base)
{
    Digit pv = internal_place_value(digit, base);
    for (;;) {
        // Get the left-most digit, e.g. '1' in "1234".
        Digit msd = digit / pv;
        if (base <= 10) {
            luaL_addchar(sb, cast(char)msd + '0');
        } else if (base <= 36) {
            luaL_addchar(sb, cast(char)msd + 'a' - 10);
        }

        // 'Trim off' the MSD's magnitude.
        digit -= msd * pv;
        pv /= base;
        // No more digits to process? Also avoids division by zero.
        if (pv == 0) {
            break;
        }
    }
}

static int
bigint_tostring(lua_State *L)
{
    const BigInt *bi = internal_ensure(L, 1);
    Digit base = cast(Digit)luaL_optinteger(L, /*narg=*/2, /*def=*/10);
    luaL_argcheck(L, 2 <= base && base <= 36, /*numarg=*/2, /*extramsg=*/"Invalid base");

    if (internal_is_zero(bi)) {
        lua_pushliteral(L, "0");
        return 1;
    }

    luaL_Buffer sb;
    luaL_buffinit(L, &sb);

    if (internal_is_neg(bi)) {
        luaL_addchar(&sb, '-');
    }

    switch (base) {
    case 2:  luaL_addstring(&sb, "0b"); break;
    case 8:  luaL_addstring(&sb, "0o"); break;
    case 16: luaL_addstring(&sb, "0x"); break;
    }

    // Write the MSD which will never have leading zeroes.
    size_t msd_index = bi->len - 1;
    internal_write_digit(&sb, bi->digits[msd_index], base);

    // Write from MSD - 1 to LSD.
    // Don't subtract 1 immediately due to unsigned overflow.
    for (size_t i = msd_index; i > 0; i -= 1) {
        Digit digit = bi->digits[i - 1];

        // Convert base-`BASE` to base-`base` with leading zeroes as needed.
        Word tmp = cast(Word)digit;
        while (tmp * cast(Word)base < BIGINT_DIGIT_BASE) {
            luaL_addchar(&sb, '0');
            tmp *= cast(Word)base;
        }
        internal_write_digit(&sb, digit, base);
    }
    luaL_pushresult(&sb);
    return 1;
}

static const luaL_Reg
bigint_fns[] = {
    {"new",      bigint_ctor},
    {"tostring", bigint_tostring},
    {"__call",   bigint_call},
    {NULL,       NULL},
};

static const luaL_Reg
bigint_mt[] = {
    {"__add",      bigint_add},
    {"__sub",      bigint_sub},
    {"__unm",      bigint_unm},
    {"__eq",       bigint_eq},
    {"__lt",       bigint_lt},
    {"__le",       bigint_le},
    {"__tostring", bigint_tostring},
    {NULL, NULL},
};

LUALIB_API int
luaopen_bigint(lua_State *L)
{
    // bigint library
    luaL_register(L, "bigint", bigint_fns);
    lua_pushvalue(L, -1);
    lua_setmetatable(L, -2);

    // bigint main metatable
    // -> bigint, mt
    luaL_newmetatable(L, BIGINT_MTNAME);
    luaL_register(L, NULL, bigint_mt);

    // -> bigint, mt, mt
    lua_pushvalue(L, -1);

    // -> bigint, mt ; mt.__index = mt
    lua_setfield(L, -2, "__index");
    return 1;
}

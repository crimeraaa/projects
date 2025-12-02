#include <stdio.h>  // printf
#include <stdlib.h> // malloc, realloc, free, abort

#include "bigint.h"

#include "internal.c"

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

typedef bool (*Compare_Fn)(const BigInt *a, const BigInt *b);

static int
bigint_compare(lua_State *L, Compare_Fn cmp_bigint)
{
    // Lua defines the comparison metamethods as only occuring when both
    // operands share the same metatable. Thus we can never reach here by
    // comparing with a `number` for example.
    //
    // See: https://www.lua.org/pil/13.2.html
    const BigInt *a = internal_get_bigint(L, 1);
    const BigInt *b = internal_get_bigint(L, 2);
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
bigint_sub_bigint(lua_State *L, const BigInt *a, const BigInt *b)
{
    BigInt *dst;
    Sign sign;
    bool a_is_neg;

    a_is_neg = internal_is_neg(a);
    if (a->sign != b->sign) {
        // 1.) (-a) -   b  == -(a + b)
        // 2.)   a  - (-b) ==   a + b
        dst = internal_add_bigint_unsigned(L, a, b);
        if (a_is_neg) {
            internal_neg(dst);
        }
        return 1;
    }

    sign = a->sign;
    if (internal_lt_bigint_abs(a, b)) {
        sign = (sign == POSITIVE) ? NEGATIVE : POSITIVE;
        internal_swap_ptr(&a, &b);
    }

    dst = internal_sub_bigint_unsigned(L, a, b);
    if (!internal_is_zero(dst)) {
        dst->sign = sign;
    }
    return 1;
}

/** @brief `|a| + b` where 0 <= b and b < DIGIT_BASE. */
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
        return 1;
    }
    // 2.) a + b
    internal_add_digit_unsigned(L, a, b);
    return 1;
}


/** @brief `|a| - b` where 0 <= b and b < DIGIT_BASE. */
static int
bigint_sub_digit(lua_State *L, const BigInt *a, Digit b)
{
    BigInt *dst;
    Sign sign = a->sign;

    // 1.) (-a) - b == -(a + b)
    if (internal_is_neg(a)) {
        dst = internal_add_digit_unsigned(L, a, b);
        dst->sign = sign;
        return 1;
    }

    // 2.) a - b < 0 == -(b - a)
    //  where a < b
    //    and a >= 0
    //    and b >= 0
    if (internal_lt_digit_abs(a, b)) {
        dst = internal_make(L, 1);
        dst->digits[0] = b - a->digits[0];
        internal_neg(dst);
        internal_clamp(dst);
        return 1;
    }

    // 3.) a - b >= 0
    //  where a >= b
    //    and a >= 0
    //    and b >= 0
    dst = internal_sub_digit_unsigned(L, a, b);
    dst->sign = sign;
    return 1;
}


static int
bigint_add_integer(lua_State *L, const BigInt *a, lua_Integer b);

static int
bigint_sub_integer(lua_State *L, const BigInt *a, lua_Integer b);

typedef int (*Arith_Fn1)(lua_State *L, const BigInt *a, const BigInt *b);
typedef int (*Arith_Fn2)(lua_State *L, const BigInt *a, lua_Integer b);

static int
bigint_arith(lua_State *L, Arith_Fn1 bigint_fn, Arith_Fn2 integer_fn)
{
    Arg a = internal_arg_get(L, 1);
    Arg b = internal_arg_get(L, 2);

    switch (a.type) {
    // 1.) a: BigInt *
    case ARG_BIGINT:
        switch (b.type) {
        // 1.1.) a: BigInt, b: BigInt
        case ARG_BIGINT: return bigint_fn(L, a.bigint, b.bigint);

        // 1.2.) a: BigInt, b: integer
        // It is tempting to swap `a` and `b` then delegate to the last line,
        // but we cannot assume `a op b == b op a` for all operations.
        case ARG_INTEGER: return integer_fn(L, a.bigint, b.integer);
        default:
            break;
        }
        return luaL_typerror(L, 2, "BigInt|integer");
    case ARG_INTEGER:
        // 2.) a: integer
        // 2.1.) a: integer, b: BigInt
        // 2.2.) a: integer, b: integer (Impossible.)
        if (internal_arg_is_bigint(b)) {
            return integer_fn(L, b.bigint, a.integer);
        }
        return luaL_typerror(L, 2, "BigInt");
    default:
        break;
    }
    return luaL_typerror(L, 1, "BigInt|integer");
}

static int
bigint_add(lua_State *L)
{
    return bigint_arith(L, bigint_add_bigint, bigint_add_integer);
}

static int
bigint_add_integer(lua_State *L, const BigInt *a, lua_Integer b)
{
    // 1.) a + b where 0 <= b and b < DIGIT_BASE
    if (integer_fits_digit(b)) {
        return bigint_add_digit(L, a, cast(Digit)b);
    }
    // 2.) (-a) + b == -(a - b)
    else if (internal_is_neg(a)) {
        BigInt *dst;
        // 2.1.) (-a) + (-b) == -(a + |b|)
        if (b < 0) {
            dst = internal_add_integer_unsigned(L, a, b);
        }
        // 2.2.) (-a) + b == -(|a| - b)
        else {
            dst = internal_sub_integer_unsigned(L, a, b);
        }
        internal_neg(dst);
        return 1;
    }

    // 3.) a + b where 0 < a and DIGIT_BASE <= b
    internal_add_integer_unsigned(L, a, b);
    return 1;
}

static int
bigint_sub(lua_State *L)
{
    return bigint_arith(L, bigint_sub_bigint, bigint_sub_integer);
}


static int
bigint_sub_integer(lua_State *L, const BigInt *a, lua_Integer b)
{
    // 1.) a - b where 0 <= b and b < DIGIT_BASE
    if (integer_fits_digit(b)) {
        return bigint_sub_digit(L, a, cast(Digit)b);
    }
    // 2.) a - (-b) == a + |b|
    else if (b < 0) {
        // 1.1.) (-a) + |b| == -(|a| - |b|)
        //  where a < 0
        //    and b < 0
        // 1.1.1) -(|a| - |b|) > 0 where |a| < |b|
        // 1.1.2) -(|a| - |b|) < 0 where |a| > |b|
        if (internal_is_neg(a)) {
            BigInt *dst = internal_sub_integer_unsigned(L, a, b);
            internal_neg(dst);
        }
        // 1.3.) a + |b|
        //  where a >= 0
        //    and b <  0
        else {
            internal_add_integer_unsigned(L, a, b);
        }
        return 1;
    }
    // 3.) (-a) - b == -(a + b)
    //  where a <  0
    //    and b >= 0
    else if (internal_is_neg(a)) {
        BigInt *dst = internal_add_integer_unsigned(L, a, b);
        internal_neg(dst);
        return 1;
    }

    // 3.) a - b where DIGIT_BASE <= b
    internal_sub_integer_unsigned(L, a, b);
    return 1;
}

static int
bigint_mul_bigint(lua_State *L, const BigInt *a, const BigInt *b)
{
    Sign sign;
    BigInt *dst;

    sign = (a->sign == b->sign) ? POSITIVE : NEGATIVE;
    dst  = internal_mul_bigint_unsigned(L, a, b);
    dst->sign = sign;
    return 1;
}


static int
bigint_mul_integer(lua_State *L, const BigInt *a, lua_Integer b)
{
    if (integer_fits_digit_unsigned(b)) {
        Digit b_abs;
        BigInt *dst;

        b_abs     = cast(Digit)internal_integer_abs(b);
        dst       = internal_mul_digit_unsigned(L, a, b_abs);
        dst->sign = (internal_is_pos(a) == (b >= 0)) ? POSITIVE : NEGATIVE;
        return 1;
    }
    return STUB(L, "unimplemented");
}

static int
bigint_mul(lua_State *L)
{
    return bigint_arith(L, bigint_mul_bigint, bigint_mul_integer);
}

static int
bigint_unm(lua_State *L)
{
    const BigInt *src;
    BigInt *dst;

    src = internal_get_bigint(L, 1);
    dst = internal_make_copy(L, src);
    internal_neg(dst);
    return 1;
}

static int
bigint_tostring(lua_State *L)
{
    luaL_Buffer sb;
    const BigInt *a;
    Digit base;

    luaL_buffinit(L, &sb);
    // Avoid `bigint.tostring(bigint)` or `bigint:tostring()`.
    a    = internal_ensure_bigint(L, 1);
    base = cast(Digit)luaL_optinteger(L, /*narg=*/2, /*def=*/10);

    // Conversion to base-2, base-8 and base-16 strings doesn't work yet.
    // e.g. 0xfeedbeef
    //    0d4_276_993_775 |   0b01111111_01110110_11011111_011101111
    //  = 0d4_000_000_000 | = 0b01110111_00110101_10010100_000000000
    //  + 0d0_276_993_775 | + 0b00001000_01000001_01001011_011101111
    luaL_argcheck(L, 2 <= base && base <= 36,
        /*numarg=*/2,
        /*extramsg=*/"Invalid base");

    if (internal_is_zero(a)) {
        lua_pushliteral(L, "0");
        return 1;
    }


    if (internal_is_neg(a)) {
        luaL_addchar(&sb, '-');
    }

    switch (base) {
    case 2:
    case 8:
    case 16: internal_write_binary_string(&sb, a, base); break;
    case 10: internal_write_decimal_string(&sb, a);      break;
    default: return luaL_error(L, "Unsupported base (%d)", cast(int)base);
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
    {"__mul",      bigint_mul},
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
    luaL_register(L, "bigint", bigint_fns);     // bigint
    lua_pushvalue(L, -1);    // bigint, bigint
    lua_setmetatable(L, -2); // bigint ; setmetatable(bigint, bigint)

    // bigint main metatable
    luaL_newmetatable(L, BIGINT_MTNAME); // bigint, mt
    luaL_register(L, NULL, bigint_mt);
    lua_pushvalue(L, -1);           // bigint, mt, mt
    lua_setfield(L, -2, "__index"); // bigint, mt ; mt.__index = mt
    lua_pop(L, 1);                  // bigint
    return 1;
}

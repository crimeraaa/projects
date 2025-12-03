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
bigint_add_integer(lua_State *L, const BigInt *a, lua_Integer b);

static int
bigint_sub_integer(lua_State *L, const BigInt *a, lua_Integer b);

typedef int (*Arith_Big)(lua_State *L, const BigInt *a, const BigInt *b);
typedef int (*Arith_Int)(lua_State *L, const BigInt *a, lua_Integer b);

static int
bigint_arith(lua_State *L, Arith_Big bigint_fn, Arith_Int integer_fn)
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
        return luaL_typerror(L, 2, BIGINT_TYPENAME "|integer");
    case ARG_INTEGER:
        // 2.) a: integer
        // 2.1.) a: integer, b: BigInt
        // 2.2.) a: integer, b: integer (Impossible.)
        if (internal_arg_is_bigint(b)) {
            return integer_fn(L, b.bigint, a.integer);
        }
        return luaL_typerror(L, 2, BIGINT_TYPENAME);
    default:
        break;
    }
    return luaL_typerror(L, 1, BIGINT_TYPENAME "|integer");
}

static int
bigint_add_bigint(lua_State *L, const BigInt *a, const BigInt *b)
{
    BigInt *dst;
    int max_used;
    bool a_is_neg, a_is_less;

    // 1.)   a  +   b
    // 2.) (-a) + (-b) == -(a + b)
    a_is_neg = internal_is_neg(a);
    if (a->sign == b->sign) {
        max_used = (a->len >= b->len) ? a->len : b->len;
        dst      = internal_make(L, max_used + 1);
        internal_add_bigint_unsigned(dst, a, b);
        // Both negative?
        if (a_is_neg) {
            internal_neg(dst);
        }
        return 1;
    }

    // 3.) (-a) +   b  == -(a - b)
    // 4.)   a  + (-b) ==   a - b
    a_is_less = internal_lt_bigint_abs(a, b);
    if (a_is_less) {
        internal_swap_ptr(&a, &b);
    }

    max_used = a->len;
    dst      = internal_make(L, max_used + 1);
    internal_sub_bigint_unsigned(dst, a, b);

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
bigint_add(lua_State *L)
{
    return bigint_arith(L, bigint_add_bigint, bigint_add_integer);
}


/** @brief `|a| + b` where 0 <= b and b < DIGIT_BASE. */
static int
bigint_add_digit(lua_State *L, const BigInt *a, DIGIT b)
{
    BigInt *dst;
    int max_used;

    // May overallocate by 1 digit, which is acceptable.
    max_used = a->len;
    dst      = internal_make(L, max_used + 1);

    // 1.) (-a) + b == -(a - b)
    if (internal_is_neg(a)) {
        // 1.1.) -(a - b) >= 0
        //  where |a| < |b|
        if (internal_lt_digit_abs(a, b)) {
            dst->digits[0] = b - a->digits[0];
        }
        // 1.2.) -(a - b) < 0
        //  where |a| > |b|
        else {
            internal_sub_digit(dst, a, b);
            internal_neg(dst);
        }
        return 1;
    }
    // 2.) a + b
    internal_add_digit(dst, a, b);
    return 1;
}

static int
bigint_sub_digit(lua_State *L, const BigInt *a, DIGIT b);

static int
bigint_add_integer(lua_State *L, const BigInt *a, lua_Integer b)
{
    // 1.) a + b where 0 <= b and b < DIGIT_BASE
    if (integer_fits_digit_abs(b)) {
        DIGIT b_abs = internal_integer_abs(b);
        // 1.1.) a +   b
        // 1.2.) a + (-b) == a - |b|
        return (b >= 0 ? bigint_add_digit : bigint_sub_digit)(L, a, b_abs);
    }
    // 2.) (-a) + b == -(a - b)
    // 3.) a + b where 0 < a and DIGIT_BASE <= b
    BigInt *b_tmp = internal_make_integer(L, b);
    return bigint_add_bigint(L, a, b_tmp);
}

static int
bigint_sub_bigint(lua_State *L, const BigInt *a, const BigInt *b)
{
    BigInt *dst;
    int max_used;
    Sign sign;
    bool a_is_neg;

    a_is_neg = internal_is_neg(a);
    if (a->sign != b->sign) {
        if (a->len < b->len) {
            internal_swap_ptr(&a, &b);
        }
        max_used = a->len;
        dst      = internal_make(L, max_used);

        // 1.) (-a) -   b  == -(a + b)
        // 2.)   a  - (-b) ==   a + b
        internal_add_bigint_unsigned(dst, a, b);
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

    max_used = a->len;
    dst      = internal_make(L, max_used);
    internal_sub_bigint_unsigned(dst, a, b);
    if (!internal_is_zero(dst)) {
        dst->sign = sign;
    }
    return 1;
}

static int
bigint_sub(lua_State *L)
{
    return bigint_arith(L, bigint_sub_bigint, bigint_sub_integer);
}


/** @brief `|a| - b` where 0 <= b and b < DIGIT_BASE. */
static int
bigint_sub_digit(lua_State *L, const BigInt *a, DIGIT b)
{
    BigInt *dst;
    Sign sign;

    dst  = internal_make(L, a->len);
    sign = a->sign;

    // 1.) (-a) - b == -(a + b)
    if (internal_is_neg(a)) {
        internal_add_digit(dst, a, b);
        dst->sign = sign;
        return 1;
    }

    // 2.) a - b < 0 == -(b - a)
    //  where a < b
    //    and a >= 0
    //    and b >= 0
    if (internal_lt_digit_abs(a, b)) {
        dst->digits[0] = b - a->digits[0];
        internal_neg(dst);
        internal_clamp(dst);
        return 1;
    }

    // 3.) a - b >= 0
    //  where a >= b
    //    and a >= 0
    //    and b >= 0
    internal_sub_digit(dst, a, b);
    dst->sign = sign;
    return 1;
}

static int
bigint_sub_integer(lua_State *L, const BigInt *a, lua_Integer b)
{
    // 1.) a - b where 0 <= b and b < DIGIT_BASE
    if (integer_fits_digit_abs(b)) {
        DIGIT b_abs = internal_integer_abs(b);
        // 1.1.) a -   b
        // 1.2.) a - (-b) == a + |b|
        return (b >= 0 ? bigint_sub_digit : bigint_add_digit)(L, a, b_abs);
    }
    BigInt *b_tmp = internal_make_integer(L, b);
    return bigint_sub_bigint(L, a, b_tmp);
}

static int
bigint_mul_bigint(lua_State *L, const BigInt *a, const BigInt *b)
{
    BigInt *dst;
    int max_used, min_used;

    if (a->len < b->len) {
        internal_swap_ptr(&a, &b);
    }

    max_used  = a->len;
    min_used  = b->len;
    dst       = internal_make(L, min_used + max_used + 1);
    dst->sign = (a->sign == b->sign) ? POSITIVE : NEGATIVE;
    internal_mul_bigint_unsigned(dst, a, b);
    return 1;
}


static int
bigint_mul_integer(lua_State *L, const BigInt *a, lua_Integer b)
{
    if (integer_fits_digit_abs(b)) {
        BigInt *dst;
        DIGIT b_abs;
        int used;

        used  = a->len;
        dst   = internal_make(L, used);
        b_abs = cast(DIGIT)internal_integer_abs(b);

        internal_mul_digit(dst, a, b_abs);
        dst->sign = (internal_is_pos(a) == (b >= 0)) ? POSITIVE : NEGATIVE;
        return 1;
    }

    BigInt *b_tmp = internal_make_integer(L, b);
    return bigint_mul_bigint(L, a, b_tmp);
}

static int
bigint_mul(lua_State *L)
{
    return bigint_arith(L, bigint_mul_bigint, bigint_mul_integer);
}

static int
bigint_div_bigint(lua_State *L, const BigInt *a, const BigInt *b)
{
    luaL_argcheck(L, !internal_is_zero(b), 2, "Division by zero");
    if (internal_is_zero(a)) {
        internal_make_integer(L, 0);
        return 1;
    }
    return STUB(L, "unimplemented");
}

static int
bigint_div_integer(lua_State *L, const BigInt *a, lua_Integer b)
{
    if (integer_fits_digit_abs(b)) {
        BigInt *dst;
        DIGIT b_abs;

        luaL_argcheck(L, b != 0, 2, "Division by zero");
        b_abs     = cast(DIGIT)internal_integer_abs(b);
        dst       = internal_make(L, a->len);
        dst->sign = (internal_is_pos(a) && b >= 0) ? POSITIVE : NEGATIVE;
        internal_divmod_digit(dst, a, b_abs);
        return 1;
    }

    BigInt *b_tmp = internal_make_integer(L, b);
    return bigint_div_bigint(L, a, b_tmp);
}

static int
bigint_div(lua_State *L)
{
    return bigint_arith(L, bigint_div_bigint, bigint_div_integer);
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
    DIGIT base;

    luaL_buffinit(L, &sb);
    // Avoid `bigint.tostring(bigint)` or `bigint:tostring()`.
    a    = internal_ensure_bigint(L, 1);
    base = cast(DIGIT)luaL_optinteger(L, /*narg=*/2, /*def=*/10);

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
    case 8:  internal_write_binary_string(&sb, a, base);    break;
    default: internal_write_nonbinary_string(&sb, a, base); break;
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
    {"__div",      bigint_div},
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
    luaL_register(L, BIGINT_LIBNAME, bigint_fns);     // bigint
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

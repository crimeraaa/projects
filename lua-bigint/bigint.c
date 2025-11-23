#include <stdio.h>  // printf
#include <stdlib.h> // malloc, realloc, free, abort
#include <string.h> // memset

#include "bigint.h"

static bool
bigint_is_zero(const BigInt *bi)
{
    return bi->len == 0;
}

static bool
bigint_is_neg(const BigInt *bi)
{
    return bi->sign == BIGINT_NEGATIVE;
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
bigint_make(lua_State *L, int digit_count)
{
    BigInt *bi;
    size_t  array_size = sizeof(bi->digits[0]) * cast(size_t)digit_count;

    // -> (..., bi: BigInt *)
    bi = cast(BigInt *)lua_newuserdata(L, sizeof(*bi) + array_size);
    bi->sign = BIGINT_POSITIVE;
    bi->len  = cast(size_t)digit_count;
    memset(bi->digits, 0, array_size);

    luaL_getmetatable(L, BIGINT_MTNAME); // -> (..., bi, mt: {})
    lua_setmetatable(L, -2);             // -> (..., bi) ; setmetatable(bi, mt)
    return bi;
}

static BigInt *
bigint_make_from_integer(lua_State *L, lua_Integer value)
{
    Sign sign = (value >= 0) ? BIGINT_POSITIVE : BIGINT_NEGATIVE;
    if (sign == BIGINT_NEGATIVE) {
        value = -value;
    }

    int digit_count = count_digits(cast(uintmax_t)value, BIGINT_DIGIT_BASE);
    BigInt *bi = bigint_make(L, digit_count);
    bi->sign = sign;

    // Write `value` from LSD to MSD.
    for (size_t i = 0; i < bi->len; i += 1) {
        bi->digits[i] = cast(Digit)(value % BIGINT_DIGIT_BASE);
        value /= BIGINT_DIGIT_BASE;
    }

    return bi;
}

static int
bigint_ctor_at(lua_State *L, int bigint_index)
{
    lua_Number n = lua_tonumber(L, bigint_index);
    if (n != 0 || lua_isnumber(L, bigint_index)) {
        lua_Integer i = cast(lua_Number)n;
        // Ensure `n` can be represented as an integer without truncation.
        if (cast(lua_Number)i == n) {
            bigint_make_from_integer(L, i);
            return 1;
        }
    }
    return luaL_typerror(L, bigint_index, "integer");
}

static int
bigint_ctor(lua_State *L)
{
    return bigint_ctor_at(L, 1);
}


/** @brief `bigint(...) => bigint.__call(bigint, ...)` */
static int
bigint_call(lua_State *L)
{
    return bigint_ctor_at(L, 2);
}

static BigInt *
bigint_ensure(lua_State *L, int arg)
{
    return luaL_checkudata(L, arg, BIGINT_MTNAME);
}

static Digit
place_value(Digit digit, Digit base)
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
write_digit(luaL_Buffer *sb, Digit digit, Digit base)
{
    Digit pv = place_value(digit, base);
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
    const BigInt *bi = bigint_ensure(L, 1);
    Digit base = cast(Digit)luaL_optinteger(L, /*narg=*/2, /*def=*/10);
    luaL_argcheck(L, 2 <= base && base <= 36, /*numarg=*/2, /*extramsg=*/"Invalid base");

    if (bigint_is_zero(bi)) {
        lua_pushliteral(L, "0");
        return 1;
    }

    luaL_Buffer sb;
    luaL_buffinit(L, &sb);

    if (bigint_is_neg(bi)) {
        luaL_addchar(&sb, '-');
    }

    switch (base) {
    case 2:  luaL_addstring(&sb, "0b"); break;
    case 8:  luaL_addstring(&sb, "0o"); break;
    case 16: luaL_addstring(&sb, "0x"); break;
    }

    // Write the MSD which will never have leading zeroes.
    size_t msd_index = bi->len - 1;
    write_digit(&sb, bi->digits[msd_index], base);

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
        write_digit(&sb, digit, base);
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

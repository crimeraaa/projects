#include <string.h> // memset

#include "bigint.h"

LUAI_FUNC BigInt *
internal_make_lstring(lua_State *L, const char *s, size_t s_len);

LUAI_FUNC int
internal_count_digits(uintmax_t value, Digit base)
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


LUAI_FUNC lua_Integer
internal_integer_abs(lua_Integer value)
{
    return (value >= 0) ? value : -value;
}


/** @brief `|value|` which also works for `value == min(type)`. */
LUAI_FUNC uintmax_t
internal_integer_abs_unsigned(lua_Integer value)
{
    return (value >= 0) ? cast(uintmax_t)value : -cast(uintmax_t)value;
}

LUAI_FUNC bool
internal_is_zero(const BigInt *bi)
{
    return bi->len == 0;
}

LUAI_FUNC bool
internal_is_neg(const BigInt *bi)
{
    return bi->sign == NEGATIVE;
}

LUAI_FUNC void
internal_swap_ptr(const BigInt **a, const BigInt **b)
{
    const BigInt *tmp = *a;
    *a = *b;
    *b = tmp;
}

LUAI_FUNC void
internal_neg(BigInt *dst)
{
    Sign sign = NEGATIVE;
    if (internal_is_zero(dst) || internal_is_neg(dst)) {
        sign = POSITIVE;
    }
    dst->sign = sign;
}

// Pushes a new BigInt and returns a pointer to said instance.
LUAI_FUNC BigInt *
internal_make(lua_State *L, size_t digit_count)
{
    BigInt *bi;
    size_t  array_size = sizeof(bi->digits[0]) * digit_count;

    // -> (..., bi: BigInt *)
    bi = cast(BigInt *)lua_newuserdata(L, sizeof(*bi) + array_size);
    bi->sign = POSITIVE;
    bi->len  = digit_count;
    memset(bi->digits, 0, array_size);

    luaL_getmetatable(L, BIGINT_MTNAME); // -> (..., bi, mt: {})
    lua_setmetatable(L, -2);             // -> (..., bi) ; setmetatable(bi, mt)
    return bi;
}

LUAI_FUNC BigInt *
internal_make_copy(lua_State *L, const BigInt *src)
{
    BigInt *dst = internal_make(L, src->len);
    dst->sign = src->sign;
    memcpy(dst->digits, src->digits, sizeof(src->digits[0]) * src->len);
    return dst;
}

LUAI_FUNC BigInt *
internal_make_integer(lua_State *L, lua_Integer value)
{
    BigInt *bi;
    uintmax_t value_abs;
    int digit_count;

    value_abs   = internal_integer_abs_unsigned(value);
    digit_count = internal_count_digits(value_abs, BIGINT_DIGIT_BASE);

    bi = internal_make(L, cast(size_t)digit_count);
    bi->sign = (value >= 0) ? POSITIVE : NEGATIVE;

    // Write `value` from LSD to MSD.
    for (size_t i = 0; i < bi->len; i += 1) {
        bi->digits[i] = cast(Digit)(value_abs % BIGINT_DIGIT_BASE);
        value_abs /= BIGINT_DIGIT_BASE;
    }

    return bi;
}

LUAI_FUNC BigInt *
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

LUAI_FUNC Arg
internal_arg_get(lua_State *L, int arg_i)
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
        size_t s_len = 0;
        const char *s = lua_tolstring(L, arg_i, &s_len);
        if (s != NULL) {
            arg.type         = ARG_STRING;
            arg.lstring.data = s;
            arg.lstring.len  = s_len;
            return arg;
        }
    }
    luaL_typerror(L, arg_i, "BigInt");
    return arg;
}

LUAI_FUNC bool
internal_arg_is_integer(Arg arg)
{
    return arg.type == ARG_INTEGER;
}

LUAI_FUNC bool
internal_arg_is_bigint(Arg arg)
{
    return arg.type == ARG_BIGINT;
}

LUAI_FUNC int
internal_ctor(lua_State *L, int first_arg)
{
    Arg arg = internal_arg_get(L, first_arg);
    switch (arg.type) {
    case ARG_BIGINT:
        internal_make_copy(L, arg.bigint);
        break;
    case ARG_INTEGER:
        internal_make_integer(L, arg.integer);
        break;
    case ARG_STRING:
        internal_make_lstring(L, arg.lstring.data, arg.lstring.len);
        break;
    default:
        return luaL_typerror(L, first_arg, "BigInt|integer|string");
    }
    return 1;
}

LUAI_FUNC Comparison
internal_cmp_bigint(const BigInt *a, const BigInt *b)
{
    bool a_is_neg = internal_is_neg(a);

    // 1.) Differing signs?
    if (a->sign != b->sign) {
        // 1.1.) (-a) <   b
        // 1.2.)   a  > (-b)
        return (a_is_neg) ? LESS : GREATER;
    }

    // 2.) Same signs, but differing lengths?
    if (a->len != b->len) {
        // 2.1.) -a < -b when #a > #b (more negative => less)
        // 2.2.) -a > -b when #a < #b (less negative => greater)
        if (a_is_neg) {
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
            return (a_is_neg) ? GREATER : LESS;
        } else if (a_digit > b_digit) {
            return (a_is_neg) ? LESS : GREATER;
        }
    }
    return EQUAL;
}

LUAI_FUNC bool
internal_eq_bigint(const BigInt *a, const BigInt *b)
{
    return internal_cmp_bigint(a, b) == EQUAL;
}

LUAI_FUNC bool
internal_lt_bigint(const BigInt *a, const BigInt *b)
{
    return internal_cmp_bigint(a, b) == LESS;
}

LUAI_FUNC bool
internal_le_bigint(const BigInt *a, const BigInt *b)
{
    return internal_cmp_bigint(a, b) <= EQUAL;
}

LUAI_FUNC bool
integer_fits_digit(lua_Integer b)
{
    return 0 <= b && b < BIGINT_DIGIT_BASE;
}

LUAI_FUNC bool
integer_fits_digit_unsigned(lua_Integer b)
{
    return -BIGINT_DIGIT_BASE < b && b < BIGINT_DIGIT_BASE;
}

LUAI_FUNC Comparison
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

LUAI_FUNC bool
internal_lt_bigint_abs(const BigInt *a, const BigInt *b)
{
    return internal_cmp_bigint_abs(a, b) == LESS;
}

LUAI_FUNC Comparison
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

LUAI_FUNC bool
internal_lt_digit_abs(const BigInt *a, Digit b)
{
    return internal_cmp_digit_abs(a, b) == LESS;
}

LUAI_FUNC Comparison
internal_cmp_integer_abs(const BigInt *a, lua_Integer b)
{
    if (integer_fits_digit_unsigned(b)) {
        return internal_cmp_digit_abs(a, cast(Digit)internal_integer_abs(b));
    }

    // `b >= DIGIT_BASE` and `#b > 1`
    uintmax_t b_abs = internal_integer_abs_unsigned(b);
    Digit b_digits[sizeof(b_abs) / sizeof(a->digits[0])];
    size_t b_len = count_of(b_digits);
    if (a->len != b_len) {
        return (a->len < b_len) ? LESS : GREATER;
    }

    // Separate the digits of `b`, from LSD to MSD.
    {
        uintmax_t tmp = b_abs;
        for (size_t i = 0; i < b_len; i += 1) {
            b_digits[i] = cast(Digit)(tmp % BIGINT_DIGIT_BASE);
            tmp /= BIGINT_DIGIT_BASE;
        }
    }

    // Compare MSD to LSD.
    for (size_t i = b_len; i > 0; i -= 1) {
        if (a->digits[i] < b_digits[i]) {
            return LESS;
        } else if (a->digits[i] > b_digits[i]) {
            return GREATER;
        }
    }
    return EQUAL;
}

LUAI_FUNC bool
internal_lt_integer_abs(const BigInt *a, lua_Integer b)
{
    return internal_cmp_integer_abs(a, b) == LESS;
}

/** @brief `|a| + |b|` */
LUAI_FUNC BigInt *
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
        carry = cast(Digit)(sum >= BIGINT_DIGIT_BASE);
        if (carry == 1) {
            sum -= BIGINT_DIGIT_BASE;
        }
        dst->digits[i] = sum;
    }

    for (; i < max_used; i += 1) {
        Digit sum = a->digits[i] + carry;
        carry = cast(Digit)(sum >= BIGINT_DIGIT_BASE);
        if (carry == 1) {
            sum -= BIGINT_DIGIT_BASE;
        }
        dst->digits[i] = sum;
    }
    dst->digits[i] = carry;
    return internal_clamp(dst);
}


/** @brief `|a| - |b|` where `|a| >= |b|`. */
LUAI_FUNC BigInt *
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


/** @brief `|a| + |b|` where `b >= 0` and `b` fits in a `Digit`. */
LUAI_FUNC BigInt *
internal_add_digit_unsigned(lua_State *L, const BigInt *a, Digit b)
{
    size_t used = a->len;

    // May overallocate by 1 digit, which is acceptable.
    BigInt *dst   = internal_make(L, used + 1);
    Digit   carry = b;
    for (size_t i = 0; i < used; i += 1) {
        Digit sum = a->digits[i] + carry;
        carry = cast(Digit)(sum >= BIGINT_DIGIT_BASE);
        if (carry == 1) {
            sum -= BIGINT_DIGIT_BASE;
        }
        dst->digits[i] = sum;
    }
    dst->digits[used] = carry;
    return internal_clamp(dst);
}


/** @brief `a - b` where a >= b and b >= 0 */
LUAI_FUNC BigInt *
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


/** @brief `|a| * |b|` */
LUAI_FUNC BigInt *
internal_mul_digit_unsigned(lua_State *L, const BigInt *a, Digit b)
{
    size_t used = a->len;
    BigInt *dst = internal_make(L, used);

    size_t i = 0;
    Word carry = 0;
    for (; i < used; i += 1) {
        Word prod = (cast(Word)a->digits[i] * cast(Word)b) + carry;
        // New carry is the 'overflow' from the product, e.g. 1000 in 1234
        // for base-1000.
        carry = prod / BIGINT_DIGIT_BASE;

        // Only the portion of the product that fits in `Digit` will be written.
        dst->digits[i] = cast(Digit)(prod % BIGINT_DIGIT_BASE);
    }
    dst->digits[i] = cast(Digit)carry;
    return dst;
}


/** @brief `|a| + |b|` where `DIGIT_BASE <= b` */
LUAI_FUNC BigInt *
internal_add_integer_unsigned(lua_State *L, const BigInt *a, lua_Integer b)
{
    uintmax_t carry = internal_integer_abs_unsigned(b);
    size_t    b_len = 2;
    assert(carry >= BIGINT_DIGIT_BASE);

    size_t    used  = (a->len < b_len) ? b_len : a->len;
    BigInt   *dst   = internal_make(L, used + 1);

    size_t i = 0;

    // Add up digit places common to both `a` and `b`.
    for (; i < a->len; i += 1) {
        Digit lsd = cast(Digit)(carry % BIGINT_DIGIT_BASE);
        Digit sum = a->digits[i] + lsd;
        if (sum >= BIGINT_DIGIT_BASE) {
            sum -= BIGINT_DIGIT_BASE;
        }
        dst->digits[i] = cast(Digit)sum;
        carry /= BIGINT_DIGIT_BASE;
    }

    // `b` was larger, copy over its higher digits.
    for (; i < b_len; i += 1) {
        dst->digits[i] = cast(Digit)(carry % BIGINT_DIGIT_BASE);
        carry /= BIGINT_DIGIT_BASE;
    }

    dst->digits[i] = cast(Digit)carry;
    return internal_clamp(dst);
}


/** @brief `|a| - |b|` where `|b| >= BIGINT_DIGIT_BASE` */
LUAI_FUNC BigInt *
internal_sub_integer_unsigned(lua_State *L, const BigInt *a, lua_Integer b)
{
    uintmax_t b_abs = internal_integer_abs_unsigned(b);
    assert(b_abs >= BIGINT_DIGIT_BASE);
    unused(a);
    unused(b_abs);
    luaL_error(L, "subtracting large `integer` not yet supported");
    return NULL;
}

LUAI_FUNC Digit
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


/** @brief Assumes `2 <= base and base <= 36`. */
LUAI_FUNC void
internal_write_digit(luaL_Buffer *sb, Digit digit, Digit base)
{
    if (digit == 0) {
        luaL_addchar(sb, '0');
        return;
    }

    Digit pv = internal_place_value(digit, base);
    while (pv > 0) {
        // Get the left-most digit, e.g. '1' in "1234".
        Digit msd = digit / pv;
        // Binary, Octal, Decimal: 0-9
        if (msd < 10) {
            luaL_addchar(sb, cast(char)msd + '0');
        }
        // Dozenal, Hexadecimal, Base-36: a-z (lowercase only for simpicity)
        else if (msd < 36) {
            luaL_addchar(sb, cast(char)msd - 10 + 'a');
        }

        // 'Trim off' the MSD's magnitude.
        digit -= msd * pv;
        pv /= base;
    }
}

static bool
char_is_number(char ch)
{
    return '0' <= ch && ch <= '9';
}

static bool
char_is_lower(char ch)
{
    return 'a' <= ch && ch <= 'z';
}

static bool
char_is_upper(char ch)
{
    return 'A' <= ch && ch <= 'Z';
}

static bool
char_is_space(char ch)
{
    switch (ch) {
    case '\t':
    case '\n':
    case '\v':
    case '\r':
    case ' ':
        return true;
    default:
        break;
    }
    return false;
}

#define INVALID_DIGIT   -1

static int
char_to_digit(char ch, Digit base)
{
    int d = INVALID_DIGIT;
    if (char_is_number(ch)) {
        d = ch - '0';
    } else if (char_is_upper(ch)) {
        d = ch - 'A' + 10;
    } else if (char_is_lower(ch)) {
        d = ch - 'a' + 10;
    }

    if (0 <= d && d < cast(int)base) {
        return d;
    }
    return INVALID_DIGIT;
}

LUAI_FUNC Sign
internal_get_sign(const char **s, size_t *s_len)
{
    Sign sign = POSITIVE;
    for (; (*s_len > 0); *s += 1, *s_len -= 1) {
        char ch = (*s)[0];
        // Negation will always flip the sign.
        if (ch == '-') {
            sign = (sign == POSITIVE) ? NEGATIVE : POSITIVE;
        }
        // Unary plus, spaces do nothing to the sign but must be skipped.
        else if (ch == '+' || char_is_space(ch)) {
            continue;
        }
        // Not a sign character and we cannot necessarily skip it.
        else {
            break;
        }
    }
    return sign;
}

LUAI_FUNC Digit
internal_get_base(const char **s, size_t *s_len)
{
    Digit base = 10;
    if ((*s_len) >= 2 && (*s)[0] == '0') {
        switch ((*s)[1]) {
        case 'b': case 'B': base = 2;  goto trim_string;
        case 'o': case 'O': base = 8;  goto trim_string;
        case 'd': case 'D': base = 10; goto trim_string;
        case 'x': case 'X': base = 16;
trim_string:
            *s     += 2;
            *s_len -= 2;
            break;
        // case 'z': case 'Z': base = 12; break;
        }
    }
    // Assume base-10 otherwise
    return base;
}


/** @brief How many base-`base` digits can fit in a single base-`BASE` digit? */
static int
internal_digit_length(Digit base)
{
    // Check prefixes for most common bases.
    switch (base) {
    case 2:     return BIGINT_DIGIT_BASE2_LENGTH;
    case 8:     return BIGINT_DIGIT_BASE8_LENGTH;
    case 10:    return BIGINT_DIGIT_BASE10_LENGTH;
    case 16:    return BIGINT_DIGIT_BASE16_LENGTH;
    }
    // Uncommon base, calculate as needed.
    return internal_count_digits(BIGINT_DIGIT_BASE - 1, base);
}


LUAI_FUNC BigInt *
internal_make_lstring(lua_State *L, const char *s, size_t s_len)
{
    Sign  sign = internal_get_sign(&s, &s_len);
    Digit base = internal_get_base(&s, &s_len);
    size_t n_digits = 0;

    // Count number of base-`base` digits in the string
    for (size_t i = 0; i < s_len; i += 1) {
        char ch = s[i];
        if (ch == ',' || ch == '_') {
            continue;
        }

        int digit = char_to_digit(ch, base);
        if (digit == INVALID_DIGIT) {
            luaL_error(L, "Invalid base-%d digit '%c'", cast(int)base, ch);
            return NULL;
        }
        n_digits += 1;
    }

    // Will most likely over-allocate, but this is acceptable.
    size_t used = (n_digits / cast(size_t)internal_digit_length(base)) + 1;
    BigInt *dst = internal_make(L, used + 1);
    dst->sign = sign;

    Digit *digits = dst->digits;
    for (size_t s_i = 0; s_i < s_len; s_i += 1) {
        char ch = s[s_i];
        if (ch == ',' || ch == '_') {
            continue;
        }

        int digit = char_to_digit(ch, base);

        // dst *= base
        Word mul_carry = 0;
        for (size_t mul_i = 0; mul_i < used; mul_i += 1) {
            Word prod = (cast(Word)digits[mul_i] * cast(Word)base) + mul_carry;
            mul_carry = prod / BIGINT_DIGIT_BASE;
            digits[mul_i] = cast(Digit)(prod % BIGINT_DIGIT_BASE);
        }
        digits[used] = cast(Digit)mul_carry;

        // dst += digit
        Digit add_carry = digit;
        for (size_t add_i = 0; add_i < used; add_i += 1) {
            Digit sum = digits[add_i] + add_carry;
            add_carry = cast(Digit)(sum >= BIGINT_DIGIT_BASE);
            if (add_carry == 1) {
                sum -= BIGINT_DIGIT_BASE;
            }
            digits[add_i] = sum;
        }
        digits[used] = add_carry;
    }
    return internal_clamp(dst);
}

#include <string.h> // memset

#include "bigint.h"

LUAI_FUNC BigInt *
internal_make_lstring(lua_State *L, const char *s, size_t s_len, Digit base);

LUAI_FUNC BigInt *
internal_get_bigint(lua_State *L, int arg_i)
{
    return cast(BigInt *)lua_touserdata(L, arg_i);
}

LUAI_FUNC BigInt *
internal_ensure_bigint(lua_State *L, int arg_i)
{
    return cast(BigInt *)luaL_checkudata(L, arg_i, BIGINT_MTNAME);
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


/** @brief `|value|` which also works for `value == min(type)`. */
LUAI_FUNC uintmax_t
internal_integer_abs(lua_Integer value)
{
    return (value >= 0) ? cast(uintmax_t)value : -cast(uintmax_t)value;
}

LUAI_FUNC bool
internal_is_zero(const BigInt *a)
{
    return a->len == 0;
}

LUAI_FUNC bool
internal_is_neg(const BigInt *a)
{
    return a->sign == NEGATIVE;
}

LUAI_FUNC bool
internal_is_pos(const BigInt *a)
{
    return !internal_is_neg(a);
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
    BigInt *dst;
    size_t  array_size = sizeof(dst->digits[0]) * digit_count;

    // -> (..., bi: BigInt *)
    dst = cast(BigInt *)lua_newuserdata(L, sizeof(*dst) + array_size);
    dst->sign = POSITIVE;
    dst->len  = digit_count;
    memset(dst->digits, 0, array_size);

    luaL_getmetatable(L, BIGINT_MTNAME); // -> (..., bi, mt: {})
    lua_setmetatable(L, -2);             // -> (..., bi) ; setmetatable(bi, mt)
    return dst;
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
    BigInt   *dst;
    uintmax_t value_abs;
    int       digit_count;

    value_abs   = internal_integer_abs(value);
    digit_count = count_digits(value_abs, BIGINT_DIGIT_BASE);

    dst = internal_make(L, cast(size_t)digit_count);
    dst->sign = (value >= 0) ? POSITIVE : NEGATIVE;

    // Write `value` from LSD to MSD.
    for (size_t i = 0; i < dst->len; i += 1) {
        dst->digits[i] = cast(Digit)(value_abs % BIGINT_DIGIT_BASE);
        value_abs /= BIGINT_DIGIT_BASE;
    }

    return dst;
}

LUAI_FUNC void
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
}

LUAI_FUNC Arg
internal_arg_get(lua_State *L, int arg_i)
{
    Arg arg;
    arg.type    = ARG_INVALID;
    arg.integer = 0;

    switch (lua_type(L, arg_i)) {
    case LUA_TNUMBER: {
        lua_Number  n = lua_tonumber(L, arg_i);
        lua_Integer i = cast(lua_Integer)n;
        // Number type accurately represents the integer (no truncation)?
        if (cast(lua_Number)i == n) {
            arg.type    = ARG_INTEGER;
            arg.integer = i;
        } else {
            luaL_typerror(L, arg_i, "integer");
        }
        break;
    }
    case LUA_TSTRING: {
        size_t      s_len = 0;
        const char *s     = lua_tolstring(L, arg_i, &s_len);

        arg.type         = ARG_STRING;
        arg.lstring.data = s;
        arg.lstring.len  = s_len;
        break;
    }
    default:
        arg.type   = ARG_BIGINT;
        arg.bigint = internal_ensure_bigint(L, arg_i);
        break;
    }
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
    Arg arg;
    Digit base;

    arg  = internal_arg_get(L, first_arg);
    base = cast(Digit)luaL_optinteger(L, first_arg + 1, /*def=*/0);
    switch (arg.type) {
    case ARG_BIGINT:
        luaL_argcheck(L, base == 0 || base == 10, first_arg + 1, "Don't.");
        internal_make_copy(L, arg.bigint);
        break;
    case ARG_INTEGER:
        // Don't follow `tonumber()` where you can pass a number and read it
        // in another base...
        luaL_argcheck(L, base == 0 || base == 10, first_arg + 1, "Don't.");
        internal_make_integer(L, arg.integer);
        break;
    case ARG_STRING:
        internal_make_lstring(L, arg.lstring.data, arg.lstring.len, base);
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
        if (a->digits[i - 1] < b->digits[i - 1]) {
            return (a_is_neg) ? GREATER : LESS;
        } else if (a->digits[i - 1] > b->digits[i - 1]) {
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
        size_t j = i - 1;
        if (a->digits[j] < b->digits[j]) {
            return LESS;
        } else if (a->digits[j] > b->digits[j]) {
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
    uintmax_t b_abs = internal_integer_abs(b);
    Digit     b_digits[sizeof(b_abs) / sizeof(a->digits[0])];
    size_t    b_len = count_of(b_digits);
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

static Digit
arith_add_carry(Digit *carry, Digit a, Digit b)
{
    Digit sum;

    sum    = a + b + *carry;
    *carry = cast(Digit)(sum >= BIGINT_DIGIT_BASE);
    if (*carry == 1) {
        sum -= BIGINT_DIGIT_BASE;
    }
    return sum;
}

/** @brief `|a| + |b|` where #a >= #b. */
LUAI_FUNC void
internal_add_bigint_unsigned(BigInt *dst, const BigInt *a, const BigInt *b)
{
    size_t max_used;
    size_t min_used;
    Digit carry = 0;

    max_used = a->len;
    min_used = b->len;
    for (size_t i = 0; i < min_used; i += 1) {
        dst->digits[i] = arith_add_carry(&carry, a->digits[i], b->digits[i]);
    }

    for (size_t i = min_used; i < max_used; i += 1) {
        dst->digits[i] = arith_add_carry(&carry, a->digits[i], /*b=*/0);
    }
    dst->digits[max_used] = carry;
    internal_clamp(dst);
}

static Digit
arith_sub_borrow(Word *borrow, Digit a, Digit b)
{
    Word diff;

    diff    = cast(Word)a - cast(Word)b - *borrow;
    *borrow = cast(Word)(diff < 0);
    if (*borrow == 1) {
        diff += BIGINT_DIGIT_BASE;
    }
    return cast(Digit)diff;
}


/** @brief `|a| - |b|` where `|a| >= |b|` and `#a >= #b`. */
LUAI_FUNC void
internal_sub_bigint_unsigned(BigInt *dst, const BigInt *a, const BigInt *b)
{
    size_t max_used, min_used, i = 0;
    Word borrow = 0;

    max_used = a->len;
    min_used = b->len;
    for (; i < min_used; i += 1) {
        dst->digits[i] = arith_sub_borrow(&borrow, a->digits[i], b->digits[i]);
    }

    for (; i < max_used; i += 1) {
        dst->digits[i] = arith_sub_borrow(&borrow, a->digits[i], /*b=*/0);
    }
    dst->digits[i] = cast(Digit)borrow;
    internal_clamp(dst);
}


/** @brief `|a| + |b|` where `b >= 0` and `b` fits in a `Digit`. */
LUAI_FUNC void
internal_add_digit_unsigned(BigInt *dst, const BigInt *a, Digit b)
{
    size_t used;
    Digit carry;

    used  = a->len;
    carry = b;
    for (size_t i = 0; i < used; i += 1) {
        dst->digits[i] = arith_add_carry(&carry, a->digits[i], /*b=*/0);
    }
    dst->digits[used] = carry;
    internal_clamp(dst);
}


/** @brief `a - b` where a >= b and b >= 0 */
LUAI_FUNC void
internal_sub_digit_unsigned(BigInt *dst, const BigInt *a, Digit b)
{
    size_t used;
    Word borrow;

    used   = a->len;
    borrow = cast(Word)b;

    for (size_t i = 0; i < used; i += 1) {
        dst->digits[i] = arith_sub_borrow(&borrow, a->digits[i], /*b=*/0);
    }
    dst->digits[used] = cast(Digit)borrow;
    internal_clamp(dst);
}


static Digit
arith_mul_carry(Word *carry, Digit a, Digit b)
{
    Word prod;

    // New carry is the 'overflow' from the product, e.g. 1000 in 1234
    // for base-1000.
    prod   = (cast(Word)a * cast(Word)b) + *carry;
    *carry = prod / BIGINT_DIGIT_BASE;

    // Only the portion of the product that fits in `Digit` will be written.
    return cast(Digit)(prod % BIGINT_DIGIT_BASE);
}


/** @brief `|a| * |b|` */
LUAI_FUNC void
internal_mul_digit_unsigned(BigInt *dst, const BigInt *a, Digit b)
{
    size_t used;
    Word carry = 0;

    used = a->len;
    for (size_t i = 0; i < used; i += 1) {
        dst->digits[i] = arith_mul_carry(&carry, a->digits[i], b);
    }
    dst->digits[used] = cast(Digit)carry;
}


/** @brief `|a| * |b|` where #a >= #b.
 * @param dst Should alias neither `a` nor `b`.
 */
LUAI_FUNC void
internal_mul_bigint_unsigned(BigInt *dst, const BigInt *a, const BigInt *b)
{
    size_t max_used, min_used;
    max_used = a->len;
    min_used = b->len;

    for (size_t b_i = 0; b_i < min_used; b_i += 1) {
        Word prod = 0, carry = 0;
        for (size_t a_i = 0; a_i < max_used; a_i += 1) {
            prod = arith_mul_carry(&carry, a->digits[a_i], b->digits[b_i]);
            dst->digits[a_i + b_i] += prod;
        }
        dst->digits[b_i + max_used] += carry;
    }
    internal_clamp(dst);
}

/** @brief `dst, mod = |a| / |b|, |a| % |b|`
 *  where |a| > 0
 *    and |b| > 0
 *
 * @param mod Optional out-parameter for the remainder. We know that
 *          `a % b` will always have at most `#b` digits.
 */
LUAI_FUNC void
internal_divmod_digit_unsigned(BigInt *dst, Digit *mod, const BigInt *a, Digit b)
{
    Word carry = 0;

    // Divide MSD to LSD.
    for (size_t i = a->len; i > 0; i -= 1) {
        size_t a_i;
        Digit quot = 0, rem = 0;

        a_i    = i - 1;
        carry += cast(Word)a->digits[a_i];
        // Need to carry AND we can actually carry? If we can't carry, just
        // skip this block because div-mod ends up as 0 anyway.
        if (carry < cast(Word)b && a_i > 0) {
            carry *= BIGINT_DIGIT_BASE;
            assert(carry > cast(Word)b);
            dst->digits[a_i] = 0;
            continue;
        }

        quot = cast(Digit)(carry / cast(Word)b);
        rem  = cast(Digit)(carry % cast(Word)b); // <=> carry - b*quot

        dst->digits[a_i] = quot;
        carry = cast(Word)rem; // Reset carry because we didn't propagate.
    }

    internal_clamp(dst);
    if (mod != NULL) {
        assert(0 <= carry && carry < BIGINT_DIGIT_BASE);
        *mod = cast(Digit)carry;
    }
}


static Digit
digit_place_value(Digit digit, Digit base)
{
    // Use intermediate type in case of overflow from multiplication.
    Word place = 1;
    while (place * cast(Word)base <= cast(Word)digit) {
        place *= cast(Word)base;
    }
    return cast(Digit)place;
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

static Sign
string_get_sign(const char **s, size_t *s_len)
{
    Sign sign = POSITIVE;
    for (; (*s_len > 0); *s += 1, *s_len -= 1) {
        char ch = (*s)[0];
        // Negation will always flip the sign.
        if (ch == '-') {
            sign = (sign == POSITIVE) ? NEGATIVE : POSITIVE;
        }
        // Unary plus and spaces do nothing to the sign but must be skipped.
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

static Digit
string_get_base(const char **s, size_t *s_len)
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
digit_length_base(Digit base)
{
    // Check prefixes for most common bases.
    switch (base) {
    case 2:     return BIGINT_DIGIT_BASE2_LENGTH;
    case 8:     return BIGINT_DIGIT_BASE8_LENGTH;
    case 10:    return BIGINT_DIGIT_BASE10_LENGTH;
    case 16:    return BIGINT_DIGIT_BASE16_LENGTH;
    }
    // Uncommon base, calculate as needed.
    return count_digits(BIGINT_DIGIT_BASE - 1, base);
}


LUAI_FUNC BigInt *
internal_make_lstring(lua_State *L, const char *s, size_t s_len, Digit base)
{
    BigInt *dst;
    Sign   sign;
    size_t used, n_digits = 0;

    sign = string_get_sign(&s, &s_len);

    // Skip base prefix.
    {
        Digit tmp = string_get_base(&s, &s_len);
        // Didn't know the base beforehand, we do now.
        if (base == 0) {
            base = tmp;
        }
    }

    // Count number of base-`base` digits in the string
    for (size_t i = 0; i < s_len; i += 1) {
        int digit;
        char ch;

        ch = s[i];
        if (ch == ',' || ch == '_') {
            continue;
        }

        digit = char_to_digit(ch, base);
        if (digit == INVALID_DIGIT) {
            luaL_error(L, "Invalid base-%d digit '%c'", cast(int)base, ch);
            return NULL;
        }
        n_digits += 1;
    }

    // Will most likely over-allocate, but this is acceptable.
    used      = (n_digits / cast(size_t)digit_length_base(base)) + 1;
    dst       = internal_make(L, used + 1);
    dst->sign = sign;

    for (size_t s_i = 0; s_i < s_len; s_i += 1) {
        Word  mul_carry = 0;
        Digit add_carry;
        int   digit;
        char  ch;

        ch = s[s_i];
        if (ch == ',' || ch == '_') {
            continue;
        }

        digit = char_to_digit(ch, base);

        // dst *= base
        for (size_t mul_i = 0; mul_i < used; mul_i += 1) {
            dst->digits[mul_i] = arith_mul_carry(&mul_carry, dst->digits[mul_i], base);
        }
        dst->digits[used] = cast(Digit)mul_carry;

        // dst += digit
        add_carry = digit;
        for (size_t add_i = 0; add_i < used; add_i += 1) {
            dst->digits[add_i] = arith_add_carry(&add_carry, dst->digits[add_i], /*b=*/0);
        }
        dst->digits[used] = add_carry;
    }
    internal_clamp(dst);
    return dst;
}

__attribute__((__unused__))
static void
dump_stack(lua_State *L)
{
    int n = lua_gettop(L);

    lua_getglobal(L, "tostring");
    println("*** STACK DUMP ***");
    for (int i = 1; i <= n; i += 1) {
        lua_pushvalue(L, -1);
        lua_pushvalue(L, i);
        lua_call(L, /*nargs=*/1, /*nresults=*/1);
        printfln("[%i]: %s = %s", i, luaL_typename(L, i), lua_tostring(L, -1));
        lua_pop(L, 1); // ..., tostring
    }
    println("******************\n");
    lua_pop(L, 1);
}


/** @brief Assumes `2 <= base and base <= 36`. */
static void
string_write_digit(luaL_Buffer *sb, Digit digit, Digit base)
{
    if (digit == 0) {
        luaL_addchar(sb, '0');
        return;
    }

    Digit pv = digit_place_value(digit, base);
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

LUAI_FUNC void
internal_write_binary_string(luaL_Buffer *sb, const BigInt *a, Digit base)
{
    lua_State *L;
    BigInt *tmp;
    luaL_Buffer rev_buf; // Holds the currently reverse binary string.

    L   = sb->L;
    tmp = internal_make_copy(L, a);
    luaL_buffinit(L, &rev_buf);

    switch (base) {
    case 2:  luaL_addstring(sb, "0b"); break;
    case 8:  luaL_addstring(sb, "0o"); break;
    case 16: luaL_addstring(sb, "0x"); break;
    default: luaL_error(L, "non-binary base %d", cast(int)base); return;
    }

    // Write from MSD to LSD.
    while (!internal_is_zero(tmp)) {
        Digit lsd = 0;
        internal_divmod_digit_unsigned(tmp, &lsd, tmp, base);
        string_write_digit(&rev_buf, lsd, base);
    }

    // Correct the arrangement of the binary string.
    luaL_pushresult(&rev_buf);
    lua_getfield(L, -1, "reverse");
    lua_pushvalue(L, -2);
    lua_call(L, /*nargs=*/1, /*nresults=*/1);

    // Ensure the original string builder knows about the now-correct string.
    luaL_addvalue(sb);
}

LUAI_FUNC void
internal_write_decimal_string(luaL_Buffer *sb, const BigInt *a)
{
    // Write the MSD which will never have leading zeroes.
    size_t msd_index;

    msd_index = a->len - 1;
    string_write_digit(sb, a->digits[msd_index], /*base=*/10);

    // Write from MSD - 1 to LSD.
    // Don't subtract 1 immediately due to unsigned overflow.
    for (size_t i = msd_index; i > 0; i -= 1) {
        // Convert base-`BASE` to base-`base`.
        Word tmp;
        Digit digit;

        digit = a->digits[i - 1];
        tmp   = cast(Word)digit;

        // Avoid infinite loops when multiplying by zero.
        if (tmp == 0) {
            tmp = 1;
        }

        // Add leading zeroes as needed.
        while (tmp * cast(Word)10 < BIGINT_DIGIT_BASE) {
            luaL_addchar(sb, '0');
            tmp *= cast(Word)10;
        }
        string_write_digit(sb, digit, /*base=*/10);
    }
}

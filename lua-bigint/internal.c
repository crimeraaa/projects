#include <string.h> // memset

#include "bigint.h"

typedef struct Writer Writer;
struct Writer {
    lua_State *L;
    char *data;
    size_t cap;
    size_t left;
    size_t right;
};

LUAI_FUNC BigInt *
internal_make_lstring(lua_State *L, const char *s, size_t s_len, DIGIT base);

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
count_digits(uintmax_t value, DIGIT base)
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
    const BigInt *tmp;

    tmp = *a;
    *a  = *b;
    *b  = tmp;
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
internal_make(lua_State *L, int digit_count)
{
    BigInt *dst;
    size_t  array_size = sizeof(dst->digits[0]) * cast(size_t)digit_count;

    // -> (..., bi: BigInt *)
    dst = cast(BigInt *)lua_newuserdata(L, sizeof(*dst) + array_size);
    dst->sign = POSITIVE;
    dst->len  = cast(size_t)digit_count;
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


/** @brief `quot, rem = dividend / BASE, dividend % BASE`.
 *  where `BASE` is a power of 2. */
static DIGIT
arith_divmod_base(DIGIT *rem, uintmax_t numerator)
{
    DIGIT quot;
    quot = cast(DIGIT)(numerator >> DIGIT_BITS);
    *rem = cast(DIGIT)(numerator & DIGIT_MASK);
    return quot;
}

LUAI_FUNC BigInt *
internal_make_integer(lua_State *L, lua_Integer value)
{
    BigInt   *dst;
    uintmax_t value_abs;
    int       digit_count;

    value_abs   = internal_integer_abs(value);
    digit_count = count_digits(value_abs, DIGIT_BASE);
    dst         = internal_make(L, digit_count);
    dst->sign   = (value >= 0) ? POSITIVE : NEGATIVE;

    // Write `value` from LSD to MSD.
    for (int i = 0; i < dst->len; i += 1) {
        DIGIT quot_next, rem_lsd;

        quot_next      = arith_divmod_base(&rem_lsd, value_abs);
        value_abs      = quot_next;
        dst->digits[i] = rem_lsd;
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
        lua_Number  n;
        lua_Integer i;

        n = lua_tonumber(L, arg_i);
        i = cast(lua_Integer)n;
        // Number type accurately represents the integer (no truncation)?
        if (cast(lua_Number)i == n) {
            arg.type    = ARG_INTEGER;
            arg.integer = i;
        } else {
            luaL_typerror(L, arg_i, "integer");
        }
        break;
    }
    case LUA_TSTRING:
        arg.type         = ARG_STRING;
        arg.lstring.data = lua_tolstring(L, arg_i, &arg.lstring.len);
        break;
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
    DIGIT base;

    arg  = internal_arg_get(L, first_arg);
    base = cast(DIGIT)luaL_optinteger(L, first_arg + 1, /*def=*/0);
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
        return luaL_typerror(L, first_arg, BIGINT_TYPENAME "|integer|string");
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
    for (int i = a->len - 1; i >= 0; i -= 1) {
        if (a->digits[i] < b->digits[i]) {
            return (a_is_neg) ? GREATER : LESS;
        } else if (a->digits[i] > b->digits[i]) {
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
    return 0 <= b && b < DIGIT_BASE;
}

LUAI_FUNC bool
integer_fits_digit_abs(lua_Integer b)
{
    return -DIGIT_BASE < b && b < DIGIT_BASE;
}

LUAI_FUNC Comparison
internal_cmp_bigint_abs(const BigInt *a, const BigInt *b)
{
    if (a->len != b->len) {
        return (a->len < b->len) ? LESS : GREATER;
    }

    for (int i = a->len - 1; i >= 0; i -= 1) {
        if (a->digits[i] < b->digits[i]) {
            return LESS;
        } else if (a->digits[i] > b->digits[i]) {
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
internal_cmp_digit_abs(const BigInt *a, DIGIT b)
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
internal_lt_digit_abs(const BigInt *a, DIGIT b)
{
    return internal_cmp_digit_abs(a, b) == LESS;
}

static DIGIT
arith_add_carry(DIGIT *carry, DIGIT a, DIGIT b)
{
    DIGIT sum;

    sum    = a + b + *carry;
    *carry = sum >> DIGIT_BITS; // Extract carry bits, if any.
    return sum & DIGIT_MASK;    // Get the valid digit bits.
}

/** @brief `|a| + |b|` where #a >= #b. */
LUAI_FUNC void
internal_add_bigint_unsigned(BigInt *dst, const BigInt *a, const BigInt *b)
{
    DIGIT carry = 0;
    int max_used, min_used, i = 0;

    max_used = a->len;
    min_used = b->len;
    for (; i < min_used; i += 1) {
        dst->digits[i] = arith_add_carry(&carry, a->digits[i], b->digits[i]);
    }

    for (; i < max_used; i += 1) {
        dst->digits[i] = arith_add_carry(&carry, a->digits[i], /*b=*/0);
    }
    dst->digits[i] = carry;
    internal_clamp(dst);
}

static DIGIT
arith_sub_borrow(DIGIT *borrow, DIGIT a, DIGIT b)
{
    DIGIT diff;

    // In unsigned subtraction, the carry is propagated all the way to MSB.
    // when overflow occurs.
    //
    // Concept check: DIGIT(3 - 4) where DIGIT == uint32_t
    //      =   0b00000000_00000000_00000000_00000011   ; 3
    //      -   0b00000000_00000000_00000000_00000100   ; 4
    //      =   0b11111111_11111111_11111111_11111111   ; DIGIT(-1)
    //
    // borrow = 0b11111111_11111111_11111111_11111111   ; DIGIT(-1)
    //       >> 31
    //        = 1
    //
    // diff   = 0b11111111_11111111_11111111_11111111   ; DIGIT(-1)
    //        & 0b00111111_11111111_11111111_11111111   ; DIGIT_MASK
    //        = 0b00111111_11111111_11111111_11111111   ; 1_073_741_823
    diff    = a - b - *borrow;
    *borrow = diff >> (DIGIT_TYPE_BITS - 1);
    return diff & DIGIT_MASK;
}


/** @brief `|a| - |b|` where `|a| >= |b|` and `#a >= #b`. */
LUAI_FUNC void
internal_sub_bigint_unsigned(BigInt *dst, const BigInt *a, const BigInt *b)
{
    DIGIT borrow = 0;
    int max_used, min_used, i = 0;

    max_used = a->len;
    min_used = b->len;
    for (; i < min_used; i += 1) {
        dst->digits[i] = arith_sub_borrow(&borrow, a->digits[i], b->digits[i]);
    }

    for (; i < max_used; i += 1) {
        dst->digits[i] = arith_sub_borrow(&borrow, a->digits[i], /*b=*/0);
    }
    dst->digits[i] = borrow;
    internal_clamp(dst);
}


/** @brief `|a| + |b|` where `b >= 0` and `b` fits in a `DIGIT`. */
LUAI_FUNC void
internal_add_digit(BigInt *dst, const BigInt *a, DIGIT b)
{
    DIGIT carry;
    int used;

    used  = a->len;
    carry = b;
    for (int i = 0; i < used; i += 1) {
        dst->digits[i] = arith_add_carry(&carry, a->digits[i], /*b=*/0);
    }
    dst->digits[used] = carry;
    internal_clamp(dst);
}


/** @brief `a - b` where a >= b and b >= 0 */
LUAI_FUNC void
internal_sub_digit(BigInt *dst, const BigInt *a, DIGIT b)
{
    DIGIT borrow;
    int used;

    used   = a->len;
    borrow = b;

    for (int i = 0; i < used; i += 1) {
        dst->digits[i] = arith_sub_borrow(&borrow, a->digits[i], /*b=*/0);
    }
    dst->digits[used] = borrow;
    internal_clamp(dst);
}


static DIGIT
arith_mul_carry(DIGIT *carry, DIGIT a, DIGIT b)
{
    WORD prod;
    DIGIT rem;

    // New carry is the 'overflow' from the product, e.g. 1000 in 1234
    // for base-1000.
    prod   = (cast(WORD)a * cast(WORD)b) + cast(WORD)*carry;
    *carry = arith_divmod_base(&rem, cast(uintmax_t)prod);

    // Only the portion of the product that fits in `DIGIT` will be written.
    return rem;
}


/** @brief `|a| * |b|` */
LUAI_FUNC void
internal_mul_digit(BigInt *dst, const BigInt *a, DIGIT multiplier)
{
    DIGIT carry = 0;
    int used;

    used = a->len;
    for (int i = 0; i < used; i += 1) {
        dst->digits[i] = arith_mul_carry(&carry, a->digits[i], multiplier);
    }
    dst->digits[used] = carry;
}


/** @brief `|a| * |b|` where #a >= #b.
 * @param dst Should alias neither `a` nor `b`.
 */
LUAI_FUNC void
internal_mul_bigint_unsigned(BigInt *restrict dst, const BigInt *a, const BigInt *b)
{
    int max_used, min_used;

    max_used = a->len;
    min_used = b->len;
    for (int b_i = 0; b_i < min_used; b_i += 1) {
        DIGIT multiplier, prod, carry = 0;

        multiplier = b->digits[b_i];
        for (int a_i = 0; a_i < max_used; a_i += 1) {
            prod = arith_mul_carry(&carry, a->digits[a_i], multiplier);
            dst->digits[a_i + b_i] += prod;
        }
        dst->digits[b_i + max_used] += carry;
    }
    internal_clamp(dst);
}


/** @brief `dst, mod = |a| / |b|, |a| % |b|`
 *  where `|a| > 0`
 *    and `|b| > 0`
 *    and `(0 <= mod and mod < BASE)`
 *
 * @return `mod`
 */
LUAI_FUNC DIGIT
internal_divmod_digit(BigInt *dst, const BigInt *a, DIGIT denominator)
{
    WORD carry = 0;

    // Divide MSD to LSD.
    for (int i = a->len - 1; i >= 0; i -= 1) {
        DIGIT digit = 0;

        // carry *= base where base is a power of 2.
        carry <<= DIGIT_BITS;

        // carry += a[i] since the lower `DIGIT` bits of carry are all 0.
        carry |= cast(WORD)a->digits[i];
        if (carry >= cast(WORD)denominator) {
            // dst[i] will be the portion of `carry` that fits.
            digit = cast(DIGIT)(carry / cast(WORD)denominator);

            // Remove the magnitude of dst[i] from the carry.
            carry -= cast(WORD)digit * cast(WORD)denominator;
        }

        dst->digits[i] = digit;
    }

    internal_clamp(dst);
    return cast(DIGIT)carry;
}

static bool
char_is_lower(char ch)
{
    return 'a' <= ch && ch <= 'z';
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

static char
char_to_upper(char ch)
{
    if (char_is_lower(ch)) {
        return 'A' + (ch - 'a');
    }
    return ch;
}

#define RADIX_TABLE_REVERSE_OFFSET  '+'


/** @brief Maps digits in the range `[0,64)` to their appropriate ASCII characters.
 *
 * @link https://www.rfc-editor.org/rfc/rfc4648.txt
 */
static const char
RADIX_TABLE[] = "0123456789"                                                   \
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"                                               \
    "abcdefghijklmnopqrstuvwxyz"                                               \
    "+/";


/** @brief Maps ASCII digits in the range `['+'..'z']` to their appropriate
 *  digit values.
 *
 * @link https://www.rfc-editor.org/rfc/rfc4648.txt
 */
static const uint8_t
RADIX_TABLE_REVERSE[] = {
    0x3e, 0xff, 0xff, 0xff, 0x3f,                               // +,-./
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, // 0123456789
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,                   // :;<=>?@
    0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, // ABCDEFGHIJ
    0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, // KLMNOPQRST
    0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,                         // UVWXYZ
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,                         // [\]^_`
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, // abcdefghij
    0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, // klmnopqrst
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d,                         // uvwxyz
};

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

static DIGIT
string_get_base(const char **s, size_t *s_len)
{
    DIGIT base = 10;
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
digit_length_base(DIGIT base)
{
    // Check prefixes for most common bases.
    switch (base) {
    case 2:     return DIGIT_BASE2_LENGTH;
    case 8:     return DIGIT_BASE8_LENGTH;
    case 10:    return DIGIT_BASE10_LENGTH;
    case 16:    return DIGIT_BASE16_LENGTH;
    }
    // Uncommon base, calculate as needed.
    return count_digits(DIGIT_MAX, base);
}


LUAI_FUNC BigInt *
internal_make_lstring(lua_State *L, const char *s, size_t s_len, DIGIT base)
{
    BigInt *dst;
    DIGIT *digits;
    Sign sign;
    int used, n_digits = 0;

    sign = string_get_sign(&s, &s_len);

    // Skip base prefix.
    {
        DIGIT tmp;

        tmp = string_get_base(&s, &s_len);
        // Didn't know the base beforehand, we do now.
        if (base == 0) {
            base = tmp;
        }
    }

    // Count number of base-`base` digits in the string
    for (size_t i = 0; i < s_len; i += 1) {
        size_t lut_i;
        int digit;
        char ch;

        ch = s[i];
        if (ch == ',' || ch == '_' || char_is_space(ch)) {
            continue;
        }

        // Can do case-insensitive conversion?
        if (base <= 36 && char_is_lower(ch)) {
            ch = char_to_upper(ch);
        }

        lut_i = cast(size_t)ch - cast(size_t)RADIX_TABLE_REVERSE_OFFSET;
        if (lut_i >= count_of(RADIX_TABLE_REVERSE)) {
            luaL_error(L, "Non-digit character '%c'", cast(int)base, ch);
            return NULL;
        }

        digit = cast(DIGIT)RADIX_TABLE_REVERSE[lut_i];
        if (digit == 0xff) {
            luaL_error(L, "Invalid base-%d digit '%c'", cast(int)base, ch);
            return NULL;
        }
        n_digits += 1;
    }

    // Will most likely over-allocate, but this is acceptable.
    used      = (n_digits / digit_length_base(base)) + 1;
    dst       = internal_make(L, used + 1);
    dst->sign = sign;
    digits    = dst->digits;

    for (size_t s_i = 0; s_i < s_len; s_i += 1) {
        DIGIT digit, mul_carry = 0, add_carry = 0;
        char ch;

        ch = s[s_i];
        if (ch == ',' || ch == '_' || char_is_space(ch)) {
            continue;
        }

        // Can do case-insensitive conversion?
        if (base <= 36 && char_is_lower(ch)) {
            ch = char_to_upper(ch);
        }

        // Assumed to never fail by this point.
        digit = cast(DIGIT)RADIX_TABLE_REVERSE[ch - RADIX_TABLE_REVERSE_OFFSET];

        // dst *= base
        for (int mul_i = 0; mul_i < used; mul_i += 1) {
            digits[mul_i] = arith_mul_carry(&mul_carry, digits[mul_i], base);
        }
        digits[used] = mul_carry;

        // dst += digit
        add_carry = digit;
        for (int add_i = 0; add_i < used; add_i += 1) {
            digits[add_i] = arith_add_carry(&add_carry, digits[add_i], /*b=*/0);
        }
        digits[used] = add_carry;
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

static void
string_write_char(Writer *w, char ch)
{
    // Increasing top still fits?
    if (w->left + 1 <= w->cap) {
        w->data[w->left++] = ch;
        return;
    }
    __builtin_trap();
}

static void
string_write_lstring(Writer *w, const char *data, size_t len)
{
    // Increasing top still fits?
    if (w->left + len <= w->cap) {
        char *top;

        top = w->data + w->left;
        memcpy(top, data, len);
        w->left += len;
        return;
    }
    __builtin_trap();
}

#define string_write_literal(w, s)  string_write_lstring(w, s, sizeof(s) - 1)

static void
string_write_char_back(Writer *w, char ch)
{
    if (w->left < w->right && w->right <= w->cap) {
        w->data[--w->right] = ch;
        return;
    }
    __builtin_trap();
}

/** @brief Writes `digit` from LSD to MSD, assuming a large `base_fast`. */
static void
string_write_digit(Writer *w, DIGIT digit, DIGIT base_fast, DIGIT base_slow)
{
    if (digit == 0) {
        string_write_char(w, '0');
        return;
    }

    while (digit > 0) {
        DIGIT lsd;

        lsd = digit % base_fast;
        while (lsd > 0) {
            DIGIT right;
            char ch;

            right = lsd % base_slow;
            ch    = RADIX_TABLE[right];
            string_write_char_back(w, ch);
            lsd /= base_slow;
        }
        digit /= base_fast;
    }
}


/** @brief Help optimize non-binary strings by getting the closest multiple of
 *  `base` to the binary `BASE`. */
static DIGIT
digit_get_base_fast(DIGIT base)
{
    DIGIT base_fast = 0;

    // 10**9
    if (base == 10) {
        return DIGIT_BASE_DECIMAL;
    }

    base_fast = base;
    while (base_fast * base < DIGIT_BASE) {
        base_fast *= base;
    }
    return base_fast;
}

static int
count_bits(const BigInt *a)
{
    int count = 0;
    if (!internal_is_zero(a)) {
        // Count MSD digits exactly, all other digits have fixed width.
        count += count_digits(a->digits[a->len - 1], /*base=*/2);
        count += (a->len - 1) * DIGIT_BITS;
    }
    return count;
}


LUAI_FUNC bool
internal_is_pow2(DIGIT v)
{
    return (v & (v - 1)) == 0;
}

/** @brief Approximate how many bytes are needed for string conversion.
 *  May overestimate. */
LUAI_FUNC size_t
internal_string_length(const BigInt *a, DIGIT base)
{
    size_t size = 0;
    if (!internal_is_zero(a)) {
        // Negative requires the '-' sign.
        size += cast(size_t)internal_is_neg(a);

        // Power of 2 requires the "0\d" prefix.
        if (internal_is_pow2(base)) {
            size += 2;
        }

        // Determine likely size for MSD.
        // All digits past MSD are fixed-size.
        size += cast(size_t)(count_digits(a->digits[a->len - 1], base));
        size += cast(size_t)(a->len - 1) * cast(size_t)digit_length_base(base);
    }
    return size;
}


/** @brief Write non-zero `|a|` as a non-binary `base` string. */
LUAI_FUNC void
internal_write_nonbinary_string(Writer *w, const BigInt *a, DIGIT base)
{
    BigInt *dst;

    // Help reduce the number of divmod calls.
    DIGIT base_fast;

    if (internal_is_neg(a)) {
        string_write_char(w, '-');
    }

    // Write from LSD to MSD.
    dst       = internal_make_copy(w->L, a);
    base_fast = digit_get_base_fast(base);
    while (!internal_is_zero(dst)) {
        DIGIT lsd;
        lsd = internal_divmod_digit(dst, dst, base_fast);
        string_write_digit(w, lsd, base_fast, base);
    }
}

static int
min_int(int a, int b)
{
    return (a < b) ? a : b;
}

/** @brief Slice `a` in terms of bits: `a[bit_i:bit_i+bit_count]`. */
static WORD
bitfield_extract(const BigInt *a, int bit_i, int bit_count)
{
    WORD bits, digit, shift, mask;
    int digit_i, num_bits;


    digit_i = bit_i / DIGIT_BITS; // Index of digit `bit_i` can be found at.
    bit_i  %= DIGIT_BITS; // Index of the bit within the digit.
    shift   = bit_i;

    if (bit_count == 1) {
        mask  = cast(WORD)1 << shift;
        digit = a->digits[digit_i];

        // Truncate result to just the 1 bit.
        return (digit & mask) != 0 ? 1 : 0;
    }

    // 1.) Check if all the bits fit in the 1st digit.
    //     Concept check: assuming base-2**30
    //          bit_i: 3, bit_count: 4 = bits [3..7)
    num_bits  = min_int(bit_count, DIGIT_BITS - bit_i);
    mask      = (cast(WORD)1 << cast(WORD)num_bits) - 1;
    bits      = (cast(WORD)a->digits[digit_i] >> shift) & mask;

    bit_count -= num_bits;
    if (bit_count == 0) {
        return bits;
    }
    digit_i += 1;

    // 2.) Check if the 2nd digit has the remaining bits we need.
    //      Concept check: assuming base-2**30:
    //          bit_i: 28, bit_count: 6 = bits [28..31), [31..34)
    shift    = num_bits;
    num_bits = min_int(bit_count, DIGIT_BITS);
    mask     = (cast(WORD)1 << cast(WORD)num_bits) - 1;
    bits    |= (cast(WORD)a->digits[digit_i] & mask) << shift;

    bit_count -= num_bits;
    if (bit_count == 0) {
        return bits;
    }
    digit_i += 1;

    // 3.) Check the 3rd digit.
    mask   = (cast(WORD)1 << cast(WORD)bit_count) - 1;
    shift += DIGIT_BITS;
    bits  |= (cast(WORD)a->digits[digit_i] & mask) << shift;

    return bits;
}


/** @brief Write `|a|` as a binary `base` string where `|a| > 0`. */
LUAI_FUNC void
internal_write_binary_string(Writer *w, const BigInt *a, DIGIT base)
{
    // `log2(base)` such that `base = 2**shift == 1<<shift`.
    int bit_count, shift, shift_fast = 0;

    if (internal_is_neg(a)) {
        string_write_char(w, '-');
    }

    // Powers of 2
    // | Decimal | Binary                 | Octal     | Hexadecimal  |
    // |       2 |  0b0000_0000_0000_0010 | 0o000_002 |       0x0002 |
    // |       4 |  0b0000_0000_0000_0100 | 0o000_004 |       0x0004 |
    // |       8 |  0b0000_0000_0000_1000 | 0o000_010 |       0x0008 |
    // |      16 |  0b0000_0000_0001_0000 | 0o000_020 |       0x0010 |
    // |      32 |  0b0000_0000_0010_0000 | 0o000_040 |       0x0020 |
    // |      64 |  0b0000_0000_0100_0000 | 0o000_100 |       0x0040 |
    // |     128 |  0b0000_0000_1000_0000 | 0o000_200 |       0x0080 |
    // |     256 |  0b0000_0001_0000_0000 | 0o000_400 |       0x0100 |
    // |     512 |  0b0000_0010_0000_0000 | 0o001_000 |       0x0200 |
    // |    1024 |  0b0000_0100_0000_0000 | 0o002_000 |       0x0400 |
    // |    2048 |  0b0000_1000_0000_0000 | 0o004_000 |       0x0800 |
    // |    4096 |  0b0001_0000_0000_0000 | 0o010_000 |       0x1000 |
    // |    8192 |  0b0010_0000_0000_0000 | 0o020_000 |       0x2000 |
    // |  16,384 |  0b0100_0000_0000_0000 | 0o040_000 |       0x4000 |
    // |  32,768 |  0b1000_0000_0000_0000 | 0o100_000 |       0x8000 |
    //
    // base-8:   8=3-bit,   64=6-bits,  512=9-bits,    4096=12-bits
    // base-16: 16=4-bits, 256=8-bits, 4096=12-bits, 65,536=16-bits
    switch (base) {
    case 2:  shift = 1; shift_fast = DIGIT_BITS; string_write_literal(w, "0b"); break;
    case 8:  shift = 3; string_write_literal(w, "0o"); break;
    case 16: shift = 4; string_write_literal(w, "0x"); break;
    case 32: shift = 5; break;
    case 64: shift = 6; break;
    default: __builtin_unreachable();
    }

    // Find nearest power of `shift` to `BASE`.
    if (shift_fast == 0) {
        shift_fast = shift;
        while (cast(size_t)shift_fast * cast(size_t)shift < DIGIT_BITS) {
            shift_fast *= shift;
        }
    }


    // Write from LSD to MSD.
    bit_count = count_bits(a);
    for (int bit_i = 0; bit_i < bit_count; bit_i += shift_fast) {
        WORD bitset;
        int bits_to_get;
        char ch;

        // When we reach MSD we might not be able to read all bits.
        bits_to_get = min_int(bit_count - bit_i, shift_fast);

        // We assume that the resulting bitfield can be converted.
        bitset = bitfield_extract(a, bit_i, bits_to_get);
        if (bitset >= cast(WORD)base) {
            DIGIT bit;
            while (bitset > 0) {
                bit      = bitset & (base - 1);
                ch       = RADIX_TABLE[bit];
                bitset >>= cast(WORD)shift;
                string_write_char_back(w, ch);
            }
        } else {
            ch = RADIX_TABLE[bitset];
            string_write_char_back(w, ch);
        }
    }
}

// Macro cleanup
#undef RADIX_TABLE_REVERSE_OFFSET
#undef string_write_literal

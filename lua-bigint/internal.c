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
internal_new_from_lstring(lua_State *L, const char *s, size_t s_len, DIGIT base);

LUAI_FUNC BigInt *
internal_ensure_bigint(lua_State *L, int arg_i)
{
    return cast(BigInt *)luaL_checkudata(L, arg_i, BIGINT_MTNAME);
}

LUAI_FUNC bool
internal_is_pow2(DIGIT v)
{
    return (v & (v - 1)) == 0;
}

static int
integer_count_digits(uintmax_t value, DIGIT base)
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
internal_new(lua_State *L, int digit_count)
{
    BigInt *dst;
    size_t  array_size;

    // -> (..., bi: BigInt *)
    array_size = sizeof(dst->digits[0]) * cast(size_t)digit_count;
    dst = cast(BigInt *)lua_newuserdata(L, sizeof(*dst) + array_size);
    dst->sign = POSITIVE;
    dst->len = cast(size_t)digit_count;
    memset(dst->digits, 0, array_size);

    luaL_getmetatable(L, BIGINT_MTNAME); // -> (..., bi, mt: {})
    lua_setmetatable(L, -2);             // -> (..., bi) ; setmetatable(bi, mt)
    return dst;
}

LUAI_FUNC BigInt *
internal_new_copy(lua_State *L, const BigInt *src)
{
    BigInt *dst = internal_new(L, src->len);
    dst->sign = src->sign;
    memcpy(dst->digits, src->digits, sizeof(src->digits[0]) * src->len);
    return dst;
}


LUAI_FUNC BigInt *
internal_new_from_integer(lua_State *L, lua_Integer value)
{
    BigInt *dst;
    uintmax_t value_abs;
    int digit_count;

    value_abs = internal_integer_abs(value);
    digit_count = integer_count_digits(value_abs, DIGIT_BASE);
    dst = internal_new(L, digit_count);
    dst->sign = (value >= 0) ? POSITIVE : NEGATIVE;

    // Write `value` from LSD to MSD.
    for (int i = 0; i < dst->len; i += 1) {
        // dst[i] = value_abs % DIGIT_BASE
        dst->digits[i] = cast(DIGIT)(value_abs & DIGIT_MASK);
        // value //= DIGIT_BASE
        value_abs >>= DIGIT_SHIFT;
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
    arg.type = ARG_INVALID;
    arg.integer = 0;

    switch (lua_type(L, arg_i)) {
    case LUA_TNUMBER: {
        lua_Number  n;
        lua_Integer i;

        n = lua_tonumber(L, arg_i);
        i = cast(lua_Integer)n;
        // Number type accurately represents the integer (no truncation)?
        if (cast(lua_Number)i == n) {
            arg.type = ARG_INTEGER;
            arg.integer = i;
        } else {
            luaL_typerror(L, arg_i, "integer");
        }
        break;
    }
    case LUA_TSTRING:
        arg.type = ARG_STRING;
        arg.lstring.data = lua_tolstring(L, arg_i, &arg.lstring.len);
        break;
    default:
        arg.type = ARG_BIGINT;
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

    arg = internal_arg_get(L, first_arg);
    base = cast(DIGIT)luaL_optinteger(L, first_arg + 1, /*def=*/0);
    switch (arg.type) {
    case ARG_BIGINT:
        luaL_argcheck(L, base == 0 || base == 10, first_arg + 1, "Don't.");
        internal_new_copy(L, arg.bigint);
        break;
    case ARG_INTEGER:
        // Don't follow `tonumber()` where you can pass a number and read it
        // in another base...
        luaL_argcheck(L, base == 0 || base == 10, first_arg + 1, "Don't.");
        internal_new_from_integer(L, arg.integer);
        break;
    case ARG_STRING:
        internal_new_from_lstring(L, arg.lstring.data, arg.lstring.len, base);
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


/** @brief `|a| + |b|` where #a >= #b. */
LUAI_FUNC void
internal_add_bigint_unsigned(BigInt *dst, const BigInt *a, const BigInt *b)
{
    DIGIT sum, carry = 0;
    int max_used, min_used, i = 0;

    max_used = a->len;
    min_used = b->len;
    for (; i < min_used; i += 1) {
        sum = a->digits[i] + b->digits[i] + carry;
        carry = sum >> DIGIT_SHIFT; // Extra the carry bit.
        dst->digits[i] = sum & DIGIT_MASK; // Mask out the carry bit.
    }

    for (; i < max_used; i += 1) {
        sum = a->digits[i] + carry;
        carry = sum >> DIGIT_SHIFT;
        dst->digits[i] = sum & DIGIT_MASK;
    }
    dst->digits[i] = carry;
    internal_clamp(dst);
}


/** @brief `|a| - |b|` where `|a| >= |b|` and `#a >= #b`. */
LUAI_FUNC void
internal_sub_bigint_unsigned(BigInt *dst, const BigInt *a, const BigInt *b)
{
    DIGIT diff, borrow = 0;
    int max_used, min_used, i = 0;

    max_used = a->len;
    min_used = b->len;
    for (; i < min_used; i += 1) {
        // In unsigned subtraction, the carry is propagated all the way to
        // MSB when overflow occurs. In the final mask, the unneeded magnitude
        // is masked out (e.g. bit[30] when bits[0:30] are used).
        //
        // Concept check: DIGIT(3 - 4) where DIGIT == uint32_t
        //      =   0b00000000_00000000_00000000_00000011   ; 3
        //      -   0b00000000_00000000_00000000_00000100   ; 4
        //      =   0b11111111_11111111_11111111_11111111   ; DIGIT(-1)
        //
        // borrow = 0b11111111_11111111_11111111_11111111   ; DIGIT(-1)
        //       >> (DIGIT_TYPE_BITS - 1)                   ; 31
        //        = 1
        //
        // diff   = 0b11111111_11111111_11111111_11111111   ; DIGIT(-1)
        //        & 0b00111111_11111111_11111111_11111111   ; DIGIT_MASK
        //        = 0b00111111_11111111_11111111_11111111   ; 1_073_741_823
        diff = a->digits[i] - b->digits[i]  - borrow;
        borrow = diff >> (DIGIT_TYPE_BITS - 1);
        dst->digits[i] = diff & DIGIT_MASK;
    }

    for (; i < max_used; i += 1) {
        diff = a->digits[i] - borrow;
        borrow = diff >> (DIGIT_TYPE_BITS - 1);
        dst->digits[i] = diff & DIGIT_MASK;
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
        DIGIT sum;

        sum = a->digits[i] + carry;
        carry = sum >> DIGIT_SHIFT;
        dst->digits[i] = sum & DIGIT_MASK;
    }
    dst->digits[used] = carry;
    internal_clamp(dst);
}


/** @brief `a - b` where a >= b and b >= 0 */
LUAI_FUNC void
internal_sub_digit(BigInt *dst, const BigInt *a, DIGIT b)
{
    DIGIT diff, borrow;
    int used;

    used   = a->len;
    borrow = b;

    for (int i = 0; i < used; i += 1) {
        diff = a->digits[i] - borrow;
        borrow = diff >> (DIGIT_TYPE_BITS - 1);
        dst->digits[i] = diff & DIGIT_MASK;
    }
    dst->digits[used] = borrow;
    internal_clamp(dst);
}


/** @brief `|a| * |b|` */
LUAI_FUNC void
internal_mul_digit(BigInt *dst, const BigInt *a, DIGIT multiplier)
{
    WORD prod;
    DIGIT carry = 0;
    int used;

    used = a->len;
    for (int i = 0; i < used; i += 1) {
        prod = cast(WORD)a->digits[i] * cast(WORD)multiplier + cast(WORD)carry;
        // New carry is the 'overflow' from the product.
        carry = cast(DIGIT)(prod >> DIGIT_SHIFT);
        dst->digits[i] = cast(DIGIT)(prod & DIGIT_MASK);
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
        WORD multiplier, prod;
        DIGIT carry = 0;

        multiplier = cast(WORD)b->digits[b_i];
        for (int a_i = 0; a_i < max_used; a_i += 1) {
            prod  = cast(WORD)a->digits[a_i] * multiplier + cast(WORD)carry;
            carry = cast(DIGIT)(prod >> DIGIT_SHIFT);
            dst->digits[a_i + b_i] += cast(DIGIT)(prod & DIGIT_SHIFT);
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
        carry <<= DIGIT_SHIFT;

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


/** @brief Maps digits in the range `[0,36)` to their appropriate ASCII characters.
 *
 * @link https://www.rfc-editor.org/rfc/rfc4648.txt
 */
static char
digit_to_char(DIGIT digit)
{
    static const char
    DIGIT_TO_CHAR_TABLE[] = "0123456789abcdefghijklmnopqrstuvwxyz";

    return DIGIT_TO_CHAR_TABLE[digit];
}

static DIGIT
char_to_digit(char ch, DIGIT base)
{
    DIGIT digit = DIGIT_MAX;
    if ('0' <= ch && ch <= '9') {
        digit = cast(DIGIT)(ch - '0');
    } else if ('A' <= ch && ch <= 'Z') {
        digit = cast(DIGIT)(ch - 'A' + 10);
    } else if ('a' <= ch && ch <= 'z') {
        digit = cast(DIGIT)(ch - 'a' + 10);
    }

    if (0 <= digit && digit < base) {
        return digit;
    }
    return DIGIT_MAX;
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


/** @brief How many base-`base` digits can fit in a single base-`DIGIT_BASE`
 *  digit? */
static int
digit_length_base(DIGIT base)
{
    if (base == 2) {
        return DIGIT_SHIFT;
    }
    // Calculate as needed. This is not called in hot loops anyway.
    return integer_count_digits(DIGIT_MAX, base);
}


LUAI_FUNC BigInt *
internal_new_from_lstring(lua_State *L, const char *s, size_t s_len, DIGIT base)
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
        DIGIT digit;
        char ch;

        ch = s[i];
        if (ch == ',' || ch == '_' || char_is_space(ch)) {
            continue;
        }

        digit = char_to_digit(ch, base);
        if (digit == DIGIT_MAX) {
            luaL_error(L, "Invalid base-%d digit '%c'", cast(int)base, ch);
            return NULL;
        }
        n_digits += 1;
    }

    // Will most likely over-allocate, but this is acceptable.
    used      = (n_digits / digit_length_base(base)) + 1;
    dst       = internal_new(L, used + 1);
    dst->sign = sign;
    digits    = dst->digits;

    for (size_t s_i = 0; s_i < s_len; s_i += 1) {
        DIGIT digit, mul_carry = 0, add_carry = 0;
        char ch;

        ch = s[s_i];
        if (ch == ',' || ch == '_' || char_is_space(ch)) {
            continue;
        }

        // Assumed to never fail by this point.
        digit = char_to_digit(ch, base);

        // dst *= base
        for (int i = 0; i < used; i += 1) {
            WORD prod;

            prod = cast(WORD)digits[i] * cast(WORD)base + cast(WORD)mul_carry;
            mul_carry = cast(DIGIT)(prod >> DIGIT_SHIFT);
            digits[i] = cast(DIGIT)(prod & DIGIT_MASK);
        }
        digits[used] = mul_carry;

        // dst += digit
        add_carry = digit;
        for (int i = 0; i < used; i += 1) {
            DIGIT sum;

            sum = digits[i] + add_carry;
            add_carry = sum >> DIGIT_SHIFT;
            digits[i] = sum & DIGIT_MASK;
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
        // ..., tostring, tostring, arg[i]
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
    assertfln(w->left + 1 <= w->cap,
        "Out of bounds index: left=%zu > cap=%zu",
        w->left, w->cap);

    w->data[w->left++] = ch;
}

static void
string_write_lstring(Writer *w, const char *data, size_t len)
{
    // Increasing top still fits?
    char *top;

    assertfln(w->left + len <= w->cap,
        "Out of bounds index: left=%zu + len=%zu > cap=%zu",
        w->left, len, w->cap);

    top = &w->data[w->left];
    memcpy(top, data, len);
    w->left += len;
}

#define string_write_literal(w, s)  string_write_lstring(w, s, sizeof(s) - 1)

static void
string_write_char_back(Writer *w, char ch)
{
    assertfln(w->left < w->right && w->right <= w->cap,
        "Out of bounds index: left=%zu, right=%zu, cap=%zu",
        w->left, w->right, w->cap);

    w->data[--w->right] = ch;
}


/** @brief Help optimize non-binary strings by getting the closest multiple of
 *  `base` to the binary `DIGIT_BASE`. */
static DIGIT
digit_get_base_fast(int *digit_count, DIGIT base)
{
    DIGIT base_fast = 0;

    // 10**9
    if (base == 10) {
        *digit_count = DIGIT_SHIFT_DECIMAL;
        return DIGIT_BASE_DECIMAL;
    }

    *digit_count = 1;
    base_fast = base;
    while (base_fast * base < DIGIT_BASE) {
        base_fast *= base;
        *digit_count += 1;
    }
    return base_fast;
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

        // base-2, base-8 and base-16 requires the "0\d" prefix.
        if (base == 2 || base == 8 || base == 16) {
            size += 2;
        }

        // Determine likely size for MSD.
        // All digits past MSD are fixed-size.
        size += cast(size_t)(integer_count_digits(a->digits[a->len - 1], base));
        size += cast(size_t)(a->len - 1) * cast(size_t)digit_length_base(base);
    }
    return size;
}


/** @brief Writes `digit` from LSD to MSD, assuming a large `base_fast`.
 * @return The number of significant digits written.
 */
static int
string_write_digit(Writer *w, DIGIT digit, DIGIT base_fast, DIGIT base_slow)
{
    int written = 0;
    if (digit == 0) {
        string_write_char(w, '0');
        return 1;
    }

    while (digit > 0) {
        DIGIT lsd;

        lsd = digit % base_fast;
        while (lsd > 0) {
            string_write_char_back(w, digit_to_char(lsd % base_slow));
            lsd /= base_slow;
            written += 1;
        }
        digit /= base_fast;
    }
    return written;
}


/** @brief Write non-zero `|a|` as a non-binary `base` string. */
LUAI_FUNC void
internal_write_nonbinary_string(Writer *w, const BigInt *a, DIGIT base)
{
    BigInt *dst;

    // Help reduce the number of divmod calls.
    DIGIT base_fast;
    int digit_count;

    if (internal_is_neg(a)) {
        string_write_char(w, '-');
    }

    // Write from LSD to MSD.
    dst       = internal_new_copy(w->L, a);
    base_fast = digit_get_base_fast(&digit_count, base);
    while (!internal_is_zero(dst)) {
        DIGIT lsd_fast;
        int written, lzeroes;

        lsd_fast = internal_divmod_digit(dst, dst, base_fast);
        written = string_write_digit(w, lsd_fast, base_fast, base);
        lzeroes = digit_count - written;

        // Leading zeroes needed for non-MSD? Recall that `divmod` may
        // mutate `len`. If we just divmod the MSD, then `len` is 0.
        if (dst->len >= 1) {
            while (lzeroes > 0) {
                string_write_char_back(w, '0');
                lzeroes -= 1;
            }
        }
    }
}

static int
count_bits(const BigInt *a)
{
    int count = 0;
    if (!internal_is_zero(a)) {
        // Count MSD digits exactly, all other digits have fixed width.
        count += integer_count_digits(a->digits[a->len - 1], /*base=*/2);
        count += (a->len - 1) * DIGIT_SHIFT;
    }
    return count;
}

static int
int_min(int a, int b)
{
    return (a < b) ? a : b;
}

/** @brief Slice `a` in terms of bits: `a[bit_i:bit_i+bit_count]`. */
static WORD
bitfield_extract(const BigInt *a, int bit_i, int bit_count)
{
    WORD bits, digit, shift, mask;
    int digit_i, num_bits;


    digit_i = bit_i / DIGIT_SHIFT; // Index of digit `bit_i` can be found at.
    bit_i  %= DIGIT_SHIFT; // Index of the bit within the digit.
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
    num_bits  = int_min(bit_count, DIGIT_SHIFT - bit_i);
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
    num_bits = int_min(bit_count, DIGIT_SHIFT);
    mask     = (cast(WORD)1 << cast(WORD)num_bits) - 1;
    bits    |= (cast(WORD)a->digits[digit_i] & mask) << shift;

    bit_count -= num_bits;
    if (bit_count == 0) {
        return bits;
    }
    digit_i += 1;

    // 3.) Check the 3rd digit.
    mask   = (cast(WORD)1 << cast(WORD)bit_count) - 1;
    shift += DIGIT_SHIFT;
    bits  |= (cast(WORD)a->digits[digit_i] & mask) << shift;

    return bits;
}

/** @brief The the largest multiple of `shift` closest to `WORD_SHIFT` such
 *  that we we can read that many bits at once, reducing bigint calls. */
static int
shift_get(int *shift_fast, int shift)
{
    int tmp;
    if (shift == 1) {
        *shift_fast = WORD_SHIFT;
        return 1;
    }

    tmp = shift;
    while (tmp + shift <= cast(int)WORD_SHIFT) {
        tmp += shift;
    }
    *shift_fast = tmp;
    return shift;
}


/** @brief Write `|a|` as a binary `base` string where `|a| > 0`. */
LUAI_FUNC void
internal_write_binary_string(Writer *w, const BigInt *a, DIGIT base)
{
    // `log2(base)` such that `base = 2**shift == 1<<shift`.
    int bit_count, shift, shift_fast;

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
    case 2:  shift = shift_get(&shift_fast, 1); string_write_literal(w, "0b"); break;
    case 4:  shift = shift_get(&shift_fast, 2); break;
    case 8:  shift = shift_get(&shift_fast, 3); string_write_literal(w, "0o"); break;
    case 16: shift = shift_get(&shift_fast, 4); string_write_literal(w, "0x"); break;
    case 32: shift = shift_get(&shift_fast, 5); break;
    default: unreachable();
    }

    // Write from LSD to MSD.
    bit_count = count_bits(a);
    for (int bit_i = 0; bit_i < bit_count; bit_i += shift_fast) {
        WORD bits;
        int bits_to_get;

        // When we reach MSD we might not be able to read all bits.
        bits_to_get = int_min(bit_count - bit_i, shift_fast);
        bits = bitfield_extract(a, bit_i, bits_to_get);
        int counter = 0;
        do {
            DIGIT digit;

            digit = cast(DIGIT)(bits & (cast(WORD)base - 1));
            bits >>= cast(WORD)shift;
            counter += shift;
            string_write_char_back(w, digit_to_char(digit));
        } while (bits > 0);

        // Add leading zeroes for digits before MSD.
        if (bit_i + shift_fast < bit_count) {
            while (counter < shift_fast) {
                counter += shift;
                string_write_char_back(w, '0');
            }
        }
    }
}

// Macro cleanup
#undef CHAR_TO_DIGIT_OFFSET
#undef string_write_literal

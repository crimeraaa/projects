#include <string.h> // memset

#include "bigint.h"

LUAI_FUNC BigInt *
internal_make_lstring(lua_State *L, const char *s, size_t s_len);

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


static lua_Integer
integer_abs(lua_Integer value)
{
    return (value >= 0) ? value : -value;
}


/** @brief `|value|` which also works for `value == min(type)`. */
static uintmax_t
integer_abs_unsigned(lua_Integer value)
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

    value_abs   = integer_abs_unsigned(value);
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

    const Digit *a_ = a->digits;
    const Digit *b_ = b->digits;

    // 3.) Same signs and same lengths. Compare digits from MSD to LSD.
    for (size_t i = a->len; i > 0; i -= 1) {
        Digit a_i = a_[i - 1];
        Digit b_i = b_[i - 1];
        if (a_i < b_i) {
            return (a_is_neg) ? GREATER : LESS;
        } else if (a_i > b_i) {
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

    const Digit *a_ = a->digits;
    const Digit *b_ = b->digits;
    for (size_t i = a->len; i > 0; i -= 1) {
        size_t j = i - 1;
        if (a_[j] < b_[j]) {
            return LESS;
        } else if (a_[j] > b_[j]) {
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
        return internal_cmp_digit_abs(a, cast(Digit)integer_abs(b));
    }

    // `b >= DIGIT_BASE` and `#b > 1`
    uintmax_t b_abs = integer_abs_unsigned(b);
    Digit     b_[sizeof(b_abs) / sizeof(a->digits[0])];
    size_t    b_len = count_of(b_);
    if (a->len != b_len) {
        return (a->len < b_len) ? LESS : GREATER;
    }

    // Separate the digits of `b`, from LSD to MSD.
    {
        uintmax_t tmp = b_abs;
        for (size_t i = 0; i < b_len; i += 1) {
            b_[i] = cast(Digit)(tmp % BIGINT_DIGIT_BASE);
            tmp /= BIGINT_DIGIT_BASE;
        }
    }

    // Compare MSD to LSD.
    const Digit *a_ = a->digits;
    for (size_t i = b_len; i > 0; i -= 1) {
        if (a_[i] < b_[i]) {
            return LESS;
        } else if (a_[i] > b_[i]) {
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
arith_add_carry(Digit a, Digit b, Digit *carry)
{
    Digit sum = a + b + *carry;
    *carry = cast(Digit)(sum >= BIGINT_DIGIT_BASE);
    if (*carry == 1) {
        sum -= BIGINT_DIGIT_BASE;
    }
    return sum;
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

    const Digit *a_   = a->digits;
    const Digit *b_   = b->digits;
    Digit       *dst_ = dst->digits;

    size_t i     = 0;
    Digit  carry = 0;
    for (; i < min_used; i += 1) {
        dst_[i] = arith_add_carry(a_[i], b_[i], &carry);
    }

    for (; i < max_used; i += 1) {
        dst_[i] = arith_add_carry(a_[i], /*b=*/0, &carry);
    }
    dst_[i] = carry;
    return internal_clamp(dst);
}

static Digit
arith_sub_borrow(Digit a, Digit b, Word *borrow)
{
    Word diff = cast(Word)a - cast(Word)b - *borrow;
    *borrow = cast(Word)(diff < 0);
    if (*borrow == 1) {
        diff += BIGINT_DIGIT_BASE;
    }
    return cast(Digit)diff;
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

    const Digit *a_   = a->digits;
    const Digit *b_   = b->digits;
    Digit       *dst_ = dst->digits;

    Word borrow = 0;
    size_t i = 0;
    for (; i < min_used; i += 1) {
        dst_[i] = arith_sub_borrow(a_[i], b_[i], &borrow);
    }

    for (; i < max_used; i += 1) {
        dst_[i] = arith_sub_borrow(a_[i], /*b=*/0, &borrow);
    }
    dst_[i] = cast(Digit)borrow;
    return internal_clamp(dst);
}


/** @brief `|a| + |b|` where `b >= 0` and `b` fits in a `Digit`. */
LUAI_FUNC BigInt *
internal_add_digit_unsigned(lua_State *L, const BigInt *a, Digit b)
{
    size_t used = a->len;

    // May overallocate by 1 digit, which is acceptable.
    BigInt *dst = internal_make(L, used + 1);

    const Digit *a_   = a->digits;
    Digit       *dst_ = dst->digits;

    Digit carry = b;
    for (size_t i = 0; i < used; i += 1) {
        dst_[i] = arith_add_carry(a_[i], /*b=*/0, &carry);
    }
    dst_[used] = carry;
    return internal_clamp(dst);
}


/** @brief `a - b` where a >= b and b >= 0 */
LUAI_FUNC BigInt *
internal_sub_digit_unsigned(lua_State *L, const BigInt *a, Digit b)
{
    size_t  used = a->len;
    BigInt *dst  = internal_make(L, used + 1);

    const Digit *a_   = a->digits;
    Digit       *dst_ = dst->digits;

    Word borrow = cast(Word)b;
    for (size_t i = 0; i < used; i += 1) {
        dst_[i] = arith_sub_borrow(a_[i], /*b=*/0, &borrow);
    }
    dst_[used] = cast(Digit)borrow;
    return internal_clamp(dst);
}


static Digit
arith_mul_carry(Digit a, Digit b, Word *carry)
{
    Word prod = (cast(Word)a * cast(Word)b) + *carry;

    // New carry is the 'overflow' from the product, e.g. 1000 in 1234
    // for base-1000.
    *carry = prod / BIGINT_DIGIT_BASE;

    // Only the portion of the product that fits in `Digit` will be written.
    return cast(Digit)(prod % BIGINT_DIGIT_BASE);
}


/** @brief `|a| * |b|` */
LUAI_FUNC BigInt *
internal_mul_digit_unsigned(lua_State *L, const BigInt *a, Digit b)
{
    size_t  used = a->len;
    BigInt *dst  = internal_make(L, used);
    
    const Digit *a_   = a->digits;
    Digit       *dst_ = dst->digits;

    Word carry = 0;
    for (size_t i = 0; i < used; i += 1) {
        dst_[i] = arith_mul_carry(a_[i], b, &carry);
    }
    dst_[used] = cast(Digit)carry;
    return dst;
}


/** @brief `|a| + |b|` where `DIGIT_BASE <= b` */
LUAI_FUNC BigInt *
internal_add_integer_unsigned(lua_State *L, const BigInt *a, lua_Integer b)
{
    uintmax_t carry = integer_abs_unsigned(b);
    size_t    b_len = 2;
    assert(carry >= BIGINT_DIGIT_BASE);

    size_t  used = (a->len < b_len) ? b_len : a->len;
    BigInt *dst  = internal_make(L, used + 1);

    const Digit *a_   = a->digits;
    Digit       *dst_ = dst->digits;
    
    // Add up digit places common to both `a` and `b`.
    size_t i = 0;
    for (; i < a->len; i += 1) {
        Digit lsd = cast(Digit)(carry % BIGINT_DIGIT_BASE);
        Digit sum = a_[i] + lsd;
        if (sum >= BIGINT_DIGIT_BASE) {
            sum -= BIGINT_DIGIT_BASE;
        }
        dst_[i] = cast(Digit)sum;
        carry /= BIGINT_DIGIT_BASE;
    }

    // `b` was larger, copy over its higher digits.
    for (; i < b_len; i += 1) {
        dst_[i] = cast(Digit)(carry % BIGINT_DIGIT_BASE);
        carry /= BIGINT_DIGIT_BASE;
    }

    dst_[i] = cast(Digit)carry;
    return internal_clamp(dst);
}


/** @brief `|a| - |b|` where `|b| >= BIGINT_DIGIT_BASE` */
LUAI_FUNC BigInt *
internal_sub_integer_unsigned(lua_State *L, const BigInt *a, lua_Integer b)
{
    uintmax_t b_abs = integer_abs_unsigned(b);
    assert(b_abs >= BIGINT_DIGIT_BASE);
    unused(a);
    unused(b_abs);
    luaL_error(L, "subtracting large `integer` not yet supported");
    return NULL;
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
internal_make_lstring(lua_State *L, const char *s, size_t s_len)
{
    Sign   sign     = string_get_sign(&s, &s_len);
    Digit  base     = string_get_base(&s, &s_len);
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
    size_t used = (n_digits / cast(size_t)digit_length_base(base)) + 1;
    BigInt *dst = internal_make(L, used + 1);
    dst->sign = sign;

    Digit *dst_ = dst->digits;
    for (size_t s_i = 0; s_i < s_len; s_i += 1) {
        char ch = s[s_i];
        if (ch == ',' || ch == '_') {
            continue;
        }

        int digit = char_to_digit(ch, base);

        // dst *= base
        Word mul_carry = 0;
        for (size_t mul_i = 0; mul_i < used; mul_i += 1) {
            dst_[mul_i] = arith_mul_carry(dst_[mul_i], base, &mul_carry);
        }
        dst_[used] = cast(Digit)mul_carry;

        // dst += digit
        Digit add_carry = digit;
        for (size_t add_i = 0; add_i < used; add_i += 1) {
            dst_[add_i] = arith_add_carry(dst_[add_i], /*b=*/0, &add_carry);
        }
        dst_[used] = add_carry;
    }
    return internal_clamp(dst);
}

static size_t
digit_write_binary(Digit digit, char buf[], Digit base)
{
    if (digit == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }

    size_t end = 0;
    for (; digit != 0; end += 1) {
        Digit lsd = digit % base;
        char ch = cast(char)lsd + ((lsd < 10) ? '0' : 'a' - 10);
        buf[end] = ch;
        digit /= base;
    }
    buf[end] = '\0';

    // Reverse the string.
    for (size_t left = 0, right = end - 1; left < right; left += 1, right -= 1) {
        char tmp = buf[left];
        buf[left] = buf[right];
        buf[right] = tmp;
    }
    return end;
}

LUAI_FUNC void
internal_write_binary_string(luaL_Buffer *sb, const BigInt *a, Digit base)
{
    lua_State *L = sb->L;
    switch (base) {
    case 2:  luaL_addstring(sb, "0b"); break;
    case 8:  luaL_addstring(sb, "0o"); break;
    case 16: luaL_addstring(sb, "0x"); break;
    default: luaL_error(L, "non-binary base %d", cast(int)base); return;
    }
    STUB(L, "unimplemented");

    // Convert base-`BASE` array to a base-{2,8,16} array.
    size_t  d_len = cast(size_t)digit_length_base(base);
    size_t  a_len = a->len;
    size_t  used  = a_len * d_len;
    BigInt *tmp   = internal_make(L, used);

    const Digit *a_   = a->digits;
    Digit       *tmp_ = tmp->digits;
    unused(tmp_);

    // Write from MSD to LSD.
    for (size_t i = a->len; i > 0; i -= 1) {
        // Convert base-`BASE` to a decimal integer representing base-{2,8,16}.
        char buf[BIGINT_DIGIT_BASE2_LENGTH + 1];
        size_t buf_used = digit_write_binary(a_[i], buf, base);
        unused(buf_used);
    }
}

LUAI_FUNC void
internal_write_decimal_string(luaL_Buffer *sb, const BigInt *a)
{
    // Write the MSD which will never have leading zeroes.
    size_t msd_index = a->len - 1;
    const Digit *a_ = a->digits;
    string_write_digit(sb, a_[msd_index], /*base=*/10);

    // Write from MSD - 1 to LSD.
    // Don't subtract 1 immediately due to unsigned overflow.
    for (size_t i = msd_index; i > 0; i -= 1) {
        Digit digit = a_[i - 1];

        // Convert base-`BASE` to base-`base` with leading zeroes as needed.
        Word tmp = cast(Word)digit;

        // Avoid infinite loops when multiplying by zero.
        if (tmp == 0) {
            tmp = 1;
        }

        while (tmp * cast(Word)10 < BIGINT_DIGIT_BASE) {
            luaL_addchar(sb, '0');
            tmp *= cast(Word)10;
        }
        string_write_digit(sb, digit, /*base=*/10);
    }
}

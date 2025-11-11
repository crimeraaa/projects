#ifndef BIGINT_H
#define BIGINT_H

#include <stddef.h>
#include <stdint.h>
#include <limits.h>

// #include <lua.h>
// #include <lauxlib.h>

typedef uint32_t Digit;
typedef uint64_t Word;

typedef struct {
    Digit *data;
    size_t len;
} Digit_Slice;

#define BIGINT_LEN_MASK     (SIZE_MAX >> 1)
#define BIGINT_SIGN_MASK    (~BIGINT_LEN_MASK)
#define BIGINT_DIGIT_BITS   (sizeof(Digit) * CHAR_BIT)
#define BIGINT_DIGIT_BASE   (1ULL << BIGINT_DIGIT_BITS)


#define cast(T) (T)
#define count_of(array)     (sizeof(array) / sizeof((array)[0]))

typedef struct {
    // Bit  63:   sign
    // Bits 62-0: len
    size_t len;
    Digit digits[];
} BigInt;

// LUALIB_API int
// luaopen_bigint(lua_State *L);

#endif /* BIGINT_H */

#ifndef BIGINT_H
#define BIGINT_H

#include <limits.h>
#include <lua.h>
#include <lauxlib.h>

#include "../common.h"

typedef uint32_t Digit;
typedef uint64_t Word;

#define BIGINT_DIGIT_BASE   1000000000
#define BIGINT_MTNAME       "bigint.BigInt"

enum BigInt_Sign {
    BIGINT_POSITIVE,
    BIGINT_NEGATIVE,
};
typedef enum BigInt_Sign Sign;

typedef struct BigInt BigInt;
struct BigInt {
    Sign   sign;
    size_t len;
    Digit  digits[];
};

LUALIB_API int
luaopen_bigint(lua_State *L);

#endif /* BIGINT_H */

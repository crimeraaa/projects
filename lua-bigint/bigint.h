#ifndef BIGINT_H
#define BIGINT_H

#include <limits.h>
#include <lua.h>
#include <lauxlib.h>

#include "../common.h"

typedef uint32_t Digit;
typedef int64_t  Word;

#define BIGINT_DIGIT_BASE   1000000000
#define BIGINT_MTNAME       "bigint.BigInt"

enum Sign {
    POSITIVE,
    NEGATIVE,
};
typedef enum Sign Sign;

enum Comparison {
    LESS    = -1,
    EQUAL   = 0,
    GREATER = 1,
};

typedef enum Comparison Comparison;

typedef struct BigInt BigInt;
struct BigInt {
    Sign   sign;
    size_t len;
    Digit  digits[];
};

enum Arg_Type {
    ARG_INVALID,
    ARG_INTEGER,
    ARG_BIGINT,
};

typedef struct Arg Arg;
struct Arg {
    enum Arg_Type type;
    union {
        lua_Integer integer;
        BigInt     *bigint;
    };
};

LUALIB_API int
luaopen_bigint(lua_State *L);

#endif /* BIGINT_H */

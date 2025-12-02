#ifndef BIGINT_H
#define BIGINT_H

#include <limits.h>
#include <lua.h>
#include <lauxlib.h>

#include "../common.h"

typedef uint32_t Digit;
typedef int64_t  Word;


/** @note max = base - 1 */
#define BIGINT_DIGIT_BASE   1000000000


/** @brief How many base-2 digits can fit in a single base-`BASE` digit?
 *  e.g. in base-1_000_000_000: 0b11_1011_1001_1010_1100_1001_1111_1111 */
#define BIGINT_DIGIT_BASE2_LENGTH   30


/** @brief How many base-8 digits can fit in a single base-`BASE` digit?
 *  e.g. in base-1_000_000_000: 0o7_346_544_777 */
#define BIGINT_DIGIT_BASE8_LENGTH   10

/** @brief How many base-10 digits can fit in a single base-`BASE` digit?
 *  e.g. in base-1_000_000_000: 0d999_999_999 */
#define BIGINT_DIGIT_BASE10_LENGTH  9


/** @brief How many base-16 digits can fit in a single base-`BASE` digit?
 *  e.g. in base-1_000_000_000: 0x3b9a_c9ff */
#define BIGINT_DIGIT_BASE16_LENGTH  8

#define BIGINT_MTNAME       "bigint.BigInt"

#define STUB(L, msg)    luaL_error(L, "%s:%d: %s", __FILE__, __LINE__, msg)

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
    size_t len;
    Sign   sign;
    Digit  digits[];
};

enum Arg_Type {
    ARG_INVALID,
    ARG_BIGINT,
    ARG_INTEGER,
    ARG_STRING,
};

typedef struct Arg Arg;
struct Arg {
    enum Arg_Type type;
    union {
        lua_Integer integer;
        BigInt     *bigint;
        struct {
            const char *data;
            size_t      len;
        } lstring;
    };
};

LUALIB_API int
luaopen_bigint(lua_State *L);

#endif /* BIGINT_H */

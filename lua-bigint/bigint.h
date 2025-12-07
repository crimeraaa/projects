#ifndef BIGINT_H
#define BIGINT_H

#include <lua.h>
#include <lauxlib.h>

#include "../common.h"


/** @brief Must be able to hold the range `[0, (BASE-1)*2]`. */
typedef uint32_t DIGIT;

/** @brief Must be able to hold the range `[0, (BASE-1)**2]`. */
typedef uint64_t WORD;


#define BIGINT_LIBNAME      "bigint"
#define BIGINT_TYPENAME     "BigInt"
#define BIGINT_MTNAME       (BIGINT_LIBNAME "." BIGINT_TYPENAME)

/** @brief How many unused bits in a `DIGIT`?
 *  Using 1 is tempting, but risks overflow in some comparisons. */
#define DIGIT_NAILS         2
#define DIGIT_TYPE_BITS     (CHAR_BIT * sizeof(DIGIT))
#define DIGIT_SHIFT         (DIGIT_TYPE_BITS - DIGIT_NAILS)
#define WORD_SHIFT          (DIGIT_SHIFT * 2)

/** @brief base-`2**DIGIT_SHIFT`. */
#define DIGIT_BASE          (1 << DIGIT_SHIFT)
#define DIGIT_MASK          (DIGIT_BASE - 1)
#define DIGIT_MAX           DIGIT_MASK

/** @brief Used to optimize conversion to base-10 strings. */
#define DIGIT_SHIFT_DECIMAL 9

/** @brief base-`10**DIGIT_SHIFT_DECIMAL`. */
#define DIGIT_BASE_DECIMAL  1000000000
#define STUB(L, msg)    luaL_error(L, "%s:%d: %s", __FILE__, __LINE__, msg)

enum Sign {
    POSITIVE,
    NEGATIVE,
};

enum Comparison {
    LESS    = -1,
    EQUAL   =  0,
    GREATER =  1,
};

typedef enum Sign Sign;
typedef enum Comparison Comparison;
typedef struct BigInt BigInt;

struct BigInt {
    size_t len;
    Sign sign;
    DIGIT digits[];
};

enum Arg_Type {
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

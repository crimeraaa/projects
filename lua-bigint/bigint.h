#ifndef BIGINT_H
#define BIGINT_H

#include <limits.h>
#include <lua.h>
#include <lauxlib.h>

#include "../common.h"


/** @brief Must be able to hold the range `[0, (BASE-1)*4]`. */
typedef uint32_t DIGIT;

/** @brief Must be able to hold the range `[0, (BASE-1)**2]`. */
typedef uint64_t WORD;


#define BIGINT_LIBNAME      "bigint"
#define BIGINT_TYPENAME     "BigInt"
#define BIGINT_MTNAME       (BIGINT_LIBNAME "." BIGINT_TYPENAME)

/** @brief How many unused bits in a `DIGIT`? */
#define DIGIT_NAILS         2
#define DIGIT_TYPE_BITS     (CHAR_BIT * sizeof(DIGIT))
#define DIGIT_BITS          (DIGIT_TYPE_BITS - DIGIT_NAILS)

#define WORD_BITS           60


/** @brief base-`2**BITS`.
* @note `max = base - 1`.
*/
#define DIGIT_BASE          (1 << DIGIT_BITS)

/** @brief Used to optimize conversion to base-10 strings. */
#define DIGIT_BASE_DECIMAL  1000000000
#define DIGIT_MASK          (DIGIT_BASE - 1)
#define DIGIT_MAX           DIGIT_MASK


/** @brief How many base-2 digits can fit in a single base-`BASE` digit?
 *  e.g. in base-2**30: 0b00111111_11111111_11111111_11111111 */
#define DIGIT_BASE2_LENGTH   DIGIT_BITS


/** @brief How many base-8 digits can fit in a single base-`BASE` digit?
 *  e.g. in base-2**30: 0o007_777_777_777 */
#define DIGIT_BASE8_LENGTH   10


/** @brief How many base-10 digits can fit in a single base-`BASE` digit?
 *  e.g. in base-2**30: 0d1_073_741_823 */
#define DIGIT_BASE10_LENGTH  10


/** @brief How many base-16 digits can fit in a single base-`BASE` digit?
 *  e.g. in base-2**30: 0x3fff_ffff */
#define DIGIT_BASE16_LENGTH  8

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
    int  len;
    Sign sign;
    DIGIT digits[];
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

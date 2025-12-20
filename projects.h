#ifndef PROJECTS_COMMON_H
#define PROJECTS_COMMON_H

#include <assert.h>     // assert
#include <limits.h>     // CHAR_BIT
#include <stdalign.h>   // alignof
#include <stdbool.h>    // bool, true, false
#include <stddef.h>     // NULL, size_t, ptrdiff_t, max_align_t
#include <stdint.h>     // u?int[\d]+_t, uintptr_t


#define COMMON__STRINGIFY(x)    #x
#define STRINGIFY(x)            COMMON__STRINGIFY(x)
#define FILE_LINE_STRING        __FILE__ ":" STRINGIFY(__LINE__)

#define cast(T)             (T)
#define unused(expr)        ((void)(expr))
#define count_of(array)     (sizeof(array) / sizeof((array)[0]))

#define printfln(fmt, ...)  printf(fmt "\n", __VA_ARGS__)
#define println(s)          printfln("%s", s)

#define eprintf(fmt, ...)   fprintf(stderr, fmt, __VA_ARGS__)
#define eprintfln(fmt, ...) eprintf(fmt "\n", __VA_ARGS__)
#define eprintln(s)         eprintfln("%s", s)

#define stub() \
    eprintfln("%s:%i: Unimplemented\n", __FILE__, __LINE__); \
    __builtin_trap()


#define TYPE_BITS(T)        (sizeof(T) * CHAR_BIT)

typedef unsigned char uchar;
typedef unsigned int  uint;

typedef uint8_t     u8;
typedef uint16_t    u16;
typedef uint32_t    u32;
typedef uint64_t    u64;

#define U8_MAX      UINT8_MAX
#define U16_MAX     UINT16_MAX
#define U32_MAX     UINT32_MAX
#define U64_MAX     UINT64_MAX

typedef int8_t      i8;
typedef int16_t     i16;
typedef int32_t     i32;
typedef int64_t     i64;

#define I8_MAX      INT8_MAX
#define I8_MIN      INT8_MIN
#define I16_MAX     INT16_MAX
#define I16_MIN     INT16_MIN
#define I32_MAX     INT32_MAX
#define I32_MIN     INT32_MIN
#define I64_MAX     INT64_MAX
#define I64_MIN     INT64_MIN


/* Compiler-specific: GCC or GCC-like */
#if defined(__GNUC__)
#   define trap()           __builtin_trap()
/*  If C23, we already have this in <stddef.h>. */
#   ifndef unreachable
#   define unreachable()    __builtin_unreachable()
#   endif

/* Compiler-specific: MSVC */
#elif defined(_MSC_VER)
#   define trap()          __debugbreak()
/*  If C23, we already have this in <stddef.h> */
#   ifndef unreachable
#   define unreachable()   __assume(0)
#   endif
#endif /* __GNUC__, _MSC_VER */


/* Fallbacks */
#ifndef unreachable
#   if __STDC_VERSION__ >= 201703L

[[noreturn]]
static inline void
unreachable() {}

#   elif __STDC_VERSION__ >= 201112L

_Noreturn
static inline void
unreachable(void) {}

#   else
#   error  Cannot define `unreachable`.
#   endif
#endif /* unreachable */

#ifndef NDEBUG
#define assertfln(expr, fmt, ...)                                              \
do {                                                                           \
    if (!(expr)) {                                                             \
        eprintfln(FILE_LINE_STRING " Assertion '" #expr "' failed (" fmt ")",  \
                __VA_ARGS__);                                                  \
        trap();                                                                \
    }                                                                          \
} while (0)
#else /* !NDEBUG */
#define assertfln(expr, fmt, ...)       ((void)0)
#endif /* NDEBUG */

#define assertln(expr, msg) assertfln(expr, "%s", msg)

/* Semantics for C99 compound literals are different in C++. */
#ifdef __cplusplus
#define COMPOUND_LITERAL(T)    T
#else
#define COMPOUND_LITERAL(T)   (T)
#endif

#endif /* PROJECTS_COMMON_H */

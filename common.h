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


#if defined(__GNUC__)
#   define trap()           __builtin_trap()
/*  If C23, we already have this in <stddef.h>. */
#   ifndef unreachable
#   define unreachable()    __builtin_unreachable()
#   endif
#elif defined(_MSC_VER)
#   define trap()          __debugbreak()
/*  If C23, we already have this in <stddef.h> */
#   ifndef unreachable
#   define unreachable()   __assume(0)
#   endif
#endif /* __GNUC__, _MSC_VER */


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
        eprintfln(FILE_LINE_STRING " Assertion '%s' failed (" fmt ")",         \
                #expr, __VA_ARGS__);                                           \
        trap();                                                                \
    }                                                                          \
} while (0)
#else
#define assertfln(expr, fmt, ...)       ((void)0)
#endif /* NDEBUG */

#define assertln(expr, msg) assertfln(expr, "%s", msg)


#endif /* PROJECTS_COMMON_H */

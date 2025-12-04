#ifndef PROJECTS_COMMON_H
#define PROJECTS_COMMON_H

#include <assert.h>     // assert
#include <limits.h>     // CHAR_BIT
#include <stdalign.h>   // alignof
#include <stdbool.h>    // bool, true, false
#include <stddef.h>     // NULL, size_t, ptrdiff_t, max_align_t
#include <stdint.h>     // u?int[\d]+_t, uintptr_t

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

#endif /* PROJECTS_COMMON_H */

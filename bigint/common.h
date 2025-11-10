#ifndef BIGINT_COMMON_H
#define BIGINT_COMMON_H

#include <assert.h>
#include <stddef.h>

#define cast(T)         (T)
#define unused(expr)    ((void)(expr))

#define eprintf(fmt, ...)   fprintf(stderr, fmt, __VA_ARGS__)
#define eprintfln(fmt, ...) eprintf(fmt "\n", __VA_ARGS__)
#define eprintln(s)         eprintfln("%s", s)


#define stub() \
    eprintfln("%s:%i: Unimplemented\n", __FILE__, __LINE__); \
    __builtin_trap()


#endif /* BIGINT_COMMON_H */

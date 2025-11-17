#ifndef PROJECTS_COMMON_H
#define PROJECTS_COMMON_H

#include <assert.h> // assert
#include <stdalign.h> // alignof
#include <stdbool.h> // bool
#include <stddef.h> // size_t, NULL
#include <stdint.h> // uinptr_t

#define cast(T)         (T)
#define unused(expr)    ((void)(expr))

#define eprintf(fmt, ...)   fprintf(stderr, fmt, __VA_ARGS__)
#define eprintfln(fmt, ...) eprintf(fmt "\n", __VA_ARGS__)
#define eprintln(s)         eprintfln("%s", s)


#define stub() \
    eprintfln("%s:%i: Unimplemented\n", __FILE__, __LINE__); \
    __builtin_trap()


#endif /* PROJECTS_COMMON_H */

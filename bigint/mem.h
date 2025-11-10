#ifndef BIGINT_MEM_H
#define BIGINT_MEM_H

#include <stddef.h> // size_t

typedef void *
(*Allocator_Fn)(void *ptr, size_t old_size, size_t new_size, void *context);

typedef struct {
    Allocator_Fn fn;
    void        *context;
} Allocator;

/**
 * @param o size_t  "Old size" or the number of bytes originally allocated.
 * @param n size_t  "New size" or the number of bytes to be reallocated.
 */
#define mem_rawresize(ptr, o, n, a) (a)->fn(ptr, o, n, (a)->context)
#define mem_rawmake(n, a)           mem_rawresize(NULL, 0, n, a)
#define mem_rawfree(ptr, size, a)   mem_rawresize(ptr, size, 0 ,a)


/**
 * @param T <type>
 * @param n <integral>          The number of `T` you wish to allocate.
 * @param a const Allocator *
 */
#define mem_make(T, n, a)           (T *)mem_rawmake(sizeof(T) * (n), a)


/**
 * @param T     <type>
 * @param ptr   T *
 * @param o     <integral>          The current number of `T` in `ptr`.
 * @param n     <integral>          The new number of `T` that `ptr` will use.
 * @param a     const Allocator *
 */
#define mem_resize(T, ptr, o, n, a) (T *)mem_rawresize(ptr, sizeof(T) * (o), sizeof(T) * (n), a)


/**
 * @param ptr   <type> *
 * @param n     <integral>          The number of `<type>` in `ptr`.
 * @param a     const Allocator *
 */
#define mem_free(ptr, n, a)         mem_rawfree(ptr, sizeof((ptr)[0]) * (n), a)

#endif /* BIGINT_MEM_H */

#ifndef STRING_BUILDER_H
#define STRING_BUILDER_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} String_Builder;


#endif /* STRING_BUILDER_H */

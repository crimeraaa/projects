#ifndef BIGINT_VALUE_H
#define BIGINT_VALUE_H

// #include "bigint.h"
#include "i128.h"

typedef enum {
    VALUE_BOOLEAN,
    VALUE_INTEGER,
} Value_Type;

typedef struct Value Value;
struct Value {
    Value_Type type;
    union {
        // BigInt *integer;
        i128 integer;
        bool boolean;
    };
};

#define value_type(v)       ((v).type)
#define value_is_boolean(v) (value_type(v)== VALUE_BOOLEAN)
#define value_is_integer(v) (value_type(v) == VALUE_INTEGER)

#endif /* BIGINT_VALUE_H */

#ifndef BIGINT_VALUE_H
#define BIGINT_VALUE_H

#include "bigint.h"

typedef enum {
    VALUE_BOOLEAN,
    VALUE_INTEGER,
} Value_Type;

typedef struct {
    Value_Type type;
    union {
        BigInt *integer;
        bool    boolean;
    };
} Value;

#define value_type(v)       ((v).type)
#define value_is_boolean(v) (value_type(v)== VALUE_BOOLEAN)
#define value_is_integer(v) (value_type(v) == VALUE_INTEGER)

#endif /* BIGINT_VALUE_H */

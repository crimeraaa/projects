#+private package
package lulu

import "base:builtin"
import "core:fmt"

/*
If `cond` is false, then panics and reports the error. This is a procedure
group to help overload depending on if custom messages are desired or not.
 */
assert :: proc {
    builtin.assert,
    fmt.assertf,
}

panic :: fmt.panicf

unreachable :: proc(msg := "", args: ..any, loc := #caller_location) -> ! {
    when ODIN_DEBUG {
        if msg == "" {
            panic("Runtime panic", loc=loc)
       } else {
            fmt.panicf(msg, ..args, loc=loc)
       }
    } else {
        builtin.unreachable()
    }
}

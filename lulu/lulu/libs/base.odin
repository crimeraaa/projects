#+private file
package lulu_libs

// standard
import "core:fmt"
import "core:strconv"

// local
import lulu ".."
import lulu_aux "../aux"

@(private="package", rodata)
base_procs := [?]lulu_aux.Library_Entry{
    {"assert",      __assert},
    {"error",       __error},
    {"print",       __print},
    {"tostring",    __to_string},
    {"tonumber",    __to_number},
    // {"sequence",    __sequence},
    // {"tuple",       __tuple},
    {"type",        __type},
}

__assert :: proc(L: ^lulu.State) -> (ret_count: int) {
    arg_count := lulu.get_top(L)
    condition := lulu.to_boolean(L, 1)
    if !condition {
        message := lulu_aux.opt_string(L, 2, "assertion failed!")
        lulu_aux.errorf(L, "%s", message)
    }
    return arg_count
}

__error :: proc(L: ^lulu.State) -> int {
    message := lulu_aux.check_string(L, 1)
    lulu_aux.errorf(L, "%s", message)
    // return 0
}

__print :: proc(L: ^lulu.State) -> (ret_count: int) {
    arg_count := lulu.get_top(L)
    lulu.get_global(L, "tostring")
    for i in 1..=arg_count {
        if i > 1 {
            fmt.print("\t", flush=false)
        }
        lulu.push_value(L, -1)
        lulu.push_value(L, i)
        lulu.call(L, 1, 1)
        fmt.print(lulu.to_string(L, -1), flush=false)
        lulu.pop(L, 1)
    }
    fmt.println()
    return 0
}

__to_string :: proc(L: ^lulu.State) -> (ret_count: int) {
    i := 1

    lulu_aux.check_any(L, i)
    t := lulu.type(L, i)
    #partial switch t {
    case .Nil:     lulu.push(L, "nil")
    case .Boolean: lulu.push(L, "true" if lulu.to_boolean(L, i) else "false")
    case .Number, .String:
        // Convert stack slot in-place
        lulu.to_string(L, i)
    case:
        ts := lulu_aux.type_name_at(L, i)
        p  := lulu.to_pointer(L, i)
        lulu.push_fstring(L, "%s: %p", ts, p)
    }
    return 1
}

__to_number :: proc(L: ^lulu.State) -> (ret_count: int) {
    lulu_aux.check_any(L, 1)

    base := int(lulu_aux.opt_number(L, 2, default=10))
    lulu_aux.arg_check(L, 2 <= base && base <= 36, index=2)

    parse_number: if base == 10 {
        lulu.push(L, lulu.to_number(L, 1) or_break parse_number)
        return 1
    } else {
        type := lulu.type(L, 1)
        #partial switch type {
        case .Number, .String:
            // Re-parse as an unsigned integer in given base.
            s    := lulu.to_string(L, 1)
            sign := f64(1)

            // Have a sign with at least 1 digit?
            if len(s) > 1 {
                switch s[0] {
                case '+':
                    s = s[1:]
                case '-':
                    sign = -1
                    s = s[1:]
                }
            }

            // Have an integer prefix with at least 1 digit?
            parse_prefix: if len(s) > 2 && s[0] == '0' {
                prefix := 0
                switch s[1] {
                case 'b', 'B': prefix = 2
                case 'd', 'D': prefix = 10
                case 'o', 'O': prefix = 8
                case 'x', 'X': prefix = 16
                case 'z', 'Z': prefix = 12
                case:
                    break parse_prefix
                }

                // Check consistency if given an explicit prefix.
                if base != prefix {
                    break parse_number
                }
                s = s[2:]
            }
            n := strconv.parse_uint(s, base=base) or_break parse_number
            lulu.push(L, f64(n) * sign)
            return 1
        case:
            break
        }
    }
    lulu.push_nil(L)
    return 1
}

// __sequence :: proc(L: ^lulu.State) -> (ret_count: int) {
//     start := int(lulu_aux.check_number(L, 1))
//     stop  := int(lulu_aux.check_number(L, 2))
//     step  := int(lulu_aux.opt_number(L, index=3, default=1))

//     for i := start; i < stop; i += step {
//         lulu.push(L, f64(i))
//         ret_count += 1
//     }
//     return ret_count
// }

// __tuple :: proc(L: ^lulu.State) -> (ret_count: int) {
//     return lulu.get_top(L)
// }

__type :: proc(L: ^lulu.State) -> (ret_count: int) {
    lulu_aux.check_any(L, 1)
    type_name := lulu_aux.type_name_at(L, 1)
    lulu.push(L, type_name)
    return 1
}

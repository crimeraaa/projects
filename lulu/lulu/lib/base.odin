#+private file
package lulu_lib

// standard
import "core:fmt"

// local
import lulu ".."
import lulu_aux "../aux"

@(private="package", rodata)
base_procs := [?]lulu_aux.Library_Entry{
    {"print",    _print},
    {"tostring", _tostring},
    {"assert",   _assert},
    {"error",    _error},
}

_assert :: proc(L: ^lulu.State) -> int {
    arg_count := lulu.get_top(L)
    condition := lulu.to_boolean(L, 1)
    if !condition {
        message := lulu_aux.opt_string(L, 2, "assertion failed!")
        lulu_aux.errorf(L, "%s", message)
    }
    return max(arg_count - 2, 0)
}

_error :: proc(L: ^lulu.State) -> int {
    message := lulu_aux.check_string(L, 1)
    lulu_aux.errorf(L, "%s", message)
    // return 0
}

_print :: proc(L: ^lulu.State) -> (ret_count: int) {
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

_tostring :: proc(L: ^lulu.State) -> (ret_count: int) {
    i := 1

    lulu_aux.check_any(L, i)
    t := lulu.type(L, i)
    #partial switch t {
    case .Nil:     lulu.push_string(L, "nil")
    case .Boolean: lulu.push_string(L, "true" if lulu.to_boolean(L, i) else "false")
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

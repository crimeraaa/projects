#+private file
package lulu_libs

// standard
import "core:math"

// local
import lulu ".."
import lulu_aux "../aux"

@(private="package", rodata)
math_procs := [?]lulu_aux.Library_Entry{
    {"abs",     _abs},
    {"ceil",    _ceil},
    {"floor",   _floor},
    {"log",     _log},
    {"log2",    _log2},
    {"log10",   _log10},
    {"modf",    _modf},
    {"sqrt",    _sqrt},
}

_arg1_ret1 :: #force_inline proc(L: ^lulu.State, $procedure: $T) -> int {
    n := lulu_aux.check_number(L, 1)
    lulu.push(L, procedure(n))
    return 1
}

_arg1_ret2 :: #force_inline proc(L: ^lulu.State, $procedure: $T) -> int {
    n := lulu_aux.check_number(L, 1)
    a, b := procedure(n)
    lulu.push(L, a)
    lulu.push(L, b)
    return 2
}

_arg2_ret1 :: #force_inline proc(L: ^lulu.State, $procedure: $T) -> int {
    a := lulu_aux.check_number(L, 1)
    b := lulu_aux.check_number(L, 2)
    lulu.push(L, procedure(a, b))
    return 1
}

_abs_f64 :: proc(n: f64) -> f64 { return math.abs(n) }

_abs   :: proc(L: ^lulu.State) -> int { return _arg1_ret1(L, _abs_f64)       }
_ceil  :: proc(L: ^lulu.State) -> int { return _arg1_ret1(L, math.ceil_f64)  }
_floor :: proc(L: ^lulu.State) -> int { return _arg1_ret1(L, math.floor_f64) }
_log   :: proc(L: ^lulu.State) -> int { return _arg2_ret1(L, math.log_f64)   }
_log2  :: proc(L: ^lulu.State) -> int { return _arg1_ret1(L, math.log2_f64)  }
_log10 :: proc(L: ^lulu.State) -> int { return _arg1_ret1(L, math.log10_f64) }
_modf  :: proc(L: ^lulu.State) -> int { return _arg1_ret2(L, math.modf_f64)  }
_sqrt  :: proc(L: ^lulu.State) -> int { return _arg1_ret1(L, math.sqrt_f64)  }


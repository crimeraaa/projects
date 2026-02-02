package lulu_libs

// local
import lulu ".."
import lulu_aux "../aux"

open :: proc(L: ^lulu.State) {
    open_base(L)
    open_math(L)
    open_string(L)
}

open_base :: proc(L: ^lulu.State) {
    lulu.push_value(L, lulu.GLOBALS_INDEX)
    lulu.push_value(L, -1)   // _G = _G
    lulu.set_global(L, "_G")
    lulu_aux.set_library(L, base_procs[:])
}

open_math :: proc(L: ^lulu.State) {
    lulu_aux.new_library(L, math_procs[:])
    lulu.set_global(L, "math")
}

open_string :: proc(L: ^lulu.State) {
    lulu_aux.new_library(L, string_procs[:])
    lulu.set_global(L, "string")
}

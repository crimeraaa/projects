package lulu_libs

// local
import lulu ".."
import lulu_aux "../aux"

@(private="package", rodata)
libs := [?]lulu_aux.Library_Entry{
    {"_G",      open_base},
    {"math",    open_math},
    // {"string",  open_string},
    // {"utf8",    open_utf8},
}

open :: proc(L: ^lulu.State) {
    L.global_state.gc_state = .Paused
    for lib in libs[:] {
        lulu.push_api_proc(L, lib.procedure)
        lulu.call(L, arg_count=0, ret_count=1)
        lulu.set_global(L, lib.name)
    }
    L.global_state.gc_state = .None
}

open_base :: proc(L: ^lulu.State) -> (ret_count: int) {
    lulu.push_value(L, lulu.GLOBALS_INDEX)
    lulu_aux.set_library(L, base_procs[:])
    return 1
}

open_math :: proc(L: ^lulu.State) -> (ret_count: int) {
    lulu_aux.new_library(L, math_procs[:])
    return 1
}

open_string :: proc(L: ^lulu.State) -> (ret_count: int) {
    lulu_aux.new_library(L, string_procs[:])
    return 1
}

open_utf8 :: proc(L: ^lulu.State) -> (ret_count: int) {
    lulu_aux.new_library(L, utf8_procs[:])
    return 1
}

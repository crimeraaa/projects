package lulu_repl

import "core:fmt"
import os "core:os/os2"
import "core:mem"

import "lulu"

_ :: mem

main :: proc() {
    when ODIN_DEBUG {
        ta: mem.Tracking_Allocator
        mem.tracking_allocator_init(&ta, context.allocator)
        context.allocator = mem.tracking_allocator(&ta)
        defer {
            if len(ta.bad_free_array) > 0 {
                fmt.eprintln("bad frees:")
                for info, i in ta.bad_free_array {
                    ptr  := info.memory
                    file := info.location.file_path
                    line := info.location.line
                    func := info.location.procedure
                    fmt.eprintfln("%s:%i: %p in %q", file, line, ptr, func)
                }
            }

            if len(ta.allocation_map) > 0 {
                fmt.eprintln("memory leaks: ")
                for ptr, info in ta.allocation_map {
                    file := info.location.file_path
                    line := info.location.line
                    func := info.location.procedure
                    fmt.eprintfln("%s:%i: %p in %q", file, line, ptr, func)
                }
            }
            mem.tracking_allocator_destroy(&ta)
        }
    }

    L, ok := lulu.new_state()
    defer lulu.close(L)

    switch len(os.args) {
    case 1: run_repl(L)
    case 2:
        err := run_file(L, os.args[1])
        if err != nil {
            fmt.eprintln("[ERROR]:", os.error_string(err))
        }
    }
    fmt.println(L.global_state.bytes_allocated, "bytes remaining")
}

run_repl :: proc(L: ^lulu.State) {
    for {
        fmt.print(">>> ")
        line_buf: [512]byte
        line_read, read_err := os.read(os.stdin, line_buf[:])
        if read_err != nil {
            if read_err == .EOF {
                fmt.println()
            } else {
                fmt.println("[ERROR]:", os.error_string(read_err))
            }
            break
        }
        // Skip the newline stored at `line_buf[line_read]`.
        run_input(L, "stdin", string(line_buf[:line_read - 1]))
    }
}

run_file :: proc(L: ^lulu.State, name: string) -> (err: os.Error) {
    buf := os.read_entire_file(name, context.allocator) or_return
    defer delete(buf)
    run_input(L, name, string(buf))
    return nil
}

run_input :: proc(L: ^lulu.State, name, input: string) {
    err := lulu.load(L, name, input)
    if err == nil {
        err = lulu.pcall(L, arg_count=0, ret_count=0)
    }

    if err != nil {
        fmt.println(lulu.to_string(L, -1))
        lulu.pop(L, 1)
    }
}

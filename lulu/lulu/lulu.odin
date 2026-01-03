package lulu

import "core:fmt"
import "core:strings"
import os "core:os/os2"
import "core:mem"

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
                    fmt.eprintfln("[%i] %p @ %s:%i (in '%s')", i, info, file, line, func)
                }
            }

            if len(ta.allocation_map) > 0 {
                fmt.eprintln("memory leaks: ")
                for ptr, info in ta.allocation_map {
                    file := info.location.file_path
                    line := info.location.line
                    func := info.location.procedure
                    fmt.eprintfln("%p @ %s:%i (in '%s')", ptr, file, line, func)
                }
            }
            mem.tracking_allocator_destroy(&ta)
        }
    }

    g := &Global_State{}
    L := &VM{global_state=g}
    vm_init(L)
    defer vm_destroy(L)

    switch len(os.args) {
    case 1: run_repl(L)
    case 2:
        err := run_file(L, os.args[1])
        if err != nil {
            fmt.eprintln("[ERROR]:", os.error_string(err))
        }
    }
}

run_repl :: proc(L: ^VM) {
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

run_file :: proc(L: ^VM, name: string) -> (err: os.Error) {
    buf := os.read_entire_file(name, context.allocator) or_return
    defer delete(buf)
    run_input(L, name, string(buf))
    return nil
}

run_input :: proc(L: ^VM, name, input: string) {
    // Must be outside protected call to ensure that we can defer destroy.
    b, _ := strings.builder_make(allocator=context.allocator)
    defer strings.builder_destroy(&b)

    Data :: struct {
        builder:    ^strings.Builder,
        name, input: string,
    }

    parse :: proc(L: ^VM, ud: rawptr) {
        data  := (cast(^Data)ud)^
        name  := ostring_new(L, data.name)
        chunk := chunk_new(L, name)
        p := parser_make(L, data.builder, name, data.input)
        c := compiler_make(L, &p, chunk)
        program(&p, &c)
        vm_execute(L, chunk)
    }
    vm_run_protected(L, parse, &Data{&b, name, input})
}

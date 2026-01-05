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
    defer {
        lulu.close(L)
        fmt.println(L.global_state.bytes_allocated, "bytes remaining")
    }
    data := Pmain_Data{os.args, 0}
    err  := lulu.api_pcall(L, pmain, &data)

    if err != nil {
        os.exit(cast(int)err)
    }

    if data.code != 0 {
        os.exit(data.code)
    }
}

Pmain_Data :: struct {
    args: []string,
    code: int,
}

pmain :: proc(L: ^lulu.State) -> (ret_count: int) {
    data := cast(^Pmain_Data)lulu.to_userdata(L, 1)
    lulu.push_value(L, lulu.GLOBALS_INDEX)
    lulu.set_global(L, "_G")

    lulu.push_api_proc(L, print)
    lulu.set_global(L, "print")

    switch len(data.args) {
    case 1:
        run_repl(L)

    case 2:
        err := run_file(L, data.args[1])
        if err != nil {
            fmt.eprintln("[ERROR]:", os.error_string(err))
        }
        data.code = 1
    }
    return 0
}

print :: proc(L: ^lulu.State) -> (ret_count: int) {
    arg_count := lulu.get_top(L)
    for i in 1..=arg_count {
        if i > 1 {
            fmt.print("\t")
        }
        t := lulu.type(L, i)
        #partial switch t {
        case .Nil:     fmt.print("nil")
        case .Boolean: fmt.print("true" if lulu.to_boolean(L, i) else "false")
        case .Number:  fmt.print(lulu.to_number(L, i))
        case .String:  fmt.print(lulu.to_string(L, i))
        case:
            ts := lulu.type_name_at(L, i)
            p  := lulu.to_pointer(L, i)
            fmt.printf("%s: %p", ts, p)
        }
    }
    fmt.println()
    return 0
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

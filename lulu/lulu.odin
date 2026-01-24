package lulu_repl

import "core:fmt"
import os "core:os/os2"
import "core:math"
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

    ms: lulu.Main_State
    L, ok := lulu.new_state(&ms, context.allocator)
    defer {
        lulu.close(L)
        fmt.println(L.global_state.bytes_allocated, "bytes remaining")
    }

    Data :: struct {
        args: []string,
        err: os.Error,
    }

    data := Data{os.args, nil}
    err  := lulu.api_pcall(L, proc(L: ^lulu.State) -> (ret_count: int) {
        data := cast(^Data)lulu.to_userdata(L, 1)
        open_base(L)

        switch len(data.args) {
        case 1: data.err = run_repl(L)
        case 2: data.err = run_file(L, data.args[1])
        case:
            fmt.eprintfln("Usage: %s [script]", data.args[0])
            data.err = .Invalid_Command
        }

        if data.err != nil {
            if data.err == .EOF {
                data.err = nil
            } else {
                fmt.eprintln("[ERROR]:", os.error_string(data.err))
            }
        }
        return 0
    }, &data)

    if err != nil {
        os.exit(int(err))
    }

    if data.err != nil {
        os.exit(1)
    }
}

open_base :: proc(L: ^lulu.State) {
    lulu.push_value(L, lulu.GLOBALS_INDEX)
    lulu.push_value(L, -1)
    lulu.set_global(L, "_G")
    
    @(static, rodata)
    lib_base := [?]lulu.Named_Proc{
        {"print",    print},
        {"tostring", tostring},
        {"modf",     modf},
    }
    
    lulu.set_library(L, lib_base[:])
}

print :: proc(L: ^lulu.State) -> (ret_count: int) {
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

tostring :: proc(L: ^lulu.State) -> (ret_count: int) {
    i := 1
    t := lulu.type(L, i)
    #partial switch t {
    case .Nil:     lulu.push_string(L, "nil")
    case .Boolean: lulu.push_string(L, "true" if lulu.to_boolean(L, i) else "false")
    case .Number, .String:
        // Convert stack slot in-place
        lulu.to_string(L, i)
    case:
        ts := lulu.type_name_at(L, i)
        p  := lulu.to_pointer(L, i)
        lulu.push_fstring(L, "%s: %p", ts, p)
    }
    return 1
}

modf :: proc(L: ^lulu.State) -> (ret_count: int) {
    if number, ok := lulu.to_number(L, 1); ok {
        integer, fraction  := math.modf(number)
        lulu.push_number(L, integer)
        lulu.push_number(L, fraction)
        // lulu.push_boolean(L, true)
        return 2

        // mantissa, exponent := math.frexp(number)
        // lulu.push_number(L, mantissa)
        // lulu.push_number(L, f64(exponent))
        // return 4
    }
    return 0
}

run_repl :: proc(L: ^lulu.State) -> (err: os.Error) {
    for {
        fmt.print(">>> ")
        line_buf: [512]byte
        line_read := os.read(os.stdin, line_buf[:]) or_return
        // Skip the newline stored at `line_buf[line_read]`.
        run_input(L, "stdin", string(line_buf[:line_read - 1]))
    }
    unreachable()
}

run_file :: proc(L: ^lulu.State, name: string) -> (err: os.Error) {
    buf := os.read_entire_file(name, context.allocator) or_return
    run_input(L, name, string(buf))
    delete(buf)
    return nil
}

run_input :: proc(L: ^lulu.State, name, input: string) {
    lulu.set_top(L, 0)
    err := lulu.load(L, name, input)
    if err == nil {
        err = lulu.pcall(L, arg_count=0, ret_count=lulu.VARIADIC)
    }

    if err != nil {
        fmt.println(lulu.to_string(L, -1))
        lulu.pop(L, 1)
    } else {
        if n := lulu.get_top(L); n > 0 {
            fmt.printf("'%s' returned: ", name, flush=false)
            lulu.get_global(L, "print")
            lulu.insert(L, 1)
            lulu.call(L, arg_count=n, ret_count=0)
        }
    }
    // assert(lulu.get_top(L) == 0)
}

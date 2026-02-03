package lulu_repl

// standard
import "core:fmt"
import os "core:os/os2"
import "core:mem"

// local
import "lulu"
import lulu_aux "lulu/aux"
import lulu_lib "lulu/libs"

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
        lulu_lib.open(L)
        lulu.set_top(L, 0)

        // Test if lexer can properly handle token stream memory errors.
        backing: [mem.DEFAULT_PAGE_SIZE]byte
        arena: mem.Arena
        mem.arena_init(&arena, backing[:])

        allocator := mem.arena_allocator(&arena)
        switch len(data.args) {
        case 1: data.err = run_repl(L, allocator)
        case 2: data.err = run_file(L, data.args[1], allocator)
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

run_repl :: proc(L: ^lulu.State, allocator := context.allocator) -> (err: os.Error) {
    for {
        fmt.print(">>> ")
        line_buf: [512]byte
        line_read := os.read(os.stdin, line_buf[:]) or_return
        // Skip the newline stored at `line_buf[line_read]`.
        line     := string(line_buf[:line_read - 1])
        load_err := lulu_aux.load(L, "stdin", line, allocator)
        check_no_error(L, load_err) or_continue
        run_input(L, "stdin")
    }
    unreachable()
}


run_file :: proc(L: ^lulu.State, name: string, allocator := context.allocator) -> (err: os.Error) {
    load_err := lulu_aux.load(L, name, allocator)
    if check_no_error(L, load_err) {
        run_input(L, name)
    }
    return nil
}

check_no_error :: proc(L: ^lulu.State, err: lulu.Error) -> (ok: bool) {
    if err != nil {
        fmt.println(lulu.to_string(L, -1))
        lulu.pop(L, 1)
        return false
    }
    return true
}

run_input :: proc(L: ^lulu.State, name: string) {
    call_err := lulu.pcall(L, arg_count=0, ret_count=lulu.VARIADIC)
    if check_no_error(L, call_err) {
        if n := lulu.get_top(L); n > 0 {
            fmt.printf("'%s' returned: ", name, flush=false)
            lulu.get_global(L, "print")
            lulu.insert(L, 1)
            lulu.call(L, arg_count=n, ret_count=0)
        }
    } else {
        // Pop main chunk as well.
        lulu.pop(L, 1)
    }
    assert(lulu.get_top(L) == 0)
}

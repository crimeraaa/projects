package lulu

import "core:fmt"
import "core:strings"
import os "core:os/os2"

// token_formatter :: proc(fi: ^fmt.Info, arg: any, verb: rune) -> bool {
//     t := (cast(^Token)arg.data)^
//     q := '\'' if len(t.lexeme) == 1 else '\"'
//     fi.n += fmt.wprintf(fi.writer, "%s %c%s%c", t.type, q, t.lexeme, q)
//     return true
// }

main :: proc() {
    // m: map[typeid]fmt.User_Formatter
    // fmt.set_user_formatters(&m)
    // fmt.register_user_formatter(Token, token_formatter)
    // defer delete(m)

    g := &Global_State{}
    L := &VM{global=g}
    // lulu/cpp/src/vm.cpp:lulu_open()
    {
        // Ensure that, when we start interning strings, we already have
        // valid indexes.
        intern_resize(L, &g.intern, 32)
        o      := ostring_new(L, "out of memory")
        o.mark += {.Fixed}

        for kw_type in Token_Type.And..=Token_Type.While {
            o       = ostring_new(L, TOKEN_TYPE_STRINGS[kw_type])
            o.mark += {.Fixed}
        }
    }
    defer intern_destroy(L, &g.intern)

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
        name, input: string
    }

    data := Data{builder=&b, name=name, input=input}
    lex :: proc(L: ^VM, ud: rawptr) {
        data := (cast(^Data)ud)^
        x    := lexer_make(L, data.builder, data.name, data.input)
        line := -1
        fmt.println("[FILE]:", x.name)
        for {
            t := lexer_scan_token(&x)
            if t.type == .EOF {
                break
            }
            if t.line != line {
                fmt.printf("% -4i ", t.line)
                line = t.line
            } else {
                fmt.print("|--- ")
            }
            q := '\'' if len(t.lexeme) == 1 else '\"'
            fmt.printf("%-16v %c%s%c ", t.type, q, t.lexeme, q)
            switch v in t.data {
            case nil:
                fmt.println()
            case f64:
                fmt.println(v)
            case ^OString:
                fmt.printfln("%q", ostring_to_string(v))
            }
        }
    }
    vm_run_protected(L, lex, &data)
}

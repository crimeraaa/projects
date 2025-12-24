package lulu

import "core:c/libc"
import "core:fmt"
import os "core:os/os2"

token_formatter :: proc(fi: ^fmt.Info, arg: any, verb: rune) -> bool {
    t := (cast(^Token)arg.data)^
    q := '\'' if len(t.lexeme) == 1 else '\"'
    fi.n += fmt.wprintf(fi.writer, "%s %c%s%c", t.type, q, t.lexeme, q)
    return true
}

main :: proc() {
    line_buf: [256]byte
    m: map[typeid]fmt.User_Formatter
    fmt.set_user_formatters(&m)
    fmt.register_user_formatter(Token, token_formatter)
    defer delete(m)

    for {
        fmt.print(">>> ")
        line_read, read_err := os.read(os.stdin, line_buf[:])
        if read_err != nil {
            if read_err == .EOF {
                fmt.println()
            }
            break
        }
        handler: libc.jmp_buf
        x := lexer_make(string(line_buf[:line_read - 1]), &handler)
        if libc.setjmp(&handler) == 0 {
            for {
                t := lexer_lex(&x)
                if t.type == .EOF {
                    break
                }
                fmt.println(t)
            }
        }
    }
}

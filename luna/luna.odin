package luna

import "core:fmt"
import os "core:os/os2"
import "core:strings"

line_reader :: proc(user_data: rawptr) -> []byte {
    input := cast(^[]byte)user_data
    res   := input^
    if res != nil {
        input^ = nil
    }
    return res
}

main :: proc() {
    for {
        buf: [256]byte

        fmt.print(">>> ")
        n, err := os.read(os.stdin, buf[:])
        if err != nil {
            if err != .EOF {
                fmt.print("[ERROR] --- ", err)
            }
            fmt.println()
            break
        }
        input := buf[:n]
        b     := strings.builder_make()
        defer strings.builder_destroy(&b)

        r := reader_make(line_reader, &input)
        x := lexer_make(r, &b)

        line_count: i32
        for {
            t := lexer_lex(&x)
            if t.type == .EOF {
                break
            }

            if line_count != t.line {
                line_count = t.line
                fmt.printf("% -4i ", line_count)
            } else {
                fmt.print("|--- ")
            }
            fmt.printfln("%v(%q)", t.type, t.lexeme)
        }
    }

    // chunk: Chunk
    // stack: [16]Value
    // defer chunk_destroy(&chunk)

    // code_ABx(&chunk, .Load_Const, 0, chunk_add_constant(&chunk, 1))
    // code_ABx(&chunk, .Load_Const, 1, chunk_add_constant(&chunk, 2))
    // code_ABx(&chunk, .Load_Const, 2, chunk_add_constant(&chunk, 3))
    // code_ABC(&chunk, .Mul_int, 1, 1, 2)
    // code_ABC(&chunk, .Add_int, 0, 0, 1)
    // code_ABC(&chunk, .Return, 0, 1, 0)
    // vm_execute(stack[:], &chunk)
}

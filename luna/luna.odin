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
    L := &State{}
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
        load(L, "stdin", line_reader, &input, context.allocator)
    }
}

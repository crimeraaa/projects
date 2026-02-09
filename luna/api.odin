package luna

import "core:mem"
import "core:strings"

load :: proc(L: ^State, name: string, reader: Reader_Proc, user_data: rawptr, allocator: mem.Allocator) -> Error {
    Data :: struct {
        name:     string,
        reader:   Reader,
        builder: ^strings.Builder,
    }

    b := strings.builder_make(allocator)
    defer strings.builder_destroy(&b)

    r    := reader_make(reader, user_data)
    data := Data{name, r, &b}
    return run_protected(L, proc(L: ^State, user_data: rawptr) {
        data := cast(^Data)user_data
        chunk := program(L, data.name, data.reader, data.builder)
        vm_execute(L, &chunk)
        chunk_destroy(&chunk)
    }, &data)
}

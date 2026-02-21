#+private package
package luna

import "base:intrinsics"
import "core:c/libc"

State :: struct {
    handler: ^Error_Handler,
    objects: ^Object,
    stack: [16]Value,
}

Error :: enum u8 {
    Ok,
    Syntax,
    Runtime,
    Memory,
}

Error_Handler :: struct {
    prev:   ^Error_Handler,
    error:  Error,
    buffer: libc.jmp_buf,
}

Protected_Proc :: proc(L: ^State, user_data: rawptr)

run_protected :: proc(L: ^State, procedure: Protected_Proc, user_data: rawptr) -> Error {
    handler := Error_Handler{prev=L.handler}
    L.handler = &handler

    if libc.setjmp(&handler.buffer) == 0 {
        procedure(L, user_data)
    }

    L.handler = handler.prev
    return intrinsics.volatile_load(&handler.error)
}

run_throw_error :: proc(L: ^State, error: Error) -> ! {
    h := L.handler
    if h == nil {
        panic("Unprotected call to Luna API")
    }
    intrinsics.volatile_store(&h.error, error)
    libc.longjmp(&h.buffer, 1)
}

#+private package
package lulu

import "base:intrinsics"
import "core:c/libc"

VM :: struct {
    // Shared state across all VM instances.
    global: ^Global_State,

    // Stack-allocated linked list of error handlers.
    handler: ^Error_Handler,
}

Global_State :: struct {
    // Hash table of all interned strings.
    intern: Intern,

    // Singly linked list of all possibly-collectable objects across all
    // VM states.
    objects: ^Object_List,
}

Error_Handler :: struct {
    buffer: libc.jmp_buf,
    code:   Error,
    prev:  ^Error_Handler,
}

Error :: enum u8 {
    // No error occured.
    Ok,

    // Invalid token or semantically invalid sequence of tokens, was received.
    // This error is often easy to recover from.
    Syntax,

    // Some operation or a function call failed.
    // This error is slightly difficult to recover from, but manageable.
    Runtime,

    // Failed to allocate or reallocate some memory.
    // This error is often fatal. It is extremely difficult to recover from.
    Memory,
}

G :: proc(L: ^VM) -> ^Global_State {
    return L.global
}

/*
Run the procedure `p` in "protected mode", i.e. when an error is thrown
we are able to catch it safely.

**Parameters**
- p: The procedure to be run.
- ud: Arbitrary user-defined data for `p`.

**Guarantees**
- `p` is only ever called with the `ud` it was passed with.
 */
vm_run_protected :: proc(L: ^VM, p: proc(^VM, rawptr), ud: rawptr) -> Error {
    // Push new error handler.
    h: Error_Handler
    h.prev    = L.handler
    L.handler = &h

    if libc.setjmp(&L.handler.buffer) == 0 {
        p(L, ud)
    }

    // Restore old error handler.
    L.handler = h.prev
    return intrinsics.volatile_load(&h.code)
}

vm_throw :: proc(L: ^VM, code: Error) -> ! {
    h := L.handler
    // Unprotected call?
    if h == nil {
        panic("lulu panic: unprotected call")
    }
    intrinsics.volatile_store(&h.code, code)
    libc.longjmp(&h.buffer, 1)
}

vm_error_syntax :: proc(L: ^VM) -> ! {
    vm_throw(L, .Syntax)
}

vm_error_memory :: proc(L: ^VM) -> ! {
    vm_throw(L, .Memory)
}

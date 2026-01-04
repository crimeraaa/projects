#+private package
package lulu

import "base:intrinsics"
import "core:c/libc"

Error_Handler :: struct {
    buf:    libc.jmp_buf,
    code:   Error,
    prev:  ^Error_Handler,
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
vm_raw_run_protected :: proc(L: ^State, p: proc(^State, rawptr), user_data: rawptr) -> Error {
    // Push new error handler.
    h: Error_Handler
    h.prev    = L.handler
    L.handler = &h

    if libc.setjmp(&L.handler.buf) == 0 {
        p(L, user_data)
    }

    // Restore old error handler.
    L.handler = h.prev
    return intrinsics.volatile_load(&h.code)
}

/*
Runs the procedure `p` in 'protected mode'. If an error is thrown, the
previous stack is restored with an error message pushed on top.
 */
vm_raw_pcall :: proc(L: ^State, p: proc(^State, rawptr), user_data: rawptr) -> Error {
    old_base := vm_save_base(L)
    old_top  := vm_save_top(L)

    err := vm_raw_run_protected(L, p, user_data)
    switch err {
    case .Ok:
        return nil

    // We just pushed an error message, so move it to the top of the previous
    // stack view.
    case .Syntax, .Runtime:
        err_obj := L.registers[get_top(L) - 1]
        L.stack[old_top] = err_obj

    // We failed to (re)allocate some memory but we were able to get past VM
    // startup, so the memory error message is definitely interned.
    case .Memory:
        s := ostring_new(L, MEMORY_ERROR_MESSAGE)
        L.stack[old_top] = value_make(s)
    }

    // Had an error, so restore the previous stack view plus the error object.
    L.registers = L.stack[old_base:old_top + 1]
    return err
}

vm_throw :: proc(L: ^State, code: Error) -> ! {
    // Unprotected call?
    if h := L.handler; h == nil {
        panic("[PANIC] Unprotected call to Lulu API")
    } else {
        intrinsics.volatile_store(&h.code, code)
        libc.longjmp(&h.buf, 1)
    }
}

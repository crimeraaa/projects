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
Run the procedure `p` in "unrestoring protected mode".

**Parameters**
- p: The procedure to be run.
- ud: Arbitrary user-defined data for `p`.

**Returns**
- err: The API error code, if any, that was caught.

**Guarantees**
- `p` is only ever called with the `ud` it was passed with.
- The caller's stack frame is not restored. This is important to help handle
out-of-memory errors on main state startup.
 */
raw_run_unrestoring :: proc(L: ^State, p: proc(^State, rawptr), user_data: rawptr) -> (err: Error) {
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
Propagates the error code `code` to the first protected caller, if one exists.
 */
throw_error :: proc(L: ^State, code: Error) -> ! {
    // Unprotected call?
    if h := L.handler; h == nil {
        panic("[PANIC] Unprotected call to Lulu API")
    } else {
        intrinsics.volatile_store(&h.code, code)
        libc.longjmp(&h.buf, 1)
    }
}

/*
Runs the procedure `p` in "restoring protected mode".

**Parameters**
- p: The procedure to be run.
- ud: Arbitrary user-defined data for `p`.

**Returns**
- err: The API error code, if any, that was caught.

**Guarantees**
- `p` is only ever called with the `ud` it was passed with.
- If an error is caught, then the previous stack frame from before the
protected call is restored, plus an error message (error "object") pushed
on top.
 */
raw_run_restoring :: proc(L: ^State, p: proc(^State, rawptr), user_data: rawptr) -> (err: Error) {
    old_base := vm_save_base(L)
    old_top  := vm_save_top(L)

    err = raw_run_unrestoring(L, p, user_data)
    switch err {
    case .Ok:
        return nil

    case .Syntax, .Runtime:
        // We just pushed an error message, so move it to the top of the previous
        // stack view.
        err_obj := L.registers[get_top(L) - 1]
        L.stack[old_top] = err_obj

    case .Memory:
        // We failed to (re)allocate some memory but we were able to get past VM
        // startup, so the memory error message is definitely interned.
        err_obj := value_make(ostring_new(L, MEMORY_ERROR_MESSAGE))
        L.stack[old_top] = err_obj
    }

    // Had an error, so restore the previous stack view plus the error object.
    L.registers = L.stack[old_base:old_top + 1]
    return err
}

run_call :: proc(L: ^State, callee: ^Value, arg_count, ret_count: int) {
    // Index of the very first argument for the current stack frame.
    // Majority of the following instructions operate on indices to the full
    // stack, not just the registers window.
    old_base := vm_save_base(L)

    // Index of 1 past the last valid stack index for the current stack frame.
    old_top  := vm_save_top(L)

    // Index of the very first argument for the current stack frame,
    // which is always right after `callee`.
    new_base := vm_save_stack(L, callee) + 1

    callee_type := value_type(callee^);

    // Index of 1 past the last valid stack index for the new stack frame.
    // We start with the index of 1 past the last argument which is a good
    // default.
    new_top := new_base + arg_count
    #partial switch callee_type {
    case .Api_Proc:
        // When calling API procedures, they can only see their arguments.
        // So there is nothing more we can do.
        break

    case .Chunk:
        // When calling Lua functions, they can see their arguments
        // along with stack space needed for temporaries. Use whichever
        // one is larger to determine the actual top of the new stack frame.
        chunk := value_get_chunk(callee^)
        if extra := chunk.stack_used - arg_count; extra > 0 {
            new_top += extra
        }

        // Set all non-arguments (or unprovided arguments) to `nil`.
        for &reg in L.stack[new_base + arg_count:new_top] {
            reg = value_make()
        }
    case:
        unreachable("Cannot call value %v", callee_type)
    }

    // Push new stack frame.
    frame         := &L.frames[L.frame_index]
    L.frame_index += 1
    L.frame        = frame

    // Initialize the newly pushed stack frame.
    registers       := L.stack[new_base:new_top]
    L.registers     = registers
    frame.callee    = callee
    frame.registers = registers
    frame.saved_pc  = -1

    // Can call the Odin procedure directly?
    if callee_type == .Api_Proc {
        procedure := value_get_api_proc(callee^)
        procedure(L)
    } else {
        vm_execute(L)
    }

    // Move return values from the now-finished callee to the top of our
    // previous stack frame.
    ret_src := L.stack[new_top - ret_count:new_top]
    ret_dst := L.stack[old_top:old_top + ret_count]
    copy(ret_dst, ret_src)


    // Restore previous stack frame.
    L.frame_index -= 1
    frame         = &L.frames[L.frame_index]
    L.frame       = frame
    L.registers   = frame.registers

    // Lua chunks/closures, when not dealing with varargs, already have
    // the correct register state. Only API procedures need to worry about
    // seeing the returned values.
    if value_is_api_proc(L.frame.callee^) {
        new_top         = new_base - 1 + ret_count
        L.registers     = L.stack[old_base:new_top]
        frame.registers = L.registers
    }
}

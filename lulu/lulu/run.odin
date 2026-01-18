#+private package
package lulu

import "base:builtin"
import "base:intrinsics"
import "core:c/libc"
import "core:fmt"
import "core:mem"

Error_Handler :: struct {
    buf:    libc.jmp_buf,
    code:   Error,
    prev:  ^Error_Handler,
}

Frame :: struct {
    // The value which represents the function being called. It must be a pointer
    // so that we can try to report the variable name which points to the
    // function.
    callee: ^Value,

    // Index of instruction where we left off (e.g. if we dispatch a Lua
    // function call).
    saved_pc: int,

    // Window into VM's primary stack.
    registers: []Value,
}

unreachable :: proc(msg := "", args: ..any, loc := #caller_location) -> ! {
    when ODIN_DEBUG {
        if msg == "" {
            panic("Runtime panic", loc=loc)
       } else {
            fmt.panicf(msg, ..args, loc=loc)
       }
    } else {
        builtin.unreachable()
    }
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
        err_obj := value_make_ostring(ostring_new(L, MEMORY_ERROR_MESSAGE))
        L.stack[old_top] = err_obj
    }

    // Had an error, so restore the previous stack view plus the error object.
    L.registers = L.stack[old_base:old_top + 1]
    return err
}

run_call :: proc(L: ^State, func: ^Value, arg_count, ret_expect: int) {
    if !value_is_function(func^) {
        debug_type_error(L, "call", func)
    }

    // Index of the very first argument for the current stack frame.
    // Majority of the following instructions operate on indices to the full
    // stack, not just the registers window.
    old_base := vm_save_base(L)

    // Index of 1 past the last valid stack index for the current stack frame.
    old_top  := vm_save_top(L)

    // Index of `callee` in the current stack frame. Upon return, this is
    // the first index to be overwritten (if there are nonzero return values).
    call_index := vm_save_stack(L, func)

    // Index of the very first argument for the current stack frame,
    // which is always right after `callee`.
    new_base := call_index + 1

    closure := value_get_function(func^)

    // Index of 1 past the last valid stack index for the new stack frame.
    // We start with the index of 1 past the last argument which is a good
    // default.
    new_top := new_base + arg_count

    // When calling API procedures, they can only see their arguments.
    // So there is nothing more we can do.
    //
    // Otherwise, when calling Lua functions, they can see their arguments
    // along with stack space needed for temporaries. Use whichever
    // one is larger to determine the actual top of the new stack frame.
    if closure.is_lua {
        if extra := closure.lua.chunk.stack_used - arg_count; extra > 0 {
            new_top += extra
        }

        // Set all non-arguments (or unprovided arguments) to `nil`.
        empty := L.stack[new_base + arg_count:new_top]
        mem.zero_slice(empty)
    }

    // Push new stack frame.
    next           := &L.frames[L.frame_count]
    L.frame_count  += 1
    L.frame         = next

    // Initialize the newly pushed stack frame.
    registers      := L.stack[new_base:new_top]
    L.registers     = registers
    next.callee     = func
    next.registers  = registers
    next.saved_pc   = -1

    ret_actual: int
    if !closure.is_lua {
        // Can call the Odin procedure directly?
        ret_actual = closure.api.procedure(L)
        // API procedure may have pushed an arbitrary amount of values.
        new_top = vm_save_top(L)
    } else {
        ret_actual = vm_execute(L)
    }

    // Move return values from the now-finished callee to the top of our
    // previous stack frame.
    ret_from := L.stack[new_top - ret_actual:new_top]
    ret_to   := L.stack[call_index:call_index + ret_actual]
    copy(ret_to, ret_from)

    // If we have less return values than expected, set the unassigned
    // registers to `nil`.
    if extra := ret_expect - ret_actual; extra > 0 {
        // Index of 1 past the last returned value.
        last  := call_index + ret_actual
        empty := L.stack[last:last + extra]
        mem.zero_slice(empty)
        ret_actual += extra
    }

    // Restore previous stack frame, if one exists.
    if n := L.frame_count - 2; n >= 0 {
        prev := &L.frames[n]
        // Parent caller needs to see the now-returned values because
        // they have no other way of knowing.
        if !value_get_function(prev.callee^).is_lua || ret_expect == VARIADIC {
            new_top = call_index + ret_actual
            prev.registers = L.stack[old_base:new_top]
        }
        L.frame_count -= 1
        L.frame        = prev
        L.registers    = prev.registers
    } else {
        L.frame_count = 0
        L.frame       = nil
    }
}

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
    //
    // **Note(2026-01-20)**
    //
    // If the stack is being reallocated, make sure to revalidate this!
    //
    callee: ^Value,

    // Index of instruction where we left off (e.g. if we dispatch a Lua
    // function call).
    saved_pc: i32,

    // Window into VM's primary stack.
    registers: []Value,
}

Call_Type :: enum {
    Odin,
    Lua,
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
Propagates the error code `code` to the first protected caller, if one exists.
 */
throw_error :: proc(L: ^State, code: Error) -> ! {
    // Unprotected call?
    h := L.handler
    if h == nil {
        panic("[PANIC] Unprotected call to Lulu API")
    } else {
        intrinsics.volatile_store(&h.code, code)
        libc.longjmp(&h.buf, 1)
    }
}

/*
Run the procedure `p` in "unrestoring protected mode".

**Parameters**
- procedure: The procedure to be run.
- user_data: Arbitrary user-defined data for `procedure`.

**Returns**
- err: The API error code, if any, that was caught.

**Guarantees**
- `procedure` is only ever called with the `user_data` it was passed with.
- The caller's stack frame is not restored. This is important to help handle
out-of-memory errors on main state startup.
 */
run_raw_call :: proc(L: ^State, procedure: proc(^State, rawptr), user_data: rawptr) -> (err: Error) {
    // Push new error handler.
    h: Error_Handler
    h.prev    = L.handler
    L.handler = &h

    if libc.setjmp(&L.handler.buf) == 0 {
        procedure(L, user_data)
    }

    // Restore old error handler.
    L.handler = h.prev
    return intrinsics.volatile_load(&h.code)
}

/*
Runs the procedure `procedure` in "restoring protected mode".

**Parameters**
- procedure: The procedure to be run.
- user_data: Arbitrary user-defined data for `procedure`.

**Returns**
- err: The API error code, if any, that was caught.

**Guarantees**
- `procedure` is only ever called with the `user_data` it was passed with.
- If an error is caught, then the previous stack frame from before the
protected call is restored, plus an error message (error "object") pushed
on top.
 */
run_raw_pcall :: proc(L: ^State, procedure: proc(^State, rawptr), user_data: rawptr) -> (err: Error) {
    old_base  := state_save_base(L)
    old_top   := state_save_top(L)
    old_frame := L.frame_count - 1

    err = run_raw_call(L, procedure, user_data)
    switch err {
    case .Ok:
        // Don't change the stack because everything is (assumed to be) fine.
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
    registers := L.stack[old_base:old_top + 1]
    L.registers = registers

    if old_frame != -1 {
        frame := &L.frames[old_frame]
        frame.registers = registers

        // In normal execution, the pushed frames are popped. However we
        // have to do so explicitly when errors are thrown.
        L.frame       = frame
        L.frame_count = old_frame + 1
    } else {
        L.frame       = nil
        L.frame_count = 0
    }
    return err
}

run_call :: proc(L: ^State, func: ^Value, arg_count, ret_expect: int) {
    type := run_call_prologue(L, func, arg_count, ret_expect)
    if type == .Lua {
        vm_execute(L, ret_expect)
    }
}

run_call_prologue :: proc(L: ^State, func: ^Value, arg_count, ret_expect: int) -> Call_Type {
    if !value_is_function(func^) {
        debug_type_error(L, "call", func)
    }

    // Index of `callee` in the current stack frame. Upon return, this is
    // the first index to be overwritten (if there are nonzero return values).
    call_index := state_save_stack(L, func)

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
        if extra := int(closure.lua.chunk.stack_used) - arg_count; extra > 0 {
            new_top += extra
        }

        // Set all non-arguments (or unprovided arguments) to `nil`.
        empty := L.stack[new_base + arg_count:new_top]
        mem.zero_slice(empty)
    }

    // Initialize the newly pushed stack frame.
    registers := L.stack[new_base:new_top]
    _push_frame(L, func, registers)

    ret_actual: int
    if !closure.is_lua {
        // Can call the Odin procedure directly?
        ret_actual = closure.api.procedure(L)

        // API procedure may have pushed an arbitrary amount of values.
        new_top = state_save_top(L)
        ret_values := L.stack[new_top - ret_actual:new_top]
        run_call_return(L, ret_expect, ret_values)
        return .Odin
    } else {
        return .Lua
    }
}

@(private="file")
_push_frame :: proc(L: ^State, func: ^Value, registers: []Value) {
    // Push new stack frame.
    next := &L.frames[L.frame_count]
    next.saved_pc   = -1
    next.callee     = func
    next.registers  = registers

    // prev_callee := L.frame.callee if L.frame != nil else nil
    // fmt.printfln("[PUSH ] --- prev=%p, next=%p", prev_callee, next.callee)

    L.frame_count  += 1
    L.frame         = next
    L.registers     = registers

}

run_call_return :: proc(L: ^State, ret_expect: int, ret_src: []Value) {
    ret_actual := len(ret_src)

    // Index of the function being called in the stack.
    call_index := state_save_stack(L, L.frame.callee)

    // Index of the last actual returned value in the stack.
    ret_top := call_index + ret_actual

    // If we have less return values than expected, set the unassigned
    // registers to `nil`.
    if extra := ret_expect - ret_actual; extra > 0 {
        empty := L.stack[ret_top:ret_top + extra]
        mem.zero_slice(empty)
        ret_top += extra
    }

    ret_dst := L.stack[call_index:ret_top]
    copy(ret_dst, ret_src)
    _pop_frame(L, ret_expect, ret_top)
}

@(private="file")
_pop_frame :: proc(L: ^State, ret_expect, ret_top: int) {
    prev := &L.frames[L.frame_count - 2] if L.frame_count - 2 >= 0 else nil
    // fmt.printfln("[POP  ] --- pop %p, restore %p", L.frame.callee, prev.callee if prev != nil else nil)

    // Restore previous stack frame, if one exists.
    if prev != nil {
        assert(prev.callee != L.frame.callee, "Overwriting callee will fuck us over")

        // Parent caller needs to see the now-returned values because
        // they have no other way of knowing.
        if !value_get_function(prev.callee^).is_lua || ret_expect == VARIADIC {
            old_base := state_save_stack(L, raw_data(prev.registers))
            prev.registers = L.stack[old_base:ret_top]
        }
        L.frame_count -= 1
        L.frame        = prev
        L.registers    = prev.registers
    } else {
        assert(L.frame_count == 1)
        L.frame       = nil
        L.frame_count = 0
        L.registers   = L.stack[:ret_top]
    }
}

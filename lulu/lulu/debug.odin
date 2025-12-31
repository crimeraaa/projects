#+private package
package lulu

import "core:fmt"

/* 
**Parameters**
- a, b: Must be pointers so that we can check if they are inside the VM
stack or not. This information is useful when reporting errors from local or
global variables.
 */
debug_arith_error :: proc(L: ^VM, a, b: ^Value) -> ! {
    culprit := b if value_is_number(a^) else a
    debug_type_error(L, "perform arithmetic on", culprit)
}

debug_type_error :: proc(L: ^VM, action: string, culprit: ^Value) -> ! {
    frame := &L.frame    
    chunk := frame.chunk

    // Check if `culprit` is in the stack
    reg := -1
    for &v, i in frame.stack {
        if &v == culprit {
            reg = i
            break
        }
    }
    
    type_name := value_type_name(culprit^)
    if reg != -1 {
        local_name, ok := find_local_by_pc(chunk, reg + 1, frame.saved_pc)
        if ok {
            debug_runtime_error(L, "Attempt to %s local '%s' (a %s value)",
                action, local_name, type_name)
        }
    }
    debug_runtime_error(L, "Attempt to %s a %s value", action, type_name)
}

/* 
Throws a runtime error and reports an error message.

**Assumptions**
- `L.frame.saved_pc` was set beforehand so we know where to look.
 */
debug_runtime_error :: proc(L: ^VM, format := "", args: ..any) -> ! {
    frame := L.frame
    chunk := frame.chunk
    file  := ostring_to_string(chunk.name)
    line  := chunk.lines[frame.saved_pc]
    fmt.eprintf("%s:%i: ", file, line)
    fmt.eprintfln(format, ..args)
    vm_throw(L, .Runtime)
}

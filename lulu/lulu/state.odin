package lulu

import "core:strings"
import "core:mem"

// **Note(2025-01-05)**
//
// Although the fields are public, do not directly access nor modify them.
// Use only the public API to do so.
//
State :: struct {
    // Shared state across all VM instances.
    global_state: ^Global_State,

    // Hash table of all defined global variables.
    globals_table: Value,

    // Used for string concatenation and internal string formatting.
    builder: strings.Builder,

    // Stack-allocated linked list of error handlers.
    handler: ^Error_Handler,

    // Stack window of current function, a 'window' into `stack`.
    registers: []Value,

    // Current frame information.
    frame:      ^Frame,
    frame_count: int,
    frames:      [16]Frame,

    // Stack used across all active call frames.
    stack: [64]Value,
}

Global_State :: struct {
    // Pointer to the main state we were allocated alongside.
    main_state: ^State,

    // Allocator passed whenever memory is allocated.
    backing_allocator: mem.Allocator,

    // Hash table of all interned strings.
    intern: Intern,

    // Singly linked list of all possibly-collectable objects across all
    // VM states.
    objects: ^Object,

    bytes_allocated: int,
}

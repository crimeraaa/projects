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

    // Mark phase:  Filled up, can be modified in-place.
    // Trace phase: Traversed.
    gray_head: ^Gc_List,

    // The very last node in `gray_head`. This is useful when appending child
    // nodes from roots so that we do not mess up iteration.
    //
    // Mark phase:  Never used.
    // Trace phase: Filled up, can be modified in-place.
    gray_tail: ^Gc_List,

    bytes_allocated: int,
    gc_state: Gc_State,
}

@private
G :: proc(L: ^State) -> ^Global_State {
    return L.global_state
}

@private
state_init :: proc(L: ^State, g: ^Global_State, allocator: mem.Allocator) -> (ok: bool) {
    L.global_state      = g
    g.main_state        = L
    g.backing_allocator = allocator
    g.gc_state          = .Paused

    // Don't use `run_raw_pcall()`, because we won't be able to push an
    // error object in case of memory errors.
    err := run_raw_call(L, proc(L: ^State, _: rawptr) {
        // Ensure the pointed-to data is non-nil.
        L.registers = L.stack[:0]

        g := G(L)

        // Ensure that, when we start interning strings, we already have
        // valid indexes.
        intern_resize(L, &g.intern, 32)
        ostring := ostring_new(L, MEMORY_ERROR_MESSAGE)
        ostring.flags += {.Fixed}
        for kw_type in Token_Type.And..=Token_Type.While {
            kw      := token_type_string(kw_type)
            ostring  = ostring_new(L, kw)
            ostring.kw_type = kw_type
            object_set_fixed(ostring)
        }

        // Ensure that the globals table is of some non-zero minimum size.
        table := table_new(L, hash_count=17, array_count=0)
        L.globals_table = value_make(table)

        // Initialize concat string builder with some reasonable default size.
        L.builder = strings.builder_make(32, allocator=g.backing_allocator)
    }, nil)

    g.gc_state = .None
    return err == nil
}

@private
state_destroy :: proc(L: ^State) {
    g := L.global_state
    strings.builder_destroy(&L.builder)
    intern_destroy(L, &g.intern)
    object_free_all(L, g.objects)
}

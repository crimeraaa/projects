#+private file
package lulu

import "core:fmt"
import os "core:os/os2"
import "core:strings"
import "core:terminal/ansi"

RESET :: ansi.ESC + ansi.CSI + ansi.RESET + "m"

@(private="package")
Gc_List :: distinct Object

@(private="package")
Gc_State :: enum u8 {
    None,
    Paused,
    Mark,
    Trace,
    Sweep,
}

@(private="package")
Gc_Log_Mode :: enum {
    Begin,
    End,
    Mark,
    Trace,
    New,
    Free,
}

GC_LOG_MODE_STRINGS := [Gc_Log_Mode]string {
    .Begin  = _log_mode_string(ansi.FG_MAGENTA, "[BEGIN]"),
    .End    = _log_mode_string(ansi.FG_MAGENTA, "[END  ]"),
    .Mark   = _log_mode_string(ansi.FG_CYAN,    "[MARK ]"),
    .Trace  = _log_mode_string(ansi.FG_YELLOW,  "[TRACE]"),
    .New    = _log_mode_string(ansi.FG_GREEN,   "[NEW  ]"),
    .Free   = _log_mode_string(ansi.FG_RED,     "[FREE ]"),
}

_log_mode_string :: proc "contextless" ($color, $action: string) -> string {
    return ansi.ESC + ansi.CSI + color + "m" + action + RESET
}

@(private="package")
gc_collect :: proc(L: ^State, g: ^Global_State, loc := #caller_location) {
    if g.gc_state == .Paused {
        return
    }

    gc_log(.Begin, "---", loc=loc)
    defer gc_log(.End, "---", loc=loc)

    _mark_roots(L, g)
    _trace_references(L, g)
    _sweep_strings(L, g, loc=loc)
    _sweep_objects(L, g, loc=loc)
}

_mark_roots :: proc(L: ^State, g: ^Global_State, loc := #caller_location) {
    assert(g.gc_state == nil)
    g.gc_state = .Mark
    _mark_value(L, g, L.globals_table, loc=loc)
    _mark_array(L, g, L.stack[:state_save_top(L)], loc=loc)
    for frame in L.frames[:L.frame_count] {
        _mark_value(L, g, frame.callee^, loc=loc)
    }

    // No tail yet? This is possible if we have a 1-object graph (somehow).
    if g.gray_tail == nil {
        // Possible for the head to be nil, e.g. user explicitly set the
        // globals table to `nil` and we are not in a protected API call.
        g.gray_tail = g.gray_head
    }
}

_mark_array :: proc(L: ^State, g: ^Global_State, array: []Value, loc := #caller_location) {
    for value in array {
        _mark_value(L, g, value, loc=loc)
    }
}

_mark_value :: proc(L: ^State, g: ^Global_State, value: Value, loc := #caller_location) {
    if !value_is_object(value) {
        return
    }

    object := value_get_object(value, loc=loc)
    _mark_object(L, g, object, loc=loc)
}

_mark_object :: proc(L: ^State, g: ^Global_State, object: ^Object, loc := #caller_location) {
    set_gc_list :: proc(object: ^Object, next: ^Gc_List) {
        switch object.type {
        case .Nil, .Boolean, .Number, .Light_Userdata, .String, .Integer:
            unreachable("%v has no gc_list", object.type)
        case .Table:    object.table.gc_list    = next
        case .Function: object.function.gc_list = next
        case .Chunk:    object.chunk.gc_list    = next
        }
    }

    // Already marked?
    if object_is_reachable(object) {
        return
    }
    object_set_gray(object)
    gc_log_alloc(object_typeid(object), Gc_Log_Mode(g.gc_state), object, object_size(object), loc=loc)

    // Strings can be marked gray but we do not add them to the gray list
    // because all strings are already visible to `Intern` anyway.
    if object.type == .String {
        return
    }

    #partial switch g.gc_state {
    case .Mark:
        // Mark phase: `object` (the new node) will link to the current head of
        // the gray list as we are going to prepend it to make it the new head.
        set_gc_list(object, g.gray_head)

        // Since we are constantly prepending, the very first non-nil node
        // we encounter will actually be the tail by the time we trace.
        if g.gray_tail == nil {
            g.gray_tail = g.gray_head
        }

        // `object` was already linked to the current head, so the head will
        // now be the node in order to prepend it.
        g.gray_head = cast(^Gc_List)object

    case .Trace:
        // If we reached this point, we must have a non-nil GC list.
        assert(g.gray_tail != nil)

        // Trace phase: `object` (the new node) will link to nothing as we are
        // going to append it, thus making it the new tail.
        set_gc_list(object, nil)

        // Update current tail to link to `object` (the new node)...
        set_gc_list(cast(^Object)g.gray_tail, cast(^Gc_List)object)

        // ...then set `object` (the new node) as the new tail.
        g.gray_tail = cast(^Gc_List)object
    case:
        unreachable("Invalid state %v", g.gc_state)
    }
}

_trace_references :: proc(L: ^State, g: ^Global_State) {
    assert(g.gc_state == .Mark)
    g.gc_state = .Trace

    // While we're traversing, we may append new objects. This is fine because
    // since they're appended we have not invalidated the current iteration.
    for node := g.gray_head; node != nil; {
        next := _blacken_object(L, g, cast(^Object)node)
        node = next
    }

    // Prepare for next cycle.
    g.gray_head = nil
    g.gray_tail = nil
}

_blacken_object :: proc(L: ^State, g: ^Global_State, object: ^Object) -> (next: ^Gc_List) {
    assert(object_is_marked(object))
    assert(!object_is_fixed(object))

    gc_log_alloc(object_typeid(object), .Trace, object, object_size(object))
    list: ^^Gc_List
    #partial switch object.type {
    case .Table:    list = _blacken_table(L, g, &object.table)
    case .Function: list = _blacken_closure(L, g, &object.function)
    case .Chunk:    list = _blacken_chunk(L, g, &object.chunk)
    case:
        unreachable("Cannot blacken %v", object.type)
    }
    // Unlink `object` from the gray list.
    next  = list^
    list^ = nil
    return next
}

_blacken_table :: proc(L: ^State, g: ^Global_State, table: ^Table) -> ^^Gc_List {
    entries := table_get_entries(table)
    for entry in entries {
        key   := entry.key.v
        value := entry.value
        // Even if key is non-nil, if the value referenced is nil then we
        // assume that this key is unreachable, it may be collectible.
        if !value_is_nil(value) {
            _mark_value(L, g, key)
            _mark_value(L, g, value)
        }
    }
    return &table.gc_list
}

_blacken_closure :: proc(L: ^State, g: ^Global_State, closure: ^Closure) -> ^^Gc_List {
    if closure.is_lua {
        chunk := closure.lua.chunk
        _mark_object(L, g, cast(^Object)chunk)
        // Traverse the chunk later.
    } else {
        #no_bounds_check {
            upvalues := closure.api.upvalues[:closure.api.upvalue_count]
            _mark_array(L, g, upvalues)
        }
    }
    return &closure.gc_list
}

_blacken_chunk :: proc(L: ^State, g: ^Global_State, chunk: ^Chunk) -> ^^Gc_List {
    _mark_object(L, g, cast(^Object)chunk.name)

    // Given a reachable chunk, all its local names are not collectible.
    // Interned identifiers may also be shared across multiple closures.
    for local in chunk.locals {
        // We could be in the middle of a compilation.
        if local.name != nil {
            _mark_object(L, g, cast(^Object)local.name)
        }
    }
    _mark_array(L, g, chunk.constants)
    return &chunk.gc_list
}

_sweep_strings :: proc(L: ^State, g: ^Global_State, loc := #caller_location) {
    intern := &g.intern
    for &list in intern.table[:] {
        // Since strings are kept in their own collision lists, we can free
        // then directly.
        prev: ^Object
        state := list
        for node in object_iterator(&state) {
            assert(node.type == .String)
            ostring := &node.string
            // Save now in case `ostring` gets freed.
            next := node.next

            // Previously marked (in stack, etc.) or is a keyword?
            if object_is_reachable(node) {
                object_set_white(node)
                prev = node
            } else {
                if prev != nil {
                    // Unlink from the middle of a list.
                    prev.next = next
                } else {
                    // Unlink from primary array slot (the head).
                    list = next
                }
                ostring_free(L, ostring, loc=loc)
            }
        }
    }
}

_sweep_objects :: proc(L: ^State, g: ^Global_State, loc := #caller_location) {
    assert(g.gc_state == .Trace)
    g.gc_state = .Sweep

    prev: ^Object
    object := g.objects
    for object != nil {
        next := object.next
        if object_is_reachable(object) {
            // Set white to prepare it for the next cycle.
            object_set_white(object)

            // We may unlink an unreachable object from this one.
            prev = object
        } else {
            // Unlink the unreached object from its parent linked list
            // right before we free it.
            if prev != nil {
                prev.next = next
            } else {
                g.objects = next
            }
            object_free(L, object, loc=loc)
        }
        object = next
    }
    g.gc_state = nil
}

@(disabled=!DEBUG_LOG_GC)
gc_log :: proc(mode: Gc_Log_Mode, message: string, loc := #caller_location) {
    gc_logf(mode, "%s", message, loc=loc)
}

@(disabled=!DEBUG_LOG_GC)
gc_logf :: proc(mode: Gc_Log_Mode, format: string, args: ..any, loc := #caller_location) {
    buf: [64]byte
    file := loc.file_path
    file  = file[strings.last_index_byte(file, os.Path_Separator) + 1:]
    file_line := fmt.bprintf(buf[:], "%s:%i", file, loc.line)

    fmt.printf("%s %-18s | ", GC_LOG_MODE_STRINGS[mode], file_line, flush=false)
    fmt.printfln(format, ..args)
}

@(private="package", disabled=!DEBUG_LOG_GC)
gc_log_alloc :: proc(T: typeid, mode: Gc_Log_Mode, ptr: rawptr, size: int, loc := #caller_location) {
    // Will just clog up the logs.
    if mode == .Free && ptr == nil {
        return
    }

    buf: [64]byte
    type_name := fmt.bprint(buf[:], T)
    if mode != .New && T == typeid_of(^Ostring) {
        text := ostring_to_string(cast(^Ostring)ptr)
        gc_logf(mode, "%-13s | %q", type_name, text, loc=loc)
    } else {
        gc_logf(mode, "%-13s | %p (%i bytes)", type_name, ptr, size, loc=loc)
    }
}

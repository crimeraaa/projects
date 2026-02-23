package lulu_aux

// standard
import os "core:os/os2"

// local
import lulu ".."

type_name_at :: proc(L: ^lulu.State, index: int) -> string {
    t := lulu.type(L, index)
    return lulu.type_name(L, t)
}

Library_Entry :: struct {
    name:      string,
    procedure: lulu.Api_Proc,
}

/*
Pushes a new library table, setting each key-value pair to the ones in
`library`.

**Side-effects**
- push: 1
- pop:  0
 */
new_library :: proc(L: ^lulu.State, library: []Library_Entry) {
    lulu.new_table(L, hash_count=len(library))
    set_library(L, library)
}

/*
Sets each field in the library table at `R[-1]` to some associated procedure.

**Side-effects**
- push: 0
- pop:  0
 */
set_library :: proc(L: ^lulu.State, library: []Library_Entry) {
    for field in library {
        lulu.push_api_proc(L, field.procedure)
        lulu.set_field(L, -2, field.name)
    }
}

/*
Pushes an error message and throws a runtime error, returning immediately to
the first protected caller.
 */
errorf :: proc(L: ^lulu.State, format: string, args: ..any) -> ! {
    lulu.location(L, 1)
    lulu.push_fstring(L, format, ..args)
    lulu.concat(L, 2)
    lulu.error(L)
}

type_error :: proc(L: ^lulu.State, index: int, expected: lulu.Type) -> ! {
    type_name_error(L, index, lulu.type_name(L, expected))
}

type_name_error :: proc(L: ^lulu.State, index: int, expected: string) -> ! {
    actual   := type_name_at(L, index)
    message  := lulu.push_fstring(L, "%s expected, got %s", expected, actual)
    arg_error(L, index, message)
}

arg_error :: proc(L: ^lulu.State, index: int, message: string) -> ! {
    errorf(L, "bad argument #%i (%s)", index, message)
}

arg_check :: proc(L: ^lulu.State, cond: bool, index: int, message := #caller_expression(cond)) {
    if !cond {
        arg_error(L, index, message)
    }
}

check_any :: proc(L: ^lulu.State, index: int) {
    if lulu.is_none(L, index) {
        arg_error(L, index, "value expected")
    }
}

check_number :: proc(L: ^lulu.State, index: int) -> f64 {
    n, ok := lulu.to_number(L, index)
    if !ok {
        type_error(L, index, .Number)
    }
    return n
}

check_integer :: proc(L: ^lulu.State, index: int) -> int {
    return int(check_number(L, index))
}

check_string :: proc(L: ^lulu.State, index: int) -> string {
    s, ok := lulu.to_string(L, index)
    if !ok {
        type_error(L, index, .String)
    }
    return s
}

opt_number :: proc(L: ^lulu.State, index: int, default: f64) -> f64 {
    if lulu.is_none_or_nil(L, index) {
        return default
    }
    return check_number(L, index)
}

opt_integer :: proc(L: ^lulu.State, index, default: int) -> int {
    return int(opt_number(L, index, f64(default)))
}

opt_string :: proc(L: ^lulu.State, index: int, default: string) -> string {
    if lulu.is_none_or_nil(L, index) {
        return default
    }
    return check_string(L, index)
}

load :: proc {
    load_line,
    load_file,
}

/*
**Errors**
- .Syntax
 */
load_line :: proc(L: ^lulu.State, name, line: string, allocator := context.allocator) -> (err: lulu.Error) {
    line_reader_proc :: proc(user_data: rawptr) -> (current: []byte) {
        line   := cast(^[]byte)user_data
        current = line^

        // Upon the first read, make it so that `buf` will always cause the EOF
        // condition from now on.
        if len(current) > 0 {
            line^ = nil
        }
        return
    }

    line := transmute([]byte)line
    return lulu.load(L, name, line_reader_proc, &line, allocator)
}

/*
Read the file given by `name` fully until EOF or an error is reached.
That is, newlines do not terminate the input.

Parameters:
name: As a special behavior, the empty string `""` represents `os.stdin`.
This is useful if you want to read stdin as an EOF-terminated file.

Errors:
- .Runtime: We failed to load the file for some reason.
- .Syntax
 */
load_file :: proc(L: ^lulu.State, name: string, allocator := context.allocator) -> (err: lulu.Error) {
    File_Reader :: struct {
        file: ^os.File,
        buf:  [BUFFER_SIZE]byte,
    }

    file_reader_proc :: proc(user_data: rawptr) -> (current: []byte) {
        data := cast(^File_Reader)user_data
        n, err := os.read(data.file, data.buf[:])

        // EOF was not reached and nothing bad happened?
        if n > 0 && err == nil {
            current = data.buf[:n]
        }
        return
    }

    file: ^os.File
    name := name
    if name == "" {
        file = os.stdin
        name = "stdin"
    } else {
        open_err: os.Error
        file, open_err = os.open(name)
        if open_err != nil {
            lulu.push_fstring(L, "%s: %s", name, os.error_string(open_err))
            return .Runtime
        }
    }
    // Don't close stdin!
    defer if file != os.stdin {
        os.close(file)
    }

    data: File_Reader = ---
    data.file = file
    return lulu.load(L, name, file_reader_proc, &data, allocator)
}

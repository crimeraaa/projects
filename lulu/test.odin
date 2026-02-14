package lulu_repl

// standard
import "core:fmt"
import "core:testing"

// local
import "lulu"
import lulu_aux "lulu/aux"
import lulu_lib "lulu/libs"

Value :: union {
    bool,
    f64,
    string,
    lulu.Error,
}

Test :: struct {
    name, input: string,
    expected:    Value,
}

_test :: proc "contextless" ($name: string, expected: Value) -> Test {
    return Test{name, #load("tests/" + name), expected}
}

TESTS := [?]Test{
    _test("ambiguous.lua",     lulu.Error.Syntax),
    _test("arith.lua",         7.8),
    _test("blocks.lua",        "file"),
    _test("break.lua",         2.0),
    _test("compare.lua",       1.0),
    _test("compare-neq.lua",   -1.0),
    _test("error-concat.lua",  lulu.Error.Runtime),
    _test("error-comment.lua", lulu.Error.Syntax),
    _test("error-stream-overflow.lua", lulu.Error.Syntax),
    _test("fun.lua",           nil),
    _test("hello.lua",         nil),
    _test("if-else.lua",       nil),
    _test("if-return.lua",     nil),
    _test("literal.lua",       7.8),
    _test("return-number.lua", lulu.Error.Syntax),
    _test("table.lua",         nil),
    _test("while.lua",         4.0),
}

try_test :: proc(t: ^testing.T, L: ^lulu.State, test: Test) -> Value {
    fmt.printfln("[LULU ] --- Running '%s'...", test.name, flush=false)

    lulu_aux.load(L, test.name, test.input, context.temp_allocator) or_return
    lulu.pcall(L, arg_count=0, ret_count=1) or_return

    #partial switch type := lulu.type(L, -1); type {
    case .None, .Nil:   return nil
    case .Boolean:      return lulu.to_boolean(L, -1)
    case .Number:       return lulu.to_number(L, -1)
    case .String:       return lulu.to_string(L, -1)
    case:
        testing.expectf(t, false, "Invalid test type: %v", type)
    }
    return nil
}

@test
run_tests :: proc(t: ^testing.T) {
    ms: lulu.Main_State
    L, ok := lulu.new_state(&ms, context.allocator)
    testing.expect(t, ok && L != nil)

    // NOTE: Failures can prevent us from cleaning up properly.
    defer lulu.close(L)
    lulu_lib.open(L)

    backing: [256]byte
    fb_buffer := fb_buffer_make(backing[:])
    context.temp_allocator = fb_buffer_allocator(&fb_buffer)
    for test, index in TESTS {
        lulu.set_top(L, 0)
        res := try_test(t, L, test)
        testing.expect_value(t, res, test.expected)
    }
}

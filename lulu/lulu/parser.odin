#+private file
package lulu

import "core:strings"
import "core:fmt"

@(private="package")
Parser :: struct {
    // Lexical state to help manage token stream.
    lexer: Lexer,

    // Token stream. Used to determine how we should act.
    consumed: Token,

    // Token stream. Helps determine what the next action should be.
    token: Token,

    // Token stream. The lookahead token is used only for table constructors
    // in order to differentiate key-value pairs and identifiers in expressions.
    lookahead: Token,

    // Used to prevent invalidation of `consumed.lexeme` whenever we scan
    // a new token. The backing buffer must exist at the same lifetime of
    // the parent parser. It is tempting to inline the backing buffer in the
    // struct directly, but returning parsers by value on the stack makes this
    // complicated.
    consumed_builder: strings.Builder,
}

Precedence :: enum u8 {
    None,
    Equality,   // == ~=
    Comparison, // < <= > >=
    Concat,     // ..
    Terminal,   // + -
    Factor,     // * / %
    Exponent,   // ^
    Unary,      // - not #
}

Rule :: struct {
    // Knowing the opcode already helps us dispatch to more specific
    // infix expression parsers.
    binop: Binop,

    // Left-hand-side precedence. Helps determine when we should terminate
    // recursion in the face of higher-precedence parent expressions.
    left:  Precedence,

    // Right-hand-side precedence. Helps determine when we should recursively
    // parse higher-precedence child expressions.
    right: Precedence,
}

parser_make :: proc(
    L: ^State,
    compiler: ^Compiler,
    builder: ^strings.Builder,
    buf: []byte,
    name: ^Ostring,
    input: Reader
) -> Parser {
    p: Parser
    p.lexer            = lexer_make(L, compiler, builder, name, input)
    p.consumed_builder = strings.builder_from_bytes(buf[:])
    advance_token(&p)
    return p
}

@(private="package", disabled=ODIN_DISABLE_ASSERT)
parser_assert :: proc(p: ^Parser, cond: bool, msg := #caller_expression(cond), args: ..any, loc := #caller_location) {
    if !cond {
        file := ostring_to_string(p.lexer.name)
        line := p.consumed.line
        col  := p.consumed.col

        here := fmt.tprintf("\n%s:%i:%i", file, line, col)
        info := fmt.tprintf(msg, ..args)
        context.assertion_failure_proc(here, info, loc)
    }
}

@(private="package")
program :: proc(L: ^State, builder: ^strings.Builder, name: ^Ostring, input: Reader) {
    main_chunk := chunk_new(L, name)
    // Ensure `main_chunk` cannot be collected.
    state_push(L, value_make(cast(^Object)main_chunk, .Chunk))

    // Any more is probably overkill. We really only need this to report errors.
    buf: [16]byte
    c: Compiler
    p: Parser
    // MUST be initialized before the parser.
    c = compiler_make(L, &p, main_chunk)
    p = parser_make(L, &c, builder, buf[:], name, input)

    // Block for file scope (outermost scope).
    block(&p, &c)
    expect_token(&p, .EOF)
    compiler_end(&c)

    // Make the closure BEFORE popping the chunk in order to prevent the chunk
    // from being collected in case GC is run during the closure's creation.
    main_closure := closure_lua_new(L, main_chunk, 0)
    state_pop(L)
    state_push(L, main_closure)
}

/*
Unconditionally consumes a new token.

**Guarantees**
- The old consumed token is thrown away.
- The old current token  is the new consumed token.
- The next scanned token is now the new current token if there is no
lookahead token to discharge. Otherwise, the lookahead token is discharged.
 */
advance_token :: proc(p: ^Parser) {
    next: Token
    if p.lookahead.type == nil {
        // For numbers, copy the current token's lexeme to avoid invalidation.
        // Otherwise we can use the token type or the string payload.
        if s, is_owned := token_string(p.token); is_owned {
            b   := &p.consumed_builder
            cap := strings.builder_cap(b^)
            strings.builder_reset(b)

            n := strings.write_string(b, s if len(s) <= cap else s[:cap - 3])
            if n < len(s) {
                strings.write_string(b, "...")
            }
            p.token.lexeme = strings.to_string(b^)
        }
        next = lexer_scan_token(&p.lexer)
    } else {
        // Discharge the lookahead.
        next        = p.lookahead
        p.lookahead = {}
    }

    if next.type == nil {
        error_at(p, next, "Unexpected token")
    }

    p.consumed = p.token
    p.token    = next
    // fmt.println(next)
}

/*
**Returns**
- type: The token type of the lookahead token.
 */
consume_lookahead :: proc(p: ^Parser) -> (type: Token_Type) {
    parser_assert(p, p.lookahead.type == nil)
    next := lexer_scan_token(&p.lexer)
    if next.type == nil {
        error_at(p, next, "Unexpected token")
    }
    p.lookahead = next
    return next.type
}

peek_consumed :: proc(p: ^Parser) -> Token_Type {
    return p.consumed.type
}

peek_token :: proc(p: ^Parser) -> Token_Type {
    return p.token.type
}

expect_token :: proc(p: ^Parser, want: Token_Type, info := "") {
    if !match_token(p, want) {
        buf: [256]byte
        msg: string
        what := token_type_string(want)
        if info == "" {
            msg = fmt.bprintf(buf[:], "Expected '%s'", what)
        } else {
            msg = fmt.bprintf(buf[:], "Expected '%s' %s", what, info)
        }
        error_current(p, msg)
    }
}

check_token :: proc(p: ^Parser, want: Token_Type) -> (found: bool) {
    return p.token.type == want
}

/*
Consumes the lookahead token iff it matches `want`.

**Returns**
- `true` if the lookahead token matched `want`, else `false`.
 */
match_token :: proc(p: ^Parser, want: Token_Type) -> (found: bool) {
    found = p.token.type == want
    if found {
        advance_token(p)
    }
    return found
}


/*
**Note(2026-01-26)**
- This function does not verify correctness, it is up to the caller to
expect a proper ending token.
 */
block_continue :: proc(p: ^Parser) -> bool {
    #partial switch p.token.type {
    case .Else, .Elseif, .End, .EOF: return false
    case:
        break
    }
    return true
}

/*
Throws a syntax error at the consumed token.
 */
@(private="package")
parser_error :: proc(p: ^Parser, msg: string) -> ! {
    error_at(p, p.consumed, msg)
}

/*
Throws a syntax error at the current token.
 */
error_current :: proc(p: ^Parser, msg: string) -> ! {
    error_at(p, p.token, msg)
}

error_at :: proc(p: ^Parser, t: Token, msg: string) -> ! {
    debug_syntax_error(&p.lexer, t, msg)
}

/*
**Grammar**
```
<statement> ::= 'do' <block> 'end'
    | <if_statement>
    | <local_statement>
    | <return_statement>
    | <while_statement>
    | <identifier_statement>
```
 */
statement :: proc(p: ^Parser, c: ^Compiler)  {
    advance_token(p)
    #partial switch peek_consumed(p) {
    case .Break:      break_statement(p, c)
    case .Do:         block(p, c); expect_token(p, .End)
    case .If:         if_statement(p, c)
    case .Local:      local_statement(p, c)
    case .Return:     return_statement(p, c)
    case .While:      while_statement(p, c)
    case .Identifier: identifier_statement(p, c)
    case:
        parser_error(p, "Expected a statement")
    }
    match_token(p, .Semicolon)
}

/*
**Grammar**
```
<block> ::= ( <statement> )*
```

**Note(2026-01-27)**
- This procedure eagerly stops at any 'block-ending' token.
- That is, it does not check for correctness if 'end' or '<eof>' is encountered.
- It is up to the caller to expect and consume the correct ending token.
 */
block :: proc(p: ^Parser, c: ^Compiler) {
    // Breakable blocks must only exist outside of this call because
    // there may be context for break lists.
    b: Block = ---
    compiler_push_block(c, &b, breakable=false)
    for block_continue(p) {
        statement(p, c)
        // Ensure all temporary registers were popped.
        c.free_reg = c.active_count
    }

    // Ensure we pop in order.
    compiler_assert(c, c.block == &b && !c.block.breakable)
    compiler_assert(c, c.block.break_list == NO_JUMP)
    compiler_pop_block(c)
}


/*
**Grammar**
```
<local_statement> ::= 'local' <identifier_list> ( '=' <expression_list> )?
<identifier_list> ::= <identifier> ( ',' <identifier> )*
```

**Assumptions**
- We just consumed the `local` token.
 */
local_statement :: proc(p: ^Parser, c: ^Compiler) {
    lhs_count: u16
    for {
        expect_token(p, .Identifier)
        compiler_declare_local(c, p.consumed.string, lhs_count)
        lhs_count += 1

        if !match_token(p, .Comma) {
            break
        }
    }

    rhs_last:  Expr
    rhs_count: u16
    if match_token(p, .Assign) {
        rhs_last, rhs_count = expression_list(p, c)
    }

    adjust_assign(c, lhs_count, &rhs_last, rhs_count)
    compiler_define_locals(c, lhs_count)
}

adjust_assign :: proc(c: ^Compiler, lhs_count: u16, rhs_last: ^Expr, rhs_count: u16) {
    extra := int(lhs_count) - int(rhs_count)
    if rhs_last.type == .Call {
        // Concept check:
        // local x       = f()              -- #lhs=1, #rhs=1
        // local x, y    = f()              -- #lhs=2, #rhs=1
        // local x, y, z = 1, f()           -- #lhs=3, #rhs=2
        // local x, y, z = 1, 2, 3, f()     -- #lhs=3, #rhs-4
        extra = max(extra + 1, 0)

        compiler_set_returns(c, rhs_last, u16(extra))
        if extra > 1 {
            // Subtract one because the function itself already reserved its
            // register, so we only need to push the remaining assignments.
            compiler_push_reg(c, u16(extra - 1))
        }
        return
    }

    if rhs_last.type != nil {
        compiler_push_expr_next(c, rhs_last)
    }

    if extra > 0 {
        // `local x, y, z = 1, 2`
        reg := compiler_push_reg(c, u16(extra))
        compiler_load_nil(c, reg, u16(extra))
    }
}

Assign_List :: struct {
    variable: Expr,
    prev:    ^Assign_List,
}

/*
**Grammar**
```
<identifier_statement> ::= <assignment>
    | <variable_call>
```

**Assumptions**
- We just consumed an `<identifier>` token.
 */
identifier_statement :: proc(p: ^Parser, c: ^Compiler) {
    var := variable_expression(p, c)
    if var.type == .Call {
        compiler_discharge_returns(c, &var, 0)
        compiler_pop_reg(c, var.reg)
    } else {
        head := Assign_List{variable=var, prev=nil}
        assignment(p, c, &head, 1)
    }
}

/*
**Grammar**
```
<assignment> ::= <lvalue> ( ',' <lvalue> )* '=' <expression_list>
<lvalue>     ::= <identifier>
    | <lvalue> '.' <identifier>
    | <lvalue> '[' <expression> ']'
```
 */
assignment :: proc(p: ^Parser, c: ^Compiler, tail: ^Assign_List, lhs_count: u16) {
    VALID_TARGETS :: bit_set[Expr_Type]{.Global, .Local, .Table}

    if tail.variable.type not_in VALID_TARGETS {
        parser_error(p, "Invalid assignment target")
    }

    // Recursive case: <lvalue> ( ',' <lvalue> )*
    if match_token(p, .Comma) {
        // Link a new assignment target using the recursive stack.
        expect_token(p, .Identifier)
        next := Assign_List{variable=variable_expression(p, c), prev=tail}
        assignment(p, c, &next, lhs_count + 1)
        return
    }

    // Base case: <lvalue_list> '=' <expression_list>
    expect_token(p, .Assign)

    rhs_last, rhs_count := expression_list(p, c)
    node := tail

    if lhs_count != rhs_count {
        // For the last function call, don't truncate it and don't use
        // `compiler_set_variable()`. Since we know how many return values
        // are to be used we can just use them to assign.
        adjust_assign(c, lhs_count, &rhs_last, rhs_count)
        if lhs_count < rhs_count {
            extra := rhs_count - lhs_count
            compiler_pop_reg(c, c.free_reg - extra, extra)
        }
    } else {
        // We can afford to truncate the last function call.
        compiler_discharge_returns(c, &rhs_last, 1)
        compiler_set_variable(c, &tail.variable, &rhs_last)
        node = node.prev
    }

    src := expr_make_reg(.Register, c.free_reg - 1)
    for ; node != nil; node = node.prev {
        compiler_set_variable(c, &node.variable, &src)
        src.reg -= 1
    }
}

break_statement :: proc(p: ^Parser, c: ^Compiler) {
    for b := c.block; b != nil; b = b.prev {
        if b.breakable {
            compiler_add_jump_list(c, &b.break_list)
            return
        }
    }
    parser_error(p, "No block to 'break' at")
}

/*
**Grammar**
```
<return_statement> ::= 'return' ( <expression> ( ',' <expression> )* )?
```
 */
return_statement :: proc(p: ^Parser, c: ^Compiler) {
    if !block_continue(p) {
        compiler_code_return(c, 0, 0)
        return
    }

    first_reg := c.free_reg
    last, arg_count := expression_list(p, c)
    if last.type != .Call && arg_count == 1 {
        last_reg := compiler_push_expr_any(c, &last)
        compiler_code_return(c, last_reg, arg_count)
        compiler_pop_reg(c, last_reg)
    } else {
        ret_count := arg_count
        if last.type != .Call {
            last_reg := compiler_push_expr_next(c, &last)
            parser_assert(p, first_reg == last_reg - arg_count + 1)
        } else {
            ret_count = u16(VARIADIC)
            compiler_discharge_returns(c, &last, ret_count)
            parser_assert(p, first_reg == last.reg - arg_count + 1)
        }
        compiler_pop_reg(c, first_reg, arg_count)
        compiler_code_return(c, first_reg, ret_count)
    }
}

/*
**Grammar**
```
<if_statement> ::= 'if' <then_block> ( 'elseif' <then_block> )* ( 'else' <block> )? 'end'
<then_block> ::= <expression> 'then' <block>
```
 */
if_statement :: proc(p: ^Parser, c: ^Compiler) {
    // Absolute pc of `.Jump_If_False` and the absolute target pc for
    // said jump when 'if' fails.
    then_jump, then_target := then_block(p, c)

    // Jump list. Absolute pc of the current `.Jump`, if any 'else' block
    // (even implicit) exists.
    else_list := NO_JUMP

    for match_token(p, .Elseif) {
        // 'if' and all its sister 'elseif' blocks share the same 'else' jump.
        compiler_add_jump_list(c, &else_list)

        // Current 'then' jump will go to the 'elseif' condition upon failure.
        // This ensures that we try 'elseif' blocks sequentially.
        compiler_patch_jump(c, then_jump, else_list + 1)
        then_jump, then_target = then_block(p, c)
    }

    // Note(2026-01-27)
    //  - If we just had a `break`, we have no way of knowing that an explicit
    //  jump over the else block is actually unnecessary.
    //
    // Concept check:
    // ```
    // while x do
    //  print("loop")
    //  if x then
    //      print("break")
    //      break
    //      -- unnecessary jump here!
    //  else
    //      print("continue")
    //  end
    // end
    // ```
    if match_token(p, .Else) {
        // We want the 'if' or last 'elseif' to jump over, hence + 1.
        then_target = compiler_add_jump_list(c, &else_list) + 1
        block(p, c)
    }

    expect_token(p, .End, "to close 'if' statement")
    compiler_patch_jump(c, then_jump, then_target)
    compiler_patch_jump_list(c, else_list)
}

/*
**Grammar**
```
<then_block> ::= ( 'if' | 'elseif' ) <expression> 'then' <block>
```
 */
then_block :: proc(p: ^Parser, c: ^Compiler) -> (jump, target: i32) {
    is_if := p.consumed.type == .If
    jump = condition(p, c)
    expect_token(p, .Then, "after 'if' condition" if is_if else "after 'elseif' condition")
    block(p, c)
    return jump, compiler_get_target(c)
}

condition :: proc(p: ^Parser, c: ^Compiler) -> (jump: i32) {
    expr := expression(p, c)

    // Conditions that are known to always be true can be omitted so that
    // the block is executed unconditionally.
    if expr_is_truthy(&expr) {
        return NO_JUMP
    }

    if expr.type != .Compare {
        reg := compiler_push_expr_any(c, &expr)
        compiler_pop_reg(c, reg)
        return compiler_code_jump(c, .Jump_Not, reg)
    }
    return compiler_code_jump(c, .Jump, 0)
}

/*
**Grammar**
```
<while_statement> ::= 'while' <expression> 'do' <block> 'end'
```
 */
while_statement :: proc(p: ^Parser, c: ^Compiler) {
    // Absolute pc of the `.Jump_If`, or the first block pc if always true.
    loop_start := compiler_get_target(c)
    jump_false := condition(p, c)
    expect_token(p, .Do, "after 'while' condition")

    // Must be outside of `block()` so that we can patch the break list
    // AFTER the unconditional jump has been emitted.
    b: Block = ---
    compiler_push_block(c, &b, breakable=true)
    block(p, c)
    expect_token(p, .End, "to close 'while' loop")

    jump_true := compiler_code_jump(c, .Jump, 0)
    compiler_patch_jump(c, jump_true, loop_start)

    compiler_pop_block(c)
    compiler_patch_jump(c, jump_false)
}

/*
Parse a comma-separated list of expressions. All expressions, except the
last (and possible only) are pushed to the next free register.

**Grammar**
```
<expression_list> ::= <expression> ( ',' <expression> )*
```

**Returns**
- last: The last, unpushed expression.
- count: How many expressions were parsed in total.
 */
expression_list :: proc(p: ^Parser, c: ^Compiler) -> (last: Expr, count: u16) {
    for {
        expr  := expression(p, c)
        count += 1
        if !match_token(p, .Comma) {
            return expr, count
        }
        compiler_push_expr_next(c, &expr)
    }
    return {}, count
}

/*
Parse an expression using a Depth-First-Search (DFS). The 'parse tree' is
constructed and traversed entirely on the native stack. We do not ever work
with a complete parse tree- nodes are discarded along with their associated
stack frames.

**Parameters**
- prec: Parent caller expression precedence. Useful to enforce operator
precedence and left/right associativity.

**Returns**
- e: An expression with a pending register for the result.

**Assumptions**
- Register allocation of `e` is the caller's responsibility.
 */
expression :: proc(p: ^Parser, c: ^Compiler, prec: Precedence = nil) -> Expr {
    advance_token(p)
    left := prefix(p, c)
    infix(p, c, &left, prec)
    return left
}

// PREFIX EXPRESSIONS ====================================================== {{{


prefix :: proc(p: ^Parser, c: ^Compiler) -> (expr: Expr) {
    // The 3 unary expressions are parsed in the exact same ways.
    unary :: proc(p: ^Parser, c: ^Compiler, op: Opcode) -> (expr: Expr) {
        expr = expression(p, c, .Unary)
        compiler_code_unary(c, op, &expr)
        return expr
    }

    #partial switch peek_consumed(p) {
    // Grouping
    case .Paren_Open:
        expr = expression(p, c)
        expect_token(p, .Paren_Close, "after expression")
        return expr

    case .Curly_Open: return constructor(p, c)

    // Unary
    case .Not:      return unary(p, c, .Not)
    case .Minus:    return unary(p, c, .Unm)
    case .Sharp:    return unary(p, c, .Len)

    // Literal values
    case .Nil:      return expr_make_nil()
    case .False:    return expr_make_boolean(false)
    case .True:     return expr_make_boolean(true)
    case .Number:   return expr_make_number(p.consumed.number)
    case .String:   return string_expression(p, c)

    case .Identifier: return variable_expression(p, c)
    case:
        parser_error(p, "Expected an expression")
    }
}

string_expression :: proc(p: ^Parser, c: ^Compiler) -> Expr {
    VALID_STRINGS :: bit_set[Token_Type]{.String, .Identifier}
    parser_assert(p, p.consumed.type in VALID_STRINGS)

    index := compiler_add_string(c, p.consumed.string)
    return expr_make_index(.Constant, index)
}

/*
**Grammar**
```
<variable_expression> ::= <lvalue>
    | <variable_call>

<variable_call> ::= <lvalue> <function_call>

<lvalue> ::= <identifier>
    | <lvalue> '.' <identifier>
    | <lvalue> '[' <expression> ']'
    | <lvalue> ':' <identifier>
```

**Assumptions**
- We just consumed the `<identifier>` token.
- The `<identifier>` tokoen is potentially the start of an index expression or
a function call expression.
 */
variable_expression :: proc(p: ^Parser, c: ^Compiler) -> (var: Expr) {
    parser_assert(p, p.consumed.type == .Identifier)

    name := p.consumed.string
    reg, scope := compiler_resolve_local(c, name)
    if scope == SCOPE_GLOBAL {
        var = expr_make_index(.Global, compiler_add_string(c, name))
    } else {
        var = expr_make_reg(.Local, reg)
    }

    for {
        #partial switch peek_token(p) {
        case .Paren_Open, .String, .Curly_Open:
            function_call(p, c, &var)

        case .Period:
            advance_token(p)
            expect_token(p, .Identifier)
            key := string_expression(p, c)
            compiler_get_table(c, &var, &key)

        case .Bracket_Open:
            advance_token(p)
            key := expression(p, c)
            expect_token(p, .Bracket_Close)
            compiler_get_table(c, &var, &key)

        case .Colon:
            advance_token(p)
            expect_token(p, .Identifier)
            parser_error(p, "'self' syntax not yet supported")

        case:
            return var
        }
    }
    unreachable()
}


/*
**Grammar**
```
<function_call> ::= '(' <expression_list>? ')'
    | <string_expression>
    | <constructor>
```

**Assumptions**
- The expression needed to resolve the function itself resides in `func`.
- The `'('`, `<string>` or `'{'` token is about to be consumed.
 */
function_call :: proc(p: ^Parser, c: ^Compiler, func: ^Expr) {
    arg_count:   u16
    is_variadic: bool

    ret_count := u16(1)
    call_reg  := compiler_push_expr_next(c, func)

    if p.consumed.line != p.token.line {
        error_current(p, "Ambiguous function call")
    }
    advance_token(p)
    #partial switch t := peek_consumed(p); t {
    case .Paren_Open:
        if !check_token(p, .Paren_Close) {
            arg_last: Expr
            arg_last, arg_count = expression_list(p, c)
            is_variadic = arg_last.type == .Call
            if is_variadic {
                compiler_set_returns(c, &arg_last, u16(VARIADIC))
            } else {
                compiler_push_expr_next(c, &arg_last)
            }
        }
        expect_token(p, .Paren_Close)

    case .String, .Curly_Open:
        arg := string_expression(p, c) if t == .String else constructor(p, c)
        compiler_push_expr_next(c, &arg)
        arg_count = 1

    case:
        unreachable()
    }

    // Pop all registers used for the arguments.
    // Do not, however, pop the function itself yet.
    compiler_pop_reg(c, call_reg + 1, arg_count)
    if is_variadic {
        arg_count = u16(VARIADIC)
    }

    // Default case is 1 return.
    arg_count -= u16(VARIADIC)
    ret_count -= u16(VARIADIC)
    pc   := compiler_code_ABC(c, .Call, call_reg, arg_count, ret_count)
    func^ = expr_make_pc(.Call, pc)
}

/*
**Grammar**
```
<constructor> ::= '{' ( <element> ( ',' <element> )* ) '}'

<element> ::= <key> '=' <expression>
    | <expression>

<key> ::= <identifier>
    | '[' <expression> ']'
```
 */
constructor :: proc(p: ^Parser, c: ^Compiler) -> (expr: Expr) {
    Constructor :: struct {
        // Counters for the table's hash and array segments, respectively.
        hash_count, array_count: int,

        // Index of the `.New_Table` instruction.
        pc: i32,

        // R[A] of the destination table in the `.New_Table` instruction.
        reg: u16,
    }

    set_field :: proc(p: ^Parser, c: ^Compiler, ctor: ^Constructor, key: ^Expr) {
        // Explicit field also works as a continuous array index?
        if key.type == .Number && key.number == f64(ctor.array_count + 1) {
            set_array(p, c, ctor)
        } else {
            value := expression(p, c)
            compiler_set_table(c, ctor.reg, key, &value)
            ctor.hash_count += 1
        }
    }

    set_array :: proc(p: ^Parser, c: ^Compiler, ctor: ^Constructor) {
        key   := expr_make_number(f64(ctor.array_count + 1))
        value := expression(p, c)
        compiler_set_table(c, ctor.reg, &key, &value)
        ctor.array_count += 1
    }

    ctor: Constructor
    // Use a temporary register so set instructions know where to look.
    ctor.reg = compiler_push_reg(c)
    ctor.pc  = compiler_code_ABC(c, .New_Table, ctor.reg, 0, 0)
    expr     = expr_make_pc(.Pc_Pending_Register, ctor.pc)

    for !check_token(p, .Curly_Close) {
        // Don't advance immediately because expressions representing array
        // elements require their first token to be in the lookahead.
        #partial switch peek_token(p) {
        case .Identifier:
            if consume_lookahead(p) == .Assign {
                // skip whatever was before <identifier>.
                advance_token(p)
                key := string_expression(p, c)

                // skip <identifier>. itself.
                advance_token(p)
                set_field(p, c, &ctor, &key)
            } else {
                set_array(p, c, &ctor)
            }

        case .Bracket_Open:
            advance_token(p)
            key := expression(p, c)
            expect_token(p, .Bracket_Close)
            expect_token(p, .Assign)
            set_field(p, c, &ctor, &key)

        case:
            set_array(p, c, &ctor)
        }

        if !match_token(p, .Comma) {
            break
        }
    }
    expect_token(p, .Curly_Close)
    // Pop the temporary table register.
    compiler_pop_reg(c, ctor.reg)

    ip := &c.chunk.code[ctor.pc]
    if ctor.hash_count > 0 {
        ip.B = u16(table_log2(ctor.hash_count) + 1)
    }

    if ctor.array_count > 0 {
        ip.C = u16(table_log2(ctor.array_count) + 1)
    }
    return expr
}



// === }}} =====================================================================
// === INFIX EXPRESSIONS =================================================== {{{


infix :: proc(p: ^Parser, c: ^Compiler, left: ^Expr, prec: Precedence = nil) {
    for {
        rule  := get_rule(p.token.type)
        binop := rule.binop
        // No binary operation OR parent caller is of a higher precedence?
        if binop == nil || prec > rule.left {
            break
        }

        // Don't advance here, we need the correct line/col info for `left`.
        #partial switch binop {
        case .Add..=.Pow: arith(p, c, binop, left, rule.right)
        case .Neq..=.Leq: compare(p, c, binop, left, rule.right)
        case .Concat:     _concat(p, c, left, rule.right)
        case:
            unreachable("Invalid binop %v", binop)
        }
    }
}

get_rule  :: proc(type: Token_Type) -> Rule {
    left :: #force_inline proc(binop: Binop, prec: Precedence) -> Rule {
        return Rule{binop, prec, prec + Precedence(1)}
    }

    right :: #force_inline proc(binop: Binop, prec: Precedence) -> Rule {
        return Rule{binop, prec, prec}
    }

    #partial switch type {
    // Arithmetic
    case .Plus:          return left(.Add, .Terminal)
    case .Minus:         return left(.Sub, .Terminal)
    case .Asterisk:      return left(.Mul, .Factor)
    case .Slash:         return left(.Div, .Factor)
    case .Percent:       return left(.Mod, .Factor)
    case .Caret:         return right(.Pow, .Exponent)

    // Comparison
    case .Not_Equal:     return left(.Neq,  .Equality)
    case .Equal_To:      return left(.Eq,   .Equality)
    case .Greater_Than:  return left(.Gt,   .Comparison)
    case .Less_Than:     return left(.Lt,   .Comparison)
    case .Greater_Equal: return left(.Geq,  .Comparison)
    case .Less_Equal:    return left(.Leq,  .Comparison)

    // Misc.
    case .Ellipsis2:     return right(.Concat, .Concat)
    }
    return Rule{}
}

/*
**Parameters**
- prec: Parent expression right-hand-side precedence.
 */
arith :: proc(p: ^Parser, c: ^Compiler, binop: Binop, left: ^Expr, prec: Precedence) {
    // If we absolutely cannot fold this, then left MUST be pushed BEFORE
    // parsing the right side to ensure correct register ordering and thus
    // correct operations. We cannot assume that `a op b` is equal to `b op a`
    // for all operations.
    if left.type != .Number {
        compiler_push_expr_any(c, left)
    }

    // Advance only now so that the above push has the correct line/col info.
    advance_token(p)
    right := expression(p, c, prec)
    compiler_code_arith(c, binop, left, &right)
}

compare :: proc(p: ^Parser, c: ^Compiler, binop: Binop, left: ^Expr, prec: Precedence) {
    if left.type != .Number {
        compiler_push_expr_any(c, left)
    }
    advance_token(p)
    right := expression(p, c, prec)
    compiler_code_compare(c, binop, left, &right)
}

_concat :: proc(p: ^Parser, c: ^Compiler, left: ^Expr, prec: Precedence) {
    compiler_push_expr_next(c, left)

    // Advance only now so that the above push has the correct line/col info.
    advance_token(p)
    right := expression(p, c, prec)
    compiler_code_concat(c, left, &right)
}

// === }}} =====================================================================

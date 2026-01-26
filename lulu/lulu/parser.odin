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
    lookahead: Token,

    // Token stream. The lookahead(2) token is used only for table constructors
    // in order to differentiate key-value pairs and identifiers in expressions.
    lookahead2: Token,
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
    op: Opcode,

    // Left-hand-side precedence. Helps determine when we should terminate
    // recursion in the face of higher-precedence parent expressions.
    left:  Precedence,

    // Right-hand-side precedence. Helps determine when we should recursively
    // parse higher-precedence child expressions.
    right: Precedence,
}

parser_make :: proc(L: ^State, builder: ^strings.Builder, name: ^Ostring, input: string) -> Parser {
    p := Parser{lexer=lexer_make(L, builder, name, input)}
    advance_token(&p)
    return p
}

@(private="package")
program :: proc(L: ^State, builder: ^strings.Builder, name: ^Ostring, input: string) {
    chunk := chunk_new(L, name)
    // Ensure `chunk` cannot be collected.
    vm_push(L, value_make(cast(^Object)chunk, .Chunk))

    p := parser_make(L, builder, name, input)
    c := compiler_make(L, &p, chunk)

    // Block for file scope (outermost scope).
    block: Block
    compiler_push_block(&c, &block)

    for !check_token(&p, .EOF) {
        statement(&p, &c)
        // Ensure all temporary registers were popped.
        c.free_reg = c.active_count
    }

    expect_token(&p, .EOF)
    compiler_pop_block(&c)
    compiler_end(&c)

    // Make the closure BEFORE popping the chunk in order to prevent the chunk
    // from being collected in case GC is run during the closure's creation.
    cl := closure_lua_new(L, chunk, 0)
    vm_pop(L)
    vm_push(L, cl)
}

/*
Unconditionally consumes a new token.

**Guarantees**
- The old consumed token is thrown away.
- The old lookahead is now the new consumed token.
- The next scanned token is now the new lookahead token if there is no
lookahead(2) token to discharge. Otherwise, the lookahead(2) token is used
as the new primary lookahead and the lookahead(2) token is thrown away.
 */
advance_token :: proc(p: ^Parser) {
    next: Token
    if p.lookahead2.type == nil {
        next = lexer_scan_token(&p.lexer)
    } else {
        next = p.lookahead2
        p.lookahead2 = {}
    }

    if next.type == nil {
        error_lookahead(p, "Unexpected token")
    }

    p.consumed  = p.lookahead
    p.lookahead = next
}

/*
**Returns**
- type: The token type of the lookahead(2) token.
 */
consume_lookahead2 :: proc(p: ^Parser) -> (type: Token_Type) {
    assert(p.lookahead2.type == nil)
    next := lexer_scan_token(&p.lexer)
    if next.type == nil {
        error_lookahead(p, "Unexpected token")
    }
    p.lookahead2 = next
    return next.type
}

peek_consumed :: proc(p: ^Parser) -> Token_Type {
    return p.consumed.type
}

peek_lookahead :: proc(p: ^Parser) -> Token_Type {
    return p.lookahead.type
}

expect_token :: proc(p: ^Parser, want: Token_Type, info := "") {
    if !match_token(p, want) {
        buf: [256]byte
        msg: string
        what := token_string(want)
        if info == "" {
            msg = fmt.bprintf(buf[:], "Expected '%s'", what)
        } else {
            msg = fmt.bprintf(buf[:], "Expected '%s' %s", what, info)
        }
        error_lookahead(p, msg)
    }
}

check_token :: proc(p: ^Parser, want: Token_Type) -> (found: bool) {
    return p.lookahead.type == want
}

/*
Consumes the lookahead token iff it matches `want`.

**Returns**
- `true` if the lookahead token matched `want`, else `false`.
 */
match_token :: proc(p: ^Parser, want: Token_Type) -> (found: bool) {
    found = p.lookahead.type == want
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
block_end :: proc(p: ^Parser) -> bool {
    #partial switch p.lookahead.type {
    case .Else, .Elseif, .End, .EOF: return true
    case:
        break
    }
    return false
}

/*
Throws a syntax error at the consumed token.
 */
@(private="package")
parser_error :: proc(p: ^Parser, msg: string) -> ! {
    error_at(p, p.consumed, msg)
}

/*
Throws a syntax error at the lookahead token.
 */
error_lookahead :: proc(p: ^Parser, msg: string) -> ! {
    error_at(p, p.lookahead, msg)
}

error_at :: proc(p: ^Parser, t: Token, msg: string) -> ! {
    here := t
    if len(here.lexeme) == 0 {
        here.lexeme = token_string(t.type)
    }
    debug_syntax_error(&p.lexer, here, msg)
}

/*
**Grammar**
```
<statement> ::= 'do' <block> 'end'
      | <local_statement>
      | <identifier_statement>
      | <return_statement>
```
 */
statement :: proc(p: ^Parser, c: ^Compiler)  {
    advance_token(p)
    #partial switch peek_consumed(p) {
    case .Do:
        block(p, c)
        expect_token(p, .End)
    case .If:         if_statement(p, c)
    case .Local:      local_statement(p, c)
    case .Return:     return_statement(p, c)
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

**Assumptions**
- We just consumed the `do` token.
 */
block :: proc(p: ^Parser, c: ^Compiler) {
    curr_block: Block
    compiler_push_block(c, &curr_block)
    for !block_end(p) {
        statement(p, c)
    }
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

/*
**Grammar**
```
<return_statement> ::= 'return' ( <expression> ( ',' <expression> )* )?
```
 */
return_statement :: proc(p: ^Parser, c: ^Compiler) {
    if block_end(p) {
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
            assert(first_reg == last_reg - arg_count + 1)
        } else {
            ret_count = u16(VARIADIC)
            compiler_discharge_returns(c, &last, ret_count)
            assert(first_reg == last.reg - arg_count + 1)
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
    // Absolute pc of `.Jump_If_False`.
    then_pc := then_block(p, c)

    // Absolute target pc for `.Jump_If_False` when 'if' fails.
    then_target := c.pc - 1

    // Absolute pc of `.Jump`, if any 'else' block (even implicit) exists.
    else_list := NO_JUMP
    for match_token(p, .Elseif) {
        // All 'elseif' blocks share the same 'else' jump (even implicit).
        compiler_add_jump_list(c, &else_list)

        // Absolute target pc for `.Jump_If_False` when 'elseif' fails.
        elseif_pc := then_block(p, c)

        // Current 'then' jump will go to the new 'else' jump upon
        // failure. This ensures that we try 'elseif' blocks sequentially.
        compiler_patch_jump(c, then_pc, else_list)

        // Next jump to be discharged.
        then_pc       = elseif_pc
        then_target   = c.pc - 1
    }

    if match_token(p, .Else) {
        then_target = compiler_add_jump_list(c, &else_list)
        block(p, c)
    }

    expect_token(p, .End)
    compiler_patch_jump(c, then_pc, then_target)
    compiler_patch_jump_list(c, else_list, c.pc - 1)
}

/*
**Grammar**
```
<then_block> ::= ( 'if' | 'elseif' ) <expression> 'then' <block>
```
 */
then_block :: proc(p: ^Parser, c: ^Compiler) -> (then_pc: i32) {
    cond := expression(p, c)
    expect_token(p, .Then, "after <condition>")

    reg := compiler_push_expr_any(c, &cond)
    compiler_pop_reg(c, reg)

    then_pc = compiler_code_jump(c, .Jump_If, reg)
    block(p, c)
    return then_pc
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


prefix :: proc(p: ^Parser, c: ^Compiler) -> (e: Expr) {
    // The 3 unary expressions are parsed in the exact same ways.
    unary :: proc(p: ^Parser, c: ^Compiler, op: Opcode) -> (e: Expr) {
        e = expression(p, c, .Unary)
        compiler_code_unary(c, op, &e)
        return e
    }

    #partial switch peek_consumed(p) {
    // Grouping
    case .Paren_Open:
        e = expression(p, c)
        expect_token(p, .Paren_Close, "after expression")
        return e

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
    assert(p.consumed.type in VALID_STRINGS)

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
    assert(p.consumed.type == .Identifier)

    name := p.consumed.string
    reg, scope := compiler_resolve_local(c, name)
    if scope == SCOPE_GLOBAL {
        var = expr_make_index(.Global, compiler_add_string(c, name))
    } else {
        var = expr_make_reg(.Local, reg)
    }

    for {
        #partial switch peek_lookahead(p) {
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
    advance_token(p)
    #partial switch peek_consumed(p) {
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

    case .String:
        arg := string_expression(p, c)
        compiler_push_expr_next(c, &arg)
        arg_count = 1

    case .Curly_Open:
        arg := constructor(p, c)
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

Constructor :: struct {
    // Counters for the table's hash and array segments, respectively.
    hash_count, array_count: int,

    // Index of the `.New_Table` instruction.
    pc: i32,

    // R[A] of the destination table in the `.New_Table` instruction.
    reg: u16,
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
constructor :: proc(p: ^Parser, c: ^Compiler) -> (e: Expr) {
    ct: Constructor
    // Use a temporary register so set instructions know where to look.
    ct.reg  = compiler_push_reg(c)
    ct.pc   = compiler_code_ABC(c, .New_Table, ct.reg, 0, 0)
    e       = expr_make_pc(.Pc_Pending_Register, ct.pc)

    for !check_token(p, .Curly_Close) {
        // Don't advance immediately because expressions representing array
        // elements require their first token to be in the lookahead.
        #partial switch peek_lookahead(p) {
        case .Identifier:
            if consume_lookahead2(p) == .Assign {
                // skip whatever was before <identifier>.
                //
                //  consumed:  <identifier>
                //  lookahead: '='
                advance_token(p)
                key := string_expression(p, c)

                // skip <identifier>. itself.
                //
                //  consumed:   '='
                //  lookahead:  <prefix>
                advance_token(p)
                _set_field(p, c, &ct, &key)
            } else {
                // consumed:   ?
                // lookahead:  <identifier>
                // lookahead2: <infix>
                _set_array(p, c, &ct)
            }

        case .Bracket_Open:
            advance_token(p)
            key := expression(p, c)
            expect_token(p, .Bracket_Close)
            expect_token(p, .Assign)
            _set_field(p, c, &ct, &key)

        case:
            _set_array(p, c, &ct)
        }

        if !match_token(p, .Comma) {
            break
        }
    }
    expect_token(p, .Curly_Close)
    // Pop the temporary table register.
    compiler_pop_reg(c, ct.reg)

    ip := &c.chunk.code[ct.pc]
    if ct.hash_count > 0 {
        ip.B = u16(table_log2(ct.hash_count) + 1)
    }

    if ct.array_count > 0 {
        ip.C = u16(table_log2(ct.array_count) + 1)
    }
    return e
}

_set_field :: proc(p: ^Parser, c: ^Compiler, ct: ^Constructor, key: ^Expr) {
    // Explicit field also works as a continuous array index?
    if key.type == .Number && key.number == f64(ct.array_count + 1) {
        _set_array(p, c, ct)
    } else {
        value := expression(p, c)
        compiler_set_table(c, ct.reg, key, &value)
        ct.hash_count += 1
    }
}

_set_array :: proc(p: ^Parser, c: ^Compiler, ct: ^Constructor) {
    key   := expr_make_number(f64(ct.array_count + 1))
    value := expression(p, c)
    compiler_set_table(c, ct.reg, &key, &value)
    ct.array_count += 1
}



// === }}} =====================================================================
// === INFIX EXPRESSIONS =================================================== {{{


infix :: proc(p: ^Parser, c: ^Compiler, left: ^Expr, prec: Precedence = nil) {
    for {
        rule := get_rule(p.lookahead.type)
        // No binary operation OR parent caller is of a higher precedence?
        if rule.op == nil || prec > rule.left {
            break
        }

        // Don't advance here, we need the correct line/col info for `left`.
        #partial switch rule.op {
        case .Add..=.Pow:
            arith(p, c, rule.op, left, rule.right)
        case .Concat:
            _concat(p, c, left, rule.right)
        case:
            unreachable()
        }
    }
}

get_rule  :: proc(type: Token_Type) -> Rule {
    #partial switch type {
    // right = left + 0: Enforce right-associativity for exponentiation.
    // right = left + 1: Enforce left-associativity for all other operators.
    case .Plus:         return Rule{.Add,    .Terminal, .Factor}
    case .Minus:        return Rule{.Sub,    .Terminal, .Factor}
    case .Asterisk:     return Rule{.Mul,    .Factor,   .Exponent}
    case .Slash:        return Rule{.Div,    .Factor,   .Exponent}
    case .Percent:      return Rule{.Mod,    .Factor,   .Exponent}
    case .Caret:        return Rule{.Pow,    .Exponent, .Exponent}
    case .Ellipsis2:    return Rule{.Concat, .Concat,   .Concat}
    }
    return Rule{}
}

/*
**Parameters**
- prec: Parent expression right-hand-side precedence.
 */
arith :: proc(p: ^Parser, c: ^Compiler, op: Opcode, left: ^Expr, prec: Precedence) {
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
    compiler_code_arith(c, op, left, &right)
}

_concat :: proc(p: ^Parser, c: ^Compiler, left: ^Expr, prec: Precedence) {
    compiler_push_expr_next(c, left)

    // Advance only now so that the above push has the correct line/col info.
    advance_token(p)
    right := expression(p, c, prec)
    compiler_code_concat(c, left, &right)
}

// === }}} =====================================================================

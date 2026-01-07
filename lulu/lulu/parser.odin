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
    // Also helps to "unget" a token.
    lookahead: Token,
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
    vm_push_value(L, value_make_object(cast(^Object)chunk, .Chunk))

    p := parser_make(L, builder, name, input)
    c := compiler_make(L, &p, chunk)

    // Block for file scope (outermost scope).
    block: Block
    compiler_push_block(&c, &block)

    for !check_token(&p, .EOF) {
        statement(&p, &c)
        // Ensure all temporary registers were popped.
        fmt.assertf(c.free_reg == c.active_count,
            "Expected c.free_reg(%i) but got c.free_reg(%i)",
            c.active_count, c.free_reg)
    }

    expect_token(&p, .EOF)
    compiler_pop_block(&c)
    compiler_end(&c)

    // Make the closure BEFORE popping the chunk in order to prevent the chunk
    // from being collected in case GC is run during the closure's creation.
    closure := lua_closure_new(L, chunk, 0)
    vm_pop_value(L)
    vm_push_value(L, value_make_function(closure))
}

/*
Unconditionally consumes a new token.

**Guarantees**
- The old consumed token is thrown away.
- The old lookahead is now the new consumed token.
- The next scanned token is now the new lookahead token.
 */
advance_token :: proc(p: ^Parser) {
    p.consumed, p.lookahead = p.lookahead, lexer_scan_token(&p.lexer)
    if p.lookahead.type == nil {
        error_lookahead(p, "Unexpected token")
    }
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

check_either_token :: proc(p: ^Parser, want1, want2: Token_Type) -> bool {
    return check_token(p, want1) || check_token(p, want2)
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

statement :: proc(p: ^Parser, c: ^Compiler)  {
    #partial switch p.lookahead.type {
    case .Do:
        advance_token(p)
        block(p, c)

    case .Local:
        advance_token(p)
        local_statement(p, c)

    case .Identifier:
        advance_token(p)
        var := resolve_variable(p, c)
        if check_either_token(p, .Comma, .Assign) {
            assignment(p, c, &Assign_List{var=var, prev=nil}, 1)
        } else {
            // If a function call, make it disregard the return values.
            compiler_set_returns(c, &var, 0)
        }
        compiler_pop_expr(c, &var)
    case .Return:
        advance_token(p)
        return_statement(p, c)

    case:
        parser_error(p, "Expected a statement")
    }
    match_token(p, .Semicolon)
}

block :: proc(p: ^Parser, c: ^Compiler) {
    block: Block
    compiler_push_block(c, &block)
    for !check_token(p, .End) {
        statement(p, c)
    }
    expect_token(p, .End)
    compiler_pop_block(c)
}


/*
**Assumptions**
- The currently consumed token is an identifier, which is potentially the
start of an index expression and maybe even a function call.
 */
resolve_variable :: proc(p: ^Parser, c: ^Compiler) -> (var: Expr) {
    assert(p.consumed.type == .Identifier)

    name := p.consumed.string
    if reg, ok := compiler_resolve_local(c, name); ok {
        var = expr_make_reg(.Local, reg)
    } else {
        index := compiler_add_string(c, name)
        var = expr_make_index(.Global, index)
    }

    if check_either_token(p, .Period, .Bracket_Open) {
        parser_error(p, "TODO(2025-01-06) Table access")
    }

    if check_token(p, .Paren_Open) {
        function_call(p, c, &var)
    }
    return var
}

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

    adjust_assign(c, &rhs_last, lhs_count, rhs_count)
    compiler_define_locals(c, lhs_count)
}

adjust_assign :: proc(c: ^Compiler, rhs_last: ^Expr, lhs_count, rhs_count: u16) {
    switch {
    case rhs_last.type == .Call:
        // Subtract 1 from `rhs_count` first. Concept check:
        // local x = f()
        // local x, y = f()
        // local x, y, z = 1, f()
        call_ret := int(lhs_count) - (int(rhs_count) - 1)
        if call_ret > 1 {
            // Subtract one because the function itself already reserved its
            // register, so we only need to push the remaining assignments.
            compiler_push_reg(c, u16(call_ret) - 1)
        }
        compiler_set_returns(c, rhs_last, u16(call_ret))

    case lhs_count == rhs_count:
        compiler_push_expr_next(c, rhs_last)

    case lhs_count < rhs_count:
        // `local x, y = 1, 2, 3`
        extra := rhs_count - lhs_count
        compiler_pop_reg(c, c.free_reg - 1, extra)

    case lhs_count > rhs_count:
        // `local x, y, z = 1, 2`
        extra := lhs_count - rhs_count
        reg   := compiler_push_reg(c, extra)
        compiler_load_nil(c, reg, extra)
    }

}

Assign_List :: struct {
    var:   Expr,
    prev: ^Assign_List,
}

assignment :: proc(p: ^Parser, c: ^Compiler, tail: ^Assign_List, lhs_count: u16) {
    VALID_TARGETS :: bit_set[Expr_Type]{.Global, .Local}

    if tail.var.type not_in VALID_TARGETS {
        parser_error(p, "Invalid assignment target")
    }

    if match_token(p, .Comma) {
        // Link a new assignment target using the recursive stack.
        expect_token(p, .Identifier)
        next := &Assign_List{var=resolve_variable(p, c), prev=tail}
        assignment(p, c, next, lhs_count + 1)
        return
    }
    expect_token(p, .Assign)

    first_reg := c.free_reg
    rhs_last, rhs_count := expression_list(p, c)
    adjust_assign(c, &rhs_last, lhs_count, rhs_count)

    src_reg  := c.free_reg - 1
    for node := tail; node != nil; node = node.prev {
        var := node.var
        #partial switch var.type {
        case .Global: compiler_code_abx(c, .Set_Global, src_reg, var.index)
        case .Local:  compiler_code_abc(c, .Move, var.reg, src_reg, 0)
        case:
            unreachable("Invalid expr to assign: %v", var.type)
        }
        src_reg -= 1
    }
    // assert(first_reg == top_reg, "base=%i, top=%i", first_reg, top_reg)
    c.free_reg = first_reg
}

return_statement :: proc(p: ^Parser, c: ^Compiler) {
    last, count := expression_list(p, c)
    if count == 1 {
        last_reg := compiler_push_expr_any(c, &last)
        compiler_code_return(c, last_reg, count)
        compiler_pop_expr(c, &last)
    } else {
        if last.type == .Call {
            parser_error(p, "Variadic returns not yet supported")
        }
        last_reg  := compiler_push_expr_next(c, &last)
        first_reg := last_reg - count + 1
        compiler_code_return(c, first_reg, count)
        compiler_pop_reg(c, first_reg, count)
    }
}

function_call :: proc(p: ^Parser, c: ^Compiler, func: ^Expr) {
    arg_count: u16
    ret_count := u16(1)
    call_reg  := compiler_push_expr_next(c, func)
    expect_token(p, .Paren_Open)
    if !check_token(p, .Paren_Close) {
        arg_last: Expr
        arg_last, arg_count = expression_list(p, c)
        is_variadic := arg_last.type == .Call
        if is_variadic {
            compiler_set_returns(c, &arg_last, u16(VARIADIC))
        } else {
            compiler_push_expr_next(c, &arg_last)
        }
        // Pop all registers used for the arguments.
        // Do not, however, pop the function itself yet.
        compiler_pop_reg(c, call_reg + 1, arg_count)
        if is_variadic {
            arg_count = u16(VARIADIC)
        }
    }
    expect_token(p, .Paren_Close)

    // Default case is 1 return.
    arg_count -= u16(VARIADIC)
    ret_count -= u16(VARIADIC)
    pc   := compiler_code_abc(c, .Call, call_reg, arg_count, ret_count)
    func^ = expr_make_pc(.Call, pc)
}


/*
Parse a comma-separated list of expressions. All expressions, except the
last (and possible only) are pushed to the next free register.

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

prefix :: proc(p: ^Parser, c: ^Compiler) -> (e: Expr) {
    // The 3 unary expressions are parsed in the exact same ways.
    unary :: proc(p: ^Parser, c: ^Compiler, op: Opcode) -> (e: Expr) {
        e = expression(p, c, .Unary)
        compiler_code_unary(c, op, &e)
        return e
    }

    #partial switch p.consumed.type {
    // Grouping
    case .Paren_Open:
        e = expression(p, c)
        expect_token(p, .Paren_Close, "after expression")
        return e

    // Unary
    case .Not:      return unary(p, c, .Not)
    case .Minus:    return unary(p, c, .Unm)
    case .Sharp:    return unary(p, c, .Len)

    // Literal values
    case .Nil:      return expr_make_nil()
    case .False:    return expr_make_boolean(false)
    case .True:     return expr_make_boolean(true)
    case .Number:   return expr_make_number(p.consumed.number)
    case .String:
        index := compiler_add_string(c, p.consumed.string)
        return expr_make_index(.Constant, index)

    case .Identifier: return resolve_variable(p, c)
    case:
        parser_error(p, "Expected an expression")
    }
}

// INFIX EXPRESSIONS


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
            concat(p, c, left, rule.right)
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

concat :: proc(p: ^Parser, c: ^Compiler, left: ^Expr, prec: Precedence) {
    compiler_push_expr_next(c, left)

    // Advance only now so that the above push has the correct line/col info.
    advance_token(p)
    right := expression(p, c, prec)
    compiler_code_concat(c, left, &right)
}

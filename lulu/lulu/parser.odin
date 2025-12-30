#+private file
package lulu

import "core:strings"
import "core:fmt"

@(private="package")
Parser :: struct {
    // Parent state used to catch errors.
    L: ^VM,

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

@(private="package")
parser_make :: proc(L: ^VM, builder: ^strings.Builder, name, input: string) -> Parser {
    p := Parser{L=L, lexer=lexer_make(L, builder, name, input)}
    advance_token(&p)
    return p
}

@(private="package")
parser_program :: proc(p: ^Parser, c: ^Compiler) {
    e: Expr
    for !check_token(p, .EOF) {
        e = statement(p, c)
        match_token(p, .Semicolon)
    }
    expect_token(p, .EOF)
    compiler_push_expr_any(c, &e, p.consumed.line)
    compiler_end(c, p.consumed.line)
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
    file := p.lexer.name
    line := t.line
    loc  := t.lexeme if len(t.lexeme) > 0 else token_string(t.type)
    fmt.eprintfln("%s:%i: %s near '%s'", file, line, msg, loc)
    vm_error_syntax(p.L)
}

statement :: proc(p: ^Parser, c: ^Compiler) -> (e: Expr) {
    #partial switch p.lookahead.type {
    case .Identifier:
        advance_token(p)
        value := value_make(p.consumed.data.(^Ostring))
        index := compiler_add_constant(c, value)
        expr_set_global(&e, index)
        if check_token(p, .Comma) || check_token(p, .Assign) {
            head := &Assign_List{expr=e, prev=nil}
            e = assignment(p, c, head, 1)
        } else {
           // hack if you want to treat lone variables as expressions
            infix(p, c, &e)
        }
    case:
        e = expression(p, c)
    }
    compiler_pop_expr(c, &e)
    match_token(p, .Semicolon)
    return e
}

Assign_List :: struct {
    expr:  Expr,
    prev: ^Assign_List,
}

/* 
Assignment (single or multiple). Returns the right-most assigned expression
similar to C's comma operator. The comma operator shenanigans is just for
testing, it will be removed.
 */
assignment :: proc(p: ^Parser, c: ^Compiler, tail: ^Assign_List, lhs_count: int) -> Expr {
    if tail.expr.type != .Global {
        parser_error(p, "Invalid assignment target")
    }

    if match_token(p, .Comma) {
        // Link a new assignment target using the recursive stack.
        next := &Assign_List{expr=expression(p, c), prev=tail}
        return assignment(p, c, next, lhs_count + 1)
    }
    expect_token(p, .Assign)
    
    base_reg := c.free_reg
    rhs_count := 1
    for {
        rhs_expr := expression(p, c)
        compiler_push_expr_next(c, &rhs_expr, p.consumed.line)
        if !match_token(p, .Comma) {
            break
        }
        rhs_count += 1
    }
    
    line := p.consumed.line
    if lhs_count != rhs_count {
        // x, y = 1, 2, 3
        if lhs_count < rhs_count {
            c.free_reg -= cast(u16)(rhs_count - lhs_count)
        } // x, y, z = 1, 2
        else {
            extra := cast(u16)(lhs_count - rhs_count)
            compiler_load_nil(c, c.free_reg, extra, line)
            c.free_reg += extra
        }
    }
    
    top_reg  := c.free_reg - 1
    for node := tail; node != nil; node = node.prev {
        compiler_code_abx(c, .Set_Global, top_reg, node.expr.index, line)
        top_reg -= 1
    }
    c.free_reg = base_reg
    return tail.expr
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
        compiler_code_unary(c, op, &e, p.consumed.line)
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
    case .Number:   return expr_make_number(p.consumed.data.(f64))
    case .String:
        value := value_make(p.consumed.data.(^Ostring))
        index := compiler_add_constant(c, value)
        expr_set_constant(&e, index)
        return e

    case .Identifier:
        value := value_make(p.consumed.data.(^Ostring))
        index := compiler_add_constant(c, value)
        expr_set_global(&e, index)
        return e

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

        advance_token(p)
        #partial switch rule.op {
        case .Add..=.Pow:
            arith(p, c, rule.op, left, rule.right)
        case:
            unreachable()
        }
    }
}

get_rule  :: proc(type: Token_Type) -> Rule {
    #partial switch type {
    // right = left + 0: Enforce right-associativity for exponentiation.
    // right = left + 1: Enforce left-associativity for all other operators.
    case .Plus:     return Rule{.Add, .Terminal, .Factor}
    case .Minus:    return Rule{.Sub, .Terminal, .Factor}
    case .Asterisk: return Rule{.Mul, .Factor,   .Exponent}
    case .Slash:    return Rule{.Div, .Factor,   .Exponent}
    case .Percent:  return Rule{.Mod, .Factor,   .Exponent}
    case .Caret:    return Rule{.Pow, .Exponent, .Exponent}
    }
    return Rule{}
}

/*
**Parameters**
- prec: Parent expression right-hand-side precedence.
 */
arith :: proc(p: ^Parser, c: ^Compiler, op: Opcode, left: ^Expr, prec: Precedence) {
    // Left MUST be pushed BEFORE parsing the right side to ensure correct
    // register ordering and thus correct operations. We cannot assume that
    // `a op b` is equal to `b op a` for all operations.
    compiler_push_expr_rk(c, left, p.consumed.line)
    right := expression(p, c, prec)
    compiler_code_arith(c, op, left, &right, p.consumed.line)
}

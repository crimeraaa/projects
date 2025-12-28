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

Parser_Rule :: struct {
    infix:  proc(p: ^Parser, c: ^Compiler, left: ^Expr, prec: Precedence),
    prec:   Precedence,
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

@(private="package")
parser_make :: proc(L: ^VM, builder: ^strings.Builder, name, input: string) -> Parser {
    p := Parser{L=L, lexer=lexer_make(L, builder, name, input)}
    parser_advance(&p)
    return p
}

@(private="package")
parser_parse :: proc(p: ^Parser, c: ^Compiler) {
    e := expression(p, c)
    compiler_push_expr(c, &e, p.consumed.line)
    parser_expect(p, .EOF)
    compiler_end(c, p.consumed.line)
}

/*
Unconditionally consumes a new token.

**Guarantees**
- The old consumed token is thrown away.
- The old lookahead is now the new consumed token.
- The next scanned token is now the new lookahead token.
 */
parser_advance :: proc(p: ^Parser) {
    p.consumed, p.lookahead = p.lookahead, lexer_scan_token(&p.lexer)
    if p.lookahead.type == .Unknown {
        parser_error_lookahead(p, "Unexpected token")
    }
}

parser_expect :: proc(p: ^Parser, want: Token_Type, info := "") {
    if !parser_match(p, want) {
        buf: [256]byte
        msg: string
        what := token_type_string(want)
        if info == "" {
            msg = fmt.bprintf(buf[:], "Expected '%s'", what)
        } else {
            msg = fmt.bprintf(buf[:], "Expected '%s' %s", what, info)
        }
        parser_error_lookahead(p, msg)
    }
}

/*
Consumes the lookahead token iff it matches `want`.

**Returns**
- `true` if the lookahead token matched `want`, else `false`.
 */
parser_match :: proc(p: ^Parser, want: Token_Type) -> (found: bool) {
    found = p.lookahead.type == want
    if found {
        parser_advance(p)
    }
    return found
}

/*
Throws a syntax error at the consumed token.
 */
@(private="package")
parser_error :: proc(p: ^Parser, msg: string) -> ! {
    parser_error_at(p, p.consumed, msg)
}

/*
Throws a syntax error at the lookahead token.
 */
parser_error_lookahead :: proc(p: ^Parser, msg: string) -> ! {
    parser_error_at(p, p.lookahead, msg)
}

parser_error_at :: proc(p: ^Parser, t: Token, msg: string) -> ! {
    file := p.lexer.name
    line := t.line
    loc  := t.lexeme if len(t.lexeme) > 0 else token_type_string(t.type)
    fmt.eprintfln("%s:%i: %s near '%s'", file, line, msg, loc)
    vm_error_syntax(p.L)
}

/*
Parse an expression using a Depth-First-Search (DFS). The 'parse tree' is
constructed and traversed entirely on the native stack. We do not ever work
with a complete parse tree- nodes are discarded along with their associated
stack frames.

**Parameters**
- prec: Mainly useful for parsing sub-expressions.

**Returns**
- e: An expression with a pending register for the result.

**Assumptions**
- Register allocation of `e` is the caller's responsibility.
 */
expression :: proc(p: ^Parser, c: ^Compiler, prec: Precedence = nil) -> Expr {
    parser_advance(p)
    left := prefix(p, c)
    for {
        rule := parser_get_rule(p.lookahead.type)
        // Don't advance yet if break- parent caller will need the token!
        if prec > rule.prec || rule.infix == nil {
            break
        }
        parser_advance(p)
        rule.infix(p, c, &left, rule.prec)
    }
    return left
}

/*
Every expression starts with a prefix expression.
 */
prefix :: proc(p: ^Parser, c: ^Compiler) -> (e: Expr) {
    #partial switch p.consumed.type {
    // Grouping
    case .Paren_Open:
        e = expression(p, c)
        parser_expect(p, .Paren_Close, "after expression")

    // Unary
    case .Not, .Sharp, .Minus:
        op: OpCode
        #partial switch p.consumed.type {
        case .Minus: op = OpCode.Unm
        case .Not:   op = OpCode.Not
        case .Sharp: op = OpCode.Len
        case:
            unreachable()
        }
        e = expression(p, c, .Unary)
        compiler_code_unary(c, op, &e, p.consumed.line)

    // Literal values
    case .False:    expr_set_boolean(&e, false)
    case .True:     expr_set_boolean(&e, true)
    case .Number:   expr_set_number(&e, p.consumed.data.(f64))
    case .String:
        value := value_make(p.consumed.data.(^OString))
        index := compiler_add_constant(c, value)
        expr_set_constant(&e, index)

    case:
        parser_error(p, "Expected an expression")
    }
    return e
}

// INFIX EXPRESSIONS

arith :: proc(p: ^Parser, c: ^Compiler, left: ^Expr, prec: Precedence) {
    // 0: Enforce right-associativity for exponentiation.
    // 1: Enforce left-associativity for all other operators.
    assoc := cast(Precedence)1
    op: OpCode
    #partial switch p.consumed.type {
    case .Plus:     op = .Add
    case .Minus:    op = .Sub
    case .Asterisk: op = .Mul
    case .Slash:    op = .Div
    case .Percent:  op = .Mod
    case .Caret:    op = .Pow; assoc = cast(Precedence)0
    case:
        unreachable()
    }

    // Left MUST be pushed BEFORE parsing the right side to ensure correct
    // register ordering and thus correct operations. We cannot assume that
    // `a op b` is equal to `b op a` for all operations.
    compiler_push_expr(c, left, p.consumed.line)
    right := expression(p, c, prec + assoc)
    compiler_code_arith(c, op, left, &right, p.consumed.line)
}

parser_get_rule :: proc(type: Token_Type) -> Parser_Rule {
    @(static, rodata)
    PARSER_RULES := #partial [Token_Type]Parser_Rule{
        // Arithmetic
        .Plus       = {infix = arith,   prec = .Terminal},
        .Minus      = {infix = arith,   prec = .Terminal},
        .Asterisk   = {infix = arith,   prec = .Factor},
        .Slash      = {infix = arith,   prec = .Factor},
        .Percent    = {infix = arith,   prec = .Factor},
        .Caret      = {infix = arith,   prec = .Exponent},
    }
    return PARSER_RULES[type]
}

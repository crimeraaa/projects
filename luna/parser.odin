#+private file
package luna

// standard
import "core:fmt"
import "core:strings"

@(private="package")
Parser :: struct {
    L:        ^State,
    lexer:    Lexer,
    consumed: Token,
    current:  Token,
}

// A binary expression rule.
Rule :: struct {
    op:    Expr_Op,
    left:  Precedence,
    right: Precedence,
}

Precedence :: enum u8 {
    None,
    Logic_Or,   // or
    Logic_And,  // and
    Equality,   // == ~=
    Comparison, // < <= > >=
    Terminal,   // + - | ~
    Factor,     // * / % &
    Unary,
}

@(private="package")
program :: proc(L: ^State, name: string, r: Reader, b: ^strings.Builder) -> (chunk: Chunk) {
    p: Parser
    p.L     = L
    p.lexer = lexer_make(L, name, r, b)
    // Set the first consumed token.
    parser_advance(&p)

    c := Compiler{parser=&p, chunk=&chunk}
    e := expression(&p, &c)
    parser_expect(&p, .EOF)

    reg := compiler_push_expr_any(&c, &e)
    compiler_code_ABC(&c, .Return, reg, 1, 0)
    return chunk
}

parser_advance :: proc(p: ^Parser) {
    token := lexer_lex(&p.lexer)
    if token.type == nil {
        parser_error(p, token, "Unexpected token")
    }
    p.consumed = p.current
    p.current  = token
}

parser_check :: proc(p: ^Parser, want: Token_Type) -> bool {
    return p.current.type == want
}

parser_match :: proc(p: ^Parser, want: Token_Type) -> bool {
    ok := parser_check(p, want)
    if ok {
        parser_advance(p)
    }
    return ok
}

parser_expect :: proc(p: ^Parser, want: Token_Type, info := "", line := i32(-1)) {
    if !parser_match(p, want) {
        buf: [64]byte
        message := token_type_string(want)
        if info != "" {
            assert(line != -1)
            message = fmt.bprintf(buf[:], "Expected '%s' %s at %i", message, info, line)
        } else {
            message = fmt.bprintf(buf[:], "Expected '%s'", message)
        }
        parser_error(p, p.current, message)
    }
}

@(private="package")
parser_error :: proc(p: ^Parser, t: Token, message: string) -> ! {
    file := p.lexer.name
    here := token_string(t)
    fmt.eprintfln("%s:%i:%i: %s at '%s'", file, t.line, t.col, message, here)
    run_throw_error(p.L, .Syntax)
}

// === EXPRESSION PARSING ================================================== {{{

/*
Parse an expression using Depth-First-Search (DFS).
*/
expression :: proc(p: ^Parser, c: ^Compiler, prec := Precedence.None) -> Expr {
    expr := prefix(p, c)
    for {
        rule := get_rule(p.current.type)
        // No binary rule to speak of OR it's a lower precedence?
        if rule.op == nil || rule.left < prec {
            break
        }

        // Consume the binary operator.
        parser_advance(p)
        binary(p, c, rule.op, &expr, rule.right)
    }
    return expr
}

prefix :: proc(p: ^Parser, c: ^Compiler) -> Expr {
    #partial switch p.current.type {
    case .Paren_Open:
        parser_advance(p)
        line := p.consumed.line
        e    := expression(p, c)
        parser_expect(p, .Paren_Close, "to close '('", line)
        return e
    case .Pound: return unary(p, c, .Len)
    case .Not:   return unary(p, c, .Not)
    case .Dash:  return unary(p, c, .Unm)
    case .Tilde: return unary(p, c, .Bnot)
    case .Nil:   parser_advance(p); return Expr{type=.Nil}
    case .True:  parser_advance(p); return Expr{type=.True}
    case .False: parser_advance(p); return Expr{type=.False}
    case .Number:
        parser_advance(p)
        expr: Expr
        switch value in p.consumed.data {
        case nil: expr.type = .Nil
        case f64: expr.type = .Number;  expr.number  = value
        case int: expr.type = .Integer; expr.integer = value
        case:
            unreachable()
        }
        return expr
    case:
        parser_error(p, p.current, "Expected an expression")
    }
}

unary :: proc(p: ^Parser, c: ^Compiler, op: Expr_Op) -> Expr {
    parser_advance(p)
    arg := expression(p, c, .Unary)
    compiler_code_unary(c, op, &arg)
    return arg
}

binary :: proc(p: ^Parser, c: ^Compiler, op: Expr_Op, left: ^Expr, prec: Precedence) {
    right := expression(p, c)
    compiler_code_binary(c, op, left, &right)
}

get_rule :: proc(type: Token_Type) -> Rule {
    left :: #force_inline proc(op: Expr_Op, prec: Precedence) -> Rule {
        return Rule{op, prec, prec + Precedence(1)}
    }

    // right :: #force_inline proc(op: Expr_Op, prec: Precedence) -> Rule {
    //     return Rule{op, prec, prec}
    // }

    #partial switch type {
    // Comparison
    case .Equal_To:     return left(.Eq,    .Equality)
    case .Not_Equal:    return left(.Neq,   .Equality)
    case .Less_Than:    return left(.Lt,    .Comparison)
    case .Less_Equal:   return left(.Leq,   .Comparison)
    case .Great_Than:   return left(.Gt,    .Comparison)
    case .Great_Equal:  return left(.Geq,   .Comparison)

    // Arithmetic
    case .Plus:         return left(.Add,   .Terminal)
    case .Dash:         return left(.Sub,   .Terminal)
    case .Asterisk:     return left(.Mul,   .Factor)
    case .Slash:        return left(.Div,   .Factor)
    case .Percent:      return left(.Mod,   .Factor)

    // Bitwise
    case .Ampersand:    return left(.Band, .Factor)
    case .Pipe:         return left(.Bor,  .Terminal)
    case .Tilde:        return left(.Bxor, .Terminal)
    case:
        break
    }
    return {}
}

// === }}} =====================================================================

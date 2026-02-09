#+private file
package luna

import "core:fmt"
import "core:strings"
import "core:strconv"
import "core:unicode"
import "core:unicode/utf8"

@(private="package")
Lexer :: struct {
    L:         ^State,
    name:      string,
    reader:    Reader,
    builder:   ^strings.Builder,
    curr_rune: rune,
    curr_size: int,
    line, col: i32,
}

@(private="package")
Token :: struct {
    type:      Token_Type,
    lexeme:    string,
    data:      union {f64, int},
    line, col: i32,
}

@(private="package")
Token_Type :: enum u8 {
    None,

    // Keywords
    And, Break, Do, Else, Elseif, End, False, For, Function, Local, If, In,
    Nil, Not, Or, Repeat, Return, Then, True, Until, While,

    // Balanced Pairs
    Paren_Open, Paren_Close,
    Brace_Open, Brace_Close,
    Curly_Open, Curly_Close,

    // Punctuations
    Period, Ellipsis2, Ellipsis3,
    Comma, Colon, Semicolon,
    Pound, Assign,

    // Comparison Operators
    Equal_To,   Not_Equal,
    Less_Than,  Less_Equal,
    Great_Than, Great_Equal,

    // Arithmetic Operators
    Plus, Dash, Asterisk, Slash, Percent,

    // Bitwise Operators
    Tilde, Ampersand, Pipe, Caret,

    // Misc.
    Identifier, Number, String, EOF,
}

@rodata
TOKEN_TYPE_STRINGS := [Token_Type]string {
    // Default type
    .None = "<none>",

    // Keywords
    .And      = "and",    .Break  = "break",  .Do       = "do",
    .Else     = "else",   .Elseif = "elseif", .End      = "end",
    .False    = "false",  .For    = "for",    .Function = "function",
    .Local    = "local",  .If     = "if",     .In       = "in",
    .Nil      = "nil",    .Not    = "not",    .Or       = "or",
    .Repeat   = "repeat", .Return = "return", .Then     = "then",
    .True     = "true",   .Until  = "until",  .While    = "while",

    // Balanced Pairs
    .Paren_Open = "(", .Paren_Close = ")",
    .Brace_Open = "[", .Brace_Close = "]",
    .Curly_Open = "{", .Curly_Close = "}",

    // Punctuations
    .Period = ".", .Ellipsis2 = "..", .Ellipsis3 = "...",
    .Comma  = ",", .Colon     = ":",  .Semicolon = ";",
    .Pound  = "#", .Assign    = "=",

    // Comparison operators
    .Equal_To   = "==", .Not_Equal   = "~=",
    .Less_Than  = "<",  .Less_Equal  = "<=",
    .Great_Than = ">",  .Great_Equal = ">=",

    // Arithmetic operators
    .Plus = "+", .Dash = "-", .Asterisk = "*", .Slash = "/", .Percent = "*",

    // Bitwise operators
    .Tilde = "~", .Ampersand = "&", .Pipe = "|", .Caret = "^",

    // Misc.
    .Identifier = "<identifier>", .Number = "<number>", .String = "<string>",
    .EOF = "<eof>",

}

@(private="package")
token_string :: proc(t: Token) -> string {
    #partial switch t.type {
    case .None, .Identifier, .Number, .String: return t.lexeme
    case:
        break
    }
    return TOKEN_TYPE_STRINGS[t.type]
}

@(private="package")
token_type_string :: proc(type: Token_Type) -> string {
    return TOKEN_TYPE_STRINGS[type]
}

@(private="package")
lexer_make :: proc(L: ^State, name: string, r: Reader, b: ^strings.Builder) -> Lexer {
    x: Lexer
    x.L       = L
    x.name    = name
    x.reader  = r
    x.builder = b
    x.line    = 1
    lexer_advance(&x)
    return x
}

lexer_error :: proc(x: ^Lexer, message: string) -> ! {
    location := lexer_lexeme(x)
    line     := x.line
    col      := max(x.col - i32(len(location)), x.col)

    if len(location) == 0 {
        location = TOKEN_TYPE_STRINGS[.EOF]
    }

    fmt.eprintfln("%s:%i:%i: %s at '%s'", x.name, line, col, message, location)
    run_throw_error(x.L, .Syntax)
}

lexer_is_eof :: proc(x: ^Lexer) -> bool {
    return x.curr_rune == utf8.RUNE_EOF
}

lexer_current :: proc(x: ^Lexer) -> (rune, int) {
    return x.curr_rune, x.curr_size
}

lexer_lexeme :: proc(x: ^Lexer) -> string {
    return strings.to_string(x.builder^)
}

lexer_advance :: proc(x: ^Lexer) -> rune {
    prev, size := lexer_current(x)
    if prev == utf8.RUNE_ERROR {
        lexer_error(x, "Malformed UTF-8 code point")
    }

    x.col += i32(size)
    x.curr_rune, x.curr_size = reader_get_rune(&x.reader)
    return prev
}

lexer_check :: proc(x: ^Lexer, expected: rune) -> bool {
    return x.curr_rune == expected
}

lexer_match :: proc(x: ^Lexer, expected: rune) -> bool {
    found := lexer_check(x, expected)
    if found {
        lexer_advance(x)
    }
    return found
}

lexer_save :: proc(x: ^Lexer, r: rune, size: int) {
    // Ignore error if the write otherwise succeeded?
    res, _ := strings.write_rune(x.builder, r)
    if res != size {
        buf: [64]byte
        message := fmt.bprintf(buf[:], "Failed to write '%c'", r)
        lexer_error(x, message)
    }
}

lexer_save_advance :: proc(x: ^Lexer) -> rune {
    r, size := lexer_current(x)
    lexer_save(x, r, size)
    return lexer_advance(x)
}

lexer_save_match_rune :: proc(x: ^Lexer, expected: rune) -> bool {
    found := lexer_check(x, expected)
    if found {
        lexer_save_advance(x)
    }
    return found
}

lexer_save_match_either :: proc(x: ^Lexer, expected1, expected2: rune) -> bool {
    found := lexer_check(x, expected1) || lexer_check(x, expected2)
    if found {
        lexer_save_advance(x)
    }
    return found
}

lexer_consume_sequence :: proc(x: ^Lexer, procedure: $T) {
    for !lexer_is_eof(x) {
        r, _ := lexer_current(x)
        if !procedure(r) {
            break
        }
        lexer_save_advance(x)
    }
}

lexer_next_line :: proc(x: ^Lexer) {
    x.line += 1
    x.col   = 0
}

lexer_skip_whitespace :: proc(x: ^Lexer) {
    for {
        r, n := lexer_current(x)
        switch r {
        case '\n':
            lexer_next_line(x)
            fallthrough
        case '\r', ' ', '\t', '\v':
            break
        case:
            return
        }
        lexer_advance(x)
    }
}

token_make :: proc(x: ^Lexer, type: Token_Type) -> Token {
    t: Token
    t.type   = type
    t.lexeme = strings.to_string(x.builder^)
    t.line   = x.line
    t.col    = max(x.col - i32(len(t.lexeme)), x.col)
    return t
}

is_alpha        :: proc(r: rune) -> bool { return r == '_' || unicode.is_alpha(r) }
is_numeric      :: unicode.is_digit
is_alphanumeric :: proc(r: rune) -> bool { return is_alpha(r) || is_numeric(r) }

@(private="package")
lexer_lex :: proc(x: ^Lexer) -> Token {
    lexer_skip_whitespace(x)
    if lexer_is_eof(x) {
        return token_make(x, .EOF)
    }

    strings.builder_reset(x.builder)
    r := lexer_save_advance(x)
    if is_alpha(r) {
        lexer_consume_sequence(x, is_alphanumeric)
        return token_make_identifier(x)
    } else if is_numeric(r) {
        return token_make_number(x, r)
    }
    return token_make_rune(x, r)
}

token_make_identifier :: proc(x: ^Lexer) -> Token {
    id   := lexer_lexeme(x)
    type := get_id_type(id)
    return token_make(x, type)
}

get_id_type :: proc(id: string) -> Token_Type {
    if 2 <= len(id) && len(id) <= 8 do switch id[0] {
    case 'a': return check_id_type(id, .And)
    case 'b': return check_id_type(id, .Break)
    case 'd': return check_id_type(id, .Do)
    case 'e':
        switch len(id) {
        case 3: return check_id_type(id, .End)
        case 4: return check_id_type(id, .Else)
        case 6: return check_id_type(id, .Elseif)
        }
    case 'f':
        switch len(id) {
        case 3: return check_id_type(id, .For)
        case 5: return check_id_type(id, .False)
        case 8: return check_id_type(id, .Function)
        }
    case 'l': return check_id_type(id, .Local)
    case 'n':
        if len(id) == 3 do switch id[1] {
        case 'i': return check_id_type(id, .Nil)
        case 'o': return check_id_type(id, .Not)
        }
    case 'o': return check_id_type(id, .Or)
    case 'r':
        if len(id) == 6 do switch id[2] {
        case 'p': return check_id_type(id, .Repeat)
        case 't': return check_id_type(id, .Return)
        }
    case 't':
        if len(id) == 4 do switch id[1] {
        case 'h': return check_id_type(id, .Then)
        case 'r': return check_id_type(id, .True)
        }
    case 'u': return check_id_type(id, .Until)
    case 'w': return check_id_type(id, .While)
    }
    return .Identifier
}

check_id_type :: proc(id: string, type: Token_Type, keyword := #caller_expression(type)) -> Token_Type {
    // keyword[0] is '.', keyword[1] is some captial letter so skip them.
    if id[1:] == keyword[2:] {
        return type
    }
    return .Identifier
}

token_make_number :: proc(x: ^Lexer, leader: rune) -> Token {
    parse_integer: if leader == '0' {
        base: int
        switch prefix, _ := lexer_current(x); prefix {
        case 'b', 'B': base = 2
        case 'd', 'D': base = 10
        case 'o', 'O': base = 8
        case 'x', 'X': base = 16
        case:
            break parse_integer
        }
        lexer_consume_sequence(x, is_alphanumeric)

        s := lexer_lexeme(x)
        value, ok := strconv.parse_int(s[2:], base)
        if !ok {
            lexer_error(x, "Malformed integer")
        }

        t := token_make(x, .Number)
        t.data = value
        return t
    }

    is_fraction: bool

    lexer_consume_sequence(x, is_numeric)
    for lexer_save_match_rune(x, '.') {
        is_fraction = true
        lexer_consume_sequence(x, is_numeric)
    }

    if lexer_save_match_either(x, 'E', 'e') {
        if lexer_save_match_rune(x, '-') {
            is_fraction = true
        } else {
            lexer_save_match_rune(x, '+')
        }
    }
    lexer_consume_sequence(x, is_alphanumeric)

    s := lexer_lexeme(x)
    t := token_make(x, .Number)
    ok: bool
    if is_fraction {
        t.data, ok = strconv.parse_f64(s)
    } else {
        t.data, ok = strconv.parse_int(s)
    }

    if !ok {
        lexer_error(x, "Malformed integer")
    }
    return t
}

token_make_rune :: proc(x: ^Lexer, r: rune) -> Token {
    type: Token_Type
    switch r {
    // Balanced Pairs
    case '(': type = .Paren_Open
    case ')': type = .Paren_Close
    case '[': type = .Brace_Open
    case ']': type = .Brace_Close
    case '{': type = .Curly_Open
    case '}': type = .Curly_Close

    // Punctuations
    case '.':
        if lexer_match(x, '.') {
            type = .Ellipsis3 if lexer_match(x, '.') else .Ellipsis2
        } else {
            r2, _ := lexer_current(x)
            if is_numeric(r2) {
                return token_make_number(x, r)
            }
        }
    case ',': type = .Comma
    case ':': type = .Colon
    case ';': type = .Semicolon
    case '#': type = .Pound

    // Arithmetic operators
    case '+': type = .Plus
    case '-': type = .Dash
    case '*': type = .Asterisk
    case '/': type = .Slash
    case '%': type = .Percent

    // Comparison Operators
    case '~': type = .Tilde        if !lexer_match(x, '=') else .Not_Equal
    case '=': type = .Assign       if !lexer_match(x, '=') else .Equal_To
    case '<': type = .Less_Than    if !lexer_match(x, '=') else .Less_Than
    case '>': type = .Great_Than if !lexer_match(x, '=') else .Great_Equal

    // Bitwise Operators
    case '&': type = .Ampersand
    case '|': type = .Pipe
    case '^': type = .Caret
    }
    return token_make(x, type)
}

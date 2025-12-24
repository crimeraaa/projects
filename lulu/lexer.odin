#+private file
package lulu

import "core:fmt"
import "core:c/libc"
import "core:strconv"
import "core:unicode"
import "core:unicode/utf8"

@(private="package")
Token :: struct {
    type:   Token_Type,
    lexeme: string,
    number: f64,
    line:   int,
}

@(private="package")
Lexer :: struct {
    handler: ^libc.jmp_buf, // Temporary!
    input:    string,
    start:    int, // Index of `lexeme[0]` in `input`.
    cursor:   int, // Index of `lexeme[len(lexeme) - 1]` in `input`.
    line:     int, // Current line number.
}

@(private="package")
Token_Type :: enum u8 {
    // Default Type
    Unknown,

    // Keywords
    And, Break, Do, Else, Elseif, End, False, For, Function, Local, If, In,
    Then, True, Nil, Not, Or, Repeat, Return, Until, While,

    // Balanced Pairs: ( ) [ ] { }
    Paren_Open, Paren_Close, Bracket_Open, Bracket_Close, Curly_Open, Curly_Close,

    // Punctuations: , . .. ... : ; #
    Comma, Period, Ellipsis2, Ellipsis3, Colon, Semicolon, Sharp,

    // Arithmetic Operators: + - * / % ^
    Plus, Minus, Asterisk, Slash, Percent, Caret,

    // Assignment: =
    Equal,

    // Comparison Operators: == != < <= > >=
    Equal_Equal, Tilde_Equal, Left_Angle, Left_Angle_Equal, Right_Angle, Right_Angle_Equal,

    // Misc.
    Number, String, Identifier, EOF,
}

@(private="package", rodata)
TOKEN_TYPE_STRINGS := [Token_Type]string{
    .Unknown    = "<unknown>",

    // Keywords
    .And    = "and",    .Break  = "break",  .Do       = "do",
    .Else   = "else",   .Elseif = "elseif", .End      = "end",
    .False  = "false",  .For    = "for",    .Function = "function",
    .Local  = "local",  .If     = "if",     .In       = "in",
    .Then   = "then",   .True   = "true",   .Nil      = "nil",
    .Not    = "not",    .Or     = "or",     .Repeat   = "repeat",
    .Return = "return", .Until = "until",   .While  = "while",

    // Balanced Pairs
    .Paren_Open     = "(", .Paren_Close   = ")",
    .Bracket_Open   = "[", .Bracket_Close = "]",
    .Curly_Open     = "{", .Curly_Close   = "}",

    // Punctuations
    .Comma = ",", .Period    = ".", .Ellipsis2 = "..", .Ellipsis3 = "...",
    .Colon = ":", .Semicolon = ";", .Sharp     = "#",

    // Arithmetic Operators
    .Plus    = "+", .Minus = "-", .Asterisk = "*", .Slash = "/",
    .Percent = "%", .Caret = "^",

    // Assignment
    .Equal = "=",

    // Comparison Operators
    .Equal_Equal = "==", .Tilde_Equal       = "~=",
    .Left_Angle  = "<",  .Left_Angle_Equal  = "<=",
    .Right_Angle = ">",  .Right_Angle_Equal = ">=",

    // Literals
    .Number     = "<number>",     .String = "<string>",
    .Identifier = "<identifier>", .EOF    = "<eof>",
}

@(private="package")
lexer_make :: proc(input: string, handler: ^libc.jmp_buf) -> (x: Lexer) {
    x = Lexer{handler = handler, input = input, line = 1}
    return x
}

lexer_is_eof :: proc(x: ^Lexer) -> bool {
    return x.cursor >= len(x.input)
}

/*
**Brief**

Decodes the UTF-8 character which starts at `x.cursor` plus an optional
`skip` (e.g. to mimic "peeking ahead").
 */
lexer_peek :: proc(x: ^Lexer, skip := 0) -> (r: rune, n: int) {
    r, n = utf8.decode_rune(x.input[x.cursor + skip:])
    return r, n
}

lexer_throw :: proc(x: ^Lexer, format: string, args: ..any) -> ! {
    fmt.eprintf("stdin:%i(%i): ", x.line, x.cursor)
    fmt.eprintfln(format, ..args)
    libc.longjmp(x.handler, 1)
}

lexer_peek_next :: proc(x: ^Lexer, skip := 1) -> (r: rune, n: int) {
    if lexer_is_eof(x) {
        return utf8.RUNE_EOF, 0
    }
    return lexer_peek(x, skip)
}

lexer_match :: proc {
    lexer_match_rune,
    lexer_match_either,
    lexer_match_callback,
}

lexer_match_rune :: proc(x: ^Lexer, want: rune) -> (found: bool) {
    r, n := lexer_peek(x)
    if found = r == want; found {
        lexer_advance(x, r, n)
    }
    return found
}

lexer_match_either :: proc(x: ^Lexer, first, second: rune) -> (found: bool) {
    return lexer_match_rune(x, first) || lexer_match_rune(x, second)
}

lexer_match_callback :: proc(x: ^Lexer, cb: proc(r: rune) -> bool) -> (found: bool) {
    r, n := lexer_peek(x)
    if found = cb(r); found {
        lexer_advance(x, r, n)
    }
    return found
}

lexer_expect :: proc(x: ^Lexer, r: rune) {
    if !lexer_match(x, r) {
        lexer_throw(x, "Expected '%c'", r)
    }
}

lexer_advance :: proc(x: ^Lexer, prev_rune: rune, size: int) {
    // assert(utf8.rune_size(prev_rune) == size)
    x.cursor += size
    if prev_rune == utf8.RUNE_ERROR {
        lexer_throw(x, "Invalid rune '%c' (%i)", prev_rune, prev_rune)
    }
}

lexer_skip_comment_single :: proc(x: ^Lexer) {
    for !lexer_is_eof(x) {
        r, n := lexer_peek(x)
        lexer_advance(x, r, n)
        if r == '\n' {
            x.line += 1
            break
        }
    }
}


/*
**Brief**

Skips all valid whitespace characters and comments.

**Returns**

The first non-whitespace and non-comment character encountered.
 */
lexer_skip_whitespace :: proc(x: ^Lexer) -> (r: rune, n: int) {
    for !lexer_is_eof(x) {
        r, n = lexer_peek(x)
        switch r {
        case '\n':
            x.line += 1;
            fallthrough
        case ' ', '\t', '\r':
            break
        case '-':
            if r2, n2 := lexer_peek_next(x); r2 == '-' {
                // Skip both '-'.
                lexer_advance(x, r, n)
                lexer_advance(x, r2, n2)
                lexer_skip_comment_single(x)
                continue
            }
            // Otherwise is a single '-' so don't consume it here.
            fallthrough
        case:
            return r, n
        }
        lexer_advance(x, r, n)
    }
    return utf8.RUNE_EOF, 0
}

is_number :: unicode.is_number

is_alpha :: proc(r: rune) -> bool {
    return r == '_' || unicode.is_alpha(r)
}

is_alphanumeric :: proc(r: rune) -> bool {
    return is_alpha(r) || is_number(r)
}

lexer_make_token :: proc {
    lexer_make_token_type,
    lexer_make_token_string,
}

lexer_make_token_type :: proc(x: ^Lexer, type: Token_Type) -> Token {
    return lexer_make_token_string(x, type, x.input[x.start:x.cursor])
}

lexer_make_token_string :: proc(x: ^Lexer, type: Token_Type, s: string) -> Token {
    token := Token{type=type, lexeme=s, line=x.line}
    return token
}

@(private="package")
lexer_lex :: proc(x: ^Lexer) -> (token: Token) {
    r, n := lexer_skip_whitespace(x)
    if lexer_is_eof(x) {
        return lexer_make_token(x, .EOF)
    }
    x.start = x.cursor

    // Skip 'r'
    lexer_advance(x, r, n)
    if is_alpha(r) {
        return lexer_make_keyword_or_identifier(x)
    } else if is_number(r) {
        return lexer_make_number(x, r)
    }

    type := lexer_check_rune(x, r)
    return lexer_make_number(x, r) if type == .Number else lexer_make_token(x, type)
}

lexer_make_keyword_or_identifier :: proc(x: ^Lexer) -> (token: Token) {
    lexer_consume_sequence(x, is_alphanumeric)
    lexeme := x.input[x.start:x.cursor]
    type   := lexer_check_keyword(x, lexeme)
    return lexer_make_token(x, type, lexeme)
}

lexer_consume_sequence :: proc(x: ^Lexer, condition: proc(r: rune) -> bool) {
    for !lexer_is_eof(x) {
        r, n := lexer_peek(x)
        if !condition(r) {
            break
        }
        lexer_advance(x, r, n)
    }
}

lexer_check_keyword :: proc(x: ^Lexer, s: string) -> (type: Token_Type) {
    // Helper
    check_type :: proc(s: string, type: Token_Type, offset: int) -> Token_Type {
        kw := TOKEN_TYPE_STRINGS[type]
        return type if s[offset:] == kw[offset:] else .Identifier
    }

    // Guaranteed to be nonzero by this point.
    switch s[0] {
    case 'a': return check_type(s, .And, 1)
    case 'b': return check_type(s, .Break, 1)
    case 'd': return check_type(s, .Do, 1)
    case 'e':
        switch len(s) {
        case len("end"):    return check_type(s, .End, 1)
        case len("else"):   return check_type(s, .Else, 1)
        case len("elseif"): return check_type(s, .Elseif, 1)
        }
    case 'f':
        switch len(s) {
        case len("false"):    return check_type(s, .False, 1)
        case len("function"): return check_type(s, .Function, 1)
        }
    case 'i':
        if len(s) == 2 {
            if s[1] == 'f' {
                return .If
            } else if s[2] == 'n' {
                return .In
            }
        }
    case 'l': return check_type(s, .Local, 1)
    case 'n':
        if len(s) == 3 {
            if s[1] == 'i' && s[2] == 'l' {
                return .Nil
            } else if s[1] == 'o' && s[2] == 't' {
                return .Not
            }
        }
    case 'o': return check_type(s, .Or, 1)
    case 'r':
        // #"return" == #"repeat"
        if len(s) == len("return") && s[1] == 'e' {
            if s[2] == 'p' {
                return check_type(s, .Repeat, 3)
            } else if s[2] == 't' {
                return check_type(s, .Return, 3)
            }
        }
    case 't':
        // #"true" == #"then"
        if len(s) == len("true") {
            if s[1] == 'h' {
                return check_type(s, .Then, 2)
            } else if s[2] == 'r' {
                return check_type(s, .True, 2)
            }
        }
    case 'u': return check_type(s, .Until, 1)
    case 'w': return check_type(s, .While, 1)
    }
    return .Identifier
}

lexer_make_number :: proc(x: ^Lexer, leader: rune) -> (token: Token) {
    if leader == '0' {
        base := 0
        r, n := lexer_peek(x)
        switch r {
        case 'b', 'B': base = 2
        case 'd', 'D': base = 10
        case 'o', 'O': base = 8
        case 'x', 'X': base = 16
        case 'z', 'Z': base = 12
        }

        if base > 0 {
            lexer_advance(x, r, n)
            lexer_consume_sequence(x, is_alphanumeric)
            token = lexer_make_token(x, .Number)
            i, ok := strconv.parse_uint(token.lexeme[2:], base)
            if !ok {
                lexer_throw(x, "Malformed integer '%s'", token.lexeme)
            }
            token.number = cast(f64)i
            return token
        }
    }

    lexer_consume_sequence(x, is_alphanumeric)
    // Maximal munch to help catch invalid number literals.
    for {
        // The decimal (radix) point can only come after the integer portion,
        // or the number sequence.
        if lexer_match(x, '.') {
            lexer_consume_sequence(x, is_alphanumeric)
            continue
        }

        // Explicit scientific-notation exponents can come after the integer
        // portion or the decimal portion.
        if lexer_match(x, 'e', 'E') {
            lexer_match(x, '+', '-')
            lexer_consume_sequence(x, is_alphanumeric)
            continue
        }
        break
    }

    token = lexer_make_token(x, .Number)
    n, ok := strconv.parse_f64(token.lexeme)
    if !ok {
        lexer_throw(x, "Malformed number '%s'", token.lexeme)
    }
    token.number = n
    return token
}

lexer_check_rune :: proc(x: ^Lexer, r: rune) -> (type: Token_Type) {
    switch r {
    // Balanced Pairs
    case '(': return .Paren_Open
    case ')': return .Paren_Close
    case '[': return .Bracket_Open
    case ']': return .Bracket_Close
    case '{': return .Curly_Open
    case '}': return .Curly_Close

    // Punctuations
    case ',': return .Comma
    case '.':
        if lexer_match(x, '.') {
            return .Ellipsis3 if lexer_match(x, '.') else .Ellipsis2
        } else if lexer_match(x, is_number) {
            return .Number
        }
        return .Period
    case ':': return .Colon
    case ';': return .Semicolon
    case '#': return .Sharp

    // Arithmetic Operators
    case '+': return .Plus
    case '-': return .Minus
    case '*': return .Asterisk
    case '/': return .Slash
    case '%': return .Percent
    case '^': return .Caret

    // Assignment, Comparison Operators
    case '=': return .Equal_Equal if lexer_match(x, '=') else .Equal
    case '~': lexer_expect(x, '='); return .Tilde_Equal
    case '<': return .Left_Angle  if lexer_match(x, '=') else .Left_Angle_Equal
    case '>': return .Right_Angle if lexer_match(x, '=') else .Right_Angle_Equal
    }
    return .Unknown
}

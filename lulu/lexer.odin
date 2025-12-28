#+private file
package lulu

import "core:fmt"
import "core:strconv"
import "core:strings"
import "core:unicode"
import "core:unicode/utf8"

@(private="package")
Token :: struct {
    type:   Token_Type,
    lexeme: string,
    data:   Token_Data,
    line:   int,
}

@(private="package")
Token_Data :: union {
    f64, ^OString,
}

@(private="package")
Lexer :: struct {
    // Parent state which will catch any errors we throw.
    L: ^VM,

    // Used to build string literals with escape characters.
    // Also helps in string interning.
    builder: ^strings.Builder,

    // File name. Mainly used for error reporting.
    name: string,

    // Input data which we are lexing.
    input: string,

    // 0th index of lexeme in `input`.
    start: int,

    // Current 1-past-end index of lexeme in `input`.
    cursor: int,

    // Current line number.
    line: int,

    // Size of rune read in during `lexer_read_rune()`.
    curr_size: int,

    // Rune read in during `lexer_read_rune()`.
    curr_rune: rune,
}

@(private="package")
Token_Type :: enum u8 {
    // Default Type
    Unknown,

    // Keywords
    And, Break, Do, Else, Elseif, End, False, For, Function, Local, If, In,

    // Keywords
    Then, True, Nil, Not, Or, Repeat, Return, Until, While,

    // Balanced Pairs: ( ) [ ] { }
    Paren_Open, Paren_Close, Bracket_Open, Bracket_Close, Curly_Open, Curly_Close,

    // Punctuations: , . .. ... : ; #
    Comma, Period, Ellipsis2, Ellipsis3, Colon, Semicolon, Sharp,

    // Arithmetic Operators: + - * / % ^
    Plus, Minus, Asterisk, Slash, Percent, Caret,

    // Assignment: =
    Assign,

    // Comparison Operators: == != < <= > >=
    Equal_To, Not_Equal, Less_Than, Less_Equal, Greater_Than, Greater_Equal,

    // Misc.
    Number, String, Identifier, EOF,
}

@(private="package")
token_type_string :: proc(type: Token_Type) -> string {
    return TOKEN_TYPE_STRINGS[type]
}

@rodata
TOKEN_TYPE_STRINGS := [Token_Type]string{
    // Default Type
    .Unknown = "<unknown>",

    // Keywords
    .And    = "and",    .Break  = "break",  .Do       = "do",
    .Else   = "else",   .Elseif = "elseif", .End      = "end",
    .False  = "false",  .For    = "for",    .Function = "function",
    .Local  = "local",  .If     = "if",     .In       = "in",
    .Then   = "then",   .True   = "true",   .Nil      = "nil",
    .Not    = "not",    .Or     = "or",     .Repeat   = "repeat",
    .Return = "return", .Until  = "until",  .While    = "while",

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
    .Assign = "=",

    // Comparison Operators
    .Equal_To     = "==", .Not_Equal     = "~=",
    .Less_Than    = "<",  .Less_Equal    = "<=",
    .Greater_Than = ">",  .Greater_Equal = ">=",

    // Literals
    .Number     = "<number>",     .String = "<string>",
    .Identifier = "<identifier>", .EOF    = "<eof>",
}

/*
Create a lexer with the state needed to manage itself across calls to
`lexer_scan_token()`.

**Parameters**
- builder: Pointer to a caller-provided string builder which will be used
to construct string literals with escape sequences.
- name: File name where `input` was read in from.
- input: The actual text data to be lexed.
 */
@(private="package")
lexer_make :: proc(L: ^VM, builder: ^strings.Builder, name, input: string) -> Lexer {
    x := Lexer{L=L, builder=builder, name=name, input=input, line=1}
    // Read first char so first `lexer_scan_token()` call is valid.
    lexer_read_rune(&x)
    return x
}

lexer_is_eof :: proc(x: ^Lexer) -> bool {
    return x.cursor >= len(x.input)
}

/*
Decodes the UTF-8 character which starts at `x.cursor` and saves the result.

**Guarantees**
- Once the result is saved, you can simply call `lexer_peek()` to save on
UTF-8 decoding calls.
 */
lexer_read_rune :: proc(x: ^Lexer) -> (r: rune) {
    n: int
    r, n = utf8.decode_rune(x.input[x.cursor:])
    // Update nonlocal state.
    x.curr_rune = r
    x.curr_size = n
    return r
}

/*
Reports a formatted error message and throws a syntax error to the parent VM.
 */
lexer_error :: proc(x: ^Lexer, format: string, args: ..any) -> ! {
    fmt.eprintf("%s:%i(%i): ", x.name, x.line, x.cursor)
    fmt.eprintfln(format, ..args)
    vm_throw(x.L, .Syntax)
}

lexer_peek :: proc(x: ^Lexer) -> (r: rune) {
    return x.curr_rune
}

lexer_peek_size :: proc(x: ^Lexer) -> (size: int) {
    return x.curr_size
}

lexer_peek_next :: proc(x: ^Lexer) -> (r: rune) {
    if lexer_is_eof(x) {
        return utf8.RUNE_EOF
    }
    r, _ = utf8.decode_rune(x.input[x.cursor + 1:])
    return r
}

/*
Advances only if the current character matches `want`.

**Parameters**
- want: The desired character to match the current one against.

**Assumptions**
- `lexer_read_rune()` was called beforehand.
 */
lexer_match_rune :: proc(x: ^Lexer, want: rune) -> (found: bool) {
    r := lexer_peek(x)
    found = r == want
    if found {
        lexer_advance(x)
    }
    return found
}

/*
Advances only if the current character matches one of `want1` or `want2`.

**Parameters**
- want1: The first character to check against.
- want2: The second character to check against, only if `want1` didn't match.

**Assumptions**
- `lexer_read_rune()` was called beforehand.
 */
lexer_match_either :: proc(x: ^Lexer, want1, want2: rune) -> (found: bool) {
    r := lexer_peek(x)
    found = r == want1 || r == want2
    if found {
        lexer_advance(x)
    }
    return found
}

/*
Advances only if the current character fulfills the predicate `p`.

**Parameters**
- p: The procedure which checks the current character.

**Assumptions**
- `lexer_read_rune()` was called beforehand.
 */
lexer_match_proc :: proc(x: ^Lexer, p: proc(rune) -> bool) -> (found: bool) {
    r := lexer_peek(x)
    found = p(r)
    if found {
        lexer_advance(x)
    }
    return found
}

/*
Asserts that the current character matches `want`. If it does not, then a
syntax error is thrown.

**Parameters**
- want: The character to check the current one against.

**Assumptions**
- We are in a protected call, so throwing errors is safe.
 */
lexer_expect :: proc(x: ^Lexer, want: rune) {
    if !lexer_match_rune(x, want) {
        lexer_error(x, "Expected '%c'", want)
    }
}

/*
Increments the cursor by the size of the currently read character.

**Guarantees**
- The next character is read in and saved. `lexer_peek()` can thus be
called to save on UTF-8 decoding calls.
 */
lexer_advance :: proc(x: ^Lexer) {
    // assert(utf8.rune_size(prev_rune) == size)
    x.cursor += lexer_peek_size(x)
    if x.curr_rune == utf8.RUNE_ERROR {
        lexer_error(x, "Invalid rune '%c' (%i)", x.curr_rune, x.curr_rune)
    }
    lexer_read_rune(x)
}

lexer_skip_comment_multi :: proc(x: ^Lexer, nest_open: int) {
    for !lexer_is_eof(x) {
        r := lexer_peek(x)
        switch r {
        case ']':
            nest_close := lexer_consume_rune(x, '=')
            if lexer_match_rune(x, ']') && nest_open == nest_close {
                return
            }
        case '\n':
            x.line += 1
        }
        lexer_advance(x)
    }
    lexer_error(x, "Unterminated multiline comment")
}

lexer_skip_comment :: proc(x: ^Lexer) {
    if lexer_match_rune(x, '[') {
        nest_open := lexer_consume_rune(x, '=')
        // Is definitely a multiline comment?
        if lexer_match_rune(x, '[') {
            lexer_skip_comment_multi(x, nest_open)
            return
        }
        // Otherwise, treat this as a single line comment.
    }

    for !lexer_is_eof(x) {
        r := lexer_peek(x)
        lexer_advance(x)
        if r == '\n' {
            x.line += 1
            break
        }
    }
}


/*
Skips all valid whitespace characters and comments.

**Returns**
- r: The first non-whitespace and non-comment UTF-8 character encountered.
 */
lexer_skip_whitespace :: proc(x: ^Lexer) -> (r: rune) {
    for !lexer_is_eof(x) {
        r = lexer_peek(x)
        switch r {
        case '\n':
            x.line += 1;
            fallthrough
        case ' ', '\t', '\r':
            break
        case '-':
            if r2 := lexer_peek_next(x); r2 == '-' {
                // Skip both '-'. These 2 calls are valid since `r == r2`.
                lexer_advance(x)
                lexer_advance(x)
                lexer_skip_comment(x)
                continue
            }
            // Otherwise is a single '-' so don't consume it here.
            fallthrough
        case:
            return r
        }
        lexer_advance(x)
    }
    return 0
}

is_number :: unicode.is_number

// For our purposes, even '_' is a valid alphabetical character.
is_alpha :: proc(r: rune) -> bool {
    return r == '_' || unicode.is_alpha(r)
}

is_alphanumeric :: proc(r: rune) -> bool {
    return is_alpha(r) || is_number(r)
}

token_make :: proc {
    token_make_type,
    token_make_type_string,
}

token_make_type :: proc(x: ^Lexer, type: Token_Type) -> Token {
    return token_make_type_string(x, type, x.input[x.start:x.cursor])
}

token_make_type_string :: proc(x: ^Lexer, type: Token_Type, s: string) -> Token {
    return Token{type=type, lexeme=s, line=x.line}
}

@(private="package")
lexer_scan_token :: proc(x: ^Lexer) -> (token: Token) {
    r := lexer_skip_whitespace(x)
    if lexer_is_eof(x) {
        return token_make(x, Token_Type.EOF)
    }
    // Lexeme start is the first non-whitespace, non-comment character.
    x.start = x.cursor

    // Skip 'r'
    lexer_advance(x)
    if is_alpha(r) {
        return lexer_make_keyword_or_identifier(x)
    } else if is_number(r) {
        return lexer_make_number_token(x, r)
    }
    return lexer_make_rune_token(x, r)
}

lexer_make_keyword_or_identifier :: proc(x: ^Lexer) -> (token: Token) {
    lexer_consume_proc(x, is_alphanumeric)
    token      = token_make(x, Token_Type.Identifier)
    token.type = lexer_check_keyword(x, token.lexeme)
    return token
}

/*
Continuously advances while the current character matches `r`.

**Parameters**
- r: The desired character to check against.

**Returns**
- n: The number of times `r` was matched.
 */
lexer_consume_rune :: proc(x: ^Lexer, r: rune) -> (n: int) {
    for !lexer_is_eof(x) && lexer_peek(x) == r {
        lexer_advance(x)
        n += 1
    }
    return n
}

/*
Continuously advances while the current character fulfills the predicate `p`.

**Parameters**
- p: The procedure which checks the current character.
 */
lexer_consume_proc :: proc(x: ^Lexer, p: proc(rune) -> bool) {
    for !lexer_is_eof(x) && p(lexer_peek(x)) {
        lexer_advance(x)
    }
}

lexer_check_keyword :: proc(x: ^Lexer, s: string) -> (type: Token_Type) {
    // Helper
    check :: proc(s: string, type: Token_Type, offset: int) -> Token_Type {
        kw := token_type_string(type)
        return type if s[offset:] == kw[offset:] else .Identifier
    }

    if !(len("do") <= len(s) && len(s) <= len("function")) {
        return .Identifier
    }

    // Guaranteed to be nonzero by this point.
    switch s[0] {
    case 'a': return check(s, .And, 1)
    case 'b': return check(s, .Break, 1)
    case 'd': return check(s, .Do, 1)
    case 'e':
        switch len(s) {
        case len("end"):    return check(s, .End, 1)
        case len("else"):   return check(s, .Else, 1)
        case len("elseif"): return check(s, .Elseif, 1)
        }
    case 'f':
        switch s[1] {
        case 'a': return check(s, .False, 1)
        case 'u': return check(s, .Function, 1)
        }
    case 'i':
        switch s[1] {
        case 'f': return .If
        case 'n': return .In
        }
    case 'l': return check(s, .Local, 1)
    case 'n':
        // #"nil" == #"not"
        if len(s) == len("nil") {
            switch s[1] {
            case 'i': return .Nil if s[2] == 'l' else .Identifier
            case 'o': return .Not if s[2] == 't' else .Identifier
            }
        }
    case 'o': return .Or if s[1] == 'r' else .Identifier
    case 'r':
        // #"return" == #"repeat"
        if len(s) == len("return") && s[1] == 'e' {
            switch s[2] {
            case 'p': return check(s, .Repeat, 3)
            case 't': return check(s, .Return, 3)
            }
        }
    case 't':
        // #"true" == #"then"
        if len(s) == len("true") {
            switch s[1] {
            case 'h': return check(s, .Then, 2)
            case 'r': return check(s, .True, 2)
            }
        }
    case 'u': return check(s, .Until, 1)
    case 'w': return check(s, .While, 1)
    }
    return .Identifier
}

lexer_make_number_token :: proc(x: ^Lexer, leader: rune) -> Token {
    if leader == '0' {
        base := 0
        switch lexer_peek(x) {
        case 'b', 'B': base = 2
        case 'd', 'D': base = 10
        case 'o', 'O': base = 8
        case 'x', 'X': base = 16
        case 'z', 'Z': base = 12
        }
        lexer_advance(x)

        // Is definitely a prefixed integer?
        if base > 0 {
            lexer_consume_proc(x, is_alphanumeric)
            token := token_make(x, Token_Type.Number)
            i, ok := strconv.parse_uint(token.lexeme[2:], base)
            if !ok {
                lexer_error(x, "Malformed integer '%s'", token.lexeme)
            }
            f := cast(f64)i
            // `i` as an `f64` might not be accurately represented?
            if cast(uint)f != i {
                lexer_error(x, "Invalid f64 integer '%s'", token.lexeme)
            }
            token.data = f
            return token
        }
    }

    lexer_consume_proc(x, is_number)

    // The decimal (radix) point can only come after the integer portion,
    // or the number sequence.
    if lexer_match_rune(x, '.') {
        lexer_consume_proc(x, is_number)
    }

    // Explicit scientific-notation exponents can come after the integer
    // portion or the decimal portion.
    if lexer_match_either(x, 'e', 'E') {
        lexer_match_either(x, '+', '-')
    }

    // Consume any and all trailing characters even if they do not form
    // a valid number. This helps us inform the user ASAP.
    lexer_consume_proc(x, is_alphanumeric)

    token := token_make(x, Token_Type.Number)
    n, ok := strconv.parse_f64(token.lexeme)
    if !ok {
        lexer_error(x, "Malformed number '%s'", token.lexeme)
    }
    token.data = n
    return token
}

lexer_make_rune_token :: proc(x: ^Lexer, r: rune) -> Token {
    type := Token_Type.Unknown
    switch r {
    // Balanced Pairs
    case '(': type = .Paren_Open
    case ')': type = .Paren_Close
    case '[': type = .Bracket_Open
    case ']': type = .Bracket_Close
    case '{': type = .Curly_Open
    case '}': type = .Curly_Close

    // Punctuations
    case '\"', '\'':
        return lexer_make_string_token(x, r)
    case ',': type = .Comma
    case '.':
        if lexer_match_rune(x, '.') {
            type = .Ellipsis3 if lexer_match_rune(x, '.') else .Ellipsis2
        } else if lexer_match_proc(x, is_number) {
            return lexer_make_number_token(x, r)
        } else {
            type = .Period
        }
    case ':': type = .Colon
    case ';': type = .Semicolon
    case '#': type = .Sharp

    // Arithmetic Operators
    case '+': type = .Plus
    case '-': type = .Minus
    case '*': type = .Asterisk
    case '/': type = .Slash
    case '%': type = .Percent
    case '^': type = .Caret

    // Assignment, Comparison Operators
    case '=': type = .Equal_To if lexer_match_rune(x, '=') else .Assign
    case '~': lexer_expect(x, '='); type = .Not_Equal
    case '<': type = .Less_Than    if lexer_match_rune(x, '=') else .Less_Equal
    case '>': type = .Greater_Than if lexer_match_rune(x, '=') else .Greater_Equal
    }
    return token_make(x, type)
}

lexer_make_string_token :: proc(x: ^Lexer, q: rune) -> Token {
    // Wrapper to help catch memory errors.
    write :: proc(L: ^VM, b: ^strings.Builder, r: rune, size: int) {
        n, err := strings.write_rune(b, r)

        /*
        **Note** (2025-12-25)

        See the following functions:
            - core/strings/builder.odin:write_rune()
            - core/io/io.odin:write()
            - core/io/io.odin:write_bytes()

        This shows us that the underlying call can never return a non-nil
        error (other than io.Error.EOF). At no point is reallocation failure
        handled. We do, however, get the number of bytes actually written.
        We can use that to check if we successfully wrote `r`.
         */
        if err != nil || n != size {
            vm_error_memory(L)
        }
    }

    L := x.L
    b := x.builder
    strings.builder_reset(b)

    consume_loop: for {
        if lexer_is_eof(x) {
            lexer_error(x, "Unfinished string")
        }

        r, r_size := lexer_peek(x), lexer_peek_size(x)
        lexer_advance(x)
        switch r {
        case q: break consume_loop
        case '\'':
            esc, esc_size := lexer_peek(x), lexer_peek_size(x)
            lexer_advance(x)
            // We assume the escaped part of escape sequences are all ASCII.
            // https://odin-lang.org/docs/overview/
            // https://www.lua.org/pil/2.4.html
            switch esc {
            case 'a': esc = '\a'
            case 'b': esc = '\b'
            case 'e': esc = '\e'
            case 'n': esc = '\n'
            case 'r': esc = '\r'
            case 't': esc = '\t'
            case 'v': esc = '\v'
            case '\n', '\\', '\"', '\'', '[', ']':
                break
            case:
                lexer_error(x, "Unsupported escape sequence '%c'", esc)
            }
            write(L, b, esc, esc_size)

        // Unescaped newline. If explicitly escaped then it's valid.
        case '\n':
            lexer_error(x, "Unfinished string")
        case:
            write(L, b, r, r_size)
        }
    }

    token := token_make(x, Token_Type.String)
    // Skip single quotes in the string.
    token.lexeme = token.lexeme[1:len(token.lexeme) - 1]
    token.data   = ostring_new(L, strings.to_string(b^))
    return token
}

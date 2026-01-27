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
    using data: Token_Data,
    line, col: i32,
}

@(private="package")
Token_Data :: struct #raw_union {
    number: f64,
    string: ^Ostring,
}

@(private="package")
Lexer :: struct {
    // Parent state which will catch any errors we throw.
    L: ^State,

    // Used to build string literals with escape characters.
    // Also helps in string interning.
    builder: ^strings.Builder,

    // File name. Mainly used for error reporting.
    name: ^Ostring,

    // Input data which we are lexing.
    input: string,

    // 0th index of lexeme in `input`.
    start: int,

    // Current 1-past-end index of lexeme in `input`.
    cursor: int,

    // Current line and column number.
    line, col: i32,

    // Size of rune read in during `read_rune()`.
    curr_size: int,

    // Rune read in during `read_rune()`.
    curr_rune: rune,
}

@(private="package")
Token_Type :: enum u8 {
    // Default Type
    None,

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
token_string :: proc(type: Token_Type) -> string {
    return TOKEN_TYPE_STRINGS[type]
}

@rodata
TOKEN_TYPE_STRINGS := [Token_Type]string{
    // Default Type
    .None = "<unknown>",

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
lexer_make :: proc(L: ^State, builder: ^strings.Builder, name: ^Ostring, input: string) -> Lexer {
    x := Lexer{L=L, builder=builder, name=name, input=input, line=1}
    // Read first char so first `lexer_scan_token()` call is valid.
    read_rune(&x)
    return x
}

is_eof :: proc(x: ^Lexer) -> bool {
    return x.cursor >= len(x.input)
}

/*
Decodes the UTF-8 character which starts at `x.cursor` and saves the result.

**Guarantees**
- Once the result is saved, you can simply call `peek_rune()` to save on
UTF-8 decoding calls.
 */
read_rune :: proc(x: ^Lexer) -> (r: rune) {
    n: int
    r, n = utf8.decode_rune(x.input[x.cursor:])
    // Update nonlocal state.
    x.curr_rune = r
    x.curr_size = n

    // TODO(2026-01-04): Should we assume each character maps to 1 column?
    x.col += 1
    return r
}

/*
Reports a error message and throws a syntax error to the parent VM.
 */
error :: proc(x: ^Lexer, msg: string) -> ! {
    here    := make_token_type(x, nil)
    here.col = i32(x.cursor)
    debug_syntax_error(x, here, msg)
}

peek_rune :: proc(x: ^Lexer) -> (r: rune) {
    return x.curr_rune
}

peek_size :: proc(x: ^Lexer) -> (size: int) {
    return x.curr_size
}

peek_next_rune :: proc(x: ^Lexer) -> (r: rune) {
    if is_eof(x) {
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
- `read_rune()` was called beforehand.
 */
match_rune :: proc(x: ^Lexer, want: rune) -> (found: bool) {
    r := peek_rune(x)
    found = r == want
    if found {
        advance_rune(x)
    }
    return found
}

/*
Advances only if the current character matches one of `want1` or `want2`.

**Parameters**
- want1: The first character to check against.
- want2: The second character to check against, only if `want1` didn't match.

**Assumptions**
- `read_rune()` was called beforehand.
 */
match_either_rune :: proc(x: ^Lexer, want1, want2: rune) -> (found: bool) {
    r := peek_rune(x)
    found = r == want1 || r == want2
    if found {
        advance_rune(x)
    }
    return found
}

/*
Advances only if the current character fulfills the predicate `p`.

**Parameters**
- p: The procedure which checks the current character.

**Assumptions**
- `read_rune()` was called beforehand.
 */
match_proc :: proc(x: ^Lexer, p: proc(rune) -> bool) -> (found: bool) {
    r := peek_rune(x)
    found = p(r)
    if found {
        advance_rune(x)
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
expect_rune :: proc(x: ^Lexer, want: rune) {
    if !match_rune(x, want) {
        buf: [64]byte
        msg := fmt.bprintf(buf[:], "Expected '%c'", want)
        error(x, msg)
    }
}

/*
Increments the cursor by the size of the currently read character.

**Guarantees**
- The next character is read in and saved. `peek_rune()` can thus be
called to save on UTF-8 decoding calls.
 */
advance_rune :: proc(x: ^Lexer) {
    // assert(utf8.rune_size(prev_rune) == size)
    x.cursor += peek_size(x)
    if x.curr_rune == utf8.RUNE_ERROR {
        buf: [64]byte
        msg := fmt.bprintf(buf[:], "Invalid rune '%c' (%i)", x.curr_rune, x.curr_rune)
        error(x, msg)
    }
    read_rune(x)
}

skip_comment_multi :: proc(x: ^Lexer, nest_open: int) -> (start, stop: int) {
    start = x.cursor
    for !is_eof(x) {
        r := peek_rune(x)
        switch r {
        case ']':
            stop = x.cursor

            // Skip the first ']' so we can see the '=' or the second ']'.
            advance_rune(x)
            nest_close := consume_rune_multi(x, '=')
            if match_rune(x, ']') && nest_open == nest_close {
                return start, stop
            }

            // Didn't find ']' with appropriate number of '=', continue so that
            // we don't needlessly advance.
            continue
        case '\n':
            next_line(x)
        }
        advance_rune(x)
    }
    error(x, "Unterminated multiline comment")
}

next_line :: proc(x: ^Lexer) {
    x.line += 1
    x.col = 0
}

skip_comment :: proc(x: ^Lexer) {
    if match_rune(x, '[') {
        nest_open := consume_rune_multi(x, '=')
        // Is definitely a multiline comment?
        if match_rune(x, '[') {
            skip_comment_multi(x, nest_open)
            return
        }
        // Otherwise, treat this as a single line comment.
    }

    for !is_eof(x) {
        r := peek_rune(x)
        advance_rune(x)
        if r == '\n' {
            next_line(x)
            break
        }
    }
}


/*
Skips all valid whitespace characters and comments.

**Returns**
- r: The first non-whitespace and non-comment UTF-8 character encountered.
 */
skip_whitespace :: proc(x: ^Lexer) -> (r: rune) {
    for !is_eof(x) {
        r = peek_rune(x)
        switch r {
        case '\n':
            next_line(x);
            fallthrough
        case ' ', '\t', '\r':
            break
        case '-':
            if r2 := peek_next_rune(x); r2 == '-' {
                // Skip both '-'. These 2 calls are valid since `r == r2`.
                advance_rune(x)
                advance_rune(x)
                skip_comment(x)
                continue
            }
            // Otherwise is a single '-' so don't consume it here.
            fallthrough
        case:
            return r
        }
        advance_rune(x)
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

make_token_type :: proc(x: ^Lexer, type: Token_Type) -> Token {
    return make_token_type_string(x, type, x.input[x.start:x.cursor])
}

make_token_type_string :: proc(x: ^Lexer, type: Token_Type, s: string) -> Token {
    t: Token
    t.type = type
    t.lexeme = s
    t.line   = x.line
    t.col    = i32(int(x.col) - (len(s)))
    return t
}

@(private="package")
lexer_scan_token :: proc(x: ^Lexer) -> Token {
    r := skip_whitespace(x)
    if is_eof(x) {
        token := make_token_type(x, Token_Type.EOF)
        token.col = x.col
        return token
    }
    // Lexeme start is the first non-whitespace, non-comment character.
    x.start = x.cursor

    // Skip 'r'
    advance_rune(x)
    if is_alpha(r) {
        consume_proc(x, is_alphanumeric)
        token := make_token_type(x, Token_Type.Identifier)
        // Keywords were interned on startup, so we can already check for their
        // types.
        s := ostring_new(x.L, token.lexeme)
        token.string = s
        token.type   = .Identifier if s.kw_type == nil else s.kw_type
        return token
    } else if is_number(r) {
        return make_number_token(x, r)
    }
    return make_rune_token(x, r)
}

/*
Continuously advances while the current character matches `r`.

**Parameters**
- r: The desired character to check against.

**Returns**
- n: The number of times `r` was matched.
 */
consume_rune_multi :: proc(x: ^Lexer, r: rune) -> (n: int) {
    for !is_eof(x) && peek_rune(x) == r {
        advance_rune(x)
        n += 1
    }
    return n
}

/*
Continuously advances while the current character fulfills the predicate `p`.

**Parameters**
- p: The procedure which checks the current character.
 */
consume_proc :: proc(x: ^Lexer, p: proc(rune) -> bool) {
    for !is_eof(x) && p(peek_rune(x)) {
        advance_rune(x)
    }
}

make_number_token :: proc(x: ^Lexer, leader: rune) -> Token {
    if leader == '0' {
        base := 0
        switch peek_rune(x) {
        case 'b', 'B': base = 2
        case 'd', 'D': base = 10
        case 'o', 'O': base = 8
        case 'x', 'X': base = 16
        case 'z', 'Z': base = 12
        }

        // Is definitely a prefixed integer?
        if base > 0 {
            // Advance ONLY if we absolutely have a base prefix.
            advance_rune(x)
            consume_proc(x, is_alphanumeric)
            token := make_token_type(x, Token_Type.Number)
            i, ok := strconv.parse_uint(token.lexeme[2:], base)
            if !ok {
                error(x, "Malformed integer")
            }
            f := f64(i)
            // `i` as an `f64` might not be accurately represented?
            if uint(f) != i {
                error(x, "Invalid f64 integer")
            }
            token.number = f
            return token
        }
    }

    consume_proc(x, is_number)

    // The decimal (radix) point can only come after the integer portion,
    // or the number sequence.
    if match_rune(x, '.') {
        consume_proc(x, is_number)
    }

    // Explicit scientific-notation exponents can come after the integer
    // portion or the decimal portion.
    if match_either_rune(x, 'e', 'E') {
        match_either_rune(x, '+', '-')
    }

    // Consume any and all trailing characters even if they do not form
    // a valid number. This helps us inform the user ASAP.
    consume_proc(x, is_alphanumeric)

    token := make_token_type(x, Token_Type.Number)
    n, ok := strconv.parse_f64(token.lexeme)
    if !ok {
        error(x, "Malformed number")
    }
    token.number = n
    return token
}

make_rune_token :: proc(x: ^Lexer, r: rune) -> Token {
    type: Token_Type
    switch r {
    // Balanced Pairs
    case '(': type = .Paren_Open
    case ')': type = .Paren_Close
    case '[':
        nest_open := consume_rune_multi(x, '=')
        if match_rune(x, '[') {
            start, stop := skip_comment_multi(x, nest_open)
            token       := make_token_type_string(x, .String, x.input[start:stop])
            interned    := ostring_new(x.L, token.lexeme)
            token.string = interned
            return token
        } else {
            if nest_open > 0 {
                error(x, "Expected a multiline string")
            }
            type = .Bracket_Open
        }
    case ']': type = .Bracket_Close
    case '{': type = .Curly_Open
    case '}': type = .Curly_Close

    // Punctuations
    case '\"', '\'':
        return make_string_token(x, r)
    case ',': type = .Comma
    case '.':
        if match_rune(x, '.') {
            type = .Ellipsis3 if match_rune(x, '.') else .Ellipsis2
        } else if match_proc(x, is_number) {
            return make_number_token(x, r)
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
    case '=': type = .Equal_To if match_rune(x, '=') else .Assign
    case '~': expect_rune(x, '='); type = .Not_Equal
    case '<': type = .Less_Than    if !match_rune(x, '=') else .Less_Equal
    case '>': type = .Greater_Than if !match_rune(x, '=') else .Greater_Equal
    }
    return make_token_type(x, type)
}

make_string_token :: proc(x: ^Lexer, q: rune) -> Token {
    // Wrapper to help catch memory errors.
    write :: proc(L: ^State, b: ^strings.Builder, r: rune, size: int) {
        n, err := strings.write_rune(b, r)

        /*
        **Note(2025-12-25)**

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
            debug_memory_error(L, "write rune '%c'", r)
        }
    }

    L := x.L
    b := x.builder
    strings.builder_reset(b)

    consume_loop: for {
        if is_eof(x) {
            error(x, "Unfinished string")
        }

        r, r_size := peek_rune(x), peek_size(x)
        advance_rune(x)
        switch r {
        case q: break consume_loop
        case '\\':
            esc, esc_size := peek_rune(x), peek_size(x)
            advance_rune(x)
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
                buf: [size_of(rune)]byte
                msg := fmt.bprintf(buf[:], "Unsupported escape sequence '%c'", esc)
                error(x, msg)
            }
            write(L, b, esc, esc_size)

        // Unescaped newline. If explicitly escaped then it's valid.
        case '\n':
            error(x, "Unfinished string")
        case:
            write(L, b, r, r_size)
        }
    }

    token := make_token_type(x, Token_Type.String)
    // Skip single quotes in the string.
    token.lexeme = token.lexeme[1:len(token.lexeme) - 1]
    token.string = ostring_new(L, strings.to_string(b^))
    return token
}

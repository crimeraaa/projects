#+private file
package lulu

import "core:fmt"
import "core:strconv"
import "core:strings"
import "core:unicode"
import "core:unicode/utf8"

@(private="package")
Token :: struct {
    type:       Token_Type,
    lexeme:     string,
    using data: Token_Data,
    line, col:  i32,
}

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

    // Provides the input data that we wish to lex.
    reader: Reader,

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
token_string :: proc(t: Token) -> (s: string, is_owned: bool) #optional_ok {
    #partial switch t.type {
    case .None,   .Number:     return t.lexeme, true
    case .String, .Identifier: return ostring_to_string(t.string), false
    case:
        break
    }
    return token_type_string(t.type), false
}

@(private="package")
token_type_string :: proc(type: Token_Type) -> string {
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
lexer_make :: proc(L: ^State, builder: ^strings.Builder, name: ^Ostring, input: Reader) -> Lexer {
    x: Lexer
    x.L       = L
    x.builder = builder
    x.name    = name
    x.reader  = input
    x.line    = 1
    // Read first char so first `lexer_scan_token()` call is valid.
    read_rune(&x)
    return x
}

is_eof :: proc(x: ^Lexer) -> bool {
    return x.curr_rune == utf8.RUNE_EOF
}

/*
Decodes the UTF-8 character in the input and saves the result.

**Guarantees**
- Once the result is saved, you can simply call `peek_rune()` to save on
UTF-8 decoding calls.
 */
read_rune :: proc(x: ^Lexer) -> (r: rune) {
    n: int
    r, n = reader_read_rune(&x.reader)

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
__error :: proc(x: ^Lexer, msg: string) -> ! {
    here := make_token(x, nil)

    // Pinpoint the exact error location if in a multiline sequence.
    if here.col <= 0 {
        here.col = x.col
    }

    if len(here.lexeme) == 0 {
        here.lexeme = token_type_string(.EOF)
    }
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

    r, _ = reader_lookahead(&x.reader)
    return r
}

/*
Advances only if the current character matches `want`.

**Parameters**
- want: The desired character to match the current one against.
- save: If `true` then the character is saved iff it matches.

**Assumptions**
- `read_rune()` was called beforehand.
 */
__match_rune :: proc(x: ^Lexer, want: rune, $save: bool) -> (found: bool) {
    r := peek_rune(x)
    found = r == want
    if found {
        __advance_rune(x, save=save)
    }
    return found
}

match_rune :: proc(x: ^Lexer, want: rune) -> (found: bool) {
    return __match_rune(x, want, save=false)
}


/*
Advances only if the current character matches `want`. If matched then
the character is saved.

**Parameters**
- want: The desired character to match the current one against.

**Assumptions**
- `read_rune()` was called beforehand.
 */
save_match_rune :: proc(x: ^Lexer, want: rune) -> (found: bool) {
    return __match_rune(x, want, save=true)
}


/*
Advances only if the current character matches one of `want1` or `want2`.

**Parameters**
- want1: The first character to check against.
- want2: The second character to check against, only if `want1` didn't match.

**Assumptions**
- `read_rune()` was called beforehand.
 */
save_match_either_rune :: proc(x: ^Lexer, want1, want2: rune) -> (found: bool) {
    r := peek_rune(x)
    found = r == want1 || r == want2
    if found {
        save_advance_rune(x)
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
save_match_proc :: proc(x: ^Lexer, $procedure: proc(rune) -> bool) -> (found: bool) {
    r := peek_rune(x)
    found = procedure(r)
    if found {
        save_advance_rune(x)
    }
    return found
}

/*
Asserts that the current character matches `want`. If it does not, then a
syntax error is thrown. Otherwise it is saved and we advance.

**Parameters**
- want: The character to check the current one against.

**Assumptions**
- We are in a protected call, so throwing errors is safe.
 */
expect_rune :: proc(x: ^Lexer, want: rune) {
    if !save_match_rune(x, want) {
        buf: [64]byte
        msg := fmt.bprintf(buf[:], "Expected '%c'", want)
        __error(x, msg)
    }
}

/*
**Parameters**
- save: If `true` then the current rune is saved before reading in the next one.

**Guarantees**
- The next character is read in. `peek_rune()` can thus be called to save on
UTF-8 decoding calls.
 */
__advance_rune :: proc(x: ^Lexer, $save: bool) {
    r := x.curr_rune
    if r == utf8.RUNE_ERROR {
        buf: [64]byte
        msg := fmt.bprintf(buf[:], "Invalid rune '%c' (%i)", r, r)
        __error(x, msg)
    }

    when save {
        save_rune(x, x.curr_rune, x.curr_size)
    }
    read_rune(x)
}

save_rune :: proc(x: ^Lexer, r: rune, size: int) {
    __write(x, x.builder, r, size)
}

// Wrapper to help catch memory errors.
__write :: proc(x: ^Lexer, b: ^strings.Builder, r: rune, size: int) {
    n, _ := strings.write_rune(b, r)

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
    if n != size {
        overflow_error(x, r)
    }
}


overflow_error :: proc(x: ^Lexer, r: rune) -> ! {
    // If lexer allocator is the same as the global state allocator,
    // then the final error will be a memory one.
    __error(x, "token stream overflow")
}

advance_rune :: proc(x: ^Lexer) {
    __advance_rune(x, save=false)
}

/*
**Guarantees**
- The next character is read in and saved. `peek_rune()` can thus be
called to save on UTF-8 decoding calls.
 */
save_advance_rune :: proc(x: ^Lexer) {
    __advance_rune(x, save=true)
}

consume_multi_sequence :: proc(x: ^Lexer, nest_open: int, $save: bool) -> int {
    for !is_eof(x) {
        r := peek_rune(x)
        switch r {
        case ']':
            // Skip the first ']' so we can see the '=' or the second ']'.
            __advance_rune(x, save=save)
            nest_close := consume_rune_multi(x, '=', save=save)
            terminated := __match_rune(x, ']', save=save)
            if terminated && nest_open == nest_close {
                return nest_close + 2
            }

            // Didn't find ']' with appropriate number of '=', continue so that
            // we don't needlessly advance.
            continue
        case '\n':
            advance_line(x)
        }
        __advance_rune(x, save=save)
    }
    // Report error tokens properly.
    what := "Unterminated multiline " + ("string" when save else "comment")
    __error(x, what)
}

advance_line :: proc(x: ^Lexer) {
    x.line += 1
    x.col   = 0
}

skip_comment :: proc(x: ^Lexer) {
    if match_rune(x, '[') {
        nest_open := consume_rune_multi(x, '=', save=false)
        // Is definitely a multiline comment?
        if match_rune(x, '[') {
            consume_multi_sequence(x, nest_open, save=false)
            return
        }
        // Otherwise, treat this as a single line comment.
    }

    for !is_eof(x) {
        r := peek_rune(x)
        advance_rune(x)
        if r == '\n' {
            advance_line(x)
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
            advance_line(x);
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

make_token :: proc(x: ^Lexer, type: Token_Type) -> Token {
    t: Token
    t.type   = type
    t.lexeme = strings.to_string(x.builder^)
    t.line   = x.line

    // NOTE(2026-02-03): Only works correctly for single-line tokens.
    // Multi-line strings will be wrong here!
    t.col = i32(int(x.col) - len(t.lexeme))
    return t
}

@(private="package")
lexer_scan_token :: proc(x: ^Lexer) -> Token {
    r := skip_whitespace(x)
    if is_eof(x) {
        return make_token(x, Token_Type.EOF)
    }
    strings.builder_reset(x.builder)

    // Lexeme start is the first non-whitespace, non-comment character.
    save_advance_rune(x)
    if is_alpha(r) {
        save_consume_proc(x, is_alphanumeric)
        token := make_token(x, Token_Type.Identifier)
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
consume_rune_multi :: proc(x: ^Lexer, r: rune, $save: bool) -> (n: int) {
    for !is_eof(x) {
        r2 := peek_rune(x)
        if r == r2 {
            __advance_rune(x, save=save)
            n += 1
        } else {
            break
        }
    }
    return n
}

/*
Continuously advances while the current character fulfills the predicate `p`.

**Parameters**
- p: The procedure which checks the current character.
 */
save_consume_proc :: proc(x: ^Lexer, $procedure: proc(rune) -> bool) {
    for !is_eof(x) {
        r  := peek_rune(x)
        ok := procedure(r)
        if ok {
            save_advance_rune(x)
        } else {
            break
        }
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
            save_advance_rune(x)
            save_consume_proc(x, is_alphanumeric)
            token := make_token(x, Token_Type.Number)
            i, ok := strconv.parse_uint(token.lexeme[2:], base)
            if !ok {
                __error(x, "Malformed integer")
            }
            f := f64(i)
            // `i` as an `f64` might not be accurately represented?
            if uint(f) != i {
                __error(x, "Invalid f64 integer")
            }
            token.number = f
            return token
        }
    }

    save_consume_proc(x, is_number)

    // The decimal (radix) point can only come after the integer portion,
    // or the number sequence.
    for save_match_rune(x, '.') {
        save_consume_proc(x, is_number)
    }

    // Explicit scientific-notation exponents can come after the integer
    // portion or the decimal portion.
    if save_match_either_rune(x, 'e', 'E') {
        save_match_either_rune(x, '+', '-')
    }

    // Consume any and all trailing characters even if they do not form
    // a valid number. This helps us inform the user ASAP.
    save_consume_proc(x, is_alphanumeric)

    token := make_token(x, Token_Type.Number)
    n, ok := strconv.parse_f64(token.lexeme)
    if !ok {
        __error(x, "Malformed number")
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
        // Save so that we can report errors, just in case.
        nest_open := consume_rune_multi(x, '=', save=true)
        if save_match_rune(x, '[') {
            // If we have a leading newline, skip it without saving.
            match_rune(x, '\n')

            // Clear builder of the initial nesting.
            strings.builder_reset(x.builder)
            col   := x.col
            count := consume_multi_sequence(x, nest_open, save=true)
            token := make_token(x, .String)
            token.col    = col
            token.lexeme = token.lexeme[:len(token.lexeme) - count]
            token.string = ostring_new(x.L, token.lexeme)
            return token
        }
        if nest_open > 0 {
            __error(x, "Expected a multiline string")
        }
        type = .Bracket_Open
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
        } else if save_match_proc(x, is_number) {
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
    return make_token(x, type)
}

make_string_token :: proc(x: ^Lexer, q: rune) -> Token {
    b := x.builder
    consume_loop: for {
        if is_eof(x) {
            __error(x, "Unfinished string")
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
                buf: [64]byte
                msg := fmt.bprintf(buf[:], "Unsupported escape sequence '%c%c'", r, esc)
                __error(x, msg)
            }
            __write(x, b, esc, esc_size)

        // Unescaped newline. If explicitly escaped then it's valid.
        case '\n':
            __error(x, "Unfinished string")
        case:
            __write(x, b, r, r_size)
        }
    }

    token := make_token(x, Token_Type.String)
    // Skip the opening quote.
    token.lexeme = token.lexeme[1:]
    token.string = ostring_new(x.L, token.lexeme)
    return token
}

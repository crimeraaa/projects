import gdb # type: ignore
from typing import Final

Token_Type: Final = gdb.lookup_type("enum lulu::[lexer.odin]::Token_Type")

_token_strings, _ = gdb.lookup_symbol("TOKEN_TYPE_STRINGS")
TOKEN_TYPE_STRINGS = _token_strings.value() # type: ignore
TOKEN_MODE = {
    Token_Type["Identifier"].enumval:   "string",
    Token_Type["String"].enumval:       "string",
    Token_Type["Number"].enumval:       "number",
}

class TokenPrinter:
    """
    ```
    struct lulu::[lexer.odin]::Token {
        enum lulu::[lexer.odin]::Token_Type type;
        struct string lexeme;
        union lulu::[lexer.odin]::Token_Data data;
        i32 line;
        i32 col;
    };

    enum lulu::[lexer.odin]::Token_Type : u8 { ... };
    union lulu::[lexer.odin]::Token_Data {
        f64 number;
        struct lulu::[ostring.odin]::Ostring *string;
    };
    ```
    """
    __type: str
    __data: gdb.Value | str
    __line: int
    __col:  int

    def __init__(self, token: gdb.Value):
        t = token["type"]
        i = int(t)
        self.__type = str(t)
        if i in TOKEN_MODE:
            self.__data = token["data"][TOKEN_MODE[i]]
        else:
            self.__data = None
        self.__line = int(token["line"])
        self.__col  = int(token["col"])

    def to_string(self) -> str:
        s = f"{self.__line}:{self.__col}: {self.__type}"
        if self.__data:
            s += f"({self.__data})"
        return s

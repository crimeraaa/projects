import gdb # type: ignore
from typing import Final

Token_Type: Final = gdb.lookup_type("enum lulu::[lexer.odin]::Token_Type")

_token_strings, _ = gdb.lookup_symbol("TOKEN_TYPE_STRINGS")
TOKEN_TYPE_STRINGS = _token_strings.value() # type: ignore

"""
```odin
```

```c
```
"""
TOKEN_MODE = {
    Token_Type["Identifier"].enumval:   "ostring",
    Token_Type["String"].enumval:       "ostring",
    Token_Type["Number"].enumval:       "number",
}

class TokenPrinter:
    """
    ```
    struct lulu::[lexer.odin]::Token {
        enum lulu::[lexer.odin]::Token_Type type;
        struct string lexeme;
        union lulu::[lexer.odin]::Token_Data data;
        int line;
    };

    enum lulu::[lexer.odin]::Token_Type : u8 { ... };
    union lulu::[lexer.odin]::Token_Data {
        f64 number;
        struct lulu::[ostring.odin]::Ostring *ostring;
    };
    ```
    """
    __type: gdb.Value
    __data: gdb.Value | str

    def __init__(self, token: gdb.Value):
        self.__type = token["type"]
        i = int(self.__type)
        if i in TOKEN_MODE:
            self.__data = token["data"][TOKEN_MODE[i]]
        else:
            self.__data = str(TOKEN_TYPE_STRINGS[i])

    def to_string(self) -> str:
        if isinstance(self.__data, str):
            return self.__data

        t = str(self.__type).lower()
        return f"{t}: {self.__data}"

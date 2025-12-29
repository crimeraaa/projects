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
mode = {
    Token_Type["Identifier"].enumval:  "v2",
    Token_Type["String"].enumval: "v2",
    Token_Type["Number"].enumval: "v1",
}

class TokenPrinter:
    """In Odin:
    ```
    Token :: struct {
        type:   Token_Type,
        lexeme: string,
        data:   Token_Data,
        line:   int,
    }
    Token_Type :: enum u8 { ... }
    Token_Data :: union { f64, ^Ostring }
    ```

    In C/C++:
    ```
    struct lulu::[lexer.odin]::Token {
        enum lulu::[lexer.odin]::Token_Type type;
        struct string lexeme;
        union lulu::[lexer.odin]::Token_Data data;
        int line;
    };
    
    enum lulu::[lexer.odin]::Token_Type : u8 { ... };
    union lulu::[lexer.odin]::Token_Data {
        u64 tag;
        f64 v1;
        struct lulu::[ostring.odin]::Ostring *v2;
    };
    ```
    """
    __type: gdb.Value
    __data: gdb.Value | str

    def __init__(self, token: gdb.Value):
        self.__type = token["type"]
        i = int(self.__type)
        if i in mode:
            self.__data = token["data"][mode[i]]
        else:
            self.__data = str(TOKEN_TYPE_STRINGS[i])

    def to_string(self) -> str:
        if isinstance(self.__data, str):
            return self.__data

        t = str(self.__type).lower()
        return f"{t}: {self.__data}"

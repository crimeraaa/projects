# This is just for type annotations; the `gdb` module only exists within GDB!
import gdb # type: ignore
from typing import Final
from printers import base
from . import opcode, lexer, expr, value

class __PrettyPrinter(gdb.printing.PrettyPrinter):
    """
    Usage:
    -   If lulu and its implementation was compiled into a standalone executable
        then create `lulu/bin/lulu-gdb.py` right next to `lulu/bin/lulu`.
    -   When invoking `gdb lulu/bin/lulu`, it will load `lulu/bin/lulu-gdb.py`
        if auto loading of scripts was enabled.

    -   Otherwise, if lulu was first compiled as a shared library, e.g.
        `lulu/bin/liblulu.so`, then create `lulu/bin/liblulu.so-gdb.py` instead.
    -   When `bin/liblulu.so` (NOT the main executable) is loaded into memory,
        GDB will load the Python script.

    Notes:
    -   GDB Python does NOT include the relative current directory in `sys.path`.
    -   We handle this by manually adding the lulu repo's directory to `sys.path`
        within `.gdbinit`.

    Sample:
    ```
    # lulu/bin/lulu-gdb.py or lulu/bin/liblulu.so-gdb.py
    import gdb
    from printers import odin, lulu

    inferior = gdb.current_objfile()
    gdb.printing.register_pretty_printer(inferior, lulu.pretty_printer)
    gdb.printing.register_pretty_printer(inferior, odin.pretty_printer)
    # Make sure to clean up the global namespace!
    del inferior
    ```
    """
    __printers: Final

    def __init__(self, name: str):
        """
        NOTE(2025-04-26):
        -   I'd love to use `gdb.printing.RegexpCollectionPrettyPrinter()`.
        -   However, based on the `__call__` implementation (e.g. in
            `/usr/share/gdb/python/gdb/printing.py`) it decares
            `typename = gdb.types.get_basic_type(val.type).tag`.
        -   This is bad for us, because pointers *don't* have tags!
        -   If that was `None`, it tries `typename = val.type.name`.
        -   That also doesn't work, because again, pointers don't have
            such information on their own.
        -   So even if our regex allows for pointers, the parent class
            literally does not allow you to work with pointers.
        """
        # Assuming demangled but fully-qualified names
        self.__printers = {
            "union lulu::[opcode.odin]::Instruction": opcode.InstructionPrinter,
            "struct lulu::[lexer.odin]::Token":       lexer.TokenPrinter,
            "struct lulu::[expr.odin]::Expr":         expr.ExprPrinter,
            "struct lulu::[value.odin]::Value":       value.Value_Printer,
            "struct lulu::[ostring.odin]::Ostring":   value.Ostring_Printer,
            "union lulu::[object.odin]::Object":      value.Object_Printer,


            # Pointers thereof (assume we never use arrays of these)
            "struct lulu::[lexer.odin]::Token *":     lexer.TokenPrinter,
            "struct lulu::[expr.odin]::Expr *":       expr.ExprPrinter,
            "union lulu::[object.odin]::Object *":    value.Object_Printer,
            # "union lulu::[object.odin]::Object_List *": value.Object_List_Printer,
            "union lulu::[gc.odin]::Gc_List *":       value.Gc_List_Printer,
            "struct lulu::[ostring.odin]::Ostring *": value.Ostring_Printer,
        }
        super().__init__(name, subprinters=base.subprinters(*list(self.__printers)))

    def __call__(self, v: gdb.Value):
        tag = str(v.type)
        if tag in self.__printers:
            return self.__printers[tag](v)
        return None


pretty_printer = __PrettyPrinter("lulu")

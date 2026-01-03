import gdb # type: ignore
from .. import base
from typing import Callable

def ensure_pointer(v: gdb.Value):
    return v if v.type.code == gdb.TYPE_CODE_PTR else v.address


class Ostring_Printer:
    """
    ```
    struct lulu::[ostring.odin]::Ostring {
        lulu::[object.odin]::Object_Header base;
        int  len;
        u32  hash;
        byte data[0];
    };
    ```
    """
    __value: gdb.Value

    def __init__(self, v: gdb.Value):
        self.__value = ensure_pointer(v)

    def to_string(self) -> str:
        n = int(self.__value["len"])
        s = self.__value["data"].string(encoding="utf-8", length=n)
        return s

    def display_hint(self) -> str:
        return "string"


class Object_Printer:
    __value: gdb.Value

    def __init__(self, v: gdb.Value):
        self.__value = ensure_pointer(v)

    def to_string(self):
        v = self.__value
        if not v:
            return "(null)"
        t = str(v["base"]["type"])
        if t == "string":
            return v["string"].address

        p = v.cast(base.VOID_POINTER)
        return f"{t.lower()}: {p}"


# In:  Object *
# Out: Ostring * | Table * | Chunk * | Closure * | Upvalue * | None
def object_get_data(node: gdb.Value):
    if not node:
        return None

    # Don't call the type() method; may crash
    t = str(node["base"]["type"]).lower()
    # p.type, if p is a pointer, returns None, annoyingly enough
    p = node[t].address
    if t == "function":
        kind = 'c' if p["c"]["is_c"] else "lua"
        p = p[kind].address
    return p

def object_iterator(node: gdb.Value, field = "next"):
    i = 0
    data = object_get_data(node)
    while node:
        yield str(i), data

        i += 1
        # field is "gc_list", this is assumed to be safe because only
        # objects with this member get linked into the gray list.
        # Otherwise it's "next" which is always safe.
        node = data[field]
        data = object_get_data(node)


class Object_List_Printer:
    __value: gdb.Value

    def __init__(self, v: gdb.Value):
        self.__value = ensure_pointer(v)

    def children(self):
        node = self.__value
        return object_iterator(node)

    def display_hint(self):
        return "array"


class GC_List_Printer:
    __value: gdb.Value

    def __init__(self, v: gdb.Value):
        self.__value = ensure_pointer(v)

    def children(self):
        node = self.__value
        return object_iterator(node, "gc_list")

    def display_hint(self):
        return "array"


class Value_Printer:
    """
    ```
    struct lulu::[value.odin]::Value {
        lulu::[value.odin]::Value_Type type;
        union {
            bool boolean;
            f64 number;
            lulu::[object.odin]::Object *object;
            void *pointer;
        } data;
    };
    ```
    """
    __type: gdb.Value
    __data: gdb.Value


    __TOSTRING: dict[str, Callable[[gdb.Value], str]] = {
        "nil":      lambda _: "nil",
        "boolean":  lambda v: str(bool(v["boolean"])),
        "number":   lambda v: str(float(v["number"])),
        "string":   lambda v: str(v["object"]["string"].address),
        "integer":  lambda v: str(int(v["integer"])),
    }

    def __init__(self, val: gdb.Value):
        # In GDB, enums are already pretty-printed to their names
        self.__type = val["type"]
        self.__data = val["data"]

    def to_string(self) -> str:
        t = str(self.__type).lower()
        if t in self.__TOSTRING:
            return self.__TOSTRING[t](self.__data)

        # Assumes data.pointer == (void *)data.object
        p = self.__data["pointer"]
        return f"{t}: {p}"



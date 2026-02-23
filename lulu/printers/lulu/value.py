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
        value = self.__value
        if not value:
            return "(null)"

        type_name = str(value["base"]["type"]).lower()
        member    = "closure" if type_name == "function" else type_name
        pointer   = value[member].address
        return pointer if type_name == "string" else f"{type_name}: {pointer}"

    # def children(self):
    #     actual, type = object_actual(self.__value)
    #     if actual:
    #         for field in type.fields():
    #             yield field.name, actual[field.name]


# def object_actual(node: gdb.Value):
#     if not node:
#         return

#     type_name = str(node["base"]["type"]).lower()
#     if type_name == "function":
#         closure = node["closure"]
#         pointer = closure["lua" if closure["base"]["is_lua"] else "api"].address
#     else:
#         pointer = node[type_name].address
#     return pointer, pointer.dereference().type


# In:  Object *
# Out: Ostring * | Table * | Chunk * | Closure * | Upvalue * | None
def object_iterator_next(node: gdb.Value):
    if not node:
        return None

    # Don't call the type() method; may crash
    type_name = str(node["base"]["type"]).lower()

    # p.type, if p is a pointer, returns None, annoyingly enough
    if type_name == "function":
        pointer = node["closure"]["base"].address
    else:
        pointer = node[type_name].address
    return pointer

def object_iterator(node: gdb.Value, field = "next"):
    i = 0
    data = object_iterator_next(node)
    while node:
        yield str(i), data

        i += 1
        node = data[field] if field == "gc_list" else data["base"][field]
        data = object_iterator_next(node)


class Object_List_Printer:
    __value: gdb.Value

    def __init__(self, v: gdb.Value):
        self.__value = ensure_pointer(v)

    def children(self):
        node = self.__value
        return object_iterator(node)

    def display_hint(self):
        return "array"


class Gc_List_Printer:
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
    __type: str
    __data: gdb.Value


    __TOSTRING: dict[str, Callable[[gdb.Value], str]] = {
        "nil":      lambda _: "nil",
        "boolean":  lambda v: str(bool(v["boolean"])),
        "number":   lambda v: str(float(v["number"])),
        "integer":  lambda v: str(int(v["integer"])),
        "pointer":  lambda v: "userdata: " + str(v["pointer"]),
    }

    def __init__(self, val: gdb.Value):
        # In GDB, enums are already pretty-printed to their names
        self.__type = str(val["type"]).lower()
        self.__data = val["data"]

    def to_string(self) -> str:
        if self.__type in self.__TOSTRING:
            return self.__TOSTRING[self.__type](self.__data)
        return str(self.__data["object"])



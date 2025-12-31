import gdb # type: ignore
from typing import Final, Literal

NO_JUMP: Final = -1

__INFO: Final[dict[str, str]] = {
    "Boolean":      "boolean",
    "Number":       "number",
    "Constant":     "index",
    "Global":       "index",
    "Local":        "reg",
    "Indexed":      "table",
    "Pc_Pending_Register": "pc",
    "Register":     "reg",
    "Jump":         "pc",
    "Call":         "pc",
}

class ExprPrinter:
    __type:  str
    __data: gdb.Value

    def __init__(self, val: gdb.Value):
        self.__type = str(val["type"])
        self.__data = val["data"]

    def to_string(self) -> str:
        s    = self.__type
        memb = __INFO[s] if s in __INFO else None
        if memb:
            s = f"{self.__type}: "
            # if memb == "indexed":
            #     s += f"{self.__table('reg')}, {self.__table('field_rk')}"
            # else:
            s += f"{memb}={self.__data[memb]}"

        # s += self.__patch("patch_true")
        # s += self.__patch("patch_false")
        return s

    def __table(self, key: Literal["reg", "field_rk"]) -> str:
        v = self.__data["table"][key]
        return f"{key}={v}"

    def __patch(self, patch: Literal["patch_true", "patch_false"]) -> str:
        pc = self.__data[patch]
        if pc != -1:
            return f", {patch}={pc}"
        return ""


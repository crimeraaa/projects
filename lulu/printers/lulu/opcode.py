import gdb # type: ignore
from typing import Final
from enum import Enum

class OpCode_Format(Enum):
    ABC  = 0
    ABX  = 1
    ASBX = 2


opmodes: Final = {
    "Load_Const": OpCode_Format.ABX,
    "Get_Global": OpCode_Format.ABX,
    "Set_Global": OpCode_Format.ABX,
    "Jump":       OpCode_Format.ASBX,
    "For_Prep":   OpCode_Format.ASBX,
    "For_Loop":   OpCode_Format.ASBX,
    "Closure":    OpCode_Format.ABX,
}

class InstructionPrinter:
    __op:  str
    __a:   int
    __b:   int
    __c:   int
    __bx:  int
    __sbx: int

    def __init__(self, ip: gdb.Value):
        self.__op  = str(ip["base"]["op"])
        self.__a   = int(ip["base"]["a"])
        self.__b   = int(ip["base"]["b"])
        self.__c   = int(ip["base"]["c"])
        self.__bx  = int(ip["u"]["bx"])
        self.__sbx = int(ip["s"]["bx"])

    def to_string(self) -> str:
        out: list[str] = [f"{self.__op}: A={self.__a}"]
        if self.__op in opmodes:
            if opmodes[self.__op] == OpCode_Format.ASBX:
                out.append(f", sBx={self.__sbx}")
            else:
                out.append(f", Bx={self.__bx}")
        else:
            out.append(f", B={self.__b}, C={self.__c}")

        return ''.join(out)


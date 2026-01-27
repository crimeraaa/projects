import gdb # type: ignore
from typing import Final
from enum import Enum

class OpCode_Format(Enum):
    ABC  = 0
    ABx  = 1
    AsBx = 2


opmodes: Final = {
    "Load_Const":   OpCode_Format.ABx,
    "Get_Global":   OpCode_Format.ABx,
    "Set_Global":   OpCode_Format.ABx,
    "Jump":         OpCode_Format.AsBx,
    "Jump_Not":     OpCode_Format.AsBx,
    "For_Prep":     OpCode_Format.AsBx,
    "For_Loop":     OpCode_Format.AsBx,
    "Closure":      OpCode_Format.ABx,
}

class InstructionPrinter:
    __op:  str
    __A:   int
    __B:   int
    __C:   int
    __Bx:  int
    __sBx: int

    def __init__(self, ip: gdb.Value):
        self.__op  = str(ip["base"]["op"])
        self.__A   = int(ip["base"]["A"])
        self.__B   = int(ip["base"]["B"])
        self.__C   = int(ip["base"]["C"])
        self.__Bx  = int(ip["u"]["Bx"])
        self.__sBx = int(ip["s"]["Bx"])

    def to_string(self) -> str:
        out: list[str] = [f"{self.__op}: A={self.__A}"]
        if self.__op in opmodes:
            if opmodes[self.__op] == OpCode_Format.AsBx:
                out.append(f", sBx={self.__sBx}")
            else:
                out.append(f", Bx={self.__Bx}")
        else:
            out.append(f", B={self.__B}, C={self.__C}")

        return ''.join(out)


import gdb # type: ignore
from typing import Final
from enum import Enum

class OpCode_Format(Enum):
    ABC  = 0
    ABX  = 1
    ASBX = 2


opmodes: Final = {
    "Load_Constant": OpCode_Format.ABX,
    "Get_Global": OpCode_Format.ABX,
    "Set_Global": OpCode_Format.ABX,
    "Jump":       OpCode_Format.ASBX,
    "For_Prep":   OpCode_Format.ASBX,
    "For_Loop":   OpCode_Format.ASBX,
    "Closure":    OpCode_Format.ABX,
}

Opcode: Final = gdb.lookup_type("enum lulu::[opcode.odin]::Opcode")


class InstructionPrinter:
    SIZE_OP:   Final = 6
    SIZE_A:    Final = 8
    SIZE_B:    Final = 9
    SIZE_C:    Final = 9
    SIZE_BX:   Final = SIZE_B + SIZE_C

    MAX_OP:    Final = (1 << SIZE_OP) - 1
    MAX_A:     Final = (1 << SIZE_A) - 1
    MAX_B:     Final = (1 << SIZE_B) - 1
    MAX_C:     Final = (1 << SIZE_C) - 1
    MAX_BX:    Final = (1 << SIZE_BX) - 1
    MAX_SBX:   Final = MAX_BX >> 1

    __op: str
    __a:  int
    __b:  int
    __c:  int

    def __init__(self, ip: gdb.Value):
        self.__op = str(ip["op"])
        self.__a  = int(ip["a"])
        self.__b  = int(ip["b"])
        self.__c  = int(ip["c"])

    def to_string(self) -> str:
        out: list[str] = [f"{self.__op}: A={self.__a}"]
        if self.__op in opmodes:
            if opmodes[self.__op] == OpCode_Format.ASBX:
                out.append(f", sBX={self.__sbx}")
            else:
                out.append(f", BX={self.__bx}")
        else:
            out.append(f", B={self.__b}, C={self.__c}")

        return ''.join(out)

    @property
    def __bx(self) -> int:
        return (self.__b << self.SIZE_B) | self.__c

    @property
    def __sbx(self) -> int:
        return self.__bx - self.MAX_SBX


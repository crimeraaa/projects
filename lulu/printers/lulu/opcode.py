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

SIZE_OP:   Final = 6
SIZE_A:    Final = 8
SIZE_B:    Final = 9
SIZE_C:    Final = 9
SIZE_Bx:   Final = SIZE_B + SIZE_C

MAX_OP:    Final = (1 << SIZE_OP) - 1
MAX_A:     Final = (1 << SIZE_A) - 1
MAX_B:     Final = (1 << SIZE_B) - 1
MAX_C:     Final = (1 << SIZE_C) - 1
MAX_Bx:    Final = (1 << SIZE_Bx) - 1
MAX_sBx:   Final = MAX_Bx >> 1

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
        self.__bx  = int(ip["x"]["bx"])
        self.__sbx = int(ip["x"]["bx"]) - MAX_sBx

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


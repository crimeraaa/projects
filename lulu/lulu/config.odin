package lulu

// Dump chunks when done compiling?
LULU_DISASSEMBLE_CHUNK :: #config(LULU_DISASSEMBLE_CHUNK, false)

// Print disassembly inline with execution?
LULU_DISASSEMBLE_INLINE :: #config(LULU_DISASSEMBLE_INLINE, false)

LULU_DISASSEMBLE :: LULU_DISASSEMBLE_CHUNK || LULU_DISASSEMBLE_INLINE

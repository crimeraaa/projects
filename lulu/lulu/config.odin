package lulu

// Dump chunks when done compiling?
DISASSEMBLE_CHUNK :: #config(LULU_DISASSEMBLE_CHUNK, false)

// Print disassembly inline with execution?
DISASSEMBLE_INLINE :: #config(LULU_DISASSEMBLE_INLINE, false)

// Either or both disassembly options are enabled?
DISASSEMBLE :: DISASSEMBLE_CHUNK || DISASSEMBLE_INLINE

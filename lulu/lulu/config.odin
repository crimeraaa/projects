package lulu

// Dump chunks when done compiling?
DISASSEMBLE_CHUNK :: #config(LULU_DISASSEMBLE_CHUNK, false)

// Print disassembly inline with execution?
DISASSEMBLE_INLINE :: #config(LULU_DISASSEMBLE_INLINE, false)

// Either or both disassembly options are enabled?
DISASSEMBLE :: DISASSEMBLE_CHUNK || DISASSEMBLE_INLINE

DEBUG_STRESS_GC :: #config(LULU_DEBUG_STRESS_GC, false)
DEBUG_LOG_GC    :: #config(LULU_DEBUG_LOG_GC, false)

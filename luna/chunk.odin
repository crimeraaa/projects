#+private package
package luna

Chunk :: struct {
    code:      [dynamic]Instruction,
    constants: [dynamic]Value,
}

chunk_destroy :: proc(chunk: ^Chunk) {
    delete(chunk.code)
    delete(chunk.constants)
}

chunk_add_constant :: proc(chunk: ^Chunk, constant: Value) -> (index: u32) {
    index = u32(len(chunk.constants))
    append(&chunk.constants, constant)
    return index
}

code_ABC :: proc(chunk: ^Chunk, op: Opcode, A, B, C: u16) -> (pc: i32) {
    pc = i32(len(chunk.code))
    append(&chunk.code, Instruction{base={op=op, A=A, B=B, C=C}})
    return pc
}

code_ABx :: proc(chunk: ^Chunk, op: Opcode, A: u16, Bx: u32) -> (pc: i32) {
    pc = i32(len(chunk.code))
    append(&chunk.code, Instruction{u={op=op, A=A, Bx=Bx}})
    return pc
}

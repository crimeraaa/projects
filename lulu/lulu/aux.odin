/* 
Lulu Auxilliary API
 */
package lulu

type_name_at :: proc(L: ^State, index: int) -> string {
    t := type(L, index)
    return type_name(L, t)
}

Named_Proc :: struct {
    name:      string,
    procedure: Api_Proc,
}

/* 
Pushes a new table with `fields`.

**Side-effects**
- push: 1
- pop:  0
 */
set_library :: proc(L: ^State, fields: []Named_Proc) {
    for field in fields {
        push_api_proc(L, field.procedure)
        set_field(L, -2, field.name)
    }
}

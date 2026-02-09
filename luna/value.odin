#+private package
package luna

// Value_Type :: enum u8 {
//     // Nil,
//     // Boolean,
//     // Integer,
//     Number,
//     // String,
// }

// Value_Data :: struct #raw_union {
//     number: f64,
// }

Value :: union {
    bool, int, f64,
}

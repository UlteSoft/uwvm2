var observed: i64 = 0;
export fn numeric_probe(value: i32, decimal: f32) i64 {
    const wide: i64 = @as(i64, value);
    const decimal64: f64 = @as(f64, decimal);
    const shadow: i32 = 7;
    const sink: *volatile i64 = &observed;
    sink.* = wide; // ZIG_NUMERIC_READY
    if (decimal64 != 1.25) @trap();
    return wide + shadow;
}
pub export fn _start() callconv(.c) void {
    if (numeric_probe(3, 1.25) != 10) @trap();
}

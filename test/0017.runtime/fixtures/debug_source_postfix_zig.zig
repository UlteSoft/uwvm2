var value: i32 = 42;
export fn debug_postfix_zig(p: *const i32) i32 {
    return p.* + 1;
}
export fn _start() void {
    _ = debug_postfix_zig(&value);
}

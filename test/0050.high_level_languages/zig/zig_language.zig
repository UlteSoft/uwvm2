const std = @import("std");

const Counter = struct {
    value: i32,
    fn add(self: *Counter, amount: i32) i32 {
        self.value += amount;
        return self.value;
    }
};

export fn debug_value() i32 {
    var value: i32 = 40;
    value += 2; // DEBUG_BEFORE
    return value; // DEBUG_AFTER
}

fn kernel(iterations: u32) u32 {
    var value: u32 = 7;
    for (0..iterations) |index| value = (value *% 33 +% @as(u32, @intCast(index))) ^ 0x55aa;
    return value;
}

fn parseIterations(args: []const [:0]u8) !u32 {
    if (args.len == 1) return 1000;
    if (args.len != 2) return error.InvalidIterations;
    const value = std.fmt.parseInt(u32, args[1], 10) catch return error.InvalidIterations;
    if (value == 0 or value > 1000000) return error.InvalidIterations;
    return value;
}

pub fn main() !void {
    var state = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = state.deinit();
    const allocator = state.allocator();
    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);
    const iterations = parseIterations(args) catch {
        try std.io.getStdErr().writer().writeAll("zig invalid iterations\n");
        std.process.exit(2);
    };
    var object = Counter{ .value = 40 };
    const classes = object.add(2);
    const numbers = [_]i32{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
    var arrays: i32 = 0;
    for (numbers) |number| arrays += number;
    const copy = try allocator.dupe(i32, &numbers);
    defer allocator.free(copy);
    const text = "π你好🙂";
    const unicode = try std.unicode.utf8CountCodepoints(text);
    const rejected = std.fmt.parseInt(u32, "bad", 10) catch 42;
    const overflow = @addWithOverflow(@as(u32, 0xffffffff), 1);
    const value = @call(.never_inline, debug_value, .{});
    if (classes != 42 or arrays != 55 or copy[9] != 10 or unicode != 4 or text.len != 12 or rejected != 42 or overflow[1] != 1 or value != 42) return error.Semantics;
    try std.io.getStdOut().writer().print("zig classes={d} arrays={d} unicode=4 utf8=12 checks=2 value={d} iterations={d} checksum={d}\n", .{ classes, arrays, value, iterations, kernel(iterations) });
}

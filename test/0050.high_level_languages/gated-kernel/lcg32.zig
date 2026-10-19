const std = @import("std");

fn kernel(iterations: u32) u32 {
    var value: u32 = 7;
    for (0..iterations) |index| {
        value = (value *% 33 +% @as(u32, @intCast(index))) ^ 0x55aa;
    }
    return value;
}

fn fail(message: []const u8, status: u8) noreturn {
    std.io.getStdErr().writer().print("lcg32 {s}\n", .{message}) catch {};
    std.process.exit(status);
}

fn parseIterations(args: []const [:0]u8) u32 {
    if (args.len == 1) return 2000000000;
    if (args.len != 2 or args[1].len == 0 or args[1].len > 10) fail("invalid iterations", 2);
    for (args[1]) |digit| {
        if (digit < '0' or digit > '9') fail("invalid iterations", 2);
    }
    const value = std.fmt.parseInt(u32, args[1], 10) catch fail("invalid iterations", 2);
    if (value == 0 or value > 2000000000) fail("invalid iterations", 2);
    return value;
}

fn marker(text: []const u8) void {
    std.io.getStdErr().writer().writeAll(text) catch fail("marker output failed", 4);
}

fn gate(wanted: u8) void {
    const actual = std.io.getStdIn().reader().readByte() catch fail("invalid gate", 3);
    if (actual != wanted) fail("invalid gate", 3);
}

pub fn main() !void {
    var state = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = state.deinit();
    const allocator = state.allocator();
    const args = try std.process.argsAlloc(allocator);
    defer std.process.argsFree(allocator, args);
    const iterations = parseIterations(args);
    marker("UWVM_PROFILE_READY\n");
    gate('G');
    var timer = std.time.Timer.start() catch fail("clock unavailable", 4);
    const checksum = @call(.never_inline, kernel, .{iterations});
    marker("UWVM_PROFILE_DONE\n");
    const elapsed = timer.read();
    gate('D');
    try std.io.getStdOut().writer().print("lcg32 iterations={d} checksum={d}\nlcg32 kernel ns={d}\n", .{ iterations, checksum, elapsed });
}

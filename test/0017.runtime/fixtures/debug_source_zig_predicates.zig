const Packet = extern struct {
    seed: i32,
    enabled: bool,
    disabled: bool,
    grid: [2][3]i32,
};
var observed: i32 = 0;
var input: i32 = 3;
pub noinline fn predicate_probe(seed: i32, enabled: bool, disabled: bool) i32 {
    const packet = Packet{ .seed = seed, .enabled = enabled, .disabled = disabled,
        .grid = .{ .{ 10, 11, 12 }, .{ 13, 14, 15 } } };
    const sink: *volatile i32 = &observed;
    const live_packet: *volatile const Packet = &packet;
    sink.* = live_packet.grid[1][2]; // ZIG_PREDICATE_READY
    if (live_packet.seed != 3 or !live_packet.enabled or live_packet.disabled) @trap();
    return live_packet.grid[1][2] + live_packet.seed + 24;
}
pub export fn _start() callconv(.c) void {
    const source: *volatile i32 = &input;
    const seed = source.*;
    if (predicate_probe(seed, seed == 3, seed != 3) != 42) @trap();
}

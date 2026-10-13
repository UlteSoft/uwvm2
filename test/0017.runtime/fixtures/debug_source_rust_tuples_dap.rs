#![no_std]
#![crate_type = "cdylib"]

#[repr(C)]
struct Pair(i32, u32);
#[repr(C)]
struct TuplePacket {
    seed: i32,
    pair: (i32, u32),
    tuple_struct: Pair,
    nested: ((i32, u32), (u32, i32)),
    arrays: [(i32, u32); 2],
    next: *const TuplePacket,
}
static mut TUPLE_OBSERVED: i32 = 0;

#[unsafe(no_mangle)]
#[inline(never)]
pub extern "C" fn source_tuple_probe(seed: i32) -> i32 {
    let mut object = TuplePacket {
        seed,
        pair: (-9, 42),
        tuple_struct: Pair(-13, 55),
        nested: ((-7, 21), (34, -11)),
        arrays: [(-3, 8), (-5, 13)],
        next: core::ptr::null(),
    };
    object.next = &raw const object;
    unsafe { core::ptr::write_volatile(&raw mut TUPLE_OBSERVED, object.seed); } // OBJECT_TUPLE_DAP_STOP
    if object.next != &raw const object { core::arch::wasm32::unreachable(); }
    let pair_total = object.pair.0 + object.pair.1 as i32;
    let struct_total = object.tuple_struct.0 + object.tuple_struct.1 as i32;
    let nested_total = object.nested.0.0 + object.nested.0.1 as i32
        + object.nested.1.0 as i32 + object.nested.1.1;
    let arrays_total = object.arrays[0].0 + object.arrays[0].1 as i32
        + object.arrays[1].0 + object.arrays[1].1 as i32;
    if pair_total != 33 || struct_total != 42 || nested_total != 37 || arrays_total != 13 {
        core::arch::wasm32::unreachable();
    }
    object.seed + pair_total + struct_total + nested_total + arrays_total
}
#[unsafe(no_mangle)]
pub extern "C" fn _start() {
    let mut iteration = 0;
    while iteration < 4096 {
        if source_tuple_probe(5) != 130 { core::arch::wasm32::unreachable(); }
        iteration += 1;
    }
}
#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! { core::arch::wasm32::unreachable() }

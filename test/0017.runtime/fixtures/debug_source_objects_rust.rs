#![no_std]
#![crate_type = "cdylib"]
// Real Rust repr(C), fixed-array and signed-enum DWARF producer fixture.
#[repr(i32)]
#[derive(Clone, Copy)]
enum ObjectColor { Negative = -1, Warm = 3 }
#[repr(C)]
struct ObjectNode {
    value: i32,
    lanes: [u8; 3],
    color: ObjectColor,
    next: *const ObjectNode,
}
static mut OBJECT_OBSERVED: i32 = 0;
#[unsafe(no_mangle)]
#[inline(never)]
pub extern "C" fn source_objects_checkpoint_rust(pointer: *const ObjectNode) -> i32 {
    let object = unsafe { &*pointer };
    let result = object.value.wrapping_add(object.lanes[2] as i32).wrapping_add(object.color as i32);
    unsafe { core::ptr::write_volatile(&raw mut OBJECT_OBSERVED, result); }
    result
}
#[unsafe(no_mangle)]
#[inline(never)]
pub extern "C" fn source_objects_outer_rust(seed: i32) -> i32 {
    let mut object = ObjectNode { value: seed, lanes: [2, 4, 6], color: ObjectColor::Warm, next: core::ptr::null() };
    object.next = &raw const object;
    unsafe { core::ptr::write_volatile(&raw mut OBJECT_OBSERVED, object.value); } // OBJECT_RUST_STOP
    let result = source_objects_checkpoint_rust(&raw const object);
    result
}
#[unsafe(no_mangle)]
pub extern "C" fn _start() {
    if source_objects_outer_rust(5) != 14 { core::arch::wasm32::unreachable(); }
}
#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! { core::arch::wasm32::unreachable() }

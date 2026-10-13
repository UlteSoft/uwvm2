// Real Rust UTF32 char/array type DWARF. Wasm32 -Copt-level=0
// -Cdebuginfo=2 -Cdwarf-version=5 -Cpanic=abort, no host callbacks.
#![no_std]
#![crate_type = "cdylib"]
#[derive(Copy, Clone)]
#[repr(C)]
struct UtfPacketRust { point: char, pair: [char; 2] }
static mut UTF_OBSERVED_RUST: u32 = 0;
#[unsafe(no_mangle)]
#[inline(never)]
pub extern "C" fn debug_utf_outer_rust(seed: u32) -> u32 {
    let object = UtfPacketRust { point: '\u{1f642}', pair: ['\u{03bb}', '\u{10ffff}'] };
    let result = (object.point as u32).wrapping_add(object.pair[0] as u32)
        .wrapping_add(object.pair[1] as u32).wrapping_add(seed);
    unsafe { core::ptr::write_volatile(&raw mut UTF_OBSERVED_RUST, result); } // UTF_RUST_STOP
    result
}
#[unsafe(no_mangle)]
pub extern "C" fn _start() {
    if debug_utf_outer_rust(7) != 0x12fa03 { panic!("UTF fixture checksum"); }
}
#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! { loop {} }

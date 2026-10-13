#![no_std]
#![crate_type = "cdylib"]

static mut SEED: i32 = 7;
static mut OBSERVED: i32 = 0;

#[inline(always)]
fn source_inner_rust(inner_arg: i32) -> i32 {
    let adjusted = inner_arg.wrapping_add(5);
    adjusted.wrapping_mul(2)
}

#[no_mangle]
#[inline(never)]
pub extern "C" fn source_outer_rust(value: i32) -> i32 {
    let adjusted = source_inner_rust(value);
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, adjusted); }
    adjusted
}

#[no_mangle]
pub extern "C" fn _start() {
    let seed = unsafe { core::ptr::read_volatile(&raw const SEED) };
    let _ = source_outer_rust(seed);
}

#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! { loop {} }

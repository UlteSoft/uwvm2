#![no_std]
#![crate_type = "cdylib"]

static mut OBSERVED: i32 = 0;

#[no_mangle]
pub extern "C" fn _start() {
    let result = debug_source_rust(7);
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, result); }
}

#[no_mangle]
pub extern "C" fn debug_source_rust(value: i32) -> i32 {
    let adjusted = value.wrapping_add(5);
    adjusted.wrapping_mul(2)
}

#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! {
    loop {}
}

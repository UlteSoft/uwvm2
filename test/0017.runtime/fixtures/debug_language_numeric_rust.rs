#![no_std]
#![crate_type = "cdylib"]
static mut OBSERVED: f64 = 0.0;
#[unsafe(no_mangle)]
#[inline(never)]
pub extern "C" fn numeric_probe() {
    let decimal32 = 1.25f32;
    let decimal64 = -2.5f64;
    // Complete aligned live scalar borrows precede the volatile operations.
    let first = unsafe { core::ptr::read_volatile(&decimal32) };
    let second = unsafe { core::ptr::read_volatile(&decimal64) };
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, f64::from(first) + second); } // NUMERIC_READY
    if first != 1.25f32 || second != -2.5f64 { core::arch::wasm32::unreachable(); }
}
#[unsafe(no_mangle)]
pub extern "C" fn _start() { numeric_probe(); }
#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! { core::arch::wasm32::unreachable() }

#![no_std]
#![crate_type = "cdylib"]
#[repr(C)]
struct TupleProbe(i32, u32);
static mut OBSERVED: u32 = 0;
#[unsafe(no_mangle)]
#[inline(never)]
pub extern "C" fn debug_tuple_rust(seed: u32) -> u32 {
    let pair: (i32, u32) = (-9, 42);
    let tuple_struct = TupleProbe(-9, 42);
    core::hint::black_box(&pair);
    core::hint::black_box(&tuple_struct);
    let result = (pair.0 as u32).wrapping_add(pair.1)
        .wrapping_add(tuple_struct.0 as u32).wrapping_add(tuple_struct.1).wrapping_add(seed);
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, result); }
    result
}
#[unsafe(no_mangle)]
pub extern "C" fn _start() { let _ = debug_tuple_rust(7); }
#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! { loop {} }

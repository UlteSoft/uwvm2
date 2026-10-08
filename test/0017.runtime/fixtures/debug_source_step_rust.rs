#![no_std]
#![crate_type = "cdylib"]
static mut SEED: i32 = 5;
static mut OBSERVED: i32 = 0;
#[inline(always)]
fn source_step_inner(value: i32) -> i32 {
    let inner = value.wrapping_add(2);
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, inner); } // STEP_INNER_WRITE
    inner.wrapping_mul(2)
}
#[inline(always)]
fn source_step_middle(value: i32) -> i32 {
    let middle = source_step_inner(value);
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, middle); } // STEP_MIDDLE_AFTER
    middle.wrapping_add(3)
}
#[no_mangle]
#[inline(never)]
pub extern "C" fn source_step_leaf(value: i32) -> i32 {
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, value); } // STEP_LEAF_ENTRY
    value.wrapping_add(11)
}
#[no_mangle]
#[inline(never)]
pub extern "C" fn source_step_outer(value: i32) -> i32 {
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, value); } // STEP_BEFORE_INLINE
    let first = source_step_middle(value); // STEP_INLINE_ONE
    let second = source_step_middle(first); // STEP_INLINE_TWO
    let physical = source_step_leaf(second); // STEP_PHYSICAL_CALL
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, physical); } // STEP_AFTER_CALL
    physical
}
#[no_mangle]
#[inline(never)]
pub extern "C" fn source_step_recursive(remaining: i32, value: i32) -> i32 {
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, remaining); } // STEP_RECURSIVE_ENTRY
    if remaining == 0 { return value; }
    let previous = source_step_recursive(remaining - 1, value.wrapping_add(1));
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, previous); } // STEP_RECURSIVE_RETURN
    previous.wrapping_add(1)
}
#[no_mangle]
pub extern "C" fn _start() {
    let seed = unsafe { core::ptr::read_volatile(&raw const SEED) };
    let result = source_step_outer(seed);
    if result != 52 || source_step_recursive(3, result) != 58 { core::arch::wasm32::unreachable(); }
}
#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! { core::arch::wasm32::unreachable() }

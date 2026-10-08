#![no_std]
#![crate_type = "cdylib"]
// Real Rust O1/DWARF fixture. A noinline store keeps each labelled call in
// the producer's own file. The helper's core::ptr inline rows stay untouched.
static mut SEED: i32 = 5;
static mut OBSERVED: i32 = 0;
#[inline(never)]
fn source_run_to_store(value: i32) {
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, value); }
}
#[no_mangle]
#[inline(never)]
pub extern "C" fn source_step_leaf(value: i32) -> i32 {
    source_run_to_store(value); // STEP_LEAF_ENTRY
    value.wrapping_add(11)
}
#[no_mangle]
#[inline(never)]
pub extern "C" fn source_step_outer(value: i32) -> i32 {
    let first = value.wrapping_add(2).wrapping_mul(2).wrapping_add(3);
    let second = first.wrapping_add(2).wrapping_mul(2).wrapping_add(3);
    let physical = source_step_leaf(second); // STEP_PHYSICAL_CALL
    source_run_to_store(physical); // STEP_AFTER_CALL
    physical
}
#[no_mangle]
#[inline(never)]
pub extern "C" fn source_step_recursive(remaining: i32, value: i32) -> i32 {
    source_run_to_store(remaining); // STEP_RECURSIVE_ENTRY
    if remaining == 0 { return value; }
    let previous = source_step_recursive(remaining - 1, value.wrapping_add(1));
    source_run_to_store(previous); // STEP_RECURSIVE_RETURN
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

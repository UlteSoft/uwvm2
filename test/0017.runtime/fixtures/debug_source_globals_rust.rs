#![no_std]
pub mod outer {
    #[unsafe(no_mangle)]
    pub static PUBLIC_COUNTER: i32 = 19;
    static FILE_COUNTER: i32 = 23;
    #[inline(never)]
    pub fn read_file() -> i32 { unsafe { core::ptr::read_volatile(&FILE_COUNTER) } }
}
#[unsafe(no_mangle)]
#[inline(never)]
pub extern "C" fn debug_source_globals_rust(seed: i32) -> i32 {
    let public_counter = seed;
    public_counter + outer::read_file() + unsafe { core::ptr::read_volatile(&outer::PUBLIC_COUNTER) }
}
#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! { core::arch::wasm32::unreachable() }

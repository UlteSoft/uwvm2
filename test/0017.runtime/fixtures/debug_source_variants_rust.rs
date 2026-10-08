#![no_std]
// Real producer evidence for signed DWARF variant tags and niche default cases.
// O0 -g -Cdwarf-version=5; linked relocation-bearing objects must be re-read by
// the actual fresh LLVM-DWARF closure. Source intent is not qualification.
use core::num::NonZeroU32;
#[repr(C, i8)]
pub enum PayloadEnum {
    Negative { signed: i32 } = -3,
    Positive { unsigned: u32 } = 7,
    Empty = 11,
}
#[unsafe(no_mangle)]
#[inline(never)]
pub extern "C" fn debug_source_variants_rust(seed: i32) -> i32 {
    let choice = if seed < 0 {
        PayloadEnum::Negative { signed: seed }
    } else if seed == 0 {
        PayloadEnum::Empty
    } else {
        PayloadEnum::Positive { unsigned: seed as u32 }
    };
    let niche = NonZeroU32::new(seed as u32);
    let payload = match &choice {
        PayloadEnum::Negative { signed } => *signed,
        PayloadEnum::Positive { unsigned } => *unsigned as i32,
        PayloadEnum::Empty => 0,
    };
    payload + match &niche { Some(value) => value.get() as i32, None => 0 }
}
#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! { core::arch::wasm32::unreachable() }

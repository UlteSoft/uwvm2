#![no_std]
#![crate_type = "cdylib"]
use core::num::NonZeroU32;

#[repr(C, i8)]
#[derive(Clone, Copy)]
enum SignedPayload {
    Negative { signed: i32 } = -3,
    Positive { unsigned: u32 } = 7,
    Empty = 11,
}

#[repr(C)]
struct VariantPacket {
    seed: i32,
    negative: SignedPayload,
    positive: SignedPayload,
    empty: SignedPayload,
    some: Option<NonZeroU32>,
    none: Option<NonZeroU32>,
    grid: [[i32; 3]; 2],
    next: *const VariantPacket,
}
static mut VARIANT_OBSERVED: i32 = 0;

#[unsafe(no_mangle)]
#[inline(never)]
pub extern "C" fn source_variant_probe(seed: i32) -> i32 {
    let mut object = VariantPacket {
        seed,
        negative: SignedPayload::Negative { signed: -3 },
        positive: SignedPayload::Positive { unsigned: 7 },
        empty: SignedPayload::Empty,
        some: const { NonZeroU32::new(13) },
        none: None,
        grid: [[1, 2, 3], [4, 5, 6]],
        next: core::ptr::null(),
    };
    object.next = &raw const object;
    unsafe { core::ptr::write_volatile(&raw mut VARIANT_OBSERVED, object.seed); } // OBJECT_VARIANT_DAP_STOP
    let negative = match object.negative { SignedPayload::Negative { signed } => signed, _ => core::arch::wasm32::unreachable() };
    let positive = match object.positive { SignedPayload::Positive { unsigned } => unsigned as i32, _ => core::arch::wasm32::unreachable() };
    if !matches!(object.empty, SignedPayload::Empty) || !matches!(object.none, None) || object.next != &raw const object {
        core::arch::wasm32::unreachable();
    }
    let some = match object.some { Some(value) => value.get() as i32, None => core::arch::wasm32::unreachable() };
    object.seed + negative + positive + some + object.grid[0][0] + object.grid[0][1] + object.grid[0][2] + object.grid[1][0] + object.grid[1][1] + object.grid[1][2]
}

#[unsafe(no_mangle)]
pub extern "C" fn _start() {
    let mut iteration = 0;
    while iteration < 4096 {
        if source_variant_probe(5) != 43 { core::arch::wasm32::unreachable(); }
        iteration += 1;
    }
}

#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! { core::arch::wasm32::unreachable() }

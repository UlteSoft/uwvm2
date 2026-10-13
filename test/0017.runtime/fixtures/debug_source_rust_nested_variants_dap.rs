#![no_std]
#![crate_type = "cdylib"]
use core::num::NonZeroU32;

#[repr(C, i8)]
#[derive(Clone, Copy)]
enum Payload { Negative { signed: i32 } = -3, Positive { unsigned: u32 } = 7, Empty = 11 }
#[repr(C, u8)]
#[derive(Clone, Copy)]
enum Envelope { Carry { payload: Payload } = 2, Empty = 7 }
#[repr(C, u8)]
#[derive(Clone, Copy)]
enum OptionalEnvelope { Wrapped { payload: Option<NonZeroU32> } = 3, Absent = 9 }
#[repr(C, u8)]
#[derive(Clone, Copy)]
enum ArrayEnvelope { Packet { items: [i32; 2] } = 5, Absent = 13 }
#[repr(C, u8)]
#[derive(Clone, Copy)]
enum TopEnvelope { Top { payload: Envelope } = 1, Absent = 17 }
#[repr(C)]
struct NestedPacket {
    seed: i32,
    negative: Envelope,
    positive: Envelope,
    empty: Envelope,
    some: OptionalEnvelope,
    none: OptionalEnvelope,
    array: ArrayEnvelope,
    deep: TopEnvelope,
    next: *const NestedPacket,
}
static mut NESTED_OBSERVED: i32 = 0;

#[unsafe(no_mangle)]
#[inline(never)]
pub extern "C" fn source_nested_variant_probe(seed: i32) -> i32 {
    let mut object = NestedPacket {
        seed,
        negative: Envelope::Carry { payload: Payload::Negative { signed: -3 } },
        positive: Envelope::Carry { payload: Payload::Positive { unsigned: 7 } },
        empty: Envelope::Empty,
        some: OptionalEnvelope::Wrapped { payload: const { NonZeroU32::new(13) } },
        none: OptionalEnvelope::Wrapped { payload: None },
        array: ArrayEnvelope::Packet { items: [17, 19] },
        deep: TopEnvelope::Top { payload: Envelope::Carry { payload: Payload::Negative { signed: -3 } } },
        next: core::ptr::null(),
    };
    object.next = &raw const object;
    unsafe { core::ptr::write_volatile(&raw mut NESTED_OBSERVED, object.seed); } // OBJECT_NESTED_VARIANT_DAP_STOP
    let negative = match object.negative { Envelope::Carry { payload: Payload::Negative { signed } } => signed, _ => core::arch::wasm32::unreachable() };
    let positive = match object.positive { Envelope::Carry { payload: Payload::Positive { unsigned } } => unsigned as i32, _ => core::arch::wasm32::unreachable() };
    let some = match object.some { OptionalEnvelope::Wrapped { payload: Some(value) } => value.get() as i32, _ => core::arch::wasm32::unreachable() };
    let array = match object.array { ArrayEnvelope::Packet { items } => items[0] + items[1], _ => core::arch::wasm32::unreachable() };
    let deep = match object.deep { TopEnvelope::Top { payload: Envelope::Carry { payload: Payload::Negative { signed } } } => signed, _ => core::arch::wasm32::unreachable() };
    if !matches!(object.empty, Envelope::Empty) || !matches!(object.none, OptionalEnvelope::Wrapped { payload: None }) || object.next != &raw const object {
        core::arch::wasm32::unreachable();
    }
    object.seed + negative + positive + some + array + deep
}
#[unsafe(no_mangle)]
pub extern "C" fn _start() {
    let mut iteration = 0;
    while iteration < 4096 {
        if source_nested_variant_probe(5) != 55 { core::arch::wasm32::unreachable(); }
        iteration += 1;
    }
}
#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! { core::arch::wasm32::unreachable() }

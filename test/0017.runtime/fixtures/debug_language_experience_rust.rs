#![no_std]
#![crate_type = "cdylib"]
static mut OBSERVED: i32 = 0;
static mut OBSERVED_F32: f32 = 0.0;
static mut OBSERVED_F64: f64 = 0.0;
#[repr(C, i8)]
#[derive(Clone, Copy)]
enum LanguageChoice { Negative { payload: i32 } = -3, Positive { payload: i32 } = 7, Empty = 11 }
#[repr(C)]
struct LanguagePacket {
    tag: i32,
    grid: [[i32; 3]; 2],
    choice: LanguageChoice,
    niche: Option<core::num::NonZeroU32>,
}
#[inline(always)]
fn language_inner_rust(value: i32) -> i32 {
    let shadow = value.wrapping_add(101);
    // [live local shadow: aligned i32] end [live OBSERVED: i32] end
    // [safe                        ]     [safe             ] typed full borrows BEFORE reads/writes.
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, core::ptr::read_volatile(&shadow)); } // LANG_FRAME_SHADOW_INNER
    let inner_cookie = value.wrapping_add(1);
    // [live aligned OBSERVED: i32] end
    // [safe                     ] raw borrow/write is the complete owned static.
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, inner_cookie); } // LANG_INLINE_INNER
    inner_cookie.wrapping_mul(2)
}
#[inline(always)]
fn language_middle_rust(value: i32) -> i32 {
    let shadow = value.wrapping_add(201);
    // [live local shadow: aligned i32] end [live OBSERVED: i32] end
    // [safe                        ]     [safe             ] typed full borrows BEFORE reads/writes.
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, core::ptr::read_volatile(&shadow)); } // LANG_FRAME_SHADOW_MIDDLE
    let middle_cookie = language_inner_rust(value);
    // [live aligned OBSERVED: i32] end
    // [safe                     ] fixed static extent before raw borrow/write.
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, middle_cookie); } // LANG_INLINE_MIDDLE
    middle_cookie.wrapping_add(3)
}
// A real non-inlined call gives the optimized fixture an own-file statement
// while the constants remain in their compiler-defined lexical ranges.
#[inline(never)]
fn language_observe_leaf_rust(value: i32) {
    // [live aligned OBSERVED: i32] end
    // [safe                     ] complete static before raw typed write.
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, value); }
}
#[unsafe(no_mangle)]
#[inline(never)]
pub extern "C" fn language_leaf_rust(value: i32) -> i32 {
    let leaf_negative = -31i32;
    let leaf_positive = 23u32;
    let leaf_boolean = true;
    let leaf_fraction = 1.25f32;
    let leaf_double = -2.5f64;
    let leaf_zero = -0.0f32;
    let leaf_point = '\u{1f642}';
    // [live aligned OBSERVED: i32] end
    // [safe                     ] complete static BEFORE each raw borrow/write.
    unsafe {
        // Complete aligned statics before raw typed writes; no source-local
        // volatile borrow forces a storage location for these optimized values.
        core::ptr::write_volatile(&raw mut OBSERVED_F32, leaf_fraction);
        core::ptr::write_volatile(&raw mut OBSERVED_F64, leaf_double);
        core::ptr::write_volatile(&raw mut OBSERVED_F32, leaf_zero);
        core::ptr::write_volatile(&raw mut OBSERVED, leaf_point as i32);
        core::ptr::write_volatile(&raw mut OBSERVED, leaf_negative);
        core::ptr::write_volatile(&raw mut OBSERVED, leaf_positive as i32);
        core::ptr::write_volatile(&raw mut OBSERVED, leaf_boolean as i32); // LANG_CONSTANT_VALUES
    }
    let leaf_cookie = value.wrapping_add(11);
    // [live aligned OBSERVED: i32] end
    // [safe                     ] fixed static extent before raw borrow/write.
    language_observe_leaf_rust(leaf_cookie); // LANG_LEAF_READY
    leaf_cookie
} // LANG_LEAF_RETURN
#[unsafe(no_mangle)]
#[inline(never)]
pub extern "C" fn language_outer_rust(value: i32) -> i32 {
    let packet = LanguagePacket { tag: value, grid: [[10, 11, 12], [13, 14, 15]],
        choice: LanguageChoice::Negative { payload: -9 }, niche: core::num::NonZeroU32::new(5) };
    let shadow = 7;
    // Volatile reads force real field storage; the debugger cannot substitute
    // these source literals for the authenticated bytes at a real stopped PC.
    // [live packet: choice LanguageChoice / niche Option<NonZeroU32>] end
    // [safe                                                       ] typed fields
    //  ^^ each aligned borrow covers exactly its complete live field, no casts.
    let copied_choice = unsafe { core::ptr::read_volatile(&packet.choice) };
    let copied_niche = unsafe { core::ptr::read_volatile(&packet.niche) };
    if !matches!(copied_choice, LanguageChoice::Negative { payload: -9 }) || copied_niche.is_none() {
        core::arch::wasm32::unreachable();
    }
    // [packet.grid: 2 rows x 3 i32] end   [live OBSERVED: i32] end
    // [safe                     ]        [safe             ]
    //  ^^ row1 < 2, column2 < 3 checked by array indexing BEFORE borrow/read;
    //  complete fixed static destination proved BEFORE raw borrow/write.
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, core::ptr::read_volatile(&packet.grid[1][2])); } // LANG_OBJECT_READY
    // [live packet.tag: i32] end
    // [safe               ] typed aligned borrow/read of complete live field.
    let nested = language_middle_rust(unsafe { core::ptr::read_volatile(&packet.tag) }); // LANG_BEFORE_INLINE
    let child;
    {
        let shadow = 23;
        // [live OBSERVED: i32] end
        // [safe             ] complete static before raw borrow/write.
        unsafe { core::ptr::write_volatile(&raw mut OBSERVED, shadow); } // LANG_INNER_SHADOW
        child = language_leaf_rust(5); // LANG_PHYSICAL_CALL
        // [live OBSERVED: i32] end
        // [safe             ] complete static before raw borrow/write.
        unsafe { core::ptr::write_volatile(&raw mut OBSERVED, child); } // LANG_AFTER_CALL
    }
    // [live OBSERVED: i32] end
    // [safe             ] complete static before raw borrow/write.
    unsafe { core::ptr::write_volatile(&raw mut OBSERVED, shadow); } // LANG_OUTER_SHADOW
    // [packet.grid: 2 rows x 3 i32] end
    // [safe                     ] row1 < 2/column2 < 3 BEFORE borrow/read.
    unsafe { core::ptr::read_volatile(&packet.grid[1][2]) }.wrapping_add(nested).wrapping_add(child)
}
#[unsafe(no_mangle)]
pub extern "C" fn _start() {
    if language_outer_rust(3) != 42 { core::arch::wasm32::unreachable(); }
}
#[panic_handler]
fn panic(_: &core::panic::PanicInfo<'_>) -> ! { core::arch::wasm32::unreachable() }

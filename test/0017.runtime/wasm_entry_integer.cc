#include <uwvm2/uwvm/run/run.h>

#include <limits>

int main()
{
    using namespace ::uwvm2::uwvm::run;
    using wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
    using wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
    using u8string_view = ::uwvm2::utils::container::u8string_view;

    auto const accepts_i32{[](u8string_view text, wasm_i32 expected) constexpr noexcept
                           {
                               wasm_i32 value{};
                               return parse_wasm_entry_i32(text, value) && value == expected;
                           }};
    auto const rejects_i32{[](u8string_view text) constexpr noexcept
                           {
                               constexpr wasm_i32 sentinel{0x12345678};
                               wasm_i32 value{sentinel};
                               return !parse_wasm_entry_i32(text, value) && value == sentinel;
                           }};
    auto const accepts_i64{[](u8string_view text, wasm_i64 expected) constexpr noexcept
                           {
                               wasm_i64 value{};
                               return parse_wasm_entry_i64(text, value) && value == expected;
                           }};
    auto const rejects_i64{[](u8string_view text) constexpr noexcept
                           {
                               constexpr wasm_i64 sentinel{0x123456789abcdef};
                               wasm_i64 value{sentinel};
                               return !parse_wasm_entry_i64(text, value) && value == sentinel;
                           }};

    if(!accepts_i32(u8"0", 0)) { return 1; }
    if(!accepts_i32(u8"-1", -1)) { return 2; }
    if(!accepts_i32(u8"2147483647", ::std::numeric_limits<wasm_i32>::max())) { return 3; }
    if(!accepts_i32(u8"-2147483648", ::std::numeric_limits<wasm_i32>::min())) { return 4; }

    // Values outside signed decimal range are accepted through the unsigned scanner and preserve their raw wasm bits.
    if(!accepts_i32(u8"2147483648", ::std::numeric_limits<wasm_i32>::min())) { return 5; }
    if(!accepts_i32(u8"4294967295", wasm_i32{-1})) { return 6; }

    // Non-zero values ensure each prefixed scanner really follows a failed or partial decimal attempt.
    if(!accepts_i32(u8"0x80000000", ::std::numeric_limits<wasm_i32>::min())) { return 7; }
    if(!accepts_i32(u8"0b10", wasm_i32{2})) { return 8; }
    if(!accepts_i32(u8"0o17", wasm_i32{15})) { return 9; }

    if(!rejects_i32(u8"")) { return 10; }
    if(!rejects_i32(u8"123abc")) { return 11; }
    if(!rejects_i32(u8"0x")) { return 12; }
    if(!rejects_i32(u8"4294967296")) { return 13; }
    if(!rejects_i32(u8"-2147483649")) { return 14; }

    if(!accepts_i64(u8"-9223372036854775808", ::std::numeric_limits<wasm_i64>::min())) { return 15; }
    if(!accepts_i64(u8"18446744073709551615", wasm_i64{-1})) { return 16; }
    if(!accepts_i64(u8"0xffffffffffffffff", wasm_i64{-1})) { return 17; }
    if(!rejects_i64(u8"18446744073709551616")) { return 18; }
    if(!rejects_i64(u8"-9223372036854775809")) { return 19; }
}

/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#include <uwvm2/parser/wasm/standard/wasm1p1/features/call_indirect_immediate.h>

#include <cstddef>
#include <cstdint>

namespace
{
    using ::uwvm2::parser::wasm::standard::wasm1p1::features::parse_call_indirect_trailing_immediate;

    template <::std::size_t N>
    [[nodiscard]] constexpr bool expect_success(::std::byte const (&bytes)[N],
                                                bool mvp_reserved_zero_byte,
                                                ::std::uint_least32_t expected) noexcept
    {
        auto cursor{bytes};
        ::std::uint_least32_t table_index{0xa5a5a5a5u};
        return parse_call_indirect_trailing_immediate(cursor, bytes + N, mvp_reserved_zero_byte, table_index) &&
               cursor == bytes + N && table_index == expected;
    }

    template <::std::size_t N>
    [[nodiscard]] constexpr bool expect_failure(::std::byte const (&bytes)[N],
                                                ::std::size_t available,
                                                bool mvp_reserved_zero_byte) noexcept
    {
        auto cursor{bytes};
        ::std::uint_least32_t table_index{0xa5a5a5a5u};
        return !parse_call_indirect_trailing_immediate(cursor, bytes + available, mvp_reserved_zero_byte, table_index) &&
               cursor == bytes && table_index == 0xa5a5a5a5u;
    }

    inline constexpr ::std::byte canonical_zero[]{::std::byte{0x00}};
    inline constexpr ::std::byte padded_zero[]{::std::byte{0x80}, ::std::byte{0x00}};
    inline constexpr ::std::byte nonzero_table[]{::std::byte{0x81}, ::std::byte{0x01}};
    inline constexpr ::std::byte truncated[]{::std::byte{0x80}};
    inline constexpr ::std::byte max_u32[]{
        ::std::byte{0xff}, ::std::byte{0xff}, ::std::byte{0xff}, ::std::byte{0xff}, ::std::byte{0x0f}};
    inline constexpr ::std::byte overflowing_u32[]{
        ::std::byte{0xff}, ::std::byte{0xff}, ::std::byte{0xff}, ::std::byte{0xff}, ::std::byte{0x1f}};
    inline constexpr ::std::byte too_long[]{
        ::std::byte{0x80}, ::std::byte{0x80}, ::std::byte{0x80}, ::std::byte{0x80}, ::std::byte{0x80}, ::std::byte{0x00}};
    inline constexpr ::std::byte nonzero_mvp[]{::std::byte{0x01}};

    static_assert(expect_success(canonical_zero, false, 0u));
    static_assert(expect_success(padded_zero, false, 0u));
    // Feature policy is intentionally outside the decoder: a nonzero u32 remains a valid immediate here.
    static_assert(expect_success(nonzero_table, false, 129u));
    static_assert(expect_success(max_u32, false, 0xffffffffu));
    static_assert(expect_failure(truncated, 1u, false));
    static_assert(expect_failure(overflowing_u32, 5u, false));
    static_assert(expect_failure(too_long, 6u, false));

    static_assert(expect_success(canonical_zero, true, 0u));
    static_assert(expect_failure(padded_zero, 2u, true));
    static_assert(expect_failure(nonzero_mvp, 1u, true));
    static_assert(expect_failure(canonical_zero, 0u, true));
}

int main() {}

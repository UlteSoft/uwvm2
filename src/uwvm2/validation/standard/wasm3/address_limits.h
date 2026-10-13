/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include "memory_immediate.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Core 3.0 binary/types#limits and valid/types#{memory,table}-types.
    // The threads extension adds bit 1 only for memories, with a mandatory max.
    // Resource/host-size limits are instantiation concerns; validation preserves
    // all 64 bits, including the legal memory64 endpoint of 2^48 pages.
    enum class storage_address_type : unsigned { i32, i64 };
    enum class address_limits_kind : unsigned { memory, table };
    enum class address_limits_error : unsigned
    {
        ok, missing_flags, invalid_flags, address64_disabled, threads_disabled,
        shared_without_maximum, minimum_encoding, maximum_encoding,
        maximum_below_minimum, limit_out_of_range
    };
    struct address_limits
    {
        ::std::uint_least64_t min{};
        ::std::uint_least64_t max{0xffff'ffff'ffff'ffffull};
        bool present_max{};
        storage_address_type address_type{};
        bool shared{};
    };
    struct address_limits_result
    {
        address_limits limits{};
        address_limits_error error{};
        // Byte offset for a diagnostic; no borrowed pointer escapes on failure.
        ::std::size_t error_offset{};
    };

    [[nodiscard]] inline constexpr ::std::uint_least64_t address_limit_maximum(
        address_limits_kind kind, storage_address_type address_type) noexcept
    {
        if(kind == address_limits_kind::memory)
        { return address_type == storage_address_type::i64 ? 0x1'0000'0000'0000ull : 0x1'0000ull; }
        return address_type == storage_address_type::i64 ? 0xffff'ffff'ffff'ffffull : 0xffff'ffffull;
    }

    [[nodiscard]] inline constexpr address_limits_result scan_address_limits(
        ::std::byte const*& cursor, ::std::byte const* end, address_limits_kind kind,
        bool address64_enabled, bool threads_enabled, bool wide_encoding = true) noexcept
    {
        address_limits_result result{};
        // limits ... (end)
        // unsafe (could be end)
        // ^^ next borrows cursor's bounded range; cursor commits only on success.
        auto next{cursor};
        if(next == end) { result.error = address_limits_error::missing_flags; return result; }
        auto const flags{::std::to_integer<unsigned>(*next)};
        if(flags >= 8u || (kind == address_limits_kind::table && (flags & 2u)))
        { result.error = address_limits_error::invalid_flags; return result; }
        auto& limits{result.limits};
        limits.present_max = (flags & 1u) != 0u;
        limits.address_type = (flags & 4u) ? storage_address_type::i64 : storage_address_type::i32;
        limits.shared = (flags & 2u) != 0u;
        if(limits.shared && !limits.present_max)
        { result.error = address_limits_error::shared_without_maximum; return result; }
        if(limits.address_type == storage_address_type::i64 && !address64_enabled)
        { result.error = address_limits_error::address64_disabled; return result; }
        if(limits.shared && !threads_enabled)
        { result.error = address_limits_error::threads_disabled; return result; }
        ++next;
        // [flags] min max? ... (end)
        // [safe ] unsafe (could be end)
        //         ^^ next: the preceding end check proved the consumed byte.
        auto const scan{[&](::std::uint_least64_t& value) constexpr noexcept
        {
            // [consumed] limit ... (end)
            // [safe    ] unsafe (could be end); bounded scanner advances next only
            // on a complete integer, and never dereferences end.
            if(wide_encoding || limits.address_type == storage_address_type::i64)
            { return scan_memory_unsigned(next, end, value); }
            ::std::uint_least32_t narrow{};
            if(!scan_memory_unsigned(next, end, narrow)) { return false; }
            // [consumed limit] ... (end)
            // [safe          ] unsafe (could be end)
            //                  ^^ next after the bounded u32 scan.
            value = narrow;
            return true;
        }};
        result.error_offset = static_cast<::std::size_t>(next - cursor);
        if(!scan(limits.min)) { result.error = address_limits_error::minimum_encoding; return result; }
        // [flags min] max? ... (end)
        // [safe     ] unsafe (could be end)
        //             ^^ next; minimum was consumed without truncation.
        if(limits.present_max)
        {
            result.error_offset = static_cast<::std::size_t>(next - cursor);
            if(!scan(limits.max)) { result.error = address_limits_error::maximum_encoding; return result; }
            // [flags min max] ... (end)
            // [safe         ] unsafe (could be end)
            //                 ^^ next; maximum was consumed without truncation.
            if(limits.max < limits.min)
            { result.error = address_limits_error::maximum_below_minimum; return result; }
        }
        auto const maximum{address_limit_maximum(kind, limits.address_type)};
        if(limits.min > maximum || (limits.present_max && limits.max > maximum))
        { result.error = address_limits_error::limit_out_of_range; return result; }
        // [validated flags/min/max bytes] next ... end
        // [safe                         ] unsafe (possibly one-past)
        // ^^ cursor: still at the original limits start; next was bounded by scan.
        cursor = next;
        // [flags min max?] next ... (end)
        // [safe          ] unsafe (could be end)
        //                  ^^ cursor: commit the complete validated encoding.
        result.error_offset = 0uz;
        return result;
    }

    // Core 3 memory/table import matching. These are declared limits, without
    // implementation resource caps. Absent actual max cannot satisfy a required max.
    [[nodiscard]] inline constexpr bool address_limits_match(address_limits const& expected,
                                                              address_limits const& actual) noexcept
    {
        return expected.address_type == actual.address_type && expected.shared == actual.shared &&
            actual.min >= expected.min && (!expected.present_max || (actual.present_max && actual.max <= expected.max));
    }
    // memory.copy/table.copy use destination and source address types separately;
    // their length is i64 only when BOTH storage objects have i64 addresses.
    [[nodiscard]] inline constexpr storage_address_type copy_length_address_type(
        storage_address_type destination, storage_address_type source) noexcept
    {
        return destination == storage_address_type::i64 && source == storage_address_type::i64 ?
            storage_address_type::i64 : storage_address_type::i32;
    }
}

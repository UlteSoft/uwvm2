/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <fast_io.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Core 3 binary/instructions: memarg bit 6 adds a u32 memory index between
    // alignment and offset. Without the proposal, reserved memory indices are a
    // literal zero byte. The scanners commit only after a complete valid encoding.
    enum class memory_immediate_error : unsigned { ok, alignment, index, offset, feature_disabled };
    struct memory_argument
    {
        ::std::uint_least32_t alignment{}, memory_index{}, offset{};
        memory_immediate_error error{};
    };

    struct memory64_argument
    {
        ::std::uint_least32_t alignment{}, memory_index{};
        ::std::uint_least64_t offset{};
        memory_immediate_error error{};
    };

    template <typename T>
    inline constexpr bool scan_memory_unsigned(::std::byte const*& cursor, ::std::byte const* end, T& value) noexcept
    {
        // [consumed] unsigned LEB ... (end)
        // [safe    ] unsafe (could be end)
        //            ^^ cursor: only the bounded parser may inspect the remaining bytes.
        auto const [next, error]{::fast_io::parse_by_scan(reinterpret_cast<char8_t const*>(cursor),
            reinterpret_cast<char8_t const*>(end), ::fast_io::mnp::leb128_get(value))};
        if(error != ::fast_io::parse_code::ok) { return false; }
        cursor = reinterpret_cast<::std::byte const*>(next);
        // [consumed unsigned LEB] ... (end)
        // [safe                 ] unsafe (could be end)
        //                         ^^ cursor: parser returned a position in the original range.
        return true;
    }

    inline constexpr memory_immediate_error scan_memory_index(::std::byte const*& cursor, ::std::byte const* end,
                                                              bool multi_memory, ::std::uint_least32_t& index) noexcept
    {
        if(multi_memory)
        {
            // [consumed] memidx ... (end)
            // [safe    ] unsafe (could be end); scanner commits a bounded u32 on success only.
            return scan_memory_unsigned(cursor, end, index) ? memory_immediate_error::ok : memory_immediate_error::index;
        }
        if(cursor == end) { return memory_immediate_error::index; }
        // [consumed] 00 ... end
        // [safe    ][safe] unsafe (could be end)
        //           ^^ cursor: cursor != end proves this literal byte is available.
        if(*cursor != ::std::byte{}) { return memory_immediate_error::feature_disabled; }
        // [consumed 0x00] ... (end)
        // [safe         ] unsafe (could be end)
        //           ^^ cursor: the equality/end checks proved one literal byte available.
        ++cursor;
        // [consumed 0x00] ... (end)
        // [safe         ] unsafe (could be end)
        //                 ^^ cursor
        index = 0u;
        return memory_immediate_error::ok;
    }

    // Core 3 decodes a u64 offset regardless of the selected memory's address
    // type. The caller must subsequently require offset < 2^32 for memory32.
    // wide_encoding=false is reserved for the legacy u32 binary grammar.
    [[nodiscard]] inline constexpr memory64_argument scan_memory_argument64(::std::byte const*& code_curr,
        ::std::byte const* code_end, bool multi_memory, bool wide_encoding = true) noexcept
    {
        memory64_argument result{};
        // memarg ... (code_end)
        // unsafe (could be code_end)
        // ^^ cursor borrows code_curr's range; code_curr is unchanged on every failure.
        auto cursor{code_curr};
        if(!scan_memory_unsigned(cursor, code_end, result.alignment)) { result.error = memory_immediate_error::alignment; return result; }
        // [alignment] [memidx? offset ...] (code_end)
        // [safe     ] unsafe (could be code_end)
        //             ^^ cursor
        if(result.alignment >= 128u) { result.error = memory_immediate_error::alignment; return result; }
        if((result.alignment & 64u) != 0u)
        {
            if(!multi_memory) { result.error = memory_immediate_error::feature_disabled; return result; }
            result.alignment -= 64u;
            if(!scan_memory_unsigned(cursor, code_end, result.memory_index)) { result.error = memory_immediate_error::index; return result; }
            // [alignment memidx] offset ... (code_end)
            // [safe            ] unsafe (could be code_end)
            //                    ^^ cursor: complete bounded u32 index consumed.
        }
        if(wide_encoding)
        {
            if(!scan_memory_unsigned(cursor, code_end, result.offset))
            { result.error = memory_immediate_error::offset; return result; }
        }
        else
        {
            ::std::uint_least32_t offset{};
            if(!scan_memory_unsigned(cursor, code_end, offset))
            { result.error = memory_immediate_error::offset; return result; }
            result.offset = offset;
        }
        // [alignment memidx? offset] next ... (code_end)
        // [safe                   ] unsafe (could be code_end)
        //                           ^^ cursor: all fields decoded; no offset bits discarded.
        // memory argument immediate ... code_end
        // [safe consumed bytes] unsafe (could be code_end)
        // ^^ code_curr: held at the pre-commit position; the right-hand scan proved its target.
        code_curr = cursor;
        // [alignment memidx? offset] next ... (code_end)
        // [safe                   ] unsafe (could be code_end)
        //                           ^^ code_curr: commit without dereferencing the next byte.
        return result;
    }

    [[nodiscard]] inline constexpr memory_argument scan_memory_argument(::std::byte const*& code_curr,
        ::std::byte const* code_end, bool multi_memory) noexcept
    {
        // memarg ... (code_end)
        // unsafe (could be code_end)
        // ^^ cursor borrows code_curr; even a valid u64 that exceeds the selected
        // memory32 range must leave code_curr unchanged.
        auto cursor{code_curr};
        // Core 3 keeps the u64 wire encoding even when memory64 and multi-memory are off.
        // The memory32 range check below is a validation rule, not a decoder width.
        auto const wide{scan_memory_argument64(cursor, code_end, multi_memory, true)};
        // [memarg] ... (code_end) only when wide.error == ok
        // [safe  ] unsafe (could be code_end)
        //          ^^ cursor, checked by the shared decoder.
        memory_argument result{wide.alignment, wide.memory_index, 0u, wide.error};
        if(result.error != memory_immediate_error::ok) { return result; }
        if(wide.offset > 0xffff'ffffull) { result.error = memory_immediate_error::offset; return result; }
        result.offset = static_cast<::std::uint_least32_t>(wide.offset);
        // memory argument immediate ... code_end
        // [safe consumed bytes] unsafe (could be code_end)
        // ^^ code_curr: held at the pre-commit position; the right-hand scan proved its target.
        code_curr = cursor;
        // [memarg] ... (code_end)
        // [safe  ] unsafe (could be code_end)
        //          ^^ code_curr: complete memory32 encoding and offset-range proof.
        return result;
    }
}

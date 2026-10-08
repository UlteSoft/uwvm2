/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>
#ifndef UWVM_MODULE
# include <fast_io.h>
# include <uwvm2/utils/macro/push_macros.h>
#endif

#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::parser::wasm::standard::wasm1p1::features
{
    /// Decode call_indirect's trailing immediate without applying table-count or feature-policy checks.
    ///
    /// Core 1.0 encodes one literal reserved 0x00 byte. Reference Types and Core 2.0 encode
    /// `tableidx ::= u32`, including permitted non-minimal trailing-zero ULEB128 encodings. The cursor and
    /// output value are committed only after the complete immediate has been validated.
    /// The opcode/type-index prefix is assumed to have been syntactically decoded by the caller;
    /// this helper proves and commits only the trailing immediate. At entry (the immediate may be section_end):
    /// call_indirect type_index table_index ...
    /// [          safe        ] unsafe (could be the section_end)
    ///                          ^^ code_curr
    /// On success (including when code_curr equals code_end):
    /// call_indirect type_index table_index ...
    /// [                safe              ] unsafe (could be the section_end)
    ///                                      ^^ code_curr
    /// On failure, code_curr remains at the entry position shown above and table_index is unchanged.
    [[nodiscard]] inline constexpr bool parse_call_indirect_trailing_immediate(
        ::std::byte const*& code_curr,
        ::std::byte const* code_end,
        bool mvp_reserved_zero_byte,
        ::std::uint_least32_t& table_index) noexcept
    {
        auto cursor{code_curr};
        if(mvp_reserved_zero_byte)
        {
            if(cursor == code_end || *cursor != ::std::byte{}) [[unlikely]] { return false; }
            ++cursor;
            table_index = 0u;
            code_curr = cursor;
            return true;
        }

        if(cursor == code_end) [[unlikely]] { return false; }
        // Bound the scanner to the five-byte WebAssembly u32 encoding limit.
        auto const remaining{static_cast<::std::size_t>(code_end - cursor)};
        auto const available{remaining < 5u ? remaining : 5u};
        auto const scan{[&](unsigned char const* first) constexpr noexcept
        {
            ::std::uint_least32_t decoded{};
            auto const [next, code]{::fast_io::parse_by_scan(first, first + available, ::fast_io::mnp::leb128_get(decoded))};
            if(code != ::fast_io::parse_code::ok || decoded > 0xffff'ffffu) [[unlikely]] { return false; }
            table_index = decoded;
            code_curr += next - first;
            return true;
        }};
        if UWVM_IF_CONSTEVAL
        {
            unsigned char buffer[5]{};
            for(::std::size_t i{}; i != available; ++i)
            { buffer[i] = ::std::to_integer<unsigned char>(cursor[i]); }
            return scan(buffer);
        }
        else { return scan(reinterpret_cast<unsigned char const*>(cursor)); }
    }
}

#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/pop_macros.h>
#endif

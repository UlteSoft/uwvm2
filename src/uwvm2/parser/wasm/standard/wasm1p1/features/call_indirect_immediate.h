/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

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

        ::std::uint_least32_t decoded{};
        for(unsigned byte_index{}; byte_index != 5u; ++byte_index)
        {
            if(cursor == code_end) [[unlikely]] { return false; }

            auto const octet{::std::to_integer<::std::uint_least8_t>(*cursor)};
            ++cursor;
            auto const payload{static_cast<::std::uint_least8_t>(octet & 0x7fu)};

            // A u32 ULEB has at most five bytes; only four payload bits fit in its fifth byte.
            if(byte_index == 4u && (payload & 0x70u) != 0u) [[unlikely]] { return false; }
            decoded |= static_cast<::std::uint_least32_t>(payload) << (byte_index * 7u);

            if((octet & 0x80u) == 0u)
            {
                table_index = decoded;
                code_curr = cursor;
                return true;
            }
        }

        return false;
    }
}

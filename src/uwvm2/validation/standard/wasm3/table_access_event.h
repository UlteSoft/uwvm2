/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include "typed_stack_semantics.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // SAME first bounded tableidx decode and typed operation DATA. This cannot
    // authorize runtime table/source/native access; those owners remain private.
    struct validated_table_access_event
    {
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type element_type{};
        ::std::uint_least32_t table_index{};
        unsigned opcode{}, element_carrier{};
        bool address64{}, stack_polymorphic{};
        ::std::size_t source_offset{}, source_bytes{}, control_depth{};
    };
    [[nodiscard]] inline constexpr bool is_table_access_event_opcode(unsigned opcode) noexcept
    { return opcode == 0x25u || opcode == 0x26u; }
    template<unsigned Opcode, typename Count, typename Consume, typename MatchReference, typename Push>
    [[nodiscard]] inline constexpr typed_stack_sequence_result transition_table_access_event(
        validated_table_access_event& output, bool polymorphic,
        Count const& count, Consume const& consume, MatchReference const& match_reference, Push const& push,
        ::std::uint_least32_t table_index, bool address64,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type element_type,
        unsigned element_carrier, ::std::size_t source_offset, ::std::size_t source_bytes,
        ::std::size_t control_depth) noexcept(noexcept(push(element_type)))
    {
        static_assert(is_table_access_event_opcode(Opcode));
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        // The caller already checked feature, complete u32 LEB and declaration
        // index. These metadata checks neither replace that admission nor read bytes.
        if(element_type.kind != t::value_kind::reference ||
           (element_carrier != 0x70u && element_carrier != 0x6fu && element_carrier != 0x69u) ||
           source_bytes < 2uz || source_bytes > 6uz)
        { return {typed_stack_error::expected_reference}; }
        constexpr unsigned arity{Opcode == 0x25u ? 1u : 2u};
        auto const expected_at{[&](::std::uint_least32_t index) constexpr noexcept
        {
            // index is bounded by the common kernel's requested arity; set
            // consumes element first, then address. Get consumes only address.
            if constexpr(Opcode == 0x26u) { if(index == 0u) { return element_type; } }
            return t::core_value_type{address64 ? t::value_kind::i64 : t::value_kind::i32};
        }};
        auto const matches{[&](t::core_value_type actual, t::core_value_type expected) constexpr noexcept
        {
            if(expected.kind == t::value_kind::reference)
            { return actual.kind == t::value_kind::reference && match_reference(actual, expected); }
            return actual.kind == expected.kind;
        }};
        // FIRST full arity/current-frame prefix, then top-first matching exactly
        // once. Synthetic value Bot never consumes; known heap Bot is reference only.
        auto const result{pop_core3_operand_sequence(polymorphic, count, consume, arity, expected_at, matches)};
        if(result.error != typed_stack_error::ok) { return result; }
        if constexpr(Opcode == 0x25u) { push(element_type); }
        output = {.element_type = element_type, .table_index = table_index,
            .opcode = Opcode, .element_carrier = element_carrier, .address64 = address64,
            .stack_polymorphic = polymorphic, .source_offset = source_offset,
            .source_bytes = source_bytes, .control_depth = control_depth};
        return result;
    }
}

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
    // Checked DATA from the ONE original typed-select immediate and transition.
    // Neither source decode certificates nor native/runtime permissions are minted.
    struct validated_typed_select_event
    {
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type result_type{};
        unsigned opcode{0x1cu}, result_carrier{};
        bool stack_polymorphic{}, left_concrete{}, left_unknown{};
        ::std::size_t source_offset{}, source_bytes{}, control_depth{};
    };
    [[nodiscard]] inline constexpr bool typed_select_carrier_consistent(
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type type, unsigned carrier) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        switch(type.kind)
        {
            case t::value_kind::i32: return carrier == 0x7fu;
            case t::value_kind::i64: return carrier == 0x7eu;
            case t::value_kind::f32: return carrier == 0x7du;
            case t::value_kind::f64: return carrier == 0x7cu;
            case t::value_kind::v128: return carrier == 0x7bu;
            case t::value_kind::reference: return carrier == 0x70u || carrier == 0x6fu || carrier == 0x69u;
            default: return false;
        }
    }
    template<typename Count, typename Consume, typename MatchReference, typename Push>
    [[nodiscard]] inline constexpr typed_stack_sequence_result transition_typed_select_event(
        validated_typed_select_event& output, bool polymorphic,
        Count const& count, Consume const& consume, MatchReference const& match_reference, Push const& push,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type result_type, unsigned result_carrier,
        ::std::size_t source_offset, ::std::size_t source_bytes, ::std::size_t control_depth)
        noexcept(noexcept(push(result_type)))
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        // Count == 1 and the complete bounded valtype were already admitted by
        // the actual original decoder. Worst extent is opcode1 + u32 LEB5 + valtype6.
        if(!typed_select_carrier_consistent(result_type, result_carrier) ||
           source_bytes < 3uz || source_bytes > 12uz)
        { return {typed_stack_error::type_mismatch}; }
        ::std::uint_least32_t consumed{};
        bool left_concrete{}, left_unknown{};
        auto const owned_consume{[&]() constexpr noexcept
        {
            // Common whole-arity/frame suffix proof precedes each actual owned pop.
            // Only the third real pop is the left value; synthetic Bot never calls us.
            auto const actual{consume()};
            if(consumed == 2u) { left_concrete = true; left_unknown = actual.unknown; }
            ++consumed;
            return actual;
        }};
        auto const expected_at{[&](::std::uint_least32_t index) constexpr noexcept
        { return index == 0u ? t::core_value_type{t::value_kind::i32} : result_type; }};
        auto const matches{[&](t::core_value_type actual, t::core_value_type expected) constexpr noexcept
        {
            if(expected.kind == t::value_kind::reference)
            { return actual.kind == t::value_kind::reference && match_reference(actual, expected); }
            return actual.kind == expected.kind;
        }};
        // FIRST full three-operand arity, then condition/v2/v1 once; known concrete
        // values remain checked under polymorphism and reference-only Bot is not i32.
        auto const result{pop_core3_operand_sequence(polymorphic, count, owned_consume, 3u, expected_at, matches)};
        if(result.error != typed_stack_error::ok) { return result; }
        push(result_type);
        output = {.result_type = result_type, .result_carrier = result_carrier,
            .stack_polymorphic = polymorphic, .left_concrete = left_concrete, .left_unknown = left_unknown,
            .source_offset = source_offset, .source_bytes = source_bytes, .control_depth = control_depth};
        return result;
    }
}

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
    // SAME first typed transition DATA, never a Wasm/source/native capability.
    struct validated_integer_compare_event
    {
        unsigned opcode{};
        ::std::uint_least8_t popped{}, pushed{}, pop_bytes{}, push_bytes{};
        bool stack_polymorphic{};
        ::std::size_t source_offset{}, source_bytes{}, control_depth{};
    };
    [[nodiscard]] inline constexpr bool is_integer_compare_event_opcode(unsigned opcode) noexcept
    { return opcode >= 0x45u && opcode <= 0x5au; }
    template<unsigned Opcode, typename Count, typename Consume, typename Push>
    [[nodiscard]] inline constexpr typed_stack_sequence_result transition_integer_compare_event(
        validated_integer_compare_event& output, bool polymorphic,
        Count const& count, Consume const& consume, Push const& push,
        ::std::size_t source_offset, ::std::size_t control_depth)
        noexcept(noexcept(push(::uwvm2::parser::wasm::standard::wasm3::type::core_value_type{})))
    {
        static_assert(is_integer_compare_event_opcode(Opcode));
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        constexpr unsigned arity{Opcode == 0x45u || Opcode == 0x50u ? 1u : 2u};
        auto const expected_at{[](::std::uint_least32_t) constexpr noexcept
        { return t::core_value_type{Opcode <= 0x4fu ? t::value_kind::i32 : t::value_kind::i64}; }};
        auto const matches{[](t::core_value_type actual, t::core_value_type required) constexpr noexcept
        { return actual.kind == required.kind; }};
        // Exact original current-frame suffix is proved BEFORE any consumption.
        // Synthetic value Bot is accepted only by the common sequence kernel;
        // reference-only heap Bot remains a known wrong numeric type.
        auto const result{pop_core3_operand_sequence(polymorphic, count, consume, arity, expected_at, matches)};
        if(result.error != typed_stack_error::ok) { return result; }
        push(t::core_value_type{t::value_kind::i32});
        output = {.opcode = Opcode, .popped = arity, .pushed = 1u,
            .pop_bytes = static_cast<::std::uint_least8_t>(arity * (Opcode <= 0x4fu ? 4u : 8u)),
            .push_bytes = 4u, .stack_polymorphic = polymorphic,
            .source_offset = source_offset, .source_bytes = 1u, .control_depth = control_depth};
        return result;
    }
}

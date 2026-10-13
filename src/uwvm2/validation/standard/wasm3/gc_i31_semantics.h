/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstdint>
# include "typed_stack_semantics.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    struct core3_i31_signature
    {
        bool supported{};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type input{};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type output{};
    };
    [[nodiscard]] inline constexpr core3_i31_signature describe_core3_i31_instruction(
        ::std::uint_least32_t opcode) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        constexpr t::core_value_type i32{t::value_kind::i32};
        constexpr t::heap_type i31{static_cast<::std::int_least64_t>(t::abstract_heap_type::i31)};
        // Core 3 valid/instructions.html#scalar-reference-instructions.
        // These owned types describe grammar only, never runtime/source authority.
        if(opcode == 28u) { return {true, i32, {t::value_kind::reference, i31, false}}; }
        if(opcode == 29u || opcode == 30u) { return {true, {t::value_kind::reference, i31, true}, i32}; }
        return {};
    }
    struct core3_i31_transition_result
    {
        bool supported{};
        typed_stack_error error{};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type output{};
    };
    template<typename Count, typename Consume, typename Match>
    [[nodiscard]] inline constexpr core3_i31_transition_result apply_core3_i31_typed_transition(
        ::std::uint_least32_t opcode, bool polymorphic,
        Count const& count, Consume const& consume, Match const& matches) noexcept
    {
        auto const signature{describe_core3_i31_instruction(opcode)};
        if(!signature.supported) { return {}; }
        // The shared fixed-arity kernel proves the actual current-frame pop.
        // ExpectedAt/Consume are never invoked for synthetic polymorphic Bot.
        auto const expected{[&](::std::uint_least32_t) constexpr noexcept { return signature.input; }};
        auto const result{pop_core3_operand_sequence(polymorphic, count, consume, 1u, expected, matches)};
        return {true, result.error, signature.output};
    }
}

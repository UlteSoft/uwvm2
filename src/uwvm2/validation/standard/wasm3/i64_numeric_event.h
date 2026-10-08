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
    // Owned compiler DATA from the SAME successful fused typed transition.
    // There is no raw Wasm cursor, heap pointer or native execution authority.
    // Pure validation/int full+lazy/LLVM full+lazy use the one transition below.
    struct validated_i64_numeric_event
    {
        unsigned opcode{};
        ::std::uint_least8_t popped{}, pushed{}, pop_bytes{}, push_bytes{};
        // Current validation-frame stack polymorphism is not physical LLVM
        // reachability: entering a child frame resets it even under dead parents.
        bool stack_polymorphic{};
        ::std::size_t source_offset{}, source_bytes{}, control_depth{};
    };

    [[nodiscard]] inline constexpr bool is_i64_numeric_event_opcode(unsigned opcode) noexcept
    { return 0x79u <= opcode && opcode <= 0x8au; }

    // Core 3 numeric typing: i64.unop [i64]->[i64], i64.binop
    // [i64 i64]->[i64]. These eighteen primary opcodes have NO immediates.
    // The actual dispatcher already read and bounded that sole opcode byte.
    // First-arity/Bot/concrete frame semantics belong to the shared kernel;
    // no backend may repeat those checks or reparse an accepted source slice.
    template<unsigned Opcode, typename Count, typename Consume, typename Push>
    [[nodiscard]] inline constexpr typed_stack_sequence_result transition_i64_numeric_event(
        validated_i64_numeric_event& output, bool polymorphic,
        Count const& count, Consume const& consume, Push const& push,
        ::std::size_t source_offset, ::std::size_t control_depth)
        noexcept(noexcept(push(::uwvm2::parser::wasm::standard::wasm3::type::core_value_type{})))
    {
        static_assert(is_i64_numeric_event_opcode(Opcode));
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        constexpr auto arity{Opcode <= 0x7bu ? 1u : 2u};
        constexpr t::core_value_type expected{t::value_kind::i64};
        auto const expected_at{[](::std::uint_least32_t) constexpr noexcept { return t::core_value_type{t::value_kind::i64}; }};
        auto const matches{[](t::core_value_type actual, t::core_value_type required) constexpr noexcept
        {
            // The kernel admits VALUE Bot via owned operand.unknown. A concrete
            // reference heap Bot remains reference and cannot become numeric.
            return actual.kind == required.kind;
        }};
        auto const result{pop_core3_operand_sequence(polymorphic, count, consume,
            arity, expected_at, matches)};
        if(result.error != typed_stack_error::ok) { return result; }
        // Push owns the backend abstract-stack update only. Preserve its original
        // allocation/exception contract; no result/event is committed before it.
        push(expected);
        output = {.opcode = Opcode, .popped = static_cast<::std::uint_least8_t>(arity),
            .pushed = 1u, .pop_bytes = static_cast<::std::uint_least8_t>(arity * 8u), .push_bytes = 8u,
            .stack_polymorphic = polymorphic, .source_offset = source_offset, .source_bytes = 1uz,
            .control_depth = control_depth};
        return result;
    }
}

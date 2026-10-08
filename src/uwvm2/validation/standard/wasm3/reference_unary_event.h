/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <type_traits>
# include "typed_stack_semantics.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Owned compiler DATA, not a source seal, runtime lease or permission to
    // execute. The adapter normalizes the sole actual popped stack entry.
    struct core3_reference_operand
    {
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type type{};
        bool unknown{};
        unsigned carrier{};
    };
    struct core3_ref_is_null_event
    {
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type input_type{};
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type result_type{
            ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::i32};
        unsigned input_carrier{};
        bool input_unknown{};
        // True only when the common pop consumed an actual stack entry.
        // Unknown concrete values and synthetic empty Bot have different
        // physical pop effects; a later backend must not guess from top width.
        bool concrete_input{};
    };
    static_assert(::std::is_trivially_copyable_v<core3_ref_is_null_event>);
    struct core3_ref_is_null_transition_result
    {
        typed_stack_error error{};
        core3_ref_is_null_event event{};
    };
    template<typename Count, typename Consume>
    [[nodiscard]] inline constexpr core3_ref_is_null_transition_result apply_core3_ref_is_null_typed_transition(
        bool polymorphic, Count const& count, Consume const& consume) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        core3_ref_is_null_transition_result result{};
        core3_reference_operand actual{};
        auto const consume_actual{[&]() constexpr noexcept
        {
            // pop_core3_typed_operand calls this only after Count > 0.
            // Set the DATA bit at the actual consume boundary, never by
            // inspecting a raw body, source offset or guessed reference width.
            result.event.concrete_input = true;
            return consume();
        }};
        result.error = pop_core3_typed_operand(polymorphic, count, consume_actual, actual);
        if(result.error != typed_stack_error::ok) { return result; }
        // Core 3 pop_ref: no physical pop occurs for an empty polymorphic
        // suffix. Reify unknown as reference-only Bot, including a concrete
        // unknown operand; do not invent an external/internal ABI width.
        if(actual.unknown)
        {
            result.event.input_type = {t::value_kind::reference, {t::heap_type::bottom_code}, false};
            result.event.input_unknown = true;
            result.event.input_carrier = actual.carrier;
            return result;
        }
        // A concrete numeric operand remains invalid even in an unreachable
        // frame. Non-null references match ref null ht without any narrowing;
        // preserve exact heap/nullability and its existing physical ABI carrier.
        if(actual.type.kind != t::value_kind::reference ||
           (actual.carrier != 0x70u && actual.carrier != 0x6fu && actual.carrier != 0x69u))
        {
            result.error = typed_stack_error::expected_reference;
            return result;
        }
        result.event.input_type = actual.type;
        result.event.input_carrier = actual.carrier;
        return result;
    }
}

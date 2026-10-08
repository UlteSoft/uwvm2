/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include <limits>
# include <uwvm2/parser/wasm/standard/wasm3/type/recursive_type.h>
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    enum class typed_stack_error : unsigned { ok, stack_underflow, type_mismatch, expected_reference };

    // Core 3 appendix/algorithm.html: pop_val / pop_val(expect) / pop_ref.
    // Count returns only concrete values ABOVE the actual current control-frame base.
    // Consume returns an owned normalized {type, unknown} copy and removes exactly one
    // of those values. It is never called for the synthetic polymorphic remainder.
    // These callbacks are compiler adapters, not runtime/source/stop capabilities.
    template<typename Count, typename Consume, typename Operand>
    [[nodiscard]] inline constexpr typed_stack_error pop_core3_typed_operand(
        bool polymorphic, Count const& count, Consume const& consume, Operand& output) noexcept
    {
        if(count() == 0uz)
        {
            if(!polymorphic) { return typed_stack_error::stack_underflow; }
            output = {{}, true};
            return typed_stack_error::ok;
        }
        // [current frame base][one or more concrete owned operands] end
        // [preserved base    ][safe                               ] one-past
        // Count > 0 proves Consume may read/remove exactly the live top entry;
        // its copied type survives vector retirement. No source pointer moves.
        output = consume();
        return typed_stack_error::ok;
    }

    template<typename Count, typename Consume, typename Expected, typename Match>
    [[nodiscard]] inline constexpr typed_stack_error pop_core3_expected_operand(
        bool polymorphic, Count const& count, Consume const& consume,
        Expected const& expected, Match const& matches) noexcept
    {
        if(count() == 0uz)
        { return polymorphic ? typed_stack_error::ok : typed_stack_error::stack_underflow; }
        // Count > 0 proves one actual operand exists above the current frame.
        // Consume copies it before removal; matches borrows that local copy only.
        auto const actual{consume()};
        return actual.unknown || matches(actual.type, expected) ?
            typed_stack_error::ok : typed_stack_error::type_mismatch;
    }

    struct typed_stack_sequence_result
    {
        typed_stack_error error{};
        // This is an owned diagnostic index, never an operand/source capability.
        // Underflow/ok have no failing concrete operand and consume no synthetic entry.
        ::std::uint_least32_t failed_pop_index{(::std::numeric_limits<::std::uint_least32_t>::max)()};
    };

    template<typename Count, typename Consume, typename ExpectedAt, typename Match>
    [[nodiscard]] inline constexpr typed_stack_sequence_result pop_core3_operand_sequence(
        bool polymorphic, Count const& count, Consume const& consume,
        ::std::uint_least32_t requested, ExpectedAt const& expected_at, Match const& matches) noexcept
    {
        auto const available{count()};
        // Fixed-arity callers preflight the entire current-frame suffix before
        // consuming anything. Missing operands outrank any first-operand mismatch.
        if(!polymorphic && requested > available) { return {typed_stack_error::stack_underflow}; }
        auto const concrete{requested < available ? static_cast<::std::size_t>(requested) : available};
        for(::std::size_t i{}; i != concrete; ++i)
        {
            // i < concrete <= requested and i < original available prove both
            // the bounded expected metadata index and the concrete owned pop.
            // The callback enumerates EXPECTED types in top-first pop order.
            auto const index{static_cast<::std::uint_least32_t>(i)};
            auto const expected{expected_at(index)};
            auto const error{pop_core3_expected_operand(polymorphic, count, consume, expected, matches)};
            if(error != typed_stack_error::ok) { return {error, index}; }
        }
        // No ExpectedAt or Consume invocation occurs for polymorphic synthetic
        // remainder, including UINT32_MAX on an empty unreachable frame.
        return {};
    }

    template<typename Count, typename Consume, typename Expected, typename Match>
    [[nodiscard]] inline constexpr typed_stack_error pop_core3_repeated_operands(
        bool polymorphic, Count const& count, Consume const& consume,
        Expected const& expected, ::std::uint_least32_t requested, Match const& matches) noexcept
    {
        auto const expected_at{[&](::std::uint_least32_t) constexpr noexcept { return expected; }};
        return pop_core3_operand_sequence(polymorphic, count, consume, requested, expected_at, matches).error;
    }

    template<typename Operand>
    [[nodiscard]] inline constexpr typed_stack_error narrow_core3_non_null_reference(
        Operand const& actual,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type& output) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        if(actual.unknown)
        {
            // Value Bot reifies as REFERENCE-only heap Bot, never numeric Bot.
            output = {t::value_kind::reference, {t::heap_type::bottom_code}, false};
            return typed_stack_error::ok;
        }
        if(actual.type.kind != t::value_kind::reference) { return typed_stack_error::expected_reference; }
        output = actual.type;
        output.nullable = false;
        return typed_stack_error::ok;
    }
    template<typename Truncate>
    inline constexpr void make_core3_frame_unreachable(bool& polymorphic, Truncate const& truncate) noexcept
    {
        // The adapter owns the actual current control frame and proves its base
        // before Truncate. Remove only that frame's concrete operands first;
        // publishing polymorphism cannot hide unretired owned operands.
        truncate();
        polymorphic = true;
    }

}

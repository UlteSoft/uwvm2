#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include "threads.h"
# include "typed_stack_semantics.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif

UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Input was selected by the one bounded FE immediate decoder. Pop ordinal is
    // counted from the TOP (timeout/replacement/value before the address).
    // The sequence kernel proves top_ordinal < the requested arity before this
    // cold DATA classifier is called; no source or guest pointer is accepted.
    [[nodiscard]] inline constexpr ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type
    atomic_expected_operand_type(typed_atomic_instruction const& decoded, ::std::uint_least32_t top_ordinal) noexcept
    {
        namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
        auto const& descriptor{decoded.immediate.descriptor};
        if(top_ordinal + 1u == descriptor.operand_count)
        { return {decoded.address_type == storage_address_type::i64 ? type::value_kind::i64 : type::value_kind::i32}; }
        bool const wait{descriptor.kind == atomic_instruction_kind::wait32 || descriptor.kind == atomic_instruction_kind::wait64};
        return {descriptor.value_i64 || (wait && top_ordinal == 0u) ? type::value_kind::i64 : type::value_kind::i32};
    }

    // Same typed operand core for pure validation, integer full/lazy's full
    // translator, and LLVM full/lazy/tiered's full translator. Reachable total
    // arity is checked BEFORE consuming any value; polymorphic synthetic Bot is
    // handled by the common kernel and a reference-only heap Bot stays numeric-invalid.
    template<typename Count, typename Consume>
    [[nodiscard]] inline constexpr auto validate_atomic_operand_sequence(
        typed_atomic_instruction const& decoded, bool polymorphic,
        Count const& count, Consume const& consume) noexcept
    {
        return pop_core3_operand_sequence(polymorphic, count, consume,
            static_cast<::std::uint_least32_t>(decoded.immediate.descriptor.operand_count),
            [&](::std::uint_least32_t top_ordinal) constexpr noexcept
            { return atomic_expected_operand_type(decoded, top_ordinal); },
            [](auto const& actual, auto const& expected) constexpr noexcept
            { return actual.kind == expected.kind; });
    }
}

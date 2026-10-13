#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include "memory_validation.h"
# include "typed_stack_semantics.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // Selected memory address and scalar value metadata came from the original
    // bounded decoder/case. The matcher deliberately admits numeric equality
    // only; value Bot is handled separately by the common typed kernel.
    template<typename Count, typename Consume>
    [[nodiscard]] inline constexpr auto validate_scalar_memory_operand_sequence(
        typed_memory_argument const& decoded, bool store,
        ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type value_type,
        bool polymorphic, Count const& count, Consume const& consume) noexcept
    {
        namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
        type::core_value_type const address{decoded.address_type == storage_address_type::i64 ?
            type::value_kind::i64 : type::value_kind::i32};
        return pop_core3_operand_sequence(polymorphic, count, consume, store ? 2u : 1u,
            [&](::std::uint_least32_t ordinal) constexpr noexcept
            { return store && ordinal == 0u ? value_type : address; },
            [](auto const& actual, auto const& expected) constexpr noexcept
            { return actual.kind == expected.kind; });
    }
}


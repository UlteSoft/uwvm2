#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include "bulk_memory_event.h"
# include "typed_stack_semantics.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    [[nodiscard]] inline constexpr ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type
    bulk_memory_expected_operand_type(decoded_bulk_memory_instruction const& decoded,
                                      ::std::uint_least32_t top_ordinal) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        return {bulk_memory_expected_operand_address(decoded, top_ordinal) == storage_address_type::i64 ?
            t::value_kind::i64 : t::value_kind::i32};
    }

    // One canonical fixed-arity typed transition, reused by all three actual
    // validators/translators. No byte reader, type stack, or guest address is owned here.
    template<typename Count, typename Consume>
    [[nodiscard]] inline constexpr auto validate_bulk_memory_operand_sequence(
        decoded_bulk_memory_instruction const& decoded, bool polymorphic,
        Count const& count, Consume const& consume) noexcept
    {
        return pop_core3_operand_sequence(polymorphic, count, consume,
            static_cast<::std::uint_least32_t>(bulk_memory_operand_count(decoded)),
            [&](::std::uint_least32_t ordinal) constexpr noexcept
            { return bulk_memory_expected_operand_type(decoded, ordinal); },
            [](auto const& actual, auto const& expected) constexpr noexcept
            { return actual.kind == expected.kind; });
    }
}

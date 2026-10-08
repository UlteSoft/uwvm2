#pragma once
#ifndef UWVM_MODULE
# include <cstddef>
# include <cstdint>
# include "address_limits.h"
# include "typed_stack_semantics.h"
#endif
#ifndef UWVM_MODULE_EXPORT
# define UWVM_MODULE_EXPORT
#endif
UWVM_MODULE_EXPORT namespace uwvm2::validation::standard::wasm3
{
    // The selected address type is checked declaration DATA from the FIRST
    // bounded memory-index decoder. Size has no operand; grow consumes one at.
    template<typename Count, typename Consume>
    [[nodiscard]] inline constexpr auto validate_memory_page_operand_sequence(
        storage_address_type address_type, bool grow, bool polymorphic,
        Count const& count, Consume const& consume) noexcept
    {
        namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
        t::core_value_type const expected{address_type == storage_address_type::i64 ? t::value_kind::i64 : t::value_kind::i32};
        return pop_core3_operand_sequence(polymorphic, count, consume, grow ? 1u : 0u,
            [&](::std::uint_least32_t) constexpr noexcept { return expected; },
            [](auto const& actual, auto const& required) constexpr noexcept { return actual.kind == required.kind; });
    }
}

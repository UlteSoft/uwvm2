// Actual BMI/import qualification fixture; source only until keeper compiles.
module;
#include <cstddef>
#include <cstdint>
export module uwvm2.tests.core3_i32_numeric_event;
import uwvm2.validation.standard.wasm3;
import uwvm2.parser.wasm.standard.wasm3.type;
namespace v = ::uwvm2::validation::standard::wasm3;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
export inline constexpr bool imported_numeric_transition() noexcept
{
    ::std::size_t concrete{2uz}, consumed{}, pushed{};
    v::validated_i32_numeric_event event{};
    auto const result{v::transition_i32_numeric_event<0x6au>(event, false,
        [&]() constexpr noexcept { return concrete; },
        [&]() constexpr noexcept -> v::core3_operand
        { --concrete; ++consumed; return {{t::value_kind::i32}, false}; },
        [&](t::core_value_type value) constexpr noexcept
        { if(value.kind == t::value_kind::i32) { ++concrete; ++pushed; } },
        1uz, 1uz)};
    return result.error == v::typed_stack_error::ok && consumed == 2uz &&
        pushed == 1uz && concrete == 1uz && event.opcode == 0x6au &&
        event.source_bytes == 1uz && !event.stack_polymorphic;
}
static_assert(imported_numeric_transition());

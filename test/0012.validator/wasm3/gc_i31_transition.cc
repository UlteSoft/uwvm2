// Semantic component only. This does not qualify a compiler/VM/native producer.
#include <uwvm2/validation/standard/wasm3/gc_i31_semantics.h>
#include <uwvm2/validation/standard/wasm3/gc_validation.h>
#include <fast_io.h>
namespace v = ::uwvm2::validation::standard::wasm3;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::print(::fast_io::err(), "FAIL ", ::fast_io::mnp::dec(__LINE__), ": ", #x, ::fast_io::mnp::chvw('\n')); ::fast_io::fast_terminate(); } } while(false)
int main()
{
    constexpr t::core_value_type i32{t::value_kind::i32}, i64{t::value_kind::i64};
    constexpr t::core_value_type nullable_i31{t::value_kind::reference, {static_cast<::std::int_least64_t>(t::abstract_heap_type::i31)}, true};
    constexpr t::core_value_type nonnull_i31{t::value_kind::reference, nullable_i31.heap, false};
    v::recursive_type_context context{};
    auto const matches{[&](auto actual, auto expected) noexcept { return context.matches(actual, expected); }};
    auto const ref_i31{v::describe_core3_i31_instruction(28u)};
    CHECK(ref_i31.supported && ref_i31.input == i32 && ref_i31.output == nonnull_i31);
    for(unsigned op : {29u, 30u})
    {
        auto const get{v::describe_core3_i31_instruction(op)};
        CHECK(get.supported && get.input == nullable_i31 && get.output == i32);
    }
    for(unsigned op : {0u, 15u, 27u, 31u, UINT32_MAX})
    { CHECK(!v::describe_core3_i31_instruction(op).supported); }
    for(unsigned op : {28u, 29u, 30u})
    {
        v::core3_operand_stack stack{};
        stack.push(op == 28u ? i32 : nonnull_i31);
        auto const count{[&]() noexcept { return stack.size(); }};
        auto const consume{[&]() noexcept { v::core3_operand value{}; CHECK(stack.pop(value)); return value; }};
        auto const result{v::apply_core3_i31_typed_transition(op, false, count, consume, matches)};
        CHECK(result.supported && result.error == v::typed_stack_error::ok && stack.size() == 0uz);
        CHECK(result.output == (op == 28u ? nonnull_i31 : i32));
        CHECK(v::validate_core3_gc_instruction(stack, op, 0u, 0u, {}, context, false) == v::core3_reference_error::feature_disabled);
    }
    {
        ::std::size_t available{}, consumed{}, tested{};
        auto const count{[&]() noexcept { return available; }};
        auto const consume{[&]() noexcept { ++consumed; --available; return v::core3_operand{i64, false}; }};
        auto const match{[&](auto actual, auto wanted) noexcept { ++tested; return matches(actual, wanted); }};
        auto result{v::apply_core3_i31_typed_transition(28u, false, count, consume, match)};
        CHECK(result.supported && result.error == v::typed_stack_error::stack_underflow && consumed == 0uz && tested == 0uz);
        result = v::apply_core3_i31_typed_transition(28u, true, count, consume, match);
        CHECK(result.error == v::typed_stack_error::ok && consumed == 0uz && tested == 0uz && result.output == nonnull_i31);
        result = v::apply_core3_i31_typed_transition(31u, true, count, consume, match);
        CHECK(!result.supported && consumed == 0uz && tested == 0uz);
        available = 1uz;
        result = v::apply_core3_i31_typed_transition(28u, true, count, consume, match);
        CHECK(result.error == v::typed_stack_error::type_mismatch && consumed == 1uz && tested == 1uz && available == 0uz);
    }
    {
        // Reified reference heap Bot stays a concrete reference in polymorphic
        // code; it cannot masquerade as the synthetic numeric value Bot.
        v::core3_operand_stack stack{}; stack.make_unreachable();
        stack.push({t::value_kind::reference, {t::heap_type::bottom_code}, false});
        auto const count{[&]() noexcept { return stack.size(); }};
        auto const consume{[&]() noexcept { v::core3_operand value{}; CHECK(stack.pop(value)); return value; }};
        CHECK(v::apply_core3_i31_typed_transition(28u, true, count, consume, matches).error == v::typed_stack_error::type_mismatch);
        stack.push({t::value_kind::reference, {static_cast<::std::int_least64_t>(t::abstract_heap_type::none)}, true});
        CHECK(v::apply_core3_i31_typed_transition(29u, true, count, consume, matches).error == v::typed_stack_error::ok);
    }
    {
        v::core3_operand_stack stack{}; stack.push(i64); CHECK(stack.set_control_frame(1uz, false));
        // Actual stack.pop_expected already consumes the same shared kernel;
        // the isolated current frame cannot borrow its caller's i64 prefix.
        CHECK(v::validate_core3_gc_instruction(stack, 28u, 0u, 0u, {}, context, true) == v::core3_reference_error::stack_underflow);
        CHECK(stack.size() == 1uz);
    }
    ::fast_io::print(::fast_io::out(), "PASS Core3 i31 semantic component: ", ::fast_io::mnp::dec(checks), ::fast_io::mnp::chvw('\n'));
}

// Exercises the actual common Core 3 stack kernel through GC/EH/reference helpers.
// This is semantic component qualification, not a runtime/full-mode producer test.
#include <uwvm2/validation/standard/wasm3/gc_validation.h>
#include <uwvm2/validation/standard/wasm3/exception_validation.h>
#include <uwvm2/validation/standard/wasm3/recursive_type_binary.h>
#include <uwvm2/validation/standard/wasm3/reference_policy.h>
#include <uwvm2/parser/wasm/standard/wasm3/type/function_signature.h>
#include <fast_io.h>
#include <array>
#include <limits>
#include <memory>
#include <utility>
namespace v = ::uwvm2::validation::standard::wasm3;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
using e = v::core3_reference_error;
using ke = v::typed_stack_error;
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::print(::fast_io::err(), "FAIL ", ::fast_io::mnp::dec(__LINE__), ": ", #x, ::fast_io::mnp::chvw('\n')); ::fast_io::fast_terminate(); } } while(false)
[[nodiscard]] static t::core_value_type ref(t::abstract_heap_type heap, bool nullable) noexcept
{ return {t::value_kind::reference, {static_cast<::std::int_least64_t>(heap)}, nullable}; }
int main()
{
    v::recursive_type_context context{};
    auto const i32{t::core_value_type{t::value_kind::i32}};
    auto const i64{t::core_value_type{t::value_kind::i64}};
    auto const exn{ref(t::abstract_heap_type::exn, true)};
    auto const bottom{t::core_value_type{t::value_kind::reference, {t::heap_type::bottom_code}, false}};
    {
        // Frame base is preserved: reachable underflow does not consume the parent's i64.
        v::core3_operand_stack stack{}; stack.push(i64); CHECK(stack.set_control_frame(1uz, false));
        CHECK(stack.pop_expected(i32, context) == e::stack_underflow && stack.size() == 1uz);
        CHECK(stack.pop_repeated(i32, 2u, context) == e::stack_underflow && stack.size() == 1uz);
        stack.make_unreachable(); CHECK(stack.pop_expected(i32, context) == e::ok && stack.size() == 1uz);
        stack.push(i64); CHECK(stack.pop_expected(i32, context) == e::type_mismatch && stack.size() == 1uz);
        CHECK(stack.pop_repeated(i32, UINT32_MAX, context) == e::ok && stack.size() == 1uz);
    }
    {
        // Value Bot becomes non-null reference Bot, which accepts exn but rejects i32.
        v::core3_operand_stack stack{}; stack.make_unreachable();
        CHECK(v::validate_core3_ref_as_non_null(stack) == e::ok);
        v::core3_operand operand{}; CHECK(stack.pop(operand) && !operand.unknown && operand.type == bottom);
        stack.push(bottom); CHECK(stack.pop_expected(exn, context) == e::ok);
        stack.push(bottom); CHECK(stack.pop_expected(i32, context) == e::type_mismatch);
        stack.push(i32); CHECK(v::validate_core3_ref_as_non_null(stack) == e::expected_reference);
        stack.push(ref(t::abstract_heap_type::noexn, true));
        CHECK(v::validate_core3_ref_as_non_null(stack) == e::ok);
        CHECK(v::validate_core3_throw_ref(stack, context, true) == v::core3_exception_error::ok);
    }
    {
        // Real adapter precondition: the GC-disabled legacy parser leaves no
        // composite records. Null runtime context and empty parser context must
        // match the pure section adapter on every abstract hierarchy below.
        struct section_adapter
        {
            v::recursive_type_context core3_context{};
            ::uwvm2::utils::container::vector<t::owned_function_signature<::std::byte>> owned_signatures{};
        } section{};
        ::std::array const positives{
            ::std::pair{ref(t::abstract_heap_type::noexn, false), exn},
            ::std::pair{ref(t::abstract_heap_type::noextern, false), ref(t::abstract_heap_type::extern_, true)},
            ::std::pair{ref(t::abstract_heap_type::nofunc, false), ref(t::abstract_heap_type::func, true)},
            ::std::pair{ref(t::abstract_heap_type::i31, false), ref(t::abstract_heap_type::eq, true)},
            ::std::pair{ref(t::abstract_heap_type::none, false), ref(t::abstract_heap_type::any, true)},
            ::std::pair{bottom, exn}};
        for(auto const& pair : positives)
        {
            CHECK(v::core3_value_type_matches_in_section(pair.first, pair.second, section));
            CHECK(v::core3_value_type_matches_with_context(pair.first, pair.second, section.owned_signatures, nullptr));
            CHECK(v::core3_value_type_matches_with_context(pair.first, pair.second,
                section.owned_signatures, ::std::addressof(section.core3_context)));
        }
        ::std::array const negatives{
            ::std::pair{ref(t::abstract_heap_type::none, false), exn},
            ::std::pair{ref(t::abstract_heap_type::noextern, false), exn},
            ::std::pair{ref(t::abstract_heap_type::noexn, false), ref(t::abstract_heap_type::func, true)},
            ::std::pair{ref(t::abstract_heap_type::noexn, true), ref(t::abstract_heap_type::exn, false)},
            ::std::pair{bottom, i32}, ::std::pair{i64, i32}};
        for(auto const& pair : negatives)
        {
            CHECK(!v::core3_value_type_matches_in_section(pair.first, pair.second, section));
            CHECK(!v::core3_value_type_matches_with_context(pair.first, pair.second, section.owned_signatures, nullptr));
            CHECK(!v::core3_value_type_matches_with_context(pair.first, pair.second,
                section.owned_signatures, ::std::addressof(section.core3_context)));
        }
        // Preserve the legacy structural-function fallback for concrete heaps;
        // abstract matching must not manufacture aggregate metadata for it.
        section.owned_signatures.resize(2uz);
        for(auto& signature : section.owned_signatures) { signature.parameters.push_back(i32); }
        auto const fn0{t::core_value_type{t::value_kind::reference, {0}, false}};
        auto const fn1{t::core_value_type{t::value_kind::reference, {1}, true}};
        CHECK(v::core3_value_type_matches_in_section(fn0, fn1, section));
        CHECK(v::core3_value_type_matches_with_context(fn0, fn1, section.owned_signatures, nullptr));
        section.owned_signatures.back_unchecked().parameters.back_unchecked() = i64;
        CHECK(!v::core3_value_type_matches_in_section(fn0, fn1, section));
        CHECK(!v::core3_value_type_matches_with_context(fn0, fn1, section.owned_signatures, nullptr));
    }
    for(auto heap : {t::abstract_heap_type::func, t::abstract_heap_type::nofunc,
                     t::abstract_heap_type::extern_, t::abstract_heap_type::noextern,
                     t::abstract_heap_type::any, t::abstract_heap_type::none})
    {
        // Concrete wrong reference families still fail above an unreachable frame.
        v::core3_operand_stack stack{}; stack.make_unreachable(); stack.push(ref(heap, true));
        CHECK(v::validate_core3_throw_ref(stack, context, true) == v::core3_exception_error::operand_type_mismatch);
    }
    {
        v::core3_operand_stack stack{}; stack.push(exn);
        CHECK(v::validate_core3_throw_ref(stack, context, false) == v::core3_exception_error::feature_disabled);
        CHECK(stack.size() == 1uz); // Feature rejection precedes actual stack mutation.
        CHECK(v::validate_core3_throw_ref(stack, context, true) == v::core3_exception_error::ok);
    }
    {
        // Direct callback boundary: huge polymorphic remainder makes zero Consume calls.
        ::std::size_t available{}, consumed{};
        auto const count{[&]() noexcept { return available; }};
        auto const consume{[&]() noexcept { ++consumed; --available; return v::core3_operand{i32, false}; }};
        auto const matches{[&](auto a, auto b) noexcept { return context.matches(a, b); }};
        CHECK(v::pop_core3_repeated_operands(true, count, consume, i32, UINT32_MAX, matches) == ke::ok && consumed == 0uz);
        available = 1uz; CHECK(v::pop_core3_repeated_operands(false, count, consume, i32, 2u, matches) == ke::stack_underflow);
        CHECK(available == 1uz && consumed == 0uz);
        CHECK(v::pop_core3_repeated_operands(true, count, consume, i32, UINT32_MAX, matches) == ke::ok);
        CHECK(available == 0uz && consumed == 1uz);
    }
    {
        // Fixed tuple semantics for upcoming atomic/bulk users: whole arity
        // underflow precedes a mismatched top; metadata is in top-first order.
        ::std::array values{v::core3_operand{i64, false}, v::core3_operand{i32, false}};
        ::std::array expected{i32, i64};
        ::std::size_t available{2uz}, consumed{}, expected_calls{};
        auto const count{[&]() noexcept { return available; }};
        auto const consume{[&]() noexcept
        {
            // Shared preflight/loop proves available > 0 before the decrement;
            // the resulting index remains inside the owned two-entry array.
            ++consumed; --available; return values[available];
        }};
        auto const at{[&](::std::uint_least32_t i) noexcept
        {
            // Actual declared arity and shared i < concrete bound this metadata borrow.
            CHECK(i < expected.size()); ++expected_calls; return expected[i];
        }};
        auto const matches{[&](auto a, auto b) noexcept { return context.matches(a, b); }};
        auto result{v::pop_core3_operand_sequence(false, count, consume, 2u, at, matches)};
        CHECK(result.error == ke::ok && consumed == 2uz && expected_calls == 2uz && available == 0uz);
        available = 1uz; consumed = expected_calls = 0uz;
        result = v::pop_core3_operand_sequence(false, count, consume, 2u, at, matches);
        CHECK(result.error == ke::stack_underflow && result.failed_pop_index == UINT32_MAX);
        CHECK(available == 1uz && consumed == 0uz && expected_calls == 0uz);
        result = v::pop_core3_operand_sequence(true, count, consume, 2u, at, matches);
        CHECK(result.error == ke::type_mismatch && result.failed_pop_index == 0u);
        CHECK(available == 0uz && consumed == 1uz && expected_calls == 1uz);
        consumed = expected_calls = 0uz;
        result = v::pop_core3_operand_sequence(true, count, consume, UINT32_MAX, at, matches);
        CHECK(result.error == ke::ok && consumed == 0uz && expected_calls == 0uz);
        // Same carrier-level number classes are insufficient: a real reference
        // Bot is a known reference value and cannot satisfy atomic numeric input.
        values[0] = {bottom, false}; available = 1uz;
        result = v::pop_core3_operand_sequence(true, count, consume, 1u, at, matches);
        CHECK(result.error == ke::type_mismatch && result.failed_pop_index == 0u);
    }
    {
        // Genuine parser/type-context GC array metadata; array.new_fixed validates
        // i31 <: eq, rejects numeric concrete tops, and bounds UINT32_MAX dead input.
        constexpr ::std::array bytes{::std::byte{1}, ::std::byte{0x5e}, ::std::byte{0x6d}, ::std::byte{1}};
        // [owned four-byte type section] end: adding its known array extent constructs
        // the same allocation's one-past endpoint. The scanner commits only bounded cursor updates.
        auto const* cursor{bytes.data()}; auto const* end{cursor + bytes.size()};
        t::recursive_type_section section{}; v::recursive_type_context gc_context{};
        CHECK(v::scan_core3_type_section(cursor, end, section).error == v::recursive_type_binary_error::ok && cursor == end);
        CHECK(v::validate_core3_type_section(section, gc_context).error == v::recursive_type_validation_error::ok);
        CHECK(section.groups.size() == 1uz && section.groups.front_unchecked().types.size() == 1uz);
        // The successful owned section counts prove this borrow; no pointer advances.
        auto const* definition{::std::addressof(section.groups.front_unchecked().types.front_unchecked())};
        ::std::array definitions{definition}; v::core3_gc_environment environment{definitions, {}, 0u};
        v::core3_operand_stack stack{}; stack.push(ref(t::abstract_heap_type::i31, false));
        CHECK(v::validate_core3_gc_instruction(stack, 8u, 0u, 1u, environment, gc_context, true) == e::ok);
        v::core3_operand output{}; CHECK(stack.pop(output) && output.type.kind == t::value_kind::reference && output.type.heap.code == 0 && !output.type.nullable);
        CHECK(v::validate_core3_gc_instruction(stack, 8u, 0u, 1u, environment, gc_context, true) == e::stack_underflow);
        stack.make_unreachable(); CHECK(v::validate_core3_gc_instruction(stack, 8u, 0u, UINT32_MAX, environment, gc_context, true) == e::ok);
        CHECK(stack.pop(output)); stack.push(i32);
        CHECK(v::validate_core3_gc_instruction(stack, 8u, 0u, UINT32_MAX, environment, gc_context, true) == e::type_mismatch);
        stack.push(i32); auto const size{stack.size()};
        CHECK(v::validate_core3_gc_instruction(stack, 8u, 0u, UINT32_MAX, environment, gc_context, false) == e::feature_disabled && stack.size() == size);
    }
    ::fast_io::print(::fast_io::out(), "PASS common Core 3 GC/EH/reference stack: ", ::fast_io::mnp::dec(checks), ::fast_io::mnp::chvw('\n'));
}

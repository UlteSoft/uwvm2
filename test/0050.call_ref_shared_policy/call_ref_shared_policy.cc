#include <array>
#include <cstddef>
#include <limits>
#include <fast_io.h>
#include <uwvm2/validation/standard/wasm3/reference_policy.h>
namespace
{
    namespace v = ::uwvm2::validation::standard::wasm3;
    namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
    struct operand
    {
        unsigned type{0x70u};
        bool from_stack{true}, is_unknown{}, is_reference_bottom{};
        ::std::size_t exact_function_type_index{SIZE_MAX};
        t::core_value_type core_type{};
        bool has_core_type{};
    };
    struct range { unsigned const* begin{}; unsigned const* end{}; };
    struct legacy_signature { range parameter{}, result{}; };
    void require(bool value, unsigned line)
    {
        if(!value)
        {
            ::fast_io::print(::fast_io::err(), "call_ref_shared_policy FAIL line=", ::fast_io::mnp::dec(line), "\n");
            ::fast_io::fast_terminate();
        }
    }
#define REQUIRE(value) require(static_cast<bool>(value), __LINE__)
    t::core_value_type ref(::std::int_least64_t heap, bool nullable = true)
    { return {t::value_kind::reference, {heap}, nullable}; }
}
int main()
{
    // Real validated declarations: open base, declared subtype, independent
    // final function with exactly the same scalar ABI. No forged interval IDs.
    t::recursive_type_section section{}; section.type_count = 3u;
    for(::std::size_t index{}; index != 3uz; ++index)
    {
        t::recursive_group group{}; group.first_type_index = index;
        t::sub_type definition{}; definition.kind = t::composite_kind::function;
        definition.final_ = index != 0uz;
        definition.parameters.push_back({t::value_kind::i32}); definition.results.push_back({t::value_kind::i32});
        if(index == 1uz) { definition.supertypes.push_back(0u); }
        group.types.push_back(::std::move(definition)); section.groups.push_back(::std::move(group));
    }
    v::recursive_type_context context{};
    REQUIRE(v::validate_core3_type_section(section, context).error == v::recursive_type_validation_error::ok);
    REQUIRE(context.records.size() == 3uz && context.matches(ref(1), ref(0)) && !context.matches(ref(2), ref(0)));
    ::std::array<unsigned, 1uz> i32{0x7fu}, i64{0x7eu};
    ::std::array<legacy_signature, 3uz> legacy{{
        {{i32.data(), i32.data() + i32.size()}, {i32.data(), i32.data() + i32.size()}},
        {{i32.data(), i32.data() + i32.size()}, {i32.data(), i32.data() + i32.size()}},
        {{i64.data(), i64.data() + i64.size()}, {i32.data(), i32.data() + i32.size()}}}};
    ::std::size_t rich_calls{}, legacy_reads{}, checks{};
    auto const rich{[&](auto actual, auto expected) noexcept
    { ++rich_calls; return context.matches(actual, expected); }};
    auto const at{[&](::std::size_t index) noexcept -> legacy_signature const&
    {
        REQUIRE(index < legacy.size()); ++legacy_reads;
        return legacy[index];
    }};
    auto const check{[&](operand value, ::std::size_t target, bool rich_available, bool expected)
    {
        REQUIRE(v::core3_call_ref_reference_matches(value, target, legacy.size(), rich_available, rich, at) == expected);
        ++checks;
    }};
    operand value{}; value.has_core_type = true; value.core_type = ref(1, false);
    check(value, 0uz, true, true); check(value, 1uz, true, true); check(value, 2uz, true, false);
    value.core_type = ref(2, false); value.exact_function_type_index = 1uz;
    check(value, 0uz, true, false); // ABI and legacy witness do not authorize another canonical heap.
    value.is_reference_bottom = true;
    check(value, 0uz, true, false); // Complete metadata takes precedence over stale legacy flags.
    value.exact_function_type_index = SIZE_MAX - 1uz;
    check(value, 0uz, true, false);
    value.core_type = ref(static_cast<::std::int_least64_t>(t::abstract_heap_type::noextern));
    check(value, 0uz, true, false);
    value.core_type = {t::value_kind::i32}; check(value, 0uz, true, false);
    value.is_reference_bottom = false; value.exact_function_type_index = SIZE_MAX; value.core_type = ref(0);
    check(value, 0uz, true, true); // call_ref permits nullable typed references; execution checks null.
    value.core_type = ref(static_cast<::std::int_least64_t>(t::abstract_heap_type::nofunc));
    check(value, 0uz, true, true);
    value.core_type = ref(static_cast<::std::int_least64_t>(t::abstract_heap_type::func));
    check(value, 0uz, true, false);
    value.core_type = ref(static_cast<::std::int_least64_t>(t::abstract_heap_type::exn));
    check(value, 0uz, true, false);
    value.core_type = ref(t::heap_type::bottom_code, false); check(value, 0uz, true, true);
    REQUIRE(legacy_reads == 0uz && rich_calls == 13uz);
    auto const old_rich_calls{rich_calls};
    value.is_unknown = true; check(value, 0uz, true, true); check(value, 3uz, true, false);
    value.is_unknown = false; value.from_stack = false; check(value, 0uz, true, true);
    REQUIRE(rich_calls == old_rich_calls && legacy_reads == 0uz);
    // Unchanged legacy function-only admission when rich metadata is absent.
    value = {}; value.exact_function_type_index = 1uz;
    check(value, 0uz, false, true); REQUIRE(legacy_reads == 2uz);
    check(value, 2uz, false, false); REQUIRE(legacy_reads == 4uz);
    value.type = 0x6fu; check(value, 0uz, false, false); REQUIRE(legacy_reads == 4uz);
    value = {}; value.exact_function_type_index = SIZE_MAX - 1uz;
    check(value, 0uz, false, true); REQUIRE(legacy_reads == 4uz);
    value.exact_function_type_index = SIZE_MAX; check(value, 0uz, false, false);
    value.is_reference_bottom = true; check(value, 0uz, false, true);
    REQUIRE(legacy_reads == 4uz && rich_calls == old_rich_calls);
    ::fast_io::print(::fast_io::out(), "CALL_REF_SHARED_POLICY checks=", ::fast_io::mnp::dec(checks),
        " canonical_context=1 rich_match_calls=", ::fast_io::mnp::dec(rich_calls),
        " legacy_reads=", ::fast_io::mnp::dec(legacy_reads), "\n");
}

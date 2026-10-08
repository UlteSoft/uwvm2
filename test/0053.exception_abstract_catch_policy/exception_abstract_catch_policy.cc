#include <fast_io.h>
#include <uwvm2/runtime/compiler/shared/wasm_exception_control.h>
namespace
{
    namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
    namespace v = ::uwvm2::validation::standard::wasm3;
    namespace c = ::uwvm2::runtime::compiler::shared::wasm_exception_control;
    namespace r = ::uwvm2::uwvm::runtime::storage;
    void require(bool condition, unsigned line)
    {
        if(!condition)
        {
            ::fast_io::print(::fast_io::err(), "exception_abstract_catch_policy FAIL line=", ::fast_io::mnp::dec(line), "\n");
            ::fast_io::fast_terminate();
        }
    }
#define REQUIRE(value) require(static_cast<bool>(value), __LINE__)
    t::core_value_type ref(t::abstract_heap_type heap, bool nullable = true)
    { return {t::value_kind::reference, {static_cast<::std::int_least64_t>(heap)}, nullable}; }
}
int main()
{
    // Exercise the actual runtime catch matcher in its three legitimate parser
    // representations. The populated context is obtained from real declared
    // types through the public type validator, never fabricated interval IDs.
    v::recursive_type_context empty{}, populated{};
    t::recursive_type_section section{}; section.type_count = 1u;
    t::recursive_group group{}; group.first_type_index = 0u;
    t::sub_type function{}; function.kind = t::composite_kind::function;
    function.parameters.push_back(ref(t::abstract_heap_type::noexn));
    group.types.push_back(::std::move(function)); section.groups.push_back(::std::move(group));
    REQUIRE(v::validate_core3_type_section(section, populated).error == v::recursive_type_validation_error::ok);
    v::recursive_type_context const* contexts[]{nullptr, ::std::addressof(empty), ::std::addressof(populated)};
    unsigned checks{};
    for(auto context : contexts)
    {
        r::type_section_storage_t types{}; types.core3_context_ptr = context;
        c::core3_matching matcher{::std::addressof(types)};
        auto const noexn{ref(t::abstract_heap_type::noexn)};
        auto const exn{ref(t::abstract_heap_type::exn)};
        auto const nonnull_exn{ref(t::abstract_heap_type::exn, false)};
        auto const extern_ref{ref(t::abstract_heap_type::extern_)};
        auto const check{[&](bool condition) { REQUIRE(condition); ++checks; }};
        check(matcher.matches(noexn, exn));
        check(!matcher.matches(exn, noexn));
        check(!matcher.matches(noexn, nonnull_exn));
        check(matcher.matches(ref(t::abstract_heap_type::noexn, false), nonnull_exn));
        check(!matcher.matches(noexn, extern_ref));
        check(!matcher.matches(extern_ref, exn));
        check(matcher.matches(ref(t::abstract_heap_type::noextern), extern_ref));
        check(!matcher.matches(ref(t::abstract_heap_type::noextern), exn));
        check(matcher.matches({t::value_kind::i64}, {t::value_kind::i64}));
        check(!matcher.matches({t::value_kind::i32}, {t::value_kind::i64}));
        auto const payload{[&](::std::size_t i) { REQUIRE(i == 0uz); return noexn; }};
        auto const label{[&](::std::size_t i) { REQUIRE(i == 0uz); return exn; }};
        check(v::validate_exception_catch_signature(v::exception_catch_kind::tagged,
            1uz, payload, 1uz, label, matcher) == v::core3_exception_error::ok);
        auto const label_ref{[&](::std::size_t i) { REQUIRE(i < 2uz); return i == 0uz ? exn : nonnull_exn; }};
        check(v::validate_exception_catch_signature(v::exception_catch_kind::tagged_ref,
            1uz, payload, 2uz, label_ref, matcher) == v::core3_exception_error::ok);
        auto const label_wrong_null{[&](::std::size_t i) { REQUIRE(i == 0uz); return nonnull_exn; }};
        check(v::validate_exception_catch_signature(v::exception_catch_kind::tagged,
            1uz, payload, 1uz, label_wrong_null, matcher) == v::core3_exception_error::label_type_mismatch);
        auto const label_wrong_family{[&](::std::size_t i) { REQUIRE(i == 0uz); return extern_ref; }};
        check(v::validate_exception_catch_signature(v::exception_catch_kind::tagged,
            1uz, payload, 1uz, label_wrong_family, matcher) == v::core3_exception_error::label_type_mismatch);
    }
    REQUIRE(checks == 42u);
    ::fast_io::print(::fast_io::out(), "EXCEPTION_ABSTRACT_CATCH_POLICY checks=", ::fast_io::mnp::dec(checks),
        " parser_context_representations=3\n");
}

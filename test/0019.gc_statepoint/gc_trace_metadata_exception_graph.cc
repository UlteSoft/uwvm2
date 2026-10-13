// Real opaque exn/aggregate graph and native census controls. This is a
// native-component fixture, not a product safepoint/root-enumeration proof.
// Run both metadata=0/1 with the same freshly compiled graph/root profile.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <uwvm2/runtime/gc/instance_phase.h>
#include <uwvm2/runtime/exception/native_roots.h>
#include <fast_io.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

#if !defined(UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH) || UWVM_EXPERIMENTAL_EXCEPTION_GC_GRAPH != 1 || \
    !defined(UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS) || UWVM_EXPERIMENTAL_NATIVE_EXCEPTION_ROOTS != 1
# error This fixture requires actual exception graph and registered native-root APIs.
#endif
#if defined(UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES) && UWVM_EXPERIMENTAL_EXTERNAL_EXCEPTION_HANDLES == 1
# error This primitive census fixture does not qualify the separate external-handle integration.
#endif

namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace eh = ::uwvm2::runtime::exception;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
using reference = gc::gc_reference;
using value = gc::gc_object_value;
using status = gc::gc_object_status;
using store_ptr = ::std::shared_ptr<gc::gc_object_store>;

namespace
{
    ::std::size_t checks{}, successful_collections{}, aggregate_reclaimed{}, exn_retired{}, rejected_collections{};
    void require(bool condition, unsigned line) noexcept
    {
        ++checks;
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL gc_trace_metadata_exception_graph line=", line);
            ::fast_io::fast_terminate();
        }
    }
#define GRAPH_CHECK(...) require(static_cast<bool>((__VA_ARGS__)), __LINE__)

    t::field_type numeric(t::value_kind kind) noexcept
    { t::field_type field{}; field.storage.value.kind = kind; field.mutable_ = true; return field; }
    t::field_type edge(t::abstract_heap_type heap) noexcept
    {
        auto field{numeric(t::value_kind::reference)};
        field.storage.value.heap = {static_cast<::std::int_least64_t>(heap)};
        field.storage.value.nullable = true;
        return field;
    }
    t::recursive_type_section declarations()
    {
        t::recursive_type_section section{};
        section.type_count = 3u;
        t::recursive_group group{};
        t::sub_type node{};
        node.kind = t::composite_kind::struct_;
        node.fields.push_back(numeric(t::value_kind::i32));
        node.fields.push_back(edge(t::abstract_heap_type::eq));
        auto packed{numeric(t::value_kind::i32)};
        packed.storage.packed = t::packed_kind::i16;
        node.fields.push_back(packed);
        node.fields.push_back(edge(t::abstract_heap_type::exn));
        group.types.push_back(::std::move(node)); // 0: actual ref indices 1/3.
        t::sub_type array{};
        array.kind = t::composite_kind::array;
        array.fields.push_back(edge(t::abstract_heap_type::exn));
        group.types.push_back(::std::move(array)); // 1: every element is an exn edge.
        t::sub_type numbers{};
        numbers.kind = t::composite_kind::array;
        numbers.fields.push_back(numeric(t::value_kind::i32));
        group.types.push_back(::std::move(numbers)); // 2: numeric leaf.
        section.groups.push_back(::std::move(group));
        return section;
    }
    store_ptr make_store(::std::shared_ptr<gc::gc_lease_owner> const& leases)
    {
        auto declaration{declarations()};
        auto store{::std::make_shared<gc::gc_object_store>(declaration, leases)};
        GRAPH_CHECK(store->valid());
        return store; // Parser fields cease to exist; canonical store owns its copy.
    }
    reference node(store_ptr const& store, ::std::uint32_t number)
    {
        reference result{};
        GRAPH_CHECK(store->struct_new_default(0u, result) == status::ok);
        GRAPH_CHECK(store->struct_set(result, 0uz, value::i32(number)) == status::ok);
        return result;
    }
    void scalar(store_ptr const& store, reference object, ::std::uint32_t number)
    {
        value output{};
        GRAPH_CHECK(store->struct_get(object, 0uz, false, output) == status::ok);
        GRAPH_CHECK(output.as<::std::uint32_t>() == number);
    }
    eh::payload_field carrier(reference input)
    {
        // [one complete native carrier] borrow its exact object representation
        // only until wasm_reference has copied every byte into its own field.
        auto result{eh::payload_field::wasm_reference(::std::as_bytes(::std::span{::std::addressof(input), 1uz}))};
        GRAPH_CHECK(result.has_value());
        return ::std::move(*result);
    }
    void check_identity(reference token, void const* identity, eh::diagnostic_trace_ref const& trace)
    {
        auto owner{gc::gc_object_store::lookup_exn_reference(token)};
        GRAPH_CHECK(owner && owner.get() == identity && owner->diagnostic().get() == trace.get());
        GRAPH_CHECK(owner->diagnostic()->frames().size() == 1uz);
        GRAPH_CHECK(owner->diagnostic()->frames()[0uz].function_index == 17uz);
        // The temporary immutable owner retires before the next locked census.
    }
    void collect(::std::array<store_ptr, 2uz> const& stores,
        eh::native_exception_root_domain& domain, ::std::span<reference const> roots,
        status expected, ::std::size_t expected_aggregates, ::std::size_t expected_exceptions)
    {
        gc::gc_object_store::retired_exception_batch retired{};
        ::std::size_t reclaimed{999uz};
        {
            // Both canonical stores and the native domain are genuinely owned.
            // This fixture has no other VM/native mutator or unregistered
            // immutable value owner; the registered wrapper stays in census.
            // The actual exclusive lease ends before any retired owner release.
            auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
            GRAPH_CHECK(static_cast<bool>(exclusive));
            status result{status::invalid_value};
            bool const census_completed{domain.with_registered_roots([&](auto const& census) noexcept
            {
                // [domain-minted locked census][complete bounded root span]
                // [safe ] no view or root address escapes this stopped callback.
                result = gc::gc_object_store::collect_exclusive_registered_exception_domain(
                    stores.data(), stores.size(), roots.data(), roots.size(), census, reclaimed, retired);
                return true; // Census completion is independent of collector status.
            })};
            GRAPH_CHECK(census_completed && result == expected);
            GRAPH_CHECK(reclaimed == expected_aggregates && retired.size() == expected_exceptions);
        } // Census, registry/cohort locks and actual exclusive lease all retired.
        if(expected == status::ok)
        { ++successful_collections; aggregate_reclaimed += reclaimed; exn_retired += retired.size(); }
        else { ++rejected_collections; }
        retired.reset_after_native_resume(); // No destructor runs under a collector/census lock.
        GRAPH_CHECK(retired.empty());
    }
    void live_cycle_and_registered_wrapper()
    {
        ::std::array leases{::std::make_shared<gc::gc_lease_owner>(), ::std::make_shared<gc::gc_lease_owner>()};
        ::std::array stores{make_store(leases[0uz]), make_store(leases[1uz])};
        auto domain{eh::native_exception_root_domain::make()};
        GRAPH_CHECK(eh::native_exception_root_domain::has_canonical_owner(domain));
        auto const a{node(stores[0uz], 111u)};
        auto const b{node(stores[0uz], 222u)};
        auto const foreign{node(stores[1uz], 333u)};
        GRAPH_CHECK(stores[0uz]->struct_set(a, 1uz, value::reference(b)) == status::ok);
        GRAPH_CHECK(stores[0uz]->struct_set(b, 1uz, value::reference(a)) == status::ok);
        GRAPH_CHECK(stores[0uz]->struct_set(a, 2uz, value::i32(0x12345u)) == status::ok);
        auto tag{::std::make_shared<unsigned>(37u)};
        auto trace{eh::diagnostic_trace::make(::std::vector<eh::diagnostic_frame>{{0uz, 17uz, u8"graph", u8"original_throw"}})};
        ::std::array old_fields{carrier(a)};
        auto old{eh::value::make(tag, old_fields, trace)};
        GRAPH_CHECK(static_cast<bool>(old));
        void const* const old_identity{old.get()}; // Comparison only; never dereferenced after owner release.
        reference old_token{};
        GRAPH_CHECK(stores[0uz]->make_exn_reference(old, old_token) == status::ok);
        old.reset(); // The actual registry owns the immutable value from here.
        GRAPH_CHECK(stores[0uz]->struct_set(a, 3uz, value::reference(old_token)) == status::ok);
        GRAPH_CHECK(stores[1uz]->struct_set(foreign, 3uz, value::reference(old_token)) == status::ok);
        // The foreign recipient creates a REAL module lease/root-index edge;
        // both module lease owners and canonical stores stay pinned until exit.
        reference refs{}, zero{}, garbage{};
        GRAPH_CHECK(stores[0uz]->array_new(1u, value::reference(old_token), 9uz, refs) == status::ok);
        GRAPH_CHECK(stores[0uz]->array_new_default(1u, 0uz, zero) == status::ok);
        GRAPH_CHECK(stores[0uz]->array_new(2u, value::i32(999u), 13uz, garbage) == status::ok);
        // New caught wrapper independently owns old-exnref and a foreign node.
        // Consume the ONLY external immutable value into the genuine census.
        ::std::array new_fields{carrier(old_token), carrier(foreign)};
        auto wrapper{eh::value::make(tag, new_fields, trace)};
        GRAPH_CHECK(static_cast<bool>(wrapper));
        eh::immutable_exception_root_lease registered{*domain, ::std::move(wrapper)};
        GRAPH_CHECK(!wrapper && registered.status() == eh::native_exception_root_status::registered);
        GRAPH_CHECK(domain->registered_count() == 1uz && registered.instance()->diagnostic().get() == trace.get());
        ::std::array roots{refs, refs, zero};
        collect(stores, *domain, roots, status::ok, 1uz, 0uz);
        scalar(stores[0uz], a, 111u); scalar(stores[0uz], b, 222u); scalar(stores[1uz], foreign, 333u);
        check_identity(old_token, old_identity, trace);
        value observed{};
        GRAPH_CHECK(stores[0uz]->struct_get(a, 2uz, false, observed) == status::ok && observed.as<::std::uint32_t>() == 0x2345u);
        GRAPH_CHECK(stores[0uz]->array_get(refs, 8uz, false, observed) == status::ok && observed.as<reference>().storage.ptr == old_token.storage.ptr);
        GRAPH_CHECK(stores[0uz]->array_get(garbage, 0uz, false, observed) == status::invalid_reference);
        // Array roots retire, but the real native wrapper still reaches the
        // complete old token -> A -> B cycle and the independent foreign node.
        roots = {};
        collect(stores, *domain, {}, status::ok, 2uz, 0uz); // refs + zero.
        scalar(stores[0uz], a, 111u); scalar(stores[0uz], b, 222u); scalar(stores[1uz], foreign, 333u);
        check_identity(old_token, old_identity, trace);
        registered.reset(); // No locked census exists; consume every native owner.
        GRAPH_CHECK(domain->registered_count() == 0uz);
        collect(stores, *domain, {}, status::ok, 3uz, 1uz); // A+B+foreign and the unreachable exn cycle.
        GRAPH_CHECK(!gc::gc_object_store::lookup_exn_reference(old_token));
        GRAPH_CHECK(stores[0uz]->struct_get(a, 0uz, false, observed) == status::invalid_reference);
        GRAPH_CHECK(stores[1uz]->struct_get(foreign, 0uz, false, observed) == status::invalid_reference);
        auto const untouched{node(stores[0uz], 444u)};
        ::std::array stale_roots{untouched, old_token};
        collect(stores, *domain, stale_roots, status::invalid_reference, 0uz, 0uz);
        scalar(stores[0uz], untouched, 444u); // Invalid later root cannot reclaim anything.
        collect(stores, *domain, {}, status::ok, 1uz, 0uz);
        GRAPH_CHECK(stores[0uz]->struct_get(untouched, 0uz, false, observed) == status::invalid_reference);
    }
    void invalid_unreachable_payload()
    {
        ::std::array leases{::std::make_shared<gc::gc_lease_owner>(), ::std::make_shared<gc::gc_lease_owner>()};
        ::std::array stores{make_store(leases[0uz]), make_store(leases[1uz])};
        auto domain{eh::native_exception_root_domain::make()};
        reference dead{};
        GRAPH_CHECK(stores[0uz]->array_new(2u, value::i32(123u), 7uz, dead) == status::ok);
        collect(stores, *domain, {}, status::ok, 1uz, 0uz);
        // Privileged native negative: carrier of a REAL retired token, not an
        // invented payload pointer. The immutable publisher copies it, while
        // the graph's global preflight must reject it even when unreachable.
        auto tag{::std::make_shared<unsigned>(99u)};
        ::std::array invalid_fields{carrier(dead)};
        auto invalid{eh::value::make(tag, invalid_fields)};
        GRAPH_CHECK(static_cast<bool>(invalid));
        reference invalid_token{};
        GRAPH_CHECK(stores[0uz]->make_exn_reference(invalid, invalid_token) == status::ok);
        invalid.reset(); // ONLY the real registry retains this malformed value.
        auto const alive{node(stores[1uz], 555u)};
        collect(stores, *domain, {}, status::invalid_reference, 0uz, 0uz);
        scalar(stores[1uz], alive, 555u); // No root/mark/sweep commit after failed payload preflight.
        GRAPH_CHECK(static_cast<bool>(gc::gc_object_store::lookup_exn_reference(invalid_token)));
        // This immutable value cannot be repaired. Normal store destruction
        // drains its private registry after this native-only scope ends.
    }
}

int main()
{
    live_cycle_and_registered_wrapper();
    invalid_unreachable_payload();
    GRAPH_CHECK(successful_collections == 5uz && aggregate_reclaimed == 8uz && exn_retired == 1uz && rejected_collections == 2uz);
    ::fast_io::io::println("{\"fixture\":\"gc_trace_metadata_exception_graph\",\"checks\":", checks,
        ",\"successful_collections\":", successful_collections, ",\"rejected_collections\":", rejected_collections,
        ",\"aggregate_reclaimed\":", aggregate_reclaimed, ",\"exn_retired\":", exn_retired,
        ",\"native_component_only\":true,\"product_collection_qualified\":false,\"performance_measured\":false}");
}

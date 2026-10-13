// Isolated collector regression against the actual product hash membership.
// The runner supplies one explicitly hashed gc_object.h candidate. This test
// does not activate collection in a VM or establish activation-root coverage.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <fast_io.h>
#include <array>
#include <limits>
#include <memory>
#if defined(UWVM_GC_COHORT_DTOR_PROBE)
# include <atomic>
# include <latch>
# include <thread>

namespace uwvm_gc_cohort_test
{
    inline ::std::atomic<::uwvm2::uwvm::runtime::storage::gc_object_store const*> target{};
    inline ::std::latch after_unregistration{1}, allow_global_unlink{1};
}
// This hook is injected into a separately hashed private native-test overlay.
// No production entry, destructor or debugger API contains this test hook.
namespace uwvm2::uwvm::runtime::storage
{
    inline void gc_cohort_test_after_unregistration(gc_object_store const* store) noexcept
    {
        if(uwvm_gc_cohort_test::target.load(::std::memory_order_acquire) != store) { return; }
        uwvm_gc_cohort_test::after_unregistration.count_down();
        uwvm_gc_cohort_test::allow_global_unlink.wait();
    }
}
#endif

#if defined(UWVM_GC_PRODUCT_COHORT_PROBE)
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace
{
    unsigned checks{}, collections{};
    ::std::size_t total_reclaimed{};
    void require(bool result, unsigned line) noexcept
    {
        ++checks;
        if(!result)
        {
            ::fast_io::io::perrln("FAIL product cohort sweep line ", line);
            ::fast_io::fast_terminate();
        }
    }
# define CHECK(condition) require((condition), __LINE__)
    [[nodiscard]] t::recursive_type_section declarations()
    {
        t::recursive_type_section section{};
        section.type_count = 2u;
        t::recursive_group group{};
        t::sub_type structure{};
        structure.kind = t::composite_kind::struct_;
        t::field_type field{};
        field.storage.value.kind = t::value_kind::reference;
        field.storage.value.heap = {static_cast<::std::int_least64_t>(t::abstract_heap_type::eq)};
        field.storage.value.nullable = true;
        field.mutable_ = true;
        structure.fields.push_back(field);
        group.types.push_back(::std::move(structure));
        t::sub_type array{};
        array.kind = t::composite_kind::array;
        array.fields.push_back(field);
        group.types.push_back(::std::move(array));
        section.groups.push_back(::std::move(group));
        return section;
    }
    template<::std::size_t Size>
    void collect(::std::array<::std::shared_ptr<gc::gc_object_store>, Size> const& cohort,
                 gc::gc_reference const* roots, ::std::size_t count,
                 gc::gc_object_status expected, ::std::size_t expected_reclaimed)
    {
        ::std::size_t reclaimed{(~::std::size_t{})};
        CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(
                  cohort.data(), cohort.size(), roots, count, reclaimed) == expected);
        CHECK(reclaimed == expected_reclaimed);
        if(expected == gc::gc_object_status::ok)
        {
            ++collections;
            total_reclaimed += reclaimed;
        }
    }
    void points_to(::std::shared_ptr<gc::gc_object_store> const& reader,
                   gc::gc_reference object, gc::gc_reference expected)
    {
        gc::gc_object_value observed{};
        CHECK(reader->struct_get(object, 0uz, false, observed) == gc::gc_object_status::ok);
        auto const reference{observed.as<gc::gc_reference>()};
        CHECK(reference.kind == expected.kind);
        if(reference.kind == ::uwvm2::object::global::wasm_ref_kind::wasm_null) { return; }
        CHECK(reference.storage.ptr == expected.storage.ptr);
    }
}

int main()
{
    using kind = ::uwvm2::object::global::wasm_ref_kind;
    auto section{declarations()};
    t::recursive_type_section empty{};
    auto leases_a{::std::make_shared<gc::gc_lease_owner>()};
    auto leases_b{::std::make_shared<gc::gc_lease_owner>()};
    auto leases_c{::std::make_shared<gc::gc_lease_owner>()};
    ::std::array stores{::std::make_shared<gc::gc_object_store>(section, leases_a),
                       ::std::make_shared<gc::gc_object_store>(section, leases_b),
                       ::std::make_shared<gc::gc_object_store>(empty, leases_c)};
    for(auto const& store: stores) { CHECK(store->valid()); }
    ::std::weak_ptr<gc::gc_object_store> weak_b{stores[1]};
    gc::gc_reference live_a{}, live_b{}, dead_a{}, dead_b{};
    CHECK(stores[0]->struct_new_default(0u, live_a) == gc::gc_object_status::ok);
    CHECK(stores[1]->struct_new_default(0u, live_b) == gc::gc_object_status::ok);
    CHECK(stores[0]->struct_new_default(0u, dead_a) == gc::gc_object_status::ok);
    CHECK(stores[1]->struct_new_default(0u, dead_b) == gc::gc_object_status::ok);
    CHECK(stores[0]->struct_set(live_a, 0u, gc::gc_object_value::reference(live_b)) == gc::gc_object_status::ok);
    CHECK(stores[1]->struct_set(live_b, 0u, gc::gc_object_value::reference(live_a)) == gc::gc_object_status::ok);
    CHECK(stores[0]->struct_set(dead_a, 0u, gc::gc_object_value::reference(dead_b)) == gc::gc_object_status::ok);
    CHECK(stores[1]->struct_set(dead_b, 0u, gc::gc_object_value::reference(dead_a)) == gc::gc_object_status::ok);

    // Store C has issued no token. It can nevertheless represent the provider
    // of an incoming global/table/host root. Omitting it must fail before any
    // mark/sweep rather than relying on the set of published object tokens.
    {
        ::std::array partial{stores[0], stores[1]};
        collect(partial, nullptr, 0uz, gc::gc_object_status::invalid_reference, 0uz);
        points_to(stores[0], live_a, live_b);
    }
    {
        ::std::array duplicate{stores[0], stores[0], stores[2]};
        collect(duplicate, nullptr, 0uz, gc::gc_object_status::invalid_store, 0uz);
        ::std::array<::std::shared_ptr<gc::gc_object_store>, 3> missing{stores[0], {}, stores[2]};
        collect(missing, nullptr, 0uz, gc::gc_object_status::invalid_store, 0uz);
        // A shared_ptr that points at the store but owns an unrelated object
        // cannot serve as a canonical lifetime pin during the sweep.
        auto unrelated{::std::make_shared<unsigned char>()};
        ::std::shared_ptr<gc::gc_object_store> fake_pin{unrelated, stores[0].get()};
        ::std::array aliases{fake_pin, stores[1], stores[2]};
        collect(aliases, nullptr, 0uz, gc::gc_object_status::invalid_store, 0uz);
    }
    {
        gc::gc_reference forged{};
        forged.kind = kind::wasm_struct;
        forged.storage.ptr = reinterpret_cast<void*>(::std::uintptr_t{0x123u});
        collect(stores, &forged, 1uz, gc::gc_object_status::invalid_reference, 0uz);
        forged.kind = kind::wasm_array;
        collect(stores, &forged, 1uz, gc::gc_object_status::invalid_reference, 0uz);
        forged.kind = static_cast<kind>(255u);
        collect(stores, &forged, 1uz, gc::gc_object_status::invalid_reference, 0uz);
        forged.kind = kind::wasm_func;
        collect(stores, &forged, 1uz, gc::gc_object_status::invalid_reference, 0uz);
        collect(stores, nullptr, 1uz, gc::gc_object_status::invalid_value, 0uz);
        auto const excessive{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(forged) + 1uz};
        collect(stores, &forged, excessive, gc::gc_object_status::invalid_value, 0uz);
    }
    collect(stores, &live_a, 1uz, gc::gc_object_status::ok, 2uz);
    points_to(stores[0], live_a, live_b);
    points_to(stores[1], live_b, live_a);
    gc::gc_object_value observed{};
    // A removed token must miss the real foreign index as well as the local
    // one; passing from another live module must never read a freed header.
    CHECK(stores[0]->struct_get(dead_a, 0u, false, observed) == gc::gc_object_status::invalid_reference);
    CHECK(stores[1]->struct_get(dead_a, 0u, false, observed) == gc::gc_object_status::invalid_reference);
    CHECK(stores[0]->struct_get(dead_b, 0u, false, observed) == gc::gc_object_status::invalid_reference);
    collect(stores, &dead_b, 1uz, gc::gc_object_status::invalid_reference, 0uz);
    collect(stores, &live_a, 1uz, gc::gc_object_status::ok, 0uz);
    CHECK(stores[0]->struct_set(live_a, 0u, gc::gc_object_value::reference({})) == gc::gc_object_status::ok);
    collect(stores, &live_a, 1uz, gc::gc_object_status::ok, 1uz);
    points_to(stores[0], live_a, {});
    CHECK(stores[0]->struct_get(live_b, 0u, false, observed) == gc::gc_object_status::invalid_reference);

    // Retaining A after overwriting A->B must not keep B's otherwise empty
    // store alive through obsolete object-level foreign leases. Module lease
    // owners are retired explicitly; this test does not silently prune roots.
    leases_a.reset();
    leases_b.reset();
    stores[1].reset();
    CHECK(weak_b.expired());
    ::std::array remaining{stores[0], stores[2]};
    std::array<gc::gc_reference, 5> non_heap{};
    non_heap[0] = live_a;
    non_heap[1] = ::uwvm2::object::global::make_wasm_i31_reference(-19);
    non_heap[2].kind = kind::wasm_func_imported;
    non_heap[3].kind = kind::wasm_func_defined;
    // Complete function carriers are unmanaged here; their module lifetime
    // authority belongs to the caller. No guest pointer is dereferenced.
    collect(remaining, non_heap.data(), non_heap.size(), gc::gc_object_status::ok, 0uz);
    gc::gc_reference ring{};
    CHECK(stores[0]->array_new_default(1u, 7uz, ring) == gc::gc_object_status::ok);
    CHECK(stores[0]->array_set(ring, 6uz, gc::gc_object_value::reference(live_a)) == gc::gc_object_status::ok);
    collect(remaining, &ring, 1uz, gc::gc_object_status::ok, 0uz);
    CHECK(stores[0]->array_get(ring, 6uz, false, observed) == gc::gc_object_status::ok);
    CHECK(observed.as<gc::gc_reference>().storage.ptr == live_a.storage.ptr);
    collect(remaining, nullptr, 0uz, gc::gc_object_status::ok, 2uz);
    CHECK(stores[2]->struct_get(live_a, 0u, false, observed) == gc::gc_object_status::invalid_reference);
    gc::gc_reference later{};
    CHECK(stores[0]->struct_new_default(0u, later) == gc::gc_object_status::ok);
    CHECK(later.storage.ptr != live_a.storage.ptr && later.storage.ptr != dead_a.storage.ptr);
    collect(remaining, &live_a, 1uz, gc::gc_object_status::invalid_reference, 0uz);
    collect(remaining, nullptr, 0uz, gc::gc_object_status::ok, 1uz);
    leases_c.reset();
    remaining = {};
    stores = {};

    // Each hidden ownership class is isolated from empty/missing store checks.
    // This aggregate collector deliberately refuses to sweep either registry.
    {
        auto lease{::std::make_shared<gc::gc_lease_owner>()};
        ::std::array singleton{::std::make_shared<gc::gc_object_store>(section, lease)};
        gc::gc_reference inner{}, wrapped{};
        CHECK(singleton[0]->struct_new_default(0u, inner) == gc::gc_object_status::ok);
        CHECK(singleton[0]->extern_convert_any(inner, wrapped) == gc::gc_object_status::ok);
        collect(singleton, nullptr, 0uz, gc::gc_object_status::invalid_reference, 0uz);
        CHECK(singleton[0]->struct_get(inner, 0u, false, observed) == gc::gc_object_status::ok);
    }
    {
        auto lease{::std::make_shared<gc::gc_lease_owner>()};
        ::std::array singleton{::std::make_shared<gc::gc_object_store>(section, lease)};
        gc::gc_reference inner{}, exn{};
        CHECK(singleton[0]->struct_new_default(0u, inner) == gc::gc_object_status::ok);
        auto tag{::std::static_pointer_cast<void const>(::std::make_shared<unsigned char>())};
        auto field{::uwvm2::runtime::exception::payload_field::wasm_reference(
            {reinterpret_cast<::std::byte const*>(&inner), sizeof(inner)}, {})};
        CHECK(field.has_value());
        auto value{::uwvm2::runtime::exception::value::make(tag, {&*field, 1uz})};
        CHECK(value != nullptr);
        CHECK(singleton[0]->make_exn_reference(value, exn) == gc::gc_object_status::ok);
        collect(singleton, nullptr, 0uz, gc::gc_object_status::invalid_reference, 0uz);
        CHECK(singleton[0]->struct_get(inner, 0u, false, observed) == gc::gc_object_status::ok);
    }
    ::std::size_t reclaimed{};
    CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(nullptr, 0uz, nullptr, 0uz, reclaimed) == gc::gc_object_status::ok);
    CHECK(reclaimed == 0uz);
    CHECK(collections == 7u && total_reclaimed == 6uz);
# if defined(UWVM_GC_COHORT_DTOR_PROBE)
    {
        auto lease{::std::make_shared<gc::gc_lease_owner>()};
        ::std::array cohort{::std::make_shared<gc::gc_object_store>(section, lease)};
        auto outside{::std::make_shared<gc::gc_object_store>(section)};
        gc::gc_reference garbage{}, outside_root{};
        CHECK(cohort[0]->struct_new_default(0u, garbage) == gc::gc_object_status::ok);
        CHECK(outside->struct_new_default(0u, outside_root) == gc::gc_object_status::ok);
        uwvm_gc_cohort_test::target.store(outside.get(), ::std::memory_order_release);
        // Move the sole canonical outside owner into a real teardown thread.
        // The hook blocks after admission unlink and before foreign-index unlink.
        ::std::thread teardown{[pin = ::std::move(outside)]() mutable noexcept { pin.reset(); }};
        uwvm_gc_cohort_test::after_unregistration.wait();
        ::std::size_t retired{(~::std::size_t{})};
        auto const status{gc::gc_object_store::collect_exclusive_aggregate_domain(
            cohort.data(), cohort.size(), &outside_root, 1uz, retired)};
        // Always release/join before assertion, including the negative control.
        uwvm_gc_cohort_test::allow_global_unlink.count_down();
        teardown.join();
        uwvm_gc_cohort_test::target.store(nullptr, ::std::memory_order_release);
        CHECK(status == gc::gc_object_status::invalid_reference && retired == 0uz);
        CHECK(cohort[0]->struct_get(garbage, 0u, false, observed) == gc::gc_object_status::ok);
        collect(cohort, nullptr, 0uz, gc::gc_object_status::ok, 1uz);
    }
    CHECK(collections == 8u && total_reclaimed == 7uz);
    ::fast_io::io::println("PASS defensive teardown rejection: real destructor admission/index gap; no collection permitted through unfinished teardown");
# endif
    ::fast_io::io::println("PASS product cohort sweep: ", checks, " checks; ", collections,
        " actual collections; ", total_reclaimed,
        " reclaimed; empty-store closure, canonical pins, foreign-index retirement, field overwrite lease pruning, cycles and stale identities; native component only");
}
# undef CHECK
#else
int main()
{
    ::fast_io::io::println("SKIP product cohort sweep: explicit reviewed collector overlay required; product GC activation absent");
    return 77;
}
#endif

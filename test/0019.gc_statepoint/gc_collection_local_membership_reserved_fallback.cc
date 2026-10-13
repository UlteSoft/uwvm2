// Collection-local opaque-token membership: actual canonical stores and a
// real admission closure. Native component only, not VM roots/performance.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <uwvm2/runtime/gc/instance_phase.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <limits>
#include <memory>
#include <thread>
#include <utility>
#if !defined(UWVM2TEST_GC_NUMERIC_SLAB_RESERVED_PROBE) || UWVM2TEST_GC_NUMERIC_SLAB_RESERVED_PROBE != 1
# error This fixture needs the actual reserved-slab rendezvous.
#endif
#if defined(UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP) && UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP == 1 && \
    (!defined(UWVM2TEST_GC_COLLECTION_LOCAL_MEMBERSHIP_PROBE) || UWVM2TEST_GC_COLLECTION_LOCAL_MEMBERSHIP_PROBE != 1)
# error Candidate ON must prove the actual private local lookup was used.
#endif
namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
using store_ptr = ::std::shared_ptr<gc::gc_object_store>;
using reference = gc::gc_reference;
using value = gc::gc_object_value;
using status = gc::gc_object_status;
namespace
{
    ::std::size_t checks{}, collections{}, rejected{}, reclaimed_total{};
    ::std::atomic_size_t local_hits{}, local_owners{};
    ::std::atomic_bool bad_probe{}, arm_reservation{}, reserved{}, release_reservation{}, worker_done{};
    ::std::atomic_bool arm_same_actor_reservation{};
    ::std::size_t same_actor_rejections{};
    ::std::array<store_ptr, 2uz> const* same_actor_stores{};
    reference const* same_actor_roots{};
    ::std::size_t same_actor_root_count{};
    ::uwvm2::runtime::gc::managed_entry_admission::shared_lease const* same_actor_shared{};
    ::std::thread::id same_actor_thread{};
    void const* expected_owners[2uz]{};
    void require(bool condition, unsigned line) noexcept
    {
        ++checks;
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL gc_collection_local_membership_reserved_fallback line=", line);
            ::fast_io::fast_terminate();
        }
    }
#define LOCAL_CHECK(...) require(static_cast<bool>((__VA_ARGS__)), __LINE__)
    t::field_type numeric() noexcept
    {
        t::field_type result{};
        result.storage.value.kind = t::value_kind::i32; result.mutable_ = true;
        return result;
    }
    t::recursive_type_section declarations()
    {
        t::recursive_type_section result{}; result.type_count = 4u;
        t::recursive_group group{};
        t::sub_type scalar{}; scalar.kind = t::composite_kind::struct_;
        scalar.fields.push_back(numeric()); group.types.push_back(::std::move(scalar)); // 0: real numeric slab
        t::field_type edge{}; edge.mutable_ = true;
        edge.storage.value.kind = t::value_kind::reference;
        edge.storage.value.heap = {static_cast<::std::int_least64_t>(t::abstract_heap_type::eq)};
        edge.storage.value.nullable = true;
        t::sub_type node{}; node.kind = t::composite_kind::struct_;
        node.fields.push_back(numeric()); node.fields.push_back(edge);
        group.types.push_back(::std::move(node)); // 1: reference-bearing, never compact
        t::sub_type refs{}; refs.kind = t::composite_kind::array;
        refs.fields.push_back(edge); group.types.push_back(::std::move(refs)); // 2
        t::sub_type numbers{}; numbers.kind = t::composite_kind::array;
        numbers.fields.push_back(numeric()); group.types.push_back(::std::move(numbers)); // 3
        result.groups.push_back(::std::move(group)); return result;
    }
    store_ptr make_store(::std::shared_ptr<gc::gc_lease_owner> const& leases)
    {
        auto schema{declarations()};
        auto store{::std::make_shared<gc::gc_object_store>(schema, leases)};
        LOCAL_CHECK(store->valid()); return store;
    }
    bool same(reference a, reference b) noexcept
    { return a.kind == b.kind && a.storage.ptr == b.storage.ptr; }
    void number(store_ptr const& store, reference object, ::std::uint32_t wanted)
    {
        value observed{};
        LOCAL_CHECK(store->struct_get(object, 0uz, false, observed) == status::ok);
        LOCAL_CHECK(observed.as<::std::uint32_t>() == wanted);
    }
    template<::std::size_t N>
    void collect(::std::array<store_ptr, N> const& stores, reference const* roots,
        ::std::size_t root_count, status wanted, ::std::size_t expected)
    {
        // Actual pins, all native readers stopped and the COMPLETE root list.
        // try_exclusive(0) is a real lease, not active_count/TLS/forged authority.
        auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
        LOCAL_CHECK(static_cast<bool>(exclusive));
        ::std::array<::std::uint64_t, N> epochs{};
        for(::std::size_t i{}; i != N; ++i)
        { epochs[i] = stores[i]->native_numeric_slab_statistics().epoch; }
        ::std::size_t reclaimed{999uz};
        auto const observed{gc::gc_object_store::collect_exclusive_aggregate_domain(
            stores.data(), stores.size(), roots, root_count, reclaimed)};
        LOCAL_CHECK(observed == wanted && reclaimed == expected);
        if(wanted == status::ok) { ++collections; reclaimed_total += reclaimed; }
        else
        {
            ++rejected;
            for(::std::size_t i{}; i != N; ++i)
            { LOCAL_CHECK(stores[i]->native_numeric_slab_statistics().epoch == epochs[i]); }
        }
    }
}
#if defined(UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP) && UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP == 1
extern "C" void collection_member_probe(void const*, ::std::uintptr_t) noexcept
    asm("uwvm2_test_gc_collection_local_membership_probe");
extern "C" void collection_member_probe(void const* owner, ::std::uintptr_t token) noexcept
{
    // Native witness only: compare actual identities, never dereference owner/token.
    if(owner == expected_owners[0uz]) { local_owners.fetch_or(1uz); }
    else if(owner == expected_owners[1uz]) { local_owners.fetch_or(2uz); }
    else { bad_probe.store(true); }
    if(token == 0u) { bad_probe.store(true); }
    local_hits.fetch_add(1uz);
}
#endif
extern "C" void collection_reserved_probe(gc::gc_object_store const*) noexcept
    asm("uwvm2_test_gc_numeric_slab_reserved_probe");
extern "C" void collection_reserved_probe(gc::gc_object_store const* reserving_store) noexcept
{
    if(arm_same_actor_reservation.exchange(false))
    {
        // Test selector is NOT permission. The callback is synchronous on the
        // same actor's real allocation stack. Its caller's owning stores, root
        // list and actual shared lease remain alive until this callback returns.
        LOCAL_CHECK(::std::this_thread::get_id() == same_actor_thread);
        LOCAL_CHECK(same_actor_stores != nullptr && same_actor_shared != nullptr &&
            same_actor_roots != nullptr && same_actor_root_count != 0uz);
        // [actual pinned main-stack cohort][unchanged complete published roots]
        // [safe ] these native borrows are never saved past this callback.
        auto const& pins{*same_actor_stores};
        LOCAL_CHECK(reserving_store == pins[0uz].get());
        auto& admission{::uwvm2::runtime::gc::runtime_gc_entry_admission};
        auto stopped{admission.try_exclusive(1uz)};
        // Actual lease issued by THIS word, not active_count==1/TLS/guessed bool.
        LOCAL_CHECK(static_cast<bool>(stopped) && admission.protects_shared(stopped, *same_actor_shared));
        ::std::array<::std::uint64_t, 2uz> epochs{};
        for(::std::size_t index{}; index != pins.size(); ++index)
        { epochs[index] = pins[index]->native_numeric_slab_statistics().epoch; }
        LOCAL_CHECK(pins[0uz]->native_numeric_slab_statistics().reserved_slots == 1uz);
        auto const before_hits{local_hits.load()};
        ::std::size_t reclaimed{999uz};
        // The real numeric slot is reserved, but NO object has been placement-
        // constructed, locally published, globally published or exposed yet.
        // All published graph actors are stopped and all semantic roots known.
        // The candidate MUST decline local membership, retain the original
        // validation/late invalid_store path and never reclaim or change epoch.
        auto const result{gc::gc_object_store::collect_exclusive_aggregate_domain(
            pins.data(), pins.size(), same_actor_roots, same_actor_root_count, reclaimed)};
        LOCAL_CHECK(result == status::invalid_store && reclaimed == 0uz);
        LOCAL_CHECK(local_hits.load() == before_hits);
        for(::std::size_t index{}; index != pins.size(); ++index)
        { LOCAL_CHECK(pins[index]->native_numeric_slab_statistics().epoch == epochs[index]); }
        LOCAL_CHECK(pins[0uz]->native_numeric_slab_statistics().reserved_slots == 1uz);
        ++rejected; ++same_actor_rejections;
        return; // Real exclusive destructor restores THIS still-owned shared lease.
    }
    if(!arm_reservation.exchange(false)) { return; }
    reserved.store(true); reserved.notify_all();
    auto const deadline{::std::chrono::steady_clock::now() + ::std::chrono::seconds{5}};
    while(!release_reservation.load())
    {
        if(::std::chrono::steady_clock::now() >= deadline) { bad_probe.store(true); return; }
        ::std::this_thread::yield();
    }
}
int main()
{
    ::std::array leases{::std::make_shared<gc::gc_lease_owner>(), ::std::make_shared<gc::gc_lease_owner>()};
    ::std::array stores{make_store(leases[0uz]), make_store(leases[1uz])};
    expected_owners[0uz] = stores[0uz].get(); expected_owners[1uz] = stores[1uz].get();
    reference a{}, b{}, refs{}, zero{}, numbers{}, dead{};
    LOCAL_CHECK(stores[0uz]->struct_new_default(1u, a) == status::ok);
    LOCAL_CHECK(stores[1uz]->struct_new_default(1u, b) == status::ok);
    LOCAL_CHECK(stores[0uz]->struct_set(a, 0uz, value::i32(111u)) == status::ok);
    LOCAL_CHECK(stores[1uz]->struct_set(b, 0uz, value::i32(222u)) == status::ok);
    LOCAL_CHECK(stores[0uz]->struct_set(a, 1uz, value::reference(b)) == status::ok);
    LOCAL_CHECK(stores[1uz]->struct_set(b, 1uz, value::reference(a)) == status::ok);
    LOCAL_CHECK(stores[0uz]->array_new(2u, value::reference(a), 8uz, refs) == status::ok);
    LOCAL_CHECK(stores[0uz]->array_set(refs, 7uz, value::reference(b)) == status::ok);
    LOCAL_CHECK(stores[0uz]->array_new_default(2u, 0uz, zero) == status::ok);
    LOCAL_CHECK(stores[0uz]->array_new(3u, value::i32(333u), 7uz, numbers) == status::ok);
    LOCAL_CHECK(stores[0uz]->array_new(3u, value::i32(444u), 7uz, dead) == status::ok);
    ::std::array roots{refs, numbers, a, b, a, zero}; // Real duplicates and foreign direct root.
    collect(stores, roots.data(), roots.size(), status::ok, 1uz);
    number(stores[0uz], a, 111u); number(stores[0uz], b, 222u); // Recipient lease remains real.
    value observed{};
    LOCAL_CHECK(stores[0uz]->array_get(refs, 7uz, false, observed) == status::ok);
    LOCAL_CHECK(same(observed.as<reference>(), b));
    LOCAL_CHECK(stores[0uz]->array_get(dead, 0uz, false, observed) == status::invalid_reference);
    reference garbage{};
    LOCAL_CHECK(stores[0uz]->array_new(3u, value::i32(555u), 7uz, garbage) == status::ok);
    auto forged{a};
    // Maximum uintptr_t is NEVER issued: token blocks retain a representable
    // exclusive end and the last issued key is strictly below that endpoint.
    forged.storage.ptr = reinterpret_cast<void*>((::std::numeric_limits<::std::uintptr_t>::max)());
    ::std::array invalid{refs, forged};
    collect(stores, invalid.data(), invalid.size(), status::invalid_reference, 0uz);
    invalid[1uz] = dead; // Genuine formerly issued token after actual reclamation.
    collect(stores, invalid.data(), invalid.size(), status::invalid_reference, 0uz);
    auto wrong_kind{refs}; wrong_kind.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_struct;
    invalid[1uz] = wrong_kind;
    collect(stores, invalid.data(), invalid.size(), status::invalid_reference, 0uz);
    auto unissued_exn{forged}; unissued_exn.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_exn;
    invalid[1uz] = unissued_exn;
    collect(stores, invalid.data(), invalid.size(), status::invalid_reference, 0uz);
    ::std::array omitted{stores[0uz]};
    collect(omitted, roots.data(), roots.size(), status::invalid_reference, 0uz);
    ::std::array aliased{stores[0uz], stores[0uz]};
    collect(aliased, roots.data(), roots.size(), status::invalid_store, 0uz);
    {
        auto outsider_leases{::std::make_shared<gc::gc_lease_owner>()};
        auto outsider{make_store(outsider_leases)};
        reference outside{};
        LOCAL_CHECK(outsider->struct_new_default(1u, outside) == status::ok);
        ::std::array outside_roots{refs, outside};
        // A REAL registered third owner omitted from this purported cohort.
        // No member lookup can authorize collection or silently retain it.
        collect(stores, outside_roots.data(), outside_roots.size(), status::invalid_reference, 0uz);
    }
    LOCAL_CHECK(stores[0uz]->array_get(garbage, 6uz, false, observed) == status::ok);
    LOCAL_CHECK(observed.as<::std::uint32_t>() == 555u); // All failed collections reclaimed zero.
    reference same_actor_object{};
    {
        // This sole current actor owns a real shared lease over the ENTIRE
        // allocation. There are no peers; the callback proves identity again.
        auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        same_actor_thread = ::std::this_thread::get_id();
        // [live main-stack pins/roots/lease][synchronous callback only]
        // [safe ] publish trusted native borrows before invoking the allocator.
        same_actor_stores = ::std::addressof(stores);
        same_actor_roots = roots.data(); same_actor_root_count = roots.size();
        same_actor_shared = ::std::addressof(shared);
        arm_same_actor_reservation.store(true);
        LOCAL_CHECK(stores[0uz]->struct_new_default(0u, same_actor_object) == status::ok);
        LOCAL_CHECK(!arm_same_actor_reservation.load() && same_actor_rejections == 1uz);
        // [returned synchronous callback][same actual main-stack owners alive]
        // [safe ] retire every temporary borrow BEFORE the shared owner dies.
        same_actor_stores = nullptr; same_actor_roots = nullptr;
        same_actor_root_count = 0uz; same_actor_shared = nullptr;
    }
    LOCAL_CHECK(stores[0uz]->native_numeric_slab_statistics().reserved_slots == 0uz);
    number(stores[0uz], same_actor_object, 0u); // Failed GC did not spoil publication.
    LOCAL_CHECK(stores[0uz]->array_get(garbage, 6uz, false, observed) == status::ok);
    LOCAL_CHECK(observed.as<::std::uint32_t>() == 555u); // Original late failure retained garbage.
    reference worker_object{};
    status worker_status{status::invalid_value};
    arm_reservation.store(true);
    ::std::thread worker{[&]
    {
        // A genuine counted native allocator owns the ENTIRE allocate/publish.
        auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        worker_status = stores[0uz]->struct_new_default(0u, worker_object);
        worker_done.store(true);
    }};
    auto const reservation_deadline{::std::chrono::steady_clock::now() + ::std::chrono::seconds{5}};
    while(!reserved.load())
    {
        if(worker_done.load() || ::std::chrono::steady_clock::now() >= reservation_deadline)
        { LOCAL_CHECK(false); }
        ::std::this_thread::yield();
    }
    LOCAL_CHECK(stores[0uz]->native_numeric_slab_statistics().reserved_slots == 1uz);
    {
        // Collector cannot enter while the tracked allocator is in flight.
        // Do NOT invoke the public collector without its actual admission.
        auto blocked{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
        LOCAL_CHECK(!blocked);
    }
    release_reservation.store(true); release_reservation.notify_all(); worker.join();
    LOCAL_CHECK(worker_status == status::ok);
    LOCAL_CHECK(stores[0uz]->native_numeric_slab_statistics().reserved_slots == 0uz);
    number(stores[0uz], worker_object, 0u);
    collect(stores, roots.data(), roots.size(), status::ok, 3uz); // Garbage + both retired allocator roots.
    roots = {}; // Every semantic native root retires before the actual sweep.
    collect(stores, nullptr, 0uz, status::ok, 5uz); // A,B,refs,zero,numbers, including the cycle.
    LOCAL_CHECK(stores[0uz]->struct_get(a, 0uz, false, observed) == status::invalid_reference);
    LOCAL_CHECK(stores[1uz]->struct_get(b, 0uz, false, observed) == status::invalid_reference);
    LOCAL_CHECK(collections == 3uz && rejected == 8uz && reclaimed_total == 9uz && same_actor_rejections == 1uz);
    LOCAL_CHECK(!bad_probe.load());
#if defined(UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP) && UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP == 1
    LOCAL_CHECK(local_hits.load() > 0uz && local_owners.load() == 3uz);
#else
    LOCAL_CHECK(local_hits.load() == 0uz && local_owners.load() == 0uz);
#endif
    ::fast_io::io::println("{\"fixture\":\"gc_collection_local_membership_reserved_fallback\",\"checks\":", checks,
        ",\"successful_collections\":", collections, ",\"rejected_collections\":", rejected,
        ",\"main_aggregate_reclaimed\":", reclaimed_total, ",\"local_lookup_hits\":", local_hits.load(),
        ",\"reserved_same_actor_rejections\":", same_actor_rejections,
        ",\"native_component_only\":true}");
}

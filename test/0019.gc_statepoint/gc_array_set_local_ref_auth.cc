// Same-operation array.set local authentication candidate.
// Actual native stores/admission/opaque references, not a VM/performance test.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <uwvm2/runtime/gc/instance_phase.h>
#include <uwvm2/utils/thread/execution_domain.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <initializer_list>
#include <memory>
#include <thread>
#include <utility>
#include <vector>
// Keep the actual platform selection live across this standalone fixture.
#include <uwvm2/utils/macro/push_macros.h>

namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace global = ::uwvm2::object::global;
using store_ptr = ::std::shared_ptr<gc::gc_object_store>;
using ref = gc::gc_reference;
using value = gc::gc_object_value;
using status = gc::gc_object_status;
namespace
{
    ::std::size_t checks{}, created{}, reclaimed_total{}, collections{};
    bool reset_domain_tested{};
    void check(bool yes, unsigned line) noexcept
    {
        ++checks;
        if(!yes)
        {
            ::fast_io::io::perrln("FAIL gc_array_set_local_ref_auth line=", line);
            ::fast_io::fast_terminate();
        }
    }
#define AUTH_CHECK(...) check(static_cast<bool>((__VA_ARGS__)), __LINE__)
    t::field_type scalar(t::value_kind kind, bool mutable_ = true)
    {
        t::field_type f{}; f.storage.value.kind = kind; f.mutable_ = mutable_; return f;
    }
    t::field_type reference_field(::std::int_least64_t heap, bool nullable = true, bool mutable_ = true)
    {
        auto f{scalar(t::value_kind::reference, mutable_)};
        f.storage.value.heap = {heap}; f.storage.value.nullable = nullable; return f;
    }
    t::recursive_type_section declarations()
    {
        t::recursive_type_section s{}; s.type_count = 12u;
        t::recursive_group g{};
        t::sub_type base{}; base.kind = t::composite_kind::struct_; base.final_ = false;
        base.fields.push_back(scalar(t::value_kind::i32));
        g.types.push_back(base); // 0: mutable ordinary node, no compact admission.
        auto child{base}; child.final_ = true; child.supertypes.push_back(0u);
        g.types.push_back(::std::move(child)); // 1: actual declared subtype.
        g.types.push_back(base); // 2: canonical equivalent at a distinct index.
        auto add_array{[&](t::field_type f)
        {
            t::sub_type a{}; a.kind = t::composite_kind::array;
            a.fields.push_back(f); g.types.push_back(::std::move(a));
        }};
        add_array(reference_field(0)); // 3: nullable mutable typed refs.
        add_array(reference_field(0, false)); // 4: non-null typed refs.
        add_array(reference_field(0, true, false)); // 5: immutable typed refs.
        add_array(scalar(t::value_kind::i32)); // 6: original numeric path.
        t::sub_type wrong{}; wrong.kind = t::composite_kind::struct_;
        wrong.fields.push_back(scalar(t::value_kind::i64));
        g.types.push_back(::std::move(wrong)); // 7: incompatible actual type.
        add_array(reference_field(static_cast<::std::int_least64_t>(t::abstract_heap_type::eq))); // 8
        t::sub_type function{}; function.kind = t::composite_kind::function;
        g.types.push_back(::std::move(function)); // 9: actual nonaggregate type.
        t::sub_type immutable{}; immutable.kind = t::composite_kind::struct_;
        immutable.fields.push_back(scalar(t::value_kind::i32, false));
        g.types.push_back(::std::move(immutable)); // 10: compact eligibility only when compact is ON.
        add_array(reference_field(10)); // 11: compact/legacy source must keep its own path.
        // Equivalent distinct indices need equivalent closed recursive groups.
        // Different projections inside one larger group are distinct types.
        unsigned first{};
        for(auto& type : g.types)
        {
            t::recursive_group singleton{}; singleton.first_type_index = first++;
            singleton.types.push_back(::std::move(type));
            s.groups.push_back(::std::move(singleton));
        }
        return s;
    }
    store_ptr make_store(::std::shared_ptr<gc::gc_lease_owner> const& leases)
    {
        auto schema{declarations()};
        auto s{::std::make_shared<gc::gc_object_store>(schema, leases)};
        AUTH_CHECK(s->valid()); return s;
    }
    bool same(ref a, ref b) noexcept { return a.kind == b.kind && a.storage.ptr == b.storage.ptr; }
    void expect_slot(store_ptr const& s, ref array, ::std::size_t index, ref wanted)
    {
        value got{}; AUTH_CHECK(s->array_get(array, index, false, got) == status::ok);
        AUTH_CHECK(same(got.as<ref>(), wanted));
    }
    void failed_write(store_ptr const& s, ref array, ::std::size_t index, ref input,
        status wanted, ref retained)
    {
        AUTH_CHECK(s->array_set(array, index, value::reference(input)) == wanted);
        expect_slot(s, array, 0uz, retained);
    }
    void collect(::std::array<store_ptr, 2uz> const& pins, ::std::vector<ref> const& roots,
        status wanted, ::std::size_t count)
    {
        // All workers/shared admissions have returned. Every actual owner and
        // semantic native root is included; no native borrow survives the pause.
        auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
        AUTH_CHECK(static_cast<bool>(exclusive));
        ::std::array<::std::uint64_t, 2uz> epochs{pins[0uz]->native_numeric_slab_statistics().epoch,
            pins[1uz]->native_numeric_slab_statistics().epoch};
        ::std::size_t reclaimed{999uz};
        auto const result{gc::gc_object_store::collect_exclusive_aggregate_domain(
            pins.data(), pins.size(), roots.data(), roots.size(), reclaimed)};
        AUTH_CHECK(result == wanted && reclaimed == count);
        if(wanted == status::ok) { ++collections; reclaimed_total += reclaimed; }
        else
        {
            AUTH_CHECK(pins[0uz]->native_numeric_slab_statistics().epoch == epochs[0uz]);
            AUTH_CHECK(pins[1uz]->native_numeric_slab_statistics().epoch == epochs[1uz]);
        }
    }
    void recipient_retirement()
    {
        auto receiver_leases{::std::make_shared<gc::gc_lease_owner>()};
        auto origin_leases{::std::make_shared<gc::gc_lease_owner>()};
        auto receiver{make_store(receiver_leases)}, origin{make_store(origin_leases)};
        ::std::weak_ptr<gc::gc_object_store> weak{origin};
        ref source{}, target{};
        {
            auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            AUTH_CHECK(origin->struct_new_default(0u, source) == status::ok);
            AUTH_CHECK(receiver->array_new_default(3u, 1uz, target) == status::ok);
            AUTH_CHECK(receiver->array_set(target, 0uz, value::reference(source)) == status::ok);
        }
        // Actual counted administration retires the receiver module's lease
        // owner. The destination object's independent embedded lease MUST survive.
        {
            ::uwvm2::runtime::gc::scoped_gc_root_graph_administration administration{};
            receiver_leases.reset(); origin.reset();
        }
        auto retained{weak.lock()};
        AUTH_CHECK(static_cast<bool>(retained));
        {
            auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            expect_slot(receiver, target, 0uz, source);
            value got{}; AUTH_CHECK(retained->struct_get(source, 0uz, false, got) == status::ok);
        }
        retained.reset(); receiver.reset(); // No active reader/borrow at owner teardown.
        AUTH_CHECK(weak.expired());
    }
    void recipient_failure()
    {
        auto origin_leases{::std::make_shared<gc::gc_lease_owner>()};
        auto origin{make_store(origin_leases)};
        auto receiver{make_store({})}; // No native recipient lease owner was supplied.
        auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        ref source{}, target{};
        AUTH_CHECK(origin->struct_new_default(0u, source) == status::ok);
        AUTH_CHECK(receiver->array_new_default(3u, 1uz, target) == status::ok);
        failed_write(receiver, target, 0uz, source, status::invalid_value, ref{});
        // Foreign weak promotion alone is not a receiving-module lease.
        // No destination mutation or embedded retention may follow this failure.
    }
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    void actual_execution_drain(store_ptr const& pin, ref target, ref source)
    {
        // This is a real execution_domain component, not a fabricated VM reset.
        ::uwvm2::utils::thread::execution_domain domain{2uz};
        ::std::atomic_bool active{}, release{}, cleaned{}, worker_ok{true};
        ::std::thread worker{[&, owner = pin]
        {
            auto generation{domain.try_enter()};
            if(!generation) { worker_ok.store(false); active.store(true); return; }
            auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            active.store(true);
            while(!release.load())
            {
                if(owner->array_set(target, 0uz, value::reference(source)) != status::ok)
                { worker_ok.store(false); break; }
                ::std::this_thread::yield();
            }
        }};
        auto const deadline{::std::chrono::steady_clock::now() + ::std::chrono::seconds{5}};
        while(!active.load())
        { AUTH_CHECK(::std::chrono::steady_clock::now() < deadline); ::std::this_thread::yield(); }
        ::std::thread maintenance{[&] { domain.stop_and_drain([&] { cleaned.store(true); }); }};
        // Capacity two leaves a real second admission available while the worker
        // holds its lease. A failed entry now proves closure, not capacity.
        // No main-thread domain lease is retained across the drain.
        while(domain.try_enter())
        { AUTH_CHECK(::std::chrono::steady_clock::now() < deadline); ::std::this_thread::yield(); }
        AUTH_CHECK(!cleaned.load());
        release.store(true); worker.join(); maintenance.join();
        AUTH_CHECK(worker_ok.load() && cleaned.load()); reset_domain_tested = true;
    }
#endif
}
// Force an inspectable actual C++/O3 entry. This is a native fixture symbol,
// never a generated-Wasm ABI or guest callable endpoint.
extern "C"
#if defined(__GNUC__) || defined(__clang__)
[[gnu::noinline]]
#endif
::std::uint32_t gc_array_auth_fixture_native_set(gc::gc_object_store* store,
    ref const* reference, ::std::size_t index, value const* input) noexcept
{
    if(store == nullptr || reference == nullptr || input == nullptr)
    { return static_cast<::std::uint32_t>(status::invalid_value); }
    return static_cast<::std::uint32_t>(store->array_set(*reference, index, *input));
}
int main()
{
    ::std::array leases{::std::make_shared<gc::gc_lease_owner>(), ::std::make_shared<gc::gc_lease_owner>()};
    ::std::array pins{make_store(leases[0uz]), make_store(leases[1uz])};
    ::std::vector<ref> roots{};
    ref a{}, child{}, alias{}, wrong{}, refs{}, nonnull{}, immutable{}, numbers{}, eqs{}, dead{}, b{}, foreign_array{}, compact{}, compact_refs{};
    {
        auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        auto new_struct{[&](store_ptr const& s, unsigned type, ref& result, bool rooted = true)
        {
            AUTH_CHECK(s->struct_new_default(type, result) == status::ok); ++created;
            if(rooted) { roots.push_back(result); }
        }};
        auto new_array{[&](store_ptr const& s, unsigned type, value input, ::std::size_t length, ref& result)
        {
            AUTH_CHECK(s->array_new(type, input, length, result) == status::ok); ++created; roots.push_back(result);
        }};
        new_struct(pins[0uz], 0u, a); new_struct(pins[0uz], 1u, child);
        new_struct(pins[0uz], 2u, alias); new_struct(pins[0uz], 7u, wrong);
        new_struct(pins[0uz], 0u, dead, false);
        new_struct(pins[1uz], 0u, b);
        new_array(pins[0uz], 3u, value::reference(a), 4uz, refs);
        new_array(pins[0uz], 4u, value::reference(a), 1uz, nonnull);
        new_array(pins[0uz], 5u, value::reference(a), 1uz, immutable);
        new_array(pins[0uz], 6u, value::i32(5u), 1uz, numbers);
        new_array(pins[0uz], 8u, value::reference(a), 1uz, eqs);
        new_array(pins[1uz], 3u, value::reference(b), 1uz, foreign_array);
        new_struct(pins[0uz], 10u, compact);
        new_array(pins[0uz], 11u, value::reference(compact), 1uz, compact_refs);
        auto input{value::reference(child)};
        AUTH_CHECK(gc_array_auth_fixture_native_set(pins[0uz].get(), &refs, 0uz, &input) == static_cast<::std::uint32_t>(status::ok));
        expect_slot(pins[0uz], refs, 0uz, child);
        AUTH_CHECK(pins[0uz]->array_set(refs, 0uz, value::reference(alias)) == status::ok);
        expect_slot(pins[0uz], refs, 0uz, alias); // Canonical equivalent distinct index.
        failed_write(pins[0uz], refs, 0uz, wrong, status::invalid_value, alias);
        auto fake{a};
        // [untrusted identity] never an object pointer: no dereference is permitted.
        // ^^ replace the carrier's opaque key by a never-issued maximum token.
        fake.storage.ptr = reinterpret_cast<void*>((::std::numeric_limits<::std::uintptr_t>::max)());
        failed_write(pins[0uz], refs, 0uz, fake, status::invalid_value, alias);
        // [live native store address] still NOT a guest reference token.
        // ^^ presenting a real header/store address must not bypass membership.
        fake.storage.ptr = pins[0uz].get();
        failed_write(pins[0uz], refs, 0uz, fake, status::invalid_value, alias);
        value native_slot{};
        // [actual native stack carrier] its address is outside issued identities.
        // ^^ only opaque-key comparison may inspect this forged payload.
        fake.storage.ptr = ::std::addressof(native_slot);
        failed_write(pins[0uz], refs, 0uz, fake, status::invalid_value, alias);
        auto wrong_kind{a}; wrong_kind.kind = global::wasm_ref_kind::wasm_array;
        failed_write(pins[0uz], refs, 0uz, wrong_kind, status::invalid_value, alias);
        failed_write(pins[0uz], refs, 4uz, fake, status::out_of_bounds, alias);
        failed_write(pins[0uz], immutable, 0uz, fake, status::immutable_field, a);
        failed_write(pins[0uz], nonnull, 0uz, ref{}, status::invalid_value, a);
        AUTH_CHECK(pins[0uz]->array_set(refs, 0uz, value::reference(ref{})) == status::ok);
        expect_slot(pins[0uz], refs, 0uz, ref{});
        AUTH_CHECK(pins[0uz]->array_set(refs, 0uz, value::reference(a)) == status::ok);
        AUTH_CHECK(pins[0uz]->array_set(eqs, 0uz, value::reference(refs)) == status::ok);
        expect_slot(pins[0uz], eqs, 0uz, refs); // Local array source, not only structs.
        auto i31{global::make_wasm_i31_reference(-1)};
        AUTH_CHECK(pins[0uz]->array_set(eqs, 0uz, value::reference(i31)) == status::ok);
        expect_slot(pins[0uz], eqs, 0uz, i31);
        for(auto kind : {global::wasm_ref_kind::wasm_exn, global::wasm_ref_kind::wasm_extern, global::wasm_ref_kind::wasm_func})
        { auto opaque{fake}; opaque.kind = kind; failed_write(pins[0uz], refs, 0uz, opaque, status::invalid_value, a); }
        AUTH_CHECK(pins[0uz]->array_set(refs, 0uz, value::reference(b)) == status::ok);
        expect_slot(pins[0uz], refs, 0uz, b); // Foreign source; module + embedded lease.
        AUTH_CHECK(pins[0uz]->array_set(foreign_array, 0uz, value::reference(b)) == status::ok);
        expect_slot(pins[0uz], foreign_array, 0uz, b); // BOTH foreign in same owner; no local proof.
        AUTH_CHECK(pins[0uz]->array_set(foreign_array, 0uz, value::reference(a)) == status::ok);
        expect_slot(pins[0uz], foreign_array, 0uz, a); // Foreign destination/other source owner.
        AUTH_CHECK(pins[0uz]->array_set(compact_refs, 0uz, value::reference(compact)) == status::ok);
        expect_slot(pins[0uz], compact_refs, 0uz, compact);
        AUTH_CHECK(pins[0uz]->array_set(numbers, 0uz, value::i32(0x80000001u)) == status::ok);
        value got{}; AUTH_CHECK(pins[0uz]->array_get(numbers, 0uz, false, got) == status::ok);
        AUTH_CHECK(got.as<::std::uint32_t>() == 0x80000001u);
    }
    collect(pins, roots, status::ok, 1uz); // The real unrooted numeric node becomes stale.
    ref replacement{};
    {
        auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        AUTH_CHECK(pins[0uz]->struct_new_default(0u, replacement) == status::ok); ++created; roots.push_back(replacement);
        // Fresh publication after actual reclamation never reissues a stale
        // identity. This assertion does not observe a native allocation address.
        AUTH_CHECK(!same(replacement, dead));
        failed_write(pins[0uz], refs, 0uz, dead, status::invalid_value, b);
        AUTH_CHECK(pins[0uz]->array_set(refs, 0uz, value::reference(replacement)) == status::ok);
        expect_slot(pins[0uz], refs, 0uz, replacement);
    }
    // Later invalid root must preserve the whole graph and every good readback.
    auto bad_roots{roots}; bad_roots.push_back(dead);
    collect(pins, bad_roots, status::invalid_reference, 0uz);
    {
        auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        expect_slot(pins[0uz], refs, 0uz, replacement);
    }
    ::std::atomic_bool workers_ok{true};
    auto mutate{[&](ref source)
    {
        auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        for(unsigned i{}; i != 4096u; ++i)
        {
            if(pins[0uz]->array_set(refs, 0uz, value::reference(source)) != status::ok)
            { workers_ok.store(false); return; }
            value got{};
            if(pins[0uz]->array_get(refs, 0uz, false, got) != status::ok ||
               (!same(got.as<ref>(), a) && !same(got.as<ref>(), child)))
            { workers_ok.store(false); return; }
        }
    }};
    // Establish a valid initial value before both admitted mutators race.
    {
        auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        AUTH_CHECK(pins[0uz]->array_set(refs, 0uz, value::reference(a)) == status::ok);
    }
    ::std::thread first{mutate, a}, second{mutate, child};
    first.join(); second.join(); AUTH_CHECK(workers_ok.load());
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    actual_execution_drain(pins[0uz], refs, a);
#endif
    recipient_retirement(); recipient_failure();
    roots.clear(); bad_roots.clear(); // Retire ALL semantic native roots before exclusive.
    collect(pins, roots, status::ok, created - 1uz);
    AUTH_CHECK(reclaimed_total == created);
    {
        auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
        value got{}; AUTH_CHECK(pins[0uz]->array_get(refs, 0uz, false, got) == status::invalid_reference);
    }
    ::fast_io::io::println("{\"fixture\":\"gc_array_set_local_ref_auth\",\"checks\":", checks,
        ",\"successful_collections\":", collections, ",\"created\":", created,
        ",\"reclaimed\":", reclaimed_total, ",\"reset_domain_tested\":", ::fast_io::mnp::boolalpha(reset_domain_tested),
#if defined(UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH) && UWVM_EXPERIMENTAL_ARRAY_SET_LOCAL_REF_AUTH == 1
        ",\"candidate_compiled\":true",
#else
        ",\"candidate_compiled\":false",
#endif
        ",\"creation_count_scope\":\"main_graph_only\",\"native_component_only\":true,\"vm_qualified\":false,\"performance_qualified\":false}");
}

#include <uwvm2/utils/macro/pop_macros.h>

// Native GC edge-liveness/admission regression. No Wasm/performance claim.
// Every collection receives the complete two-store closed cohort and explicit
// semantic roots. Saved integer tokens below are observer-only diagnostics,
// not retained guest values, native borrows, or roots.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <uwvm2/runtime/gc/instance_phase.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <thread>
#include <utility>
#include <vector>
#include <uwvm2/utils/macro/push_macros.h>

namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace global = ::uwvm2::object::global;
using store_ptr = ::std::shared_ptr<gc::gc_object_store>;
using reference = gc::gc_reference;
using value = gc::gc_object_value;
using status = gc::gc_object_status;
namespace
{
    ::std::size_t checks{};
    void check(bool yes, unsigned line) noexcept
    {
        ++checks; // Only the main thread calls this counter.
        if(!yes)
        {
            ::fast_io::io::perrln("FAIL gc_struct_set_edge_liveness line=", line);
            ::fast_io::fast_terminate();
        }
    }
#define EDGE_CHECK(...) check(static_cast<bool>((__VA_ARGS__)), __LINE__)

    struct observed_identity
    {
        ::std::uintptr_t token{};
        global::wasm_ref_kind kind{global::wasm_ref_kind::wasm_null};
        static observed_identity snapshot(reference input) noexcept
        { return {reinterpret_cast<::std::uintptr_t>(input.storage.ptr), input.kind}; }
        reference query_reference() const noexcept
        {
            // Reconstruct only an opaque lookup key. No token is dereferenced
            // or converted to a native object pointer in this fixture.
            reference result{};
            result.kind = kind;
            result.storage.ptr = reinterpret_cast<void*>(token);
            return result;
        }
        bool matches(reference input) const noexcept
        { return input.kind == kind && reinterpret_cast<::std::uintptr_t>(input.storage.ptr) == token; }
    };

    t::field_type number_field()
    {
        t::field_type field{};
        field.storage.value.kind = t::value_kind::i32;
        field.mutable_ = true; // Keep ordinary storage even in compact builds.
        return field;
    }
    t::field_type reference_field(::std::int_least64_t heap)
    {
        t::field_type field{};
        field.storage.value.kind = t::value_kind::reference;
        field.storage.value.heap = {heap};
        field.storage.value.nullable = true;
        field.mutable_ = true;
        return field;
    }
    t::recursive_type_section declarations()
    {
        t::recursive_type_section section{};
        section.type_count = 5u;
        auto add{[&](t::sub_type type)
        {
            t::recursive_group group{};
            group.first_type_index = static_cast<::std::uint_least32_t>(section.groups.size());
            group.types.push_back(::std::move(type));
            section.groups.push_back(::std::move(group));
        }};
        t::sub_type base{};
        base.kind = t::composite_kind::struct_;
        base.final_ = false;
        base.fields.push_back(number_field());
        auto child{base};
        child.final_ = true;
        child.supertypes.push_back(0u);
        add(::std::move(base));   // 0: mutable scalar source.
        add(::std::move(child));  // 1: declared subtype of source type 0.
        t::sub_type typed_holder{};
        typed_holder.kind = t::composite_kind::struct_;
        typed_holder.fields.push_back(reference_field(0));
        add(::std::move(typed_holder)); // 2: mutable typed nullable field.
        t::sub_type array{};
        array.kind = t::composite_kind::array;
        array.fields.push_back(number_field());
        add(::std::move(array)); // 3: real mutable i32 array source.
        t::sub_type eq_holder{};
        eq_holder.kind = t::composite_kind::struct_;
        eq_holder.fields.push_back(reference_field(
            static_cast<::std::int_least64_t>(t::abstract_heap_type::eq)));
        add(::std::move(eq_holder)); // 4: mutable nullable eqref field.
        return section;
    }

    struct context
    {
        ::std::array<::std::shared_ptr<gc::gc_lease_owner>, 2uz> leases{
            ::std::make_shared<gc::gc_lease_owner>(), ::std::make_shared<gc::gc_lease_owner>()};
        ::std::array<store_ptr, 2uz> pins{};
        ::std::size_t created{}, reclaimed{}, collections{}, edge_cases{}, exclusive_refusals{};
        context()
        {
            auto schema{declarations()};
            for(::std::size_t i{}; i != pins.size(); ++i)
            {
                pins[i] = ::std::make_shared<gc::gc_object_store>(schema, leases[i]);
                EDGE_CHECK(pins[i]->valid());
            }
        }
        observed_identity new_source(::std::size_t owner, unsigned type, ::std::uint32_t payload)
        {
            reference source{};
            if(type == 3u)
            {
                EDGE_CHECK(pins[owner]->array_new_default(type, 3uz, source) == status::ok);
                EDGE_CHECK(pins[owner]->array_set(source, 0uz, value::i32(payload)) == status::ok);
                EDGE_CHECK(pins[owner]->array_set(source, 2uz, value::i32(payload ^ 0x55aa55aau)) == status::ok);
            }
            else
            {
                EDGE_CHECK(pins[owner]->struct_new_default(type, source) == status::ok);
                EDGE_CHECK(pins[owner]->struct_set(source, 0uz, value::i32(payload)) == status::ok);
            }
            ++created;
            return observed_identity::snapshot(source);
        }
        observed_identity new_holder(unsigned type)
        {
            reference holder{};
            EDGE_CHECK(pins[0uz]->struct_new_default(type, holder) == status::ok);
            ++created;
            return observed_identity::snapshot(holder);
        }
        void collect(::std::initializer_list<observed_identity> semantic_roots, ::std::size_t expected)
        {
            // All mutators/native borrows have exited. Both actual source owners
            // remain strongly pinned through tracing, sweeping, and pause exit.
            auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
            EDGE_CHECK(static_cast<bool>(exclusive));
            ::std::vector<reference> roots{};
            roots.reserve(semantic_roots.size());
            for(auto root : semantic_roots) { roots.push_back(root.query_reference()); }
            ::std::size_t retired{999uz};
            auto const result{gc::gc_object_store::collect_exclusive_aggregate_domain(
                pins.data(), pins.size(), roots.data(), roots.size(), retired)};
            EDGE_CHECK(result == status::ok);
            EDGE_CHECK(retired == expected);
            reclaimed += retired;
            ++collections;
        }
        void read_source(::std::size_t owner, unsigned type, observed_identity source,
            ::std::uint32_t payload, status expected = status::ok)
        {
            value first{};
            auto const result{type == 3u ?
                pins[owner]->array_get(source.query_reference(), 0uz, false, first) :
                pins[owner]->struct_get(source.query_reference(), 0uz, false, first)};
            EDGE_CHECK(result == expected);
            if(expected != status::ok) { return; }
            EDGE_CHECK(first.as<::std::uint32_t>() == payload);
            if(type == 3u)
            {
                value last{};
                EDGE_CHECK(pins[owner]->array_get(source.query_reference(), 2uz, false, last) == status::ok);
                EDGE_CHECK(last.as<::std::uint32_t>() == (payload ^ 0x55aa55aau));
            }
        }
        void read_edge(observed_identity holder, observed_identity source)
        {
            value slot{};
            EDGE_CHECK(pins[0uz]->struct_get(holder.query_reference(), 0uz, false, slot) == status::ok);
            EDGE_CHECK(source.matches(slot.as<reference>()));
        }
        void clear_edge(observed_identity holder)
        {
            EDGE_CHECK(pins[0uz]->struct_set(holder.query_reference(), 0uz,
                value::reference(reference{})) == status::ok);
            value slot{};
            EDGE_CHECK(pins[0uz]->struct_get(holder.query_reference(), 0uz, false, slot) == status::ok);
            EDGE_CHECK(slot.as<reference>().kind == global::wasm_ref_kind::wasm_null);
        }
        void require_retired_holder(observed_identity holder)
        {
            value slot{};
            EDGE_CHECK(pins[0uz]->struct_get(holder.query_reference(), 0uz, false, slot) ==
                status::invalid_reference);
        }
    };

    void edge_liveness(context& state, ::std::size_t origin, unsigned source_type,
        ::std::uint32_t payload)
    {
        observed_identity source{}, holder{};
        {
            auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            source = state.new_source(origin, source_type, payload);
            holder = state.new_holder(source_type == 3u ? 4u : 2u);
            EDGE_CHECK(state.pins[0uz]->struct_set(holder.query_reference(), 0uz,
                value::reference(source.query_reference())) == status::ok);
            state.read_edge(holder, source);
        }
        state.collect({holder, source}, 0uz); // Establish direct-root baseline.
        // Withdraw the source's semantic root. Only the destination field keeps
        // it live. Integer observer identities do not grant tracing authority.
        state.collect({holder}, 0uz);
        {
            auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            state.read_edge(holder, source);
            state.read_source(origin, source_type, source, payload);
            state.clear_edge(holder);
        }
        state.collect({holder}, 1uz); // Broken edge makes exactly the source dead.
        {
            auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            state.read_source(origin, source_type, source, payload, status::invalid_reference);
            value slot{};
            EDGE_CHECK(state.pins[0uz]->struct_get(holder.query_reference(), 0uz, false, slot) == status::ok);
            EDGE_CHECK(slot.as<reference>().kind == global::wasm_ref_kind::wasm_null);
        }
        state.collect({}, 1uz);
        {
            auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            state.require_retired_holder(holder);
        }
        ++state.edge_cases;
    }

    void shared_admission_excludes_collection(context& state)
    {
        observed_identity source{}, holder{};
        constexpr ::std::uint32_t payload{0x31415926u};
        {
            auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            source = state.new_source(0uz, 1u, payload);
            holder = state.new_holder(2u);
        }
        ::std::atomic_bool active{}, release{}, worker_ok{true}, finished{};
        ::std::atomic<::std::size_t> writes{};
        auto const deadline{::std::chrono::steady_clock::now() + ::std::chrono::seconds{10}};
        ::std::thread worker{[&, owner = state.pins[0uz]]
        {
            {
                auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
                if(owner->struct_set(holder.query_reference(), 0uz,
                    value::reference(source.query_reference())) != status::ok)
                { worker_ok.store(false, ::std::memory_order_release); }
                else { writes.fetch_add(1uz, ::std::memory_order_relaxed); }
                active.store(true, ::std::memory_order_release);
                while(!release.load(::std::memory_order_acquire))
                {
                    if(::std::chrono::steady_clock::now() >= deadline)
                    { worker_ok.store(false, ::std::memory_order_release); break; }
                    if(owner->struct_set(holder.query_reference(), 0uz,
                        value::reference(source.query_reference())) != status::ok)
                    { worker_ok.store(false, ::std::memory_order_release); break; }
                    writes.fetch_add(1uz, ::std::memory_order_relaxed);
                    ::std::this_thread::yield();
                }
                // The worker's actual shared lease remains live through every
                // write and the main thread's nonwaiting exclusive refusals.
            }
            finished.store(true, ::std::memory_order_release);
        }};
        while(!active.load(::std::memory_order_acquire))
        {
            EDGE_CHECK(::std::chrono::steady_clock::now() < deadline);
            ::std::this_thread::yield();
        }
        EDGE_CHECK(worker_ok.load(::std::memory_order_acquire));
        EDGE_CHECK(!finished.load(::std::memory_order_acquire));
        for(unsigned attempt{}; attempt != 16u; ++attempt)
        {
            auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
            EDGE_CHECK(!exclusive);
            ++state.exclusive_refusals;
        }
        release.store(true, ::std::memory_order_release);
        while(!finished.load(::std::memory_order_acquire))
        {
            EDGE_CHECK(::std::chrono::steady_clock::now() < deadline);
            ::std::this_thread::yield();
        }
        worker.join();
        EDGE_CHECK(worker_ok.load(::std::memory_order_acquire));
        EDGE_CHECK(writes.load(::std::memory_order_relaxed) != 0uz);
        state.collect({holder}, 0uz); // Exclusive entry now succeeds; trace edge.
        {
            auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            state.read_edge(holder, source);
            state.read_source(0uz, 1u, source, payload);
            state.clear_edge(holder);
        }
        state.collect({holder}, 1uz);
        {
            auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            state.read_source(0uz, 1u, source, payload, status::invalid_reference);
        }
        state.collect({}, 1uz);
        {
            auto shared{::uwvm2::runtime::gc::runtime_gc_entry_admission.enter()};
            state.require_retired_holder(holder);
        }
    }
}

int main()
{
    context state{};
    edge_liveness(state, 0uz, 0u, 0x10203040u);
    edge_liveness(state, 0uz, 1u, 0x20304050u);
    edge_liveness(state, 0uz, 3u, 0x30405060u);
    edge_liveness(state, 1uz, 0u, 0x40506070u);
    edge_liveness(state, 1uz, 1u, 0x50607080u);
    edge_liveness(state, 1uz, 3u, 0x60708090u);
    shared_admission_excludes_collection(state);
    EDGE_CHECK(state.edge_cases == 6uz);
    EDGE_CHECK(state.exclusive_refusals == 16uz);
    EDGE_CHECK(state.created == 14uz && state.reclaimed == state.created);
    EDGE_CHECK(state.collections == 27uz);
    ::fast_io::io::println(
        "{\"fixture\":\"gc_struct_set_edge_liveness\",\"checks\":", checks,
        ",\"edge_cases\":", state.edge_cases,
        ",\"exclusive_refusals\":", state.exclusive_refusals,
        ",\"created\":", state.created, ",\"reclaimed\":", state.reclaimed,
        ",\"successful_collections\":", state.collections,
        ",\"native_component_only\":true,\"vm_qualified\":false,\"performance_qualified\":false}");
}
#include <uwvm2/utils/macro/pop_macros.h>

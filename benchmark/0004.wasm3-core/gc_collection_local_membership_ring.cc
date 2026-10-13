// Native collection component: 8/32 canonical stores, distributed reference
// cycles and exact complete roots. Not a VM or a general-GC performance claim.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <uwvm2/runtime/gc/instance_phase.h>
#include <fast_io.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
using status = gc::gc_object_status;
using reference = gc::gc_reference;
using value = gc::gc_object_value;
using clock_type = ::std::chrono::steady_clock;
namespace
{
    constexpr ::std::size_t per_store{64uz};
    constexpr ::std::size_t repetitions{1024uz};
    void require(bool condition, char const* what) noexcept
    {
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL gc_collection_local_membership_ring ",
                ::fast_io::mnp::os_c_str(what));
            ::fast_io::fast_terminate();
        }
    }
    ::std::uint64_t elapsed(clock_type::time_point first, clock_type::time_point last) noexcept
    {
        return static_cast<::std::uint64_t>(
            ::std::chrono::duration_cast<::std::chrono::nanoseconds>(last - first).count());
    }
    ::std::shared_ptr<gc::gc_object_store> make_store(
        ::std::shared_ptr<gc::gc_lease_owner> const& leases)
    {
        type::recursive_type_section schema{}; schema.type_count = 1u;
        type::recursive_group group{};
        type::sub_type node{}; node.kind = type::composite_kind::struct_;
        type::field_type scalar{}; scalar.mutable_ = true;
        scalar.storage.value.kind = type::value_kind::i32;
        type::field_type edge{}; edge.mutable_ = true;
        edge.storage.value.kind = type::value_kind::reference;
        edge.storage.value.heap = {static_cast<::std::int_least64_t>(type::abstract_heap_type::eq)};
        edge.storage.value.nullable = true;
        node.fields.push_back(scalar); node.fields.push_back(edge);
        group.types.push_back(::std::move(node)); schema.groups.push_back(::std::move(group));
        auto store{::std::make_shared<gc::gc_object_store>(schema, leases)};
        require(store->valid(), "actual canonical reference-bearing layout");
        return store;
    }
    template<::std::size_t Stores> void sample()
    {
        // This whole cohort is scoped to this sample; previous sample's stores
        // and all external/native actors have retired before the next cohort.
        ::std::array<::std::shared_ptr<gc::gc_lease_owner>, Stores> leases{};
        ::std::array<::std::shared_ptr<gc::gc_object_store>, Stores> stores{};
        ::std::array<reference, Stores * per_store> nodes{};
        ::std::array<reference, Stores> roots{};
        auto const setup_begin{clock_type::now()};
        for(::std::size_t index{}; index != Stores; ++index)
        {
            leases[index] = ::std::make_shared<gc::gc_lease_owner>();
            stores[index] = make_store(leases[index]);
        }
        for(::std::size_t index{}; index != nodes.size(); ++index)
        {
            // [nodes.data(),nodes.data()+nodes.size()) has initialized native
            // carrier slots. The real token remains opaque during construction.
            auto& owner{stores[index / per_store]};
            require(owner->struct_new_default(0u, nodes[index]) == status::ok, "new node");
            require(owner->struct_set(nodes[index], 0uz,
                value::i32(static_cast<::std::uint32_t>(index + 1uz))) == status::ok, "payload");
        }
        for(::std::size_t index{}; index != nodes.size(); ++index)
        {
            // Interleave store identities on EVERY edge, so all 8/32 pins must
            // be probed for real foreign nodes. This is not one store plus empty pins.
            auto const next{(index + per_store) % nodes.size()};
            require(stores[index / per_store]->struct_set(nodes[index], 1uz,
                value::reference(nodes[next])) == status::ok, "actual cross-store edge");
        }
        // Each column is a separate Stores-long cycle. Complete roots retain
        // every column exactly once, with duplicates used only for retention.
        ::std::array<reference, per_store + Stores> complete_roots{};
        for(::std::size_t index{}; index != per_store; ++index)
        { complete_roots[index] = nodes[index]; }
        for(::std::size_t index{}; index != Stores; ++index)
        {
            roots[index] = nodes[index * per_store];
            complete_roots[per_store + index] = roots[index];
            reference garbage{};
            require(stores[index]->struct_new_default(0u, garbage) == status::ok, "dead warmup node");
        }
        auto const setup_ns{elapsed(setup_begin, clock_type::now())};
        ::std::size_t reclaimed{};
        {
            auto stopped{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
            require(static_cast<bool>(stopped), "actual complete exclusive admission");
            require(gc::gc_object_store::collect_exclusive_aggregate_domain(stores.data(), stores.size(),
                complete_roots.data(), complete_roots.size(), reclaimed) == status::ok &&
                reclaimed == Stores, "warmup reclaims only dead nodes");
        }
        ::std::uint64_t collection_ns{};
        for(::std::size_t repetition{}; repetition != repetitions; ++repetition)
        {
            // Real closed admission is obtained before the measured collector
            // call; no mutator/readers or unpublished allocators exist here.
            auto stopped{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
            require(static_cast<bool>(stopped), "real exclusive admission");
            auto const begin{clock_type::now()};
            auto const result{gc::gc_object_store::collect_exclusive_aggregate_domain(stores.data(), stores.size(),
                complete_roots.data(), complete_roots.size(), reclaimed)};
            collection_ns += elapsed(begin, clock_type::now());
            require(result == status::ok && reclaimed == 0uz, "all complete roots stay alive");
        }
        ::std::uint64_t checksum{};
        auto const read_begin{clock_type::now()};
        for(::std::size_t index{}; index != nodes.size(); ++index)
        {
            value result{};
            require(stores[index / per_store]->struct_get(nodes[index], 0uz, false, result) == status::ok,
                "authenticated live scalar readback");
            checksum += result.as<::std::uint32_t>();
            require(stores[index / per_store]->struct_get(nodes[index], 1uz, false, result) == status::ok,
                "authenticated live cross-store readback");
            auto const actual{result.as<reference>()};
            auto const wanted{nodes[(index + per_store) % nodes.size()]};
            require(actual.kind == wanted.kind && actual.storage.ptr == wanted.storage.ptr,
                "unchanged actual foreign edge");
        }
        auto const readback_ns{elapsed(read_begin, clock_type::now())};
        auto const count{static_cast<::std::uint64_t>(nodes.size())};
        require(checksum == count * (count + 1u) / 2u, "independent scalar checksum");
        complete_roots = {}; roots = {}; // All semantic roots retire before sweeping.
        auto const drop_begin{clock_type::now()};
        {
            auto stopped{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
            require(static_cast<bool>(stopped), "actual final closed admission");
            require(gc::gc_object_store::collect_exclusive_aggregate_domain(stores.data(), stores.size(),
                nullptr, 0uz, reclaimed) == status::ok && reclaimed == nodes.size(), "actual cycle reclamation");
        }
        auto const teardown_ns{elapsed(drop_begin, clock_type::now())};
        value discarded{};
        require(stores[0uz]->struct_get(nodes[0uz], 0uz, false, discarded) == status::invalid_reference,
            "actual reclaimed token rejection");
#if defined(UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP) && UWVM_EXPERIMENTAL_COLLECTION_LOCAL_MEMBERSHIP == 1
        constexpr bool candidate{true};
#else
        constexpr bool candidate{false};
#endif
        // No hot-path IO. Collector region excludes admission acquisition,
        // setup/allocation/readback/drop; this native component is not VM ROI.
        ::fast_io::io::println("{\"fixture\":\"gc_collection_local_membership_ring\",\"candidate\":",
            ::fast_io::mnp::os_c_str(candidate ? "true" : "false"), ",\"stores\":", Stores, ",\"live_nodes\":", nodes.size(),
            ",\"collections\":", repetitions, ",\"setup_ns\":", setup_ns,
            ",\"collection_total_ns\":", collection_ns, ",\"readback_ns\":", readback_ns,
            ",\"teardown_ns\":", teardown_ns, ",\"checksum\":", checksum,
            ",\"warmup_reclaimed\":", Stores, ",\"final_reclaimed\":", reclaimed,
            ",\"native_component_only\":true}");
    }
}
int main()
{
    sample<8uz>();
    sample<32uz>();
}

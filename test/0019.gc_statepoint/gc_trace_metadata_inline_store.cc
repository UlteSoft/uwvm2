// Real token/cohort qualification of inline word bit 63 and the >64-field
// fallback. Native execution belongs exclusively to the Linux keeper.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <uwvm2/runtime/gc/instance_phase.h>
#include <fast_io.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
using reference = gc::gc_reference;
using value = gc::gc_object_value;
using status = gc::gc_object_status;

namespace
{
    ::std::size_t checks{};
    void require(bool condition, unsigned line) noexcept
    {
        ++checks;
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL gc_trace_metadata_inline_store line=", ::fast_io::mnp::dec(line));
            ::fast_io::fast_terminate();
        }
    }
#define INLINE_STORE_CHECK(...) require(static_cast<bool>((__VA_ARGS__)), __LINE__)
    ::std::shared_ptr<gc::gc_object_store> make_store(::std::size_t width,
        ::std::shared_ptr<gc::gc_lease_owner> const& leases)
    {
        type::recursive_type_section schema{};
        schema.type_count = 1u;
        type::recursive_group group{};
        type::sub_type node{};
        node.kind = type::composite_kind::struct_;
        for(::std::size_t index{}; index != width; ++index)
        {
            type::field_type field{};
            field.mutable_ = true;
            field.storage.value.kind = type::value_kind::i64;
            if(index == width - 1uz)
            {
                field.storage.value.kind = type::value_kind::reference;
                field.storage.value.heap.code = static_cast<::std::int_least64_t>(type::abstract_heap_type::eq);
                field.storage.value.nullable = true;
            }
            else if(index == 1uz)
            {
                field.storage.value.kind = type::value_kind::i32;
                field.storage.packed = type::packed_kind::i16;
            }
            node.fields.push_back(field);
        }
        group.types.push_back(::std::move(node));
        schema.groups.push_back(::std::move(group));
        auto store{::std::make_shared<gc::gc_object_store>(schema, leases)};
        INLINE_STORE_CHECK(store->valid());
        return store; // The copied canonical store outlives schema storage.
    }
    void collect(::std::array<::std::shared_ptr<gc::gc_object_store>, 2uz> const& stores,
        reference const* roots, ::std::size_t count, ::std::size_t wanted)
    {
        // All actors/roots are native fixture-owned; take an actual exclusive
        // admission lease before invoking the original stopped collector.
        auto stopped{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
        INLINE_STORE_CHECK(static_cast<bool>(stopped));
        ::std::size_t reclaimed{};
        INLINE_STORE_CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(
            stores.data(), stores.size(), roots, count, reclaimed) == status::ok);
        INLINE_STORE_CHECK(reclaimed == wanted);
    }
}

int main()
{
    ::std::array leases{::std::make_shared<gc::gc_lease_owner>(), ::std::make_shared<gc::gc_lease_owner>()};
    ::std::array stores{make_store(64uz, leases[0uz]), make_store(65uz, leases[1uz])};
    ::std::array<reference, 2uz> nodes{}, garbage{};
    for(::std::size_t index{}; index != stores.size(); ++index)
    {
        // [stores,nodes,garbage]+[0,2) are complete initialized native arrays.
        INLINE_STORE_CHECK(stores[index]->struct_new_default(0u, nodes[index]) == status::ok);
        INLINE_STORE_CHECK(stores[index]->struct_new_default(0u, garbage[index]) == status::ok);
        // Deliberately put a full reference-looking carrier into numeric field
        // zero. Its bits are valid numeric data and must NOT keep garbage alive.
        INLINE_STORE_CHECK(stores[index]->struct_set(nodes[index], 0uz,
            value::reference(garbage[index])) == status::ok);
        INLINE_STORE_CHECK(stores[index]->struct_set(nodes[index], 1uz,
            value::i32(0x12345678u)) == status::ok);
    }
    INLINE_STORE_CHECK(stores[0uz]->struct_set(nodes[0uz], 63uz, value::reference(nodes[1uz])) == status::ok);
    INLINE_STORE_CHECK(stores[1uz]->struct_set(nodes[1uz], 64uz, value::reference(nodes[0uz])) == status::ok);
    collect(stores, &nodes[0uz], 1uz, 2uz); // One root keeps the complete cross-store cycle.
    for(::std::size_t index{}; index != stores.size(); ++index)
    {
        value observed{};
        INLINE_STORE_CHECK(stores[index]->struct_get(garbage[index], 0uz, false, observed) == status::invalid_reference);
        INLINE_STORE_CHECK(stores[index]->struct_get(nodes[index], 0uz, false, observed) == status::ok);
        auto const numeric_bits{observed.as<reference>()};
        INLINE_STORE_CHECK(numeric_bits.kind == garbage[index].kind && numeric_bits.storage.ptr == garbage[index].storage.ptr);
        INLINE_STORE_CHECK(stores[index]->struct_get(nodes[index], 1uz, false, observed) == status::ok &&
            observed.as<::std::uint32_t>() == 0x5678u);
        auto const field{index == 0uz ? 63uz : 64uz};
        INLINE_STORE_CHECK(stores[index]->struct_get(nodes[index], field, false, observed) == status::ok);
        auto const edge{observed.as<reference>()};
        INLINE_STORE_CHECK(edge.kind == nodes[1uz - index].kind && edge.storage.ptr == nodes[1uz - index].storage.ptr);
    }
    collect(stores, nullptr, 0uz, 2uz); // No scalar bits or leases are semantic roots.
    value discarded{};
    INLINE_STORE_CHECK(stores[0uz]->struct_get(nodes[0uz], 63uz, false, discarded) == status::invalid_reference);
    INLINE_STORE_CHECK(stores[1uz]->struct_get(nodes[1uz], 64uz, false, discarded) == status::invalid_reference);
    ::fast_io::io::println("{\"fixture\":\"gc_trace_metadata_inline_store\",\"checks\":", ::fast_io::mnp::dec(checks),
        ",\"retained_cycle\":2,\"numeric_false_roots_reclaimed\":2,\"final_reclaimed\":2,\"native_component_only\":true}");
}

// Actual canonical store, opaque tokens, foreign lease and real exclusive
// admission. No VM root enumeration or timing claim; Linux keeper execution only.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <uwvm2/runtime/gc/instance_phase.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <utility>

namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace global = ::uwvm2::object::global;
using store_ptr = ::std::shared_ptr<gc::gc_object_store>;
using status = gc::gc_object_status;
using value = gc::gc_object_value;
using reference = gc::gc_reference;

#if defined(UWVM2TEST_TRACE_METADATA_STORE_OOM) && UWVM2TEST_TRACE_METADATA_STORE_OOM == 1
# if !defined(__ELF__) || !defined(UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA) || UWVM_EXPERIMENTAL_PRECISE_GC_TRACE_METADATA != 1
#  error Exact metadata-constructor OOM control needs ELF and the candidate ON.
# endif
# if !defined(UWVM2TEST_GC_TRACE_METADATA_ALLOCATION_PROBE) || UWVM2TEST_GC_TRACE_METADATA_ALLOCATION_PROBE != 1
#  error Store OOM must arm only at the real trace-index allocation rendezvous.
# endif
namespace
{
    ::std::atomic_bool requested_store_oom{}, fail_store_indices{};
    ::std::atomic_size_t actual_failures{}, matched_probes{};
}
extern "C" void trace_store_probe(::std::size_t) noexcept asm("uwvm2_test_gc_trace_metadata_allocation_probe");
extern "C" void trace_store_probe(::std::size_t count) noexcept
{
    if(count == 2uz && requested_store_oom.exchange(false))
    { matched_probes.fetch_add(1uz); fail_store_indices.store(true); }
}
# if SIZE_MAX == UINT64_MAX
extern "C" void* trace_store_real_array(::std::size_t, ::std::nothrow_t const&) noexcept asm("__real__ZnamRKSt9nothrow_t");
extern "C" void* trace_store_wrap_array(::std::size_t, ::std::nothrow_t const&) noexcept asm("__wrap__ZnamRKSt9nothrow_t");
# elif SIZE_MAX == UINT32_MAX
extern "C" void* trace_store_real_array(::std::size_t, ::std::nothrow_t const&) noexcept asm("__real__ZnajRKSt9nothrow_t");
extern "C" void* trace_store_wrap_array(::std::size_t, ::std::nothrow_t const&) noexcept asm("__wrap__ZnajRKSt9nothrow_t");
# else
#  error Unsupported size_t ABI for the actual allocation failure control.
# endif
extern "C" void* trace_store_wrap_array(::std::size_t bytes, ::std::nothrow_t const& tag) noexcept
{
    if(bytes == 2uz * sizeof(::std::size_t) && fail_store_indices.exchange(false))
    { actual_failures.fetch_add(1uz); return nullptr; }
    return trace_store_real_array(bytes, tag);
}
#endif

namespace
{
    ::std::size_t checks{}, collections{}, reclaimed_total{};
    void require(bool condition, unsigned line) noexcept
    {
        ++checks;
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL gc_trace_metadata_store line=", line);
            ::fast_io::fast_terminate();
        }
    }
#define TRACE_STORE_CHECK(...) require(static_cast<bool>((__VA_ARGS__)), __LINE__)
    t::field_type numeric(t::value_kind kind) noexcept
    { t::field_type result{}; result.storage.value.kind = kind; result.mutable_ = true; return result; }
    t::recursive_type_section declarations()
    {
        t::recursive_type_section section{};
        section.type_count = 4u;
        t::recursive_group group{};
        t::field_type edge{numeric(t::value_kind::reference)};
        edge.storage.value.heap = {static_cast<::std::int_least64_t>(t::abstract_heap_type::eq)};
        edge.storage.value.nullable = true;
        t::sub_type node{};
        node.kind = t::composite_kind::struct_;
        node.fields.push_back(numeric(t::value_kind::i32));
        node.fields.push_back(edge);
        auto packed{numeric(t::value_kind::i32)};
        packed.storage.packed = t::packed_kind::i16;
        node.fields.push_back(packed);
        node.fields.push_back(edge);
        node.fields.push_back(numeric(t::value_kind::i64));
        group.types.push_back(::std::move(node)); // 0: exact reference indices 1 and 3.
        t::sub_type refs{}; refs.kind = t::composite_kind::array; refs.fields.push_back(edge);
        group.types.push_back(::std::move(refs)); // 1: reference elements, including length zero.
        t::sub_type numbers{}; numbers.kind = t::composite_kind::array;
        numbers.fields.push_back(numeric(t::value_kind::i32));
        group.types.push_back(::std::move(numbers)); // 2: numeric leaf.
        t::sub_type empty{}; empty.kind = t::composite_kind::struct_;
        group.types.push_back(::std::move(empty)); // 3: zero-field numeric leaf.
        section.groups.push_back(::std::move(group));
        return section;
    }
    store_ptr make_store(::std::shared_ptr<gc::gc_lease_owner> const& leases)
    {
        auto section{declarations()};
        auto store{::std::make_shared<gc::gc_object_store>(section, leases)};
        TRACE_STORE_CHECK(store->valid());
        return store; // Parser declaration storage dies before the first object.
    }
    bool same(reference a, reference b) noexcept
    { return a.kind == b.kind && a.storage.ptr == b.storage.ptr; }
    void scalar(store_ptr const& store, reference object, ::std::size_t field, ::std::uint32_t wanted)
    {
        value observed{};
        TRACE_STORE_CHECK(store->struct_get(object, field, false, observed) == status::ok);
        TRACE_STORE_CHECK(observed.as<::std::uint32_t>() == wanted);
    }
    void edge(store_ptr const& store, reference object, ::std::size_t field, reference wanted)
    {
        value observed{};
        TRACE_STORE_CHECK(store->struct_get(object, field, false, observed) == status::ok);
        TRACE_STORE_CHECK(same(observed.as<reference>(), wanted));
    }
    void collect(::std::array<store_ptr, 2uz> const& stores,
        reference const* roots, ::std::size_t count, status wanted, ::std::size_t expected)
    {
        // Both actual stores and the complete semantic native roots are pinned.
        // No other mutator/VM/host reader exists. Borrow a REAL exclusive lease,
        // never a fabricated pause bool/ticket, and retire it before readback.
        auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
        TRACE_STORE_CHECK(static_cast<bool>(exclusive));
        ::std::size_t reclaimed{999uz};
        TRACE_STORE_CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(
            stores.data(), stores.size(), roots, count, reclaimed) == wanted);
        TRACE_STORE_CHECK(reclaimed == expected);
        if(wanted == status::ok) { ++collections; reclaimed_total += reclaimed; }
    }
}

int main()
{
    {
        auto bad{declarations()};
        // Native owned declaration control: a real invalid defined heap index
        // is rejected by canonical validation, not accepted by trace metadata.
        bad.groups.index_unchecked(0uz).types.index_unchecked(0uz).fields.index_unchecked(1uz).storage.value.heap = {99};
        auto failed{::std::make_shared<gc::gc_object_store>(bad)};
        TRACE_STORE_CHECK(!failed->valid());
    } // Failed store and every owned allocation drain before the valid cohort.
#if defined(UWVM2TEST_TRACE_METADATA_STORE_OOM) && UWVM2TEST_TRACE_METADATA_STORE_OOM == 1
    {
        static_assert(sizeof(t::field_type) != 2uz * sizeof(::std::size_t));
        auto section{declarations()};
        requested_store_oom.store(true);
        auto failed{::std::make_shared<gc::gc_object_store>(section)};
        TRACE_STORE_CHECK(matched_probes.load() == 1uz && actual_failures.load() == 1uz);
        TRACE_STORE_CHECK(!requested_store_oom.load() && !fail_store_indices.load());
        TRACE_STORE_CHECK(!failed->valid());
    } // ASan/UBSan must observe complete cleanup of all partial layout/index owners.
#endif
    // Real recipient/module lease owners outlive both stores and every foreign
    // access; a temporary constructor argument would leave only an expired weak pin.
    ::std::array leases{::std::make_shared<gc::gc_lease_owner>(), ::std::make_shared<gc::gc_lease_owner>()};
    ::std::array stores{make_store(leases[0uz]), make_store(leases[1uz])};
    reference a{}, b{}, foreign{}, refs{}, numbers{}, dead{}, zero{};
    TRACE_STORE_CHECK(stores[0uz]->struct_new_default(0u, a) == status::ok);
    TRACE_STORE_CHECK(stores[0uz]->struct_new_default(0u, b) == status::ok);
    TRACE_STORE_CHECK(stores[1uz]->struct_new_default(3u, foreign) == status::ok);
    TRACE_STORE_CHECK(stores[0uz]->struct_set(a, 0uz, value::i32(111u)) == status::ok);
    TRACE_STORE_CHECK(stores[0uz]->struct_set(b, 0uz, value::i32(222u)) == status::ok);
    TRACE_STORE_CHECK(stores[0uz]->struct_set(a, 2uz, value::i32(0x12345u)) == status::ok);
    TRACE_STORE_CHECK(stores[0uz]->struct_set(a, 1uz, value::reference(b)) == status::ok);
    TRACE_STORE_CHECK(stores[0uz]->struct_set(b, 1uz, value::reference(a)) == status::ok);
    TRACE_STORE_CHECK(stores[0uz]->struct_set(a, 3uz, value::reference(foreign)) == status::ok);
    TRACE_STORE_CHECK(stores[0uz]->array_new(1u, value::reference(a), 8uz, refs) == status::ok);
    TRACE_STORE_CHECK(stores[0uz]->array_set(refs, 7uz, value::reference(b)) == status::ok);
    TRACE_STORE_CHECK(stores[0uz]->array_new(2u, value::i32(333u), 17uz, numbers) == status::ok);
    TRACE_STORE_CHECK(stores[0uz]->array_new(2u, value::i32(444u), 17uz, dead) == status::ok);
    TRACE_STORE_CHECK(stores[0uz]->array_new_default(1u, 0uz, zero) == status::ok);
    ::std::array roots{refs, numbers, a, a, zero};
    collect(stores, roots.data(), roots.size(), status::ok, 1uz); // Only dead is unreachable.
    scalar(stores[0uz], a, 0uz, 111u); scalar(stores[0uz], b, 0uz, 222u);
    scalar(stores[0uz], a, 2uz, 0x2345u); // Packed bits must not become traced references.
    edge(stores[0uz], a, 1uz, b); edge(stores[0uz], b, 1uz, a);
    edge(stores[0uz], a, 3uz, foreign);
    value observed{};
    TRACE_STORE_CHECK(stores[0uz]->array_get(numbers, 16uz, false, observed) == status::ok);
    TRACE_STORE_CHECK(observed.as<::std::uint32_t>() == 333u);
    TRACE_STORE_CHECK(stores[0uz]->array_get(refs, 7uz, false, observed) == status::ok);
    TRACE_STORE_CHECK(same(observed.as<reference>(), b));
    TRACE_STORE_CHECK(stores[0uz]->array_get(numbers, 17uz, false, observed) == status::out_of_bounds);
    TRACE_STORE_CHECK(stores[0uz]->struct_get(a, 5uz, false, observed) == status::out_of_bounds);
    TRACE_STORE_CHECK(stores[0uz]->array_get(dead, 0uz, false, observed) == status::invalid_reference);
    reference unrooted{};
    TRACE_STORE_CHECK(stores[0uz]->array_new(2u, value::i32(555u), 17uz, unrooted) == status::ok);
    ::std::array invalid_roots{refs, dead};
    collect(stores, invalid_roots.data(), invalid_roots.size(), status::invalid_reference, 0uz);
    // A later invalid root cannot reclaim the otherwise unrooted new object.
    TRACE_STORE_CHECK(stores[0uz]->array_get(unrooted, 16uz, false, observed) == status::ok);
    TRACE_STORE_CHECK(observed.as<::std::uint32_t>() == 555u);
    edge(stores[0uz], a, 1uz, b);
    collect(stores, roots.data(), roots.size(), status::ok, 1uz);
    TRACE_STORE_CHECK(stores[0uz]->array_get(unrooted, 0uz, false, observed) == status::invalid_reference);
    // Drop EVERY semantic native root. The remaining local carrier variables
    // are deliberately retired keys, never later positive roots/borrowed views.
    roots = {};
    collect(stores, nullptr, 0uz, status::ok, 6uz); // A,B,foreign,refs,numbers,zero.
    TRACE_STORE_CHECK(stores[0uz]->struct_get(a, 0uz, false, observed) == status::invalid_reference);
    TRACE_STORE_CHECK(stores[1uz]->struct_get(foreign, 0uz, false, observed) == status::invalid_reference);
    ::fast_io::io::println("{\"fixture\":\"gc_trace_metadata_store\",\"checks\":", checks,
        ",\"successful_collections\":", collections, ",\"reclaimed\":", reclaimed_total,
        ",\"expected_reclaimed\":8,\"native_component_only\":true,\"performance_measured\":false}");
}

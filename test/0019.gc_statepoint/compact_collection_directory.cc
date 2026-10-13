// Real store/canonical cohort roots; no mock descriptor, issuer, admission
// token, pause ticket, LLVM IR, external-root registration or guest execution.
#include "compact_store_fixture.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <vector>
using namespace compact_test;

#if defined(UWVM2TEST_DIRECTORY_OOM) && UWVM2TEST_DIRECTORY_OOM == 1
# if !defined(__ELF__) || !defined(UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY) || UWVM_EXPERIMENTAL_COMPACT_COLLECTION_DIRECTORY != 1
#  error This exact array-ABI OOM proof needs ELF and the directory candidate ON.
# endif
namespace
{
    // Size description only, NOT a second descriptor, authority or directory.
    // The product's local record has these four actual ABI words; the observed
    // array-new call must match this before the test injects its one failure.
    struct allocation_shape { ::std::uintptr_t first, limit; void* descriptor; gc::gc_object_store const* owner; };
    static_assert(sizeof(allocation_shape) == 4uz*sizeof(::std::uintptr_t));
    constexpr ::std::size_t directory_bytes{65uz*sizeof(allocation_shape)};
    ::std::atomic_bool in_collection{}, fail_directory{};
    ::std::atomic_size_t matching_calls{}, injected{};
}
# if SIZE_MAX == UINT64_MAX
extern "C" void* directory_real_array(::std::size_t, ::std::nothrow_t const&) noexcept
    __asm__("__real__ZnamRKSt9nothrow_t");
extern "C" void* directory_wrap_array(::std::size_t, ::std::nothrow_t const&) noexcept
    __asm__("__wrap__ZnamRKSt9nothrow_t");
# elif SIZE_MAX == UINT32_MAX
extern "C" void* directory_real_array(::std::size_t, ::std::nothrow_t const&) noexcept
    __asm__("__real__ZnajRKSt9nothrow_t");
extern "C" void* directory_wrap_array(::std::size_t, ::std::nothrow_t const&) noexcept
    __asm__("__wrap__ZnajRKSt9nothrow_t");
# else
#  error Exact operator-new-array size_t ABI is not qualified for this target.
# endif
extern "C" void* directory_wrap_array(::std::size_t bytes, ::std::nothrow_t const& tag) noexcept
{
    if(in_collection.load(::std::memory_order_relaxed) && bytes == directory_bytes)
    {
        matching_calls.fetch_add(1uz, ::std::memory_order_relaxed);
        if(fail_directory.exchange(false, ::std::memory_order_relaxed))
        { injected.fetch_add(1uz, ::std::memory_order_relaxed); return nullptr; }
    }
    return directory_real_array(bytes, tag); // Forward the actual allocation ABI unchanged.
}
#endif

template<::std::size_t Count>
void run_collection(::std::array<shared_gc_store, Count> const& stores,
    gc::gc_reference const* roots, ::std::size_t length,
    gc::gc_object_status status, ::std::size_t expected)
{
#if defined(UWVM2TEST_DIRECTORY_OOM) && UWVM2TEST_DIRECTORY_OOM == 1
    in_collection.store(true, ::std::memory_order_relaxed); // OOM observation ONLY.
#endif
    // Original exact helper owns the REAL try_exclusive(0) lease. This fixture
    // owns ALL actual stores and semantically live native references, has no
    // peer/reader/VM/import/debug actor, and never treats a bool as admission.
    collect(stores, roots, length, status, expected);
#if defined(UWVM2TEST_DIRECTORY_OOM) && UWVM2TEST_DIRECTORY_OOM == 1
    in_collection.store(false, ::std::memory_order_relaxed);
#endif
}

int main()
{
    constexpr ::std::size_t root_total{65536uz}, stride{1024uz};
    auto leases_a{::std::make_shared<gc::gc_lease_owner>()};
    auto leases_b{::std::make_shared<gc::gc_lease_owner>()};
    auto leases_empty{::std::make_shared<gc::gc_lease_owner>()};
    t::recursive_type_section empty{};
    ::std::array stores{make_store(leases_a), make_store(leases_b),
        ::std::make_shared<gc::gc_object_store>(empty, leases_empty)};
    COMPACT_CHECK(stores[2uz]->valid());
    ::std::vector<gc::gc_reference> all(root_total);
    for(::std::size_t index{}; index != root_total; ++index)
    {
        // [0,65536) actual native carrier array. Each value is issued by the
        // REAL store; no reserved/uninitialized integer key is a positive root.
        auto field{gc::gc_object_value::i32(static_cast<::std::uint32_t>(index))};
        COMPACT_CHECK(stores[0uz]->struct_new(0u, &field, 1uz, all[index]) == gc::gc_object_status::ok);
    }
    gc::gc_reference foreign_nan{}, node{}, array{};
    auto field{gc::gc_object_value::i32(0xffc12345u)};
    COMPACT_CHECK(stores[1uz]->struct_new(1u, &field, 1uz, foreign_nan) == gc::gc_object_status::ok);
    COMPACT_CHECK(stores[0uz]->struct_new_default(3u, node) == gc::gc_object_status::ok);
    COMPACT_CHECK(stores[0uz]->struct_set(node, 0uz, gc::gc_object_value::reference(node)) == gc::gc_object_status::ok);
    COMPACT_CHECK(stores[0uz]->struct_set(node, 1uz, gc::gc_object_value::reference(all.back())) == gc::gc_object_status::ok);
    auto seed{gc::gc_object_value::reference(all.front())};
    COMPACT_CHECK(stores[0uz]->array_new(4u, seed, 128uz, array) == gc::gc_object_status::ok);
    COMPACT_CHECK(stores[0uz]->array_set(array, 127uz, gc::gc_object_value::reference(foreign_nan)) == gc::gc_object_status::ok);
    bits(stores[0uz], foreign_nan, 0xffc12345u); // Real recipient/module lease, not just pointer equality.
    // Genuine duplicate roots and a direct foreign-store root must preserve
    // the exact descriptor/slot pair; no synthetic descriptor is constructed.
    all.push_back(all.front()); all.push_back(foreign_nan); all.push_back(all.front());
    all.push_back(node); all.push_back(array);
    // Actual descriptor/reader positives: a semantic graph result alone cannot
    // prove that COMPACT was used (the valid legacy fallback has the same result).
    // Each fixed-width block starts with a real issued compact object; every
    // temporary native reader/strong view retires BEFORE try_exclusive(0).
    for(::std::size_t index{}; index != root_total; index += stride)
    {
        ::std::shared_ptr<gc::gc_object_store const> owner{stores[0uz]};
        gc::compact_numeric_reader reader{};
        // [0,65536) a real complete carrier; the native reader authenticates its
        // canonical owner/range/frontier/live before reading actual raw bits.
        COMPACT_CHECK(gc::compact_numeric_reader::try_local(owner, all[index], reader) == gc::compact_numeric_status::ok);
        ::std::uint32_t seen{};
        COMPACT_CHECK(reader.type_index() == 0u &&
            reader.raw_bits32(seen) == gc::compact_numeric_status::ok && seen == index);
        reader.reset();
    }
    {
        ::std::shared_ptr<gc::gc_object_store const> owner{stores[1uz]};
        gc::compact_numeric_reader reader{};
        COMPACT_CHECK(gc::compact_numeric_reader::try_local(owner, foreign_nan, reader) == gc::compact_numeric_status::ok);
        ::std::uint32_t seen{};
        COMPACT_CHECK(reader.type_index() == 1u && reader.value_kind() == gc::compact_numeric_kind::f32 &&
            reader.raw_bits32(seen) == gc::compact_numeric_status::ok && seen == 0xffc12345u);
        reader.reset();
    }

#if defined(UWVM2TEST_DIRECTORY_OOM) && UWVM2TEST_DIRECTORY_OOM == 1
    auto const epoch_before{stores[0uz]->native_numeric_slab_statistics().epoch};
    fail_directory.store(true, ::std::memory_order_relaxed);
    run_collection(stores, all.data(), all.size(), gc::gc_object_status::out_of_memory, 0uz);
    COMPACT_CHECK(injected.load() == 1uz && !fail_directory.load() && matching_calls.load() == 1uz);
    COMPACT_CHECK(stores[0uz]->native_numeric_slab_statistics().epoch == epoch_before);
    bits(stores[0uz], all.front(), 0u);
    bits(stores[0uz], all[root_total-1uz], static_cast<::std::uint32_t>(root_total-1uz));
    bits(stores[1uz], foreign_nan, 0xffc12345u);
#endif
    // 64 full i32 ranges plus a real f32 range in the second canonical store.
    // Entire graph is validated before sweep; numeric leafs never enter a
    // legacy pointer queue. One cycle and a 128-element reference array stay.
    run_collection(stores, all.data(), all.size(), gc::gc_object_status::ok, 0uz);
    auto wrong_kind{all.back()}; wrong_kind.kind = ::uwvm2::object::global::wasm_ref_kind::wasm_struct;
    // Actual array token with false kind is a NEGATIVE, never a fake authority.
    auto const epoch_before_late_root{stores[0uz]->native_numeric_slab_statistics().epoch};
    ::std::array bad{all.front(), foreign_nan, all.front(), wrong_kind};
    run_collection(stores, bad.data(), bad.size(), gc::gc_object_status::invalid_reference, 0uz);
    COMPACT_CHECK(stores[0uz]->native_numeric_slab_statistics().epoch == epoch_before_late_root);
    bits(stores[0uz], all.front(), 0u);
    bits(stores[0uz], all[root_total-1uz], static_cast<::std::uint32_t>(root_total-1uz));
    bits(stores[1uz], foreign_nan, 0xffc12345u);

    ::std::vector<gc::gc_reference> sparse{};
    for(::std::size_t index{}; index != root_total; index += stride)
    {
        // [0,65536) index is a genuine prior-issued key, retained as a root.
        sparse.push_back(all[index]);
    }
    sparse.push_back(node); sparse.push_back(array);
    // Main numeric survivors: 64 stride roots + final leaf (65535) via node.
    // Foreign f32 survives via array, plus 2 live legacy headers: 68 objects.
    // Other carrier copies in 'all' are DELIBERATE dead integer keys retained
    // only for stale negative checks, not omitted semantically-live host roots.
    run_collection(stores, sparse.data(), sparse.size(), gc::gc_object_status::ok, root_total - 65uz);
    gc::gc_object_value unchanged{gc::gc_object_value::i64(0x123456789abcdef0ULL)};
    COMPACT_CHECK(stores[0uz]->struct_get(all[1uz], 0uz, false, unchanged) == gc::gc_object_status::invalid_reference);
    COMPACT_CHECK(unchanged.as<::std::uint64_t>() == 0x123456789abcdef0ULL);
    bits(stores[0uz], all.front(), 0u);
    bits(stores[0uz], all[root_total-1uz], static_cast<::std::uint32_t>(root_total-1uz));
    bits(stores[0uz], foreign_nan, 0xffc12345u);
    // Reject a swept hole without advancing epochs or deleting survivors.
    auto const epoch_before_stale_root{stores[0uz]->native_numeric_slab_statistics().epoch};
    ::std::array hole{all.front(), foreign_nan, all[1uz]};
    run_collection(stores, hole.data(), hole.size(), gc::gc_object_status::invalid_reference, 0uz);
    COMPACT_CHECK(stores[0uz]->native_numeric_slab_statistics().epoch == epoch_before_stale_root);
    bits(stores[0uz], all.front(), 0u);
    bits(stores[0uz], all[root_total-1uz], static_cast<::std::uint32_t>(root_total-1uz));
    bits(stores[1uz], foreign_nan, 0xffc12345u);
    run_collection(stores, nullptr, 0uz, gc::gc_object_status::ok, 68uz);
    COMPACT_CHECK(stores[0uz]->struct_get(all.front(), 0uz, false, unchanged) == gc::gc_object_status::invalid_reference);
    COMPACT_CHECK(stores[1uz]->struct_get(foreign_nan, 0uz, false, unchanged) == gc::gc_object_status::invalid_reference);
    // Empty directory is a real canonical empty-store case, not a synthetic
    // count or directory activation. No descriptor/native owner is retained.
    run_collection(stores, nullptr, 0uz, gc::gc_object_status::ok, 0uz);
    ::fast_io::io::println("[PASS] compact_collection_directory checks=", checks,
        " collections=", collections, " reclaimed=", reclaimed_total,
        " numeric_roots=65536 genuine_ranges=65 scope=header-native-only vm=false performance=false");
}

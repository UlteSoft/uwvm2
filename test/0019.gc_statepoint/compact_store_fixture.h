// Actual product store/component proof; never mock publication or identity.
#pragma once
#if !defined(UWVM_EXPERIMENTAL_COMPACT_NUMERIC) || UWVM_EXPERIMENTAL_COMPACT_NUMERIC != 1
# error This private proof requires the explicitly pinned compact dual-store overlay.
#endif
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <fast_io.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <utility>

namespace compact_test
{
    namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
    namespace gc = ::uwvm2::uwvm::runtime::storage;
    using shared_gc_store = ::std::shared_ptr<gc::gc_object_store>;
    inline ::std::size_t checks{}, collections{}, reclaimed_total{};
    inline void require(bool condition, unsigned line) noexcept
    {
        ++checks;
        if(!condition)
        {
            ::fast_io::io::perrln("[FAIL] compact_store_native line=", line);
            ::fast_io::fast_terminate();
        }
    }
#define COMPACT_CHECK(...) ::compact_test::require(static_cast<bool>((__VA_ARGS__)), __LINE__)
    // This is the real recursive type section and real constructor-owned
    // schema, not an authority mock. Parser/validator/JIT are outside this
    // native component fixture's stated scope.
    [[nodiscard]] inline t::recursive_type_section declarations()
    {
        t::recursive_type_section section{};
        section.type_count = 8u;
        t::recursive_group group{};
        group.first_type_index = 0u;
        auto append_numeric = [&](t::value_kind kind, bool mutable_, t::packed_kind packed)
        {
            t::sub_type structure{};
            structure.kind = t::composite_kind::struct_;
            t::field_type field{};
            field.storage.value.kind = kind;
            field.storage.packed = packed;
            field.mutable_ = mutable_;
            structure.fields.push_back(field);
            group.types.push_back(::std::move(structure));
        };
        append_numeric(t::value_kind::i32, false, t::packed_kind::none); // 0 compact
        append_numeric(t::value_kind::f32, false, t::packed_kind::none); // 1 compact
        append_numeric(t::value_kind::i32, true, t::packed_kind::none);  // 2 legacy
        t::field_type reference{};
        reference.storage.value.kind = t::value_kind::reference;
        reference.storage.value.heap = {static_cast<::std::int_least64_t>(t::abstract_heap_type::eq)};
        reference.storage.value.nullable = true;
        reference.mutable_ = true;
        t::sub_type links{};
        links.kind = t::composite_kind::struct_;
        links.fields.push_back(reference);
        links.fields.push_back(reference);
        group.types.push_back(::std::move(links)); // 3 legacy cyclic node + numeric leaf
        t::sub_type array{};
        array.kind = t::composite_kind::array;
        array.fields.push_back(reference);
        group.types.push_back(::std::move(array)); // 4 legacy array
        append_numeric(t::value_kind::i64, false, t::packed_kind::none); // 5 legacy
        append_numeric(t::value_kind::i32, false, t::packed_kind::i16);  // 6 legacy
        t::sub_type pair{};
        pair.kind = t::composite_kind::struct_;
        t::field_type field{};
        field.storage.value.kind = t::value_kind::i32;
        pair.fields.push_back(field);
        pair.fields.push_back(field);
        group.types.push_back(::std::move(pair)); // 7 legacy carrier array
        section.groups.push_back(::std::move(group));
        return section;
    }
    [[nodiscard]] inline shared_gc_store make_store(::std::shared_ptr<gc::gc_lease_owner> const& leases)
    {
        auto section{declarations()};
        auto store{::std::make_shared<gc::gc_object_store>(section, leases)};
        COMPACT_CHECK(store->valid());
        // Actual constructor copies layouts/canonical IDs. The section dies
        // before the first allocation and does not become a lifetime pin.
        return store;
    }
    [[nodiscard]] inline bool same(gc::gc_reference a, gc::gc_reference b) noexcept
    { return a.kind == b.kind && a.storage.ptr == b.storage.ptr; }
    [[nodiscard]] inline ::std::uintptr_t identity(gc::gc_reference reference) noexcept
    { return reinterpret_cast<::std::uintptr_t>(reference.storage.ptr); } // Opaque key only; never dereference.
    inline void bits(shared_gc_store const& store, gc::gc_reference reference, ::std::uint32_t expected)
    {
        gc::gc_object_value observed{};
        COMPACT_CHECK(store->struct_get(reference, 0uz, false, observed) == gc::gc_object_status::ok);
        COMPACT_CHECK(observed.as<::std::uint32_t>() == expected);
        COMPACT_CHECK(store->struct_get32<false>(reference, 0uz) == expected);
        COMPACT_CHECK(store->struct_get32<true>(reference, 0uz) == expected);
    }
    template<::std::size_t Count>
    inline void collect(::std::array<shared_gc_store, Count> const& stores,
        gc::gc_reference const* roots, ::std::size_t root_count,
        gc::gc_object_status status, ::std::size_t expected)
    {
        // This fixture owns every live store, reports every semantically live
        // native reference and joins its workers before collection. Remaining
        // locals are deliberately retired token keys used only as negatives.
        // No raw native owner/reader or VM/import/debugger participant exists.
        auto exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive(0uz)};
        COMPACT_CHECK(static_cast<bool>(exclusive));
        ::std::size_t reclaimed{(~::std::size_t{})};
        COMPACT_CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(
            stores.data(), stores.size(), roots, root_count, reclaimed) == status);
        COMPACT_CHECK(reclaimed == expected);
        if(status == gc::gc_object_status::ok) { ++collections; reclaimed_total += reclaimed; }
    }
    inline void stale(shared_gc_store const& local, shared_gc_store const& recipient,
        gc::gc_reference reference)
    {
        gc::gc_object_value unchanged{gc::gc_object_value::i64(0xabcdef1234567890ULL)};
        COMPACT_CHECK(local->struct_get(reference, 0uz, false, unchanged) == gc::gc_object_status::invalid_reference);
        COMPACT_CHECK(recipient->struct_get(reference, 0uz, false, unchanged) == gc::gc_object_status::invalid_reference);
        COMPACT_CHECK(unchanged.as<::std::uint64_t>() == 0xabcdef1234567890ULL);
        gc::compact_numeric_reader reader{};
        COMPACT_CHECK(gc::compact_numeric_reader::try_foreign(reference, reader) ==
            gc::compact_numeric_status::invalid_reference && !reader.valid());
    }
}

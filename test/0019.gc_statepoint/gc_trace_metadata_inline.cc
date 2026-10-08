// Keeper-only native qualification of the optional bitmap trace plan. This
// test exercises layout boundaries and failure containment, not VM throughput.
#include <uwvm2/uwvm/runtime/storage/gc_trace_metadata.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>

namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;

#if !defined(UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA) || UWVM_EXPERIMENTAL_INLINE_GC_TRACE_METADATA != 1
# error "This qualification requires the explicit inline trace candidate"
#endif

#if defined(UWVM2TEST_INLINE_TRACE_METADATA_OOM) && UWVM2TEST_INLINE_TRACE_METADATA_OOM == 1
# if !defined(__ELF__) || !defined(UWVM2TEST_GC_TRACE_METADATA_ALLOCATION_PROBE) || UWVM2TEST_GC_TRACE_METADATA_ALLOCATION_PROBE != 1
#  error "Exact index allocation failure needs ELF and the native allocation probe"
# endif
namespace
{
    ::std::atomic_bool arm_probe{}, fail_allocation{};
    ::std::atomic_size_t probes{}, failures{};
}
extern "C" void inline_trace_probe(::std::size_t) noexcept asm("uwvm2_test_gc_trace_metadata_allocation_probe");
extern "C" void inline_trace_probe(::std::size_t count) noexcept
{
    if(count == 2uz && arm_probe.exchange(false))
    { ++probes; fail_allocation.store(true); }
}
# if SIZE_MAX == UINT64_MAX
extern "C" void* inline_trace_real_array(::std::size_t, ::std::nothrow_t const&) noexcept asm("__real__ZnamRKSt9nothrow_t");
extern "C" void* inline_trace_wrap_array(::std::size_t, ::std::nothrow_t const&) noexcept asm("__wrap__ZnamRKSt9nothrow_t");
# elif SIZE_MAX == UINT32_MAX
extern "C" void* inline_trace_real_array(::std::size_t, ::std::nothrow_t const&) noexcept asm("__real__ZnajRKSt9nothrow_t");
extern "C" void* inline_trace_wrap_array(::std::size_t, ::std::nothrow_t const&) noexcept asm("__wrap__ZnajRKSt9nothrow_t");
# else
#  error "Unqualified array-new ABI"
# endif
extern "C" void* inline_trace_wrap_array(::std::size_t bytes, ::std::nothrow_t const& tag) noexcept
{
    if(bytes == 2uz * sizeof(::std::size_t) && fail_allocation.exchange(false))
    { ++failures; return nullptr; }
    return inline_trace_real_array(bytes, tag);
}
#endif

namespace
{
    ::std::size_t checks{};
    void require(bool condition, unsigned line) noexcept
    {
        ++checks;
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL gc_trace_metadata_inline line=", ::fast_io::mnp::dec(line));
            ::fast_io::fast_terminate();
        }
    }
#define INLINE_TRACE_CHECK(...) require(static_cast<bool>((__VA_ARGS__)), __LINE__)
    type::field_type reference(type::abstract_heap_type heap) noexcept
    {
        type::field_type field{};
        field.storage.value.kind = type::value_kind::reference;
        field.storage.value.heap.code = static_cast<::std::int_least64_t>(heap);
        field.storage.value.nullable = true;
        return field;
    }
    template<::std::size_t Count> void shape()
    {
        ::std::array<type::field_type, Count> fields{};
        for(auto& field : fields) { field.storage.value.kind = type::value_kind::i32; }
        // [0,Count) Count is 64 or 65; both selected boundary fields exist.
        fields[0uz] = reference(type::abstract_heap_type::eq);
        fields[Count - 1uz] = reference(type::abstract_heap_type::exn);
        fields[1uz] = reference(type::abstract_heap_type::func);
        fields[1uz].storage.packed = type::packed_kind::i16; // Numeric, despite unused reference-looking bits.
        gc::gc_trace_metadata plan{};
        using kind = type::composite_kind;
        using status = gc::gc_trace_metadata_status;
        INLINE_TRACE_CHECK(plan.build(kind::struct_, fields.data(), Count) == status::ok);
        INLINE_TRACE_CHECK(plan.struct_reference_count() == 2uz);
        INLINE_TRACE_CHECK(plan.allocated_index_bytes() == (Count <= 64uz ? 0uz : 2uz * sizeof(::std::size_t)));
        ::std::array<::std::size_t, 2uz> visited{};
        ::std::size_t count{};
        INLINE_TRACE_CHECK(plan.visit_struct_reference_fields(Count, [&](::std::size_t field) noexcept
        {
            if(count >= visited.size()) { return false; }
            // [visited,visited+2) count < 2 before assigning this complete slot.
            visited[count++] = field;
            return true;
        }));
        INLINE_TRACE_CHECK(count == 2uz && visited[0uz] == 0uz && visited[1uz] == Count - 1uz);
        ::std::size_t result{999uz};
        INLINE_TRACE_CHECK(plan.reference_field_at(0uz, Count, result) && result == 0uz);
        INLINE_TRACE_CHECK(plan.reference_field_at(1uz, Count, result) && result == Count - 1uz);
        result = 999uz;
        INLINE_TRACE_CHECK(!plan.reference_field_at(2uz, Count, result) && result == 999uz);
        INLINE_TRACE_CHECK(!plan.reference_field_at(0uz, Count - 1uz, result) && result == 999uz);
        count = 0uz;
        INLINE_TRACE_CHECK(!plan.visit_struct_reference_fields(Count - 1uz, [&](::std::size_t) noexcept
        { ++count; return true; }) && count == 0uz);
        INLINE_TRACE_CHECK(!plan.visit_struct_reference_fields(Count, [&](::std::size_t) noexcept
        { ++count; return false; }) && count == 1uz);
#if defined(UWVM2TEST_INLINE_TRACE_METADATA_OOM) && UWVM2TEST_INLINE_TRACE_METADATA_OOM == 1
        if constexpr(Count > 64uz)
        {
            arm_probe.store(true);
            INLINE_TRACE_CHECK(plan.build(kind::struct_, fields.data(), Count) == status::out_of_memory);
            INLINE_TRACE_CHECK(probes.load() == 1uz && failures.load() == 1uz &&
                !arm_probe.load() && !fail_allocation.load());
            INLINE_TRACE_CHECK(!plan.matches_shape(kind::struct_, Count) && plan.allocated_index_bytes() == 0uz);
            count = 0uz;
            INLINE_TRACE_CHECK(!plan.visit_struct_reference_fields(Count, [&](::std::size_t) noexcept
            { ++count; return true; }) && count == 0uz);
            INLINE_TRACE_CHECK(plan.build(kind::struct_, fields.data(), Count) == status::ok);
        }
#endif
        // Invalid rebuild retires every old index/bitmap before returning.
        INLINE_TRACE_CHECK(plan.build(kind::array, fields.data(), Count) == status::invalid_layout);
        count = 0uz;
        INLINE_TRACE_CHECK(!plan.visit_struct_reference_fields(Count, [&](::std::size_t) noexcept
        { ++count; return true; }) && count == 0uz);
    }
}

int main()
{
    shape<64uz>();
    shape<65uz>();
    gc::gc_trace_metadata plan{};
    using kind = type::composite_kind;
    using status = gc::gc_trace_metadata_status;
    ::std::size_t count{};
    ::std::array<type::field_type, 64uz> dense{};
    for(auto& field : dense) { field = reference(type::abstract_heap_type::any); }
    INLINE_TRACE_CHECK(plan.build(kind::struct_, dense.data(), dense.size()) == status::ok &&
        plan.struct_reference_count() == dense.size() && plan.allocated_index_bytes() == 0uz);
    INLINE_TRACE_CHECK(plan.visit_struct_reference_fields(dense.size(), [&](::std::size_t field) noexcept
    { return field == count++; }) && count == 64uz);
    count = 0uz;
    INLINE_TRACE_CHECK(plan.build(kind::struct_, nullptr, 0uz) == status::ok);
    INLINE_TRACE_CHECK(plan.visit_struct_reference_fields(0uz, [&](::std::size_t) noexcept
    { ++count; return true; }) && count == 0uz);
    auto field{reference(type::abstract_heap_type::eq)};
    INLINE_TRACE_CHECK(plan.build(kind::array, &field, 1uz) == status::ok && plan.array_elements_are_references());
    INLINE_TRACE_CHECK(!plan.visit_struct_reference_fields(1uz, [&](::std::size_t) noexcept
    { ++count; return true; }) && count == 0uz);
    field.storage.packed = type::packed_kind::i8;
    INLINE_TRACE_CHECK(plan.build(kind::struct_, &field, 1uz) == status::ok && plan.numeric_leaf());
    INLINE_TRACE_CHECK(plan.visit_struct_reference_fields(1uz, [&](::std::size_t) noexcept
    { ++count; return true; }) && count == 0uz);
    auto const too_many{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / sizeof(field) + 1uz};
    INLINE_TRACE_CHECK(plan.build(kind::struct_, &field, too_many) == status::size_overflow);
    INLINE_TRACE_CHECK(!plan.matches_shape(kind::struct_, too_many));
    ::fast_io::io::println("{\"fixture\":\"gc_trace_metadata_inline\",\"checks\":", ::fast_io::mnp::dec(checks),
        ",\"metadata_object_bytes\":", ::fast_io::mnp::dec(sizeof(gc::gc_trace_metadata)),
        ",\"size_t_bytes\":", ::fast_io::mnp::dec(sizeof(::std::size_t)),
        ",\"small_struct_index_bytes\":0,\"two_ref_large_struct_index_bytes\":", ::fast_io::mnp::dec(2uz * sizeof(::std::size_t)),
        ",\"bitmap_boundary\":64,\"fallback_boundary\":65,\"native_component_only\":true}");
}

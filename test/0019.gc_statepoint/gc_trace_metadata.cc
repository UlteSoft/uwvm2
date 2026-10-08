// Cold native metadata controls; compile/run only in the keeper's real scope.
// This fixture classifies owned field arrays. It is not a Wasm validator,
// native admission proof, collector execution or performance measurement.
#include <uwvm2/uwvm/runtime/storage/gc_trace_metadata.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <limits>
#include <new>

namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace gc = ::uwvm2::uwvm::runtime::storage;
#if defined(UWVM2TEST_TRACE_METADATA_OOM) && UWVM2TEST_TRACE_METADATA_OOM == 1
# if !defined(__ELF__)
#  error This exact nothrow-array allocation failure control requires ELF.
# endif
namespace
{
    ::std::atomic_bool fail_trace_indices{};
    ::std::atomic_size_t actual_failures{};
}
# if SIZE_MAX == UINT64_MAX
extern "C" void* trace_real_array(::std::size_t, ::std::nothrow_t const&) noexcept asm("__real__ZnamRKSt9nothrow_t");
extern "C" void* trace_wrap_array(::std::size_t, ::std::nothrow_t const&) noexcept asm("__wrap__ZnamRKSt9nothrow_t");
# elif SIZE_MAX == UINT32_MAX
extern "C" void* trace_real_array(::std::size_t, ::std::nothrow_t const&) noexcept asm("__real__ZnajRKSt9nothrow_t");
extern "C" void* trace_wrap_array(::std::size_t, ::std::nothrow_t const&) noexcept asm("__wrap__ZnajRKSt9nothrow_t");
# else
#  error Unsupported size_t ABI for the allocation fault control.
# endif
extern "C" void* trace_wrap_array(::std::size_t bytes, ::std::nothrow_t const& tag) noexcept
{
    // This observes the ACTUAL single allocation in build(mixed fields), not
    // a fabricated descriptor/allocator result in a positive collection path.
    if(bytes == 3uz * sizeof(::std::size_t) && fail_trace_indices.exchange(false))
    { actual_failures.fetch_add(1uz); return nullptr; }
    return trace_real_array(bytes, tag);
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
            ::fast_io::io::perrln("FAIL gc_trace_metadata line=", line);
            ::fast_io::fast_terminate();
        }
    }
#define TRACE_CHECK(...) require(static_cast<bool>((__VA_ARGS__)), __LINE__)
    t::field_type numeric(t::value_kind kind) noexcept
    {
        t::field_type result{};
        result.storage.value.kind = kind;
        result.mutable_ = true;
        return result;
    }
    t::field_type reference(t::abstract_heap_type heap) noexcept
    {
        auto result{numeric(t::value_kind::reference)};
        result.storage.value.heap = {static_cast<::std::int_least64_t>(heap)};
        result.storage.value.nullable = true;
        return result;
    }
}

int main()
{
    using s = gc::gc_trace_metadata_status;
    using k = t::composite_kind;
    gc::gc_trace_metadata trace{};
    ::std::size_t result{12345uz};
    TRACE_CHECK(!trace.matches_shape(k::struct_, 0uz) && !trace.numeric_leaf());
    TRACE_CHECK(!trace.reference_field_at(0uz, 0uz, result) && result == 12345uz);
    TRACE_CHECK(trace.build(k::struct_, nullptr, 0uz) == s::ok);
    TRACE_CHECK(trace.matches_shape(k::struct_, 0uz) && trace.numeric_leaf());
    TRACE_CHECK(trace.struct_reference_count() == 0uz && trace.allocated_index_bytes() == 0uz);
    TRACE_CHECK(trace.build(k::function, nullptr, 0uz) == s::ok);
    TRACE_CHECK(!trace.numeric_leaf() && !trace.array_elements_are_references());

    ::std::array fields{numeric(t::value_kind::i32), reference(t::abstract_heap_type::any),
        reference(t::abstract_heap_type::any), numeric(t::value_kind::v128),
        reference(t::abstract_heap_type::func), reference(t::abstract_heap_type::any),
        reference(t::abstract_heap_type::exn), numeric(t::value_kind::f64)};
    // Packed storage ignores its unused value-type bits. These two slots are
    // numeric even with a reference-looking carrier descriptor; never trace them.
    fields[2uz].storage.packed = t::packed_kind::i8;
    fields[5uz].storage.packed = t::packed_kind::i16;
    TRACE_CHECK(trace.build(k::struct_, fields.data(), fields.size()) == s::ok);
    TRACE_CHECK(trace.matches_shape(k::struct_, 8uz) && !trace.matches_shape(k::array, 8uz));
    TRACE_CHECK(!trace.matches_shape(k::struct_, 7uz) && !trace.numeric_leaf());
    TRACE_CHECK(trace.struct_reference_count() == 3uz && trace.allocated_index_bytes() == 3uz * sizeof(::std::size_t));
    ::std::array const expected{1uz, 4uz, 6uz};
    for(::std::size_t position{}; position != expected.size(); ++position)
    {
        TRACE_CHECK(trace.reference_field_at(position, fields.size(), result));
        // [expected,expected+3) position < 3 selects one complete native index.
        TRACE_CHECK(result == expected[position]);
    }
    result = 12345uz;
    TRACE_CHECK(!trace.reference_field_at(3uz, fields.size(), result) && result == 12345uz);
    TRACE_CHECK(!trace.reference_field_at(0uz, fields.size() - 1uz, result) && result == 12345uz);
    TRACE_CHECK(trace.build(k::array, fields.data(), 0uz) == s::invalid_layout);
    TRACE_CHECK(!trace.matches_shape(k::struct_, 8uz) && trace.allocated_index_bytes() == 0uz);
    TRACE_CHECK(trace.build(k::array, fields.data(), 2uz) == s::invalid_layout);
    TRACE_CHECK(trace.build(k::function, fields.data(), 1uz) == s::invalid_layout);
    TRACE_CHECK(trace.build(static_cast<k>(99u), fields.data(), 1uz) == s::invalid_layout);
    TRACE_CHECK(trace.build(k::struct_, nullptr, 1uz) == s::invalid_layout);
    auto field{reference(t::abstract_heap_type::eq)};
    TRACE_CHECK(trace.build(k::array, ::std::addressof(field), 1uz) == s::ok);
    TRACE_CHECK(trace.array_elements_are_references() && !trace.numeric_leaf());
    TRACE_CHECK(trace.struct_reference_count() == 0uz && trace.allocated_index_bytes() == 0uz);
    TRACE_CHECK(!trace.reference_field_at(0uz, 1uz, result) && result == 12345uz);
    field.storage.packed = t::packed_kind::i8;
    TRACE_CHECK(trace.build(k::array, ::std::addressof(field), 1uz) == s::ok);
    TRACE_CHECK(!trace.array_elements_are_references() && trace.numeric_leaf());
    field.storage.packed = static_cast<t::packed_kind>(99u);
    TRACE_CHECK(trace.build(k::struct_, ::std::addressof(field), 1uz) == s::invalid_layout);
    field.storage.packed = t::packed_kind::none;
    field.storage.value.kind = static_cast<t::value_kind>(99u);
    TRACE_CHECK(trace.build(k::struct_, ::std::addressof(field), 1uz) == s::invalid_layout);
    field = numeric(t::value_kind::i32);
    auto const too_many{static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) /
                        sizeof(t::field_type) + 1uz};
    // The count overflow is rejected BEFORE dereferencing the one live field.
    TRACE_CHECK(trace.build(k::struct_, ::std::addressof(field), too_many) == s::size_overflow);
    TRACE_CHECK(!trace.matches_shape(k::struct_, too_many) && trace.allocated_index_bytes() == 0uz);
    TRACE_CHECK(trace.build(k::struct_, ::std::addressof(field), 1uz) == s::ok);
    TRACE_CHECK(trace.numeric_leaf() && trace.struct_reference_count() == 0uz);
#if defined(UWVM2TEST_TRACE_METADATA_OOM) && UWVM2TEST_TRACE_METADATA_OOM == 1
    fail_trace_indices.store(true);
    TRACE_CHECK(trace.build(k::struct_, fields.data(), fields.size()) == s::out_of_memory);
    TRACE_CHECK(actual_failures.load() == 1uz && !fail_trace_indices.load());
    TRACE_CHECK(!trace.matches_shape(k::struct_, fields.size()) && trace.allocated_index_bytes() == 0uz);
    result = 12345uz;
    TRACE_CHECK(!trace.reference_field_at(0uz, fields.size(), result) && result == 12345uz);
    TRACE_CHECK(trace.build(k::struct_, fields.data(), fields.size()) == s::ok);
    TRACE_CHECK(trace.struct_reference_count() == 3uz);
#endif
    ::fast_io::io::println("{\"fixture\":\"gc_trace_metadata\",\"checks\":", checks,
        ",\"metadata_object_bytes\":", sizeof(gc::gc_trace_metadata),
        ",\"mixed_struct_index_bytes\":", 3uz * sizeof(::std::size_t),
        ",\"array_or_numeric_leaf_index_bytes\":0,\"performance_measured\":false}");
}

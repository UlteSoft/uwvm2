// ASan/UBSan qualification for a one-allocation GC object/value layout.
// The same source is compiled against the frozen baseline and isolated overlay.
#include <uwvm2/uwvm/runtime/storage/gc_object.h>
#include <fast_io.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <memory>
#include <new>
#include <utility>

namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace g = ::uwvm2::object::global;

#if defined(__linux__) && defined(GC_LAYOUT_FAULT_INJECT)
namespace
{
    ::std::atomic<bool> fail_large_nothrow{};
    ::std::atomic<unsigned> failed_scalar_allocations{};
    ::std::atomic<unsigned> failed_array_allocations{};
}

// ELF linker wrapping preserves the ordinary allocator and its matching
// delete on every successful call. Only the one requested large allocation
// fails, so ASan can verify cleanup without cgroup memory pressure.
extern "C" void* bench_real_scalar_new_nothrow(::std::size_t, ::std::nothrow_t const&) noexcept
    asm("__real__ZnwmRKSt9nothrow_t");
extern "C" void* bench_real_array_new_nothrow(::std::size_t, ::std::nothrow_t const&) noexcept
    asm("__real__ZnamRKSt9nothrow_t");
extern "C" void* bench_wrap_scalar_new_nothrow(::std::size_t, ::std::nothrow_t const&) noexcept
    asm("__wrap__ZnwmRKSt9nothrow_t");
extern "C" void* bench_wrap_array_new_nothrow(::std::size_t, ::std::nothrow_t const&) noexcept
    asm("__wrap__ZnamRKSt9nothrow_t");

extern "C" void* bench_wrap_scalar_new_nothrow(::std::size_t size, ::std::nothrow_t const& tag) noexcept
{
    if(size >= 4096uz && fail_large_nothrow.load(::std::memory_order_relaxed))
    {
        failed_scalar_allocations.fetch_add(1u, ::std::memory_order_relaxed);
        return nullptr;
    }
    return bench_real_scalar_new_nothrow(size, tag);
}

extern "C" void* bench_wrap_array_new_nothrow(::std::size_t size, ::std::nothrow_t const& tag) noexcept
{
    if(size >= 4096uz && fail_large_nothrow.load(::std::memory_order_relaxed))
    {
        failed_array_allocations.fetch_add(1u, ::std::memory_order_relaxed);
        return nullptr;
    }
    return bench_real_array_new_nothrow(size, tag);
}
#endif

namespace
{
    [[noreturn]] void fail(char const* label)
    {
        ::fast_io::io::println("FAIL gc layout edge case: ", ::fast_io::mnp::os_c_str(label));
        ::fast_io::fast_terminate();
    }

    void check(bool condition, char const* label)
    { if(!condition) { fail(label); } }

    t::field_type field(t::value_kind kind, bool mutable_, bool nullable = false)
    {
        t::field_type result{};
        result.storage.value.kind = kind;
        result.storage.value.nullable = nullable;
        result.mutable_ = mutable_;
        if(kind == t::value_kind::reference)
        {
            result.storage.value.heap = t::heap_type{
                static_cast<::std::int_least64_t>(t::abstract_heap_type::any)};
        }
        return result;
    }

    t::recursive_type_section make_types()
    {
        t::recursive_type_section section{};
        section.type_count = 4u;
        t::recursive_group group{};
        group.first_type_index = 0u;
        t::sub_type source_struct{};
        source_struct.kind = t::composite_kind::struct_;
        source_struct.fields.push_back(field(t::value_kind::i64, true));
        group.types.push_back(::std::move(source_struct));
        t::sub_type numeric_array{};
        numeric_array.kind = t::composite_kind::array;
        numeric_array.fields.push_back(field(t::value_kind::i64, true));
        group.types.push_back(::std::move(numeric_array));
        t::sub_type reference_array{};
        reference_array.kind = t::composite_kind::array;
        reference_array.fields.push_back(field(t::value_kind::reference, true, true));
        group.types.push_back(::std::move(reference_array));
        t::sub_type failure_struct{};
        failure_struct.kind = t::composite_kind::struct_;
        failure_struct.fields.push_back(field(t::value_kind::i64, true));
        failure_struct.fields.push_back(field(t::value_kind::reference, true, true));
        group.types.push_back(::std::move(failure_struct));
        section.groups.push_back(::std::move(group));
        return section;
    }
}

int main()
{
    auto const types{make_types()};
    gc::gc_object_store store{types};
    check(store.valid(), "type layout initialization");
#if defined(__linux__) && defined(GC_LAYOUT_FAULT_INJECT)
    gc::gc_reference failed_large{};
    fail_large_nothrow.store(true, ::std::memory_order_relaxed);
    auto const oom_status{store.array_new(1u, gc::gc_object_value::i64(1u),
                                          1024uz, failed_large)};
    fail_large_nothrow.store(false, ::std::memory_order_relaxed);
    check(oom_status == gc::gc_object_status::out_of_memory,
          "large allocation reports out_of_memory under fault injection");
#if defined(GC_LAYOUT_EXPECT_SINGLE_BLOCK)
    check(failed_scalar_allocations.load(::std::memory_order_relaxed) == 1u &&
          failed_array_allocations.load(::std::memory_order_relaxed) == 0u,
          "single-block path failed its one large scalar allocation");
#else
    check(failed_scalar_allocations.load(::std::memory_order_relaxed) == 0u &&
          failed_array_allocations.load(::std::memory_order_relaxed) == 1u,
          "baseline path released header after large array allocation failed");
#endif
#endif
    gc::gc_reference empty{};
    check(store.array_new(1u, gc::gc_object_value::i64(7u), 0uz, empty) ==
          gc::gc_object_status::ok, "zero-length array allocation");
    ::std::size_t length{99uz};
    check(store.array_length(empty, length) == gc::gc_object_status::ok && length == 0uz,
          "zero-length array header and length");
    gc::gc_object_value readback{};
    check(store.array_get(empty, 0uz, false, readback) == gc::gc_object_status::out_of_bounds,
          "zero-length array bound");

    gc::gc_reference numeric{};
    check(store.array_new(1u, gc::gc_object_value::i64(0u), 3uz, numeric) ==
          gc::gc_object_status::ok, "eight-byte field array allocation");
    for(::std::size_t index{}; index != 3uz; ++index)
    {
        auto const value{::std::uint64_t{0x1020304050607080u} + index};
        check(store.array_set(numeric, index, gc::gc_object_value::i64(value)) ==
              gc::gc_object_status::ok, "eight-byte field set");
        check(store.array_get(numeric, index, false, readback) ==
              gc::gc_object_status::ok && readback.as<::std::uint64_t>() == value,
              "eight-byte aligned field readback");
    }

    auto const over_ptrdiff{static_cast<::std::size_t>(
        (::std::numeric_limits<::std::ptrdiff_t>::max)() / sizeof(gc::gc_object_value)) + 2uz};
    check(store.array_new(1u, gc::gc_object_value::i64(1u), over_ptrdiff, numeric) ==
          gc::gc_object_status::size_overflow, "PTRDIFF_MAX allocation rejection");
    check(store.array_new(1u, gc::gc_object_value::i64(1u),
                          (::std::numeric_limits<::std::size_t>::max)(), numeric) ==
          gc::gc_object_status::size_overflow, "SIZE_MAX allocation rejection");

    gc::gc_reference forged{};
    forged.kind = g::wasm_ref_kind::wasm_struct;
    forged.storage.ptr = reinterpret_cast<void*>(1u);
    ::std::array<::std::byte, sizeof(::std::uint64_t) + sizeof(gc::gc_reference)> struct_stack{};
    ::std::uint64_t const first{7u};
    ::std::memcpy(struct_stack.data(), ::std::addressof(first), sizeof(first));
    ::std::memcpy(struct_stack.data() + sizeof(first), ::std::addressof(forged), sizeof(forged));
    for(unsigned attempt{}; attempt != 1024u; ++attempt)
    {
        gc::gc_reference rejected{};
        check(store.struct_new_from_stack(3u, struct_stack.data(), struct_stack.size(), rejected) ==
              gc::gc_object_status::invalid_value, "partial struct failure cleanup");
    }
    gc::gc_reference null_ref{};
    null_ref.kind = g::wasm_ref_kind::wasm_null;
    ::std::array<::std::byte, 2uz * sizeof(gc::gc_reference)> array_stack{};
    ::std::memcpy(array_stack.data(), ::std::addressof(null_ref), sizeof(null_ref));
    ::std::memcpy(array_stack.data() + sizeof(null_ref), ::std::addressof(forged), sizeof(forged));
    for(unsigned attempt{}; attempt != 1024u; ++attempt)
    {
        gc::gc_reference rejected{};
        check(store.array_new_fixed_from_stack(2u, array_stack.data(), array_stack.size(),
                                               2uz, rejected) == gc::gc_object_status::invalid_value,
              "partial array failure cleanup");
    }

    ::std::weak_ptr<gc::gc_object_store> source_weak{};
    {
        auto source_owner{::std::make_shared<gc::gc_lease_owner>()};
        auto source{::std::make_shared<gc::gc_object_store>(types, source_owner)};
        auto receiver_owner{::std::make_shared<gc::gc_lease_owner>()};
        auto receiver{::std::make_shared<gc::gc_object_store>(types, receiver_owner)};
        check(source->valid() && receiver->valid(), "foreign stores initialized");
        gc::gc_object_value initial[]{gc::gc_object_value::i64(42u)};
        gc::gc_reference source_object{};
        check(source->struct_new(0u, initial, 1uz, source_object) == gc::gc_object_status::ok,
              "foreign source object allocation");
        gc::gc_reference receiver_object{};
        check(receiver->array_new(2u, gc::gc_object_value::reference(source_object),
                                  1uz, receiver_object) == gc::gc_object_status::ok,
              "foreign object field lease");
        source_weak = source;
        source.reset();
        source_owner.reset();
        check(!source_weak.expired(), "foreign source retained by receiving object");
        receiver.reset();
        receiver_owner.reset();
    }
    check(source_weak.expired(), "foreign source released after receiver teardown");
    ::fast_io::io::println("PASS GC layout edge cases: zero length, overflow, aligned fields, "
                           "fault-injected OOM, partial failure cleanup, foreign lease");
}

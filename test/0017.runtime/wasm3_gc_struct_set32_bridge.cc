// Native component only: exact production candidate versus the extracted
// original fixed-buffer bridge. No fake tokens are dereferenced and no VM
// safepoint/automatic collection or performance result is claimed here.
// Match the runtime aggregation's outer macro scope before any pragma-once
// experimental capability header is first included. Balance it after main.
#include <uwvm2/utils/macro/push_macros.h>
#include <uwvm2/uwvm/runtime/storage/wasm_module.h>
#include <uwvm2/runtime/gc/allocation_policy.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <type_traits>
#include <thread>
#include <utility>
#include <fast_io.h>
namespace fixture
{
#include "native_gc_struct_set32_baseline.h"
#include <uwvm2/runtime/compiler/llvm_jit/compile_all_from_uwvm/translate/single_func_gc_struct_set32_bridge.h>
}
namespace gc = ::uwvm2::uwvm::runtime::storage;
namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace g = ::uwvm2::object::global;
using status = gc::gc_object_status;
using reference = gc::gc_reference;
using value = gc::gc_object_value;
namespace
{
    ::std::size_t checks{};
    void require(bool condition, unsigned line) noexcept
    {
        ++checks;
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL struct.set32 native line=", line);
            ::fast_io::fast_terminate();
        }
    }
#define SET32_CHECK(...) require(static_cast<bool>((__VA_ARGS__)), __LINE__)
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    ::std::size_t boundaries{};
    void observe_boundary(bool revoke) noexcept
    { SET32_CHECK(!revoke); ++boundaries; }
#endif
    t::field_type field(t::value_kind kind, bool mutable_ = true,
        t::packed_kind packed = t::packed_kind::none)
    {
        t::field_type out{}; out.storage.value.kind = kind;
        out.mutable_ = mutable_; out.storage.packed = packed;
        if(kind == t::value_kind::reference)
        {
            out.storage.value.heap.code = static_cast<::std::int_least64_t>(t::abstract_heap_type::eq);
            out.storage.value.nullable = true;
        }
        return out;
    }
    t::recursive_type_section types()
    {
        t::recursive_type_section out{}; out.type_count = 3u;
        t::recursive_group group{};
        t::sub_type base{}; base.kind = t::composite_kind::struct_; base.final_ = false;
        base.fields.push_back(field(t::value_kind::i32));
        base.fields.push_back(field(t::value_kind::f32));
        base.fields.push_back(field(t::value_kind::i32, true, t::packed_kind::i8));
        base.fields.push_back(field(t::value_kind::i32, true, t::packed_kind::i16));
        base.fields.push_back(field(t::value_kind::i32, false));
        group.types.push_back(base);
        t::sub_type child{base}; child.final_ = true; child.supertypes.push_back(0u);
        child.fields.push_back(field(t::value_kind::i64));
        child.fields.push_back(field(t::value_kind::reference));
        group.types.push_back(::std::move(child));
        t::sub_type array{}; array.kind = t::composite_kind::array;
        array.fields.push_back(field(t::value_kind::i32)); group.types.push_back(::std::move(array));
        out.groups.push_back(::std::move(group)); return out;
    }
    void initialize(gc::wasm_module_storage_t& module)
    {
        module.gc_lease_roots = ::std::make_shared<gc::gc_lease_owner>();
        auto declarations{types()};
        module.gc_store = ::std::make_shared<gc::gc_object_store>(declarations, module.gc_lease_roots);
        SET32_CHECK(module.gc_store->valid());
    }
    reference create(gc::wasm_module_storage_t& module, ::std::uint32_t type)
    {
        reference out{};
        SET32_CHECK(module.gc_store->struct_new_default(type, out) == status::ok);
        return out;
    }
    ::std::uintptr_t scalar(gc::wasm_module_storage_t& module, reference ref,
        ::std::uint32_t index, ::std::uint32_t bits) noexcept
    {
        return fixture::uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1(
            reinterpret_cast<::std::uintptr_t>(::std::addressof(module)),
            static_cast<::std::uint32_t>(ref.kind), reinterpret_cast<::std::uintptr_t>(ref.storage.ptr), index, bits);
    }
    ::std::uint32_t generic(gc::wasm_module_storage_t& module, reference ref,
        ::std::uint32_t index, ::std::uint32_t bits) noexcept
    {
        ::std::array<value, 2uz> input{value::reference(ref), value::i32(bits)};
        // [canary][complete output carrier][canary] end
        // [safe ] byte+1 plus sizeof(value) stays inside this owning array.
        ::std::array<::std::byte, sizeof(value) + 2uz> output{};
        output.fill(::std::byte{0xa5u});
        auto const before{output};
        auto const result{fixture::llvm_jit_gc_aggregate_fixed_bridge<5u, 2uz>(
            reinterpret_cast<::std::uintptr_t>(::std::addressof(module)), 0u, index,
            reinterpret_cast<::std::uintptr_t>(input.data()),
            reinterpret_cast<::std::uintptr_t>(output.data() + 1uz))};
        SET32_CHECK(output.front() == ::std::byte{0xa5u} && output.back() == ::std::byte{0xa5u});
        SET32_CHECK(result == static_cast<::std::uint32_t>(status::ok) || output == before);
        return result;
    }
    void equivalent(gc::wasm_module_storage_t& module, reference ref, ::std::uint32_t index,
        ::std::uint32_t bits, status expected, ::std::uint32_t readback = 0u)
    {
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
        auto const old_boundaries{boundaries};
#endif
        bool const preserves_live_field{expected == status::immutable_field || expected == status::out_of_bounds};
        auto const protected_index{expected == status::immutable_field ? index : 0u};
        value before_failure{};
        if(preserves_live_field)
        { SET32_CHECK(module.gc_store->struct_get(ref, protected_index, false, before_failure) == status::ok); }
        auto const baseline{generic(module, ref, index, bits)};
        SET32_CHECK(baseline == static_cast<::std::uint32_t>(expected));
        if(expected == status::ok)
        {
            value out{}; SET32_CHECK(module.gc_store->struct_get(ref, index, false, out) == status::ok);
            SET32_CHECK(out.as<::std::uint32_t>() == readback);
            // Change the actual value first; otherwise a no-op candidate would
            // appear equivalent merely because the baseline already stored it.
            SET32_CHECK(module.gc_store->struct_set(ref, index, value::i32(~bits)) == status::ok);
        }
        SET32_CHECK(scalar(module, ref, index, bits) == baseline);
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
        // Observe the same actual false-boundary call from both helpers.
        // This does not invent a managed-page capability or a VM admission.
        SET32_CHECK(boundaries == old_boundaries + 2uz);
#endif
        if(expected == status::ok)
        {
            value out{}; SET32_CHECK(module.gc_store->struct_get(ref, index, false, out) == status::ok);
            SET32_CHECK(out.as<::std::uint32_t>() == readback);
        }
        if(preserves_live_field)
        {
            value after_failure{};
            SET32_CHECK(module.gc_store->struct_get(ref, protected_index, false, after_failure) == status::ok);
            SET32_CHECK(after_failure.as<::std::uint32_t>() == before_failure.as<::std::uint32_t>());
        }
    }
}

// Dynamic five-register candidate and five-argument buffer baseline are both real
// noinline compiler inputs. Keeper must inspect the finalized O3 instructions.
extern "C" [[gnu::noinline]] ::std::uintptr_t uwvm_test_gc_struct_set32_raw(
    ::std::uintptr_t module, ::std::uintptr_t kind, ::std::uintptr_t payload,
    ::std::uintptr_t field, ::std::uintptr_t bits) noexcept
{ return fixture::uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1(module, kind, payload, field, bits); }
extern "C" [[gnu::noinline]] ::std::uint_least32_t uwvm_test_gc_struct_set32_buffer(
    ::std::uintptr_t module, ::std::uint_least32_t type, ::std::uint_least32_t field,
    ::std::uintptr_t input, ::std::uintptr_t output) noexcept
{ return fixture::llvm_jit_gc_aggregate_fixed_bridge<5u, 2uz>(module, type, field, input, output); }

int main()
{
    using abi = ::std::uintptr_t(*)(::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t,
        ::std::uintptr_t, ::std::uintptr_t) noexcept;
    static_assert(::std::is_same_v<decltype(&fixture::uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1), abi>);
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    ::uwvm2::runtime::gc::managed_page_boundary_callback.store(observe_boundary, ::std::memory_order_release);
#endif
    gc::wasm_module_storage_t first{}, receiver{}; initialize(first); initialize(receiver);
    auto const base{create(first, 0u)}; auto const child{create(first, 1u)};
    for(auto ref : {base, child}) for(auto bits : ::std::array<::std::uint32_t, 9uz>{0u, 0xffffffffu,
        0x80000000u, 0x7f800000u, 0xff800000u, 0x7fc12345u, 0xffc54321u, 0x7f812345u, 0xff812345u})
    {
        equivalent(first, ref, 0u, bits, status::ok, bits);
        equivalent(first, ref, 1u, bits, status::ok, bits);
        equivalent(first, ref, 2u, bits, status::ok, bits & 0xffu);
        equivalent(first, ref, 3u, bits, status::ok, bits & 0xffffu);
    }
#if UINTPTR_MAX > UINT32_MAX
    auto const module_address{reinterpret_cast<::std::uintptr_t>(::std::addressof(first))};
    auto const kind{static_cast<::std::uintptr_t>(base.kind)};
    auto const payload{reinterpret_cast<::std::uintptr_t>(base.storage.ptr)};
    auto const too_large{static_cast<::std::uintptr_t>(UINT32_MAX) + 1u};
    auto const invalid{static_cast<::std::uintptr_t>(status::invalid_value)};
    // Host-only ABI misuse: no truncated high bit may reach an object access.
    SET32_CHECK(fixture::uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1(module_address, too_large, payload, 0u, 0u) == invalid);
    SET32_CHECK(fixture::uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1(module_address, kind, payload, too_large, 0u) == invalid);
    SET32_CHECK(fixture::uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1(module_address, kind, payload, 0u, too_large) == invalid);
#endif
    equivalent(first, base, 4u, 123u, status::immutable_field);
    equivalent(first, base, 5u, 123u, status::out_of_bounds);
    equivalent(first, child, UINT32_MAX, 123u, status::out_of_bounds);
    reference null{base}; null.kind = g::wasm_ref_kind::wasm_null;
    equivalent(first, null, 0u, 123u, status::null_reference);
    reference empty{base}; empty.storage.ptr = nullptr;
    equivalent(first, empty, 0u, 123u, status::invalid_reference);
    reference wrong{base}; wrong.kind = g::wasm_ref_kind::wasm_array;
    equivalent(first, wrong, 0u, 123u, status::invalid_reference);
    reference array{}; SET32_CHECK(first.gc_store->array_new_default(2u, 0uz, array) == status::ok);
    array.kind = g::wasm_ref_kind::wasm_struct;
    equivalent(first, array, 0u, 123u, status::invalid_reference);
    SET32_CHECK(fixture::uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1(0u, 0u, 0u, 0u, 0u) ==
        static_cast<::std::uint32_t>(status::invalid_value));
    gc::wasm_module_storage_t missing{};
    SET32_CHECK(scalar(missing, base, 0u, 123u) == static_cast<::std::uint32_t>(status::invalid_store));
    // Actual canonical-subtype object belongs to the first store. Both paths
    // must retain that owner when the receiving module performs a mutable set.
    equivalent(receiver, child, 1u, 0x7f812345u, status::ok, 0x7f812345u);
    ::std::weak_ptr<gc::gc_object_store> source_owner{first.gc_store}; first.gc_store.reset();
    SET32_CHECK(!source_owner.expired());
    equivalent(receiver, child, 2u, 0xabcdef12u, status::ok, 0x12u);
    // Stress the retained mutable lock through the real foreign-owner route.
    // No thread performs a component sweep or accesses a reclaimed token.
    SET32_CHECK(scalar(receiver, child, 0u, 0u) == static_cast<::std::uint32_t>(status::ok));
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    // The synchronous boundary observer has completed its equivalence test.
    // Do not expose its deliberately single-thread counter to worker threads.
    ::uwvm2::runtime::gc::managed_page_boundary_callback.store(nullptr, ::std::memory_order_release);
#endif
    ::std::atomic_bool failed{};
    ::std::array<::std::thread, 4uz> writers{};
    for(::std::size_t thread_index{}; thread_index != writers.size(); ++thread_index)
    {
        writers[thread_index] = ::std::thread{[&, thread_index]
        {
            for(::std::uint32_t index{}; index != 512u; ++index)
            {
                auto const bits{static_cast<::std::uint32_t>(thread_index << 16u) | index};
                value out{};
                if(scalar(receiver, child, 0u, bits) != static_cast<::std::uint32_t>(status::ok) ||
                   receiver.gc_store->struct_get(child, 0uz, false, out) != status::ok ||
                   (out.as<::std::uint32_t>() >> 16u) > 3u || (out.as<::std::uint32_t>() & 0xffffu) >= 512u)
                { failed.store(true, ::std::memory_order_relaxed); }
            }
        }};
    }
    for(auto& writer : writers) { writer.join(); }
    SET32_CHECK(!failed.load(::std::memory_order_relaxed));
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    ::uwvm2::runtime::gc::managed_page_boundary_callback.store(observe_boundary, ::std::memory_order_release);
#endif
    // No native readers/calls overlap this explicit component-only exclusive
    // sweep. Both canonical stores remain strongly pinned throughout it.
    ::std::array<::std::shared_ptr<gc::gc_object_store>, 2uz> cohort{receiver.gc_store, source_owner.lock()};
    ::std::size_t reclaimed{};
    SET32_CHECK(gc::gc_object_store::collect_exclusive_aggregate_domain(cohort.data(), cohort.size(),
        ::std::addressof(child), 1uz, reclaimed) == status::ok);
    SET32_CHECK(reclaimed == 2uz);
    equivalent(receiver, base, 0u, 123u, status::invalid_reference);
    equivalent(receiver, child, 3u, 0xabcd9876u, status::ok, 0x9876u);
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    ::uwvm2::runtime::gc::managed_page_boundary_callback.store(nullptr, ::std::memory_order_release);
#endif
    ::fast_io::io::println("PASS native struct.set32 checks=", checks,
        " reclaimed=", reclaimed, " whole_vm=false performance_qualified=false");
}

#include <uwvm2/utils/macro/pop_macros.h>

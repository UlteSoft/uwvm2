// Native equivalence and ownership proof of the exact extracted candidate.
// Includes the actual selected module/store layout; no mock VM/LLVM module.
#include <uwvm2/uwvm/runtime/storage/wasm_module.h>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <thread>
#include <fast_io.h>
namespace bridge_fixture
{
#include "native_gc_aggregate_bridge.h"
}

namespace storage = ::uwvm2::uwvm::runtime::storage;
namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
namespace global = ::uwvm2::object::global;
using value = storage::gc_object_value;
using reference = storage::gc_reference;
using status = storage::gc_object_status;

namespace
{
    ::std::size_t checks{}, collections{}, reclaimed{};
    void require(bool condition, char const* label) noexcept
    {
        ++checks;
        if(!condition)
        {
            ::fast_io::io::perrln("FAIL GC scalar32 bridge: ", ::fast_io::mnp::os_c_str(label));
            ::fast_io::fast_terminate();
        }
    }
    type::field_type field(type::value_kind kind, bool mutable_,
        type::packed_kind packed = type::packed_kind::none)
    {
        type::field_type result{};
        result.storage.value.kind = kind;
        result.storage.packed = packed;
        result.mutable_ = mutable_;
        return result;
    }
    type::recursive_type_section types()
    {
        type::recursive_type_section result{};
        type::recursive_group group{};
        type::sub_type structure{};
        structure.kind = type::composite_kind::struct_;
        structure.fields.push_back(field(type::value_kind::i32, true));
        structure.fields.push_back(field(type::value_kind::i32, false));
        structure.fields.push_back(field(type::value_kind::i32, true, type::packed_kind::i8));
        structure.fields.push_back(field(type::value_kind::i32, false, type::packed_kind::i16));
        structure.fields.push_back(field(type::value_kind::f32, true));
        structure.fields.push_back(field(type::value_kind::f32, false));
        group.types.push_back(::std::move(structure));
        type::sub_type array{};
        array.kind = type::composite_kind::array;
        array.fields.push_back(field(type::value_kind::i32, true));
        group.types.push_back(::std::move(array));
        result.type_count = 2u;
        result.groups.push_back(::std::move(group));
        return result;
    }
    void initialize(storage::wasm_module_storage_t& module)
    {
        module.gc_lease_roots = ::std::make_shared<storage::gc_lease_owner>();
        auto declarations{types()};
        module.gc_store = ::std::make_shared<storage::gc_object_store>(declarations, module.gc_lease_roots);
        require(module.gc_store->valid(), "actual recursive type/store initialization");
    }
    reference make_struct(storage::wasm_module_storage_t& module)
    {
        ::std::array<value, 6uz> fields{value::i32(0xabcdef01u), value::i32(0x12345678u),
            value::i32(0xffu), value::i32(0x8001u), value::from(::std::uint32_t{0x80000000u}),
            value::from(::std::uint32_t{0x7fc12345u})};
        reference result{};
        require(module.gc_store->struct_new(0u, fields.data(), fields.size(), result) == status::ok,
            "actual checked struct allocation");
        return result;
    }

    template<bool Signed>
    ::std::uint64_t scalar(::std::uintptr_t module, reference ref, ::std::uint32_t field_index) noexcept
    {
        return bridge_fixture::llvm_jit_gc_struct_get32_bridge<Signed>(module,
            static_cast<::std::uint32_t>(ref.kind), reinterpret_cast<::std::uintptr_t>(ref.storage.ptr), field_index);
    }
    template<bool Signed>
    void equivalent(storage::wasm_module_storage_t& module, reference ref,
        ::std::uint32_t field_index, status expected, ::std::uint32_t expected_bits = 0u)
    {
        auto const module_address{reinterpret_cast<::std::uintptr_t>(::std::addressof(module))};
        // [canary][one complete output slot][canary] end
        // [safe                                   ] form an unaligned address
        // exactly one byte into this owned array; memcpy alone accesses it.
        ::std::array<::std::byte, sizeof(value) + 2uz> output{};
        output.fill(::std::byte{0xa5u});
        auto const sentinel{output};
        auto const input{value::reference(ref)};
        auto const generic{bridge_fixture::llvm_jit_gc_aggregate_fixed_bridge<Signed ? 3u : 2u, 1uz>(
            module_address, 0u, field_index, reinterpret_cast<::std::uintptr_t>(::std::addressof(input)),
            reinterpret_cast<::std::uintptr_t>(output.data() + 1uz))};
        auto const actual{scalar<Signed>(module_address, ref, field_index)};
        require(generic == static_cast<::std::uint32_t>(expected) && actual >> 32u == generic,
            "scalar and fixed byte-buffer bridges report identical checked status");
        if(expected == status::ok)
        {
            value result{};
            // [one complete slot within the owned canary array] end
            // [safe                                         ] copy exactly
            // 16 bytes from byte 1, preserving both adjacent canary bytes.
            ::std::memcpy(::std::addressof(result), output.data() + 1uz, sizeof(result));
            require(result.as<::std::uint32_t>() == expected_bits &&
                static_cast<::std::uint32_t>(actual) == expected_bits,
                "exact scalar bits include signed packed results, NaN payload and negative zero");
            require(output.front() == ::std::byte{0xa5u} && output.back() == ::std::byte{0xa5u},
                "baseline complete output preserves its exact bounds");
        }
        else
        {
            require(actual == static_cast<::std::uint64_t>(expected) << 32u && output == sentinel,
                "failure returns only status and baseline writes no success bytes");
        }
    }
}

// Actual dynamic integer-only ABI entry points for assembly inspection. The
// scalar entry has no native input/output-buffer pointer and returns raw bits.
extern "C" [[gnu::noinline]] ::std::uint64_t gc_scalar32_unsigned(
    ::std::uintptr_t module, ::std::uint32_t kind, ::std::uintptr_t payload, ::std::uint32_t index) noexcept
{ return bridge_fixture::llvm_jit_gc_struct_get32_bridge<false>(module, kind, payload, index); }
extern "C" [[gnu::noinline]] ::std::uint64_t gc_scalar32_signed(
    ::std::uintptr_t module, ::std::uint32_t kind, ::std::uintptr_t payload, ::std::uint32_t index) noexcept
{ return bridge_fixture::llvm_jit_gc_struct_get32_bridge<true>(module, kind, payload, index); }
extern "C" [[gnu::noinline]] ::std::uint_least32_t gc_fixed_buffer_struct_get(
    ::std::uintptr_t module, ::std::uint_least32_t type_index, ::std::uint_least32_t field_index,
    ::std::uintptr_t input, ::std::uintptr_t output) noexcept
{ return bridge_fixture::llvm_jit_gc_aggregate_fixed_bridge<2u, 1uz>(module, type_index, field_index, input, output); }

int main()
{
    storage::wasm_module_storage_t first{}, receiver{};
    initialize(first); initialize(receiver);
    auto const structure{make_struct(first)};
    for(bool signed_ : {false, true})
    {
        auto const check{[&](::std::uint32_t index, ::std::uint32_t bits)
        {
            if(signed_) { equivalent<true>(first, structure, index, status::ok, bits); }
            else { equivalent<false>(first, structure, index, status::ok, bits); }
        }};
        check(0u, 0xabcdef01u); check(1u, 0x12345678u);
        check(2u, signed_ ? 0xffffffffu : 0xffu);
        check(3u, signed_ ? 0xffff8001u : 0x8001u);
        check(4u, 0x80000000u); check(5u, 0x7fc12345u);
    }
    for(auto const bits : ::std::array<::std::uint32_t, 8uz>{0u, 0x80000000u, 0x7f800000u,
        0xff800000u, 0x7fc12345u, 0xffc54321u, 0x7f812345u, 0xff812345u})
    {
        require(first.gc_store->struct_set(structure, 4uz, value::from(bits)) == status::ok,
            "actual mutable f32 stores retain raw bits");
        equivalent<false>(first, structure, 4u, status::ok, bits);
        equivalent<true>(first, structure, 4u, status::ok, bits);
    }
    equivalent<false>(first, structure, 6u, status::out_of_bounds);
    equivalent<true>(first, structure, UINT32_MAX, status::out_of_bounds);
    reference null{structure}; null.kind = global::wasm_ref_kind::wasm_null;
    equivalent<false>(first, null, 0u, status::null_reference);
    null.storage.ptr = nullptr;
    equivalent<true>(first, null, 0u, status::null_reference);
    reference empty{structure}; empty.storage.ptr = nullptr;
    equivalent<false>(first, empty, 0u, status::invalid_reference);
    reference forged{structure};
    // [invalid opaque token] no fixture or bridge dereferences this payload.
    // [safe                ] membership rejects an unpublished token value.
    forged.storage.ptr = reinterpret_cast<void*>((::std::numeric_limits<::std::uintptr_t>::max)() - 5u);
    equivalent<false>(first, forged, 0u, status::invalid_reference);
    for(auto const kind : ::std::array<unsigned, 8uz>{1u, 2u, 3u, 4u, 6u, 7u, 8u, 99u})
    {
        reference wrong{structure}; wrong.kind = static_cast<global::wasm_ref_kind>(kind);
        equivalent<false>(first, wrong, 0u, status::invalid_reference);
    }
    reference array{};
    require(first.gc_store->array_new_default(1u, 2uz, array) == status::ok, "actual array allocation");
    array.kind = global::wasm_ref_kind::wasm_struct;
    equivalent<false>(first, array, 0u, status::invalid_reference);
    auto const invalid_module{static_cast<::std::uint64_t>(status::invalid_value) << 32u};
    require(scalar<false>(0u, structure, 0u) == invalid_module, "null native module rejected before access");
    storage::wasm_module_storage_t no_store{};
    require(scalar<true>(reinterpret_cast<::std::uintptr_t>(::std::addressof(no_store)), structure, 0u) ==
        static_cast<::std::uint64_t>(status::invalid_store) << 32u, "missing native store rejected");

    // Borrowing a foreign object must create/retain the same checked owner
    // lease as the ordinary bridge, never substitute a raw foreign pointer.
    equivalent<false>(receiver, structure, 1u, status::ok, 0x12345678u);
    ::std::weak_ptr<storage::gc_object_store> source_owner{first.gc_store};
    first.gc_store.reset();
    require(!source_owner.expired(), "actual receiving module retains a foreign owner lease");
    equivalent<true>(receiver, structure, 3u, status::ok, 0xffff8001u);
    auto const local{make_struct(receiver)};
    auto const receiver_address{reinterpret_cast<::std::uintptr_t>(::std::addressof(receiver))};
    ::std::atomic<bool> failed{};
    ::std::array<::std::thread, 4uz> readers{};
    for(::std::size_t index{}; index != readers.size(); ++index)
    {
        readers[index] = ::std::thread{[&]
        {
            for(::std::size_t i{}; i != 2000uz; ++i)
            {
                auto const immutable{scalar<false>(receiver_address, local, 1u)};
                auto const mutable_{scalar<false>(receiver_address, local, 0u)};
                if(immutable != 0x12345678u || mutable_ >> 32u != 0u ||
                   receiver.gc_store->struct_set(local, 0uz, value::i32(static_cast<::std::uint32_t>(i))) != status::ok)
                { failed.store(true, ::std::memory_order_relaxed); }
            }
        }};
    }
    for(auto& thread : readers) { thread.join(); }
    require(!failed.load(), "actual mutable synchronization and immutable reads remain valid across native threads");
    // Every native reader has joined. Both canonical owners stay strongly
    // pinned, including the foreign store discovered through its weak owner.
    // This explicit exclusive component collection is not automatic VM GC.
    ::std::array<::std::shared_ptr<storage::gc_object_store>, 2uz> cohort{receiver.gc_store, source_owner.lock()};
    auto const source_raw{cohort[1].get()};
    ::std::size_t freed{};
    require(storage::gc_object_store::collect_exclusive_aggregate_domain(cohort.data(), cohort.size(),
        ::std::addressof(local), 1uz, freed) == status::ok, "closed real aggregate collection succeeds");
    ++collections; reclaimed += freed;
    require(freed == 2uz, "unrooted foreign structure and array are actually reclaimed");
    equivalent<false>(receiver, structure, 1u, status::invalid_reference);
    require(cohort[1].get() == source_raw, "native canonical pin remains stable through stale-token rejection");
    // Aggregate collection prunes object-owned field leases. The receiving
    // module's separate conservative lease still protects globals/tables and
    // host roots that this native component collector does not enumerate.
    require(!source_owner.expired(), "module-owned foreign lease survives aggregate-only collection");
    auto const fresh{make_struct(receiver)};
    require(fresh.storage.ptr != structure.storage.ptr, "fresh object token cannot reuse a reclaimed identity");
    equivalent<false>(receiver, fresh, 1u, status::ok, 0x12345678u);
    require(storage::gc_object_store::collect_exclusive_aggregate_domain(cohort.data(), cohort.size(),
        nullptr, 0uz, freed) == status::ok && freed == 2uz, "remaining actual structures are reclaimed");
    ++collections; reclaimed += freed;
    equivalent<true>(receiver, local, 0u, status::invalid_reference);
    // [all guests/readers joined][closed canonical cohort still pinned]
    // [safe                                                      ] the fixture
    // has no globals/tables/host roots. Reset this module-owned lease only
    // after its final foreign access and after every aggregate was reclaimed.
    cohort[1].reset();
    require(!source_owner.expired(), "receiving module retains its independent conservative lease");
    receiver.gc_lease_roots.reset();
    require(source_owner.expired(), "receiving module reset releases the foreign owner");
    ::fast_io::io::println("PASS GC scalar32 native equivalence: checks=", checks,
        " collections=", collections, " reclaimed=", reclaimed,
        "; real module/store; raw i32/f32/packed bits; owners; stale/forged references; native threads; wholeVM=false");
}

// Core 3 aggregate reference execution. Included inside the JIT translator namespace.
// The generated ABI passes only integer addresses and scalar indices. The module owns
// the GC store, and the JIT never embeds an object-store pointer in cached machine code.

#include "single_func_gc_status_dispatch.h"

// The generic ABI stays available for large/variable aggregate inputs. The
// compiler selects fixed opcode/count instances only from validated metadata.
// Their complete fixed-size memcpy overwrites every input byte, allowing O3
// to remove default initialization and the opcode switch without raw-storage
// lifetime casts or a changed gc_object_value default-value contract.
template<::std::uint_least32_t FixedOpcode = 0xffff'ffffu, ::std::size_t FixedInputs = SIZE_MAX>
[[nodiscard]] inline ::std::uint_least32_t llvm_jit_gc_aggregate_bridge(
    ::std::uintptr_t module_address, ::std::uint_least32_t opcode,
    ::std::uint_least32_t first, ::std::uint_least32_t second,
    ::std::uintptr_t input_address, ::std::size_t input_count,
    ::std::uintptr_t output_address) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    using status = storage::gc_object_status;
    using value = storage::gc_object_value;
    using reference = storage::gc_reference;
    constexpr bool fixed{FixedInputs != SIZE_MAX};
    static_assert(fixed == (FixedOpcode != 0xffff'ffffu));
    static_assert(!fixed || (FixedOpcode <= 19u && FixedInputs <= 8uz));
    if constexpr(fixed)
    {
        // Retain the ABI check for host misuse: a fixed instance must never
        // read the advertised extent of another opcode/count specialization.
        if(opcode != FixedOpcode || input_count != FixedInputs)
        { return static_cast<::std::uint_least32_t>(status::invalid_value); }
        opcode = FixedOpcode;
        input_count = FixedInputs;
    }
    if(module_address == 0u || output_address == 0u ||
       (input_count != 0uz && input_address == 0u) ||
       input_count > (::std::numeric_limits<::std::size_t>::max)() / sizeof(value))
    { return static_cast<::std::uint_least32_t>(status::invalid_value); }
    auto const* module{reinterpret_cast<storage::wasm_module_storage_t const*>(module_address)};
    auto* store{module->gc_store.get()};
    if(store == nullptr || !store->valid()) { return static_cast<::std::uint_least32_t>(status::invalid_store); }
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    // All unsupported aggregate operations keep the original ABI and store
    // checks. Publish any outstanding page references BEFORE those lookups.
    ::uwvm2::runtime::gc::managed_page_boundary(false);
#endif
    if constexpr(!fixed)
    {
        if(opcode == 0u && input_count > 8uz && store->generated_struct_input_has_complete_carriers(first, input_count))
        {
            // Avoid a new[]/delete[] temporary per wide aggregate allocation.
            // Generated input is bytes, never cast to a fictitious live value array.
            reference reference_result{};
            auto const outcome{store->struct_new_from_generated_carrier_bytes(first,
                reinterpret_cast<::std::byte const*>(input_address), input_count, reference_result)};
            if(outcome == status::ok)
            {
                auto const result{value::reference(reference_result)};
                ::std::memcpy(reinterpret_cast<void*>(output_address), ::std::addressof(result), sizeof(result));
            }
            return static_cast<::std::uint_least32_t>(outcome);
        }
    }
    constexpr auto small_count{fixed ? (FixedInputs == 0uz ? 1uz : FixedInputs) : 8uz};
    value small[small_count]{};
    ::std::unique_ptr<value[]> large{};
    auto* inputs{small};
    if constexpr(!fixed)
    {
        if(input_count > 8uz)
        {
            large.reset(new(::std::nothrow) value[input_count]{});
            if(!large) { return static_cast<::std::uint_least32_t>(status::out_of_memory); }
            // [complete input_count-element owning allocation] replaces the
            // local buffer only after the native allocation succeeds.
            // ^^ inputs borrows large until this synchronous call returns.
            inputs = large.get();
        }
    }
    auto const* bytes{reinterpret_cast<::std::byte const*>(input_address)};
    for(::std::size_t index{}; index != input_count; ++index)
    {
        // [input_count slots of 16 bytes] end
        // [safe                         ] index < count and checked multiplication bound
        //          ^^ bytes + index*16 names exactly one generated, initialized slot.
        ::std::memcpy(::std::addressof(inputs[index]), bytes + index * sizeof(value), sizeof(value));
    }
    value result{};
    reference ref{};
    status outcome{status::invalid_value};
    auto const as_ref{[&](::std::size_t index) noexcept { return inputs[index].template as<reference>(); }};
    auto const as_i32{[&](::std::size_t index) noexcept { return inputs[index].template as<::std::uint32_t>(); }};
    switch(opcode)
    {
        case 0u: outcome = store->struct_new(first, inputs, input_count, ref); break;
        case 1u: if(input_count == 0uz) { outcome = store->struct_new_default(first, ref); } break;
        case 2u: case 3u: case 4u:
            if(input_count == 1uz) { outcome = store->struct_get(as_ref(0uz), second, opcode == 3u, result); }
            break;
        case 5u:
            if(input_count == 2uz) { outcome = store->struct_set(as_ref(0uz), second, inputs[1]); }
            break;
        case 6u:
            if(input_count == 2uz) { outcome = store->array_new(first, inputs[0], as_i32(1uz), ref); }
            break;
        case 7u:
            if(input_count == 1uz) { outcome = store->array_new_default(first, as_i32(0uz), ref); }
            break;
        case 8u:
            if(input_count == second) { outcome = store->array_new_fixed(first, inputs, input_count, ref); }
            break;
        case 9u:
            if(input_count == 2uz)
            {
                outcome = storage::uwvm2_gc_array_new_data(store, module, first, second,
                    as_i32(0uz), as_i32(1uz), ::std::addressof(ref));
            }
            break;
        case 10u:
            if(input_count == 2uz)
            {
                outcome = storage::uwvm2_gc_array_new_elem(store, module, first, second,
                    as_i32(0uz), as_i32(1uz), ::std::addressof(ref));
            }
            break;
        case 11u: case 12u: case 13u:
            if(input_count == 2uz) { outcome = store->array_get(as_ref(0uz), as_i32(1uz), opcode == 12u, result); }
            break;
        case 14u:
            if(input_count == 3uz) { outcome = store->array_set(as_ref(0uz), as_i32(1uz), inputs[2]); }
            break;
        case 15u:
            if(input_count == 1uz)
            {
                ::std::size_t length{};
                outcome = store->array_length(as_ref(0uz), length);
                if(outcome == status::ok)
                {
                    if(length > (::std::numeric_limits<::std::uint32_t>::max)()) { outcome = status::size_overflow; }
                    else { result = value::i32(static_cast<::std::uint32_t>(length)); }
                }
            }
            break;
        case 16u:
            if(input_count == 4uz) { outcome = store->array_fill(as_ref(0uz), as_i32(1uz), inputs[2], as_i32(3uz)); }
            break;
        case 17u:
            if(input_count == 5uz)
            { outcome = store->array_copy(as_ref(0uz), as_i32(1uz), as_ref(2uz), as_i32(3uz), as_i32(4uz)); }
            break;
        case 18u:
            if(input_count == 4uz)
            {
                auto const array{as_ref(0uz)};
                outcome = storage::uwvm2_gc_array_init_data(store, module, first, second,
                    ::std::addressof(array), as_i32(1uz), as_i32(2uz), as_i32(3uz));
            }
            break;
        case 19u:
            if(input_count == 4uz)
            {
                auto const array{as_ref(0uz)};
                outcome = storage::uwvm2_gc_array_init_elem(store, module, first, second,
                    ::std::addressof(array), as_i32(1uz), as_i32(2uz), as_i32(3uz));
            }
            break;
        default: break;
    }
    if(outcome == status::ok)
    {
        if(opcode == 0u || opcode == 1u || (opcode >= 6u && opcode <= 10u))
        { result = value::reference(ref); }
        // [one generated 16-byte output slot] is live until this call returns.
        // [safe                             ] memcpy writes its exact extent; no guest address is involved.
        ::std::memcpy(reinterpret_cast<void*>(output_address), ::std::addressof(result), sizeof(result));
    }
    return static_cast<::std::uint_least32_t>(outcome);
}

// A validated fixed operation has no dynamic opcode/count to transmit. This
// five-argument native C ABI avoids two constant argument moves and, on SysV
// x86-64, the seventh argument's stack slot. The compiler must use the matching
// FunctionType below; this address is never exposed to guest linear memory.
template<::std::uint_least32_t Opcode, ::std::size_t Inputs>
[[nodiscard]] inline ::std::uint_least32_t llvm_jit_gc_aggregate_fixed_bridge(
    ::std::uintptr_t module_address, ::std::uint_least32_t first,
    ::std::uint_least32_t second, ::std::uintptr_t input_address,
    ::std::uintptr_t output_address) noexcept
{
    static_assert(Opcode <= 19u && Inputs <= 8uz);
    return llvm_jit_gc_aggregate_bridge<Opcode, Inputs>(module_address, Opcode,
        first, second, input_address, Inputs, output_address);
}


#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_SET_RAW_CARRIER_ABI) && UWVM_EXPERIMENTAL_GC_ARRAY_SET_RAW_CARRIER_ABI == 1
// Private generated-only leaf: six native scalar operands replace the escaped
// three-slot input scratch and unused output scratch of fixed opcode14.
// The four carrier i64 values are opaque native byte chunks, never decoded
// object addresses. Index and status are wide scalars with explicit IR casts,
// avoiding narrow argument/return extension requirements on PPC64 and other
// native ABIs. The original array_set checks and status codes are unchanged.
// Loading and memcpying that same native chunk preserves all16B on either
// endian; only the original as<gc_reference>() may decode the reference.
[[nodiscard]] inline ::std::uint64_t llvm_jit_gc_array_set_raw_carrier_bridge(
    ::std::uintptr_t module_address, ::std::uint64_t destination_low,
    ::std::uint64_t destination_high, ::std::uint64_t index,
    ::std::uint64_t value_low, ::std::uint64_t value_high) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    using value = storage::gc_object_value;
    static_assert(sizeof(value) == 2uz * sizeof(::std::uint64_t));
    if(module_address == 0u)
    { return static_cast<::std::uint64_t>(storage::gc_object_status::invalid_value); }
    auto const* module{reinterpret_cast<storage::wasm_module_storage_t const*>(module_address)};
    auto* store{module->gc_store.get()};
    if(store == nullptr || !store->valid())
    { return static_cast<::std::uint64_t>(storage::gc_object_status::invalid_store); }
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    ::uwvm2::runtime::gc::managed_page_boundary(false);
#endif
    value destination{};
    value source{};
    ::std::memcpy(destination.bits.data(), ::std::addressof(destination_low), sizeof(destination_low));
    ::std::memcpy(destination.bits.data() + sizeof(destination_low),
        ::std::addressof(destination_high), sizeof(destination_high));
    ::std::memcpy(source.bits.data(), ::std::addressof(value_low), sizeof(value_low));
    ::std::memcpy(source.bits.data() + sizeof(value_low), ::std::addressof(value_high), sizeof(value_high));
    // Identical checked store path: destination membership/bounds/mutability,
    // source kind/subtyping/lifetime, foreign owner leases, mutation lock,
    // packing and failed-write status/order all remain in array_set.
    // Inline only this generated native bridge callsite. The checked store
    // remains the authority for membership, subtyping, lifetime and locking.
    // This lets the host compiler eliminate duplicate validity checks and
    // redundant argument shuffles without changing the portable byte carrier.
#if __has_cpp_attribute(clang::always_inline)
    [[clang::always_inline]]
#endif
    return static_cast<::std::uint64_t>(store->array_set(
        destination.template as<storage::gc_reference>(), index, source));
}
#endif

#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
template<::std::uint_least32_t Opcode, ::std::size_t Inputs>
[[nodiscard]] inline ::std::uint_least32_t llvm_jit_gc_page_new32_bridge(
    ::std::uintptr_t module_address, ::std::uint_least32_t first,
    ::std::uint_least32_t second, ::std::uintptr_t input_address,
    ::std::uintptr_t output_address) noexcept
{
    static_assert((Opcode == 0u && Inputs == 1uz) || (Opcode == 1u && Inputs == 0uz));
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    using value = storage::gc_object_value;
    if(module_address == 0u || output_address == 0u || (Inputs != 0uz && input_address == 0u))
    { return static_cast<::std::uint_least32_t>(storage::gc_object_status::invalid_value); }
    // Actual validator lowered roots BEFORE input pop; source-buffer carriers
    // remain live here. Poll first, then get a freshly revalidated capability.
    ::uwvm2::runtime::gc::poll_before_managed_aggregate_allocation(module_address);
    if(auto* page{::uwvm2::runtime::gc::borrow_actual_managed_page(module_address)})
    {
        ::std::uint32_t bits{};
        if constexpr(Inputs != 0uz)
        {
            value input{};
            ::std::memcpy(&input, reinterpret_cast<void const*>(input_address), sizeof(input));
            bits = input.template as<::std::uint32_t>();
        }
        storage::gc_reference reference{};
        auto outcome{page->try_new32(first, bits, reference)};
        if(outcome == storage::numeric_page_status::ok)
        {
            auto result{value::reference(reference)};
            ::std::memcpy(reinterpret_cast<void*>(output_address), &result, sizeof(result));
            return static_cast<::std::uint_least32_t>(storage::gc_object_status::ok);
        }
        // Capacity refills are handled by the page. Unsupported type, OOM,
        // backing budget, exhausted token/epoch and pause all fail closed to
        // the checked original route AFTER complete publication/borrow drain.
        ::uwvm2::runtime::gc::managed_page_boundary(false);
    }
    return llvm_jit_gc_aggregate_fixed_bridge<Opcode, Inputs>(module_address, first, second, input_address, output_address);
}
#endif

// Allocation-only roots-enabled helpers. No ordinary/fixed field read/write
// helper changes symbol, ABI or gains a collector branch. The caller has already
// published locals/prefix/inputs; polling occurs before allocator reservation.
template<::std::uint_least32_t Opcode, ::std::size_t Inputs>
[[nodiscard]] inline ::std::uint_least32_t llvm_jit_gc_managed_aggregate_fixed_bridge(
    ::std::uintptr_t module_address, ::std::uint_least32_t first,
    ::std::uint_least32_t second, ::std::uintptr_t input_address,
    ::std::uintptr_t output_address) noexcept
{
    static_assert(Opcode == 0u || Opcode == 1u || (Opcode >= 6u && Opcode <= 10u));
    static_assert(Opcode != 6u || Inputs == 2uz);
    static_assert(Opcode != 7u || Inputs == 1uz);
#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE) && UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE == 1
    if constexpr(Opcode == 1u)
    {
        // Default constructors have no input carriers. Charge their immutable
        // native field extent through the existing bounded 1 MiB scheduling
        // budget. A charge is never a liveness or allocation-size proof.
        ::std::size_t fields{};
        if(module_address != 0u && output_address != 0u)
        {
            auto const* module{reinterpret_cast<::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const*>(module_address)};
            auto const* store{module->gc_store.get()};
            if(store != nullptr && store->valid())
            { static_cast<void>(store->field_count(first, fields)); }
        }
        if(fields > 8uz) { ::uwvm2::runtime::gc::poll_before_managed_array_allocation(module_address, fields); }
        else { ::uwvm2::runtime::gc::poll_before_managed_aggregate_allocation(module_address); }
    }
    else if constexpr(Opcode == 6u || Opcode == 7u)
    {
        // Length is the actual generated i32 carrier, not the opcode's second
        // immediate. All input carriers remain rooted/live before the poll.
        ::std::size_t length{};
        if(module_address != 0u && output_address != 0u && input_address != 0u)
        {
            // Match the original bridge's invalid-store refusal before any
            // input-carrier read. The native module address obeys that same ABI.
            auto const* module{reinterpret_cast<::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const*>(module_address)};
            auto* store{module->gc_store.get()};
            if(store == nullptr || !store->valid())
            { return static_cast<::std::uint_least32_t>(::uwvm2::uwvm::runtime::storage::gc_object_status::invalid_store); }
            ::uwvm2::uwvm::runtime::storage::gc_object_value carrier{};
            auto const* bytes{reinterpret_cast<::std::byte const*>(input_address)};
            constexpr ::std::size_t slot{Opcode == 6u ? 1uz : 0uz};
            // [original complete Inputs*16 generated buffer][length slot]
            // The fixed opcode/count contract owns this exact carrier extent.
            ::std::memcpy(::std::addressof(carrier),bytes+slot*sizeof(carrier),sizeof(carrier));
            length=carrier.template as<::std::uint32_t>();
            length=store->managed_wide_numeric_array_pressure_length(first,length);
        }
        ::uwvm2::runtime::gc::poll_before_managed_array_allocation(module_address,length);
    }
    else
#endif
    { ::uwvm2::runtime::gc::poll_before_managed_aggregate_allocation(module_address); }
    return llvm_jit_gc_aggregate_fixed_bridge<Opcode, Inputs>(module_address,
        first, second, input_address, output_address);
}
[[nodiscard]] inline ::std::uint_least32_t llvm_jit_gc_managed_aggregate_bridge(
    ::std::uintptr_t module_address, ::std::uint_least32_t opcode,
    ::std::uint_least32_t first, ::std::uint_least32_t second,
    ::std::uintptr_t input_address, ::std::size_t input_count,
    ::std::uintptr_t output_address) noexcept
{
    if(opcode != 0u && opcode != 1u && (opcode < 6u || opcode > 10u))
    { return static_cast<::std::uint_least32_t>(::uwvm2::uwvm::runtime::storage::gc_object_status::invalid_value); }
#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE) && UWVM_EXPERIMENTAL_GC_ARRAY_BYTE_PRESSURE == 1
    if((opcode == 0u || opcode == 8u) && input_count > 8uz)
    {
        // The validated generated buffer contains input_count full 16-byte
        // carriers. Wide structs and fixed arrays previously reached only the
        // 4096-object trigger: a 1024-field struct could accumulate 64 MiB.
        // Reuse the saturating carrier budget before reservation, with the
        // original published roots, cohort, admission and exception gates.
        ::uwvm2::runtime::gc::poll_before_managed_array_allocation(module_address, input_count);
    }
    else if(opcode == 1u)
    {
        ::std::size_t fields{};
        if(module_address != 0u && output_address != 0u)
        {
            auto const* module{reinterpret_cast<::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const*>(module_address)};
            auto const* store{module->gc_store.get()};
            if(store != nullptr && store->valid())
            { static_cast<void>(store->field_count(first, fields)); }
        }
        if(fields > 8uz) { ::uwvm2::runtime::gc::poll_before_managed_array_allocation(module_address, fields); }
        else { ::uwvm2::runtime::gc::poll_before_managed_aggregate_allocation(module_address); }
    }
    else if(opcode == 6u || opcode == 7u)
    {
        // Length is the actual generated i32 carrier, not the opcode's second
        // immediate. All input carriers remain rooted/live before the poll.
        ::std::size_t length{};
        if(module_address != 0u && output_address != 0u && input_address != 0u &&
           ((opcode == 6u && input_count == 2uz) || (opcode == 7u && input_count == 1uz)))
        {
            // Match the original bridge's invalid-store refusal before any
            // input-carrier read. The native module address obeys that same ABI.
            auto const* module{reinterpret_cast<::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const*>(module_address)};
            auto* store{module->gc_store.get()};
            if(store == nullptr || !store->valid())
            { return static_cast<::std::uint_least32_t>(::uwvm2::uwvm::runtime::storage::gc_object_status::invalid_store); }
            ::uwvm2::uwvm::runtime::storage::gc_object_value carrier{};
            auto const* bytes{reinterpret_cast<::std::byte const*>(input_address)};
            auto const slot{opcode == 6u ? 1uz : 0uz};
            // [original complete Inputs*16 generated buffer][length slot]
            // The fixed opcode/count contract owns this exact carrier extent.
            ::std::memcpy(::std::addressof(carrier),bytes+slot*sizeof(carrier),sizeof(carrier));
            length=carrier.template as<::std::uint32_t>();
            length=store->managed_wide_numeric_array_pressure_length(first,length);
        }
        ::uwvm2::runtime::gc::poll_before_managed_array_allocation(module_address,length);
    }
    else
#endif
    { ::uwvm2::runtime::gc::poll_before_managed_aggregate_allocation(module_address); }
    return llvm_jit_gc_aggregate_bridge<>(module_address, opcode,
        first, second, input_address, input_count, output_address);
}

// A 32-bit struct field uses a register-only integer C ABI: the upper
// result word is gc_object_status and the lower word is the exact Wasm i32/f32
// bits. No input/output native byte-buffer address crosses this bridge. Keep
// the ordinary checked store operation: immutable reads remain lock-free,
// mutable reads retain object_lock, and foreign objects retain their lease.
// https://webassembly.github.io/spec/core/exec/instructions.html#exec-struct-get
template<bool SignExtend>
[[nodiscard]] inline ::std::uint64_t llvm_jit_gc_struct_get32_bridge(
    ::std::uintptr_t module_address, ::std::uint32_t reference_kind,
    ::std::uintptr_t opaque_payload, ::std::uint32_t field_index) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    using status = storage::gc_object_status;
    static_assert(sizeof(status) == sizeof(::std::uint32_t));
    auto const packed_status{[](status value) noexcept
    { return static_cast<::std::uint64_t>(value) << 32u; }};
    if(module_address == 0u) { return packed_status(status::invalid_value); }
    // [native module object owned by the executing JIT instance] object end
    // [safe                                                 ] this address is
    // emitted/bound by the compiler; it is never a guest linear-memory value.
    // ^^ borrow exactly that live native instance for the synchronous call.
    auto const* module{reinterpret_cast<storage::wasm_module_storage_t const*>(module_address)};
    auto* store{module->gc_store.get()};
    if(store == nullptr || !store->valid()) { return packed_status(status::invalid_store); }
    storage::gc_reference reference{};
    reference.kind = static_cast<::uwvm2::object::global::wasm_ref_kind>(reference_kind);
    // [opaque non-reused object token] this is not a dereferenceable pointer.
    // [safe                         ] checked struct_get verifies kind, null,
    // membership, canonical owner and field extent before any object access.
    // ^^ preserve the complete payload value; do not infer a tag from it.
    reference.storage.ptr = reinterpret_cast<void*>(opaque_payload);
    // The checked scalar getter returns status/raw bits directly. Its mutable
    // and foreign paths keep exactly the ordinary getter's lock and lease; no
    // temporary full-carrier output buffer or native float ABI is introduced.
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    if(auto* page{::uwvm2::runtime::gc::borrow_actual_managed_page(module_address)})
    {
        if(field_index == 0u && page->is_pending(reference))
        {
            ::std::uint32_t bits{};
            if(page->read32(reference, bits) == storage::numeric_page_status::ok) { return bits; }
        }
        if(!page->is_committed_local(reference) && !page->is_pending(reference))
        { ::uwvm2::runtime::gc::managed_page_boundary(true); }
        else { ::uwvm2::runtime::gc::managed_page_boundary(false); }
    }
#endif
    return store->template struct_get32<SignExtend>(reference, field_index);
}

// A bounded native scratch lease per OS thread. Generated aggregate calls
// borrow synchronously and release before status/trap dispatch; reentry while
// borrowed falls back to the original heap path. No module/GC owner is retained.
struct llvm_jit_gc_input_scratch_t
{
    alignas(::uwvm2::uwvm::runtime::storage::gc_object_value) ::std::byte bytes[16384uz];
    bool borrowed{};
};
inline thread_local llvm_jit_gc_input_scratch_t llvm_jit_gc_input_scratch{};
[[nodiscard]] inline ::std::uintptr_t llvm_jit_gc_input_allocate_bridge(::std::size_t bytes) noexcept
{
    auto& scratch{llvm_jit_gc_input_scratch};
    if(bytes <= sizeof(scratch.bytes) && !scratch.borrowed)
    {
        scratch.borrowed = true;
        return reinterpret_cast<::std::uintptr_t>(scratch.bytes);
    }
    return reinterpret_cast<::std::uintptr_t>(::operator new(bytes, ::std::nothrow));
}
inline void llvm_jit_gc_input_free_bridge(::std::uintptr_t address) noexcept
{
    auto& scratch{llvm_jit_gc_input_scratch};
    if(address == reinterpret_cast<::std::uintptr_t>(scratch.bytes))
    { scratch.borrowed = false; return; }
    ::operator delete(reinterpret_cast<void*>(address));
}

[[nodiscard]] inline ::std::uint32_t llvm_jit_gc_ref_test_bridge(
    ::std::uintptr_t module_address, ::std::uintptr_t reference_address,
    ::std::int_least64_t heap_code, ::std::uint32_t nullable) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    if(module_address == 0u || reference_address == 0u) { return 0u; }
    auto const* module{reinterpret_cast<storage::wasm_module_storage_t const*>(module_address)};
    auto const* store{module->gc_store.get()};
    if(store == nullptr || !store->valid()) { return 0u; }
    storage::gc_reference reference{};
    // [generated reference slot, sizeof(reference)] is live for the synchronous bridge call.
    // [safe                                       ] the JIT writes its full scalar value before this exact copy.
    ::std::memcpy(::std::addressof(reference), reinterpret_cast<void const*>(reference_address), sizeof(reference));
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    ::uwvm2::runtime::gc::managed_page_boundary(false);
#endif
    return store->reference_type_matches(reference,
        {storage::gc_type::value_kind::reference, {heap_code}, nullable != 0u}) ? 1u : 0u;
}

// Exact integer ABI. A successful local immutable numeric cast may return a
// native carrier-array witness; it is never the Wasm operand or its token.
// Foreign/mutable/null successes return the non-address sentinel 1 instead.
[[nodiscard]] inline ::std::uintptr_t llvm_jit_gc_immutable_struct_cast_bridge(
    ::std::uintptr_t module_address, ::std::uint32_t reference_kind,
    ::std::uintptr_t opaque_payload, ::std::uint32_t expected_index,
    ::std::uint32_t nullable) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    if(module_address == 0u || nullable > 1u) { return 0u; }
    // [compiler-bound, execution-pinned complete native module] end
    // [safe                                                  ] guest values
    // cannot supply this native module address or type index.
    auto const* module{reinterpret_cast<storage::wasm_module_storage_t const*>(module_address)};
    auto const* store{module->gc_store.get()};
    if(store == nullptr || !store->valid()) { return 0u; }
    storage::gc_reference reference{};
    reference.kind = static_cast<::uwvm2::object::global::wasm_ref_kind>(reference_kind);
    // [opaque non-recycled key] no native object is dereferenced from this
    // value. The store proves kind/local membership/owner/type before borrow.
    reference.storage.ptr = reinterpret_cast<void*>(opaque_payload);
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    if(auto* page{::uwvm2::runtime::gc::borrow_actual_managed_page(module_address)})
    {
        if(page->is_pending(reference))
        {
            auto const values{page->cast_values(reference, expected_index)};
            if(values > 1u) { return values; }
            // Different validated target/subtype uses all original cast checks;
            // publish first. Byte-offset/BB witness consumer is unchanged.
            ::uwvm2::runtime::gc::managed_page_boundary(false);
        }
        else if(reference.kind != ::uwvm2::object::global::wasm_ref_kind::wasm_null &&
                !page->is_committed_local(reference))
        { ::uwvm2::runtime::gc::managed_page_boundary(true); }
    }
#endif
    return store->native_immutable_numeric_struct_cast_values(reference, expected_index, nullable != 0u);
}

[[nodiscard]] inline bool llvm_jit_gc_immutable_cast_witness_type(
    ::uwvm2::uwvm::runtime::storage::gc_object_store const& store,
    ::std::uint_least32_t index) noexcept
{
    namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
    type::composite_kind kind{};
    ::std::size_t count{};
    if(!store.type_kind(index, kind) || kind != type::composite_kind::struct_ ||
       !store.field_count(index, count) || count == 0uz || count > 8uz) { return false; }
    for(::std::size_t field{}; field != count; ++field)
    {
        auto const* descriptor{store.field_at(index, field)};
        if(descriptor == nullptr || descriptor->mutable_ ||
           (descriptor->storage.packed == type::packed_kind::none &&
            descriptor->storage.value.kind != type::value_kind::i32 &&
            descriptor->storage.value.kind != type::value_kind::f32)) { return false; }
    }
    return true;
}

#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
[[nodiscard]] inline ::llvm::Value* emit_runtime_local_func_llvm_jit_gc_immutable_cast_witness(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type target) noexcept
;
[[nodiscard]] inline ::llvm::Value* emit_runtime_local_func_llvm_jit_gc_immutable_cast_witness_unsealed(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type target) noexcept
#endif
#if !defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) || UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR != 1
[[nodiscard]] inline ::llvm::Value* emit_runtime_local_func_llvm_jit_gc_immutable_cast_witness(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type target) noexcept
#endif
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    using reference = storage::gc_reference;
    if(!state.valid || state.ir_builder == nullptr || state.llvm_module == nullptr ||
       state.local_func_storage_ptr == nullptr ||
       state.operand_stack.empty() || target.heap.code < 0 ||
       static_cast<::std::uint_least64_t>(target.heap.code) > UINT32_MAX) { return nullptr; }
    auto const* module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr || !module->gc_store ||
       !llvm_jit_gc_immutable_cast_witness_type(*module->gc_store,
           static_cast<::std::uint_least32_t>(target.heap.code))) { return nullptr; }
    auto& builder{*state.ir_builder};
    auto const operand{state.operand_stack.back()};
    constexpr auto bytes{sizeof(reference)};
    constexpr auto payload_offset{offsetof(reference, storage)};
    constexpr auto payload_bytes{sizeof(reference::storage)};
    constexpr auto kind_offset{offsetof(reference, kind)};
    constexpr auto kind_bytes{sizeof(reference::kind)};
    static_assert(::std::is_standard_layout_v<reference>);
    static_assert(payload_bytes == sizeof(::std::uintptr_t));
    static_assert(kind_bytes == sizeof(::std::uint32_t));
    static_assert(payload_offset <= bytes && payload_bytes <= bytes - payload_offset);
    static_assert(kind_offset <= bytes && kind_bytes <= bytes - kind_offset);
    if(operand.value == nullptr || !operand.value->getType()->isIntegerTy(
        static_cast<unsigned>(bytes * CHAR_BIT))) { return nullptr; }
    auto const& layout{state.llvm_module->getDataLayout()};
    if(state.llvm_module->getDataLayoutStr().empty() ||
       layout.getPointerSize() != sizeof(::std::uintptr_t) ||
       layout.isLittleEndian() != (::std::endian::native == ::std::endian::little)) { return nullptr; }
    constexpr bool little{::std::endian::native == ::std::endian::little};
    constexpr auto payload_shift{static_cast<unsigned>((little ? payload_offset :
        bytes - payload_offset - payload_bytes) * CHAR_BIT)};
    constexpr auto kind_shift{static_cast<unsigned>((little ? kind_offset :
        bytes - kind_offset - kind_bytes) * CHAR_BIT)};
    auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const i32{builder.getInt32Ty()};
    // [complete native Wasm carrier SSA] extract only the proved native fields;
    // padding is ignored and no opaque guest token is interpreted as a pointer.
    auto const payload{builder.CreateTrunc(builder.CreateLShr(operand.value, payload_shift), intptr)};
    auto const kind{builder.CreateTrunc(builder.CreateLShr(operand.value, kind_shift), i32)};
    auto const name{get_llvm_runtime_module_object_symbol_name(*module)};
    auto const address{get_llvm_external_host_object_address(builder,
        reinterpret_cast<::std::uintptr_t>(module),
        ::uwvm2::utils::container::u8string_view{name.data(), name.size()})};
    if(address == nullptr) { return nullptr; }
    auto const type{::llvm::FunctionType::get(intptr, {intptr, i32, intptr, i32, i32}, false)};
    auto const callee{get_llvm_runtime_bridge_function_symbol_value<llvm_jit_gc_immutable_struct_cast_bridge>(
        builder, type, ::uwvm2::utils::container::u8string_view{u8"gc_immutable_numeric_struct_cast_v1"})};
    if(callee == nullptr) { return nullptr; }
    auto const result{apply_llvm_jit_host_calling_conv(builder.CreateCall(type, callee,
        {address, kind, payload, ::llvm::ConstantInt::get(i32, static_cast<::std::uint32_t>(target.heap.code)),
         ::llvm::ConstantInt::get(i32, target.nullable ? 1u : 0u)}, "gc.cast.native.values"))};
    result->setDoesNotThrow();
    return result;
}

[[nodiscard]] inline ::llvm::Value* emit_runtime_local_func_llvm_jit_gc_reference_test(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::parser::wasm::standard::wasm3::type::core_value_type target) noexcept
{
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
    // Non-witness ref.test/cast/br_on_cast takes the ORIGINAL reader path.
    // Publish the still-unpopped reference tuple and release actual exclusive
    // before that reader may enter shared admission. No eager view reuse.
    if(!snapshot_runtime_local_func_llvm_jit_gc_roots(state) || !sealed_compact_codegen::retire(state)) { return nullptr; }
#endif
    using wasm_type = runtime_operand_stack_value_type;
    if(!state.valid || state.ir_builder == nullptr || state.llvm_module == nullptr ||
       state.local_func_storage_ptr == nullptr || state.operand_stack.empty()) { return nullptr; }
    auto const* module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr || !module->gc_store || !module->gc_store->valid()) { return nullptr; }
    auto& builder{*state.ir_builder};
    auto& stack{state.operand_stack};
    auto const operand{stack.back()};
    auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const i32{builder.getInt32Ty()};
    constexpr auto ref_bytes{sizeof(::uwvm2::uwvm::runtime::storage::gc_reference)};
    // Core 3 exnref has the same complete tagged-reference carrier. It is not
    // named in the legacy value-type enum, so compare its validated wire byte.
    constexpr auto exnref_type{static_cast<wasm_type>(0x69u)};
    if((operand.type != wasm_type::funcref && operand.type != wasm_type::externref &&
        operand.type != exnref_type) || operand.value == nullptr ||
       !operand.value->getType()->isIntegerTy(static_cast<unsigned>(ref_bytes * CHAR_BIT)))
    { return nullptr; }
    auto const slot{create_llvm_jit_entry_block_alloca(builder, builder.getInt8Ty(),
        ::llvm::ConstantInt::get(intptr, ref_bytes), get_llvm_string_ref(u8"gc.test.ref"))};
    if(slot == nullptr) { return nullptr; }
    slot->setAlignment(::llvm::Align{alignof(::uwvm2::uwvm::runtime::storage::gc_reference)});
    // [generated reference slot] is exactly sizeof(gc_reference) bytes.
    // [safe                    ] the validated operand has the same full-width scalar ABI.
    builder.CreateStore(operand.value, slot)->setAlignment(::llvm::Align{1u});
    auto const module_name{get_llvm_runtime_module_object_symbol_name(*module)};
    auto const module_address{get_llvm_external_host_object_address(builder,
        reinterpret_cast<::std::uintptr_t>(module),
        ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()})};
    if(module_address == nullptr) { return nullptr; }
    auto const bridge_type{::llvm::FunctionType::get(i32,
        {intptr, intptr, builder.getInt64Ty(), i32}, false)};
    return emit_runtime_local_func_llvm_jit_runtime_bridge_call<llvm_jit_gc_ref_test_bridge>(
        state, bridge_type,
        {module_address, builder.CreatePtrToInt(slot, intptr),
         ::llvm::ConstantInt::getSigned(builder.getInt64Ty(), target.heap.code),
         ::llvm::ConstantInt::get(i32, target.nullable ? 1u : 0u)});
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_ref_test_cast(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded,
    ::std::size_t next_instruction_offset = SIZE_MAX) noexcept
{
    using wasm_type = runtime_operand_stack_value_type;
    if(decoded.opcode < 20u || decoded.opcode > 23u || state.ir_builder == nullptr ||
       state.llvm_module == nullptr || state.operand_stack.empty()) { return false; }
    auto& builder{*state.ir_builder};
    auto& stack{state.operand_stack};
    auto const i32{builder.getInt32Ty()};
    auto const* module{state.local_func_storage_ptr == nullptr ? nullptr :
        state.local_func_storage_ptr->runtime_module_ptr};
    if(decoded.opcode >= 22u && decoded.to.heap.code >= 0 &&
       static_cast<::std::uint_least64_t>(decoded.to.heap.code) <= UINT32_MAX &&
       module != nullptr && module->gc_store &&
       llvm_jit_gc_immutable_cast_witness_type(*module->gc_store,
           static_cast<::std::uint_least32_t>(decoded.to.heap.code)))
    {
        auto const witness{emit_runtime_local_func_llvm_jit_gc_immutable_cast_witness(state, decoded.to)};
        if(witness == nullptr) { return false; }
        emit_llvm_conditional_trap(*state.llvm_module, builder,
            builder.CreateICmpEQ(witness, ::llvm::ConstantInt::get(witness->getType(), 0u)),
            ::uwvm2::runtime::lib::llvm_jit_trap_kind::cast_failure);
        // [unchanged complete Wasm reference][compiler-private native borrow]
        // [safe                                                           ]
        // the successful cast block has no intervening instruction. Consume
        // only an adjacent compatible getter before ANY possible safepoint.
        auto& operand{stack.back()};
        operand.immutable_gc_values_witness = witness;
        // [same live function-owned cast success block] no runtime pointer is
        // [safe                                       ] moved; this handle is
        // used only to prove the borrow has no intervening emitted operation.
        operand.immutable_gc_values_witness_block = builder.GetInsertBlock();
        operand.immutable_gc_values_witness_type = static_cast<::std::uint_least32_t>(decoded.to.heap.code);
        operand.immutable_gc_values_witness_next_offset = next_instruction_offset;
        return true;
    }
    auto const tested{emit_runtime_local_func_llvm_jit_gc_reference_test(state, decoded.to)};
    if(tested == nullptr) { return false; }
    if(decoded.opcode <= 21u)
    {
        // [SSA prefix][reference] end -> [SSA prefix][i32] end.
        // [safe                     ] the descriptor was copied before pop invalidates it.
        stack.pop_back();
        stack.push_back({.type = wasm_type::i32, .value = tested});
    }
    else
    {
        emit_llvm_conditional_trap(*state.llvm_module, builder,
            builder.CreateICmpEQ(tested, ::llvm::ConstantInt::get(i32, 0u)),
            ::uwvm2::runtime::lib::llvm_jit_trap_kind::cast_failure);
        // The cast refines the exact Wasm type; its 16-byte machine carrier is unchanged.
    }
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_br_on_cast(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded) noexcept
{
    if(decoded.opcode != 24u && decoded.opcode != 25u) { return false; }
    if(!state.valid || state.llvm_function == nullptr || state.ir_builder == nullptr ||
       state.control_stack.empty()) { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    auto const* target{get_runtime_local_func_llvm_jit_branch_target_by_depth(state, decoded.first)};
    if(target == nullptr || state.operand_stack.empty()) { return false; }
    auto& builder{*state.ir_builder};
    auto* current{builder.GetInsertBlock()};
    if(current == nullptr || llvm_jit_basic_block_has_terminator(current)) { return false; }
    // A br_on_cast carries its reference on both edges. The validator has
    // proved the target tuple and its refined heap type; the machine ABI does
    // not change when a reference gains a more specific static type.
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> values{};
    if(!try_get_runtime_local_func_llvm_jit_branch_values(state, target->params, values)) { return false; }
    auto* tested{emit_runtime_local_func_llvm_jit_gc_reference_test(state, decoded.to)};
    if(tested == nullptr) { return false; }
    // The test helper may add IR in the current block, but does not move the
    // insertion cursor or mutate the operand tuple used by the target PHIs.
    if(!try_add_runtime_local_func_llvm_jit_branch_target_incoming(state, *target, values, current)) { return false; }
    auto* continuation{::llvm::BasicBlock::Create(state.llvm_function->getContext(),
        get_llvm_string_ref(u8"br_on_cast.cont"), state.llvm_function)};
    auto* matches{builder.CreateICmpNE(tested, builder.getInt32(0u))};
    if(decoded.opcode == 25u) { matches = builder.CreateNot(matches); }
    builder.CreateCondBr(matches, target->block, continuation);
    // The builder cursor names a compiler-owned continuation block. No guest
    // pointer is formed, and the original reference remains the fallthrough SSA value.
    builder.SetInsertPoint(continuation);
    return true;
}

[[nodiscard]] inline ::std::uint32_t llvm_jit_gc_convert_bridge(
    ::std::uintptr_t module_address, ::std::uint32_t opcode,
    ::std::uintptr_t input_address, ::std::uintptr_t output_address) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    if(module_address == 0u || input_address == 0u || output_address == 0u)
    { return static_cast<::std::uint32_t>(storage::gc_object_status::invalid_value); }
    auto const* module{reinterpret_cast<storage::wasm_module_storage_t const*>(module_address)};
    auto const* store{module->gc_store.get()};
    if(store == nullptr || !store->valid())
    { return static_cast<::std::uint32_t>(storage::gc_object_status::invalid_store); }
    storage::gc_reference input{}, output{};
    // [generated 16-byte input slot] the caller stored the complete reference
    // [safe                       ] before this synchronous host bridge reads it.
    ::std::memcpy(::std::addressof(input), reinterpret_cast<void const*>(input_address), sizeof(input));
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    // Extern/host handle lifecycle is NOT in the numeric page proof.
    ::uwvm2::runtime::gc::managed_page_boundary(true);
#endif
    auto const status{opcode == 26u ? storage::uwvm2_gc_any_convert_extern(store, ::std::addressof(input), ::std::addressof(output)) :
        opcode == 27u ? storage::uwvm2_gc_extern_convert_any(store, ::std::addressof(input), ::std::addressof(output)) :
        storage::gc_object_status::invalid_value};
    if(status == storage::gc_object_status::ok)
    {
        // [generated 16-byte output slot] survives until the JIT caller loads it.
        // [safe                        ] copy exactly one fully initialized carrier.
        ::std::memcpy(reinterpret_cast<void*>(output_address), ::std::addressof(output), sizeof(output));
    }
    return static_cast<::std::uint32_t>(status);
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_convert(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    using wasm_type = runtime_operand_stack_value_type;
    if((decoded.opcode != 26u && decoded.opcode != 27u) || !state.valid ||
       state.ir_builder == nullptr || state.llvm_module == nullptr ||
       state.local_func_storage_ptr == nullptr || state.operand_stack.empty()) { return false; }
    auto const* module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr || !module->gc_store || !module->gc_store->valid()) { return false; }
    auto& builder{*state.ir_builder};
    auto& stack{state.operand_stack};
    auto const input{stack.back()};
    constexpr auto ref_bytes{sizeof(storage::gc_reference)};
    if(input.value == nullptr ||
       input.type != (decoded.opcode == 26u ? wasm_type::externref : wasm_type::funcref) ||
       !input.value->getType()->isIntegerTy(static_cast<unsigned>(ref_bytes * CHAR_BIT)))
    { return false; }
    auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const i32{builder.getInt32Ty()};
    auto const input_slot{create_llvm_jit_entry_block_alloca(builder, builder.getInt8Ty(),
        ::llvm::ConstantInt::get(intptr, ref_bytes), get_llvm_string_ref(u8"gc.convert.input"))};
    auto const output_slot{create_llvm_jit_entry_block_alloca(builder, builder.getInt8Ty(),
        ::llvm::ConstantInt::get(intptr, ref_bytes), get_llvm_string_ref(u8"gc.convert.output"))};
    if(input_slot == nullptr || output_slot == nullptr) { return false; }
    input_slot->setAlignment(::llvm::Align{alignof(storage::gc_reference)});
    output_slot->setAlignment(::llvm::Align{alignof(storage::gc_reference)});
    // [two generated reference slots] each owns exactly ref_bytes initialized bytes.
    // [safe                        ] no guest pointer is formed or stored.
    builder.CreateStore(input.value, input_slot)->setAlignment(::llvm::Align{1u});
    builder.CreateStore(::llvm::ConstantInt::get(input.value->getType(), 0u), output_slot)->setAlignment(::llvm::Align{1u});
    auto const module_name{get_llvm_runtime_module_object_symbol_name(*module)};
    auto const module_address{get_llvm_external_host_object_address(builder,
        reinterpret_cast<::std::uintptr_t>(module),
        ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()})};
    if(module_address == nullptr) { return false; }
    auto const bridge_type{::llvm::FunctionType::get(i32, {intptr, i32, intptr, intptr}, false)};
    auto const status{emit_runtime_local_func_llvm_jit_runtime_bridge_call<llvm_jit_gc_convert_bridge>(
        state, bridge_type, {module_address, builder.getInt32(decoded.opcode),
            builder.CreatePtrToInt(input_slot, intptr), builder.CreatePtrToInt(output_slot, intptr)})};
    if(status == nullptr) { return false; }
    emit_llvm_conditional_trap(*state.llvm_module, builder,
        builder.CreateICmpEQ(status, builder.getInt32(static_cast<unsigned>(storage::gc_object_status::out_of_memory))),
        ::uwvm2::runtime::lib::llvm_jit_trap_kind::gc_allocation_failure);
    emit_llvm_conditional_trap(*state.llvm_module, builder,
        builder.CreateICmpNE(status, builder.getInt32(0u)),
        ::uwvm2::runtime::lib::llvm_jit_trap_kind::runtime_invariant_failure);
    auto const converted{builder.CreateLoad(input.value->getType(), output_slot,
        get_llvm_string_ref(u8"gc.convert.result"))};
    converted->setAlignment(::llvm::Align{1u});
    // [SSA prefix][one reference] end -> [SSA prefix][converted reference] end.
    // [safe                         ] input was copied before this mutation.
    stack.pop_back();
    stack.push_back({.type = decoded.opcode == 26u ? wasm_type::funcref : wasm_type::externref,
        .value = converted});
    return true;
}

// The validator/type registry has proved that this field's unpacked
// result is i32 or f32. Integer status/payload transport is independent of the
// Wasm reference carrier's native byte order, including 32-bit native targets.
[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_struct_get32(
    runtime_local_func_llvm_jit_emit_state_t&,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const&,
    runtime_operand_stack_value_type, bool) noexcept;

// Consume an adjacent checked cast's LOCAL immutable storage borrow.
// Legacy objects expose their original carrier array. A compact single-field
// i32/f32 object exposes exactly one raw four-byte cell, only for field zero.
// A successful foreign/null cast has witness 1 and takes the existing checked
// getter, retaining its lease/null trap. The fast edge does one native 32-bit
// load and no helper/hash/lock. No native address becomes a Wasm stack value.
[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_cast_field_get32(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded,
    runtime_operand_stack_value_type result_type) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
    auto& builder{*state.ir_builder};
    auto& stack{state.operand_stack};
    auto const operand{stack.back()};
    auto const* module{state.local_func_storage_ptr->runtime_module_ptr};
    auto const* field{module->gc_store->field_at(decoded.first, decoded.second)};
    if(field == nullptr || field->mutable_ || operand.immutable_gc_values_witness == nullptr ||
       operand.immutable_gc_values_witness_block != builder.GetInsertBlock() ||
       !builder.GetInsertBlock()->empty() || operand.immutable_gc_values_witness_type != decoded.first ||
       operand.immutable_gc_values_witness_next_offset == SIZE_MAX ||
       operand.immutable_gc_values_witness_next_offset != state.current_wasm_op_offset ||
       static_cast<::std::uint_least64_t>(decoded.second) >
           static_cast<::std::uint_least64_t>(PTRDIFF_MAX) / sizeof(storage::gc_object_value)) { return false; }
    auto const result_kind{field->storage.packed == type::packed_kind::none ?
        field->storage.value.kind : type::value_kind::i32};
    if((result_type == runtime_operand_stack_value_type::i32 && result_kind != type::value_kind::i32) ||
       (result_type == runtime_operand_stack_value_type::f32 && result_kind != type::value_kind::f32)) { return false; }
    auto const function{builder.GetInsertBlock()->getParent()};
    // [LLVM function-owned blocks] these handles stay live through emission.
    // [safe                      ] every branch is within this same function.
    auto const fast{::llvm::BasicBlock::Create(builder.getContext(), "gc.get32.borrow.local", function)};
    auto const slow{::llvm::BasicBlock::Create(builder.getContext(), "gc.get32.borrow.checked", function)};
    auto const joined{::llvm::BasicBlock::Create(builder.getContext(), "gc.get32.borrow.merge", function)};
    auto const address{operand.immutable_gc_values_witness};
    builder.CreateCondBr(builder.CreateICmpUGT(address, ::llvm::ConstantInt::get(address->getType(), 1u)), fast, slow);
    // [LOCAL initialized legacy carriers OR one compact raw4 cell]
    // [safe                                                      ] this
    // block has no call/safepoint between producing and consuming the borrow.
    // The cast helper checked canonical owner/type and exact immutable storage.
    // A compact witness is returned ONLY for a one-field unpacked i32/f32
    // target, so its validated field index and byte offset are both zero.
    // Legacy witnesses retain the original array-extent and prefix checks.
    // Only a >1 native witness reaches this edge; sentinel/null/foreign is never loaded.
    builder.SetInsertPoint(fast);
    auto const values{builder.CreateIntToPtr(address, builder.getPtrTy(), "gc.get32.borrow.values")};
    auto const offset{static_cast<::std::size_t>(decoded.second) * sizeof(storage::gc_object_value)};
    // Legacy: [proved carrier prefix: offset][at least four readable bytes].
    // Compact: [one raw4 cell] offset==0, load size==4, field index==0.
    // [safe ] offset never advances a compact borrow past its real raw cell.
    //         ^^ form only a cast-proved native storage address, never a token.
    auto const slot{builder.CreateInBoundsGEP(builder.getInt8Ty(), values,
        ::llvm::ConstantInt::get(address->getType(), offset), "gc.get32.borrow.field")};
    auto const loaded{builder.CreateLoad(builder.getInt32Ty(), slot, "gc.get32.borrow.bits")};
    loaded->setAlignment(::llvm::Align{1u});
    ::llvm::Value* numeric{loaded};
    if(field->storage.packed != type::packed_kind::none)
    {
        unsigned const width{field->storage.packed == type::packed_kind::i8 ? 8u : 16u};
        // The store packs a native i32 at the beginning of its carrier. LLVM
        // integer shifts/masks preserve all packed values and sign extension;
        // no native floating-point evaluation or signed C++ overflow occurs.
        numeric = decoded.opcode == 3u ?
            builder.CreateAShr(builder.CreateShl(numeric, 32u - width), 32u - width) :
            builder.CreateAnd(numeric, ::llvm::ConstantInt::get(builder.getInt32Ty(), (1u << width) - 1u));
    }
    auto const fast_value{result_type == runtime_operand_stack_value_type::f32 ?
        builder.CreateBitCast(numeric, builder.getFloatTy(), "gc.get32.borrow.f32") : numeric};
    auto const fast_end{builder.GetInsertBlock()};
    builder.CreateBr(joined);
    // [separate checked edge] original complete reference is still the stack
    // [safe                 ] operand; the ordinary getter owns all status
    // traps and any foreign lease. Disable witness recursion on this edge.
    builder.SetInsertPoint(slow);
    if(!try_emit_runtime_local_func_llvm_jit_gc_struct_get32(state, decoded, result_type, false)) { return false; }
    auto const slow_value{stack.back().value};
    auto const slow_end{builder.GetInsertBlock()};
    builder.CreateBr(joined);
    // [same-function SSA values] native borrow is NOT transferred to this PHI;
    // [safe                    ] only the Wasm scalar result crosses the merge.
    builder.SetInsertPoint(joined);
    auto const value{builder.CreatePHI(fast_value->getType(), 2u, "gc.get32.borrow.result")};
    value->addIncoming(fast_value, fast_end);
    value->addIncoming(slow_value, slow_end);
    stack.back() = {.type = result_type, .value = value};
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_struct_get32(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded,
    runtime_operand_stack_value_type result_type, bool allow_cast_witness = true) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    using wasm_type = runtime_operand_stack_value_type;
    using reference = storage::gc_reference;
    static_assert(::std::is_standard_layout_v<reference>);
    constexpr auto bytes{sizeof(reference)};
    constexpr auto payload_offset{offsetof(reference, storage)};
    constexpr auto payload_bytes{sizeof(reference::storage)};
    constexpr auto kind_offset{offsetof(reference, kind)};
    constexpr auto kind_bytes{sizeof(reference::kind)};
    static_assert(payload_bytes == sizeof(::std::uintptr_t));
    static_assert(kind_bytes == sizeof(::std::uint32_t));
    static_assert(payload_offset <= bytes && payload_bytes <= bytes - payload_offset);
    static_assert(kind_offset <= bytes && kind_bytes <= bytes - kind_offset);
    static_assert(payload_offset + payload_bytes <= kind_offset ||
        kind_offset + kind_bytes <= payload_offset);
    if(!state.valid || state.ir_builder == nullptr || state.llvm_module == nullptr ||
       state.local_func_storage_ptr == nullptr || state.operand_stack.empty() ||
       decoded.opcode < 2u || decoded.opcode > 4u ||
       (result_type != wasm_type::i32 && result_type != wasm_type::f32)) { return false; }
    auto& builder{*state.ir_builder};
    auto& stack{state.operand_stack};
    auto const operand{stack.back()};
    if(operand.type != wasm_type::funcref || operand.value == nullptr ||
       !operand.value->getType()->isIntegerTy(static_cast<unsigned>(bytes * CHAR_BIT))) { return false; }
    auto const* module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr || !module->gc_store || !module->gc_store->valid()) { return false; }
    // Select the witness path only after EVERY eligibility check that can
    // decline it. A valid Wasm getter with a larger native byte offset or a
    // different target layout keeps the original checked bridge; failure to
    // optimize it must not become a compilation failure after creating IR.
    auto const* witness_field{allow_cast_witness ?
        module->gc_store->field_at(decoded.first, decoded.second) : nullptr};
    if(allow_cast_witness && witness_field != nullptr && !witness_field->mutable_ &&
       static_cast<::std::uint_least64_t>(decoded.second) <=
           static_cast<::std::uint_least64_t>(PTRDIFF_MAX) / sizeof(storage::gc_object_value) &&
       ((result_type == wasm_type::i32 &&
         (witness_field->storage.packed != ::uwvm2::parser::wasm::standard::wasm3::type::packed_kind::none ||
          witness_field->storage.value.kind == ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::i32)) ||
        (result_type == wasm_type::f32 &&
         witness_field->storage.packed == ::uwvm2::parser::wasm::standard::wasm3::type::packed_kind::none &&
         witness_field->storage.value.kind == ::uwvm2::parser::wasm::standard::wasm3::type::value_kind::f32)) &&
       operand.immutable_gc_values_witness != nullptr &&
       operand.immutable_gc_values_witness_block == builder.GetInsertBlock() &&
       operand.immutable_gc_values_witness_next_offset != SIZE_MAX &&
       operand.immutable_gc_values_witness_next_offset == state.current_wasm_op_offset &&
       builder.GetInsertBlock()->empty() && operand.immutable_gc_values_witness_type == decoded.first)
    {
        return try_emit_runtime_local_func_llvm_jit_gc_cast_field_get32(state, decoded, result_type);
    }
    auto const& layout{state.llvm_module->getDataLayout()};
    if(state.llvm_module->getDataLayoutStr().empty() ||
       layout.getPointerSize() != sizeof(::std::uintptr_t) ||
       layout.isLittleEndian() != (::std::endian::native == ::std::endian::little)) { return false; }
    llvm_jit_gc_status_dispatch_plan status_dispatch{};
    if(!status_dispatch.prepare(*state.llvm_module, builder)) { return false; }
    auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const i32{builder.getInt32Ty()};
    auto const i64{builder.getInt64Ty()};
    constexpr bool little_endian{::std::endian::native == ::std::endian::little};
    constexpr auto payload_shift{static_cast<unsigned>((little_endian ? payload_offset :
        bytes - payload_offset - payload_bytes) * CHAR_BIT)};
    constexpr auto kind_shift{static_cast<unsigned>((little_endian ? kind_offset :
        bytes - kind_offset - kind_bytes) * CHAR_BIT)};
    // [complete native reference SSA bits] no native/guest pointer is formed
    // or advanced. Extract exactly the two proved fields; padding is ignored.
    auto const payload{builder.CreateTrunc(builder.CreateLShr(operand.value, payload_shift),
        intptr, get_llvm_string_ref(u8"gc.get32.payload"))};
    auto const kind{builder.CreateTrunc(builder.CreateLShr(operand.value, kind_shift),
        i32, get_llvm_string_ref(u8"gc.get32.kind"))};
    auto const module_name{get_llvm_runtime_module_object_symbol_name(*module)};
    auto const module_address{get_llvm_external_host_object_address(builder,
        reinterpret_cast<::std::uintptr_t>(module),
        ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()})};
    if(module_address == nullptr) { return false; }
    auto const bridge_type{::llvm::FunctionType::get(i64, {intptr, i32, intptr, i32}, false)};
    ::llvm::Value* arguments[]{module_address, kind, payload,
        ::llvm::ConstantInt::get(i32, decoded.second)};
    // Function-template instances need explicit semantic names even when
    // their native signatures match. Do not rely on __PRETTY_FUNCTION__ to
    // distinguish signed and unsigned packed-field reads in object caches.
    auto const call{[&]<bool Signed>() noexcept -> ::llvm::CallInst*
    {
        auto const discriminator{Signed ? ::uwvm2::utils::container::u8string_view{u8"gc_struct_get32_signed"} :
            ::uwvm2::utils::container::u8string_view{u8"gc_struct_get32_unsigned"}};
        auto const callee{get_llvm_runtime_bridge_function_symbol_value<llvm_jit_gc_struct_get32_bridge<Signed>>(
            builder, bridge_type, discriminator)};
        if(callee == nullptr) { return nullptr; }
        // [live native declaration][four exact integer ABI arguments]
        // [safe                                                     ] only
        // compiler/native carrier bits are transmitted; no guest pointer.
        auto const result{apply_llvm_jit_host_calling_conv(builder.CreateCall(bridge_type, callee, arguments))};
        result->setDoesNotThrow();
        return result;
    }};
    auto const packed{decoded.opcode == 3u ? call.template operator()<true>() :
        call.template operator()<false>()};
    if(packed == nullptr) { return false; }
    auto const bridge_status{builder.CreateTrunc(builder.CreateLShr(packed, 32u), i32,
        get_llvm_string_ref(u8"gc.get32.status"))};
    status_dispatch.commit(*bridge_status);
    auto const raw{builder.CreateTrunc(packed, i32, get_llvm_string_ref(u8"gc.get32.bits"))};
    auto const result{result_type == wasm_type::f32 ?
        builder.CreateBitCast(raw, builder.getFloatTy(), get_llvm_string_ref(u8"gc.get32.f32")) : raw};
    // [SSA prefix][one complete reference] end -> [SSA prefix][scalar] end
    // [safe                                  ] copied operand has no vector
    // alias; read neither the retired descriptor nor a success output buffer.
    stack.pop_back();
    stack.push_back({.type = result_type, .value = result});
    return true;
}

#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
#include "single_func_gc_array_get32.h"
#endif

#if defined(UWVM_EXPERIMENTAL_NUMERIC_ARRAY_SET32) && UWVM_EXPERIMENTAL_NUMERIC_ARRAY_SET32 == 1
#include "single_func_gc_array_set32.h"
#endif

#if defined(UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32) && UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32 == 1
#include "single_func_gc_struct_set32.h"
#endif

#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_aggregate_unsealed(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded, bool allocation_already_polled = false) noexcept
#endif
#if !defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) || UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR != 1
[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_aggregate(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded) noexcept
#endif
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    namespace gc_type = ::uwvm2::parser::wasm::standard::wasm3::type;
    using wasm_type = runtime_operand_stack_value_type;
    if(!state.valid || state.ir_builder == nullptr || state.llvm_module == nullptr ||
       state.local_func_storage_ptr == nullptr) { return false; }
    auto const* module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr || !module->gc_store || !module->gc_store->valid()) { return false; }
    auto& builder{*state.ir_builder};
    auto& stack{state.operand_stack};
    auto const op{decoded.opcode};
    if(op > 19u) { return false; }
    ::std::size_t input_count{};
    gc_type::field_type const* field{};
    gc_type::composite_kind kind{};
    if(op != 15u)
    {
        if(!module->gc_store->type_kind(decoded.first, kind)) { return false; }
        if(op <= 5u && kind != gc_type::composite_kind::struct_) { return false; }
        if(op >= 6u && kind != gc_type::composite_kind::array) { return false; }
    }
    if(op == 0u)
    { if(!module->gc_store->field_count(decoded.first, input_count)) { return false; } }
    else if(op == 1u) { input_count = 0uz; }
    else if(op >= 2u && op <= 4u)
    { input_count = 1uz; field = module->gc_store->field_at(decoded.first, decoded.second); }
    else if(op == 5u)
    { input_count = 2uz; field = module->gc_store->field_at(decoded.first, decoded.second); }
    else if(op == 6u) { input_count = 2uz; field = module->gc_store->field_at(decoded.first, 0uz); }
    else if(op == 7u) { input_count = 1uz; }
    else if(op == 8u) { input_count = decoded.second; field = module->gc_store->field_at(decoded.first, 0uz); }
    else if(op == 9u || op == 10u)
    { input_count = 2uz; field = module->gc_store->field_at(decoded.first, 0uz); }
    else if(op >= 11u && op <= 13u)
    { input_count = 2uz; field = module->gc_store->field_at(decoded.first, 0uz); }
    else if(op == 14u) { input_count = 3uz; field = module->gc_store->field_at(decoded.first, 0uz); }
    else if(op == 15u) { input_count = 1uz; }
    else if(op == 16u) { input_count = 4uz; field = module->gc_store->field_at(decoded.first, 0uz); }
    else if(op == 17u) { input_count = 5uz; }
    else if(op == 18u || op == 19u)
    { input_count = 4uz; field = module->gc_store->field_at(decoded.first, 0uz); }
    else { return false; }
    if(((op >= 2u && op <= 6u) || (op >= 8u && op <= 14u) || op == 16u || op == 18u || op == 19u) &&
       field == nullptr)
    { return false; }
    if(stack.size() < input_count || input_count > (::std::numeric_limits<::std::size_t>::max)() / sizeof(storage::gc_object_value))
    { return false; }
    if(op >= 2u && op <= 4u)
    {
        auto const result_kind{field->storage.packed == gc_type::packed_kind::none ?
            field->storage.value.kind : gc_type::value_kind::i32};
        if(result_kind == gc_type::value_kind::i32 || result_kind == gc_type::value_kind::f32)
        {
            return try_emit_runtime_local_func_llvm_jit_gc_struct_get32(state, decoded,
                result_kind == gc_type::value_kind::i32 ? wasm_type::i32 : wasm_type::f32);
        }
    }
#if defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) && UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS == 1
    if(op >= 11u && op <= 13u)
    {
        auto const result_kind{field->storage.packed == gc_type::packed_kind::none ?
            field->storage.value.kind : gc_type::value_kind::i32};
        auto const& native_layout{state.llvm_module->getDataLayout()};
        // Decide all target-layout eligibility before creating scalar-path IR.
        // Other targets keep the complete original aggregate ABI below.
        if((result_kind == gc_type::value_kind::i32 || result_kind == gc_type::value_kind::f32) &&
           !state.llvm_module->getDataLayoutStr().empty() &&
           native_layout.getPointerSize() == sizeof(::std::uintptr_t) &&
           native_layout.isLittleEndian() == (::std::endian::native == ::std::endian::little))
        {
            return try_emit_runtime_local_func_llvm_jit_gc_array_get32(state, decoded,
                result_kind == gc_type::value_kind::i32 ? wasm_type::i32 : wasm_type::f32);
        }
    }
#endif
#if defined(UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32) && UWVM_EXPERIMENTAL_NUMERIC_STRUCT_SET32 == 1
    if(op == 5u)
    {
        // [retained canonical declaration][checked field pointer]
        // [safe ] the existing type/field/count checks above precede selection.
        // Numeric32 only changes carrier transport; wider/ref fields keep the
        // generic helper. Actual object membership and locks are not elided.
        auto const value_kind{field->storage.value.kind};
        auto const packed{field->storage.packed};
        auto const& native_layout{state.llvm_module->getDataLayout()};
        if(field->mutable_ &&
           ((packed == gc_type::packed_kind::none &&
             (value_kind == gc_type::value_kind::i32 || value_kind == gc_type::value_kind::f32)) ||
            ((packed == gc_type::packed_kind::i8 || packed == gc_type::packed_kind::i16) &&
             value_kind == gc_type::value_kind::i32)) &&
           !state.llvm_module->getDataLayoutStr().empty() &&
           native_layout.getPointerSize() == sizeof(::std::uintptr_t) &&
           native_layout.isLittleEndian() == (::std::endian::native == ::std::endian::little))
        {
            return try_emit_runtime_local_func_llvm_jit_gc_struct_set32(state, decoded,
                value_kind == gc_type::value_kind::i32 ? wasm_type::i32 : wasm_type::f32);
        }
    }
#endif
#if defined(UWVM_EXPERIMENTAL_NUMERIC_ARRAY_SET32) && UWVM_EXPERIMENTAL_NUMERIC_ARRAY_SET32 == 1
    if(op == 14u)
    {
        auto const value_kind{field->storage.packed == gc_type::packed_kind::none ?
            field->storage.value.kind : gc_type::value_kind::i32};
        auto const packed{field->storage.packed};
        auto const& native_layout{state.llvm_module->getDataLayout()};
        // Select only a genuinely validated mutable numeric32 array. All
        // unsupported shapes/layouts keep the complete old aggregate ABI.
        if(field->mutable_ &&
           ((packed == gc_type::packed_kind::none &&
             (value_kind == gc_type::value_kind::i32 || value_kind == gc_type::value_kind::f32)) ||
            ((packed == gc_type::packed_kind::i8 || packed == gc_type::packed_kind::i16) &&
             field->storage.value.kind == gc_type::value_kind::i32)) &&
           !state.llvm_module->getDataLayoutStr().empty() &&
           native_layout.getPointerSize() == sizeof(::std::uintptr_t) &&
           native_layout.isLittleEndian() == (::std::endian::native == ::std::endian::little))
        {
            return try_emit_runtime_local_func_llvm_jit_gc_array_set32(state, decoded,
                value_kind == gc_type::value_kind::i32 ? wasm_type::i32 : wasm_type::f32);
        }
    }
#endif
    auto const input_base{stack.size() - input_count};
    // Resolve the original cold trap before root snapshot/input allocation or
    // the native aggregate call. A declined plan cannot repeat these effects.
    llvm_jit_gc_status_dispatch_plan status_dispatch{};
    if(!status_dispatch.prepare(*state.llvm_module, builder)) { return false; }
    if((op == 0u || op == 1u || (op >= 6u && op <= 10u)) &&
       !snapshot_runtime_local_func_llvm_jit_gc_roots(state)) { return false; }
    // Both the operand prefix and every reference input remain rooted before
    // scratch allocation and the aggregate allocator; descriptors are not
    // removed until the successful helper/status/output path below.
    auto& context{builder.getContext()};
    auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const i32{builder.getInt32Ty()};
    auto const i8{builder.getInt8Ty()};
    auto const input_bytes{input_count * sizeof(storage::gc_object_value)};
    ::llvm::Value* input_address{::llvm::ConstantInt::get(intptr, 0u)};
    ::llvm::Value* input_pointer{};
    bool const heap_input{input_bytes > 2048uz};
    if(input_bytes != 0uz)
    {
        if(heap_input)
        {
            auto const alloc_type{::llvm::FunctionType::get(intptr, {intptr}, false)};
            auto const allocated{emit_runtime_local_func_llvm_jit_runtime_bridge_call<llvm_jit_gc_input_allocate_bridge>(
                state, alloc_type, {::llvm::ConstantInt::get(intptr, input_bytes)})};
            if(allocated == nullptr) { return false; }
            emit_llvm_conditional_trap(*state.llvm_module, builder,
                builder.CreateICmpEQ(allocated, ::llvm::ConstantInt::get(intptr, 0u)),
                ::uwvm2::runtime::lib::llvm_jit_trap_kind::gc_allocation_failure);
            input_address = allocated;
            input_pointer = builder.CreateIntToPtr(allocated, ::llvm::PointerType::get(context, 0u));
        }
        else
        {
            auto const slot{create_llvm_jit_entry_block_alloca(builder, i8,
                ::llvm::ConstantInt::get(intptr, input_bytes), get_llvm_string_ref(u8"gc.input"))};
            if(slot == nullptr) { return false; }
            slot->setAlignment(::llvm::Align{alignof(storage::gc_object_value)});
            input_pointer = slot;
            input_address = builder.CreatePtrToInt(slot, intptr);
        }
        // Full-width stores below initialize all input bytes.
    }
    for(::std::size_t index{}; index != input_count; ++index)
    {
        // [stack prefix][input_count live SSA descriptors] end
        // [safe                                      ] input_base+index < stack.size(); no mutation until after call.
        auto const operand{stack[input_base + index]};
        if(operand.value == nullptr || get_runtime_wasm_value_type_abi_size(operand.type) > sizeof(storage::gc_object_value))
        { return false; }
        // [input_count slots of 16 bytes] end; index<count and checked input_bytes prove the byte offset.
        // [safe                         ] ^^ slot points to the start of exactly one initialized slot.
        auto const slot{builder.CreateInBoundsGEP(i8, input_pointer,
            ::llvm::ConstantInt::get(intptr, index * sizeof(storage::gc_object_value)))};
        // Write every carrier bit once, including numeric padding. This removes
        // a separate memset pass without borrowing stale scratch contents. On a
        // big-endian target, the original narrow bytes belong at the beginning
        // of the slot, so widen then shift them into that byte position.
        auto const bits{static_cast<unsigned>(get_runtime_wasm_value_type_abi_size(operand.type) * CHAR_BIT)};
        auto const scalar_type{builder.getIntNTy(bits)};
        auto const scalar{builder.CreateBitCast(operand.value, scalar_type)};
        ::llvm::Value* carrier{};
        if(bits == 32u || bits == 64u)
        {
            // Lane zero always occupies the beginning of the native carrier.
            // Insert into a zero vector instead of zext-to-i128 plus bitcast:
            // InstCombine folds the latter back to two scalar stores.
            auto const vector_type{::llvm::FixedVectorType::get(scalar_type, 128u / bits)};
            carrier = builder.CreateInsertElement(::llvm::Constant::getNullValue(vector_type),
                                                  scalar, builder.getInt32(0u));
        }
        else
        {
            auto const carrier_type{builder.getIntNTy(static_cast<unsigned>(sizeof(storage::gc_object_value) * CHAR_BIT))};
            auto* full_bits{scalar};
            if(bits < carrier_type->getBitWidth())
            {
                full_bits = builder.CreateZExt(full_bits, carrier_type);
                if(!state.llvm_module->getDataLayout().isLittleEndian())
                { full_bits = builder.CreateShl(full_bits, carrier_type->getBitWidth() - bits); }
            }
            // LLVM's same-width bitcast preserves native byte representation,
            // including big-endian reference and v128 lane ordering.
            carrier = builder.CreateBitCast(full_bits, ::llvm::FixedVectorType::get(builder.getInt64Ty(), 2u));
        }
        auto const stored{builder.CreateStore(carrier, slot)};
        stored->setAlignment(::llvm::Align{1u});
    }
    auto const output{create_llvm_jit_entry_block_alloca(builder, i8,
        ::llvm::ConstantInt::get(intptr, sizeof(storage::gc_object_value)), get_llvm_string_ref(u8"gc.output"))};
    if(output == nullptr) { return false; }
    output->setAlignment(::llvm::Align{alignof(storage::gc_object_value)});
    auto const module_name{get_llvm_runtime_module_object_symbol_name(*module)};
    auto const module_address{get_llvm_external_host_object_address(builder,
        reinterpret_cast<::std::uintptr_t>(module),
        ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()})};
    if(module_address == nullptr) { return false; }
    auto const bridge_type{::llvm::FunctionType::get(i32,
        {intptr, i32, i32, i32, intptr, intptr, intptr}, false)};
    ::llvm::Value* arguments[]{module_address, ::llvm::ConstantInt::get(i32, op),
        ::llvm::ConstantInt::get(i32, decoded.first), ::llvm::ConstantInt::get(i32, decoded.second),
        input_address, ::llvm::ConstantInt::get(intptr, input_count), builder.CreatePtrToInt(output, intptr)};
    auto const fixed_bridge_type{::llvm::FunctionType::get(i32,
        {intptr, i32, i32, intptr, intptr}, false)};
    ::llvm::Value* fixed_arguments[]{arguments[0], arguments[2], arguments[3], arguments[4], arguments[6]};
    auto const invoke_fixed{[&]<::std::uint_least32_t Opcode, ::std::size_t Inputs>() noexcept -> ::llvm::Value*
    {

#if defined(UWVM_EXPERIMENTAL_GC_ARRAY_SET_RAW_CARRIER_ABI) && UWVM_EXPERIMENTAL_GC_ARRAY_SET_RAW_CARRIER_ABI == 1
#if !defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
        if constexpr(Opcode == 14u && Inputs == 3uz)
        {
            // Existing validated three live descriptors and complete16B slots;
            // the actual i32 SSA operand, rather than bytes in a wider carrier,
            // supplies the index without an endian-dependent truncation.
            auto const index_operand{stack[input_base + 1uz]};
            if(input_pointer == nullptr || index_operand.value == nullptr ||
               index_operand.value->getType() != i32) { return nullptr; }
            auto const i64{builder.getInt64Ty()};
            auto const raw_type{::llvm::FunctionType::get(i64,
                {intptr, i64, i64, i64, i64, i64}, false)};
            auto const raw_callee{get_llvm_runtime_bridge_function_symbol_value<
                llvm_jit_gc_array_set_raw_carrier_bridge>(builder, raw_type,
                    ::uwvm2::utils::container::u8string_view{u8"gc_array_set_raw_carrier_v3"})};
            if(raw_callee == nullptr) { return nullptr; }
            auto const chunk{[&](::std::size_t offset) noexcept -> ::llvm::Value*
            {
                // offset is one of0,8,32,40, each entire8B chunk lies inside
                // the original initialized48B buffer. Loads honor target endian
                // and the native leaf memcpy preserves those exact raw bytes.
                auto const pointer{builder.CreateInBoundsGEP(i8, input_pointer,
                    ::llvm::ConstantInt::get(intptr, offset))};
                auto const loaded{builder.CreateLoad(i64, pointer)};
                loaded->setAlignment(::llvm::Align{1u});
                return loaded;
            }};
            ::llvm::Value* raw_arguments[]{module_address, chunk(0uz), chunk(8uz),
                builder.CreateZExt(index_operand.value, i64), chunk(32uz), chunk(40uz)};
            auto const call{apply_llvm_jit_host_calling_conv(
                builder.CreateCall(raw_type, raw_callee, raw_arguments))};
            call->setDoesNotThrow();
            // Wide host index/status avoid target-specific narrow integer ABI
            // promotions. The original unsigned i32 index is zero-extended
            // explicitly; status is the unchanged checked enum narrowed only
            // inside generated code, after the complete native operation.
            return builder.CreateTrunc(call, i32);
        }
#endif // experimental managed page retains the original buffer ABI
#endif
        // Clang may spell equal-signature non-type function-template values
        // identically in __PRETTY_FUNCTION__. Without an explicit operation
        // and input-count discriminator, MCJIT can bind struct.new and
        // struct.get to the same external symbol and call the wrong helper.
        // Keep this stable semantic name independent of process addresses;
        // cached object relocation must resolve the same operation/count.
        constexpr bool allocates{Opcode == 0u || Opcode == 1u || (Opcode >= 6u && Opcode <= 10u)};
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
        bool const managed{allocates && !allocation_already_polled && state.emit_precise_gc_root_frames};
#else
        bool const managed{allocates && state.emit_precise_gc_root_frames};
#endif
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
        constexpr bool page_shape{(Opcode == 0u && Inputs == 1uz) || (Opcode == 1u && Inputs == 0uz)};
        auto const prefix{managed && page_shape ? ::uwvm2::utils::container::u8string_view{u8"gc_page_v1_managed_fixed_"} : managed ?
#else
        auto const prefix{managed ?
#endif
            ::uwvm2::utils::container::u8string_view{u8"gc_managed_fixed_"} :
            ::uwvm2::utils::container::u8string_view{u8"gc_fixed_"}};
        auto const discriminator{::uwvm2::utils::container::u8concat_uwvm(
            prefix,
            ::fast_io::mnp::dec(Opcode), u8"_", ::fast_io::mnp::dec(Inputs))};
        auto const name{::uwvm2::utils::container::u8string_view{discriminator.data(), discriminator.size()}};
        auto const callee{[&]() noexcept -> ::llvm::Value*
        {
            if constexpr(allocates)
            {
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
                if constexpr(page_shape)
                {
                    if(managed) { return get_llvm_runtime_bridge_function_symbol_value<
                        llvm_jit_gc_page_new32_bridge<Opcode, Inputs>>(builder, fixed_bridge_type, name); }
                }
#endif
                if(managed) { return get_llvm_runtime_bridge_function_symbol_value<
                    llvm_jit_gc_managed_aggregate_fixed_bridge<Opcode, Inputs>>(builder, fixed_bridge_type, name); }
            }
            return get_llvm_runtime_bridge_function_symbol_value<
                llvm_jit_gc_aggregate_fixed_bridge<Opcode, Inputs>>(builder, fixed_bridge_type, name);
        }()};
        if(callee == nullptr) { return nullptr; }
        // [compiler-owned five-argument declaration and initialized operands]
        // [safe                                                         ] the
        // declaration and call share exactly the fixed bridge's native ABI.
        auto const call{apply_llvm_jit_host_calling_conv(
            builder.CreateCall(fixed_bridge_type, callee, fixed_arguments))};
        call->setDoesNotThrow();
        return call;
    }};
    auto const bridge_status{[&]() noexcept -> ::llvm::Value*
    {
        // No guest pointer or decoding cursor changes here. These operands
        // are live builder-owned SSA handles and the existing buffer extents
        // were proved above; specialization preserves the complete native ABI.
        switch(op)
        {
            case 0u: case 8u:
            {
                auto const dispatch_count{[&]<::std::uint_least32_t Opcode>() noexcept -> ::llvm::Value*
                {
                    switch(input_count)
                    {
                        case 0uz: return invoke_fixed.template operator()<Opcode, 0uz>();
                        case 1uz: return invoke_fixed.template operator()<Opcode, 1uz>();
                        case 2uz: return invoke_fixed.template operator()<Opcode, 2uz>();
                        case 3uz: return invoke_fixed.template operator()<Opcode, 3uz>();
                        case 4uz: return invoke_fixed.template operator()<Opcode, 4uz>();
                        case 5uz: return invoke_fixed.template operator()<Opcode, 5uz>();
                        case 6uz: return invoke_fixed.template operator()<Opcode, 6uz>();
                        case 7uz: return invoke_fixed.template operator()<Opcode, 7uz>();
                        case 8uz: return invoke_fixed.template operator()<Opcode, 8uz>();
                        default: return nullptr;
                    }
                }};
                if(input_count <= 8uz)
                { return op == 0u ? dispatch_count.template operator()<0u>() :
                    dispatch_count.template operator()<8u>(); }
                break;
            }
            case 1u: return invoke_fixed.template operator()<1u, 0uz>();
            case 2u: return invoke_fixed.template operator()<2u, 1uz>();
            case 3u: return invoke_fixed.template operator()<3u, 1uz>();
            case 4u: return invoke_fixed.template operator()<4u, 1uz>();
            case 5u: return invoke_fixed.template operator()<5u, 2uz>();
            case 6u: return invoke_fixed.template operator()<6u, 2uz>();
            case 7u: return invoke_fixed.template operator()<7u, 1uz>();
            case 9u: return invoke_fixed.template operator()<9u, 2uz>();
            case 10u: return invoke_fixed.template operator()<10u, 2uz>();
            case 11u: return invoke_fixed.template operator()<11u, 2uz>();
            case 12u: return invoke_fixed.template operator()<12u, 2uz>();
            case 13u: return invoke_fixed.template operator()<13u, 2uz>();
            case 14u: return invoke_fixed.template operator()<14u, 3uz>();
            case 15u: return invoke_fixed.template operator()<15u, 1uz>();
            case 16u: return invoke_fixed.template operator()<16u, 4uz>();
            case 17u: return invoke_fixed.template operator()<17u, 5uz>();
            case 18u: return invoke_fixed.template operator()<18u, 4uz>();
            case 19u: return invoke_fixed.template operator()<19u, 4uz>();
            default: break;
        }
        if(state.emit_precise_gc_root_frames && (op == 0u || op == 1u || (op >= 6u && op <= 10u)))
        {
            // A distinct semantic discriminator is required even for the
            // unspecialized ABI: cached relocations must never bind a managed
            // allocation to the roots-disabled helper (or vice versa).
            auto const callee{get_llvm_runtime_bridge_function_symbol_value<
                llvm_jit_gc_managed_aggregate_bridge>(builder, bridge_type,
                    ::uwvm2::utils::container::u8string_view{u8"gc_managed_aggregate_v1"})};
            if(callee == nullptr) { return nullptr; }
            auto const call{apply_llvm_jit_host_calling_conv(builder.CreateCall(bridge_type, callee, arguments))};
            call->setDoesNotThrow();
            return call;
        }
        return emit_runtime_local_func_llvm_jit_runtime_bridge_call<llvm_jit_gc_aggregate_bridge<>>(
            state, bridge_type, arguments);
    }()};
    if(bridge_status == nullptr) { return false; }
    if(heap_input)
    {
        auto const free_type{::llvm::FunctionType::get(builder.getVoidTy(), {intptr}, false)};
        if(emit_runtime_local_func_llvm_jit_runtime_bridge_call<llvm_jit_gc_input_free_bridge>(
            state, free_type, {input_address}) == nullptr) { return false; }
    }
    // The original heap-input release remains before both success and trap.
    // Output loads/operand retirement below are dominated by status==ok.
    status_dispatch.commit(*bridge_status);
    wasm_type result_type{};
    bool has_result{};
    if(op == 0u || op == 1u || (op >= 6u && op <= 10u))
    { result_type = wasm_type::funcref; has_result = true; }
    else if(op == 15u) { result_type = wasm_type::i32; has_result = true; }
    else if((op >= 2u && op <= 4u) || (op >= 11u && op <= 13u))
    {
        if(field == nullptr) { return false; }
        auto const value_kind{field->storage.packed == gc_type::packed_kind::none ?
            field->storage.value.kind : gc_type::value_kind::i32};
        switch(value_kind)
        {
            case gc_type::value_kind::i32: result_type = wasm_type::i32; break;
            case gc_type::value_kind::i64: result_type = wasm_type::i64; break;
            case gc_type::value_kind::f32: result_type = wasm_type::f32; break;
            case gc_type::value_kind::f64: result_type = wasm_type::f64; break;
            case gc_type::value_kind::v128: result_type = wasm_type::v128; break;
            case gc_type::value_kind::reference:
            {
                // The validator and native ABI preserve distinct reference families.
                // [retained GC field metadata] field was proved non-null above.
                // [safe                     ] the immutable heap descriptor is borrowed; no cursor advances.
                auto const heap{field->storage.value.heap.code};
                result_type = heap == static_cast<::std::int_least64_t>(gc_type::abstract_heap_type::extern_) ||
                              heap == static_cast<::std::int_least64_t>(gc_type::abstract_heap_type::noextern) ? wasm_type::externref :
                              heap == static_cast<::std::int_least64_t>(gc_type::abstract_heap_type::exn) ||
                              heap == static_cast<::std::int_least64_t>(gc_type::abstract_heap_type::noexn) ?
                              static_cast<wasm_type>(0x69u) : wasm_type::funcref;
                break;
            }
        }
        has_result = true;
    }
    // [SSA stack prefix][input_count operands] end -> [SSA prefix][optional result] end.
    // [safe                                     ] no borrowed descriptor survives pop_back/vector growth.
    while(stack.size() > input_base) { stack.pop_back(); }
    if(has_result)
    {
        auto const llvm_result_type{get_llvm_type_from_wasm_value_type(context, result_type)};
        if(llvm_result_type == nullptr) { return false; }
        auto const loaded{builder.CreateLoad(llvm_result_type, output, get_llvm_string_ref(u8"gc.result"))};
        loaded->setAlignment(::llvm::Align{1u});
        stack.push_back({.type = result_type, .value = loaded});
    }
    return true;
}

#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
#include "sealed_compact_emit.h"
#include "sealed_local_table_emit.h"
#endif

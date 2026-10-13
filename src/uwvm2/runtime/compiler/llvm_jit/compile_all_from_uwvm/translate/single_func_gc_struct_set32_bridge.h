// Exact SET32=1 only. Five uintptr_t C arguments and one uintptr_t status;
// opaque reference/carrier authority is unchanged. No scratch/output buffer.
extern "C" [[nodiscard]] inline ::std::uintptr_t uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1(
    ::std::uintptr_t module_address, ::std::uintptr_t reference_kind,
    ::std::uintptr_t opaque_payload, ::std::uintptr_t field_index,
    ::std::uintptr_t raw_bits) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    using status = storage::gc_object_status;
    static_assert(sizeof(status) == sizeof(::std::uint32_t));
    static_assert(sizeof(::std::uintptr_t) >= sizeof(::std::uint32_t));
    // [five register-wide operands][numeric payload fields are at most u32]
    // [safe ] native misuse cannot silently truncate kind/index/value before
    // original store checks. Validated LLVM operands arrive zero-extended.
    if constexpr(sizeof(::std::uintptr_t) > sizeof(::std::uint32_t))
    {
        // Bitwise OR is equivalent to checking all three unsigned upper halves;
        // it cannot discard a high bit from any operand and needs one branch.
        if((reference_kind | field_index | raw_bits) > UINT32_MAX)
        { return static_cast<::std::uintptr_t>(status::invalid_value); }
    }
    if(module_address == 0u) { return static_cast<::std::uintptr_t>(status::invalid_value); }
    // [actual compiler relocation][synchronous owned module]
    // [safe ] Wasm operands cannot supply this argument. The original actual
    // runtime entry/source keeps this native module alive throughout the call.
    auto const* module{reinterpret_cast<storage::wasm_module_storage_t const*>(module_address)};
    auto* store{module->gc_store.get()};
    if(store == nullptr || !store->valid()) { return static_cast<::std::uintptr_t>(status::invalid_store); }
#if defined(UWVM_EXPERIMENTAL_MANAGED_NUMERIC_PAGE)
    // Preserve the SAME generic-struct retirement boundary. This leaf cannot
    // admit a provisional numeric-struct page or bypass outer root/admission.
    ::uwvm2::runtime::gc::managed_page_boundary(false);
#endif
    storage::gc_reference reference{};
    reference.kind = static_cast<::uwvm2::object::global::wasm_ref_kind>(static_cast<::std::uint32_t>(reference_kind));
    // [original opaque representation][no arithmetic/dereference]
    // [safe ] membership/kind/actual owner MUST be established in struct_set
    // before forming a struct address. This cast alone grants no authority.
    reference.storage.ptr = reinterpret_cast<void*>(opaque_payload);
    // [complete local value carrier][raw scalar bits; remaining bytes zero]
    // [safe ] numeric field eligibility is proved by the emitter. f32 uses
    // exact i32 representation bits; packed truncation, actual-object type,
    // canonical ownership, mutability, leases and locks remain in struct_set.
    auto const input{storage::gc_object_value::i32(static_cast<::std::uint32_t>(raw_bits))};
    return static_cast<::std::uintptr_t>(store->struct_set(reference, static_cast<::std::uint32_t>(field_index), input));
}

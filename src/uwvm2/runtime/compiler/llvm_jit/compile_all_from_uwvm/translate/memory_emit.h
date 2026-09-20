// Shared scalar/SIMD address lowering. A raw pointer is formed only after its reservation or bounds proof.
enum class llvm_jit_memory_protection
{
    software,
    partial_guard,
    full_wasm32_guard
};

// A faulting unaligned store may modify its in-bounds prefix on ARM/AArch64/PPC (and a legalized vector store
// may be several machine stores on any target). Wasm requires bounds failure BEFORE any byte is written.
// Only a store crossing a linear-memory page boundary can straddle the grow-only committed prefix. Probe its
// last byte first, with volatile ordering against the actual volatile store. Known-aligned accesses fold this
// entire diamond away; other page-local stores still perform one access and never load the memory length.
inline void emit_llvm_jit_guarded_store_preflight(::llvm::IRBuilder<>& b, ::llvm::Value* pointer,
    ::llvm::Value* effective_offset, ::std::size_t access_size, unsigned page_log2) noexcept
{
    if(access_size <= 1uz) { return; }
    auto function{b.GetInsertBlock()->getParent()};
    auto probe{::llvm::BasicBlock::Create(b.getContext(), "memory.store.cross_page", function)};
    auto cont{::llvm::BasicBlock::Create(b.getContext(), "memory.store.ready", function)};
    auto offset_type{effective_offset->getType()};
    auto constant{[&](::std::uint64_t x) { return ::llvm::ConstantInt::get(offset_type, x); }};
    auto page{::std::uint64_t{1u} << page_log2};
    auto crosses{access_size > page ? b.getTrue() : b.CreateICmpUGT(
        b.CreateAnd(effective_offset, constant(page - 1u)), constant(page - access_size))};
    b.CreateCondBr(crosses, probe, cont);
    b.SetInsertPoint(probe);
    auto last{b.CreateGEP(b.getInt8Ty(), pointer, b.getInt64(access_size - 1uz))};
    auto load{b.CreateLoad(b.getInt8Ty(), last, "memory.store.last_byte")};
    load->setVolatile(true);
    load->setAlignment(::llvm::Align{1u});
    b.CreateBr(cont);
    b.SetInsertPoint(cont);
}

template <typename LoadLength>
[[nodiscard]] inline ::llvm::Value* emit_llvm_jit_memory_address(
    ::llvm::IRBuilder<>& b, ::llvm::Value* base, ::llvm::Value* address,
    ::std::uint64_t static_offset, ::std::size_t access_size,
    llvm_jit_memory_protection protection, ::std::uint64_t partial_limit, LoadLength&& load_length) noexcept
{
    if(base == nullptr || address == nullptr || !address->getType()->isIntegerTy() || access_size == 0uz) { return nullptr; }
    auto address_bits{address->getType()->getIntegerBitWidth()};
    if(address_bits != 32u && address_bits != 64u) { return nullptr; }
    auto& module{*b.GetInsertBlock()->getModule()};
    auto pointer_bits{module.getDataLayout().getPointerSizeInBits()};
    if(pointer_bits != 32u && pointer_bits != 64u) { return nullptr; }
    if(address_bits > pointer_bits) { return nullptr; } // Memory64-on-ISA32 is not a supported addressing model.

    if(address_bits == 32u && static_offset > 0xffffffffull) { return nullptr; } // Never truncate an invalid memory32 memarg.
    auto dynamic{b.CreateZExtOrTrunc(address, b.getInt64Ty())};
    auto offset{b.CreateAdd(dynamic, b.getInt64(static_offset), "memory.effective")};

#if defined(UWVM_SUPPORT_MMAP)
    constexpr auto guard_width{::uwvm2::object::memory::linear::mmap_guard_max_access_size};
#else
    constexpr ::std::size_t guard_width{};
#endif
    if(access_size > guard_width ||
       (protection == llvm_jit_memory_protection::full_wasm32_guard && (address_bits != 32u || pointer_bits != 64u)))
    { protection = llvm_jit_memory_protection::software; }

    ::llvm::Value* overflow{};
    if(protection != llvm_jit_memory_protection::full_wasm32_guard)
    {
        // Future memory64 must test the carry, not merely the wrapped low 64 bits.
        overflow = address_bits == 32u ? b.CreateICmpUGT(offset, b.getInt64(0xffffffffull))
                                     : b.CreateICmpULT(offset, dynamic, "memory.carry");
    }

    auto const check{[&]() noexcept -> bool
    {
        auto length{load_length()};
        if(length == nullptr) { return false; }
        auto length64{b.CreateZExtOrTrunc(length, b.getInt64Ty())};
        auto width{b.getInt64(access_size)};
        auto small{b.CreateICmpULT(length64, width)};
        auto outside{b.CreateICmpUGT(offset, b.CreateSub(length64, width))};
        emit_llvm_conditional_memory_out_of_bounds_trap(module, b,
            b.CreateOr(overflow, b.CreateOr(small, outside)), 0uz, static_offset, offset, overflow, length, access_size);
        return true;
    }};
    if(protection == llvm_jit_memory_protection::software)
    {
        if(!check()) { return nullptr; }
    }
    else if(protection == llvm_jit_memory_protection::partial_guard)
    {
        auto function{b.GetInsertBlock()->getParent()};
        auto slow{::llvm::BasicBlock::Create(b.getContext(), "memory.partial.check", function)};
        auto cont{::llvm::BasicBlock::Create(b.getContext(), "memory.partial.cont", function)};
        // A carry must enter the slow path even when the wrapped offset appears to lie in the protected prefix.
        b.CreateCondBr(b.CreateOr(overflow, b.CreateICmpUGE(offset, b.getInt64(partial_limit))), slow, cont);
        b.SetInsertPoint(slow);
        if(!check()) { return nullptr; }
        b.CreateBr(cont);
        b.SetInsertPoint(cont);
    }
    // The full memory32 reservation owns every u32+u32 offset plus the largest supported access. No length load,
    // carry test, or software branch is emitted on that path. Never mark the deliberately guard-addressable GEP inbounds.
    auto index{b.CreateZExtOrTrunc(offset, b.getIntNTy(pointer_bits))};
    return b.CreateGEP(b.getInt8Ty(), base, index, "memory.addr");
}

[[nodiscard]] inline ::llvm::Value* emit_llvm_jit_memory_length(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    auto& info{state.memory0_access_info};
    auto module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr || info.memory_p == nullptr) { return nullptr; }
    auto& b{*state.ir_builder};
    auto type{b.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    ::std::uintptr_t slot{};
    if constexpr(runtime_native_memory_t::can_mmap) { slot = reinterpret_cast<::std::uintptr_t>(info.stable_memory_length_p); }
    else { slot = reinterpret_cast<::std::uintptr_t>(info.stable_memory_length_value_p); }
    auto name{::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(*module), u8"_memory0_length")};
    auto pointer{get_llvm_external_host_object_pointer(b, slot, type, ::uwvm2::utils::container::u8string_view{name.data(), name.size()})};
    if(pointer == nullptr) { return nullptr; }
    auto load{b.CreateLoad(type, pointer, "memory.length")};
    load->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
    if constexpr(runtime_native_memory_t::can_mmap) { load->setAtomic(::llvm::AtomicOrdering::Acquire); }
    return load;
}

[[nodiscard]] inline ::llvm::Value* emit_llvm_jit_direct_memory_pointer(runtime_local_func_llvm_jit_emit_state_t& state,
    ::std::uint64_t static_offset, ::std::size_t access_size, ::llvm::Value* address, bool is_store = false) noexcept
{
    if(state.ir_builder == nullptr || state.local_func_storage_ptr == nullptr) { return nullptr; }
    auto module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr) { return nullptr; }
    if(!state.memory0_access_info_resolved)
    {
        state.memory0_access_info = resolve_runtime_memory_access_info(*module, 0u);
        state.memory0_access_info_resolved = true;
    }
    auto const& info{state.memory0_access_info};
    if(info.memory_p == nullptr) { return nullptr; } // Provider-owned accesses must keep their callback/lock lifetime.
    if constexpr(!runtime_native_memory_t::can_mmap && runtime_native_memory_t::support_multi_thread)
    { return nullptr; } // Moving shared allocations need a pin spanning the actual load/store, not just a snapshot.

    auto& b{*state.ir_builder};
    ::llvm::Value* base{};
    auto protection{llvm_jit_memory_protection::software};
    auto name{::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(*module), u8"_memory0_begin")};
    if constexpr(runtime_native_memory_t::can_mmap)
    {
        base = get_llvm_external_host_byte_span_pointer(b, reinterpret_cast<::std::uintptr_t>(info.stable_memory_begin),
                                                       info.stable_memory_reserved_span_bytes, ::uwvm2::utils::container::u8string_view{name.data(), name.size()});
        if(!info.mmap_requires_dynamic_bounds)
        {
            if(info.mmap_covers_wasm32_effective_domain) { protection = llvm_jit_memory_protection::full_wasm32_guard; }
            else if(info.mmap_uses_partial_protection) { protection = llvm_jit_memory_protection::partial_guard; }
        }
    }
    else
    {
        // Single-thread realloc can move the base at a Wasm/host call. Load the pointer SLOT rather than freezing the
        // current allocation in the native object. LLVM can reuse this load only where intervening writes permit it.
        auto pointer_type{get_llvm_pointer_type(b.getInt8Ty())};
        auto slot{get_llvm_external_host_object_pointer(b, reinterpret_cast<::std::uintptr_t>(info.memory_begin_value_p),
                                                       pointer_type, ::uwvm2::utils::container::u8string_view{name.data(), name.size()})};
        if(slot == nullptr) { return nullptr; }
        auto load{b.CreateLoad(pointer_type, slot, "memory.base")};
        load->setAlignment(::llvm::Align{alignof(::std::uintptr_t)});
        base = load;
    }
    auto pointer{emit_llvm_jit_memory_address(b, base, address, static_offset, access_size, protection,
                                        get_runtime_partial_protection_limit_escape_offset(), [&]() noexcept { return emit_llvm_jit_memory_length(state); })};
    if(pointer != nullptr && is_store && protection != llvm_jit_memory_protection::software)
    {
        auto effective{b.CreateAdd(b.CreateZExtOrTrunc(address, b.getInt64Ty()), b.getInt64(static_offset))};
        emit_llvm_jit_guarded_store_preflight(b, pointer, effective, access_size, info.custom_page_size_log2);
    }
    return pointer;
}

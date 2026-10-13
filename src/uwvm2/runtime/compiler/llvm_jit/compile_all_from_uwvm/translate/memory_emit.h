// Shared scalar/SIMD address lowering. A raw pointer is formed only after its reservation or bounds proof.
// Only an immutable declared minimum is a lifetime-safe bound: a grown size may
// shrink on checkpoint restore. Imported/provider memories keep minimum zero.
// This proof does not assume any host access width is atomic or naturally aligned.
[[nodiscard]] inline bool llvm_jit_memory_span_within_declared_minimum(
    runtime_memory_access_info_t const& info, ::llvm::Value* address,
    ::std::uint64_t static_offset, ::std::size_t access_size) noexcept
{
    if(address == nullptr || access_size == 0uz ||
       (!address->getType()->isIntegerTy(32u) && !address->getType()->isIntegerTy(64u))) { return false; }
    ::llvm::ConstantInt* bound{::llvm::dyn_cast<::llvm::ConstantInt>(address)};
    if(bound == nullptr)
    {
        // x & mask is unsigned and never exceeds mask, including for negative x.
        // Other dynamic expressions keep the existing fault/bounds path.
        if(auto const operation{::llvm::dyn_cast<::llvm::BinaryOperator>(address)};
           operation != nullptr && operation->getOpcode() == ::llvm::Instruction::And)
        {
            bound = ::llvm::dyn_cast<::llvm::ConstantInt>(operation->getOperand(0u));
            if(bound == nullptr) { bound = ::llvm::dyn_cast<::llvm::ConstantInt>(operation->getOperand(1u)); }
        }
    }
    if(bound == nullptr) { return false; }
    auto const maximum{bound->getZExtValue()};
    auto const minimum{info.declared_minimum_byte_length};
    return maximum <= minimum && static_offset <= minimum - maximum &&
        access_size <= minimum - maximum - static_offset;
}


enum class llvm_jit_memory_protection
{
    software,
    partial_guard,
    full_wasm32_guard
};

// Memory32 and memory64 share atomic alignment and value lowering. Alignment
// uses only the low address bits; the address emitter separately proves the
// complete 33/65-bit sum before forming a native pointer.
[[nodiscard]] inline bool emit_llvm_jit_atomic_alignment(::llvm::IRBuilder<>& b, ::llvm::Value* address,
    ::std::uint64_t offset, ::std::size_t bytes) noexcept
{
    if(address == nullptr || (!address->getType()->isIntegerTy(32u) && !address->getType()->isIntegerTy(64u)) ||
       (bytes != 1uz && bytes != 2uz && bytes != 4uz && bytes != 8uz)) { return false; }
    auto const type{address->getType()};
    if(type->isIntegerTy(32u) && offset > 0xffff'ffffull) { return false; }
    if(bytes != 1uz)
    {
        auto const effective{b.CreateAdd(address, ::llvm::ConstantInt::get(type, offset))};
        auto const misaligned{b.CreateICmpNE(b.CreateAnd(effective, ::llvm::ConstantInt::get(type, bytes - 1uz)), ::llvm::ConstantInt::get(type, 0u))};
        emit_llvm_conditional_trap(*b.GetInsertBlock()->getModule(), b, misaligned, ::uwvm2::runtime::lib::llvm_jit_trap_kind::unaligned_atomic);
    }
    return true;
}

// pointer borrows the result of the complete address/bounds and dynamic
// alignment proofs. These helpers never form a host pointer from Wasm bits.
[[nodiscard]] inline ::llvm::Value* emit_llvm_jit_atomic_load(::llvm::IRBuilder<>& b, ::llvm::Value* pointer,
    ::std::size_t bytes, ::llvm::IntegerType* result_type) noexcept
{
    if(pointer == nullptr || result_type == nullptr || (bytes != 1uz && bytes != 2uz && bytes != 4uz && bytes != 8uz) ||
       (result_type->getBitWidth() != 32u && result_type->getBitWidth() != 64u) || bytes * 8uz > result_type->getBitWidth()) { return nullptr; }
    auto const loaded{b.CreateLoad(b.getIntNTy(static_cast<unsigned>(bytes * 8uz)), pointer, "atomic.load")};
    loaded->setAtomic(::llvm::AtomicOrdering::SequentiallyConsistent);
    loaded->setAlignment(::llvm::Align{bytes});
    ::llvm::Value* value{loaded}; // borrows the newly created, function-owned IR node
    if(bytes != 1uz && b.GetInsertBlock()->getModule()->getDataLayout().isBigEndian())
    { value = b.CreateUnaryIntrinsic(::llvm::Intrinsic::bswap, value); }
    // value now borrows the endian-adjusted node, with the same checked width.
    return b.CreateZExtOrTrunc(value, result_type);
}

[[nodiscard]] inline bool emit_llvm_jit_atomic_store(::llvm::IRBuilder<>& b, ::llvm::Value* pointer,
    ::std::size_t bytes, ::llvm::Value* input) noexcept
{
    if(pointer == nullptr || input == nullptr || (bytes != 1uz && bytes != 2uz && bytes != 4uz && bytes != 8uz) ||
       (!input->getType()->isIntegerTy(32u) && !input->getType()->isIntegerTy(64u)) || bytes * 8uz > input->getType()->getIntegerBitWidth()) { return false; }
    auto value{b.CreateTruncOrBitCast(input, b.getIntNTy(static_cast<unsigned>(bytes * 8uz)))};
    // value borrows the function-owned truncated operand; narrow stores wrap.
    if(bytes != 1uz && b.GetInsertBlock()->getModule()->getDataLayout().isBigEndian())
    { value = b.CreateUnaryIntrinsic(::llvm::Intrinsic::bswap, value); }
    // value now borrows the endian-adjusted operand; pointer retains both proofs.
    auto const stored{b.CreateStore(value, pointer)};
    stored->setAtomic(::llvm::AtomicOrdering::SequentiallyConsistent);
    stored->setAlignment(::llvm::Align{bytes});
    return true;
}

// Shared memory32/memory64 RMW lowering. The caller has established the
// full effective-address range and natural alignment before passing pointer.
// Operand narrowing is Wasm wrapping; every old result is zero-extended.
[[nodiscard]] inline ::llvm::Value* emit_llvm_jit_atomic_rmw(::llvm::IRBuilder<>& b, ::llvm::Value* pointer,
    ::uwvm2::runtime::compiler::shared::wasm_threads::atomic_rmw_operation operation,
    ::std::size_t bytes, ::llvm::Value* input, ::llvm::Value* expected_input = nullptr) noexcept
{
    using atomic_rmw_operation = ::uwvm2::runtime::compiler::shared::wasm_threads::atomic_rmw_operation;
    if(pointer == nullptr || input == nullptr || (bytes != 1uz && bytes != 2uz && bytes != 4uz && bytes != 8uz) ||
       (!input->getType()->isIntegerTy(32u) && !input->getType()->isIntegerTy(64u)) || bytes * 8uz > input->getType()->getIntegerBitWidth() ||
       operation > atomic_rmw_operation::compare_exchange) { return nullptr; }
    if(operation == atomic_rmw_operation::compare_exchange &&
       (expected_input == nullptr || expected_input->getType() != input->getType())) { return nullptr; }
    auto const result_type{input->getType()};
    auto const access_type{b.getIntNTy(static_cast<unsigned>(bytes * 8uz))};
    auto const operand{b.CreateTruncOrBitCast(input, access_type)};
    bool const swap{bytes != 1uz && b.GetInsertBlock()->getModule()->getDataLayout().isBigEndian()};
    auto endian{[&](::llvm::Value* value) noexcept -> ::llvm::Value*
    { return swap ? b.CreateUnaryIntrinsic(::llvm::Intrinsic::bswap, value) : value; }};
    ::llvm::Value* old{};
    if(operation == atomic_rmw_operation::compare_exchange)
    {
        auto const expected{b.CreateTruncOrBitCast(expected_input, access_type)};
        auto const exchanged{b.CreateAtomicCmpXchg(pointer, endian(expected), endian(operand), ::llvm::Align{bytes},
            ::llvm::AtomicOrdering::SequentiallyConsistent, ::llvm::AtomicOrdering::SequentiallyConsistent)};
        exchanged->setWeak(false);
        old = b.CreateExtractValue(exchanged, 0u);
        // old is the checked atomic instruction's first result, owned by this IR function.
    }
    else if(swap && (operation == atomic_rmw_operation::add ||
                     operation == atomic_rmw_operation::sub))
    {
        // Native addition does not commute with endian reversal.
        // Retry a strong CAS, recomputing guest arithmetic from the observed value.
        auto const initial{b.CreateLoad(access_type, pointer, "atomic.rmw.initial")};
        initial->setAtomic(::llvm::AtomicOrdering::SequentiallyConsistent);
        initial->setAlignment(::llvm::Align{bytes});
        auto const entry{b.GetInsertBlock()};
        auto const function{entry->getParent()};
        auto const loop{::llvm::BasicBlock::Create(b.getContext(), "atomic.rmw.retry", function)};
        auto const done{::llvm::BasicBlock::Create(b.getContext(), "atomic.rmw.done", function)};
        b.CreateBr(loop);
        b.SetInsertPoint(loop);
        auto const observed{b.CreatePHI(access_type, 2u, "atomic.rmw.observed")};
        observed->addIncoming(initial, entry);
        auto const guest{endian(observed)};
        auto const next{operation == atomic_rmw_operation::add ?
            b.CreateAdd(guest, operand) : b.CreateSub(guest, operand)};
        auto const exchanged{b.CreateAtomicCmpXchg(pointer, observed, endian(next), ::llvm::Align{bytes},
            ::llvm::AtomicOrdering::SequentiallyConsistent, ::llvm::AtomicOrdering::SequentiallyConsistent)};
        exchanged->setWeak(false);
        old = b.CreateExtractValue(exchanged, 0u);
        // old borrows this iteration's observed native bit pattern.
        observed->addIncoming(old, loop);
        b.CreateCondBr(b.CreateExtractValue(exchanged, 1u), done, loop);
        b.SetInsertPoint(done);
    }
    else
    {
        ::llvm::AtomicRMWInst::BinOp native_operation{};
        switch(operation)
        {
            case atomic_rmw_operation::add: native_operation = ::llvm::AtomicRMWInst::Add; break;
            case atomic_rmw_operation::sub: native_operation = ::llvm::AtomicRMWInst::Sub; break;
            case atomic_rmw_operation::and_: native_operation = ::llvm::AtomicRMWInst::And; break;
            case atomic_rmw_operation::or_: native_operation = ::llvm::AtomicRMWInst::Or; break;
            case atomic_rmw_operation::xor_: native_operation = ::llvm::AtomicRMWInst::Xor; break;
            case atomic_rmw_operation::exchange: native_operation = ::llvm::AtomicRMWInst::Xchg; break;
            default: return nullptr;
        }
        old = b.CreateAtomicRMW(native_operation, pointer, endian(operand), ::llvm::Align{bytes}, ::llvm::AtomicOrdering::SequentiallyConsistent);
        // old is the new atomic IR instruction's original-memory result.
    }
    return b.CreateZExtOrTrunc(endian(old), result_type);
}

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
    llvm_jit_memory_protection protection, ::std::uint64_t partial_limit, LoadLength&& load_length, ::std::size_t memory_index = 0uz) noexcept
{
    if(base == nullptr || address == nullptr || !address->getType()->isIntegerTy() || access_size == 0uz) { return nullptr; }
    auto address_bits{address->getType()->getIntegerBitWidth()};
    if(address_bits != 32u && address_bits != 64u) { return nullptr; }
    auto& module{*b.GetInsertBlock()->getModule()};
    auto pointer_bits{module.getDataLayout().getPointerSizeInBits()};
    if(pointer_bits != 32u && pointer_bits != 64u) { return nullptr; }

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
        // Memory64 requires the 65th address bit even on ISA32: carry must trap
        // before any conversion to the native pointer width.
        overflow = address_bits == 32u ? b.CreateICmpUGT(offset, b.getInt64(0xffffffffull))
                                     : b.CreateICmpULT(offset, dynamic, "memory.carry");
    }

    auto const check{[&]() noexcept -> bool
    {
        auto length{load_length()};
        if(length == nullptr) { return false; }
        // The native memory extent is size_t. Reject an inconsistent bridge ABI
        // before using its range as a proof for a narrower host pointer.
        if(!length->getType()->isIntegerTy(pointer_bits)) { return false; }
        auto length64{b.CreateZExtOrTrunc(length, b.getInt64Ty())};
        auto width{b.getInt64(access_size)};
        auto small{b.CreateICmpULT(length64, width)};
        auto outside{b.CreateICmpUGT(offset, b.CreateSub(length64, width))};
        emit_llvm_conditional_memory_out_of_bounds_trap(module, b,
            b.CreateOr(overflow, b.CreateOr(small, outside)), memory_index, static_offset, offset, overflow, length, access_size);
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
    // [base ... committed/reserved extent) | outside host object
    //          ^^ offset: software bounds prove the complete access fits size_t;
    // partial guards prove offset < their host-representable reservation limit.
    // Only now may memory64 on ISA32 discard high bits, which the proof made zero.
    auto index{b.CreateZExtOrTrunc(offset, b.getIntNTy(pointer_bits))};
    return b.CreateGEP(b.getInt8Ty(), base, index, "memory.addr");
}

[[nodiscard]] inline ::llvm::Value* emit_llvm_jit_memory_length(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    auto& info{state.selected_memory_access_info};
    auto module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr || info.memory_p == nullptr) { return nullptr; }
    auto& b{*state.ir_builder};
    auto type{b.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    ::std::uintptr_t slot{};
    if constexpr(runtime_native_memory_t::can_mmap) { slot = reinterpret_cast<::std::uintptr_t>(info.stable_memory_length_p); }
    else { slot = reinterpret_cast<::std::uintptr_t>(info.stable_memory_length_value_p); }
    auto name{::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(*module), u8"_memory", state.current_memory_index, u8"_length")};
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
    if(!state.selected_memory_access_info_resolved)
    {
        state.selected_memory_access_info = resolve_runtime_memory_access_info(*module, state.current_memory_index);
        state.selected_memory_access_info_resolved = true;
    }
    auto const& info{state.selected_memory_access_info};
    if(info.memory_p == nullptr) { return nullptr; } // Provider-owned accesses must keep their callback/lock lifetime.
    if constexpr(!runtime_native_memory_t::can_mmap && runtime_native_memory_t::support_multi_thread)
    { return nullptr; } // Moving shared allocations need a pin spanning the actual load/store, not just a snapshot.

    auto& b{*state.ir_builder};
    ::llvm::Value* base{};
    auto protection{llvm_jit_memory_protection::software};
    auto name{::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(*module), u8"_memory", state.current_memory_index, u8"_begin")};
    if constexpr(runtime_native_memory_t::can_mmap)
    {
        base = get_llvm_external_host_byte_span_pointer(b, reinterpret_cast<::std::uintptr_t>(info.stable_memory_begin),
                                                       info.stable_memory_reserved_span_bytes, ::uwvm2::utils::container::u8string_view{name.data(), name.size()});
        // Instrumented Wasm failures must reach the cold runtime observer,
        // outside an OS signal handler. Ordinary JIT keeps its guard strategy.
        if(!state.emit_debug_safe_points && !info.mmap_requires_dynamic_bounds)
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
                                        get_runtime_partial_protection_limit_escape_offset(), [&]() noexcept { return emit_llvm_jit_memory_length(state); }, state.current_memory_index)};
    if(pointer != nullptr && is_store && protection != llvm_jit_memory_protection::software)
    {
        auto effective{b.CreateAdd(b.CreateZExtOrTrunc(address, b.getInt64Ty()), b.getInt64(static_offset))};
        // Proven in-bounds stores need no last-byte probe. The actual store
        // remains volatile, preserving guest state before a later trap/callback.
        if(!llvm_jit_memory_span_within_declared_minimum(info, address, static_offset, access_size))
        { emit_llvm_jit_guarded_store_preflight(b, pointer, effective, access_size, info.custom_page_size_log2); }
    }
    return pointer;
}

#pragma once
// Private observer numeric cells updated by actual executed assignments.
// No slot, policy stamp or compiler DATA grants native/GC/restore admission.
[[nodiscard]] inline constexpr bool checkpoint_canonical_scalar(runtime_operand_stack_value_type type) noexcept
{
    auto const kind{get_runtime_wasm_value_type_encoding(type)};
    return kind == 0x7fu || kind == 0x7eu || kind == 0x7du || kind == 0x7cu;
}
[[nodiscard]] inline bool checkpoint_canonical_local_owner(runtime_local_func_llvm_jit_emit_state_t const& state) noexcept
{
    auto const count{state.local_types.size()};
    if(count == 0u || count > 8u || count != state.local_pointers.size() || state.llvm_function == nullptr ||
       state.llvm_module == nullptr || state.ir_builder == nullptr || state.llvm_function->getParent() != state.llvm_module ||
       state.ir_builder->GetInsertBlock() == nullptr || state.ir_builder->GetInsertBlock()->getParent() != state.llvm_function)
    { return false; }
    auto const owner{::llvm::dyn_cast_or_null<::llvm::IntToPtrInst>(state.checkpoint_observer_workspace)};
    auto const allocation{state.checkpoint_observer_workspace_allocation};
    auto const bytes{checkpoint_observer_allocation_extent(state)};
    auto const& layout{state.llvm_module->getDataLayout()};
    namespace cp = ::uwvm2::runtime::checkpoint;
    return owner != nullptr && allocation != nullptr && bytes != nullptr &&
        owner->getFunction() == state.llvm_function && allocation->getFunction() == state.llvm_function &&
        owner->getParent() == ::std::addressof(state.llvm_function->getEntryBlock()) &&
        allocation->getParent() == owner->getParent() && checkpoint_observer_workspace_result_matches(state,owner->getOperand(0u)) && allocation->comesBefore(owner) &&
        cp::observer_workspace_fits(count,count) &&
        bytes->getZExtValue() >= count * (cp::observer_local_metadata_bytes + cp::native_slot_bytes) &&
        !layout.isDefault() && layout.isLittleEndian() && layout.getPointerSizeInBits() == 64u &&
        state.llvm_module->getTargetTriple().getArch() == ::llvm::Triple::x86_64 &&
        state.llvm_module->getTargetTriple().isOSLinux();
}
[[nodiscard]] inline bool prepare_runtime_local_func_llvm_jit_canonical_numeric_locals(
    runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
#if defined(__linux__) && defined(__x86_64__)
    namespace cp = ::uwvm2::runtime::checkpoint;
    auto const plan{state.checkpoint_plan};
    if(plan == nullptr || !plan->profile || plan->profile->purpose() != cp::compilation_purpose::observe_values ||
       state.local_types.empty() || state.local_types.size() > 8u) { return true; }
    if(state.checkpoint_observer_canonical_numeric_locals || state.checkpoint_observer_local_epoch != nullptr)
    { return false; }
    if(!checkpoint_canonical_local_owner(state) || cp::observation_probe_hints::full_token_address() == nullptr) { return true; }
    bool scalar{}, reference{};
    for(::std::size_t i{}; i != state.local_types.size(); ++i)
    {
        auto const carrier{state.local_types.index_unchecked(i)};
        auto const kind{get_runtime_wasm_value_type_encoding(carrier)};
        auto const type{get_llvm_type_from_wasm_value_type(state.ir_builder->getContext(),carrier)};
        bool const numeric{checkpoint_canonical_scalar(carrier)};
        if(!numeric && kind != 0x7bu && kind != 0x70u && kind != 0x6fu) { return true; }
        auto const local{state.local_pointers.index_unchecked(i)};
        if(local == nullptr || !local->isStaticAlloca() || local->isArrayAllocation() ||
           local->getFunction() != state.llvm_function || type == nullptr || local->getAllocatedType() != type)
        { return false; }
        scalar = scalar || numeric;reference = reference || kind == 0x70u || kind == 0x6fu;
    }
    // Pure numeric prefixes already benefit from the existing SSA copier.
    // Direct cell maintenance is selected only for small mixed reference prefixes.
    if(!scalar || !reference) { return true; }
    auto& builder{*state.ir_builder};
    auto const stamp{create_llvm_jit_entry_block_alloca(builder,builder.getInt64Ty(),nullptr,"checkpoint.canonical.locals.epoch")};
    if(stamp == nullptr) { return false; }
    stamp->setAlignment(::llvm::Align{8u});
    builder.CreateStore(builder.getInt64(0u),stamp)->setAlignment(::llvm::Align{8u});
    state.checkpoint_observer_local_epoch = stamp;state.checkpoint_observer_canonical_numeric_locals = true;
#endif
    return true;
}
[[nodiscard]] inline bool update_runtime_local_func_llvm_jit_canonical_numeric_local(
    runtime_local_func_llvm_jit_emit_state_t const& state, ::std::size_t index) noexcept
{
    if(!state.checkpoint_observer_canonical_numeric_locals) { return state.checkpoint_observer_local_epoch == nullptr; }
    if(!checkpoint_canonical_local_owner(state) || index >= state.local_types.size()) { return false; }
    auto const carrier{state.local_types.index_unchecked(index)};
    if(!checkpoint_canonical_scalar(carrier)) { return true; } // every reference/vector still refreshes at each capture
    auto const local{state.local_pointers.index_unchecked(index)};
    auto const native{get_llvm_type_from_wasm_value_type(state.ir_builder->getContext(),carrier)};
    if(local == nullptr || !local->isStaticAlloca() || local->isArrayAllocation() ||
       local->getFunction() != state.llvm_function || local->getAllocatedType() != native) { return false; }
    auto& builder{*state.ir_builder};
    auto const bits{builder.getIntNTy(static_cast<unsigned>(get_runtime_wasm_value_type_abi_size(carrier)*CHAR_BIT))};
    auto const live{builder.CreateLoad(native,local)};
    auto const whole{builder.CreateZExtOrBitCast(builder.CreateBitCast(live,bits),builder.getInt128Ty())};
    namespace cp = ::uwvm2::runtime::checkpoint;
    auto const count{state.local_types.size()};
    // Exact function-owned allocator count/prefix and original local ordinal
    // were checked above BEFORE deriving either complete 16-byte cell or flag.
    auto const cell{builder.CreateInBoundsGEP(builder.getInt8Ty(),state.checkpoint_observer_workspace,
        builder.getInt64(count*cp::observer_local_metadata_bytes + index*cp::native_slot_bytes))};
    builder.CreateStore(whole,cell)->setAlignment(::llvm::Align{1u});
    auto const flag{builder.CreateInBoundsGEP(builder.getInt8Ty(),state.checkpoint_observer_workspace,
        builder.getInt64(count+index))};
    builder.CreateStore(builder.getInt8(1u),flag)->setAlignment(::llvm::Align{1u});
    return true;
}
[[nodiscard]] inline bool reset_runtime_local_func_llvm_jit_canonical_numeric_locals(
    runtime_local_func_llvm_jit_emit_state_t const& state) noexcept
{
    if(!state.checkpoint_observer_canonical_numeric_locals) { return state.checkpoint_observer_local_epoch == nullptr; }
    for(::std::size_t i{}; i != state.local_types.size(); ++i)
    { if(!update_runtime_local_func_llvm_jit_canonical_numeric_local(state,i)) { return false; } }
    return true;
}
template<typename Capture>
[[nodiscard]] inline bool emit_runtime_local_func_llvm_jit_canonical_local_refresh(
    runtime_local_func_llvm_jit_emit_state_t& state, ::llvm::Value*& current_token, Capture&& capture) noexcept
{
    if(!state.checkpoint_observer_canonical_numeric_locals || state.checkpoint_observer_controls == nullptr)
    { return capture(false); } // initial unbound entry and every unsupported engine retain the original complete copy
    auto const stamp{state.checkpoint_observer_local_epoch};
    if(state.checkpoint_observer_controls->plan != state.checkpoint_plan ||
       !checkpoint_canonical_local_owner(state) || stamp == nullptr || stamp->getFunction() != state.llvm_function ||
       !stamp->isStaticAlloca() || stamp->isArrayAllocation() || !stamp->getAllocatedType()->isIntegerTy(64u)) { return false; }
    auto const address{::uwvm2::runtime::checkpoint::observation_probe_hints::full_token_address()};
    if(address == nullptr) { return false; }
    auto& builder{*state.ir_builder};
    auto const pointer{::llvm::ConstantExpr::getIntToPtr(::llvm::ConstantInt::get(builder.getInt64Ty(),
        reinterpret_cast<::std::uintptr_t>(address)),builder.getPtrTy())};
    auto const token{builder.CreateLoad(builder.getInt64Ty(),pointer,"checkpoint.canonical.locals.current.epoch")};
    token->setAtomic(::llvm::AtomicOrdering::Acquire);token->setVolatile(true);token->setAlignment(::llvm::Align{8u});
    current_token = token;
    auto const previous{builder.CreateLoad(builder.getInt64Ty(),stamp)};previous->setAlignment(::llvm::Align{8u});
    // Executed scalar assignments maintain complete canonical cells even when
    // gates skip collection. Every debugger pause/resume republishes policy;
    // that mismatch (or zero exhaustion) refreshes original actual native locals.
    auto const reuse{builder.CreateAnd(builder.CreateICmpNE(token,builder.getInt64(0u)),builder.CreateICmpEQ(token,previous))};
    auto const full{::llvm::BasicBlock::Create(builder.getContext(),"checkpoint.canonical.locals.full",state.llvm_function)};
    auto const refs{::llvm::BasicBlock::Create(builder.getContext(),"checkpoint.canonical.locals.references",state.llvm_function)};
    auto const ready{::llvm::BasicBlock::Create(builder.getContext(),"checkpoint.canonical.locals.ready",state.llvm_function)};
    builder.CreateCondBr(reuse,refs,full);builder.SetInsertPoint(full);
    if(!capture(false)) { return false; }builder.CreateBr(ready);builder.SetInsertPoint(refs);
    if(!capture(true)) { return false; }builder.CreateBr(ready);builder.SetInsertPoint(ready);
    builder.CreateStore(token,stamp)->setAlignment(::llvm::Align{8u});return true;
}

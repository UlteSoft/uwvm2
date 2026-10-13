#pragma once
// Only compiler-owned IR shape validation; DATA never supplies VM authority.
// Both the original allocator and the fused observer entry return a private
// workspace whose complete extent remains the first compiler constant.
[[nodiscard]] inline ::llvm::ConstantInt* checkpoint_observer_allocation_extent(
    runtime_local_func_llvm_jit_emit_state_t const& state) noexcept
{
    auto const allocation{state.checkpoint_observer_workspace_allocation};
    if(allocation == nullptr || allocation->getFunction() != state.llvm_function) { return nullptr; }
    if(allocation->arg_size() != 1u && allocation->arg_size() != 4u) { return nullptr; }
    return ::llvm::dyn_cast<::llvm::ConstantInt>(allocation->getArgOperand(0u));
}
[[nodiscard]] inline bool checkpoint_observer_workspace_result_matches(
    runtime_local_func_llvm_jit_emit_state_t const& state, ::llvm::Value* value) noexcept
{
    auto const allocation{state.checkpoint_observer_workspace_allocation};
    if(checkpoint_observer_allocation_extent(state) == nullptr) { return false; }
    if(allocation->arg_size() == 1u) { return value == allocation; }
#if defined(__linux__) && defined(__x86_64__)
    auto const workspace{::llvm::dyn_cast_or_null<::llvm::ExtractValueInst>(value)};
    auto const token{::llvm::dyn_cast_or_null<::llvm::ExtractValueInst>(state.debug_activation_token)};
    auto const result{::llvm::dyn_cast<::llvm::StructType>(allocation->getType())};
    auto const local{state.local_func_storage_ptr};
    if(workspace == nullptr || token == nullptr || result == nullptr || result->isOpaque() ||
       result->getNumElements() != 2u || !result->getElementType(0u)->isIntegerTy(64u) ||
       !result->getElementType(1u)->isIntegerTy(64u) || state.llvm_module == nullptr || local == nullptr ||
       state.llvm_module->getDataLayout().isDefault() || !state.llvm_module->getDataLayout().isLittleEndian() ||
       state.llvm_module->getDataLayout().getPointerSizeInBits() != 64u ||
       state.llvm_module->getTargetTriple().getArch() != ::llvm::Triple::x86_64 ||
       !state.llvm_module->getTargetTriple().isOSLinux() ||
       workspace->getAggregateOperand() != allocation || token->getAggregateOperand() != allocation ||
       workspace->getNumIndices() != 1u || token->getNumIndices() != 1u ||
       workspace->getIndices()[0u] != 1u || token->getIndices()[0u] != 0u ||
       workspace->getParent() != allocation->getParent() || token->getParent() != allocation->getParent() ||
       !allocation->comesBefore(workspace) || !allocation->comesBefore(token)) { return false; }
    ::std::uint64_t const expected[]{local->module_id,local->function_index,state.debug_compiled_function_generation};
    for(unsigned i{};i!=3u;++i)
    {
        auto const actual{::llvm::dyn_cast<::llvm::ConstantInt>(allocation->getArgOperand(i+1u))};
        if(actual == nullptr || actual->getBitWidth() != 64u || actual->getZExtValue() != expected[i]) { return false; }
    }
    return state.debug_activation_enabled && state.emit_debug_safe_points;
#else
    (void)value;return false;
#endif
}

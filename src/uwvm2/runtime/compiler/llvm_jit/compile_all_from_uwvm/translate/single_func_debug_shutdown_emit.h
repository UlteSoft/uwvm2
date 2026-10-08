#pragma once
// Include after the qualified native exception/cleanup emitter. Existing
// noexcept/naked debugger bridges have ALREADY returned before this Invoke.
[[nodiscard]] inline bool emit_runtime_local_func_llvm_jit_debug_shutdown_poll(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t opcode_offset,
    ::llvm::CallBase* actual_safe_point) noexcept
{
    if(!state.emit_debug_safe_points) { return true; } // normal/null: zero IR
#if defined(UWVM_CPP_EXCEPTIONS) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    if(!state.native_guest_exceptions || state.pending_numeric_plan != nullptr || !state.debug_activation_enabled)
    { return true; } // actual final materializer will report cleanup unavailable
    if(state.ir_builder == nullptr || state.local_func_storage_ptr == nullptr ||
       state.debug_activation_token == nullptr || actual_safe_point == nullptr || !actual_safe_point->doesNotThrow())
    { return false; }
    auto& builder{*state.ir_builder};
    // This private poll splits the current opcode's block without changing
    // Wasm SSA or its observer packet. Both normal paths still descend from
    // the actual safepoint block; throwing paths retire the activation instead
    // of reaching the successor. Only this freshly constructed compiler edge
    // may carry numeric packet witnesses across blocks. Wasm branches never do.
    bool const carry_observer_packet{checkpoint_observer_workspace_selected(state) &&
        state.checkpoint_observer_controls != nullptr &&
        state.checkpoint_observer_controls->plan == state.checkpoint_plan &&
        state.checkpoint_observer_nonlocal_block == builder.GetInsertBlock() &&
        actual_safe_point->getParent() == builder.GetInsertBlock() &&
        state.checkpoint_observer_nonlocal_workspace == state.checkpoint_observer_workspace &&
        state.checkpoint_observer_nonlocal_anchor != nullptr &&
        state.checkpoint_observer_nonlocal_anchor->getFunction() == state.llvm_function};
    bool const carry_numeric_definitions{state.debug_native_numeric_block == builder.GetInsertBlock() &&
        actual_safe_point->getParent() == builder.GetInsertBlock() &&
        builder.GetInsertPoint() == builder.GetInsertBlock()->end()};
    auto const type{::llvm::FunctionType::get(builder.getVoidTy(),
        {builder.getInt64Ty(), builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t)*CHAR_BIT)),
         builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t)*CHAR_BIT)), builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t)*CHAR_BIT))}, false)};
    auto const bridge{get_llvm_runtime_bridge_function_symbol_value_unwrapped<
        ::uwvm2::runtime::lib::details::llvm_jit_debug_shutdown_poll_abi_bridge>(builder, type)};
    if(bridge == nullptr) { return false; }
    auto const& local{*state.local_func_storage_ptr};
    auto const integer{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t)*CHAR_BIT))};
    ::llvm::BasicBlock* fast_resume{};
#if defined(__linux__)
    if(auto const address{::uwvm2::runtime::lib::details::llvm_jit_debug_shutdown_flag_address_host_api()}; address != nullptr)
    {
        // Same process-lifetime lock-free atomic byte as the real host poll.
        // This relaxed hint only chooses whether to call that poll; it never
        // consumes published request state. The true path rechecks the flag
        // with acquire semantics and authenticates ownership under the host
        // mutex before throwing. A false hint skips only the no-op call.
        // Avoid unnecessary target acquire barriers between the cooperative
        // return and numeric code. This private load remains zero-line;
        // it grants neither code/register bits nor cancellation authority.
        auto* const function{builder.GetInsertBlock()->getParent()};
        auto* const requested{::llvm::BasicBlock::Create(builder.getContext(), "debug.shutdown.requested", function)};
        fast_resume = ::llvm::BasicBlock::Create(builder.getContext(), "debug.shutdown.resume", function);
        auto const pointer{::llvm::ConstantExpr::getIntToPtr(::llvm::ConstantInt::get(integer,
            reinterpret_cast<::std::uintptr_t>(address)), ::llvm::PointerType::getUnqual(builder.getContext()))};
        auto* const pending{builder.CreateLoad(builder.getInt8Ty(), pointer)};
        pending->setAtomic(::llvm::AtomicOrdering::Monotonic); pending->setAlignment(::llvm::Align{1u});
        builder.CreateCondBr(builder.CreateICmpNE(pending, builder.getInt8(0u)), requested, fast_resume);
        builder.SetInsertPoint(requested);
    }
#endif
    auto const poll{apply_llvm_jit_host_calling_conv(emit_runtime_local_func_llvm_jit_may_throw_call(state, type, bridge,
        {state.debug_activation_token, ::llvm::ConstantInt::get(integer, local.module_id),
         ::llvm::ConstantInt::get(integer, local.function_index), ::llvm::ConstantInt::get(integer, opcode_offset)}, false))};
    // Private foreign C++ cancellation MUST bypass lexical Wasm handlers and
    // retire actual activation/precise roots/instruction frames through cleanup.
    // No metadata/configuration bool is substituted for this real Invoke.
    if(poll == nullptr || !::llvm::isa<::llvm::InvokeInst>(poll) || poll->doesNotThrow()) { return false; }
    auto const identity{actual_safe_point->getMetadata("uwvm.debug.safe_point")};
    if(identity == nullptr || identity->getNumOperands() != 3u) { return false; }
    poll->setMetadata("uwvm.debug.shutdown", identity);
    actual_safe_point->setMetadata("uwvm.debug.shutdown.required", ::llvm::MDNode::get(builder.getContext(),
        {::llvm::ConstantAsMetadata::get(builder.getInt32(1u))}));
    if(fast_resume != nullptr)
    { builder.CreateBr(fast_resume); builder.SetInsertPoint(fast_resume); }
    if(carry_observer_packet)
    { state.checkpoint_observer_nonlocal_block = builder.GetInsertBlock(); }
    if(carry_numeric_definitions)
    { state.debug_native_numeric_block = builder.GetInsertBlock(); }
    return true;
#else
    static_cast<void>(opcode_offset); static_cast<void>(actual_safe_point); return true;
#endif
}

#pragma once
// Include after actual native exception call/cleanup emitters. Called through
// an earlier declaration ONLY after the existing noexcept debug point returns.
[[nodiscard]] inline bool emit_runtime_local_func_llvm_jit_checkpoint_retirement_poll(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t opcode_offset) noexcept
{
    namespace cp = ::uwvm2::runtime::checkpoint;
    auto const plan{state.checkpoint_plan};
    if(plan == nullptr || (plan->profile && plan->profile->purpose() == cp::compilation_purpose::observe_values))
    { return true; } // ordinary and observation: no retirement IR/probe/Invoke
#if defined(UWVM_CPP_EXCEPTIONS) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    // Legitimate unsupported stop points remain ordinary execution. They have
    // no sealed retirement site, so the actual manager cannot arm them later.
    if(!state.native_guest_exceptions || state.pending_numeric_plan != nullptr || state.checkpoint_current_site == 0u)
    { return true; }
    if(!plan->profile || state.checkpoint_current_site > plan->sites.size() ||
       !state.debug_activation_enabled || state.debug_activation_token == nullptr || state.ir_builder == nullptr ||
       state.local_func_storage_ptr == nullptr) { return false; }
    // [actual compiler-owned dense sites0 ... ordinal-1 ... N] end
    // [safe] complete nonzero ordinal bound before exact same-walk site borrow.
    auto const& site{plan->sites[static_cast<::std::size_t>(state.checkpoint_current_site-1u)]};
    if(site.phase != cp::frame_phase::before_opcode || site.opcode_offset != opcode_offset) { return true; }
    auto& builder{*state.ir_builder};
    auto const integer{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const type{::llvm::FunctionType::get(builder.getVoidTy(), {builder.getInt64Ty(), integer, integer, integer}, false)};
    // Deliberately UNWRAPPED and potentially throwing. The old debug bridge
    // and native single-step naked wrapper remain noexcept and are never crossed
    // by retirement. An Invoke's real cleanup edge retires activation/GC/trace.
    auto const bridge{get_llvm_runtime_bridge_function_symbol_value_unwrapped<
        ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_retirement_poll_abi_bridge>(builder, type)};
    if(bridge == nullptr) { return false; }
    auto const& local{*state.local_func_storage_ptr};
    auto const call{apply_llvm_jit_host_calling_conv(emit_runtime_local_func_llvm_jit_may_throw_call(state, type, bridge,
        {state.debug_activation_token, ::llvm::ConstantInt::get(integer, local.module_id),
         ::llvm::ConstantInt::get(integer, local.function_index), ::llvm::ConstantInt::get(integer, opcode_offset)}, false))};
    // Never route the private control signal through lexical Wasm handlers or
    // mark the actual poll nounwind. Genuine guest calls keep their old emitter.
    if(call == nullptr || !::llvm::isa<::llvm::InvokeInst>(call) || call->doesNotThrow()) { return false; }
    // Seal only sites for which THIS fused compiler actually emitted a genuine
    // cleanup Invoke. Entry/first-op identical offsets may share one dense ID;
    // each real debug-return edge still gets its own poll, metadata deduplicates.
    if(!plan->retirement_sites.empty() && plan->retirement_sites.back() > site.identifier) { return false; }
    if(plan->retirement_sites.empty() || plan->retirement_sites.back() != site.identifier)
    { plan->retirement_sites.push_back(site.identifier); }
    plan->retirement_abi_revision = 1u;
    return true;
#else
    static_cast<void>(opcode_offset); return true; // no native retirement proof/entry
#endif
}

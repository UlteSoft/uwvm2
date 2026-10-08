// PRIVATE actual validated-stream numeric exception experiment.
// Included after actual Wasm operand preparation, before ordinary return_call.
[[nodiscard]] inline bool try_emit_pending_numeric_return_call(runtime_local_func_llvm_jit_emit_state_t& state,
    validation_module_traits_t::wasm_u32 index) noexcept
{
    if(state.pending_numeric_context == nullptr || state.local_func_storage_ptr == nullptr || state.ir_builder == nullptr)
    { return false; }
    auto const* module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr || !module->imported_function_vec_storage.empty()) { return false; }
    auto const* wasm_type{resolve_runtime_callee_function_type(*module, index)};
    auto const* caller_type{state.local_func_storage_ptr->function_type_ptr};
    if(wasm_type == nullptr || caller_type == nullptr) { return false; }
    auto const caller_layout{get_runtime_wasm_call_abi_layout(*caller_type)};
    auto const callee_layout{get_runtime_wasm_call_abi_layout(*wasm_type)};
    if(!caller_layout.valid || !callee_layout.valid ||
       caller_layout.result_count != callee_layout.result_count ||
       caller_layout.result_bytes != callee_layout.result_bytes) { return false; }
    for(::std::size_t result{}; result != caller_layout.result_count; ++result)
    {
        // [validated exact numeric caller results][validated callee results]
        // [safe                                  ] both count proofs precede
        // every typed read. The surviving tuple buffer has the SAME layout,
        // including raw f32/f64 types, not merely the same LLVM void return.
        if(caller_type->result.begin[result] != wasm_type->result.begin[result]) { return false; }
    }
    auto const function{pending_numeric_core_declaration(*state.llvm_module, *state.llvm_context_holder,
        *module, index, *wasm_type)};
    if(function == nullptr || function->getReturnType() != state.llvm_function->getReturnType() ||
       function->getCallingConv() != state.llvm_function->getCallingConv() ||
       (function->getCallingConv() != ::llvm::CallingConv::Tail &&
        function->getFunctionType() != state.llvm_function->getFunctionType())) { return false; }
    auto prepared{prepare_runtime_local_func_llvm_jit_wasm_call_operands(state, *wasm_type)};
    if(!prepared.valid) { return false; }
    auto const core_parameter_count{function->arg_size()};
    auto const hidden_parameter_count{prepared.abi_layout.result_count > 1uz ? 2uz : 1uz};
    // [exact bounded core parameter array][FIRST context][LAST tuple, if any]
    // [safe                                                               ]
    // Subtraction checks both hidden elements before any vector extent grows.
    if(core_parameter_count < hidden_parameter_count ||
       prepared.arguments.size() != core_parameter_count - hidden_parameter_count)
    { return false; }
    ::uwvm2::utils::container::vector<::llvm::Value*> arguments{};
    arguments.reserve(core_parameter_count);
    arguments.push_back(state.pending_numeric_context);
    for(auto const argument: prepared.arguments) { arguments.push_back(argument); }
    if(prepared.abi_layout.result_count > 1uz)
    {
        // [surviving caller's actual result byte array][end]
        // [safe                                         ] final argument remains
        // that original buffer. No retiring frame alloca is forwarded.
        arguments.push_back(state.llvm_function->getArg(state.llvm_function->arg_size() - 1uz));
    }
    auto& builder{*state.ir_builder};
    if(!leave_runtime_llvm_jit_gc_root_frame(builder, state.gc_root_frame)) { return false; }
    if(state.emit_call_stack_frames && !emit_runtime_local_func_llvm_jit_call_stack_pop(builder)) { return false; }
    auto const transfer{apply_llvm_jit_wasm_calling_conv(builder.CreateCall(function,
        {arguments.data(), arguments.size()}))};
    transfer->setTailCallKind(::llvm::CallInst::TCK_MustTail);
    // A tail caller retires its lexical handlers. Its callee's guest effect
    // propagates unchanged even if the original return_call was inside a try.
    record_pending_numeric_call(*transfer, index, true);
    // TRUE musttail: context is forwarded unchanged and the exact return is
    // IMMEDIATE. Lexical handlers/frame of the tail caller are retired; no
    // pending check, trace append, alloca or normal-result load follows this call.
    if(function->getReturnType()->isVoidTy()) { builder.CreateRetVoid(); }
    else { builder.CreateRet(transfer); }
    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
}

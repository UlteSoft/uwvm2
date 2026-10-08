#pragma once
// Independent checkpoint-selected prologue; NOT wired into shared compilation
// or the runtime. The typed Wasm FunctionType/arguments/calling convention are
// untouched. A future genuine private runtime bridge must authenticate and
// consume its actual immutable logical frame; this IR grants no such authority.
// Include after actual emit state, entry-alloca and resume-value owner helpers.
struct llvm_jit_checkpoint_context_emit_result
{
    bool valid{}, selected{};
    ::llvm::StructType* context_type{};
    ::llvm::AllocaInst* context_owner{};
    ::llvm::CallInst* bridge_call{};
    ::llvm::Value* logical_site{};
    ::llvm::Value* payload{};
    ::llvm::Value* payload_bytes{};
    ::llvm::Value* original_flags{};
    ::llvm::Value* flag_count{};
};
[[nodiscard]] inline llvm_jit_checkpoint_context_emit_result
    emit_runtime_local_func_llvm_jit_checkpoint_context(
        runtime_local_func_llvm_jit_emit_state_t& state, ::llvm::Function* actual_context_bridge)
{
    // This early return precedes validation, allocation and IR. Ordinary full
    // engines neither create a context nor call/probe any checkpoint TLS state.
    if(state.checkpoint_plan == nullptr) { return {.valid = true}; }
    namespace checkpoint = ::uwvm2::runtime::checkpoint;
    auto const plan{state.checkpoint_plan};
    if(plan->profile && plan->profile->purpose() == checkpoint::compilation_purpose::observe_values)
    { return {.valid = true}; } // no unused restore context for observation
    if(checkpoint::validate_plan(*plan) != checkpoint::status::ok || state.llvm_function == nullptr ||
       state.llvm_module == nullptr || state.llvm_function->getParent() != state.llvm_module ||
       state.ir_builder == nullptr || state.ir_builder->GetInsertBlock() == nullptr ||
       state.ir_builder->GetInsertBlock()->getParent() != state.llvm_function ||
       (!state.ir_builder->GetInsertBlock()->empty() && state.ir_builder->GetInsertBlock()->back().isTerminator()) ||
       actual_context_bridge == nullptr || actual_context_bridge->getParent() != state.llvm_module ||
       actual_context_bridge == state.llvm_function || actual_context_bridge->getCallingConv() != ::llvm::CallingConv::C ||
       !actual_context_bridge->doesNotThrow() || actual_context_bridge->doesNotReturn() ||
       actual_context_bridge->isIntrinsic()) { return {}; }
    // This native runtime ABI is valid only for the actual JIT target. Prove
    // module DataLayout BEFORE generating any context/allocation/pointer IR;
    // a foreign pointer width is not repaired by a later native call check.
    auto const& layout{state.llvm_module->getDataLayout()};
    if(layout.isDefault() || layout.getPointerSizeInBits() != sizeof(::std::uintptr_t) * CHAR_BIT)
    { return {}; }
    auto& builder{*state.ir_builder};
    auto const signature{actual_context_bridge->getFunctionType()};
    // Fixed bridge ABI only: void(u64 actual_module,u64 actual_function,ptr
    // compiler-owned context). No fabricated aggregate-return ABI, guest args,
    // six integer parameters or mutable global policy alter the Wasm ABI.
    if(signature->isVarArg() || !signature->getReturnType()->isVoidTy() || signature->getNumParams() != 3u ||
       signature->getParamType(0u) != builder.getInt64Ty() || signature->getParamType(1u) != builder.getInt64Ty() ||
       signature->getParamType(2u) != builder.getPtrTy() ||
       actual_context_bridge->getAttributes().getRetAttrs().hasAttributes()) { return {}; }
    for(unsigned i{}; i != 3u; ++i)
    {
        // Disallow hidden ABI modifiers (sret/byval/inalloca/etc.). Even benign
        // parameter promises are added only by a separately reviewed real bridge.
        if(actual_context_bridge->getAttributes().getParamAttrs(i).hasAttributes()) { return {}; }
    }
    auto const integer{layout.getIntPtrType(builder.getContext())};
    // The manager/compiler agree this private native layout through the actual
    // target DataLayout; it is NEVER the portable checkpoint format. No C++
    // sizeof/offsetof, host address or database bytes are cast to this aggregate.
    auto const aggregate{::llvm::StructType::get(builder.getContext(),
        {builder.getInt64Ty(), builder.getPtrTy(), integer, builder.getPtrTy(), integer}, false)};
    auto const owner{create_llvm_jit_entry_block_alloca(builder, aggregate, nullptr,
        get_llvm_string_ref(u8"checkpoint.private.native.context.v1"))};
    if(owner == nullptr || owner->getFunction() != state.llvm_function) { return {}; }
    // [actual compiler-owned fixed five-field context] context_end
    // [safe                                          ] its complete type and
    // same-function owner were established before aggregate initialization.
    // Zero means normal execution if an authenticated bridge has no resume work.
    builder.CreateStore(::llvm::ConstantAggregateZero::get(aggregate), owner);
    auto const call{builder.CreateCall(actual_context_bridge,
        {builder.getInt64(plan->module), builder.getInt64(plan->function), owner})};
    call->setCallingConv(::llvm::CallingConv::C);
    call->setDoesNotThrow(); // Only the actual known-nounwind bridge was admitted.
    ::std::array<::llvm::Value*, 5u> fields{};
    for(unsigned i{}; i != fields.size(); ++i)
    {
        // [same owned context fields0 ... i ... 5] context_end
        // [safe                                ] i<5 and exact aggregate type
        // BEFORE field GEP/read; not a public pointer/size authorization test.
        auto const cell{builder.CreateStructGEP(aggregate, owner, i)};
        fields[i] = builder.CreateLoad(aggregate->getElementType(i), cell);
    }
    return {.valid = true, .selected = true, .context_type = aggregate, .context_owner = owner, .bridge_call = call,
        .logical_site = fields[0u], .payload = fields[1u], .payload_bytes = fields[2u],
        .original_flags = fields[3u], .flag_count = fields[4u]};
}

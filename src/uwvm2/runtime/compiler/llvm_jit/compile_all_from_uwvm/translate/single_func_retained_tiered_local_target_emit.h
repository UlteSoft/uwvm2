#pragma once
// Lexical implementation after the existing fused-call helpers/types. This
// optional compiler DATA is issued only by the genuine tiered retained sink.
// Unknown/unsupported emission shapes keep their original checked dispatch.
inline constexpr void attach_retained_tiered_local_target_identity(
    runtime_local_func_llvm_jit_emit_state_t& state, ::llvm::LoadInst& load,
    ::std::size_t local_index,
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& signature) noexcept
{
    if(!state.stage_retained_unwind_import_routes || state.compilation_mode != llvm_jit_compilation_mode::tiered ||
       state.local_func_storage_ptr == nullptr || state.llvm_module == nullptr || state.llvm_context_holder == nullptr ||
       state.local_func_storage_ptr->runtime_module_ptr == nullptr || state.lazy_defined_typed_entry_target_base_address == 0u)
    { return; }
    auto const& module{*state.local_func_storage_ptr->runtime_module_ptr};
    auto const imports{module.imported_function_vec_storage.size()};
    auto const count{module.local_defined_function_vec_storage.size()};
    using wasm_u32 = validation_module_traits_t::wasm_u32;
    auto const maximum{static_cast<::std::size_t>((::std::numeric_limits<wasm_u32>::max)())};
    if(local_index >= count || local_index >= state.lazy_defined_typed_entry_target_count ||
       imports > maximum || local_index > maximum - imports || load.getModule() != state.llvm_module ||
       state.local_func_storage_ptr->function_index < imports ||
       state.local_func_storage_ptr->function_index - imports >= count) { return; }
    // [actual local definition0 ... local_index ... count] end
    // [safe] count/public-index range proven BEFORE source/descriptor lookup.
    // The original fused walker already checked this call and its arguments.
    auto const actual_signature{module.local_defined_function_vec_storage.index_unchecked(local_index).function_type_ptr};
    if(actual_signature == nullptr || actual_signature != ::std::addressof(signature)) { return; }
    auto const target{get_or_create_llvm_wasm_function_declaration(*state.llvm_module, *state.llvm_context_holder,
        module, static_cast<wasm_u32>(imports + local_index), signature)};
    auto const type{get_llvm_function_type_from_wasm_function_type(*state.llvm_context_holder, signature)};
    auto const caller{load.getFunction()};
    if(target == nullptr || caller == nullptr || target->getParent() != state.llvm_module || caller->getParent() != state.llvm_module ||
       type == nullptr || target->getFunctionType() != type || target->getCallingConv() != get_llvm_jit_typed_calling_conv(*type)) { return; }
    auto const integer{::llvm::Type::getInt64Ty(*state.llvm_context_holder)};
    ::llvm::Metadata* identity[]{
        ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(integer, state.local_func_storage_ptr->module_id)),
        ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(integer, state.local_func_storage_ptr->function_index)),
        ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(integer, imports + local_index)),
        ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(integer, local_index)),
        ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(integer, state.lazy_defined_typed_entry_target_base_address)),
        ::llvm::ConstantAsMetadata::get(target), ::llvm::ConstantAsMetadata::get(caller)};
    // Function constants + exact integer DATA only: no LocalAsMetadata/SSA value
    // in ordinary attachments. The consumer rechecks the actual load/base/uses.
    load.setMetadata("uwvm.checked.tiered.local.typed.target.v2", ::llvm::MDNode::get(*state.llvm_context_holder, identity));
}

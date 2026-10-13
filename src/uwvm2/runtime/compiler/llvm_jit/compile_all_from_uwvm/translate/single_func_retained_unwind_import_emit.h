#pragma once
// Lexical checked-IR staging, included inside the existing LLVM emit namespace.
// This helper has no raw Wasm cursor, native-entry publication or callable permit.
struct retained_unwind_import_call_result
{
    bool handled{};
    bool valid{};
    ::llvm::Value* result{};
};

// Rich declaration equivalence is an optimization prerequisite, distinct
// from the legacy physical carrier check. Both descriptors must be exact
// members of this initializer-retained type array before any index/read.
[[nodiscard]] inline bool checked_retained_import_signature_equivalent(
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& module,
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& left,
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& right) noexcept
{
    if(!runtime_wasm_function_types_equal(left, right)) { return false; }
    auto const& section{module.type_section_storage};
    auto const count{get_runtime_type_section_count(module)};
    ::std::size_t first{}, second{};
    if(count == 0uz || count > static_cast<::std::size_t>((::std::numeric_limits<validation_module_traits_t::wasm_u32>::max)()) ||
       classify_runtime_storage_pointer(section.type_section_begin, count, &left, first) !=
           runtime_storage_pointer_membership::element ||
       classify_runtime_storage_pointer(section.type_section_begin, count, &right, second) !=
           runtime_storage_pointer_membership::element) { return false; }
    if(first == second) { return true; }
    auto const begin{section.owned_signature_begin}; auto const end{section.owned_signature_end};
    // [actual parser-owned signature allocation ... end] one-past
    // [safe] complete endpoints + count BEFORE difference/view creation.
    // These pointers are initializer-retained immutable fields, never advanced.
    if(begin == nullptr || end == nullptr || end < begin || static_cast<::std::size_t>(end - begin) != count)
    { return false; }
    namespace wasm3 = ::uwvm2::validation::standard::wasm3;
    namespace types = ::uwvm2::parser::wasm::standard::wasm3::type;
    auto const signatures{wasm3::core3_signature_view<::uwvm2::uwvm::runtime::storage::wasm_binfmt1_owned_signature_t>{begin, count}};
    types::core_value_type const actual{types::value_kind::reference,
        types::heap_type{static_cast<::std::int_least64_t>(first)}, false};
    types::core_value_type const expected{types::value_kind::reference,
        types::heap_type{static_cast<::std::int_least64_t>(second)}, false};
    // Runtime type indices are u32 before these s64 representations; their
    // bidirectional Core3 matching includes the original recursive forest.
    return wasm3::core3_value_type_matches_with_context(actual, expected, signatures, section.core3_context_ptr) &&
        wasm3::core3_value_type_matches_with_context(expected, actual, signatures, section.core3_context_ptr);
}

template<typename EmitTypedTarget>
[[nodiscard]] inline retained_unwind_import_call_result stage_retained_unwind_import_call(
    runtime_local_func_llvm_jit_emit_state_t& state,
    validation_module_traits_t::wasm_u32 import_index,
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& signature,
    llvm_jit_prepared_wasm_call_operands_t const& prepared,
    EmitTypedTarget&& emit_typed_target) noexcept
{
    if(!state.stage_retained_unwind_import_routes || !state.route_wasm_calls_through_runtime_bridge ||
       !state.emit_unwind_call_stack_frames || state.local_func_storage_ptr == nullptr ||
       state.ir_builder == nullptr || state.llvm_module == nullptr || state.llvm_context_holder == nullptr)
    { return {}; }
    auto const module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr || import_index >= module->imported_function_vec_storage.size()) { return {}; }
    // The original initializer retains import records/provider lifetimes. Resolve
    // their exact same-module local member, then recheck the complete Wasm ABI.
    // No foreign provider entry or host function is admitted to this alternative.
    auto const resolved{resolve_runtime_direct_callee(*module, import_index)};
    if(!resolved.state_valid) { return {.handled = true}; }
    if(!resolved.direct_callable || resolved.function_type_ptr == nullptr ||
       !checked_retained_import_signature_equivalent(*module, *resolved.function_type_ptr, signature)) { return {}; }
    auto const imports{module->imported_function_vec_storage.size()};
    if(resolved.func_index < imports) { return {.handled = true}; }
    auto const local{static_cast<::std::size_t>(resolved.func_index) - imports};
    if(local >= module->local_defined_function_vec_storage.size() ||
       local >= state.lazy_defined_typed_entry_target_count || local >= state.lazy_defined_raw_call_target_count ||
       state.lazy_defined_typed_entry_target_base_address == 0u || state.lazy_defined_raw_call_target_base_address == 0u)
    { return {.handled = true}; }
    auto& builder{*state.ir_builder};
    auto& context{*state.llvm_context_holder};
    auto const current{builder.GetInsertBlock()};
    auto const caller{current == nullptr ? nullptr : current->getParent()};
    // Check the open instruction list before back(); LLVM versions differ in
    // whether the terminator accessor permits an unfinished block.
    if(caller == nullptr || caller->getParent() != state.llvm_module || (!current->empty() && current->back().isTerminator()))
    { return {.handled = true}; }
    auto const target{get_or_create_llvm_wasm_function_declaration(*state.llvm_module, context, *module,
        resolved.func_index, signature)};
    auto const type{get_llvm_function_type_from_wasm_function_type(context, signature)};
    if(target == nullptr || type == nullptr || target->getFunctionType() != type ||
       target->getCallingConv() != get_llvm_jit_typed_calling_conv(*type)) { return {.handled = true}; }
    auto const fast{::llvm::BasicBlock::Create(context, "checked.import.fast", caller)};
    auto const slow{::llvm::BasicBlock::Create(context, "checked.import.original", caller)};
    auto const merge{::llvm::BasicBlock::Create(context, "checked.import.merge", caller)};
    auto const branch{builder.CreateCondBr(::llvm::ConstantInt::getFalse(context), fast, slow)};
    auto const integer{::llvm::Type::getInt64Ty(context)};
    ::llvm::Metadata* identity[]{
        ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(integer, state.local_func_storage_ptr->module_id)),
        ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(integer, state.local_func_storage_ptr->function_index)),
        ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(integer, import_index)),
        ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(integer, resolved.func_index)),
        ::llvm::ConstantAsMetadata::get(target), ::llvm::ConstantAsMetadata::get(caller)};
    // All value operands are actual module-owned Function constants. Ordinary
    // instruction metadata must never embed LocalAsMetadata (LLVM verifier rule).
    branch->setMetadata("uwvm.checked.lazy.unwind.import.route.v1", ::llvm::MDNode::get(context, identity));
    builder.SetInsertPoint(fast);
    auto const typed{emit_typed_target(local, get_llvm_string_ref(u8"call.params"), get_llvm_string_ref(u8"call.result.buf"))};
    if(!typed.valid) { return {.handled = true}; }
    auto const fast_end{builder.GetInsertBlock()};
    builder.CreateBr(merge);
    builder.SetInsertPoint(slow);
    auto const original{emit_runtime_local_func_llvm_jit_raw_host_wasm_call(state, *module, import_index, signature,
        prepared, get_llvm_string_ref(u8"call.params"), get_llvm_string_ref(u8"call.result.buf"))};
    if(!original.valid) { return {.handled = true}; }
    auto const slow_end{builder.GetInsertBlock()};
    builder.CreateBr(merge);
    builder.SetInsertPoint(merge);
    ::llvm::Value* result{};
    if(get_runtime_block_result_count(prepared.results) != 0uz)
    {
        auto const type{get_llvm_result_type_from_wasm_result_range(context, prepared.results.begin, prepared.results.end)};
        if(type == nullptr || typed.result_value == nullptr || original.result_value == nullptr ||
           typed.result_value->getType() != type || original.result_value->getType() != type) { return {.handled = true}; }
        auto const phi{builder.CreatePHI(type, 2u, "checked.import.result")};
        phi->addIncoming(typed.result_value, fast_end); phi->addIncoming(original.result_value, slow_end);
        result = phi;
    }
    return {.handled = true, .valid = true, .result = result};
}

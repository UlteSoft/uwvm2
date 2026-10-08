/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
// Included after the original physical reference primitive and live state.
// The sole typed walk supplied a bounded decoded function index and exact
// non-null declared heap. This native emitter never accepts a raw Wasm slice.
[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_decoded_ref_func(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::core3_ref_func_event const& event) noexcept
{
    namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
    if(!state.valid || state.ir_builder == nullptr || state.llvm_context_holder == nullptr ||
       state.llvm_module == nullptr || state.llvm_function == nullptr ||
       state.local_func_storage_ptr == nullptr || state.control_stack.empty() ||
       state.current_wasm_op_offset == SIZE_MAX || event.result_type.kind != t::value_kind::reference ||
       event.result_type.nullable || event.exact_function_type_index == SIZE_MAX ||
       event.result_type.heap.code != static_cast<::std::int_least64_t>(event.exact_function_type_index))
    { return false; }
    // [owned LLVM control frames ... last] end
    // [safe] empty() proved back(); no bytecode pointer is borrowed or advanced.
    // D2 retains the original generic-family root retirement before an escape.
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
    if(state.control_stack.back().is_reachable &&
       (!snapshot_runtime_local_func_llvm_jit_gc_roots(state) || !sealed_compact_codegen::retire(state))) { return false; }
#endif
    // Existing fused dispatcher supplies this checked integer offset. Preserve
    // the same native provenance/debug hooks without a second Wasm reader.
    if(!emit_runtime_local_func_llvm_jit_native_provenance(state, state.current_wasm_op_offset)) { return false; }
    if(state.pending_numeric_plan != nullptr && !pending_numeric_primary_opcode(0xd2u)) { return false; }
    if(!emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) { return false; }
    if(!state.control_stack.back().is_reachable || state.unreachable_control_depth != 0uz) { return true; }
    auto const runtime_module_ptr{state.local_func_storage_ptr->runtime_module_ptr};
    if(runtime_module_ptr == nullptr) { return false; }
    auto const imports{runtime_module_ptr->imported_function_vec_storage.size()};
    auto const locals{runtime_module_ptr->local_defined_function_vec_storage.size()};
    auto const index{static_cast<::std::size_t>(event.function_index)};
    if(index >= imports && index - imports >= locals) { return false; }
    // This is the original physical module-owned reference lookup, not repeat
    // Wasm validation. The immutable vectors outlive lazy/tiered code; importer
    // declaration DATA stays distinct from an imported alias's resolved target.
    auto const reference{llvm_jit_funcref_from_table_elem(
        llvm_jit_resolve_table_elem_from_func_index(*runtime_module_ptr, event.function_index))};
    auto const symbol_name{::uwvm2::utils::container::u8concat_uwvm(get_llvm_runtime_module_symbol_prefix(*runtime_module_ptr),
        u8"_function_", index, u8"_reference_record")};
    // Preserve the existing relocatable host symbol and target-specific address
    // policy exactly; copying typed DATA creates no additional guest check.
    auto payload{get_llvm_external_host_object_address(*state.ir_builder,
        reinterpret_cast<::std::uintptr_t>(reference.ref.storage.ptr),
        ::uwvm2::utils::container::u8string_view{symbol_name.data(), symbol_name.size()})};
    auto result{emit_llvm_jit_ref_from_payload(*state.ir_builder, payload, reference.ref.kind,
        state.llvm_module->getDataLayout().isLittleEndian())};
    if(result == nullptr) { return false; }
    state.operand_stack.push_back({.type = runtime_operand_stack_value_type::funcref, .value = result,
        .known_ref_func_index = index});
    // [new exact SSA descriptor] end
    // [safe] owning push stores the original known function index with its SSA
    // value; no borrowed descriptor or guest pointer survives a vector change.
    return true;
}

/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
// Included inside the actual LLVM translator namespace after its emit state
// and null-value primitive. Only the typed walk supplies this successful event;
// no Wasm span, decoder or second semantic validator enters this lowering.
[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_decoded_ref_null(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::core3_ref_null_event const& event) noexcept
{
    namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
    if(!state.valid || state.ir_builder == nullptr || state.llvm_context_holder == nullptr ||
       state.llvm_module == nullptr || state.llvm_function == nullptr ||
       state.local_func_storage_ptr == nullptr || state.control_stack.empty() ||
       state.current_wasm_op_offset == SIZE_MAX || event.result_type.kind != t::value_kind::reference ||
       !event.result_type.nullable) { return false; }
    // [owned LLVM control frames ... last] end
    // [safe] empty() proved back(); no guest input pointer is inspected.
    // ref.null (0xd0) is outside the original sealed_pure family. Preserve
    // the original generic emitter's retirement BEFORE any native escape or
    // physical SSA push, using its existing actual root-snapshot helper.
#if defined(UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR) && UWVM_EXPERIMENTAL_SEALED_COMPACT_CURSOR == 1
    if(state.control_stack.back().is_reachable &&
       (!snapshot_runtime_local_func_llvm_jit_gc_roots(state) || !sealed_compact_codegen::retire(state))) { return false; }
#endif
    // The fused dispatcher already supplied this exact source offset. Reuse
    // it; no raw Wasm pointer is formed, advanced, subtracted or dereferenced.
    // The original provenance/debug helpers deduplicate an existing point.
    if(!emit_runtime_local_func_llvm_jit_native_provenance(state, state.current_wasm_op_offset)) { return false; }
    if(state.pending_numeric_plan != nullptr && !pending_numeric_primary_opcode(0xd0u)) { return false; }
    if(!emit_runtime_local_func_llvm_jit_instruction_debug_safe_point(state)) { return false; }
    if(!state.control_stack.back().is_reachable || state.unreachable_control_depth != 0uz) { return true; }
    runtime_operand_stack_value_type carrier{};
    switch(event.carrier)
    {
        case 0x70u: carrier = runtime_operand_stack_value_type::funcref; break;
        case 0x6fu: carrier = runtime_operand_stack_value_type::externref; break;
        case 0x69u: carrier = static_cast<runtime_operand_stack_value_type>(0x69u); break;
        default: return false; // Corrupt native DATA cannot authorize a new ABI carrier.
    }
    auto zero{get_llvm_zero_constant_from_wasm_value_type(state.llvm_module->getContext(), carrier)};
    if(zero == nullptr) { return false; }
    // Identical original zero constant and physical SSA push. The authoritative
    // validation stack retains the exact heap and function type in the event.
    state.operand_stack.push_back({.type = carrier, .value = zero});
    return true;
}

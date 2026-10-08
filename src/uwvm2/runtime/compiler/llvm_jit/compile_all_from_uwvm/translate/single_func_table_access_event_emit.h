// Included within the existing LLVM compiler namespace, before dispatcher.
[[nodiscard]] inline constexpr bool llvm_jit_table_access_event_consistent(
    runtime_local_func_llvm_jit_emit_state_t const& state,
    ::uwvm2::validation::standard::wasm3::validated_table_access_event const& event) noexcept
{
    namespace v = ::uwvm2::validation::standard::wasm3;
    namespace t = ::uwvm2::parser::wasm::standard::wasm3::type;
    if(!state.valid || !v::is_table_access_event_opcode(event.opcode) ||
       event.source_bytes < 2uz || event.source_bytes > 6uz ||
       event.element_type.kind != t::value_kind::reference ||
       (event.element_carrier != 0x70u && event.element_carrier != 0x6fu && event.element_carrier != 0x69u) ||
       state.local_func_storage_ptr == nullptr || event.source_offset == SIZE_MAX ||
       state.current_wasm_op_offset != event.source_offset || state.control_stack.empty() ||
       state.unreachable_control_depth > SIZE_MAX - state.control_stack.size() ||
       event.control_depth != state.control_stack.size() + state.unreachable_control_depth ||
       (state.control_stack.back().is_reachable && event.stack_polymorphic)) { return false; }
    auto const& function{*state.local_func_storage_ptr};
    // This private state was prepared from ONE actual retained runtime body.
    // [code_begin ... complete expression] | code_end
    // [safe same parser-owned range] one-past: subtraction only, no byte read.
    if(function.code_begin == nullptr || function.code_end == nullptr || function.code_end < function.code_begin)
    { return false; }
    auto const bytes{static_cast<::std::size_t>(function.code_end - function.code_begin)};
    return event.source_offset < bytes && event.source_bytes <= bytes - event.source_offset;
}

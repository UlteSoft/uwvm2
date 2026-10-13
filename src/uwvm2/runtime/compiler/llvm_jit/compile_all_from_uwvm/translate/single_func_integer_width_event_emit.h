// Included after existing unary helpers in the actual LLVM compiler namespace.
// Three first-typed immediate-free integer conversions consume owned DATA only.
[[nodiscard]] inline constexpr bool llvm_jit_integer_width_event_consistent(
    runtime_local_func_llvm_jit_emit_state_t const& state,
    ::uwvm2::validation::standard::wasm3::validated_integer_width_event const& event) noexcept
{
    namespace v = ::uwvm2::validation::standard::wasm3;
    if(!state.valid || !v::is_integer_width_event_opcode(event.opcode) || event.source_bytes != 1u ||
       event.popped != 1u || event.pushed != 1u ||
       event.pop_bytes != (event.opcode == 0xa7u ? 8u : 4u) || event.push_bytes != (event.opcode == 0xa7u ? 4u : 8u) ||
       event.source_offset == SIZE_MAX || state.current_wasm_op_offset != event.source_offset ||
       state.control_stack.empty() || state.unreachable_control_depth > SIZE_MAX - state.control_stack.size() ||
       event.control_depth != state.control_stack.size() + state.unreachable_control_depth ||
       (state.control_stack.back().is_reachable && event.stack_polymorphic)) { return false; }
    return true;
}
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_integer_width_event_body(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::validated_integer_width_event const& event) noexcept
{
    switch(event.opcode)
    {
        case 0xa7u:
            return try_emit_runtime_local_func_llvm_jit_unary(state,
                runtime_operand_stack_value_type::i64, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) constexpr noexcept
                                                       { return ir_builder.CreateTrunc(operand.value, ::llvm::Type::getInt32Ty(ir_builder.getContext())); });
        case 0xacu:
            return try_emit_runtime_local_func_llvm_jit_unary(state,
                runtime_operand_stack_value_type::i32, runtime_operand_stack_value_type::i64,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) constexpr noexcept
                                                       { return ir_builder.CreateSExt(operand.value, ::llvm::Type::getInt64Ty(ir_builder.getContext())); });
        case 0xadu:
            return try_emit_runtime_local_func_llvm_jit_unary(state,
                runtime_operand_stack_value_type::i32, runtime_operand_stack_value_type::i64,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) constexpr noexcept
                                                       { return ir_builder.CreateZExt(operand.value, ::llvm::Type::getInt64Ty(ir_builder.getContext())); });
        default: return false;
    }
}

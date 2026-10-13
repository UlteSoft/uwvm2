// Included after the existing unary/binary helpers in the LLVM namespace.
// Exact original ICmp+coerce expressions consume SAME first-typed event DATA.
[[nodiscard]] inline constexpr bool llvm_jit_integer_compare_event_consistent(
    runtime_local_func_llvm_jit_emit_state_t const& state,
    ::uwvm2::validation::standard::wasm3::validated_integer_compare_event const& event) noexcept
{
    namespace v = ::uwvm2::validation::standard::wasm3;
    auto const arity{event.opcode == 0x45u || event.opcode == 0x50u ? 1u : 2u};
    if(!state.valid || !v::is_integer_compare_event_opcode(event.opcode) || event.source_bytes != 1u ||
       event.popped != arity || event.pushed != 1u || event.push_bytes != 4u ||
       event.pop_bytes != arity * (event.opcode <= 0x4fu ? 4u : 8u) ||
       event.source_offset == SIZE_MAX || state.current_wasm_op_offset != event.source_offset ||
       state.control_stack.empty() || state.unreachable_control_depth > SIZE_MAX - state.control_stack.size() ||
       event.control_depth != state.control_stack.size() + state.unreachable_control_depth ||
       (state.control_stack.back().is_reachable && event.stack_polymorphic)) { return false; }
    return true;
}
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_integer_compare_event_body(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::validated_integer_compare_event const& event) noexcept
{
    switch(event.opcode)
    {
        case 0x45u:
            return try_emit_runtime_local_func_llvm_jit_unary(state,
                runtime_operand_stack_value_type::i32, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpEQ(operand.value, ::llvm::ConstantInt::get(operand.value->getType(), 0u))); });
        case 0x46u:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i32, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpEQ(left.value, right.value)); });
        case 0x47u:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i32, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpNE(left.value, right.value)); });
        case 0x48u:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i32, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSLT(left.value, right.value)); });
        case 0x49u:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i32, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpULT(left.value, right.value)); });
        case 0x4au:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i32, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSGT(left.value, right.value)); });
        case 0x4bu:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i32, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpUGT(left.value, right.value)); });
        case 0x4cu:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i32, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSLE(left.value, right.value)); });
        case 0x4du:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i32, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpULE(left.value, right.value)); });
        case 0x4eu:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i32, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSGE(left.value, right.value)); });
        case 0x4fu:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i32, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpUGE(left.value, right.value)); });
        case 0x50u:
            return try_emit_runtime_local_func_llvm_jit_unary(state,
                runtime_operand_stack_value_type::i64, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpEQ(operand.value, ::llvm::ConstantInt::get(operand.value->getType(), 0u))); });
        case 0x51u:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i64, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpEQ(left.value, right.value)); });
        case 0x52u:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i64, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpNE(left.value, right.value)); });
        case 0x53u:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i64, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSLT(left.value, right.value)); });
        case 0x54u:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i64, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpULT(left.value, right.value)); });
        case 0x55u:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i64, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSGT(left.value, right.value)); });
        case 0x56u:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i64, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpUGT(left.value, right.value)); });
        case 0x57u:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i64, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSLE(left.value, right.value)); });
        case 0x58u:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i64, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpULE(left.value, right.value)); });
        case 0x59u:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i64, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSGE(left.value, right.value)); });
        case 0x5au:
            return try_emit_runtime_local_func_llvm_jit_binary(state,
                runtime_operand_stack_value_type::i64, runtime_operand_stack_value_type::i32,
                [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpUGE(left.value, right.value)); });
        default: return false;
    }
}

// Included inside the actual LLVM compiler namespace after unary/binary helpers.
// Receives owned first-decode DATA only; no Wasm slice/cursor is accepted.
// Publication still requires the complete same-function fused validation proof.
[[nodiscard]] inline constexpr bool llvm_jit_i32_numeric_event_consistent(
    runtime_local_func_llvm_jit_emit_state_t const& state,
    ::uwvm2::validation::standard::wasm3::validated_i32_numeric_event const& event) noexcept
{
    namespace v = ::uwvm2::validation::standard::wasm3;
    if(!state.valid || !v::is_i32_numeric_event_opcode(event.opcode) || event.source_bytes != 1uz ||
       event.popped != (event.opcode <= 0x69u ? 1u : 2u) || event.pushed != 1u ||
       event.pop_bytes != event.popped * 4u || event.push_bytes != 4u ||
       event.source_offset == SIZE_MAX || state.current_wasm_op_offset != event.source_offset ||
       state.control_stack.empty() ||
       state.unreachable_control_depth > SIZE_MAX - state.control_stack.size() ||
       event.control_depth != state.control_stack.size() + state.unreachable_control_depth ||
       (state.control_stack.back().is_reachable && event.stack_polymorphic)) [[unlikely]] { return false; }
    // Logical child entry resets local validation polymorphism even below a dead
    // parent. LLVM keeps those child frames only in its skipped-depth counter.
    // The checked sum never overflows; physical deadness suppresses body IR.
    return true;
}

// Called only after the normalized entry's compiler DATA preflight and original
// opcode observation ordering. This emits the exact original18 expressions;
// unary/binary helpers retain their owned SSA/physical-unreachable checks.
// Existing value/module pointer borrows last through the synchronous LLVM call;
// there is no source cursor, source read, guest memory guard or native authority.
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_i32_numeric_event_body(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::validated_i32_numeric_event const& event) noexcept
{
    switch(event.opcode)
    {
        case 0x67u: // i32.clz
            return try_emit_runtime_local_func_llvm_jit_unary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) constexpr noexcept
               {
                   auto llvm_module{state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value, ::llvm::ConstantInt::getFalse(ir_builder.getContext())};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::ctlz, overloaded_types, arguments);
               });
        case 0x68u: // i32.ctz
            return try_emit_runtime_local_func_llvm_jit_unary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) constexpr noexcept
               {
                   auto llvm_module{state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value, ::llvm::ConstantInt::getFalse(ir_builder.getContext())};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::cttz, overloaded_types, arguments);
               });
        case 0x69u: // i32.popcnt
            return try_emit_runtime_local_func_llvm_jit_unary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) constexpr noexcept
               {
                   auto llvm_module{state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::ctpop, overloaded_types, arguments);
               });
        case 0x6au: // i32.add
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return ir_builder.CreateAdd(left.value, right.value); });
        case 0x6bu: // i32.sub
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return ir_builder.CreateSub(left.value, right.value); });
        case 0x6cu: // i32.mul
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return ir_builder.CreateMul(left.value, right.value); });
        case 0x6du: // i32.div_s
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               {
                   auto llvm_module{state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   emit_llvm_signed_div_overflow_trap(*llvm_module, ir_builder, left.value, right.value);
                   return ir_builder.CreateSDiv(left.value, right.value);
               });
        case 0x6eu: // i32.div_u
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               {
                   auto llvm_module{state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   emit_llvm_divide_by_zero_trap(*llvm_module, ir_builder, right.value);
                   return ir_builder.CreateUDiv(left.value, right.value);
               });
        case 0x6fu: // i32.rem_s
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               {
                   auto llvm_module{state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }
                   return emit_llvm_signed_remainder_with_wasm_semantics(*llvm_module, ir_builder, left.value, right.value);
               });
        case 0x70u: // i32.rem_u
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               {
                   auto llvm_module{state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   emit_llvm_divide_by_zero_trap(*llvm_module, ir_builder, right.value);
                   return ir_builder.CreateURem(left.value, right.value);
               });
        case 0x71u: // i32.and
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return ir_builder.CreateAnd(left.value, right.value); });
        case 0x72u: // i32.or
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return ir_builder.CreateOr(left.value, right.value); });
        case 0x73u: // i32.xor
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return ir_builder.CreateXor(left.value, right.value); });
        case 0x74u: // i32.shl
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return ir_builder.CreateShl(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); });
        case 0x75u: // i32.shr_s
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return ir_builder.CreateAShr(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); });
        case 0x76u: // i32.shr_u
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return ir_builder.CreateLShr(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); });
        case 0x77u: // i32.rotl
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return emit_llvm_rotl(ir_builder, left.value, right.value); });
        case 0x78u: // i32.rotr
            return try_emit_runtime_local_func_llvm_jit_binary(
               state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) constexpr noexcept
               { return emit_llvm_rotr(ir_builder, left.value, right.value); });
        default: return false;
    }
}

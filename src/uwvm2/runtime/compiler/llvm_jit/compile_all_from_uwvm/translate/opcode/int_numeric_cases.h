case wasm1_code::i32_clz:
{
    validate_numeric_unary(u8"i32.clz", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               {
                   auto llvm_module{llvm_jit_emit_state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value, ::llvm::ConstantInt::getFalse(ir_builder.getContext())};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::ctlz, overloaded_types, arguments);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_ctz:
{
    validate_numeric_unary(u8"i32.ctz", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               {
                   auto llvm_module{llvm_jit_emit_state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value, ::llvm::ConstantInt::getFalse(ir_builder.getContext())};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::cttz, overloaded_types, arguments);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_popcnt:
{
    validate_numeric_unary(u8"i32.popcnt", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               {
                   auto llvm_module{llvm_jit_emit_state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::ctpop, overloaded_types, arguments);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_add:
{
    validate_numeric_binary(u8"i32.add", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateAdd(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_sub:
{
    validate_numeric_binary(u8"i32.sub", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateSub(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_mul:
{
    validate_numeric_binary(u8"i32.mul", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateMul(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_div_s:
{
    validate_numeric_binary(u8"i32.div_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               {
                   auto llvm_module{llvm_jit_emit_state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   emit_llvm_signed_div_overflow_trap(*llvm_module, ir_builder, left.value, right.value);
                   return ir_builder.CreateSDiv(left.value, right.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_div_u:
{
    validate_numeric_binary(u8"i32.div_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               {
                   auto llvm_module{llvm_jit_emit_state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   emit_llvm_divide_by_zero_trap(*llvm_module, ir_builder, right.value);
                   return ir_builder.CreateUDiv(left.value, right.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_rem_s:
{
    validate_numeric_binary(u8"i32.rem_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               {
                   auto llvm_module{llvm_jit_emit_state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }
                   return emit_llvm_signed_remainder_with_wasm_semantics(*llvm_module, ir_builder, left.value, right.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_rem_u:
{
    validate_numeric_binary(u8"i32.rem_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               {
                   auto llvm_module{llvm_jit_emit_state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   emit_llvm_divide_by_zero_trap(*llvm_module, ir_builder, right.value);
                   return ir_builder.CreateURem(left.value, right.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_and:
{
    validate_numeric_binary(u8"i32.and", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateAnd(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_or:
{
    validate_numeric_binary(u8"i32.or", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateOr(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_xor:
{
    validate_numeric_binary(u8"i32.xor", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateXor(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_shl:
{
    validate_numeric_binary(u8"i32.shl", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateShl(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_shr_s:
{
    validate_numeric_binary(u8"i32.shr_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateAShr(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_shr_u:
{
    validate_numeric_binary(u8"i32.shr_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateLShr(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_rotl:
{
    validate_numeric_binary(u8"i32.rotl", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return emit_llvm_rotl(ir_builder, left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_rotr:
{
    validate_numeric_binary(u8"i32.rotr", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return emit_llvm_rotr(ir_builder, left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_clz:
{
    validate_numeric_unary(u8"i64.clz", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               {
                   auto llvm_module{llvm_jit_emit_state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value, ::llvm::ConstantInt::getFalse(ir_builder.getContext())};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::ctlz, overloaded_types, arguments);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_ctz:
{
    validate_numeric_unary(u8"i64.ctz", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               {
                   auto llvm_module{llvm_jit_emit_state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value, ::llvm::ConstantInt::getFalse(ir_builder.getContext())};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::cttz, overloaded_types, arguments);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_popcnt:
{
    validate_numeric_unary(u8"i64.popcnt", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               {
                   auto llvm_module{llvm_jit_emit_state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::ctpop, overloaded_types, arguments);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_add:
{
    validate_numeric_binary(u8"i64.add", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateAdd(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_sub:
{
    validate_numeric_binary(u8"i64.sub", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateSub(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_mul:
{
    validate_numeric_binary(u8"i64.mul", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateMul(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_div_s:
{
    validate_numeric_binary(u8"i64.div_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               {
                   auto llvm_module{llvm_jit_emit_state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   emit_llvm_signed_div_overflow_trap(*llvm_module, ir_builder, left.value, right.value);
                   return ir_builder.CreateSDiv(left.value, right.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_div_u:
{
    validate_numeric_binary(u8"i64.div_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               {
                   auto llvm_module{llvm_jit_emit_state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   emit_llvm_divide_by_zero_trap(*llvm_module, ir_builder, right.value);
                   return ir_builder.CreateUDiv(left.value, right.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_rem_s:
{
    validate_numeric_binary(u8"i64.rem_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               {
                   auto llvm_module{llvm_jit_emit_state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }
                   return emit_llvm_signed_remainder_with_wasm_semantics(*llvm_module, ir_builder, left.value, right.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_rem_u:
{
    validate_numeric_binary(u8"i64.rem_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               {
                   auto llvm_module{llvm_jit_emit_state.llvm_module};
                   if(llvm_module == nullptr) [[unlikely]] { return static_cast<::llvm::Value*>(nullptr); }

                   emit_llvm_divide_by_zero_trap(*llvm_module, ir_builder, right.value);
                   return ir_builder.CreateURem(left.value, right.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_and:
{
    validate_numeric_binary(u8"i64.and", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateAnd(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_or:
{
    validate_numeric_binary(u8"i64.or", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateOr(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_xor:
{
    validate_numeric_binary(u8"i64.xor", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateXor(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_shl:
{
    validate_numeric_binary(u8"i64.shl", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateShl(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_shr_s:
{
    validate_numeric_binary(u8"i64.shr_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateAShr(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_shr_u:
{
    validate_numeric_binary(u8"i64.shr_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateLShr(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_rotl:
{
    validate_numeric_binary(u8"i64.rotl", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return emit_llvm_rotl(ir_builder, left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_rotr:
{
    validate_numeric_binary(u8"i64.rotr", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return emit_llvm_rotr(ir_builder, left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

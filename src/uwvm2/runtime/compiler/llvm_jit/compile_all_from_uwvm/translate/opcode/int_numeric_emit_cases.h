case wasm1_code::i32_clz:
{
    ++code_curr;
    if(!emit_unary(runtime_operand_stack_value_type::i32,
                   runtime_operand_stack_value_type::i32,
                   [&](llvm_jit_stack_value_t const& operand)
                   {
                       ::llvm::Type* overloaded_types[]{operand.value->getType()};
                       ::llvm::Value* arguments[]{operand.value, ::llvm::ConstantInt::getFalse(llvm_context)};
                       return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::ctlz, overloaded_types, arguments);
                   }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_ctz:
{
    ++code_curr;
    if(!emit_unary(runtime_operand_stack_value_type::i32,
                   runtime_operand_stack_value_type::i32,
                   [&](llvm_jit_stack_value_t const& operand)
                   {
                       ::llvm::Type* overloaded_types[]{operand.value->getType()};
                       ::llvm::Value* arguments[]{operand.value, ::llvm::ConstantInt::getFalse(llvm_context)};
                       return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::cttz, overloaded_types, arguments);
                   }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_popcnt:
{
    ++code_curr;
    if(!emit_unary(runtime_operand_stack_value_type::i32,
                   runtime_operand_stack_value_type::i32,
                   [&](llvm_jit_stack_value_t const& operand)
                   {
                       ::llvm::Type* overloaded_types[]{operand.value->getType()};
                       ::llvm::Value* arguments[]{operand.value};
                       return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::ctpop, overloaded_types, arguments);
                   }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_clz:
{
    ++code_curr;
    if(!emit_unary(runtime_operand_stack_value_type::i64,
                   runtime_operand_stack_value_type::i64,
                   [&](llvm_jit_stack_value_t const& operand)
                   {
                       ::llvm::Type* overloaded_types[]{operand.value->getType()};
                       ::llvm::Value* arguments[]{operand.value, ::llvm::ConstantInt::getFalse(llvm_context)};
                       return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::ctlz, overloaded_types, arguments);
                   }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_ctz:
{
    ++code_curr;
    if(!emit_unary(runtime_operand_stack_value_type::i64,
                   runtime_operand_stack_value_type::i64,
                   [&](llvm_jit_stack_value_t const& operand)
                   {
                       ::llvm::Type* overloaded_types[]{operand.value->getType()};
                       ::llvm::Value* arguments[]{operand.value, ::llvm::ConstantInt::getFalse(llvm_context)};
                       return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::cttz, overloaded_types, arguments);
                   }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_popcnt:
{
    ++code_curr;
    if(!emit_unary(runtime_operand_stack_value_type::i64,
                   runtime_operand_stack_value_type::i64,
                   [&](llvm_jit_stack_value_t const& operand)
                   {
                       ::llvm::Type* overloaded_types[]{operand.value->getType()};
                       ::llvm::Value* arguments[]{operand.value};
                       return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::ctpop, overloaded_types, arguments);
                   }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_add:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return ir_builder.CreateAdd(left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_sub:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return ir_builder.CreateSub(left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_mul:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return ir_builder.CreateMul(left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_and:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return ir_builder.CreateAnd(left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_or:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return ir_builder.CreateOr(left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_xor:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return ir_builder.CreateXor(left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_shl:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
                    { return ir_builder.CreateShl(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_shr_s:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
                    { return ir_builder.CreateAShr(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_shr_u:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
                    { return ir_builder.CreateLShr(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_rotl:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return emit_llvm_rotl(ir_builder, left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_rotr:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return emit_llvm_rotr(ir_builder, left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_add:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return ir_builder.CreateAdd(left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_sub:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return ir_builder.CreateSub(left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_mul:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return ir_builder.CreateMul(left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_and:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return ir_builder.CreateAnd(left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_or:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return ir_builder.CreateOr(left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_xor:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return ir_builder.CreateXor(left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_shl:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
                    { return ir_builder.CreateShl(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_shr_s:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
                    { return ir_builder.CreateAShr(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_shr_u:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
                    { return ir_builder.CreateLShr(left.value, emit_llvm_shift_count_mask(ir_builder, right.value, get_llvm_integer_bit_width(left.value))); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_rotl:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return emit_llvm_rotl(ir_builder, left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_rotr:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) { return emit_llvm_rotr(ir_builder, left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_div_s:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
                    {
                        emit_llvm_signed_div_overflow_trap(*llvm_module, ir_builder, left.value, right.value);
                        return ir_builder.CreateSDiv(left.value, right.value);
                    }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_div_u:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
                    {
                        emit_llvm_divide_by_zero_trap(*llvm_module, ir_builder, right.value);
                        return ir_builder.CreateUDiv(left.value, right.value);
                    }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_rem_s:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
                    { return emit_llvm_signed_remainder_with_wasm_semantics(*llvm_module, ir_builder, left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i32_rem_u:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i32,
                    runtime_operand_stack_value_type::i32,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
                    {
                        emit_llvm_divide_by_zero_trap(*llvm_module, ir_builder, right.value);
                        return ir_builder.CreateURem(left.value, right.value);
                    }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_div_s:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
                    {
                        emit_llvm_signed_div_overflow_trap(*llvm_module, ir_builder, left.value, right.value);
                        return ir_builder.CreateSDiv(left.value, right.value);
                    }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_div_u:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
                    {
                        emit_llvm_divide_by_zero_trap(*llvm_module, ir_builder, right.value);
                        return ir_builder.CreateUDiv(left.value, right.value);
                    }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_rem_s:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
                    { return emit_llvm_signed_remainder_with_wasm_semantics(*llvm_module, ir_builder, left.value, right.value); }))
    {
        return result;
    }
    break;
}
case wasm1_code::i64_rem_u:
{
    ++code_curr;
    if(!emit_binary(runtime_operand_stack_value_type::i64,
                    runtime_operand_stack_value_type::i64,
                    [&](llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
                    {
                        emit_llvm_divide_by_zero_trap(*llvm_module, ir_builder, right.value);
                        return ir_builder.CreateURem(left.value, right.value);
                    }))
    {
        return result;
    }
    break;
}

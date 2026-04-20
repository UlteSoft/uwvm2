case wasm1_code::i32_const:
{
    // i32.const i32 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // i32.const i32 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // i32.const i32 ...
    // [ safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32 imm;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(imm))};
    if(imm_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.invalid_const_immediate.op_code_name = u8"i32.const";
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_const_immediate;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(imm_err);
    }

    // i32.const i32 ...
    // [    safe   ] unsafe (could be the section_end)
    //           ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(imm_next);

    // i32.const i32 ...
    // [    safe   ] unsafe (could be the section_end)
    //               ^^ code_curr

    // Stack effect: () -> (i32)
    operand_stack_push(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_constant(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::LLVMContext& llvm_context) -> ::llvm::Value*
               {
                   return ::llvm::ConstantInt::getSigned(::llvm::Type::getInt32Ty(llvm_context), static_cast<::std::int_least64_t>(imm));
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_const:
{
    // i64.const i64 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // i64.const i64 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // i64.const i64 ...
    // [ safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64 imm;  // No initialization necessary

    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;

    auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(imm))};
    if(imm_err != ::fast_io::parse_code::ok) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.invalid_const_immediate.op_code_name = u8"i64.const";
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_const_immediate;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(imm_err);
    }

    // i64.const i64 ...
    // [     safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    code_curr = reinterpret_cast<::std::byte const*>(imm_next);

    // i64.const i64 ...
    // [     safe  ] unsafe (could be the section_end)
    //               ^^ code_curr

    // Stack effect: () -> (i64)
    operand_stack_push(::uwvm2::parser::wasm::standard::wasm1::type::value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_constant(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::LLVMContext& llvm_context) -> ::llvm::Value*
               {
                   return ::llvm::ConstantInt::getSigned(::llvm::Type::getInt64Ty(llvm_context), static_cast<::std::int_least64_t>(imm));
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_const:
{
    // f32.const f32 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // f32.const f32 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // f32.const f32 ...
    // [ safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    ::std::uint32_t bits{};

    if(static_cast<::std::size_t>(code_end - code_curr) < sizeof(bits)) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.invalid_const_immediate.op_code_name = u8"f32.const";
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_const_immediate;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
    }

    for(::std::size_t byte_index{}; byte_index != sizeof(bits); ++byte_index)
    {
        bits |= static_cast<::std::uint32_t>(::std::to_integer<::std::uint_least8_t>(code_curr[byte_index])) << (byte_index * 8u);
    }

    // f32.const f32 ...
    // [ safe      ] unsafe (could be the section_end)
    //           ^^ code_curr

    code_curr += sizeof(bits);

    // f32.const f32 ...
    // [ safe      ] unsafe (could be the section_end)
    //               ^^ code_curr

    // Stack effect: () -> (f32)
    operand_stack_push(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_constant(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f32,
               [&](::llvm::LLVMContext& llvm_context) -> ::llvm::Value* { return get_llvm_f32_constant_from_bits(llvm_context, bits); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f64_const:
{
    // f64.const f64 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ code_curr

    auto const op_begin{code_curr};

    // f64.const f64 ...
    // [ safe  ] unsafe (could be the section_end)
    // ^^ op_begin

    ++code_curr;

    // f64.const f64 ...
    // [ safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    ::std::uint64_t bits{};

    if(static_cast<::std::size_t>(code_end - code_curr) < sizeof(bits)) [[unlikely]]
    {
        err.err_curr = op_begin;
        err.err_selectable.invalid_const_immediate.op_code_name = u8"f64.const";
        err.err_code = ::uwvm2::validation::error::code_validation_error_code::invalid_const_immediate;
        ::uwvm2::parser::wasm::base::throw_wasm_parse_code(::fast_io::parse_code::end_of_file);
    }

    for(::std::size_t byte_index{}; byte_index != sizeof(bits); ++byte_index)
    {
        bits |= static_cast<::std::uint64_t>(::std::to_integer<::std::uint_least8_t>(code_curr[byte_index])) << (byte_index * 8u);
    }

    // f64.const f64 ...
    // [     safe  ] unsafe (could be the section_end)
    //           ^^ code_curr

    code_curr += sizeof(bits);

    // f64.const f64 ...
    // [     safe  ] unsafe (could be the section_end)
    //               ^^ code_curr

    // Stack effect: () -> (f64)
    operand_stack_push(::uwvm2::parser::wasm::standard::wasm1::type::value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_constant(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f64,
               [&](::llvm::LLVMContext& llvm_context) -> ::llvm::Value* { return get_llvm_f64_constant_from_bits(llvm_context, bits); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_eqz:
{
    validate_numeric_unary(u8"i32.eqz", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpEQ(operand.value, ::llvm::ConstantInt::get(operand.value->getType(), 0u))); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_eq:
{
    validate_numeric_binary(u8"i32.eq", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpEQ(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_ne:
{
    validate_numeric_binary(u8"i32.ne", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpNE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_lt_s:
{
    validate_numeric_binary(u8"i32.lt_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSLT(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_lt_u:
{
    validate_numeric_binary(u8"i32.lt_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpULT(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_gt_s:
{
    validate_numeric_binary(u8"i32.gt_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSGT(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_gt_u:
{
    validate_numeric_binary(u8"i32.gt_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpUGT(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_le_s:
{
    validate_numeric_binary(u8"i32.le_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSLE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_le_u:
{
    validate_numeric_binary(u8"i32.le_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpULE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_ge_s:
{
    validate_numeric_binary(u8"i32.ge_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSGE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_ge_u:
{
    validate_numeric_binary(u8"i32.ge_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpUGE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_eqz:
{
    validate_numeric_unary(u8"i64.eqz", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpEQ(operand.value, ::llvm::ConstantInt::get(operand.value->getType(), 0u))); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_eq:
{
    validate_numeric_binary(u8"i64.eq", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpEQ(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_ne:
{
    validate_numeric_binary(u8"i64.ne", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpNE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_lt_s:
{
    validate_numeric_binary(u8"i64.lt_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSLT(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_lt_u:
{
    validate_numeric_binary(u8"i64.lt_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpULT(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_gt_s:
{
    validate_numeric_binary(u8"i64.gt_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSGT(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_gt_u:
{
    validate_numeric_binary(u8"i64.gt_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpUGT(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_le_s:
{
    validate_numeric_binary(u8"i64.le_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSLE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_le_u:
{
    validate_numeric_binary(u8"i64.le_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpULE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_ge_s:
{
    validate_numeric_binary(u8"i64.ge_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpSGE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_ge_u:
{
    validate_numeric_binary(u8"i64.ge_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateICmpUGE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_eq:
{
    validate_numeric_binary(u8"f32.eq", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateFCmpOEQ(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_ne:
{
    validate_numeric_binary(u8"f32.ne", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateFCmpUNE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_lt:
{
    validate_numeric_binary(u8"f32.lt", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateFCmpOLT(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_gt:
{
    validate_numeric_binary(u8"f32.gt", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateFCmpOGT(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_le:
{
    validate_numeric_binary(u8"f32.le", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateFCmpOLE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_ge:
{
    validate_numeric_binary(u8"f32.ge", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateFCmpOGE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f64_eq:
{
    validate_numeric_binary(u8"f64.eq", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateFCmpOEQ(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f64_ne:
{
    validate_numeric_binary(u8"f64.ne", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateFCmpUNE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f64_lt:
{
    validate_numeric_binary(u8"f64.lt", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateFCmpOLT(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f64_gt:
{
    validate_numeric_binary(u8"f64.gt", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateFCmpOGT(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f64_le:
{
    validate_numeric_binary(u8"f64.le", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateFCmpOLE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f64_ge:
{
    validate_numeric_binary(u8"f64.ge", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return coerce_llvm_bool_to_i32(ir_builder, ir_builder.CreateFCmpOGE(left.value, right.value)); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

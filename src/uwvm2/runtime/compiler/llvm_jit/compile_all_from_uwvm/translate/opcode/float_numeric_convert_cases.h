case wasm1_code::f32_abs:
case wasm1_code::f64_abs:
{
    auto const is_f32{curr_opbase == wasm1_code::f32_abs};
    auto const op_name{is_f32 ? ::uwvm2::utils::container::u8string_view{u8"f32.abs"} : ::uwvm2::utils::container::u8string_view{u8"f64.abs"}};
    auto const operand_type{is_f32 ? curr_operand_stack_value_type::f32 : curr_operand_stack_value_type::f64};
    auto const llvm_operand_type{is_f32 ? runtime_operand_stack_value_type::f32 : runtime_operand_stack_value_type::f64};

    validate_numeric_unary(op_name, operand_type, operand_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               llvm_operand_type,
               llvm_operand_type,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::fabs, overloaded_types, arguments);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_neg:
case wasm1_code::f64_neg:
{
    auto const is_f32{curr_opbase == wasm1_code::f32_neg};
    auto const op_name{is_f32 ? ::uwvm2::utils::container::u8string_view{u8"f32.neg"} : ::uwvm2::utils::container::u8string_view{u8"f64.neg"}};
    auto const operand_type{is_f32 ? curr_operand_stack_value_type::f32 : curr_operand_stack_value_type::f64};
    auto const llvm_operand_type{is_f32 ? runtime_operand_stack_value_type::f32 : runtime_operand_stack_value_type::f64};

    validate_numeric_unary(op_name, operand_type, operand_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               llvm_operand_type,
               llvm_operand_type,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) { return ir_builder.CreateFNeg(operand.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_ceil:
case wasm1_code::f64_ceil:
{
    auto const is_f32{curr_opbase == wasm1_code::f32_ceil};
    auto const op_name{is_f32 ? ::uwvm2::utils::container::u8string_view{u8"f32.ceil"} : ::uwvm2::utils::container::u8string_view{u8"f64.ceil"}};
    auto const operand_type{is_f32 ? curr_operand_stack_value_type::f32 : curr_operand_stack_value_type::f64};
    auto const llvm_operand_type{is_f32 ? runtime_operand_stack_value_type::f32 : runtime_operand_stack_value_type::f64};

    validate_numeric_unary(op_name, operand_type, operand_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               llvm_operand_type,
               llvm_operand_type,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::ceil, overloaded_types, arguments);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_floor:
case wasm1_code::f64_floor:
{
    auto const is_f32{curr_opbase == wasm1_code::f32_floor};
    auto const op_name{is_f32 ? ::uwvm2::utils::container::u8string_view{u8"f32.floor"} : ::uwvm2::utils::container::u8string_view{u8"f64.floor"}};
    auto const operand_type{is_f32 ? curr_operand_stack_value_type::f32 : curr_operand_stack_value_type::f64};
    auto const llvm_operand_type{is_f32 ? runtime_operand_stack_value_type::f32 : runtime_operand_stack_value_type::f64};

    validate_numeric_unary(op_name, operand_type, operand_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               llvm_operand_type,
               llvm_operand_type,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::floor, overloaded_types, arguments);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_trunc:
case wasm1_code::f64_trunc:
{
    auto const is_f32{curr_opbase == wasm1_code::f32_trunc};
    auto const op_name{is_f32 ? ::uwvm2::utils::container::u8string_view{u8"f32.trunc"} : ::uwvm2::utils::container::u8string_view{u8"f64.trunc"}};
    auto const operand_type{is_f32 ? curr_operand_stack_value_type::f32 : curr_operand_stack_value_type::f64};
    auto const llvm_operand_type{is_f32 ? runtime_operand_stack_value_type::f32 : runtime_operand_stack_value_type::f64};

    validate_numeric_unary(op_name, operand_type, operand_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               llvm_operand_type,
               llvm_operand_type,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::trunc, overloaded_types, arguments);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_nearest:
case wasm1_code::f64_nearest:
{
    auto const is_f32{curr_opbase == wasm1_code::f32_nearest};
    auto const op_name{is_f32 ? ::uwvm2::utils::container::u8string_view{u8"f32.nearest"} : ::uwvm2::utils::container::u8string_view{u8"f64.nearest"}};
    auto const operand_type{is_f32 ? curr_operand_stack_value_type::f32 : curr_operand_stack_value_type::f64};
    auto const llvm_operand_type{is_f32 ? runtime_operand_stack_value_type::f32 : runtime_operand_stack_value_type::f64};

    validate_numeric_unary(op_name, operand_type, operand_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               llvm_operand_type,
               llvm_operand_type,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::rint, overloaded_types, arguments);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_sqrt:
case wasm1_code::f64_sqrt:
{
    auto const is_f32{curr_opbase == wasm1_code::f32_sqrt};
    auto const op_name{is_f32 ? ::uwvm2::utils::container::u8string_view{u8"f32.sqrt"} : ::uwvm2::utils::container::u8string_view{u8"f64.sqrt"}};
    auto const operand_type{is_f32 ? curr_operand_stack_value_type::f32 : curr_operand_stack_value_type::f64};
    auto const llvm_operand_type{is_f32 ? runtime_operand_stack_value_type::f32 : runtime_operand_stack_value_type::f64};

    validate_numeric_unary(op_name, operand_type, operand_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               llvm_operand_type,
               llvm_operand_type,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   ::llvm::Type* overloaded_types[]{operand.value->getType()};
                   ::llvm::Value* arguments[]{operand.value};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::sqrt, overloaded_types, arguments);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_add:
case wasm1_code::f64_add:
{
    auto const is_f32{curr_opbase == wasm1_code::f32_add};
    auto const op_name{is_f32 ? ::uwvm2::utils::container::u8string_view{u8"f32.add"} : ::uwvm2::utils::container::u8string_view{u8"f64.add"}};
    auto const operand_type{is_f32 ? curr_operand_stack_value_type::f32 : curr_operand_stack_value_type::f64};
    auto const llvm_operand_type{is_f32 ? runtime_operand_stack_value_type::f32 : runtime_operand_stack_value_type::f64};

    validate_numeric_binary(op_name, operand_type, operand_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               llvm_operand_type,
               llvm_operand_type,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateFAdd(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_sub:
case wasm1_code::f64_sub:
{
    auto const is_f32{curr_opbase == wasm1_code::f32_sub};
    auto const op_name{is_f32 ? ::uwvm2::utils::container::u8string_view{u8"f32.sub"} : ::uwvm2::utils::container::u8string_view{u8"f64.sub"}};
    auto const operand_type{is_f32 ? curr_operand_stack_value_type::f32 : curr_operand_stack_value_type::f64};
    auto const llvm_operand_type{is_f32 ? runtime_operand_stack_value_type::f32 : runtime_operand_stack_value_type::f64};

    validate_numeric_binary(op_name, operand_type, operand_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               llvm_operand_type,
               llvm_operand_type,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateFSub(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_mul:
case wasm1_code::f64_mul:
{
    auto const is_f32{curr_opbase == wasm1_code::f32_mul};
    auto const op_name{is_f32 ? ::uwvm2::utils::container::u8string_view{u8"f32.mul"} : ::uwvm2::utils::container::u8string_view{u8"f64.mul"}};
    auto const operand_type{is_f32 ? curr_operand_stack_value_type::f32 : curr_operand_stack_value_type::f64};
    auto const llvm_operand_type{is_f32 ? runtime_operand_stack_value_type::f32 : runtime_operand_stack_value_type::f64};

    validate_numeric_binary(op_name, operand_type, operand_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               llvm_operand_type,
               llvm_operand_type,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateFMul(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_div:
case wasm1_code::f64_div:
{
    auto const is_f32{curr_opbase == wasm1_code::f32_div};
    auto const op_name{is_f32 ? ::uwvm2::utils::container::u8string_view{u8"f32.div"} : ::uwvm2::utils::container::u8string_view{u8"f64.div"}};
    auto const operand_type{is_f32 ? curr_operand_stack_value_type::f32 : curr_operand_stack_value_type::f64};
    auto const llvm_operand_type{is_f32 ? runtime_operand_stack_value_type::f32 : runtime_operand_stack_value_type::f64};

    validate_numeric_binary(op_name, operand_type, operand_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               llvm_operand_type,
               llvm_operand_type,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return ir_builder.CreateFDiv(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_min:
case wasm1_code::f64_min:
{
    auto const is_f32{curr_opbase == wasm1_code::f32_min};
    auto const op_name{is_f32 ? ::uwvm2::utils::container::u8string_view{u8"f32.min"} : ::uwvm2::utils::container::u8string_view{u8"f64.min"}};
    auto const operand_type{is_f32 ? curr_operand_stack_value_type::f32 : curr_operand_stack_value_type::f64};
    auto const llvm_operand_type{is_f32 ? runtime_operand_stack_value_type::f32 : runtime_operand_stack_value_type::f64};

    validate_numeric_binary(op_name, operand_type, operand_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               llvm_operand_type,
               llvm_operand_type,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return emit_llvm_float_min(ir_builder, left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_max:
case wasm1_code::f64_max:
{
    auto const is_f32{curr_opbase == wasm1_code::f32_max};
    auto const op_name{is_f32 ? ::uwvm2::utils::container::u8string_view{u8"f32.max"} : ::uwvm2::utils::container::u8string_view{u8"f64.max"}};
    auto const operand_type{is_f32 ? curr_operand_stack_value_type::f32 : curr_operand_stack_value_type::f64};
    auto const llvm_operand_type{is_f32 ? runtime_operand_stack_value_type::f32 : runtime_operand_stack_value_type::f64};

    validate_numeric_binary(op_name, operand_type, operand_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               llvm_operand_type,
               llvm_operand_type,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right)
               { return emit_llvm_float_max(ir_builder, left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_copysign:
case wasm1_code::f64_copysign:
{
    auto const is_f32{curr_opbase == wasm1_code::f32_copysign};
    auto const op_name{is_f32 ? ::uwvm2::utils::container::u8string_view{u8"f32.copysign"} : ::uwvm2::utils::container::u8string_view{u8"f64.copysign"}};
    auto const operand_type{is_f32 ? curr_operand_stack_value_type::f32 : curr_operand_stack_value_type::f64};
    auto const llvm_operand_type{is_f32 ? runtime_operand_stack_value_type::f32 : runtime_operand_stack_value_type::f64};

    validate_numeric_binary(op_name, operand_type, operand_type);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_binary(
               llvm_jit_emit_state,
               llvm_operand_type,
               llvm_operand_type,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   ::llvm::Type* overloaded_types[]{left.value->getType()};
                   ::llvm::Value* arguments[]{left.value, right.value};
                   return call_llvm_intrinsic(*llvm_module, ir_builder, ::llvm::Intrinsic::copysign, overloaded_types, arguments);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_wrap_i64:
{
    validate_numeric_unary(u8"i32.wrap_i64", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateTrunc(operand.value, ::llvm::Type::getInt32Ty(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_trunc_f32_s:
{
    validate_numeric_unary(u8"i32.trunc_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   return emit_llvm_trunc_float_to_int<float>(
                       *llvm_module, ir_builder, ::llvm::Type::getInt32Ty(ir_builder.getContext()), true, -2147483904.0f, 2147483648.0f, operand.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_trunc_f64_s:
{
    validate_numeric_unary(u8"i32.trunc_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   return emit_llvm_trunc_float_to_int<double>(
                       *llvm_module, ir_builder, ::llvm::Type::getInt32Ty(ir_builder.getContext()), true, -2147483649.0, 2147483648.0, operand.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_trunc_f32_u:
{
    validate_numeric_unary(u8"i32.trunc_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   return emit_llvm_trunc_float_to_int<float>(
                       *llvm_module, ir_builder, ::llvm::Type::getInt32Ty(ir_builder.getContext()), false, -1.0f, 4294967296.0f, operand.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_trunc_f64_u:
{
    validate_numeric_unary(u8"i32.trunc_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f64,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   return emit_llvm_trunc_float_to_int<double>(
                       *llvm_module, ir_builder, ::llvm::Type::getInt32Ty(ir_builder.getContext()), false, -1.0, 4294967296.0, operand.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_extend_i32_s:
{
    validate_numeric_unary(u8"i64.extend_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateSExt(operand.value, ::llvm::Type::getInt64Ty(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_extend_i32_u:
{
    validate_numeric_unary(u8"i64.extend_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateZExt(operand.value, ::llvm::Type::getInt64Ty(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_trunc_f32_s:
{
    validate_numeric_unary(u8"i64.trunc_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f32,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   return emit_llvm_trunc_float_to_int<float>(*llvm_module,
                                                              ir_builder,
                                                              ::llvm::Type::getInt64Ty(ir_builder.getContext()),
                                                              true,
                                                              -9223373136366403584.0f,
                                                              9223372036854775808.0f,
                                                              operand.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_trunc_f64_s:
{
    validate_numeric_unary(u8"i64.trunc_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   return emit_llvm_trunc_float_to_int<double>(*llvm_module,
                                                               ir_builder,
                                                               ::llvm::Type::getInt64Ty(ir_builder.getContext()),
                                                               true,
                                                               -9223372036854777856.0,
                                                               9223372036854775808.0,
                                                               operand.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_trunc_f32_u:
{
    validate_numeric_unary(u8"i64.trunc_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f32,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   return emit_llvm_trunc_float_to_int<float>(*llvm_module,
                                                              ir_builder,
                                                              ::llvm::Type::getInt64Ty(ir_builder.getContext()),
                                                              false,
                                                              -1.0f,
                                                              18446744073709551616.0f,
                                                              operand.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_trunc_f64_u:
{
    validate_numeric_unary(u8"i64.trunc_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) -> ::llvm::Value*
               {
                   auto insert_block{ir_builder.GetInsertBlock()};
                   auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                   auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                   if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                   return emit_llvm_trunc_float_to_int<double>(*llvm_module,
                                                               ir_builder,
                                                               ::llvm::Type::getInt64Ty(ir_builder.getContext()),
                                                               false,
                                                               -1.0,
                                                               18446744073709551616.0,
                                                               operand.value);
               })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_convert_i32_s:
{
    validate_numeric_unary(u8"f32.convert_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::f32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateSIToFP(operand.value, ::llvm::Type::getFloatTy(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_convert_i32_u:
{
    validate_numeric_unary(u8"f32.convert_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::f32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateUIToFP(operand.value, ::llvm::Type::getFloatTy(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_convert_i64_s:
{
    validate_numeric_unary(u8"f32.convert_i64_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::f32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateSIToFP(operand.value, ::llvm::Type::getFloatTy(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_convert_i64_u:
{
    validate_numeric_unary(u8"f32.convert_i64_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::f32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateUIToFP(operand.value, ::llvm::Type::getFloatTy(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_demote_f64:
{
    validate_numeric_unary(u8"f32.demote_f64", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f64,
               runtime_operand_stack_value_type::f32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateFPTrunc(operand.value, ::llvm::Type::getFloatTy(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f64_convert_i32_s:
{
    validate_numeric_unary(u8"f64.convert_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::f64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateSIToFP(operand.value, ::llvm::Type::getDoubleTy(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f64_convert_i32_u:
{
    validate_numeric_unary(u8"f64.convert_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::f64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateUIToFP(operand.value, ::llvm::Type::getDoubleTy(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f64_convert_i64_s:
{
    validate_numeric_unary(u8"f64.convert_i64_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::f64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateSIToFP(operand.value, ::llvm::Type::getDoubleTy(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f64_convert_i64_u:
{
    validate_numeric_unary(u8"f64.convert_i64_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::f64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateUIToFP(operand.value, ::llvm::Type::getDoubleTy(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f64_promote_f32:
{
    validate_numeric_unary(u8"f64.promote_f32", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f32,
               runtime_operand_stack_value_type::f64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateFPExt(operand.value, ::llvm::Type::getDoubleTy(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i32_reinterpret_f32:
{
    validate_numeric_unary(u8"i32.reinterpret_f32", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f32,
               runtime_operand_stack_value_type::i32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateBitCast(operand.value, ::llvm::Type::getInt32Ty(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::i64_reinterpret_f64:
{
    validate_numeric_unary(u8"i64.reinterpret_f64", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::f64,
               runtime_operand_stack_value_type::i64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateBitCast(operand.value, ::llvm::Type::getInt64Ty(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f32_reinterpret_i32:
{
    validate_numeric_unary(u8"f32.reinterpret_i32", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i32,
               runtime_operand_stack_value_type::f32,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateBitCast(operand.value, ::llvm::Type::getFloatTy(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}
case wasm1_code::f64_reinterpret_i64:
{
    validate_numeric_unary(u8"f64.reinterpret_i64", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(
               llvm_jit_emit_state,
               runtime_operand_stack_value_type::i64,
               runtime_operand_stack_value_type::f64,
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand)
               { return ir_builder.CreateBitCast(operand.value, ::llvm::Type::getDoubleTy(ir_builder.getContext())); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

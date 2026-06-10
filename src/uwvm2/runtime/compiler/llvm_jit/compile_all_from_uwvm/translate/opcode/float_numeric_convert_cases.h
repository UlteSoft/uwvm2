// Floating-point numeric operators and numeric conversions for WebAssembly 1.0/MVP scalar values.
// Future proposal value spaces must add their own opcode decode and stack tags rather than assuming these f32/f64/i32/i64
// cases describe the expanded type system.
// `validate_numeric_unary` / `validate_numeric_binary` consume the opcode byte, enforce the
// WebAssembly operand-stack contract, and push the validated result type.  The LLVM JIT path
// mirrors that stack effect with runtime stack-value tags, then emits the corresponding LLVM IR
// operation.  If an inline emission path cannot obtain the required module/intrinsic state, it
// deliberately falls back to the non-inline runtime path by disabling inline JIT emission.

// f32.abs / f64.abs
// Stack effect: (fN) -> (fN).  WebAssembly absolute value clears the sign bit for finite values,
// infinities, zeros, and NaNs; LLVM's overloaded `fabs` intrinsic is used so the IR keeps the
// operation typed as either float or double without routing through libm.
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
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept -> ::llvm::Value*
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

// f32.neg / f64.neg
// Stack effect: (fN) -> (fN).  Negation flips the sign bit and is not an arithmetic subtraction
// from zero, which matters for signed zero and NaN payload/sign handling.
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
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       llvm_operand_type,
                                                       llvm_operand_type,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateFNeg(operand.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.ceil / f64.ceil
// Stack effect: (fN) -> (fN).  Round toward positive infinity while preserving the floating-point
// type.  The LLVM intrinsic gives the backend a target-independent rounding operation.
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
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept -> ::llvm::Value*
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

// f32.floor / f64.floor
// Stack effect: (fN) -> (fN).  Round toward negative infinity; NaNs and infinities remain in the
// floating-point domain and are handled by the target lowering of the LLVM intrinsic.
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
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept -> ::llvm::Value*
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

// f32.trunc / f64.trunc
// Stack effect: (fN) -> (fN).  Round toward zero to an integral floating-point value.  This is the
// floating-point truncation instruction, not the trapping float-to-integer conversion below.
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
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept -> ::llvm::Value*
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

// f32.nearest / f64.nearest
// Stack effect: (fN) -> (fN).  Round to the nearest integral floating-point value with the
// WebAssembly nearest/ties-to-even semantics represented through LLVM `rint`, keeping the
// operation intrinsic and overloaded on the original operand type.
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
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept -> ::llvm::Value*
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

// f32.sqrt / f64.sqrt
// Stack effect: (fN) -> (fN).  Square root is emitted as LLVM's overloaded `sqrt` intrinsic so the
// generated IR can be optimized or lowered to a hardware instruction when the target supports it.
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
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept -> ::llvm::Value*
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

// f32.add / f64.add
// Stack effect: (fN, fN) -> (fN).  Plain LLVM floating-point add is used without fast-math flags,
// keeping WebAssembly's IEEE-754 observable behavior for NaNs, infinities, and signed zeros.
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
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) noexcept { return ir_builder.CreateFAdd(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.sub / f64.sub
// Stack effect: (fN, fN) -> (fN).  Emitted as a normal LLVM floating-point subtraction with no
// fast-math assumptions, preserving WebAssembly-visible exceptional values.
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
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) noexcept { return ir_builder.CreateFSub(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.mul / f64.mul
// Stack effect: (fN, fN) -> (fN).  Multiplication remains a strict IEEE operation; no reassociation
// or contraction flags are attached here.
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
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) noexcept { return ir_builder.CreateFMul(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.div / f64.div
// Stack effect: (fN, fN) -> (fN).  Floating-point division never traps in WebAssembly; division by
// zero produces IEEE infinities or NaNs, so the JIT emits a direct LLVM `fdiv`.
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
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) noexcept { return ir_builder.CreateFDiv(left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.min / f64.min
// Stack effect: (fN, fN) -> (fN).  WebAssembly `min` has precise NaN propagation and signed-zero
// tie-breaking rules that differ from many native/library min operations, so this uses a custom
// helper instead of LLVM's generic minnum/minimum intrinsics.
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
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) noexcept { return emit_llvm_float_min(ir_builder, left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.max / f64.max
// Stack effect: (fN, fN) -> (fN).  Like `min`, WebAssembly `max` requires explicit handling of
// quieted NaNs and +0.0 versus -0.0 ties; the helper encodes those rules directly in IR.
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
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) noexcept { return emit_llvm_float_max(ir_builder, left.value, right.value); })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.copysign / f64.copysign
// Stack effect: (fN, fN) -> (fN).  The magnitude comes from the left operand and only the sign bit
// is taken from the right operand, including for NaNs and signed zeros.
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
               [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& left, llvm_jit_stack_value_t const& right) noexcept -> ::llvm::Value*
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

// i32.wrap_i64
// Stack effect: (i64) -> (i32).  WebAssembly keeps the low 32 bits and discards the high 32 bits;
// this is a modulo-2^32 wrap, so LLVM `trunc` is the exact representation.
case wasm1_code::i32_wrap_i64:
{
    validate_numeric_unary(u8"i32.wrap_i64", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::i64,
                                                       runtime_operand_stack_value_type::i32,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateTrunc(operand.value, ::llvm::Type::getInt32Ty(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.trunc_f32_s
// Stack effect: (f32) -> (i32).  This is a trapping conversion: NaN traps as an invalid integer
// conversion and values outside the signed i32 range trap as integer overflow before `fptosi`.
case wasm1_code::i32_trunc_f32_s:
{
    validate_numeric_unary(u8"i32.trunc_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::f32,
                                                       runtime_operand_stack_value_type::i32,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept -> ::llvm::Value*
                                                       {
                                                           auto insert_block{ir_builder.GetInsertBlock()};
                                                           auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                                                           auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                                                           if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                                                           return emit_llvm_trunc_float_to_int<float>(*llvm_module,
                                                                                                      ir_builder,
                                                                                                      ::llvm::Type::getInt32Ty(ir_builder.getContext()),
                                                                                                      true,
                                                                                                      -2147483904.0f,
                                                                                                      2147483648.0f,
                                                                                                      operand.value);
                                                       })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.trunc_f64_s
// Stack effect: (f64) -> (i32).  The helper emits WebAssembly's pre-conversion traps, then rounds
// toward zero with LLVM `fptosi`; the lower/upper sentinels bound the legal i32 interval exactly
// for a double-precision source.
case wasm1_code::i32_trunc_f64_s:
{
    validate_numeric_unary(u8"i32.trunc_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::f64,
                                                       runtime_operand_stack_value_type::i32,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept -> ::llvm::Value*
                                                       {
                                                           auto insert_block{ir_builder.GetInsertBlock()};
                                                           auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                                                           auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                                                           if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                                                           return emit_llvm_trunc_float_to_int<double>(*llvm_module,
                                                                                                       ir_builder,
                                                                                                       ::llvm::Type::getInt32Ty(ir_builder.getContext()),
                                                                                                       true,
                                                                                                       -2147483649.0,
                                                                                                       2147483648.0,
                                                                                                       operand.value);
                                                       })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.trunc_f32_u
// Stack effect: (f32) -> (i32).  Unsigned float-to-int truncation has the same NaN/overflow traps
// as the signed form, but the accepted numeric interval is [0, 2^32).
case wasm1_code::i32_trunc_f32_u:
{
    validate_numeric_unary(u8"i32.trunc_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::f32,
                                                       runtime_operand_stack_value_type::i32,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept -> ::llvm::Value*
                                                       {
                                                           auto insert_block{ir_builder.GetInsertBlock()};
                                                           auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                                                           auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                                                           if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                                                           return emit_llvm_trunc_float_to_int<float>(*llvm_module,
                                                                                                      ir_builder,
                                                                                                      ::llvm::Type::getInt32Ty(ir_builder.getContext()),
                                                                                                      false,
                                                                                                      -1.0f,
                                                                                                      4294967296.0f,
                                                                                                      operand.value);
                                                       })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.trunc_f64_u
// Stack effect: (f64) -> (i32).  The resulting i32 holds the low 32-bit unsigned integer value;
// the conversion traps before LLVM `fptoui` can observe NaN or an out-of-range operand.
case wasm1_code::i32_trunc_f64_u:
{
    validate_numeric_unary(u8"i32.trunc_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::f64,
                                                       runtime_operand_stack_value_type::i32,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept -> ::llvm::Value*
                                                       {
                                                           auto insert_block{ir_builder.GetInsertBlock()};
                                                           auto function{insert_block == nullptr ? nullptr : insert_block->getParent()};
                                                           auto llvm_module{function == nullptr ? nullptr : function->getParent()};
                                                           if(llvm_module == nullptr) [[unlikely]] { return nullptr; }

                                                           return emit_llvm_trunc_float_to_int<double>(*llvm_module,
                                                                                                       ir_builder,
                                                                                                       ::llvm::Type::getInt32Ty(ir_builder.getContext()),
                                                                                                       false,
                                                                                                       -1.0,
                                                                                                       4294967296.0,
                                                                                                       operand.value);
                                                       })) [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.extend_i32_s
// Stack effect: (i32) -> (i64).  Sign-extension preserves the signed numeric value of the i32
// operand when widened to 64 bits.
case wasm1_code::i64_extend_i32_s:
{
    validate_numeric_unary(u8"i64.extend_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::i32,
                                                       runtime_operand_stack_value_type::i64,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateSExt(operand.value, ::llvm::Type::getInt64Ty(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.extend_i32_u
// Stack effect: (i32) -> (i64).  Zero-extension treats the i32 operand as an unsigned 32-bit value
// and widens it without sign propagation.
case wasm1_code::i64_extend_i32_u:
{
    validate_numeric_unary(u8"i64.extend_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::i32,
                                                       runtime_operand_stack_value_type::i64,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateZExt(operand.value, ::llvm::Type::getInt64Ty(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.trunc_f32_s
// Stack effect: (f32) -> (i64).  The helper checks WebAssembly's trapping conditions before
// emitting `fptosi`; the f32 lower sentinel is the representable value just below the signed i64
// range needed by the `<= lower_bound` overflow test.
case wasm1_code::i64_trunc_f32_s:
{
    validate_numeric_unary(u8"i64.trunc_f32_s", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::f32,
                                                       runtime_operand_stack_value_type::i64,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept -> ::llvm::Value*
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

// i64.trunc_f64_s
// Stack effect: (f64) -> (i64).  Signed conversion from f64 traps for NaN and values outside
// [-2^63, 2^63), then truncates toward zero.
case wasm1_code::i64_trunc_f64_s:
{
    validate_numeric_unary(u8"i64.trunc_f64_s", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::f64,
                                                       runtime_operand_stack_value_type::i64,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept -> ::llvm::Value*
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

// i64.trunc_f32_u
// Stack effect: (f32) -> (i64).  Unsigned conversion accepts the interval [0, 2^64); the explicit
// guards are required because LLVM `fptoui` is undefined for NaN and out-of-range inputs.
case wasm1_code::i64_trunc_f32_u:
{
    validate_numeric_unary(u8"i64.trunc_f32_u", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::f32,
                                                       runtime_operand_stack_value_type::i64,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept -> ::llvm::Value*
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

// i64.trunc_f64_u
// Stack effect: (f64) -> (i64).  This emits WebAssembly's trapping unsigned conversion to a
// 64-bit integer and only reaches LLVM `fptoui` for values that are defined by the Wasm spec.
case wasm1_code::i64_trunc_f64_u:
{
    validate_numeric_unary(u8"i64.trunc_f64_u", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::f64,
                                                       runtime_operand_stack_value_type::i64,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept -> ::llvm::Value*
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

// f32.convert_i32_s
// Stack effect: (i32) -> (f32).  Signed integer-to-float conversion is non-trapping; values that
// are not exactly representable are rounded by the target's IEEE-754 conversion lowering.
case wasm1_code::f32_convert_i32_s:
{
    validate_numeric_unary(u8"f32.convert_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::i32,
                                                       runtime_operand_stack_value_type::f32,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateSIToFP(operand.value, ::llvm::Type::getFloatTy(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.convert_i32_u
// Stack effect: (i32) -> (f32).  The source bits are interpreted as an unsigned i32 value before
// conversion to f32.
case wasm1_code::f32_convert_i32_u:
{
    validate_numeric_unary(u8"f32.convert_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::i32,
                                                       runtime_operand_stack_value_type::f32,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateUIToFP(operand.value, ::llvm::Type::getFloatTy(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.convert_i64_s
// Stack effect: (i64) -> (f32).  This is a non-trapping signed conversion; large i64 values may
// round because f32 cannot represent every 64-bit integer exactly.
case wasm1_code::f32_convert_i64_s:
{
    validate_numeric_unary(u8"f32.convert_i64_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::i64,
                                                       runtime_operand_stack_value_type::f32,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateSIToFP(operand.value, ::llvm::Type::getFloatTy(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.convert_i64_u
// Stack effect: (i64) -> (f32).  The input is interpreted as an unsigned 64-bit integer and then
// rounded to the nearest representable f32 value by the LLVM conversion.
case wasm1_code::f32_convert_i64_u:
{
    validate_numeric_unary(u8"f32.convert_i64_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::i64,
                                                       runtime_operand_stack_value_type::f32,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateUIToFP(operand.value, ::llvm::Type::getFloatTy(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.demote_f64
// Stack effect: (f64) -> (f32).  Demotion narrows precision and range without trapping; overflow
// becomes an infinity and non-representable finite values are rounded according to IEEE-754.
case wasm1_code::f32_demote_f64:
{
    validate_numeric_unary(u8"f32.demote_f64", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::f64,
                                                       runtime_operand_stack_value_type::f32,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateFPTrunc(operand.value, ::llvm::Type::getFloatTy(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f64.convert_i32_s
// Stack effect: (i32) -> (f64).  Every signed i32 value is exactly representable as f64, so this is
// a straightforward non-trapping signed integer-to-double conversion.
case wasm1_code::f64_convert_i32_s:
{
    validate_numeric_unary(u8"f64.convert_i32_s", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::i32,
                                                       runtime_operand_stack_value_type::f64,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateSIToFP(operand.value, ::llvm::Type::getDoubleTy(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f64.convert_i32_u
// Stack effect: (i32) -> (f64).  Every unsigned i32 value is exactly representable as f64; LLVM
// `uitofp` models the WebAssembly conversion directly.
case wasm1_code::f64_convert_i32_u:
{
    validate_numeric_unary(u8"f64.convert_i32_u", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::i32,
                                                       runtime_operand_stack_value_type::f64,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateUIToFP(operand.value, ::llvm::Type::getDoubleTy(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f64.convert_i64_s
// Stack effect: (i64) -> (f64).  Signed i64 values convert without trapping, although values above
// f64's integer precision are rounded.
case wasm1_code::f64_convert_i64_s:
{
    validate_numeric_unary(u8"f64.convert_i64_s", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::i64,
                                                       runtime_operand_stack_value_type::f64,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateSIToFP(operand.value, ::llvm::Type::getDoubleTy(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f64.convert_i64_u
// Stack effect: (i64) -> (f64).  The 64-bit input is interpreted as unsigned and may round when it
// exceeds the exact integer precision of f64.
case wasm1_code::f64_convert_i64_u:
{
    validate_numeric_unary(u8"f64.convert_i64_u", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::i64,
                                                       runtime_operand_stack_value_type::f64,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateUIToFP(operand.value, ::llvm::Type::getDoubleTy(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f64.promote_f32
// Stack effect: (f32) -> (f64).  Promotion widens precision and preserves the f32 numeric value
// exactly in double precision.
case wasm1_code::f64_promote_f32:
{
    validate_numeric_unary(u8"f64.promote_f32", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::f32,
                                                       runtime_operand_stack_value_type::f64,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateFPExt(operand.value, ::llvm::Type::getDoubleTy(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i32.reinterpret_f32
// Stack effect: (f32) -> (i32).  Reinterpretation is a bitcast, not a numeric conversion; all
// payload bits, including NaN payloads and signed-zero encodings, are preserved.
case wasm1_code::i32_reinterpret_f32:
{
    validate_numeric_unary(u8"i32.reinterpret_f32", curr_operand_stack_value_type::f32, curr_operand_stack_value_type::i32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::f32,
                                                       runtime_operand_stack_value_type::i32,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateBitCast(operand.value, ::llvm::Type::getInt32Ty(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// i64.reinterpret_f64
// Stack effect: (f64) -> (i64).  The 64-bit IEEE-754 representation is moved into an integer value
// unchanged.
case wasm1_code::i64_reinterpret_f64:
{
    validate_numeric_unary(u8"i64.reinterpret_f64", curr_operand_stack_value_type::f64, curr_operand_stack_value_type::i64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::f64,
                                                       runtime_operand_stack_value_type::i64,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateBitCast(operand.value, ::llvm::Type::getInt64Ty(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f32.reinterpret_i32
// Stack effect: (i32) -> (f32).  The integer bits are viewed as an f32 encoding without arithmetic
// conversion or validation of the resulting floating-point payload.
case wasm1_code::f32_reinterpret_i32:
{
    validate_numeric_unary(u8"f32.reinterpret_i32", curr_operand_stack_value_type::i32, curr_operand_stack_value_type::f32);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::i32,
                                                       runtime_operand_stack_value_type::f32,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateBitCast(operand.value, ::llvm::Type::getFloatTy(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

// f64.reinterpret_i64
// Stack effect: (i64) -> (f64).  The integer's 64-bit pattern becomes the f64 bit pattern exactly.
case wasm1_code::f64_reinterpret_i64:
{
    validate_numeric_unary(u8"f64.reinterpret_i64", curr_operand_stack_value_type::i64, curr_operand_stack_value_type::f64);

    if(emit_llvm_jit_active)
    {
        llvm_jit_instruction_emitted_inline = true;
        if(!try_emit_runtime_local_func_llvm_jit_unary(llvm_jit_emit_state,
                                                       runtime_operand_stack_value_type::i64,
                                                       runtime_operand_stack_value_type::f64,
                                                       [&](::llvm::IRBuilder<>& ir_builder, llvm_jit_stack_value_t const& operand) noexcept { return ir_builder.CreateBitCast(operand.value, ::llvm::Type::getDoubleTy(ir_builder.getContext())); }))
            [[unlikely]]
        {
            disable_inline_llvm_jit_emission();
        }
    }

    break;
}

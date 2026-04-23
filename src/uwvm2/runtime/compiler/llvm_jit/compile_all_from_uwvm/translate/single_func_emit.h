#include <uwvm2/object/memory/linear/access.h>

struct llvm_jit_stack_value_t
{
    runtime_operand_stack_value_type type{};
    ::llvm::Value* value{};
};

struct llvm_jit_runtime_wasm_call_abi_layout_t
{
    bool valid{};
    ::std::size_t parameter_count{};
    ::std::size_t result_count{};
    ::std::size_t parameter_bytes{};
    ::std::size_t result_bytes{};
};

struct llvm_jit_runtime_raw_call_buffers_t
{
    bool valid{};
    ::llvm::Value* param_buffer_address{};
    ::llvm::AllocaInst* result_buffer{};
    ::llvm::Value* result_buffer_address{};
};

struct llvm_jit_runtime_raw_bridge_emit_result_t
{
    bool valid{};
    ::llvm::CallInst* bridge_call{};
    ::llvm::Value* result_value{};
};

struct llvm_jit_prepared_wasm_call_operands_t
{
    bool valid{};
    llvm_jit_runtime_wasm_call_abi_layout_t abi_layout{};
    runtime_operand_stack_value_type result_type{};
    bool has_result{};
    ::uwvm2::utils::container::vector<::llvm::Value*> arguments{};
};

struct llvm_jit_memory_snapshot_values_t
{
    ::llvm::Value* memory_begin_address{};
    ::llvm::Value* byte_length{};
};

[[nodiscard]] inline constexpr ::std::uint_least8_t get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type value_type) noexcept
{ return static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(value_type)); }

[[nodiscard]] inline ::llvm::Type* get_llvm_type_from_wasm_value_type(::llvm::LLVMContext& llvm_context, runtime_operand_stack_value_type value_type) noexcept
{
    switch(get_runtime_wasm_value_type_encoding(value_type))
    {
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::i32)):
            return ::llvm::Type::getInt32Ty(llvm_context);
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::i64)):
            return ::llvm::Type::getInt64Ty(llvm_context);
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::f32)):
            return ::llvm::Type::getFloatTy(llvm_context);
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::f64)):
            return ::llvm::Type::getDoubleTy(llvm_context);
        [[unlikely]] default:
            return nullptr;
    }
}

[[nodiscard]] inline ::llvm::PointerType* get_llvm_pointer_type(::llvm::Type* pointee_type, unsigned address_space = 0u) noexcept
{
    if(pointee_type == nullptr) [[unlikely]] { return nullptr; }
    return ::llvm::PointerType::get(pointee_type->getContext(), address_space);
}

[[nodiscard]] inline ::llvm::Constant* get_llvm_host_pointer_constant(::std::uintptr_t host_address, ::llvm::Type* pointer_type) noexcept
{
    if(pointer_type == nullptr) [[unlikely]] { return nullptr; }

    auto& llvm_context{pointer_type->getContext()};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto host_address_value{::llvm::ConstantInt::get(llvm_intptr_type, host_address)};
    return ::llvm::ConstantExpr::getIntToPtr(host_address_value, pointer_type);
}

[[nodiscard]] inline ::std::size_t get_runtime_wasm_value_type_abi_size(runtime_operand_stack_value_type value_type) noexcept
{
    switch(get_runtime_wasm_value_type_encoding(value_type))
    {
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::i32)):
        {
            return sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32);
        }
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::i64)):
        {
            return sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64);
        }
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::f32)):
        {
            return sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32);
        }
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::f64)):
        {
            return sizeof(::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64);
        }
        [[unlikely]] default:
        {
            return 0uz;
        }
    }
}

[[nodiscard]] inline ::llvm::Constant* get_llvm_zero_constant_from_wasm_value_type(::llvm::LLVMContext& llvm_context,
                                                                                   runtime_operand_stack_value_type value_type) noexcept
{
    auto llvm_type{get_llvm_type_from_wasm_value_type(llvm_context, value_type)};
    if(llvm_type == nullptr) [[unlikely]] { return nullptr; }
    return ::llvm::Constant::getNullValue(llvm_type);
}

[[nodiscard]] inline ::llvm::Value* coerce_llvm_bool_to_i32(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* bool_value) noexcept
{
    if(bool_value == nullptr) [[unlikely]] { return nullptr; }
    return ir_builder.CreateZExt(bool_value, ::llvm::Type::getInt32Ty(ir_builder.getContext()));
}

[[nodiscard]] inline ::llvm::Value* call_llvm_intrinsic(::llvm::Module& llvm_module,
                                                        ::llvm::IRBuilder<>& ir_builder,
                                                        ::llvm::Intrinsic::ID intrinsic_id,
                                                        ::llvm::ArrayRef<::llvm::Type*> overloaded_types,
                                                        ::llvm::ArrayRef<::llvm::Value*> arguments)
{
    auto llvm_intrinsic{::llvm::Intrinsic::getOrInsertDeclaration(::std::addressof(llvm_module), intrinsic_id, overloaded_types)};
    return ir_builder.CreateCall(llvm_intrinsic, arguments);
}

[[nodiscard]] inline unsigned get_llvm_integer_bit_width(::llvm::Value* value) noexcept
{
    if(value == nullptr) [[unlikely]] { return 0u; }
    return ::llvm::cast<::llvm::IntegerType>(value->getType())->getBitWidth();
}

[[nodiscard]] inline ::llvm::Value* emit_llvm_shift_count_mask(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* shift_count, unsigned num_bits) noexcept
{
    if(shift_count == nullptr || num_bits == 0u) [[unlikely]] { return nullptr; }
    auto bits_minus_one{::llvm::ConstantInt::get(shift_count->getType(), num_bits - 1u)};
    return ir_builder.CreateAnd(shift_count, bits_minus_one);
}

[[nodiscard]] inline ::llvm::Value* emit_llvm_rotl(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* left, ::llvm::Value* right) noexcept
{
    if(left == nullptr || right == nullptr) [[unlikely]] { return nullptr; }

    auto const bit_width{get_llvm_integer_bit_width(left)};
    if(bit_width == 0u) [[unlikely]] { return nullptr; }

    auto masked_right{emit_llvm_shift_count_mask(ir_builder, right, bit_width)};
    if(masked_right == nullptr) [[unlikely]] { return nullptr; }

    auto bit_width_value{::llvm::ConstantInt::get(right->getType(), bit_width)};
    auto inverse_shift{emit_llvm_shift_count_mask(ir_builder, ir_builder.CreateSub(bit_width_value, masked_right), bit_width)};
    return ir_builder.CreateOr(ir_builder.CreateShl(left, masked_right), ir_builder.CreateLShr(left, inverse_shift));
}

[[nodiscard]] inline ::llvm::Value* emit_llvm_rotr(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* left, ::llvm::Value* right) noexcept
{
    if(left == nullptr || right == nullptr) [[unlikely]] { return nullptr; }

    auto const bit_width{get_llvm_integer_bit_width(left)};
    if(bit_width == 0u) [[unlikely]] { return nullptr; }

    auto masked_right{emit_llvm_shift_count_mask(ir_builder, right, bit_width)};
    if(masked_right == nullptr) [[unlikely]] { return nullptr; }

    auto bit_width_value{::llvm::ConstantInt::get(right->getType(), bit_width)};
    auto inverse_shift{emit_llvm_shift_count_mask(ir_builder, ir_builder.CreateSub(bit_width_value, masked_right), bit_width)};
    return ir_builder.CreateOr(ir_builder.CreateLShr(left, masked_right), ir_builder.CreateShl(left, inverse_shift));
}

[[nodiscard]] inline ::llvm::Constant* get_llvm_f32_constant_from_bits(::llvm::LLVMContext& llvm_context, ::std::uint_least32_t bits)
{
    return ::llvm::ConstantFP::get(::llvm::Type::getFloatTy(llvm_context),
                                   ::llvm::APFloat(::llvm::APFloat::IEEEsingle(), ::llvm::APInt(32u, static_cast<::std::uint64_t>(bits))));
}

[[nodiscard]] inline ::llvm::Constant* get_llvm_f64_constant_from_bits(::llvm::LLVMContext& llvm_context, ::std::uint_least64_t bits)
{ return ::llvm::ConstantFP::get(::llvm::Type::getDoubleTy(llvm_context), ::llvm::APFloat(::llvm::APFloat::IEEEdouble(), ::llvm::APInt(64u, bits))); }

inline void emit_llvm_conditional_trap(::llvm::Module& llvm_module, ::llvm::IRBuilder<>& ir_builder, ::llvm::Value* condition)
{
    if(condition == nullptr) [[unlikely]] { return; }

    auto current_block{ir_builder.GetInsertBlock()};
    auto function{current_block == nullptr ? nullptr : current_block->getParent()};
    if(function == nullptr) [[unlikely]] { return; }

    auto& llvm_context{ir_builder.getContext()};
    auto trap_block{::llvm::BasicBlock::Create(llvm_context, "wasmTrap", function)};
    auto continue_block{::llvm::BasicBlock::Create(llvm_context, "wasmTrapCont", function)};

    ir_builder.CreateCondBr(condition, trap_block, continue_block);

    ir_builder.SetInsertPoint(trap_block);
    auto trap_intrinsic{::llvm::Intrinsic::getOrInsertDeclaration(::std::addressof(llvm_module), ::llvm::Intrinsic::trap, {})};
    ir_builder.CreateCall(trap_intrinsic);
    ir_builder.CreateUnreachable();

    ir_builder.SetInsertPoint(continue_block);
}

inline void emit_llvm_divide_by_zero_trap(::llvm::Module& llvm_module, ::llvm::IRBuilder<>& ir_builder, ::llvm::Value* divisor)
{
    if(divisor == nullptr) [[unlikely]] { return; }
    emit_llvm_conditional_trap(llvm_module, ir_builder, ir_builder.CreateICmpEQ(divisor, ::llvm::ConstantInt::get(divisor->getType(), 0u)));
}

inline void emit_llvm_signed_div_overflow_trap(::llvm::Module& llvm_module, ::llvm::IRBuilder<>& ir_builder, ::llvm::Value* dividend, ::llvm::Value* divisor)
{
    if(dividend == nullptr || divisor == nullptr) [[unlikely]] { return; }

    auto const bit_width{get_llvm_integer_bit_width(dividend)};
    auto int_type{::llvm::cast<::llvm::IntegerType>(dividend->getType())};
    auto signed_min{::llvm::ConstantInt::get(int_type, ::llvm::APInt::getSignedMinValue(bit_width))};
    auto neg_one{::llvm::ConstantInt::getSigned(int_type, -1)};
    auto divisor_zero{ir_builder.CreateICmpEQ(divisor, ::llvm::ConstantInt::get(divisor->getType(), 0u))};
    auto signed_overflow{ir_builder.CreateAnd(ir_builder.CreateICmpEQ(dividend, signed_min), ir_builder.CreateICmpEQ(divisor, neg_one))};
    emit_llvm_conditional_trap(llvm_module, ir_builder, ir_builder.CreateOr(divisor_zero, signed_overflow));
}

[[nodiscard]] inline ::llvm::Value* emit_llvm_signed_remainder_with_wasm_semantics(::llvm::Module& llvm_module,
                                                                                   ::llvm::IRBuilder<>& ir_builder,
                                                                                   ::llvm::Value* dividend,
                                                                                   ::llvm::Value* divisor)
{
    if(dividend == nullptr || divisor == nullptr) [[unlikely]] { return nullptr; }

    emit_llvm_divide_by_zero_trap(llvm_module, ir_builder, divisor);

    auto const bit_width{get_llvm_integer_bit_width(dividend)};
    auto int_type{::llvm::cast<::llvm::IntegerType>(dividend->getType())};
    auto signed_min{::llvm::ConstantInt::get(int_type, ::llvm::APInt::getSignedMinValue(bit_width))};
    auto neg_one{::llvm::ConstantInt::getSigned(int_type, -1)};

    auto pre_overflow_block{ir_builder.GetInsertBlock()};
    auto function{pre_overflow_block == nullptr ? nullptr : pre_overflow_block->getParent()};
    if(function == nullptr) [[unlikely]] { return nullptr; }

    auto& llvm_context{ir_builder.getContext()};
    auto no_overflow_block{::llvm::BasicBlock::Create(llvm_context, "wasmSRemNoOverflow", function)};
    auto end_block{::llvm::BasicBlock::Create(llvm_context, "wasmSRemEnd", function)};
    auto no_overflow{ir_builder.CreateOr(ir_builder.CreateICmpNE(dividend, signed_min), ir_builder.CreateICmpNE(divisor, neg_one))};
    ir_builder.CreateCondBr(no_overflow, no_overflow_block, end_block);

    ir_builder.SetInsertPoint(no_overflow_block);
    auto no_overflow_value{ir_builder.CreateSRem(dividend, divisor)};
    ir_builder.CreateBr(end_block);

    ir_builder.SetInsertPoint(end_block);
    auto phi{ir_builder.CreatePHI(int_type, 2u)};
    phi->addIncoming(::llvm::ConstantInt::get(int_type, 0u), pre_overflow_block);
    phi->addIncoming(no_overflow_value, no_overflow_block);
    return phi;
}

[[nodiscard]] inline ::llvm::Value* quiet_llvm_nan(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* nan)
{
    if(nan == nullptr) [[unlikely]] { return nullptr; }

    if(nan->getType()->isFloatTy())
    {
        auto int_value{ir_builder.CreateBitCast(nan, ::llvm::Type::getInt32Ty(ir_builder.getContext()))};
        auto quiet_mask{::llvm::ConstantInt::get(int_value->getType(), 0x00400000u)};
        return ir_builder.CreateBitCast(ir_builder.CreateOr(int_value, quiet_mask), nan->getType());
    }
    if(nan->getType()->isDoubleTy())
    {
        auto int_value{ir_builder.CreateBitCast(nan, ::llvm::Type::getInt64Ty(ir_builder.getContext()))};
        auto quiet_mask{::llvm::ConstantInt::get(int_value->getType(), 0x0008000000000000ull)};
        return ir_builder.CreateBitCast(ir_builder.CreateOr(int_value, quiet_mask), nan->getType());
    }
    return nullptr;
}

[[nodiscard]] inline ::llvm::Value* emit_llvm_float_min(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* left, ::llvm::Value* right)
{
    if(left == nullptr || right == nullptr) [[unlikely]] { return nullptr; }

    auto int_type{left->getType()->isFloatTy() ? ::llvm::Type::getInt32Ty(ir_builder.getContext()) : ::llvm::Type::getInt64Ty(ir_builder.getContext())};
    auto is_left_nan{ir_builder.CreateFCmpUNO(left, left)};
    auto is_right_nan{ir_builder.CreateFCmpUNO(right, right)};
    auto is_left_less_than_right{ir_builder.CreateFCmpOLT(left, right)};
    auto is_left_greater_than_right{ir_builder.CreateFCmpOGT(left, right)};

    return ir_builder.CreateSelect(
        is_left_nan,
        quiet_llvm_nan(ir_builder, left),
        ir_builder.CreateSelect(
            is_right_nan,
            quiet_llvm_nan(ir_builder, right),
            ir_builder.CreateSelect(is_left_less_than_right,
                                    left,
                                    ir_builder.CreateSelect(is_left_greater_than_right,
                                                            right,
                                                            ir_builder.CreateBitCast(ir_builder.CreateOr(ir_builder.CreateBitCast(left, int_type),
                                                                                                         ir_builder.CreateBitCast(right, int_type)),
                                                                                     left->getType())))));
}

[[nodiscard]] inline ::llvm::Value* emit_llvm_float_max(::llvm::IRBuilder<>& ir_builder, ::llvm::Value* left, ::llvm::Value* right)
{
    if(left == nullptr || right == nullptr) [[unlikely]] { return nullptr; }

    auto int_type{left->getType()->isFloatTy() ? ::llvm::Type::getInt32Ty(ir_builder.getContext()) : ::llvm::Type::getInt64Ty(ir_builder.getContext())};
    auto is_left_nan{ir_builder.CreateFCmpUNO(left, left)};
    auto is_right_nan{ir_builder.CreateFCmpUNO(right, right)};
    auto is_left_less_than_right{ir_builder.CreateFCmpOLT(left, right)};
    auto is_left_greater_than_right{ir_builder.CreateFCmpOGT(left, right)};

    return ir_builder.CreateSelect(
        is_left_nan,
        quiet_llvm_nan(ir_builder, left),
        ir_builder.CreateSelect(
            is_right_nan,
            quiet_llvm_nan(ir_builder, right),
            ir_builder.CreateSelect(is_left_less_than_right,
                                    right,
                                    ir_builder.CreateSelect(is_left_greater_than_right,
                                                            left,
                                                            ir_builder.CreateBitCast(ir_builder.CreateAnd(ir_builder.CreateBitCast(left, int_type),
                                                                                                          ir_builder.CreateBitCast(right, int_type)),
                                                                                     left->getType())))));
}

template <typename Float>
[[nodiscard]] inline ::llvm::Value* emit_llvm_trunc_float_to_int(::llvm::Module& llvm_module,
                                                                 ::llvm::IRBuilder<>& ir_builder,
                                                                 ::llvm::Type* dest_type,
                                                                 bool is_signed,
                                                                 Float min_bounds,
                                                                 Float max_bounds,
                                                                 ::llvm::Value* operand)
{
    if(dest_type == nullptr || operand == nullptr) [[unlikely]] { return nullptr; }

    auto is_nan{ir_builder.CreateFCmpUNO(operand, operand)};
    emit_llvm_conditional_trap(llvm_module, ir_builder, is_nan);

    auto min_bound{::llvm::ConstantFP::get(operand->getType(), static_cast<double>(min_bounds))};
    auto max_bound{::llvm::ConstantFP::get(operand->getType(), static_cast<double>(max_bounds))};
    auto is_overflow{ir_builder.CreateOr(ir_builder.CreateFCmpOGE(operand, max_bound), ir_builder.CreateFCmpOLE(operand, min_bound))};
    emit_llvm_conditional_trap(llvm_module, ir_builder, is_overflow);

    return is_signed ? ir_builder.CreateFPToSI(operand, dest_type) : ir_builder.CreateFPToUI(operand, dest_type);
}

[[nodiscard]] inline ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const*
    resolve_runtime_callee_function_type(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                         validation_module_traits_t::wasm_u32 func_index) noexcept
{
    auto const import_func_count{runtime_module.imported_function_vec_storage.size()};
    auto const local_func_count{runtime_module.local_defined_function_vec_storage.size()};
    auto const func_index_uz{static_cast<::std::size_t>(func_index)};

    if(func_index_uz < import_func_count)
    {
        auto const& imported_rec{runtime_module.imported_function_vec_storage.index_unchecked(func_index_uz)};
        auto import_type_ptr{imported_rec.import_type_ptr};
        if(import_type_ptr == nullptr || import_type_ptr->imports.type != validation_module_traits_t::external_types::func) [[unlikely]] { return nullptr; }
        return import_type_ptr->imports.storage.function;
    }

    auto const local_func_index{func_index_uz - import_func_count};
    if(local_func_index >= local_func_count) [[unlikely]] { return nullptr; }

    return runtime_module.local_defined_function_vec_storage.index_unchecked(local_func_index).function_type_ptr;
}

struct runtime_direct_callee_resolution_t
{
    bool state_valid{};
    bool direct_callable{};
    validation_module_traits_t::wasm_u32 func_index{};
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* function_type_ptr{};
};

[[nodiscard]] inline runtime_direct_callee_resolution_t
    resolve_runtime_direct_callee(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                  validation_module_traits_t::wasm_u32 func_index) noexcept
{
    using imported_function_storage_t = ::uwvm2::uwvm::runtime::storage::imported_function_storage_t;
    using function_link_kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;

    runtime_direct_callee_resolution_t result{.state_valid = true};

    auto const import_func_count{runtime_module.imported_function_vec_storage.size()};
    auto const local_func_count{runtime_module.local_defined_function_vec_storage.size()};
    auto const func_index_uz{static_cast<::std::size_t>(func_index)};
    auto local_func_begin{runtime_module.local_defined_function_vec_storage.data()};

    if(func_index_uz >= import_func_count)
    {
        auto const local_func_index{func_index_uz - import_func_count};
        if(local_func_index >= local_func_count) [[unlikely]] { return {}; }

        auto function_type_ptr{runtime_module.local_defined_function_vec_storage.index_unchecked(local_func_index).function_type_ptr};
        if(function_type_ptr == nullptr) [[unlikely]] { return {}; }

        result.direct_callable = true;
        result.func_index = func_index;
        result.function_type_ptr = function_type_ptr;
        return result;
    }

    imported_function_storage_t const* curr{::std::addressof(runtime_module.imported_function_vec_storage.index_unchecked(func_index_uz))};
    for(::std::size_t steps{};; ++steps)
    {
        if(steps > 8192uz || curr == nullptr) [[unlikely]] { return {}; }

        switch(curr->link_kind)
        {
            case function_link_kind::imported:
            {
                curr = curr->target.imported_ptr;
                continue;
            }
            case function_link_kind::defined:
            {
                auto defined_func_ptr{curr->target.defined_ptr};
                if(defined_func_ptr == nullptr || defined_func_ptr->function_type_ptr == nullptr) [[unlikely]] { return {}; }

                result.function_type_ptr = defined_func_ptr->function_type_ptr;
                if(local_func_begin == nullptr || defined_func_ptr < local_func_begin || defined_func_ptr >= local_func_begin + local_func_count)
                {
                    return result;
                }

                auto const local_func_index{static_cast<::std::size_t>(defined_func_ptr - local_func_begin)};
                if(import_func_count > static_cast<::std::size_t>(::std::numeric_limits<validation_module_traits_t::wasm_u32>::max()) ||
                   local_func_index > static_cast<::std::size_t>(::std::numeric_limits<validation_module_traits_t::wasm_u32>::max()) - import_func_count)
                    [[unlikely]]
                {
                    return {};
                }

                result.direct_callable = true;
                result.func_index = static_cast<validation_module_traits_t::wasm_u32>(import_func_count + local_func_index);
                return result;
            }
            case function_link_kind::local_imported:
            case function_link_kind::unresolved:
            {
                return result;
            }
#if defined(UWVM_SUPPORT_PRELOAD_DL)
            case function_link_kind::dl:
            {
                return result;
            }
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
            case function_link_kind::weak_symbol:
            {
                return result;
            }
#endif
            [[unlikely]] default:
            {
                return {};
            }
        }
    }
}

[[nodiscard]] inline ::std::string get_llvm_runtime_module_symbol_prefix(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module)
{ return ::std::string{"uwvm_m_"} + ::std::to_string(reinterpret_cast<::std::uintptr_t>(::std::addressof(runtime_module))); }

[[nodiscard]] inline ::std::string get_llvm_wasm_function_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                               validation_module_traits_t::wasm_u32 func_index)
{
    auto const func_index_uz{static_cast<::std::size_t>(func_index)};
    return get_llvm_runtime_module_symbol_prefix(runtime_module) + "_func_" + ::std::to_string(func_index_uz);
}

[[nodiscard]] inline ::std::string get_llvm_wasm_function_ir_module_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                                         validation_module_traits_t::wasm_u32 func_index)
{
    auto const func_index_uz{static_cast<::std::size_t>(func_index)};
    return get_llvm_runtime_module_symbol_prefix(runtime_module) + "_ir_module_for_func_" + ::std::to_string(func_index_uz);
}

[[nodiscard]] inline ::std::string get_llvm_wasm_ir_module_name(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module)
{ return get_llvm_runtime_module_symbol_prefix(runtime_module) + "_ir_module"; }

[[nodiscard]] inline ::llvm::FunctionType*
    get_llvm_function_type_from_wasm_function_type(::llvm::LLVMContext& llvm_context,
                                                   ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type) noexcept;

[[nodiscard]] inline ::llvm::Function*
    get_or_create_llvm_wasm_function_declaration(::llvm::Module& llvm_module,
                                                 ::llvm::LLVMContext& llvm_context,
                                                 ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                 validation_module_traits_t::wasm_u32 func_index,
                                                 ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type)
{
    auto callee_function_type{get_llvm_function_type_from_wasm_function_type(llvm_context, wasm_function_type)};
    if(callee_function_type == nullptr) [[unlikely]] { return nullptr; }

    auto const callee_name{get_llvm_wasm_function_name(runtime_module, func_index)};
    auto callee_function{llvm_module.getFunction(callee_name)};
    if(callee_function == nullptr)
    {
        return ::llvm::Function::Create(callee_function_type, ::llvm::Function::ExternalLinkage, callee_name, ::std::addressof(llvm_module));
    }
    if(callee_function->getFunctionType() != callee_function_type) [[unlikely]] { return nullptr; }
    return callee_function;
}

[[nodiscard]] inline ::llvm::FunctionType*
    get_llvm_function_type_from_wasm_function_type(::llvm::LLVMContext& llvm_context,
                                                   ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type) noexcept
{
    auto const parameter_begin{wasm_function_type.parameter.begin};
    auto const parameter_end{wasm_function_type.parameter.end};
    auto const result_begin{wasm_function_type.result.begin};
    auto const result_end{wasm_function_type.result.end};

    if(parameter_begin == nullptr && parameter_begin != parameter_end) [[unlikely]] { return nullptr; }
    if(result_begin == nullptr && result_begin != result_end) [[unlikely]] { return nullptr; }

    auto const parameter_count{parameter_begin == nullptr ? 0uz : static_cast<::std::size_t>(parameter_end - parameter_begin)};
    auto const result_count{result_begin == nullptr ? 0uz : static_cast<::std::size_t>(result_end - result_begin)};
    if(result_count > 1uz) [[unlikely]] { return nullptr; }

    ::uwvm2::utils::container::vector<::llvm::Type*> llvm_parameter_types{};
    llvm_parameter_types.reserve(parameter_count);

    for(::std::size_t parameter_index{}; parameter_index != parameter_count; ++parameter_index)
    {
        auto llvm_parameter_type{
            get_llvm_type_from_wasm_value_type(llvm_context, static_cast<runtime_operand_stack_value_type>(parameter_begin[parameter_index]))};
        if(llvm_parameter_type == nullptr) [[unlikely]] { return nullptr; }
        llvm_parameter_types.push_back(llvm_parameter_type);
    }

    ::llvm::Type* llvm_result_type{::llvm::Type::getVoidTy(llvm_context)};
    if(result_count == 1uz)
    {
        llvm_result_type = get_llvm_type_from_wasm_value_type(llvm_context, static_cast<runtime_operand_stack_value_type>(result_begin[0]));
        if(llvm_result_type == nullptr) [[unlikely]] { return nullptr; }
    }

    return ::llvm::FunctionType::get(llvm_result_type, llvm_parameter_types, false);
}

[[nodiscard]] inline ::llvm::FunctionType* get_llvm_runtime_raw_call_bridge_function_type(::llvm::LLVMContext& llvm_context) noexcept
{
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
    return ::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context),
                                     {llvm_intptr_type, llvm_i32_type, llvm_intptr_type, llvm_intptr_type, llvm_intptr_type, llvm_intptr_type},
                                     false);
}

[[nodiscard]] inline ::llvm::FunctionType* get_llvm_runtime_raw_call_target_entry_function_type(::llvm::LLVMContext& llvm_context) noexcept
{
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    return ::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context),
                                     {llvm_intptr_type, llvm_intptr_type, llvm_intptr_type, llvm_intptr_type, llvm_intptr_type},
                                     false);
}

[[nodiscard]] inline ::llvm::StructType* get_llvm_runtime_raw_call_target_struct_type(::llvm::LLVMContext& llvm_context) noexcept
{
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
    return ::llvm::StructType::get(llvm_context, {llvm_intptr_type, llvm_intptr_type, llvm_i32_type}, false);
}

[[nodiscard]] inline ::llvm::StructType* get_llvm_runtime_call_indirect_table_view_struct_type(::llvm::LLVMContext& llvm_context) noexcept
{
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    return ::llvm::StructType::get(llvm_context, {llvm_intptr_type, llvm_intptr_type}, false);
}

[[nodiscard]] inline ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const*
    resolve_runtime_type_section_function_type(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                               validation_module_traits_t::wasm_u32 type_index) noexcept
{
    auto type_begin{runtime_module.type_section_storage.type_section_begin};
    auto const type_count{get_runtime_type_section_count(runtime_module)};
    auto const type_index_uz{static_cast<::std::size_t>(type_index)};
    if(type_begin == nullptr || type_index_uz >= type_count) [[unlikely]] { return nullptr; }
    return type_begin + type_index_uz;
}

[[nodiscard]] inline llvm_jit_runtime_wasm_call_abi_layout_t
    get_runtime_wasm_call_abi_layout(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type) noexcept
{
    auto const parameter_begin{wasm_function_type.parameter.begin};
    auto const parameter_end{wasm_function_type.parameter.end};
    auto const result_begin{wasm_function_type.result.begin};
    auto const result_end{wasm_function_type.result.end};

    if(parameter_begin == nullptr && parameter_begin != parameter_end) [[unlikely]] { return {}; }
    if(result_begin == nullptr && result_begin != result_end) [[unlikely]] { return {}; }

    auto const parameter_count{parameter_begin == nullptr ? 0uz : static_cast<::std::size_t>(parameter_end - parameter_begin)};
    auto const result_count{result_begin == nullptr ? 0uz : static_cast<::std::size_t>(result_end - result_begin)};
    if(result_count > 1uz) [[unlikely]] { return {}; }

    ::std::size_t parameter_bytes{};
    for(::std::size_t parameter_index{}; parameter_index != parameter_count; ++parameter_index)
    {
        auto const abi_size{get_runtime_wasm_value_type_abi_size(static_cast<runtime_operand_stack_value_type>(parameter_begin[parameter_index]))};
        if(abi_size == 0uz || parameter_bytes > ::std::numeric_limits<::std::size_t>::max() - abi_size) [[unlikely]] { return {}; }
        parameter_bytes += abi_size;
    }

    auto const result_bytes{result_count == 0uz ? 0uz : get_runtime_wasm_value_type_abi_size(static_cast<runtime_operand_stack_value_type>(result_begin[0]))};
    if(result_count == 1uz && result_bytes == 0uz) [[unlikely]] { return {}; }

    return llvm_jit_runtime_wasm_call_abi_layout_t{.valid = true,
                                                   .parameter_count = parameter_count,
                                                   .result_count = result_count,
                                                   .parameter_bytes = parameter_bytes,
                                                   .result_bytes = result_bytes};
}

[[nodiscard]] inline llvm_jit_runtime_raw_call_buffers_t
    emit_runtime_raw_call_buffers(::llvm::IRBuilder<>& ir_builder,
                                  ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type,
                                  ::llvm::ArrayRef<::llvm::Value*> call_arguments,
                                  char const* param_buffer_name,
                                  char const* result_buffer_name)
{
    auto const abi_layout{get_runtime_wasm_call_abi_layout(wasm_function_type)};
    if(!abi_layout.valid || abi_layout.parameter_count != call_arguments.size()) [[unlikely]] { return {}; }

    auto const parameter_begin{wasm_function_type.parameter.begin};
    auto const result_begin{wasm_function_type.result.begin};

    auto& llvm_context{ir_builder.getContext()};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto llvm_i8_type{::llvm::Type::getInt8Ty(llvm_context)};

    ::llvm::Value* param_buffer_address{::llvm::ConstantInt::get(llvm_intptr_type, 0u)};
    if(abi_layout.parameter_bytes != 0uz)
    {
        auto param_buffer{ir_builder.CreateAlloca(llvm_i8_type, ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.parameter_bytes), param_buffer_name)};

        ::std::size_t parameter_offset{};
        for(::std::size_t parameter_index{}; parameter_index != abi_layout.parameter_count; ++parameter_index)
        {
            auto const abi_size{get_runtime_wasm_value_type_abi_size(static_cast<runtime_operand_stack_value_type>(parameter_begin[parameter_index]))};
            auto argument{call_arguments[parameter_index]};
            if(abi_size == 0uz || argument == nullptr) [[unlikely]] { return {}; }

            auto store_address{ir_builder.CreateInBoundsGEP(llvm_i8_type, param_buffer, {::llvm::ConstantInt::get(llvm_intptr_type, parameter_offset)})};
            auto typed_store_address{ir_builder.CreateBitCast(store_address, get_llvm_pointer_type(argument->getType()))};
            ir_builder.CreateStore(argument, typed_store_address);
            parameter_offset += abi_size;
        }

        param_buffer_address = ir_builder.CreatePtrToInt(param_buffer, llvm_intptr_type);
    }

    ::llvm::AllocaInst* result_buffer{};
    ::llvm::Value* result_buffer_address{::llvm::ConstantInt::get(llvm_intptr_type, 0u)};
    if(abi_layout.result_count == 1uz)
    {
        auto llvm_result_type{get_llvm_type_from_wasm_value_type(llvm_context, static_cast<runtime_operand_stack_value_type>(result_begin[0]))};
        if(llvm_result_type == nullptr) [[unlikely]] { return {}; }
        result_buffer = ir_builder.CreateAlloca(llvm_result_type, nullptr, result_buffer_name);
        result_buffer_address = ir_builder.CreatePtrToInt(result_buffer, llvm_intptr_type);
    }

    return llvm_jit_runtime_raw_call_buffers_t{.valid = true,
                                               .param_buffer_address = param_buffer_address,
                                               .result_buffer = result_buffer,
                                               .result_buffer_address = result_buffer_address};
}

[[nodiscard]] inline bool runtime_wasm_function_types_equal(::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& left,
                                                            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& right) noexcept
{
    auto const left_param_begin{left.parameter.begin};
    auto const left_param_end{left.parameter.end};
    auto const right_param_begin{right.parameter.begin};
    auto const right_param_end{right.parameter.end};
    auto const left_result_begin{left.result.begin};
    auto const left_result_end{left.result.end};
    auto const right_result_begin{right.result.begin};
    auto const right_result_end{right.result.end};

    if((left_param_begin == nullptr && left_param_begin != left_param_end) || (right_param_begin == nullptr && right_param_begin != right_param_end) ||
       (left_result_begin == nullptr && left_result_begin != left_result_end) || (right_result_begin == nullptr && right_result_begin != right_result_end))
        [[unlikely]]
    {
        return false;
    }

    auto const left_param_count{left_param_begin == nullptr ? 0uz : static_cast<::std::size_t>(left_param_end - left_param_begin)};
    auto const right_param_count{right_param_begin == nullptr ? 0uz : static_cast<::std::size_t>(right_param_end - right_param_begin)};
    auto const left_result_count{left_result_begin == nullptr ? 0uz : static_cast<::std::size_t>(left_result_end - left_result_begin)};
    auto const right_result_count{right_result_begin == nullptr ? 0uz : static_cast<::std::size_t>(right_result_end - right_result_begin)};

    if(left_param_count != right_param_count || left_result_count != right_result_count) { return false; }

    for(::std::size_t i{}; i != left_param_count; ++i)
    {
        if(left_param_begin[i] != right_param_begin[i]) { return false; }
    }

    for(::std::size_t i{}; i != left_result_count; ++i)
    {
        if(left_result_begin[i] != right_result_begin[i]) { return false; }
    }

    return true;
}

[[nodiscard]] inline constexpr ::std::uint_least32_t invalid_runtime_canonical_type_id() noexcept
{ return (::std::numeric_limits<::std::uint_least32_t>::max)(); }

[[nodiscard]] inline ::std::uint_least32_t resolve_runtime_canonical_type_id(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                                             validation_module_traits_t::wasm_u32 type_index) noexcept
{
    auto type_begin{runtime_module.type_section_storage.type_section_begin};
    auto type_end{runtime_module.type_section_storage.type_section_end};
    auto const type_index_uz{static_cast<::std::size_t>(type_index)};
    if(type_begin == nullptr || type_end == nullptr || type_begin > type_end) [[unlikely]] { return invalid_runtime_canonical_type_id(); }

    auto const total{static_cast<::std::size_t>(type_end - type_begin)};
    if(type_index_uz >= total) [[unlikely]] { return invalid_runtime_canonical_type_id(); }

    auto const& target_type{type_begin[type_index_uz]};
    ::std::size_t canonical_index{type_index_uz};
    for(::std::size_t i{}; i != type_index_uz; ++i)
    {
        if(runtime_wasm_function_types_equal(type_begin[i], target_type))
        {
            canonical_index = i;
            break;
        }
    }

    if(canonical_index > static_cast<::std::size_t>((::std::numeric_limits<::std::uint_least32_t>::max)())) [[unlikely]]
    {
        return invalid_runtime_canonical_type_id();
    }
    return static_cast<::std::uint_least32_t>(canonical_index);
}

struct runtime_global_access_info_t
{
    runtime_operand_stack_value_type value_type{};
    bool is_mutable{};
    ::uwvm2::object::global::wasm_global_storage_t* storage_ptr{};
    ::uwvm2::uwvm::wasm::type::local_imported_t* local_imported_module_ptr{};
    ::std::size_t local_imported_global_index{};
};

[[nodiscard]] inline runtime_global_access_info_t
    resolve_runtime_global_access_info(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                       validation_module_traits_t::wasm_u32 global_index) noexcept
{
    runtime_global_access_info_t result{};

    auto const imported_global_count{runtime_module.imported_global_vec_storage.size()};
    auto const local_global_count{runtime_module.local_defined_global_vec_storage.size()};
    auto const global_index_uz{static_cast<::std::size_t>(global_index)};

    if(global_index_uz < imported_global_count)
    {
        using imported_global_storage_t = ::uwvm2::uwvm::runtime::storage::imported_global_storage_t;
        using global_link_kind = imported_global_storage_t::imported_global_link_kind;

        auto const& imported_global_rec{runtime_module.imported_global_vec_storage.index_unchecked(global_index_uz)};
        auto import_type_ptr{imported_global_rec.import_type_ptr};
        if(import_type_ptr == nullptr || import_type_ptr->imports.type != validation_module_traits_t::external_types::global) [[unlikely]] { return result; }

        result.value_type = static_cast<runtime_operand_stack_value_type>(import_type_ptr->imports.storage.global.type);
        result.is_mutable = import_type_ptr->imports.storage.global.is_mutable;

        imported_global_storage_t const* curr{::std::addressof(imported_global_rec)};
        for(;;)
        {
            if(curr == nullptr) [[unlikely]] { return {}; }

            switch(curr->link_kind)
            {
                case global_link_kind::imported:
                {
                    curr = curr->target.imported_ptr;
                    continue;
                }
                case global_link_kind::defined:
                {
                    auto defined_global{curr->target.defined_ptr};
                    if(defined_global == nullptr) [[unlikely]] { return {}; }
                    result.storage_ptr = const_cast<::uwvm2::object::global::wasm_global_storage_t*>(::std::addressof(defined_global->global));
                    return result;
                }
                case global_link_kind::local_imported:
                {
                    result.local_imported_module_ptr = curr->target.local_imported.module_ptr;
                    result.local_imported_global_index = curr->target.local_imported.index;
                    return result;
                }
                [[unlikely]] default:
                {
                    return {};
                }
            }
        }
    }

    auto const local_global_index{global_index_uz - imported_global_count};
    if(local_global_index >= local_global_count) [[unlikely]] { return result; }

    auto const& local_global_rec{runtime_module.local_defined_global_vec_storage.index_unchecked(local_global_index)};
    auto global_type_ptr{local_global_rec.global_type_ptr};
    if(global_type_ptr == nullptr) [[unlikely]] { return result; }

    result.value_type = static_cast<runtime_operand_stack_value_type>(global_type_ptr->type);
    result.is_mutable = global_type_ptr->is_mutable;
    result.storage_ptr = const_cast<::uwvm2::object::global::wasm_global_storage_t*>(::std::addressof(local_global_rec.global));
    return result;
}

[[nodiscard]] inline ::llvm::Constant* get_llvm_global_storage_pointer(::llvm::LLVMContext& llvm_context,
                                                                       ::uwvm2::object::global::wasm_global_storage_t* global_storage_ptr,
                                                                       runtime_operand_stack_value_type value_type) noexcept
{
    if(global_storage_ptr == nullptr) [[unlikely]] { return nullptr; }

    auto llvm_value_type{get_llvm_type_from_wasm_value_type(llvm_context, value_type)};
    if(llvm_value_type == nullptr) [[unlikely]] { return nullptr; }

    ::std::uintptr_t storage_address{};
    switch(value_type)
    {
        case runtime_operand_stack_value_type::i32:
        {
            storage_address = reinterpret_cast<::std::uintptr_t>(::std::addressof(global_storage_ptr->storage.i32));
            break;
        }
        case runtime_operand_stack_value_type::i64:
        {
            storage_address = reinterpret_cast<::std::uintptr_t>(::std::addressof(global_storage_ptr->storage.i64));
            break;
        }
        case runtime_operand_stack_value_type::f32:
        {
            storage_address = reinterpret_cast<::std::uintptr_t>(::std::addressof(global_storage_ptr->storage.f32));
            break;
        }
        case runtime_operand_stack_value_type::f64:
        {
            storage_address = reinterpret_cast<::std::uintptr_t>(::std::addressof(global_storage_ptr->storage.f64));
            break;
        }
        [[unlikely]] default:
        {
            return nullptr;
        }
    }

    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto llvm_address{::llvm::ConstantInt::get(llvm_intptr_type, storage_address)};
    return ::llvm::ConstantExpr::getIntToPtr(llvm_address, get_llvm_pointer_type(llvm_value_type));
}

template <typename ValueType>
[[nodiscard]] inline ValueType llvm_jit_local_imported_global_get_bridge(::std::uintptr_t local_imported_module_address, ::std::size_t global_index) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    ValueType value{};
    local_imported_module->global_get_from_index(global_index, reinterpret_cast<::std::byte*>(::std::addressof(value)));
    return value;
}

template <typename ValueType>
inline void llvm_jit_local_imported_global_set_bridge(::std::uintptr_t local_imported_module_address, ::std::size_t global_index, ValueType value) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }

    if(!local_imported_module->global_set_from_index(global_index, reinterpret_cast<::std::byte const*>(::std::addressof(value)))) [[unlikely]]
    {
        ::fast_io::fast_terminate();
    }
}

using runtime_native_memory_t = ::uwvm2::object::memory::linear::native_memory_t;
using runtime_wasm_i32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i32;
using runtime_wasm_i64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_i64;
using runtime_wasm_f32 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f32;
using runtime_wasm_f64 = ::uwvm2::parser::wasm::standard::wasm1::type::wasm_f64;
using runtime_table_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_table_storage_t;
using runtime_table_elem_storage_t = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_t;

struct runtime_call_indirect_callee_resolution_t
{
    bool state_valid{};
    bool present{};
    bool belongs_to_current_module{};
    validation_module_traits_t::wasm_u32 func_index{};
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const* function_type_ptr{};
};

[[nodiscard]] inline runtime_table_storage_t const* resolve_runtime_table_storage(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                                                  validation_module_traits_t::wasm_u32 table_index) noexcept
{
    using imported_table_storage_t = ::uwvm2::uwvm::runtime::storage::imported_table_storage_t;
    using table_link_kind = imported_table_storage_t::imported_table_link_kind;

    auto const imported_table_count{runtime_module.imported_table_vec_storage.size()};
    auto const table_index_uz{static_cast<::std::size_t>(table_index)};

    if(table_index_uz < imported_table_count)
    {
        auto curr{::std::addressof(runtime_module.imported_table_vec_storage.index_unchecked(table_index_uz))};
        for(;;)
        {
            if(curr == nullptr) [[unlikely]] { return nullptr; }

            switch(curr->link_kind)
            {
                case table_link_kind::defined:
                {
                    return curr->target.defined_ptr;
                }
                case table_link_kind::imported:
                {
                    curr = curr->target.imported_ptr;
                    continue;
                }
                [[unlikely]] default:
                {
                    return nullptr;
                }
            }
        }
    }

    auto const local_table_index{table_index_uz - imported_table_count};
    if(local_table_index >= runtime_module.local_defined_table_vec_storage.size()) [[unlikely]] { return nullptr; }
    return ::std::addressof(runtime_module.local_defined_table_vec_storage.index_unchecked(local_table_index));
}

[[nodiscard]] inline runtime_call_indirect_callee_resolution_t
    resolve_runtime_call_indirect_callee(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                         runtime_table_elem_storage_t const& elem) noexcept
{
    using table_elem_type = ::uwvm2::uwvm::runtime::storage::local_defined_table_elem_storage_type_t;

    runtime_call_indirect_callee_resolution_t result{.state_valid = true};

    auto const imported_func_count{runtime_module.imported_function_vec_storage.size()};
    auto const local_func_count{runtime_module.local_defined_function_vec_storage.size()};
    auto imported_func_begin{runtime_module.imported_function_vec_storage.data()};
    auto local_func_begin{runtime_module.local_defined_function_vec_storage.data()};

    switch(elem.type)
    {
        case table_elem_type::func_ref_imported:
        {
            auto imported_func_ptr{elem.storage.imported_ptr};
            if(imported_func_ptr == nullptr) { return result; }

            auto import_type_ptr{imported_func_ptr->import_type_ptr};
            if(import_type_ptr == nullptr || import_type_ptr->imports.type != validation_module_traits_t::external_types::func ||
               import_type_ptr->imports.storage.function == nullptr) [[unlikely]]
            {
                return {};
            }

            result.present = true;
            result.function_type_ptr = import_type_ptr->imports.storage.function;

            if(imported_func_begin == nullptr || imported_func_ptr < imported_func_begin || imported_func_ptr >= imported_func_begin + imported_func_count)
            {
                return result;
            }

            auto const func_index_uz{static_cast<::std::size_t>(imported_func_ptr - imported_func_begin)};
            if(func_index_uz > static_cast<::std::size_t>(::std::numeric_limits<validation_module_traits_t::wasm_u32>::max())) [[unlikely]] { return {}; }

            auto const callee_resolution{resolve_runtime_direct_callee(runtime_module, static_cast<validation_module_traits_t::wasm_u32>(func_index_uz))};
            if(!callee_resolution.state_valid) [[unlikely]] { return {}; }

            if(callee_resolution.function_type_ptr != nullptr) { result.function_type_ptr = callee_resolution.function_type_ptr; }
            if(!callee_resolution.direct_callable) { return result; }

            result.belongs_to_current_module = true;
            result.func_index = callee_resolution.func_index;
            return result;
        }
        case table_elem_type::func_ref_defined:
        {
            auto defined_func_ptr{elem.storage.defined_ptr};
            if(defined_func_ptr == nullptr) { return result; }
            if(defined_func_ptr->function_type_ptr == nullptr) [[unlikely]] { return {}; }

            result.present = true;
            result.function_type_ptr = defined_func_ptr->function_type_ptr;

            if(local_func_begin == nullptr || defined_func_ptr < local_func_begin || defined_func_ptr >= local_func_begin + local_func_count) { return result; }

            auto const local_func_index{static_cast<::std::size_t>(defined_func_ptr - local_func_begin)};
            if(imported_func_count > static_cast<::std::size_t>(::std::numeric_limits<validation_module_traits_t::wasm_u32>::max()) ||
               local_func_index > static_cast<::std::size_t>(::std::numeric_limits<validation_module_traits_t::wasm_u32>::max()) - imported_func_count)
                [[unlikely]]
            {
                return {};
            }

            result.belongs_to_current_module = true;
            result.func_index = static_cast<validation_module_traits_t::wasm_u32>(imported_func_count + local_func_index);
            return result;
        }
        [[unlikely]] default:
        {
            return {};
        }
    }
}

struct runtime_memory_access_info_t
{
    runtime_native_memory_t* memory_p{};
    ::uwvm2::uwvm::wasm::type::local_imported_t* local_imported_module_ptr{};
    ::std::size_t local_imported_memory_index{};
    ::std::uint_least64_t local_imported_page_size_bytes{};
    ::std::size_t max_limit_memory_length{};
    ::std::byte* stable_memory_begin{};
    ::std::atomic_size_t* stable_memory_length_p{};
    ::std::size_t const* stable_memory_length_value_p{};
    unsigned custom_page_size_log2{};
    bool mmap_requires_dynamic_bounds{};
    bool mmap_uses_partial_protection{};
};

template <typename Memory>
[[nodiscard]] inline ::std::size_t get_runtime_memory_backend_max_limit_length_impl(Memory const& memory) noexcept
{
#if defined(UWVM_SUPPORT_MMAP)
    if constexpr(Memory::can_mmap)
    {
        if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
        {
            switch(memory.status)
            {
                case ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm32:
                {
                    constexpr auto max_full_protection_wasm32_length_half{::uwvm2::object::memory::linear::max_full_protection_wasm32_length / 2u};
                    return static_cast<::std::size_t>(max_full_protection_wasm32_length_half);
                }
                case ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm64:
                {
                    return static_cast<::std::size_t>(::uwvm2::object::memory::linear::max_partial_protection_wasm64_length);
                }
                [[unlikely]] default:
                {
# if (defined(_DEBUG) || defined(DEBUG)) && defined(UWVM_ENABLE_DETAILED_DEBUG_CHECK)
                    ::uwvm2::utils::debug::trap_and_inform_bug_pos();
# endif
                    return 0uz;
                }
            }
        }
        else
        {
            return static_cast<::std::size_t>(::uwvm2::object::memory::linear::max_partial_protection_wasm32_length);
        }
    }
    else
    {
        static_cast<void>(memory);
        return ::std::numeric_limits<::std::size_t>::max();
    }
#else
    static_cast<void>(memory);
    return ::std::numeric_limits<::std::size_t>::max();
#endif
}

template <typename Memory>
inline void populate_runtime_memory_access_info_mmap_fields(runtime_memory_access_info_t& result, Memory& memory) noexcept
{
    if constexpr(requires { memory.memory_length; }) { result.stable_memory_length_value_p = ::std::addressof(memory.memory_length); }

#if defined(UWVM_SUPPORT_MMAP)
    if constexpr(Memory::can_mmap)
    {
        result.stable_memory_begin = memory.memory_begin;
        result.stable_memory_length_p = memory.memory_length_p;
        result.mmap_requires_dynamic_bounds = memory.require_dynamic_determination_memory_size();
        if(!result.mmap_requires_dynamic_bounds)
        {
            if constexpr(sizeof(::std::uintptr_t) >= sizeof(::std::uint_least64_t))
            {
                result.mmap_uses_partial_protection = memory.status == ::uwvm2::object::memory::linear::mmap_memory_status_t::wasm64;
            }
            else
            {
                result.mmap_uses_partial_protection = true;
            }
        }
    }
    else
    {
        static_cast<void>(result);
        static_cast<void>(memory);
    }
#else
    static_cast<void>(result);
    static_cast<void>(memory);
#endif
}

[[nodiscard]] inline ::std::size_t get_runtime_memory_max_limit_length_from_limits(auto const& limits) noexcept
{
    if(!limits.present_max) { return ::std::numeric_limits<::std::size_t>::max(); }

    constexpr ::std::size_t wasm_page_bytes{65536uz};
    auto const max_pages{static_cast<::std::size_t>(limits.max)};
    if(max_pages > (::std::numeric_limits<::std::size_t>::max() / wasm_page_bytes)) { return ::std::numeric_limits<::std::size_t>::max(); }
    return max_pages * wasm_page_bytes;
}

[[nodiscard]] inline ::std::size_t get_runtime_memory_backend_max_limit_length(runtime_native_memory_t const& memory) noexcept
{ return get_runtime_memory_backend_max_limit_length_impl(memory); }

[[nodiscard]] inline ::std::uint_least64_t get_runtime_partial_protection_limit_escape_offset() noexcept
{
#if defined(UWVM_SUPPORT_MMAP)
    if constexpr(sizeof(::std::uintptr_t) >= sizeof(::std::uint_least64_t))
    {
        return static_cast<::std::uint_least64_t>(1u) << ::uwvm2::object::memory::linear::max_partial_protection_wasm64_index;
    }
    else
    {
        return static_cast<::std::uint_least64_t>(1u) << ::uwvm2::object::memory::linear::max_partial_protection_wasm32_index;
    }
#else
    return 0u;
#endif
}

[[nodiscard]] inline ::std::size_t refine_runtime_memory_max_limit_length(runtime_native_memory_t const& memory, ::std::size_t max_limit_memory_length) noexcept
{
    auto const backend_max_limit_length{get_runtime_memory_backend_max_limit_length(memory)};
    return backend_max_limit_length < max_limit_memory_length ? backend_max_limit_length : max_limit_memory_length;
}

[[nodiscard]] inline runtime_memory_access_info_t
    resolve_runtime_memory_access_info(::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                       validation_module_traits_t::wasm_u32 memory_index) noexcept
{
    runtime_memory_access_info_t result{};

    auto const imported_memory_count{runtime_module.imported_memory_vec_storage.size()};
    auto const local_memory_count{runtime_module.local_defined_memory_vec_storage.size()};
    auto const memory_index_uz{static_cast<::std::size_t>(memory_index)};

    if(memory_index_uz < imported_memory_count)
    {
        using imported_memory_storage_t = ::uwvm2::uwvm::runtime::storage::imported_memory_storage_t;
        using memory_link_kind = imported_memory_storage_t::imported_memory_link_kind;

        auto const& imported_memory_rec{runtime_module.imported_memory_vec_storage.index_unchecked(memory_index_uz)};
        auto import_type_ptr{imported_memory_rec.import_type_ptr};
        if(import_type_ptr == nullptr || import_type_ptr->imports.type != validation_module_traits_t::external_types::memory) [[unlikely]] { return result; }

        result.max_limit_memory_length = get_runtime_memory_max_limit_length_from_limits(imported_memory_rec.effective_limits);

        imported_memory_storage_t const* curr{::std::addressof(imported_memory_rec)};
        for(;;)
        {
            if(curr == nullptr) [[unlikely]] { return {}; }

            switch(curr->link_kind)
            {
                case memory_link_kind::imported:
                {
                    curr = curr->target.imported_ptr;
                    continue;
                }
                case memory_link_kind::defined:
                {
                    auto defined_memory{curr->target.defined_ptr};
                    if(defined_memory == nullptr) [[unlikely]] { return {}; }
                    result.memory_p = ::std::addressof(defined_memory->memory);
                    result.max_limit_memory_length = get_runtime_memory_max_limit_length_from_limits(defined_memory->effective_limits);
                    result.max_limit_memory_length = refine_runtime_memory_max_limit_length(defined_memory->memory, result.max_limit_memory_length);
                    result.custom_page_size_log2 = defined_memory->memory.custom_page_size_log2;
                    populate_runtime_memory_access_info_mmap_fields(result, defined_memory->memory);
                    return result;
                }
                case memory_link_kind::local_imported:
                {
                    result.local_imported_module_ptr = curr->target.local_imported.module_ptr;
                    result.local_imported_memory_index = curr->target.local_imported.index;
                    if(result.local_imported_module_ptr == nullptr) [[unlikely]] { return {}; }
                    result.local_imported_page_size_bytes = result.local_imported_module_ptr->memory_page_size_from_index(result.local_imported_memory_index);
                    return result;
                }
                [[unlikely]] default:
                {
                    return {};
                }
            }
        }
    }

    auto const local_memory_index{memory_index_uz - imported_memory_count};
    if(local_memory_index >= local_memory_count) [[unlikely]] { return result; }

    auto const& local_memory_rec{runtime_module.local_defined_memory_vec_storage.index_unchecked(local_memory_index)};
    auto memory_type_ptr{local_memory_rec.memory_type_ptr};
    if(memory_type_ptr == nullptr) [[unlikely]] { return result; }

    result.memory_p = const_cast<runtime_native_memory_t*>(::std::addressof(local_memory_rec.memory));
    result.max_limit_memory_length = get_runtime_memory_max_limit_length_from_limits(local_memory_rec.effective_limits);
    result.max_limit_memory_length = refine_runtime_memory_max_limit_length(local_memory_rec.memory, result.max_limit_memory_length);
    result.custom_page_size_log2 = local_memory_rec.memory.custom_page_size_log2;
    populate_runtime_memory_access_info_mmap_fields(result, local_memory_rec.memory);
    return result;
}

struct llvm_jit_wasm32_effective_offset_t
{
    ::std::uint_least64_t offset{};
    bool offset_65_bit{};
};

template <typename Integer>
[[nodiscard]] inline bool llvm_jit_add_overflow(Integer left, Integer right, Integer& result) noexcept
{
    if constexpr(requires { __builtin_add_overflow(left, right, ::std::addressof(result)); })
    {
        return __builtin_add_overflow(left, right, ::std::addressof(result));
    }
    else
    {
        result = static_cast<Integer>(left + right);
        return result < left;
    }
}

[[nodiscard]] inline llvm_jit_wasm32_effective_offset_t llvm_jit_compute_wasm32_effective_offset(runtime_wasm_i32 address,
                                                                                                 validation_module_traits_t::wasm_u32 static_offset) noexcept
{
    if constexpr(sizeof(::std::size_t) >= sizeof(::std::uint_least64_t))
    {
        auto const dynamic_offset{static_cast<::std::int_least64_t>(address)};
        auto const static_offset_i64{static_cast<::std::int_least64_t>(static_cast<::std::uint_least32_t>(static_offset))};
        auto const sum_u64{static_cast<::std::uint_least64_t>(dynamic_offset + static_offset_i64)};
        return {.offset = sum_u64, .offset_65_bit = (sum_u64 & 0xffffffff00000000ull) != 0ull};
    }
    else
    {
        auto const static_offset_u32{static_cast<::std::uint_least32_t>(static_offset)};
        ::std::uint_least32_t low{};
        bool out_of_range{};

        if(address >= 0) [[likely]]
        {
            auto const dynamic_offset_u32{static_cast<::std::uint_least32_t>(address)};
            out_of_range = llvm_jit_add_overflow(dynamic_offset_u32, static_offset_u32, low);
        }
        else
        {
            auto const abs_dynamic_offset{static_cast<::std::uint_least32_t>(0u - static_cast<::std::uint_least32_t>(address))};
            low = static_cast<::std::uint_least32_t>(static_offset_u32 - abs_dynamic_offset);
            out_of_range = static_offset_u32 < abs_dynamic_offset;
        }

        return {.offset = static_cast<::std::uint_least64_t>(low), .offset_65_bit = out_of_range};
    }
}

template <typename UInt>
[[nodiscard]] inline UInt llvm_jit_load_little_endian_integer(::std::byte const* memory_ptr) noexcept
{
    UInt value{};
    ::std::memcpy(::std::addressof(value), memory_ptr, sizeof(UInt));
    return ::fast_io::little_endian(value);
}

template <typename UInt>
inline void llvm_jit_store_little_endian_integer(::std::byte* memory_ptr, UInt value) noexcept
{
    value = ::fast_io::little_endian(value);
    ::std::memcpy(memory_ptr, ::std::addressof(value), sizeof(UInt));
}

[[noreturn]] inline void llvm_jit_memory_bridge_trap() noexcept { ::fast_io::fast_terminate(); }

template <typename MemoryT, typename Fn>
[[nodiscard]] inline bool llvm_jit_try_checked_memory_access(MemoryT const& memory,
                                                             llvm_jit_wasm32_effective_offset_t effective_offset,
                                                             ::std::size_t access_size,
                                                             Fn&& fn)
{
    auto const checked_access{
        [&](::std::byte* memory_begin, ::std::size_t memory_length) noexcept
        {
            if(memory_begin == nullptr || effective_offset.offset_65_bit || access_size > memory_length ||
               effective_offset.offset > static_cast<::std::uint_least64_t>(memory_length - access_size))
            {
                return false;
            }

            fn(memory_begin, static_cast<::std::size_t>(effective_offset.offset));
            return true;
        }};

    if constexpr(MemoryT::can_mmap)
    {
        return ::uwvm2::object::memory::linear::with_memory_access_snapshot(
            memory,
            [&](::std::byte* memory_begin, ::std::size_t memory_length) noexcept { return checked_access(memory_begin, memory_length); });
    }
    else if constexpr(MemoryT::support_multi_thread)
    {
        [[maybe_unused]] ::uwvm2::object::memory::linear::memory_operation_guard_t guard{memory.growing_flag_p, memory.active_ops_p};
        return checked_access(memory.memory_begin, memory.memory_length);
    }
    else
    {
        return checked_access(memory.memory_begin, memory.memory_length);
    }
}

template <typename Fn>
inline void llvm_jit_with_checked_memory_access(::std::uintptr_t memory_address,
                                                validation_module_traits_t::wasm_u32 static_offset,
                                                runtime_wasm_i32 address,
                                                ::std::size_t access_size,
                                                Fn&& fn)
{
    auto memory_p{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    if(memory_p == nullptr) [[unlikely]] { llvm_jit_memory_bridge_trap(); }

    auto const effective_offset{llvm_jit_compute_wasm32_effective_offset(address, static_offset)};
    if(!llvm_jit_try_checked_memory_access(*memory_p, effective_offset, access_size, ::std::forward<Fn>(fn))) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
    }
}

template <typename ResultType, ::std::size_t LoadBytes, bool Signed = false>
[[nodiscard]] inline ResultType
    llvm_jit_memory_load_bridge(::std::uintptr_t memory_address, validation_module_traits_t::wasm_u32 static_offset, runtime_wasm_i32 address)
{
    ResultType result{};
    llvm_jit_with_checked_memory_access(
        memory_address,
        static_offset,
        address,
        LoadBytes,
        [&](::std::byte* memory_begin, ::std::size_t effective_offset) noexcept
        {
            auto load_ptr{memory_begin + effective_offset};

            if constexpr(::std::same_as<ResultType, runtime_wasm_i32>)
            {
                if constexpr(LoadBytes == 1uz)
                {
                    auto const value{llvm_jit_load_little_endian_integer<::std::uint_least8_t>(load_ptr)};
                    if constexpr(Signed) { result = static_cast<runtime_wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least8_t>(value))); }
                    else
                    {
                        result = static_cast<runtime_wasm_i32>(static_cast<::std::uint_least32_t>(value));
                    }
                }
                else if constexpr(LoadBytes == 2uz)
                {
                    auto const value{llvm_jit_load_little_endian_integer<::std::uint_least16_t>(load_ptr)};
                    if constexpr(Signed)
                    {
                        result = static_cast<runtime_wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least16_t>(value)));
                    }
                    else
                    {
                        result = static_cast<runtime_wasm_i32>(static_cast<::std::uint_least32_t>(value));
                    }
                }
                else if constexpr(LoadBytes == 4uz)
                {
                    result = ::std::bit_cast<runtime_wasm_i32>(llvm_jit_load_little_endian_integer<::std::uint_least32_t>(load_ptr));
                }
            }
            else if constexpr(::std::same_as<ResultType, runtime_wasm_i64>)
            {
                if constexpr(LoadBytes == 1uz)
                {
                    auto const value{llvm_jit_load_little_endian_integer<::std::uint_least8_t>(load_ptr)};
                    if constexpr(Signed) { result = static_cast<runtime_wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least8_t>(value))); }
                    else
                    {
                        result = static_cast<runtime_wasm_i64>(static_cast<::std::uint_least64_t>(value));
                    }
                }
                else if constexpr(LoadBytes == 2uz)
                {
                    auto const value{llvm_jit_load_little_endian_integer<::std::uint_least16_t>(load_ptr)};
                    if constexpr(Signed)
                    {
                        result = static_cast<runtime_wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least16_t>(value)));
                    }
                    else
                    {
                        result = static_cast<runtime_wasm_i64>(static_cast<::std::uint_least64_t>(value));
                    }
                }
                else if constexpr(LoadBytes == 4uz)
                {
                    auto const value{llvm_jit_load_little_endian_integer<::std::uint_least32_t>(load_ptr)};
                    if constexpr(Signed)
                    {
                        result = static_cast<runtime_wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least32_t>(value)));
                    }
                    else
                    {
                        result = static_cast<runtime_wasm_i64>(static_cast<::std::uint_least64_t>(value));
                    }
                }
                else if constexpr(LoadBytes == 8uz)
                {
                    result = ::std::bit_cast<runtime_wasm_i64>(llvm_jit_load_little_endian_integer<::std::uint_least64_t>(load_ptr));
                }
            }
            else if constexpr(::std::same_as<ResultType, runtime_wasm_f32>)
            {
                static_assert(LoadBytes == 4uz);
                result = ::std::bit_cast<runtime_wasm_f32>(llvm_jit_load_little_endian_integer<::std::uint_least32_t>(load_ptr));
            }
            else if constexpr(::std::same_as<ResultType, runtime_wasm_f64>)
            {
                static_assert(LoadBytes == 8uz);
                result = ::std::bit_cast<runtime_wasm_f64>(llvm_jit_load_little_endian_integer<::std::uint_least64_t>(load_ptr));
            }
        });
    return result;
}

template <typename ResultType, ::std::size_t LoadBytes, bool Signed = false>
[[nodiscard]] inline ResultType llvm_jit_local_imported_memory_load_bridge(::std::uintptr_t local_imported_module_address,
                                                                           ::std::size_t memory_index,
                                                                           validation_module_traits_t::wasm_u32 static_offset,
                                                                           runtime_wasm_i32 address)
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { llvm_jit_memory_bridge_trap(); }

    auto const effective_offset{llvm_jit_compute_wasm32_effective_offset(address, static_offset)};
    if(effective_offset.offset_65_bit) [[unlikely]] { llvm_jit_memory_bridge_trap(); }

    ::std::byte load_buffer[LoadBytes]{};
    if(!local_imported_module->memory_read_from_index(memory_index, effective_offset.offset, load_buffer, LoadBytes)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
    }

    ResultType result{};
    auto load_ptr{load_buffer};

    if constexpr(::std::same_as<ResultType, runtime_wasm_i32>)
    {
        if constexpr(LoadBytes == 1uz)
        {
            auto const value{llvm_jit_load_little_endian_integer<::std::uint_least8_t>(load_ptr)};
            if constexpr(Signed) { result = static_cast<runtime_wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least8_t>(value))); }
            else
            {
                result = static_cast<runtime_wasm_i32>(static_cast<::std::uint_least32_t>(value));
            }
        }
        else if constexpr(LoadBytes == 2uz)
        {
            auto const value{llvm_jit_load_little_endian_integer<::std::uint_least16_t>(load_ptr)};
            if constexpr(Signed) { result = static_cast<runtime_wasm_i32>(static_cast<::std::int_least32_t>(static_cast<::std::int_least16_t>(value))); }
            else
            {
                result = static_cast<runtime_wasm_i32>(static_cast<::std::uint_least32_t>(value));
            }
        }
        else if constexpr(LoadBytes == 4uz)
        {
            result = ::std::bit_cast<runtime_wasm_i32>(llvm_jit_load_little_endian_integer<::std::uint_least32_t>(load_ptr));
        }
    }
    else if constexpr(::std::same_as<ResultType, runtime_wasm_i64>)
    {
        if constexpr(LoadBytes == 1uz)
        {
            auto const value{llvm_jit_load_little_endian_integer<::std::uint_least8_t>(load_ptr)};
            if constexpr(Signed) { result = static_cast<runtime_wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least8_t>(value))); }
            else
            {
                result = static_cast<runtime_wasm_i64>(static_cast<::std::uint_least64_t>(value));
            }
        }
        else if constexpr(LoadBytes == 2uz)
        {
            auto const value{llvm_jit_load_little_endian_integer<::std::uint_least16_t>(load_ptr)};
            if constexpr(Signed) { result = static_cast<runtime_wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least16_t>(value))); }
            else
            {
                result = static_cast<runtime_wasm_i64>(static_cast<::std::uint_least64_t>(value));
            }
        }
        else if constexpr(LoadBytes == 4uz)
        {
            auto const value{llvm_jit_load_little_endian_integer<::std::uint_least32_t>(load_ptr)};
            if constexpr(Signed) { result = static_cast<runtime_wasm_i64>(static_cast<::std::int_least64_t>(static_cast<::std::int_least32_t>(value))); }
            else
            {
                result = static_cast<runtime_wasm_i64>(static_cast<::std::uint_least64_t>(value));
            }
        }
        else if constexpr(LoadBytes == 8uz)
        {
            result = ::std::bit_cast<runtime_wasm_i64>(llvm_jit_load_little_endian_integer<::std::uint_least64_t>(load_ptr));
        }
    }
    else if constexpr(::std::same_as<ResultType, runtime_wasm_f32>)
    {
        static_assert(LoadBytes == 4uz);
        result = ::std::bit_cast<runtime_wasm_f32>(llvm_jit_load_little_endian_integer<::std::uint_least32_t>(load_ptr));
    }
    else if constexpr(::std::same_as<ResultType, runtime_wasm_f64>)
    {
        static_assert(LoadBytes == 8uz);
        result = ::std::bit_cast<runtime_wasm_f64>(llvm_jit_load_little_endian_integer<::std::uint_least64_t>(load_ptr));
    }

    return result;
}

template <typename ValueType, ::std::size_t StoreBytes>
inline void
    llvm_jit_memory_store_bridge(::std::uintptr_t memory_address, validation_module_traits_t::wasm_u32 static_offset, runtime_wasm_i32 address, ValueType value)
{
    llvm_jit_with_checked_memory_access(memory_address,
                                        static_offset,
                                        address,
                                        StoreBytes,
                                        [&](::std::byte* memory_begin, ::std::size_t effective_offset) noexcept
                                        {
                                            auto store_ptr{memory_begin + effective_offset};

                                            if constexpr(::std::same_as<ValueType, runtime_wasm_i32>)
                                            {
                                                auto const unsigned_value{::std::bit_cast<::std::uint_least32_t>(value)};
                                                if constexpr(StoreBytes == 1uz)
                                                {
                                                    auto const truncated{static_cast<::std::uint_least8_t>(unsigned_value)};
                                                    ::std::memcpy(store_ptr, ::std::addressof(truncated), sizeof(truncated));
                                                }
                                                else if constexpr(StoreBytes == 2uz)
                                                {
                                                    llvm_jit_store_little_endian_integer(store_ptr, static_cast<::std::uint_least16_t>(unsigned_value));
                                                }
                                                else if constexpr(StoreBytes == 4uz) { llvm_jit_store_little_endian_integer(store_ptr, unsigned_value); }
                                            }
                                            else if constexpr(::std::same_as<ValueType, runtime_wasm_i64>)
                                            {
                                                auto const unsigned_value{::std::bit_cast<::std::uint_least64_t>(value)};
                                                if constexpr(StoreBytes == 1uz)
                                                {
                                                    auto const truncated{static_cast<::std::uint_least8_t>(unsigned_value)};
                                                    ::std::memcpy(store_ptr, ::std::addressof(truncated), sizeof(truncated));
                                                }
                                                else if constexpr(StoreBytes == 2uz)
                                                {
                                                    llvm_jit_store_little_endian_integer(store_ptr, static_cast<::std::uint_least16_t>(unsigned_value));
                                                }
                                                else if constexpr(StoreBytes == 4uz)
                                                {
                                                    llvm_jit_store_little_endian_integer(store_ptr, static_cast<::std::uint_least32_t>(unsigned_value));
                                                }
                                                else if constexpr(StoreBytes == 8uz) { llvm_jit_store_little_endian_integer(store_ptr, unsigned_value); }
                                            }
                                            else if constexpr(::std::same_as<ValueType, runtime_wasm_f32>)
                                            {
                                                static_assert(StoreBytes == 4uz);
                                                llvm_jit_store_little_endian_integer(store_ptr, ::std::bit_cast<::std::uint_least32_t>(value));
                                            }
                                            else if constexpr(::std::same_as<ValueType, runtime_wasm_f64>)
                                            {
                                                static_assert(StoreBytes == 8uz);
                                                llvm_jit_store_little_endian_integer(store_ptr, ::std::bit_cast<::std::uint_least64_t>(value));
                                            }
                                        });
}

template <typename ValueType, ::std::size_t StoreBytes>
inline void llvm_jit_local_imported_memory_store_bridge(::std::uintptr_t local_imported_module_address,
                                                        ::std::size_t memory_index,
                                                        validation_module_traits_t::wasm_u32 static_offset,
                                                        runtime_wasm_i32 address,
                                                        ValueType value)
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { llvm_jit_memory_bridge_trap(); }

    auto const effective_offset{llvm_jit_compute_wasm32_effective_offset(address, static_offset)};
    if(effective_offset.offset_65_bit) [[unlikely]] { llvm_jit_memory_bridge_trap(); }

    ::std::byte store_buffer[StoreBytes]{};
    auto store_ptr{store_buffer};

    if constexpr(::std::same_as<ValueType, runtime_wasm_i32>)
    {
        auto const unsigned_value{::std::bit_cast<::std::uint_least32_t>(value)};
        if constexpr(StoreBytes == 1uz)
        {
            auto const truncated{static_cast<::std::uint_least8_t>(unsigned_value)};
            ::std::memcpy(store_ptr, ::std::addressof(truncated), sizeof(truncated));
        }
        else if constexpr(StoreBytes == 2uz) { llvm_jit_store_little_endian_integer(store_ptr, static_cast<::std::uint_least16_t>(unsigned_value)); }
        else if constexpr(StoreBytes == 4uz) { llvm_jit_store_little_endian_integer(store_ptr, unsigned_value); }
    }
    else if constexpr(::std::same_as<ValueType, runtime_wasm_i64>)
    {
        auto const unsigned_value{::std::bit_cast<::std::uint_least64_t>(value)};
        if constexpr(StoreBytes == 1uz)
        {
            auto const truncated{static_cast<::std::uint_least8_t>(unsigned_value)};
            ::std::memcpy(store_ptr, ::std::addressof(truncated), sizeof(truncated));
        }
        else if constexpr(StoreBytes == 2uz) { llvm_jit_store_little_endian_integer(store_ptr, static_cast<::std::uint_least16_t>(unsigned_value)); }
        else if constexpr(StoreBytes == 4uz) { llvm_jit_store_little_endian_integer(store_ptr, static_cast<::std::uint_least32_t>(unsigned_value)); }
        else if constexpr(StoreBytes == 8uz) { llvm_jit_store_little_endian_integer(store_ptr, unsigned_value); }
    }
    else if constexpr(::std::same_as<ValueType, runtime_wasm_f32>)
    {
        static_assert(StoreBytes == 4uz);
        llvm_jit_store_little_endian_integer(store_ptr, ::std::bit_cast<::std::uint_least32_t>(value));
    }
    else if constexpr(::std::same_as<ValueType, runtime_wasm_f64>)
    {
        static_assert(StoreBytes == 8uz);
        llvm_jit_store_little_endian_integer(store_ptr, ::std::bit_cast<::std::uint_least64_t>(value));
    }

    if(!local_imported_module->memory_write_to_index(memory_index, effective_offset.offset, store_buffer, StoreBytes)) [[unlikely]]
    {
        llvm_jit_memory_bridge_trap();
    }
}

[[nodiscard]] inline bool llvm_jit_local_imported_memory_snapshot_bridge(::std::uintptr_t local_imported_module_address,
                                                                         ::std::size_t memory_index,
                                                                         ::std::uintptr_t* memory_begin_address_out,
                                                                         ::std::size_t* byte_length_out) noexcept
{
    if(memory_begin_address_out == nullptr || byte_length_out == nullptr) [[unlikely]] { return false; }

    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { return false; }

    ::uwvm2::uwvm::wasm::type::memory_access_snapshot_result_t snapshot{};
    if(!local_imported_module->memory_access_snapshot_from_index(memory_index, snapshot)) [[unlikely]] { return false; }

    auto const page_size_bytes{local_imported_module->memory_page_size_from_index(memory_index)};
    if(page_size_bytes == 0u) [[unlikely]] { return false; }
    if(snapshot.page_count > static_cast<::std::uint_least64_t>((::std::numeric_limits<::std::size_t>::max)() / page_size_bytes)) [[unlikely]] { return false; }

    *memory_begin_address_out = reinterpret_cast<::std::uintptr_t>(snapshot.memory_begin);
    *byte_length_out = static_cast<::std::size_t>(snapshot.page_count * page_size_bytes);
    return true;
}

[[nodiscard]] inline runtime_wasm_i32
    llvm_jit_memory_grow_bridge(::std::uintptr_t memory_address, ::std::size_t max_limit_memory_length, runtime_wasm_i32 delta_i32) noexcept
{
    auto memory_p{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    if(memory_p == nullptr) [[unlikely]] { return static_cast<runtime_wasm_i32>(-1); }

    auto const delta_pages{static_cast<::std::size_t>(static_cast<::std::uint_least32_t>(delta_i32))};
    ::std::size_t old_pages{};

    if(::uwvm2::object::memory::flags::grow_strict)
    {
        return memory_p->grow_strictly(delta_pages, max_limit_memory_length, ::std::addressof(old_pages)) ? static_cast<runtime_wasm_i32>(old_pages)
                                                                                                          : static_cast<runtime_wasm_i32>(-1);
    }

    if constexpr(runtime_native_memory_t::support_multi_thread)
    {
        return memory_p->try_grow_silently(delta_pages, max_limit_memory_length, ::std::addressof(old_pages)) ? static_cast<runtime_wasm_i32>(old_pages)
                                                                                                              : static_cast<runtime_wasm_i32>(-1);
    }
    else
    {
        old_pages = static_cast<::std::size_t>(memory_p->get_page_size());

        auto const limit_pages{max_limit_memory_length >> memory_p->custom_page_size_log2};
        if(old_pages > limit_pages || delta_pages > (limit_pages - old_pages)) [[unlikely]] { return static_cast<runtime_wasm_i32>(-1); }

        memory_p->grow_silently(delta_pages, max_limit_memory_length);
        return static_cast<runtime_wasm_i32>(old_pages);
    }
}

[[nodiscard]] inline runtime_wasm_i32 llvm_jit_memory_size_bridge(::std::uintptr_t memory_address) noexcept
{
    auto memory_p{reinterpret_cast<runtime_native_memory_t*>(memory_address)};
    if(memory_p == nullptr) [[unlikely]] { llvm_jit_memory_bridge_trap(); }

    return static_cast<runtime_wasm_i32>(memory_p->get_page_size());
}

[[nodiscard]] inline runtime_wasm_i32 llvm_jit_local_imported_memory_grow_bridge(::std::uintptr_t local_imported_module_address,
                                                                                 ::std::size_t memory_index,
                                                                                 ::std::size_t max_limit_memory_length,
                                                                                 runtime_wasm_i32 delta_i32) noexcept
{
    auto local_imported_module{reinterpret_cast<::uwvm2::uwvm::wasm::type::local_imported_t*>(local_imported_module_address)};
    if(local_imported_module == nullptr) [[unlikely]] { return static_cast<runtime_wasm_i32>(-1); }

    auto const old_pages{local_imported_module->memory_size_from_index(memory_index)};
    auto const delta_pages{static_cast<::std::uint_least64_t>(static_cast<::std::uint_least32_t>(delta_i32))};
    if(delta_pages == 0u) { return static_cast<runtime_wasm_i32>(old_pages); }

    if(max_limit_memory_length != ::std::numeric_limits<::std::size_t>::max())
    {
        auto const page_size_bytes{local_imported_module->memory_page_size_from_index(memory_index)};
        if(page_size_bytes == 0u) [[unlikely]] { return static_cast<runtime_wasm_i32>(-1); }

        auto const limit_pages{static_cast<::std::uint_least64_t>(max_limit_memory_length) / page_size_bytes};
        if(old_pages > limit_pages || delta_pages > (limit_pages - old_pages)) [[unlikely]] { return static_cast<runtime_wasm_i32>(-1); }
    }

    return local_imported_module->memory_grow_from_index(memory_index, delta_pages) ? static_cast<runtime_wasm_i32>(old_pages)
                                                                                    : static_cast<runtime_wasm_i32>(-1);
}

template <typename FunctionPtr>
[[nodiscard]] inline ::llvm::Constant*
    get_llvm_runtime_bridge_function_pointer(::llvm::LLVMContext& llvm_context, ::llvm::FunctionType* function_type, FunctionPtr function_pointer) noexcept
{
    if(function_type == nullptr) [[unlikely]] { return nullptr; }

    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto llvm_address{::llvm::ConstantInt::get(llvm_intptr_type, reinterpret_cast<::std::uintptr_t>(function_pointer))};
    return ::llvm::ConstantExpr::getIntToPtr(llvm_address, get_llvm_pointer_type(function_type));
}

template <typename Immediate>
[[nodiscard]] inline bool parse_wasm_leb128_immediate(::std::byte const*& code_curr, ::std::byte const* code_end, Immediate& immediate)
{
    using char8_t_const_may_alias_ptr UWVM_GNU_MAY_ALIAS = char8_t const*;
    auto const [imm_next, imm_err]{::fast_io::parse_by_scan(reinterpret_cast<char8_t_const_may_alias_ptr>(code_curr),
                                                            reinterpret_cast<char8_t_const_may_alias_ptr>(code_end),
                                                            ::fast_io::mnp::leb128_get(immediate))};
    if(imm_err != ::fast_io::parse_code::ok) [[unlikely]] { return false; }

    code_curr = reinterpret_cast<::std::byte const*>(imm_next);
    return true;
}

template <typename UInt>
[[nodiscard]] inline bool parse_wasm_little_endian_immediate(::std::byte const*& code_curr, ::std::byte const* code_end, UInt& immediate) noexcept
{
    if(static_cast<::std::size_t>(code_end - code_curr) < sizeof(UInt)) [[unlikely]] { return false; }

    immediate = 0;
    for(::std::size_t byte_index{}; byte_index != sizeof(UInt); ++byte_index)
    {
        immediate |= static_cast<UInt>(::std::to_integer<::std::uint_least8_t>(code_curr[byte_index])) << (byte_index * 8u);
    }

    code_curr += sizeof(UInt);
    return true;
}

inline constexpr runtime_operand_stack_value_type llvm_jit_i32_block_result_arr[]{runtime_operand_stack_value_type::i32};
inline constexpr runtime_operand_stack_value_type llvm_jit_i64_block_result_arr[]{runtime_operand_stack_value_type::i64};
inline constexpr runtime_operand_stack_value_type llvm_jit_f32_block_result_arr[]{runtime_operand_stack_value_type::f32};
inline constexpr runtime_operand_stack_value_type llvm_jit_f64_block_result_arr[]{runtime_operand_stack_value_type::f64};

enum class llvm_jit_control_context_type : unsigned
{
    function,
    block,
    if_then,
    if_else,
    loop
};

struct llvm_jit_control_context_t
{
    llvm_jit_control_context_type type{};
    runtime_block_result_type result{};
    ::llvm::BasicBlock* end_block{};
    ::llvm::PHINode* end_phi{};
    ::llvm::BasicBlock* else_block{};
    ::std::size_t outer_stack_size{};
    ::std::size_t outer_branch_target_stack_size{};
    bool is_reachable{true};
    bool end_block_has_incoming{};
};

struct llvm_jit_branch_target_t
{
    runtime_block_result_type params{};
    ::llvm::BasicBlock* block{};
    ::llvm::PHINode* phi{};
    ::std::size_t control_stack_index{};
};

[[nodiscard]] inline ::std::size_t get_runtime_block_result_count(runtime_block_result_type result) noexcept
{
    if(result.begin == nullptr || result.end == nullptr) { return 0uz; }
    return static_cast<::std::size_t>(result.end - result.begin);
}

[[nodiscard]] inline runtime_operand_stack_value_type get_runtime_block_single_result_type(runtime_block_result_type result) noexcept
{ return get_runtime_block_result_count(result) == 1uz ? result.begin[0] : runtime_operand_stack_value_type{}; }

[[nodiscard]] inline bool
    parse_wasm_block_result_type(::std::byte const*& code_curr, ::std::byte const* code_end, runtime_block_result_type& block_result) noexcept
{
    if(code_curr == code_end) [[unlikely]] { return false; }

    auto const blocktype_byte{static_cast<::std::uint_least8_t>(::std::to_integer<::std::uint_least8_t>(*code_curr))};
    ++code_curr;

    switch(blocktype_byte)
    {
        case 0x40u:
        {
            block_result = {};
            return true;
        }
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::i32)):
        {
            block_result.begin = llvm_jit_i32_block_result_arr;
            block_result.end = llvm_jit_i32_block_result_arr + 1u;
            return true;
        }
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::i64)):
        {
            block_result.begin = llvm_jit_i64_block_result_arr;
            block_result.end = llvm_jit_i64_block_result_arr + 1u;
            return true;
        }
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::f32)):
        {
            block_result.begin = llvm_jit_f32_block_result_arr;
            block_result.end = llvm_jit_f32_block_result_arr + 1u;
            return true;
        }
        case static_cast<::std::uint_least8_t>(static_cast<::uwvm2::parser::wasm::standard::wasm1::type::wasm_byte>(runtime_operand_stack_value_type::f64)):
        {
            block_result.begin = llvm_jit_f64_block_result_arr;
            block_result.end = llvm_jit_f64_block_result_arr + 1u;
            return true;
        }
        [[unlikely]] default:
        {
            return false;
        }
    }
}

[[nodiscard]] inline bool skip_wasm_memarg(::std::byte const*& code_curr, ::std::byte const* code_end)
{
    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    return parse_wasm_leb128_immediate(code_curr, code_end, align) && parse_wasm_leb128_immediate(code_curr, code_end, offset);
}

[[nodiscard]] inline bool skip_wasm_unreachable_noncontrol_instruction(::std::byte const*& code_curr, ::std::byte const* code_end)
{
    if(code_curr == code_end) [[unlikely]] { return false; }

    wasm1_code curr_opbase;
    ::std::memcpy(::std::addressof(curr_opbase), code_curr, sizeof(wasm1_code));

    switch(curr_opbase)
    {
        case wasm1_code::local_get:
        case wasm1_code::local_set:
        case wasm1_code::local_tee:
        case wasm1_code::global_get:
        case wasm1_code::global_set:
        case wasm1_code::call:
        case wasm1_code::br:
        case wasm1_code::br_if:
        {
            ++code_curr;
            validation_module_traits_t::wasm_u32 immediate{};
            return parse_wasm_leb128_immediate(code_curr, code_end, immediate);
        }
        case wasm1_code::call_indirect:
        {
            ++code_curr;
            validation_module_traits_t::wasm_u32 type_index{};
            validation_module_traits_t::wasm_u32 table_index{};
            return parse_wasm_leb128_immediate(code_curr, code_end, type_index) && parse_wasm_leb128_immediate(code_curr, code_end, table_index);
        }
        case wasm1_code::br_table:
        {
            ++code_curr;
            validation_module_traits_t::wasm_u32 target_count{};
            if(!parse_wasm_leb128_immediate(code_curr, code_end, target_count)) [[unlikely]] { return false; }

            for(validation_module_traits_t::wasm_u32 i{}; i != target_count; ++i)
            {
                validation_module_traits_t::wasm_u32 label_index{};
                if(!parse_wasm_leb128_immediate(code_curr, code_end, label_index)) [[unlikely]] { return false; }
            }

            validation_module_traits_t::wasm_u32 default_label{};
            return parse_wasm_leb128_immediate(code_curr, code_end, default_label);
        }
        case wasm1_code::i32_const:
        {
            ++code_curr;
            ::std::int_least32_t immediate{};
            return parse_wasm_leb128_immediate(code_curr, code_end, immediate);
        }
        case wasm1_code::i64_const:
        {
            ++code_curr;
            ::std::int_least64_t immediate{};
            return parse_wasm_leb128_immediate(code_curr, code_end, immediate);
        }
        case wasm1_code::f32_const:
        {
            ++code_curr;
            ::std::uint_least32_t immediate{};
            return parse_wasm_little_endian_immediate(code_curr, code_end, immediate);
        }
        case wasm1_code::f64_const:
        {
            ++code_curr;
            ::std::uint_least64_t immediate{};
            return parse_wasm_little_endian_immediate(code_curr, code_end, immediate);
        }
        case wasm1_code::i32_load:
        case wasm1_code::i64_load:
        case wasm1_code::f32_load:
        case wasm1_code::f64_load:
        case wasm1_code::i32_load8_s:
        case wasm1_code::i32_load8_u:
        case wasm1_code::i32_load16_s:
        case wasm1_code::i32_load16_u:
        case wasm1_code::i64_load8_s:
        case wasm1_code::i64_load8_u:
        case wasm1_code::i64_load16_s:
        case wasm1_code::i64_load16_u:
        case wasm1_code::i64_load32_s:
        case wasm1_code::i64_load32_u:
        case wasm1_code::i32_store:
        case wasm1_code::i64_store:
        case wasm1_code::f32_store:
        case wasm1_code::f64_store:
        case wasm1_code::i32_store8:
        case wasm1_code::i32_store16:
        case wasm1_code::i64_store8:
        case wasm1_code::i64_store16:
        case wasm1_code::i64_store32:
        {
            ++code_curr;
            return skip_wasm_memarg(code_curr, code_end);
        }
        case wasm1_code::memory_size:
        case wasm1_code::memory_grow:
        {
            ++code_curr;
            validation_module_traits_t::wasm_u32 memidx{};
            return parse_wasm_leb128_immediate(code_curr, code_end, memidx);
        }
        [[unlikely]] default:
        {
            ++code_curr;
            return true;
        }
    }
}

struct runtime_local_func_llvm_jit_emit_state_t
{
    bool valid{};
    local_func_storage_t const* local_func_storage_ptr{};
    ::uwvm2::utils::container::vector<runtime_operand_stack_value_type> local_types{};
    ::llvm::LLVMContext* llvm_context_holder{};
    ::llvm::Module* llvm_module{};
    ::llvm::Function* llvm_function{};
    ::std::unique_ptr<::llvm::IRBuilder<>> ir_builder{};
    ::uwvm2::utils::container::vector<::llvm::AllocaInst*> local_pointers{};
    runtime_memory_access_info_t memory0_access_info{};
    bool memory0_access_info_resolved{};
    runtime_block_result_type function_result{};
    ::std::size_t func_result_count_uz{};
    ::llvm::BasicBlock* return_block{};
    ::llvm::PHINode* return_phi{};
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> operand_stack{};
    ::uwvm2::utils::container::vector<llvm_jit_control_context_t> control_stack{};
    ::uwvm2::utils::container::vector<llvm_jit_branch_target_t> branch_target_stack{};
    ::std::size_t unreachable_control_depth{};
};

[[nodiscard]] inline bool try_prepare_runtime_llvm_jit_module_storage(
    ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
    llvm_jit_module_storage_t& module_storage)
{
    module_storage = {};
    module_storage.llvm_context_holder = ::std::make_unique<::llvm::LLVMContext>();
    if(module_storage.llvm_context_holder == nullptr) [[unlikely]] { return false; }

    module_storage.llvm_module = ::std::make_unique<::llvm::Module>(get_llvm_wasm_ir_module_name(runtime_module), *module_storage.llvm_context_holder);
    return module_storage.llvm_module != nullptr;
}

[[nodiscard]] inline bool try_prepare_runtime_local_func_llvm_jit_emit_state(local_func_storage_t const& local_func_storage,
                                                                             llvm_jit_module_storage_t& module_storage,
                                                                             runtime_local_func_llvm_jit_emit_state_t& state)
{
    state = {};

    auto function_type_ptr{local_func_storage.function_type_ptr};
    auto wasm_code_ptr{local_func_storage.wasm_code_ptr};
    if(function_type_ptr == nullptr || wasm_code_ptr == nullptr) [[unlikely]] { return false; }

    auto const func_parameter_begin{function_type_ptr->parameter.begin};
    auto const func_parameter_end{function_type_ptr->parameter.end};
    auto const func_result_begin{function_type_ptr->result.begin};
    auto const func_result_end{function_type_ptr->result.end};

    if(func_parameter_begin == nullptr && func_parameter_begin != func_parameter_end) [[unlikely]] { return false; }
    if(func_result_begin == nullptr && func_result_begin != func_result_end) [[unlikely]] { return false; }

    auto const func_parameter_count_uz{func_parameter_begin == nullptr ? 0uz : static_cast<::std::size_t>(func_parameter_end - func_parameter_begin)};
    auto const func_result_count_uz{func_result_begin == nullptr ? 0uz : static_cast<::std::size_t>(func_result_end - func_result_begin)};
    auto const defined_local_count_uz{static_cast<::std::size_t>(wasm_code_ptr->all_local_count)};
    auto const all_local_count_uz{func_parameter_count_uz + defined_local_count_uz};
    if(func_result_count_uz > 1uz) [[unlikely]] { return false; }

    state.local_types.reserve(all_local_count_uz);
    for(::std::size_t i{}; i != func_parameter_count_uz; ++i)
    {
        state.local_types.push_back(static_cast<runtime_operand_stack_value_type>(func_parameter_begin[i]));
    }
    for(auto const& local_part: wasm_code_ptr->locals)
    {
        for(validation_module_traits_t::wasm_u32 i{}; i != local_part.count; ++i)
        {
            state.local_types.push_back(static_cast<runtime_operand_stack_value_type>(local_part.type));
        }
    }

    if(state.local_types.size() != all_local_count_uz) [[unlikely]] { return false; }

    auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
    if(runtime_module_ptr == nullptr) [[unlikely]] { return false; }
    if(module_storage.llvm_context_holder == nullptr || module_storage.llvm_module == nullptr) [[unlikely]] { return false; }

    state.local_func_storage_ptr = ::std::addressof(local_func_storage);
    state.func_result_count_uz = func_result_count_uz;
    state.function_result = runtime_block_result_type{func_result_begin, func_result_end};

    state.llvm_context_holder = module_storage.llvm_context_holder.get();
    auto& llvm_context{*state.llvm_context_holder};
    state.llvm_module = module_storage.llvm_module.get();

    ::uwvm2::utils::container::vector<::llvm::Type*> llvm_parameter_types{};
    llvm_parameter_types.reserve(func_parameter_count_uz);
    for(::std::size_t i{}; i != func_parameter_count_uz; ++i)
    {
        auto llvm_parameter_type{get_llvm_type_from_wasm_value_type(llvm_context, static_cast<runtime_operand_stack_value_type>(func_parameter_begin[i]))};
        if(llvm_parameter_type == nullptr) [[unlikely]] { return false; }
        llvm_parameter_types.push_back(llvm_parameter_type);
    }

    ::llvm::Type* llvm_result_type{::llvm::Type::getVoidTy(llvm_context)};
    if(func_result_count_uz == 1uz)
    {
        llvm_result_type = get_llvm_type_from_wasm_value_type(llvm_context, static_cast<runtime_operand_stack_value_type>(func_result_begin[0]));
        if(llvm_result_type == nullptr) [[unlikely]] { return false; }
    }

    auto llvm_function_type{::llvm::FunctionType::get(llvm_result_type, llvm_parameter_types, false)};
    auto const function_name{get_llvm_wasm_function_name(*runtime_module_ptr, static_cast<validation_module_traits_t::wasm_u32>(local_func_storage.function_index))};
    state.llvm_function = state.llvm_module->getFunction(function_name);
    if(state.llvm_function == nullptr)
    {
        state.llvm_function = ::llvm::Function::Create(llvm_function_type, ::llvm::Function::ExternalLinkage, function_name, state.llvm_module);
    }
    else
    {
        if(state.llvm_function->getFunctionType() != llvm_function_type || !state.llvm_function->empty()) [[unlikely]] { return false; }
        state.llvm_function->setLinkage(::llvm::Function::ExternalLinkage);
    }
    if(state.llvm_function == nullptr) [[unlikely]] { return false; }

    auto entry_block{::llvm::BasicBlock::Create(llvm_context, "entry", state.llvm_function)};
    state.ir_builder = ::std::make_unique<::llvm::IRBuilder<>>(entry_block);

    state.local_pointers.reserve(state.local_types.size());
    for(::std::size_t local_index{}; local_index != state.local_types.size(); ++local_index)
    {
        auto const local_type{state.local_types[local_index]};
        auto llvm_local_type{get_llvm_type_from_wasm_value_type(llvm_context, local_type)};
        if(llvm_local_type == nullptr) [[unlikely]] { return false; }

        auto local_pointer{state.ir_builder->CreateAlloca(llvm_local_type, nullptr, "")};
        state.local_pointers.push_back(local_pointer);

        if(local_index < func_parameter_count_uz) { state.ir_builder->CreateStore(state.llvm_function->getArg(local_index), local_pointer); }
        else
        {
            auto zero_constant{get_llvm_zero_constant_from_wasm_value_type(llvm_context, local_type)};
            if(zero_constant == nullptr) [[unlikely]] { return false; }
            state.ir_builder->CreateStore(zero_constant, local_pointer);
        }
    }

    auto const create_optional_result_phi{[&](::llvm::BasicBlock* block, runtime_block_result_type block_result, char const* name) -> ::llvm::PHINode*
                                          {
                                              auto const result_count{get_runtime_block_result_count(block_result)};
                                              if(result_count == 0uz) { return nullptr; }
                                              if(result_count != 1uz || block == nullptr) [[unlikely]] { return nullptr; }

                                              auto phi_type{
                                                  get_llvm_type_from_wasm_value_type(llvm_context, get_runtime_block_single_result_type(block_result))};
                                              if(phi_type == nullptr) [[unlikely]] { return nullptr; }

                                              ::llvm::IRBuilder<> phi_builder(block);
                                              return phi_builder.CreatePHI(phi_type, 0u, name);
                                          }};

    state.return_block = ::llvm::BasicBlock::Create(llvm_context, "return", state.llvm_function);
    state.return_phi = create_optional_result_phi(state.return_block, state.function_result, "return.phi");
    if(func_result_count_uz == 1uz && state.return_phi == nullptr) [[unlikely]] { return false; }

    state.control_stack.push_back({.type = llvm_jit_control_context_type::function,
                                   .result = state.function_result,
                                   .end_block = state.return_block,
                                   .end_phi = state.return_phi,
                                   .else_block = nullptr,
                                   .outer_stack_size = 0uz,
                                   .outer_branch_target_stack_size = 0uz,
                                   .is_reachable = true,
                                   .end_block_has_incoming = false});
    state.branch_target_stack.push_back({.params = state.function_result, .block = state.return_block, .phi = state.return_phi, .control_stack_index = 0uz});
    state.valid = true;
    return true;
}

[[nodiscard]] inline bool finalize_runtime_local_func_llvm_jit_emit_state(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                          llvm_jit_module_storage_t&)
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_module == nullptr || state.llvm_function == nullptr || state.ir_builder == nullptr)
        [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.empty()) [[unlikely]] { return false; }

    auto& ir_builder{*state.ir_builder};

    ir_builder.SetInsertPoint(state.return_block);
    if(state.func_result_count_uz == 0uz) { ir_builder.CreateRetVoid(); }
    else if(state.return_phi != nullptr && state.return_phi->getNumIncomingValues() != 0u) { ir_builder.CreateRet(state.return_phi); }
    else
    {
        if(state.return_phi != nullptr) { state.return_phi->eraseFromParent(); }
        ir_builder.CreateUnreachable();
    }

    ::std::string verify_error{};
    ::llvm::raw_string_ostream verify_stream(verify_error);
    if(::llvm::verifyFunction(*state.llvm_function, ::std::addressof(verify_stream))) [[unlikely]] { return false; }
    if(::llvm::verifyModule(*state.llvm_module, ::std::addressof(verify_stream))) [[unlikely]] { return false; }
    return true;
}

[[nodiscard]] inline llvm_jit_branch_target_t const*
    get_runtime_local_func_llvm_jit_branch_target_by_depth(runtime_local_func_llvm_jit_emit_state_t const& state,
                                                           validation_module_traits_t::wasm_u32 label_depth) noexcept
{
    auto const depth{static_cast<::std::size_t>(label_depth)};
    auto const label_count{state.branch_target_stack.size()};
    if(depth >= label_count) { return nullptr; }
    return ::std::addressof(state.branch_target_stack.index_unchecked(label_count - 1uz - depth));
}

inline void mark_runtime_local_func_llvm_jit_branch_target_has_incoming(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                        llvm_jit_branch_target_t const& target) noexcept
{
    if(target.control_stack_index >= state.control_stack.size()) [[unlikely]] { return; }

    auto& target_context{state.control_stack.index_unchecked(target.control_stack_index)};
    if(target_context.end_block == target.block) { target_context.end_block_has_incoming = true; }
}

[[nodiscard]] inline bool try_get_runtime_local_func_llvm_jit_branch_value(runtime_local_func_llvm_jit_emit_state_t const& state,
                                                                           runtime_block_result_type params,
                                                                           llvm_jit_stack_value_t& branch_value) noexcept
{
    auto const arity{get_runtime_block_result_count(params)};
    if(arity == 0uz)
    {
        branch_value = {};
        return true;
    }
    if(arity != 1uz || state.operand_stack.empty()) [[unlikely]] { return false; }

    branch_value = state.operand_stack.back();
    return branch_value.value != nullptr && branch_value.type == get_runtime_block_single_result_type(params);
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_branch_to_target(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                llvm_jit_branch_target_t const& target)
{
    if(!state.valid || state.ir_builder == nullptr) [[unlikely]] { return false; }

    auto& ir_builder{*state.ir_builder};
    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || current_block->getTerminator() != nullptr) [[unlikely]] { return false; }

    llvm_jit_stack_value_t branch_value{};
    if(!try_get_runtime_local_func_llvm_jit_branch_value(state, target.params, branch_value)) [[unlikely]] { return false; }

    if(get_runtime_block_result_count(target.params) == 1uz)
    {
        if(target.phi == nullptr) [[unlikely]] { return false; }
        target.phi->addIncoming(branch_value.value, current_block);
    }

    ir_builder.CreateBr(target.block);
    mark_runtime_local_func_llvm_jit_branch_target_has_incoming(state, target);
    return true;
}

inline void enter_runtime_local_func_llvm_jit_unreachable_control_context(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    if(state.control_stack.empty()) [[unlikely]] { return; }

    auto& current_context{state.control_stack.back()};
    while(state.operand_stack.size() > current_context.outer_stack_size) { state.operand_stack.pop_back(); }
    current_context.is_reachable = false;
    state.unreachable_control_depth = 0uz;
}

[[nodiscard]] inline ::llvm::PHINode*
    create_runtime_local_func_llvm_jit_optional_result_phi(::llvm::LLVMContext& llvm_context,
                                                           ::llvm::BasicBlock* block,
                                                           runtime_block_result_type block_result,
                                                           char const* name)
{
    auto const result_count{get_runtime_block_result_count(block_result)};
    if(result_count == 0uz) { return nullptr; }
    if(result_count != 1uz || block == nullptr) [[unlikely]] { return nullptr; }

    auto phi_type{get_llvm_type_from_wasm_value_type(llvm_context, get_runtime_block_single_result_type(block_result))};
    if(phi_type == nullptr) [[unlikely]] { return nullptr; }

    ::llvm::IRBuilder<> phi_builder(block);
    return phi_builder.CreatePHI(phi_type, 0u, name);
}

inline void truncate_runtime_local_func_llvm_jit_operand_stack_to(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                  ::std::size_t target_size) noexcept
{
    while(state.operand_stack.size() > target_size) { state.operand_stack.pop_back(); }
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_unreachable(runtime_local_func_llvm_jit_emit_state_t& state)
{
    if(!state.valid || state.llvm_module == nullptr || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }

    auto& ir_builder{*state.ir_builder};
    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || current_block->getTerminator() != nullptr) [[unlikely]] { return false; }

    auto trap_intrinsic{
        ::llvm::Intrinsic::getOrInsertDeclaration(::std::addressof(*state.llvm_module), ::llvm::Intrinsic::trap, {})};
    ir_builder.CreateCall(trap_intrinsic);
    ir_builder.CreateUnreachable();
    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_nop(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    return state.valid && !state.control_stack.empty();
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_block(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                     runtime_block_result_type block_result)
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_function == nullptr || state.control_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable)
    {
        ++state.unreachable_control_depth;
        return true;
    }

    auto& llvm_context{*state.llvm_context_holder};
    auto end_block{::llvm::BasicBlock::Create(llvm_context, "block.end", state.llvm_function)};
    auto end_phi{create_runtime_local_func_llvm_jit_optional_result_phi(llvm_context, end_block, block_result, "block.result")};
    if(get_runtime_block_result_count(block_result) == 1uz && end_phi == nullptr) [[unlikely]] { return false; }

    auto const control_stack_index{state.control_stack.size()};
    state.control_stack.push_back({.type = llvm_jit_control_context_type::block,
                                   .result = block_result,
                                   .end_block = end_block,
                                   .end_phi = end_phi,
                                   .else_block = nullptr,
                                   .outer_stack_size = state.operand_stack.size(),
                                   .outer_branch_target_stack_size = state.branch_target_stack.size(),
                                   .is_reachable = true,
                                   .end_block_has_incoming = false});
    state.branch_target_stack.push_back(
        {.params = block_result, .block = end_block, .phi = end_phi, .control_stack_index = control_stack_index});
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_loop(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                    runtime_block_result_type block_result)
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_function == nullptr || state.ir_builder == nullptr ||
       state.control_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable)
    {
        ++state.unreachable_control_depth;
        return true;
    }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};

    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || current_block->getTerminator() != nullptr) [[unlikely]] { return false; }

    auto loop_body_block{::llvm::BasicBlock::Create(llvm_context, "loop.body", state.llvm_function)};
    auto end_block{::llvm::BasicBlock::Create(llvm_context, "loop.end", state.llvm_function)};
    auto end_phi{create_runtime_local_func_llvm_jit_optional_result_phi(llvm_context, end_block, block_result, "loop.result")};
    if(get_runtime_block_result_count(block_result) == 1uz && end_phi == nullptr) [[unlikely]] { return false; }

    ir_builder.CreateBr(loop_body_block);
    ir_builder.SetInsertPoint(loop_body_block);

    auto const control_stack_index{state.control_stack.size()};
    state.control_stack.push_back({.type = llvm_jit_control_context_type::loop,
                                   .result = block_result,
                                   .end_block = end_block,
                                   .end_phi = end_phi,
                                   .else_block = nullptr,
                                   .outer_stack_size = state.operand_stack.size(),
                                   .outer_branch_target_stack_size = state.branch_target_stack.size(),
                                   .is_reachable = true,
                                   .end_block_has_incoming = false});
    state.branch_target_stack.push_back(
        {.params = {}, .block = loop_body_block, .phi = nullptr, .control_stack_index = control_stack_index});
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_if(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                  runtime_block_result_type block_result)
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_function == nullptr || state.ir_builder == nullptr ||
       state.control_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable)
    {
        ++state.unreachable_control_depth;
        return true;
    }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    auto const condition{state.operand_stack.back()};
    state.operand_stack.pop_back();
    if(condition.type != runtime_operand_stack_value_type::i32 || condition.value == nullptr) [[unlikely]] { return false; }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};

    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || current_block->getTerminator() != nullptr) [[unlikely]] { return false; }

    auto then_block{::llvm::BasicBlock::Create(llvm_context, "if.then", state.llvm_function)};
    auto else_block{::llvm::BasicBlock::Create(llvm_context, "if.else", state.llvm_function)};
    auto end_block{::llvm::BasicBlock::Create(llvm_context, "if.end", state.llvm_function)};
    auto end_phi{create_runtime_local_func_llvm_jit_optional_result_phi(llvm_context, end_block, block_result, "if.result")};
    if(get_runtime_block_result_count(block_result) == 1uz && end_phi == nullptr) [[unlikely]] { return false; }

    auto cond_i1{ir_builder.CreateICmpNE(condition.value, ::llvm::ConstantInt::get(condition.value->getType(), 0u))};
    ir_builder.CreateCondBr(cond_i1, then_block, else_block);
    ir_builder.SetInsertPoint(then_block);

    auto const control_stack_index{state.control_stack.size()};
    state.control_stack.push_back({.type = llvm_jit_control_context_type::if_then,
                                   .result = block_result,
                                   .end_block = end_block,
                                   .end_phi = end_phi,
                                   .else_block = else_block,
                                   .outer_stack_size = state.operand_stack.size(),
                                   .outer_branch_target_stack_size = state.branch_target_stack.size(),
                                   .is_reachable = true,
                                   .end_block_has_incoming = false});
    state.branch_target_stack.push_back(
        {.params = block_result, .block = end_block, .phi = end_phi, .control_stack_index = control_stack_index});
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_else(runtime_local_func_llvm_jit_emit_state_t& state)
{
    if(!state.valid || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }

    if(!state.control_stack.back().is_reachable && state.unreachable_control_depth != 0uz) { return true; }

    auto const current_control_stack_index{state.control_stack.size() - 1uz};
    auto& current_context{state.control_stack.back()};
    if(current_context.type != llvm_jit_control_context_type::if_then || current_context.else_block == nullptr) [[unlikely]]
    {
        return false;
    }

    auto const branch_target{llvm_jit_branch_target_t{.params = current_context.result,
                                                      .block = current_context.end_block,
                                                      .phi = current_context.end_phi,
                                                      .control_stack_index = current_control_stack_index}};
    if(current_context.is_reachable && !try_emit_runtime_local_func_llvm_jit_branch_to_target(state, branch_target)) [[unlikely]]
    {
        return false;
    }

    truncate_runtime_local_func_llvm_jit_operand_stack_to(state, current_context.outer_stack_size);
    current_context.type = llvm_jit_control_context_type::if_else;
    current_context.is_reachable = true;
    state.ir_builder->SetInsertPoint(current_context.else_block);
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_end(runtime_local_func_llvm_jit_emit_state_t& state)
{
    if(!state.valid || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }

    if(!state.control_stack.back().is_reachable && state.unreachable_control_depth != 0uz)
    {
        --state.unreachable_control_depth;
        return true;
    }

    auto const current_control_stack_index{state.control_stack.size() - 1uz};
    auto& current_context{state.control_stack.back()};

    auto const branch_target{llvm_jit_branch_target_t{.params = current_context.result,
                                                      .block = current_context.end_block,
                                                      .phi = current_context.end_phi,
                                                      .control_stack_index = current_control_stack_index}};

    if(current_context.type == llvm_jit_control_context_type::if_then)
    {
        if(get_runtime_block_result_count(current_context.result) != 0uz || current_context.else_block == nullptr) [[unlikely]]
        {
            return false;
        }
        if(current_context.is_reachable && !try_emit_runtime_local_func_llvm_jit_branch_to_target(state, branch_target)) [[unlikely]]
        {
            return false;
        }

        ::llvm::IRBuilder<> else_builder(current_context.else_block);
        else_builder.CreateBr(current_context.end_block);
        current_context.end_block_has_incoming = true;
    }
    else if(current_context.is_reachable && !try_emit_runtime_local_func_llvm_jit_branch_to_target(state, branch_target)) [[unlikely]]
    {
        return false;
    }

    auto const block_result{current_context.result};
    auto end_block{current_context.end_block};
    auto end_phi{current_context.end_phi};
    auto const outer_stack_size{current_context.outer_stack_size};
    auto const outer_branch_target_stack_size{current_context.outer_branch_target_stack_size};
    auto const continuation_reachable{current_context.end_block_has_incoming};

    state.control_stack.pop_back();
    while(state.branch_target_stack.size() > outer_branch_target_stack_size) { state.branch_target_stack.pop_back(); }
    truncate_runtime_local_func_llvm_jit_operand_stack_to(state, outer_stack_size);

    if(state.control_stack.empty()) { return true; }

    state.control_stack.back().is_reachable = continuation_reachable;
    if(!continuation_reachable)
    {
        if(end_block != nullptr && end_block->getTerminator() == nullptr)
        {
            if(end_phi != nullptr && end_phi->getNumIncomingValues() == 0u) { end_phi->eraseFromParent(); }
            ::llvm::IRBuilder<> unreachable_builder(end_block);
            unreachable_builder.CreateUnreachable();
        }
        return true;
    }
    if(end_block == nullptr) [[unlikely]] { return false; }

    state.ir_builder->SetInsertPoint(end_block);
    if(get_runtime_block_result_count(block_result) == 1uz)
    {
        if(end_phi == nullptr) [[unlikely]] { return false; }
        state.operand_stack.push_back({.type = get_runtime_block_single_result_type(block_result), .value = end_phi});
    }
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_br(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                  validation_module_traits_t::wasm_u32 label_index)
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_function == nullptr || state.ir_builder == nullptr ||
       state.control_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }

    auto branch_target{get_runtime_local_func_llvm_jit_branch_target_by_depth(state, label_index)};
    if(branch_target == nullptr || !try_emit_runtime_local_func_llvm_jit_branch_to_target(state, *branch_target)) [[unlikely]] { return false; }

    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_br_if(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                     validation_module_traits_t::wasm_u32 label_index)
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_function == nullptr || state.ir_builder == nullptr ||
       state.control_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    auto branch_target{get_runtime_local_func_llvm_jit_branch_target_by_depth(state, label_index)};
    if(branch_target == nullptr) [[unlikely]] { return false; }

    auto& llvm_context{*state.llvm_context_holder};
    auto llvm_function{state.llvm_function};
    auto& ir_builder{*state.ir_builder};

    auto const condition{state.operand_stack.back()};
    state.operand_stack.pop_back();
    if(condition.type != runtime_operand_stack_value_type::i32 || condition.value == nullptr) [[unlikely]] { return false; }

    llvm_jit_stack_value_t branch_value{};
    if(!try_get_runtime_local_func_llvm_jit_branch_value(state, branch_target->params, branch_value)) [[unlikely]] { return false; }

    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || current_block->getTerminator() != nullptr) [[unlikely]] { return false; }

    if(get_runtime_block_result_count(branch_target->params) == 1uz)
    {
        if(branch_target->phi == nullptr) [[unlikely]] { return false; }
        branch_target->phi->addIncoming(branch_value.value, current_block);
    }

    auto fallthrough_block{::llvm::BasicBlock::Create(llvm_context, "br_if.cont", llvm_function)};
    auto cond_i1{ir_builder.CreateICmpNE(condition.value, ::llvm::ConstantInt::get(condition.value->getType(), 0u))};
    ir_builder.CreateCondBr(cond_i1, branch_target->block, fallthrough_block);
    mark_runtime_local_func_llvm_jit_branch_target_has_incoming(state, *branch_target);
    ir_builder.SetInsertPoint(fallthrough_block);
    return true;
}

[[nodiscard]] inline bool
    try_emit_runtime_local_func_llvm_jit_br_table(runtime_local_func_llvm_jit_emit_state_t& state,
                                                  ::uwvm2::utils::container::vector<validation_module_traits_t::wasm_u32> const& label_indices,
                                                  validation_module_traits_t::wasm_u32 default_label_index)
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_function == nullptr || state.ir_builder == nullptr ||
       state.control_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};

    ::uwvm2::utils::container::vector<llvm_jit_branch_target_t const*> branch_targets{};
    branch_targets.reserve(label_indices.size() + 1uz);

    ::std::size_t expected_arity{};
    runtime_operand_stack_value_type expected_type{};
    bool have_expected_signature{};

    for(auto const label_index: label_indices)
    {
        auto branch_target{get_runtime_local_func_llvm_jit_branch_target_by_depth(state, label_index)};
        if(branch_target == nullptr) [[unlikely]] { return false; }

        auto const arity{get_runtime_block_result_count(branch_target->params)};
        auto const result_type{get_runtime_block_single_result_type(branch_target->params)};
        if(!have_expected_signature)
        {
            have_expected_signature = true;
            expected_arity = arity;
            expected_type = result_type;
        }
        else if(arity != expected_arity || (arity == 1uz && result_type != expected_type)) [[unlikely]]
        {
            return false;
        }

        branch_targets.push_back(branch_target);
    }

    auto default_target{get_runtime_local_func_llvm_jit_branch_target_by_depth(state, default_label_index)};
    if(default_target == nullptr) [[unlikely]] { return false; }

    auto const default_arity{get_runtime_block_result_count(default_target->params)};
    auto const default_type{get_runtime_block_single_result_type(default_target->params)};
    if(!have_expected_signature)
    {
        have_expected_signature = true;
        expected_arity = default_arity;
        expected_type = default_type;
    }
    else if(default_arity != expected_arity || (default_arity == 1uz && default_type != expected_type)) [[unlikely]]
    {
        return false;
    }

    auto const condition{state.operand_stack.back()};
    state.operand_stack.pop_back();
    if(condition.type != runtime_operand_stack_value_type::i32 || condition.value == nullptr) [[unlikely]] { return false; }

    llvm_jit_stack_value_t branch_value{};
    if(!try_get_runtime_local_func_llvm_jit_branch_value(state, default_target->params, branch_value)) [[unlikely]] { return false; }

    auto current_block{ir_builder.GetInsertBlock()};
    if(current_block == nullptr || current_block->getTerminator() != nullptr) [[unlikely]] { return false; }

    ::uwvm2::utils::container::vector<::llvm::BasicBlock*> seen_blocks{};
    seen_blocks.reserve(branch_targets.size() + 1uz);

    auto const add_unique_target_incoming{[&](llvm_jit_branch_target_t const& branch_target)
                                          {
                                              for(auto seen_block: seen_blocks)
                                              {
                                                  if(seen_block == branch_target.block) { return true; }
                                              }

                                              if(expected_arity == 1uz)
                                              {
                                                  if(branch_target.phi == nullptr) [[unlikely]] { return false; }
                                                  branch_target.phi->addIncoming(branch_value.value, current_block);
                                              }

                                              seen_blocks.push_back(branch_target.block);
                                              mark_runtime_local_func_llvm_jit_branch_target_has_incoming(state, branch_target);
                                              return true;
                                          }};

    if(!add_unique_target_incoming(*default_target)) [[unlikely]] { return false; }
    for(auto branch_target: branch_targets)
    {
        if(branch_target == nullptr || !add_unique_target_incoming(*branch_target)) [[unlikely]] { return false; }
    }

    auto switch_inst{
        ir_builder.CreateSwitch(condition.value, default_target->block, static_cast<unsigned>(label_indices.size()))};
    for(::std::size_t target_index{}; target_index != label_indices.size(); ++target_index)
    {
        switch_inst->addCase(::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), target_index),
                             branch_targets.index_unchecked(target_index)->block);
    }

    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_return(runtime_local_func_llvm_jit_emit_state_t& state)
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_function == nullptr || state.ir_builder == nullptr ||
       state.control_stack.empty() || state.branch_target_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }

    auto const& return_target{state.branch_target_stack.index_unchecked(0u)};
    if(!try_emit_runtime_local_func_llvm_jit_branch_to_target(state, return_target)) [[unlikely]] { return false; }

    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
}

[[nodiscard]] inline llvm_jit_prepared_wasm_call_operands_t
    prepare_runtime_local_func_llvm_jit_wasm_call_operands(runtime_local_func_llvm_jit_emit_state_t& state,
                                                           ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type)
{
    llvm_jit_prepared_wasm_call_operands_t prepared{};
    prepared.abi_layout = get_runtime_wasm_call_abi_layout(wasm_function_type);
    if(!prepared.abi_layout.valid || state.operand_stack.size() < prepared.abi_layout.parameter_count) [[unlikely]] { return prepared; }

    auto const parameter_begin{wasm_function_type.parameter.begin};
    auto const result_begin{wasm_function_type.result.begin};
    prepared.arguments.resize(prepared.abi_layout.parameter_count);

    for(::std::size_t parameter_index{prepared.abi_layout.parameter_count}; parameter_index != 0uz; --parameter_index)
    {
        auto const argument{state.operand_stack.back()};
        state.operand_stack.pop_back();

        auto const expected_type{static_cast<runtime_operand_stack_value_type>(parameter_begin[parameter_index - 1uz])};
        if(argument.type != expected_type || argument.value == nullptr) [[unlikely]] { return {}; }

        prepared.arguments[parameter_index - 1uz] = argument.value;
    }

    if(prepared.abi_layout.result_count == 1uz)
    {
        prepared.has_result = true;
        prepared.result_type = static_cast<runtime_operand_stack_value_type>(result_begin[0]);
    }

    prepared.valid = true;
    return prepared;
}

[[nodiscard]] inline bool push_runtime_local_func_llvm_jit_wasm_call_result(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                             llvm_jit_prepared_wasm_call_operands_t const& prepared,
                                                                             ::llvm::Value* value)
{
    if(!prepared.has_result) { return true; }
    if(value == nullptr) [[unlikely]] { return false; }

    state.operand_stack.push_back({.type = prepared.result_type, .value = value});
    return true;
}

[[nodiscard]] inline ::llvm::CallInst*
    emit_runtime_local_func_llvm_jit_direct_wasm_call_value(runtime_local_func_llvm_jit_emit_state_t& state,
                                                            ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const& runtime_module,
                                                            validation_module_traits_t::wasm_u32 func_index,
                                                            ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type,
                                                            ::llvm::ArrayRef<::llvm::Value*> call_arguments)
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.llvm_module == nullptr || state.ir_builder == nullptr) [[unlikely]]
    {
        return nullptr;
    }

    auto& llvm_context{*state.llvm_context_holder};
    auto llvm_module{state.llvm_module};
    auto& ir_builder{*state.ir_builder};

    auto callee_function{get_or_create_llvm_wasm_function_declaration(*llvm_module, llvm_context, runtime_module, func_index, wasm_function_type)};
    if(callee_function == nullptr) [[unlikely]] { return nullptr; }
    return ir_builder.CreateCall(callee_function, call_arguments);
}

template <typename BridgeFunction>
[[nodiscard]] inline ::llvm::CallInst* emit_runtime_local_func_llvm_jit_runtime_bridge_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                             BridgeFunction bridge_function,
                                                                                             ::llvm::FunctionType* bridge_function_type,
                                                                                             ::llvm::ArrayRef<::llvm::Value*> arguments)
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.ir_builder == nullptr || bridge_function_type == nullptr) [[unlikely]]
    {
        return nullptr;
    }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};

    auto bridge_pointer{get_llvm_runtime_bridge_function_pointer(llvm_context, bridge_function_type, bridge_function)};
    if(bridge_pointer == nullptr) [[unlikely]] { return nullptr; }
    return ir_builder.CreateCall(bridge_function_type, bridge_pointer, arguments);
}

template <typename EmitBridgeCallFromBuffers>
[[nodiscard]] inline llvm_jit_runtime_raw_bridge_emit_result_t
    emit_runtime_local_func_llvm_jit_runtime_raw_host_bridge_call(
        runtime_local_func_llvm_jit_emit_state_t& state,
        ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& wasm_function_type,
        ::llvm::ArrayRef<::llvm::Value*> call_arguments,
        char const* param_buffer_name,
        char const* result_buffer_name,
        EmitBridgeCallFromBuffers&& emit_bridge_call_from_buffers)
{
    if(!state.valid || state.ir_builder == nullptr) [[unlikely]] { return {}; }

    auto& ir_builder{*state.ir_builder};

    auto const raw_call_buffers{emit_runtime_raw_call_buffers(ir_builder, wasm_function_type, call_arguments, param_buffer_name, result_buffer_name)};
    if(!raw_call_buffers.valid) [[unlikely]] { return {}; }

    auto bridge_call{emit_bridge_call_from_buffers(raw_call_buffers)};
    if(bridge_call == nullptr) [[unlikely]] { return {}; }

    ::llvm::Value* result_value{};
    if(raw_call_buffers.result_buffer != nullptr)
    {
        result_value = ir_builder.CreateLoad(raw_call_buffers.result_buffer->getAllocatedType(), raw_call_buffers.result_buffer);
    }

    return llvm_jit_runtime_raw_bridge_emit_result_t{.valid = true, .bridge_call = bridge_call, .result_value = result_value};
}

template <typename I32BridgeFunction, typename I64BridgeFunction, typename F32BridgeFunction, typename F64BridgeFunction>
[[nodiscard]] inline ::llvm::CallInst* emit_runtime_local_func_llvm_jit_runtime_scalar_bridge_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                                                    runtime_operand_stack_value_type value_type,
                                                                                                    ::llvm::FunctionType* bridge_function_type,
                                                                                                    ::llvm::ArrayRef<::llvm::Value*> arguments,
                                                                                                    I32BridgeFunction i32_bridge_function,
                                                                                                    I64BridgeFunction i64_bridge_function,
                                                                                                    F32BridgeFunction f32_bridge_function,
                                                                                                    F64BridgeFunction f64_bridge_function)
{
    switch(value_type)
    {
        case runtime_operand_stack_value_type::i32:
            return emit_runtime_local_func_llvm_jit_runtime_bridge_call(state, i32_bridge_function, bridge_function_type, arguments);
        case runtime_operand_stack_value_type::i64:
            return emit_runtime_local_func_llvm_jit_runtime_bridge_call(state, i64_bridge_function, bridge_function_type, arguments);
        case runtime_operand_stack_value_type::f32:
            return emit_runtime_local_func_llvm_jit_runtime_bridge_call(state, f32_bridge_function, bridge_function_type, arguments);
        case runtime_operand_stack_value_type::f64:
            return emit_runtime_local_func_llvm_jit_runtime_bridge_call(state, f64_bridge_function, bridge_function_type, arguments);
        [[unlikely]] default:
            return nullptr;
    }
}

[[nodiscard]] inline ::llvm::CallInst*
    emit_runtime_local_func_llvm_jit_local_imported_global_get_bridge_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                           runtime_global_access_info_t const& global_access_info,
                                                                           ::llvm::Type* llvm_global_type)
{
    if(!state.valid || state.llvm_context_holder == nullptr || global_access_info.local_imported_module_ptr == nullptr || llvm_global_type == nullptr)
        [[unlikely]]
    {
        return nullptr;
    }

    auto& llvm_context{*state.llvm_context_holder};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto bridge_function_type{::llvm::FunctionType::get(llvm_global_type, {llvm_intptr_type, llvm_intptr_type}, false)};
    auto const bridge_arguments{
        ::llvm::ArrayRef<::llvm::Value*>{
                                         ::llvm::ConstantInt::get(llvm_intptr_type, reinterpret_cast<::std::uintptr_t>(global_access_info.local_imported_module_ptr)),
                                         ::llvm::ConstantInt::get(llvm_intptr_type, global_access_info.local_imported_global_index)}
    };

    return emit_runtime_local_func_llvm_jit_runtime_scalar_bridge_call(state,
                                                                       global_access_info.value_type,
                                                                       bridge_function_type,
                                                                       bridge_arguments,
                                                                       llvm_jit_local_imported_global_get_bridge<runtime_wasm_i32>,
                                                                       llvm_jit_local_imported_global_get_bridge<runtime_wasm_i64>,
                                                                       llvm_jit_local_imported_global_get_bridge<runtime_wasm_f32>,
                                                                       llvm_jit_local_imported_global_get_bridge<runtime_wasm_f64>);
}

[[nodiscard]] inline ::llvm::CallInst*
    emit_runtime_local_func_llvm_jit_local_imported_global_set_bridge_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                           runtime_global_access_info_t const& global_access_info,
                                                                           ::llvm::Type* llvm_value_type,
                                                                           ::llvm::Value* value)
{
    if(!state.valid || state.llvm_context_holder == nullptr || global_access_info.local_imported_module_ptr == nullptr || llvm_value_type == nullptr ||
       value == nullptr)
        [[unlikely]]
    {
        return nullptr;
    }

    auto& llvm_context{*state.llvm_context_holder};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
    auto bridge_function_type{
        ::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context), {llvm_intptr_type, llvm_intptr_type, llvm_value_type}, false)};
    auto const bridge_arguments{
        ::llvm::ArrayRef<::llvm::Value*>{
                                         ::llvm::ConstantInt::get(llvm_intptr_type, reinterpret_cast<::std::uintptr_t>(global_access_info.local_imported_module_ptr)),
                                         ::llvm::ConstantInt::get(llvm_intptr_type, global_access_info.local_imported_global_index),
                                         value}
    };

    return emit_runtime_local_func_llvm_jit_runtime_scalar_bridge_call(state,
                                                                       global_access_info.value_type,
                                                                       bridge_function_type,
                                                                       bridge_arguments,
                                                                       llvm_jit_local_imported_global_set_bridge<runtime_wasm_i32>,
                                                                       llvm_jit_local_imported_global_set_bridge<runtime_wasm_i64>,
                                                                       llvm_jit_local_imported_global_set_bridge<runtime_wasm_f32>,
                                                                       llvm_jit_local_imported_global_set_bridge<runtime_wasm_f64>);
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_drop(runtime_local_func_llvm_jit_emit_state_t& state)
{
    if(!state.valid || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    state.operand_stack.pop_back();
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_select(runtime_local_func_llvm_jit_emit_state_t& state)
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.size() < 3uz) [[unlikely]] { return false; }

    auto const selector{state.operand_stack.back()};
    state.operand_stack.pop_back();
    auto const false_value{state.operand_stack.back()};
    state.operand_stack.pop_back();
    auto const true_value{state.operand_stack.back()};
    state.operand_stack.pop_back();

    if(selector.type != runtime_operand_stack_value_type::i32 || true_value.type != false_value.type || selector.value == nullptr ||
       true_value.value == nullptr || false_value.value == nullptr)
        [[unlikely]]
    {
        return false;
    }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};
    auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
    if(selector.value->getType() != llvm_i32_type || true_value.value->getType() != false_value.value->getType()) [[unlikely]] { return false; }

    auto selector_is_nonzero{ir_builder.CreateICmpNE(selector.value, ::llvm::ConstantInt::get(llvm_i32_type, 0u))};
    auto selected_value{ir_builder.CreateSelect(selector_is_nonzero, true_value.value, false_value.value)};
    if(selected_value == nullptr) [[unlikely]] { return false; }

    state.operand_stack.push_back({.type = true_value.type, .value = selected_value});
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_local_get(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                         validation_module_traits_t::wasm_u32 local_index)
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }

    auto const local_index_uz{static_cast<::std::size_t>(local_index)};
    if(local_index_uz >= state.local_types.size() || local_index_uz >= state.local_pointers.size()) [[unlikely]] { return false; }

    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};
    auto const local_type{state.local_types[local_index_uz]};
    auto llvm_local_type{get_llvm_type_from_wasm_value_type(llvm_context, local_type)};
    auto local_pointer{state.local_pointers[local_index_uz]};
    if(llvm_local_type == nullptr || local_pointer == nullptr) [[unlikely]] { return false; }

    auto loaded_value{ir_builder.CreateLoad(llvm_local_type, local_pointer, "local.get")};
    if(loaded_value == nullptr) [[unlikely]] { return false; }

    state.operand_stack.push_back({.type = local_type, .value = loaded_value});
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_local_set(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                         validation_module_traits_t::wasm_u32 local_index)
{
    if(!state.valid || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    auto const local_index_uz{static_cast<::std::size_t>(local_index)};
    if(local_index_uz >= state.local_types.size() || local_index_uz >= state.local_pointers.size()) [[unlikely]] { return false; }

    auto const value{state.operand_stack.back()};
    state.operand_stack.pop_back();
    auto const local_type{state.local_types[local_index_uz]};
    auto local_pointer{state.local_pointers[local_index_uz]};
    if(value.type != local_type || value.value == nullptr || local_pointer == nullptr) [[unlikely]] { return false; }

    state.ir_builder->CreateStore(value.value, local_pointer);
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_local_tee(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                         validation_module_traits_t::wasm_u32 local_index)
{
    if(!state.valid || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    auto const local_index_uz{static_cast<::std::size_t>(local_index)};
    if(local_index_uz >= state.local_types.size() || local_index_uz >= state.local_pointers.size()) [[unlikely]] { return false; }

    auto const value{state.operand_stack.back()};
    auto const local_type{state.local_types[local_index_uz]};
    auto local_pointer{state.local_pointers[local_index_uz]};
    if(value.type != local_type || value.value == nullptr || local_pointer == nullptr) [[unlikely]] { return false; }

    state.ir_builder->CreateStore(value.value, local_pointer);
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_global_get(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                          validation_module_traits_t::wasm_u32 global_index)
{
    if(!state.valid || state.local_func_storage_ptr == nullptr || state.llvm_context_holder == nullptr || state.ir_builder == nullptr ||
       state.control_stack.empty())
        [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }

    auto runtime_module_ptr{state.local_func_storage_ptr->runtime_module_ptr};
    if(runtime_module_ptr == nullptr) [[unlikely]] { return false; }

    auto const global_access_info{resolve_runtime_global_access_info(*runtime_module_ptr, global_index)};
    auto& llvm_context{*state.llvm_context_holder};
    auto& ir_builder{*state.ir_builder};
    auto llvm_global_type{get_llvm_type_from_wasm_value_type(llvm_context, global_access_info.value_type)};
    if(llvm_global_type == nullptr) [[unlikely]] { return false; }

    if(global_access_info.storage_ptr != nullptr)
    {
        auto global_pointer{get_llvm_global_storage_pointer(llvm_context, global_access_info.storage_ptr, global_access_info.value_type)};
        if(global_pointer == nullptr) [[unlikely]] { return false; }

        auto loaded_value{ir_builder.CreateLoad(llvm_global_type, global_pointer, "global.get")};
        if(loaded_value == nullptr) [[unlikely]] { return false; }

        state.operand_stack.push_back({.type = global_access_info.value_type, .value = loaded_value});
        return true;
    }

    auto bridge_call{emit_runtime_local_func_llvm_jit_local_imported_global_get_bridge_call(state, global_access_info, llvm_global_type)};
    if(bridge_call == nullptr) [[unlikely]] { return false; }

    state.operand_stack.push_back({.type = global_access_info.value_type, .value = bridge_call});
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_global_set(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                          validation_module_traits_t::wasm_u32 global_index)
{
    if(!state.valid || state.local_func_storage_ptr == nullptr || state.llvm_context_holder == nullptr || state.ir_builder == nullptr ||
       state.control_stack.empty())
        [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    auto runtime_module_ptr{state.local_func_storage_ptr->runtime_module_ptr};
    if(runtime_module_ptr == nullptr) [[unlikely]] { return false; }

    auto const global_access_info{resolve_runtime_global_access_info(*runtime_module_ptr, global_index)};
    if(!global_access_info.is_mutable) [[unlikely]] { return false; }

    auto const value{state.operand_stack.back()};
    state.operand_stack.pop_back();
    if(value.type != global_access_info.value_type || value.value == nullptr) [[unlikely]] { return false; }

    auto& llvm_context{*state.llvm_context_holder};
    if(global_access_info.storage_ptr != nullptr)
    {
        auto global_pointer{get_llvm_global_storage_pointer(llvm_context, global_access_info.storage_ptr, global_access_info.value_type)};
        if(global_pointer == nullptr) [[unlikely]] { return false; }

        state.ir_builder->CreateStore(value.value, global_pointer);
        return true;
    }

    auto llvm_value_type{get_llvm_type_from_wasm_value_type(llvm_context, global_access_info.value_type)};
    if(llvm_value_type == nullptr) [[unlikely]] { return false; }

    return emit_runtime_local_func_llvm_jit_local_imported_global_set_bridge_call(state, global_access_info, llvm_value_type, value.value) != nullptr;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_call(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                    validation_module_traits_t::wasm_u32 func_index)
{
    if(!state.valid || state.local_func_storage_ptr == nullptr || state.llvm_context_holder == nullptr || state.llvm_module == nullptr ||
       state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }

    auto const& local_func_storage{*state.local_func_storage_ptr};
    auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
    if(runtime_module_ptr == nullptr) [[unlikely]] { return false; }

    auto callee_type_ptr{resolve_runtime_callee_function_type(*runtime_module_ptr, func_index)};
    if(callee_type_ptr == nullptr) [[unlikely]] { return false; }

    auto const prepared_call{prepare_runtime_local_func_llvm_jit_wasm_call_operands(state, *callee_type_ptr)};
    if(!prepared_call.valid) [[unlikely]] { return false; }

    auto const abi_layout{prepared_call.abi_layout};
    auto const import_func_count{runtime_module_ptr->imported_function_vec_storage.size()};
    if(static_cast<::std::size_t>(func_index) < import_func_count)
    {
        auto const callee_resolution{resolve_runtime_direct_callee(*runtime_module_ptr, func_index)};
        if(!callee_resolution.state_valid) [[unlikely]] { return false; }

        if(callee_resolution.direct_callable && callee_resolution.function_type_ptr != nullptr &&
           runtime_wasm_function_types_equal(*callee_resolution.function_type_ptr, *callee_type_ptr))
        {
            auto call_value{emit_runtime_local_func_llvm_jit_direct_wasm_call_value(
                state, *runtime_module_ptr, callee_resolution.func_index, *callee_resolution.function_type_ptr, prepared_call.arguments)};
            return push_runtime_local_func_llvm_jit_wasm_call_result(state, prepared_call, call_value);
        }

        auto& llvm_context{*state.llvm_context_holder};
        auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
        auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};

        auto const raw_bridge_result{emit_runtime_local_func_llvm_jit_runtime_raw_host_bridge_call(
            state,
            *callee_type_ptr,
            {prepared_call.arguments.data(), prepared_call.arguments.size()},
            "call.params",
            "call.result.buf",
            [&](llvm_jit_runtime_raw_call_buffers_t const& raw_call_buffers) -> ::llvm::CallInst*
            {
                auto bridge_function_type{get_llvm_runtime_raw_call_bridge_function_type(llvm_context)};
                return emit_runtime_local_func_llvm_jit_runtime_bridge_call(
                    state,
                    ::uwvm2::runtime::lib::llvm_jit_call_raw_host_api,
                    bridge_function_type,
                    {::llvm::ConstantInt::get(llvm_intptr_type, reinterpret_cast<::std::uintptr_t>(runtime_module_ptr)),
                     ::llvm::ConstantInt::get(llvm_i32_type, func_index),
                     raw_call_buffers.result_buffer_address,
                     ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.result_bytes),
                     raw_call_buffers.param_buffer_address,
                     ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.parameter_bytes)});
            })};
        if(!raw_bridge_result.valid) [[unlikely]] { return false; }

        return push_runtime_local_func_llvm_jit_wasm_call_result(state, prepared_call, raw_bridge_result.result_value);
    }

    auto call_value{emit_runtime_local_func_llvm_jit_direct_wasm_call_value(state, *runtime_module_ptr, func_index, *callee_type_ptr, prepared_call.arguments)};
    return push_runtime_local_func_llvm_jit_wasm_call_result(state, prepared_call, call_value);
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_call_indirect(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                             validation_module_traits_t::wasm_u32 type_index,
                                                                             validation_module_traits_t::wasm_u32 table_index)
{
    if(!state.valid || state.local_func_storage_ptr == nullptr || state.llvm_context_holder == nullptr || state.llvm_module == nullptr ||
       state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]]
    {
        return false;
    }

    if(!state.control_stack.back().is_reachable) { return true; }

    auto const& local_func_storage{*state.local_func_storage_ptr};
    auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
    if(runtime_module_ptr == nullptr) [[unlikely]] { return false; }

    auto const all_table_count{
        runtime_module_ptr->imported_table_vec_storage.size() + runtime_module_ptr->local_defined_table_vec_storage.size()};
    if(static_cast<::std::size_t>(table_index) >= all_table_count) [[unlikely]] { return false; }

    auto callee_type_ptr{resolve_runtime_type_section_function_type(*runtime_module_ptr, type_index)};
    if(callee_type_ptr == nullptr) [[unlikely]] { return false; }

    auto const abi_layout{get_runtime_wasm_call_abi_layout(*callee_type_ptr)};
    constexpr auto max_operand_stack_requirement{::std::numeric_limits<::std::size_t>::max()};
    if(!abi_layout.valid || abi_layout.parameter_count == max_operand_stack_requirement ||
       state.operand_stack.size() < abi_layout.parameter_count + 1uz) [[unlikely]]
    {
        return false;
    }

    auto const selector{state.operand_stack.back()};
    state.operand_stack.pop_back();
    if(selector.type != runtime_operand_stack_value_type::i32 || selector.value == nullptr) [[unlikely]] { return false; }

    auto const prepared_call{prepare_runtime_local_func_llvm_jit_wasm_call_operands(state, *callee_type_ptr)};
    if(!prepared_call.valid) [[unlikely]] { return false; }

    auto& llvm_context{*state.llvm_context_holder};
    auto llvm_module{state.llvm_module};
    auto& ir_builder{*state.ir_builder};
    auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
    auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};

    auto const expected_type_id{resolve_runtime_canonical_type_id(*runtime_module_ptr, type_index)};
    if(expected_type_id == invalid_runtime_canonical_type_id()) [[unlikely]] { return false; }

    auto table_view_begin{runtime_module_ptr->llvm_jit_call_indirect_table_views.data()};
    if(table_view_begin == nullptr) [[unlikely]] { return false; }

    auto const raw_bridge_result{emit_runtime_local_func_llvm_jit_runtime_raw_host_bridge_call(
        state,
        *callee_type_ptr,
        {prepared_call.arguments.data(), prepared_call.arguments.size()},
        "call_indirect.params",
        "call_indirect.result.buf",
        [&](llvm_jit_runtime_raw_call_buffers_t const& raw_call_buffers) -> ::llvm::CallInst*
        {
            auto raw_entry_function_type{get_llvm_runtime_raw_call_target_entry_function_type(llvm_context)};
            auto raw_target_struct_type{get_llvm_runtime_raw_call_target_struct_type(llvm_context)};
            auto table_view_struct_type{get_llvm_runtime_call_indirect_table_view_struct_type(llvm_context)};
            if(raw_entry_function_type == nullptr || raw_target_struct_type == nullptr || table_view_struct_type == nullptr) [[unlikely]]
            {
                return nullptr;
            }

            auto table_view_base_ptr{
                get_llvm_host_pointer_constant(reinterpret_cast<::std::uintptr_t>(table_view_begin), get_llvm_pointer_type(table_view_struct_type))};
            if(table_view_base_ptr == nullptr) [[unlikely]] { return nullptr; }

            auto selector_index{ir_builder.CreateZExt(selector.value, llvm_intptr_type, "call_indirect.selector.index")};
            auto table_view_ptr{ir_builder.CreateInBoundsGEP(table_view_struct_type,
                                                              table_view_base_ptr,
                                                              {::llvm::ConstantInt::get(llvm_intptr_type, table_index)},
                                                              "call_indirect.table_view.ptr")};
            auto table_data_address_ptr{ir_builder.CreateStructGEP(table_view_struct_type, table_view_ptr, 0u, "call_indirect.table.data.addr.ptr")};
            auto table_size_ptr{ir_builder.CreateStructGEP(table_view_struct_type, table_view_ptr, 1u, "call_indirect.table.size.ptr")};
            auto table_data_address{ir_builder.CreateLoad(llvm_intptr_type, table_data_address_ptr, "call_indirect.table.data.addr")};
            auto table_size{ir_builder.CreateLoad(llvm_intptr_type, table_size_ptr, "call_indirect.table.size")};

            emit_llvm_conditional_trap(*llvm_module, ir_builder, ir_builder.CreateICmpUGE(selector_index, table_size));
            emit_llvm_conditional_trap(*llvm_module,
                                       ir_builder,
                                       ir_builder.CreateICmpEQ(table_data_address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)));

            auto target_base_ptr{
                ir_builder.CreateIntToPtr(table_data_address, get_llvm_pointer_type(raw_target_struct_type), "call_indirect.target.base.ptr")};
            auto target_ptr{
                ir_builder.CreateInBoundsGEP(raw_target_struct_type, target_base_ptr, selector_index, "call_indirect.target.ptr")};
            auto entry_address_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_ptr, 0u, "call_indirect.entry.addr.ptr")};
            auto context_address_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_ptr, 1u, "call_indirect.context.addr.ptr")};
            auto encoded_type_id_ptr{ir_builder.CreateStructGEP(raw_target_struct_type, target_ptr, 2u, "call_indirect.type.id.ptr")};
            auto entry_address{ir_builder.CreateLoad(llvm_intptr_type, entry_address_ptr, "call_indirect.entry.addr")};
            auto context_address{ir_builder.CreateLoad(llvm_intptr_type, context_address_ptr, "call_indirect.context.addr")};
            auto encoded_type_id{ir_builder.CreateLoad(llvm_i32_type, encoded_type_id_ptr, "call_indirect.type.id")};

            emit_llvm_conditional_trap(*llvm_module, ir_builder, ir_builder.CreateICmpEQ(entry_address, ::llvm::ConstantInt::get(llvm_intptr_type, 0u)));
            emit_llvm_conditional_trap(
                *llvm_module, ir_builder, ir_builder.CreateICmpNE(encoded_type_id, ::llvm::ConstantInt::get(llvm_i32_type, expected_type_id)));

            auto raw_entry_function_ptr{
                ir_builder.CreateIntToPtr(entry_address, get_llvm_pointer_type(raw_entry_function_type), "call_indirect.entry.ptr")};
            return ir_builder.CreateCall(raw_entry_function_type,
                                         raw_entry_function_ptr,
                                         {context_address,
                                          raw_call_buffers.result_buffer_address,
                                          ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.result_bytes),
                                          raw_call_buffers.param_buffer_address,
                                          ::llvm::ConstantInt::get(llvm_intptr_type, abi_layout.parameter_bytes)});
        })};
    if(!raw_bridge_result.valid) [[unlikely]] { return false; }

    return push_runtime_local_func_llvm_jit_wasm_call_result(state, prepared_call, raw_bridge_result.result_value);
}

template <typename CreateValue>
[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_constant(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                        runtime_operand_stack_value_type result_type,
                                                                        CreateValue&& create_value)
{
    if(!state.valid || state.llvm_context_holder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }

    auto value{create_value(*state.llvm_context_holder)};
    if(value == nullptr) [[unlikely]] { return false; }

    state.operand_stack.push_back({.type = result_type, .value = value});
    return true;
}

template <typename CreateValue>
[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_unary(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                     runtime_operand_stack_value_type expected_type,
                                                                     runtime_operand_stack_value_type result_type,
                                                                     CreateValue&& create_value)
{
    if(!state.valid || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.empty()) [[unlikely]] { return false; }

    auto const operand{state.operand_stack.back()};
    state.operand_stack.pop_back();

    if(operand.type != expected_type || operand.value == nullptr) [[unlikely]] { return false; }

    auto value{create_value(*state.ir_builder, operand)};
    if(value == nullptr) [[unlikely]] { return false; }

    state.operand_stack.push_back({.type = result_type, .value = value});
    return true;
}

template <typename CreateValue>
[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_binary(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                      runtime_operand_stack_value_type expected_type,
                                                                      runtime_operand_stack_value_type result_type,
                                                                      CreateValue&& create_value)
{
    if(!state.valid || state.ir_builder == nullptr || state.control_stack.empty()) [[unlikely]] { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.operand_stack.size() < 2uz) [[unlikely]] { return false; }

    auto const right{state.operand_stack.back()};
    state.operand_stack.pop_back();
    auto const left{state.operand_stack.back()};
    state.operand_stack.pop_back();

    if(left.type != expected_type || right.type != expected_type || left.value == nullptr || right.value == nullptr) [[unlikely]]
    {
        return false;
    }

    auto value{create_value(*state.ir_builder, left, right)};
    if(value == nullptr) [[unlikely]] { return false; }

    state.operand_stack.push_back({.type = result_type, .value = value});
    return true;
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_instruction(runtime_local_func_llvm_jit_emit_state_t& state,
                                                                           ::std::byte const* instruction_begin,
                                                                           ::std::byte const* instruction_end)
{
    if(!state.valid || state.local_func_storage_ptr == nullptr || state.llvm_context_holder == nullptr || state.llvm_module == nullptr ||
       state.llvm_function == nullptr || state.ir_builder == nullptr || instruction_begin == nullptr || instruction_end == nullptr ||
       instruction_begin > instruction_end) [[unlikely]]
    {
        return false;
    }

    auto const& local_func_storage{*state.local_func_storage_ptr};
    [[maybe_unused]] auto const& local_types{state.local_types};
    auto& llvm_context{*state.llvm_context_holder};
    auto llvm_module{state.llvm_module};
    [[maybe_unused]] auto llvm_function{state.llvm_function};
    auto& ir_builder{*state.ir_builder};
    [[maybe_unused]] auto const& local_pointers{state.local_pointers};
    auto& memory0_access_info{state.memory0_access_info};
    auto& memory0_access_info_resolved{state.memory0_access_info_resolved};
    auto& operand_stack{state.operand_stack};
    auto& control_stack{state.control_stack};
    auto& unreachable_control_depth{state.unreachable_control_depth};
    auto code_curr{instruction_begin};
    auto const code_end{instruction_end};
    constexpr bool result{};

    auto const push_operand{[&](runtime_operand_stack_value_type type, ::llvm::Value* value) { operand_stack.push_back({.type = type, .value = value}); }};

    auto const ensure_memory0_access_info{[&]()
                                          {
                                              if(memory0_access_info_resolved)
                                              {
                                                  return memory0_access_info.memory_p != nullptr || memory0_access_info.local_imported_module_ptr != nullptr;
                                              }

                                              auto runtime_module_ptr{local_func_storage.runtime_module_ptr};
                                              if(runtime_module_ptr == nullptr) [[unlikely]]
                                              {
                                                  memory0_access_info_resolved = true;
                                                  return false;
                                              }

                                              memory0_access_info = resolve_runtime_memory_access_info(*runtime_module_ptr, 0u);
                                              memory0_access_info_resolved = true;
                                              return memory0_access_info.memory_p != nullptr || memory0_access_info.local_imported_module_ptr != nullptr;
                                          }};

    auto const emit_runtime_bridge_call{
        [&](auto bridge_function, ::llvm::FunctionType* bridge_function_type, ::llvm::ArrayRef<::llvm::Value*> arguments) -> ::llvm::CallInst*
        {
            auto bridge_pointer{get_llvm_runtime_bridge_function_pointer(llvm_context, bridge_function_type, bridge_function)};
            if(bridge_pointer == nullptr) [[unlikely]] { return nullptr; }
            return ir_builder.CreateCall(bridge_function_type, bridge_pointer, arguments);
        }};

    auto const emit_runtime_scalar_bridge_call{[&](runtime_operand_stack_value_type value_type,
                                                   ::llvm::FunctionType* bridge_function_type,
                                                   ::llvm::ArrayRef<::llvm::Value*> bridge_arguments,
                                                   auto i32_bridge_function,
                                                   auto i64_bridge_function,
                                                   auto f32_bridge_function,
                                                   auto f64_bridge_function) -> ::llvm::CallInst*
                                               {
                                                   switch(value_type)
                                                   {
                                                       case runtime_operand_stack_value_type::i32:
                                                           return emit_runtime_bridge_call(i32_bridge_function, bridge_function_type, bridge_arguments);
                                                       case runtime_operand_stack_value_type::i64:
                                                           return emit_runtime_bridge_call(i64_bridge_function, bridge_function_type, bridge_arguments);
                                                       case runtime_operand_stack_value_type::f32:
                                                           return emit_runtime_bridge_call(f32_bridge_function, bridge_function_type, bridge_arguments);
                                                       case runtime_operand_stack_value_type::f64:
                                                           return emit_runtime_bridge_call(f64_bridge_function, bridge_function_type, bridge_arguments);
                                                       [[unlikely]] default:
                                                           return nullptr;
                                                   }
                                               }};

    [[maybe_unused]] auto const emit_local_imported_global_get_bridge_call{
        [&](runtime_global_access_info_t const& global_access_info, ::llvm::Type* llvm_global_type) -> ::llvm::CallInst*
        {
            if(global_access_info.local_imported_module_ptr == nullptr || llvm_global_type == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{::llvm::FunctionType::get(llvm_global_type, {llvm_intptr_type, llvm_intptr_type}, false)};
            auto const bridge_arguments{
                ::llvm::ArrayRef<::llvm::Value*>{
                                                 ::llvm::ConstantInt::get(llvm_intptr_type, reinterpret_cast<::std::uintptr_t>(global_access_info.local_imported_module_ptr)),
                                                 ::llvm::ConstantInt::get(llvm_intptr_type, global_access_info.local_imported_global_index)}
            };

            return emit_runtime_scalar_bridge_call(global_access_info.value_type,
                                                   bridge_function_type,
                                                   bridge_arguments,
                                                   llvm_jit_local_imported_global_get_bridge<runtime_wasm_i32>,
                                                   llvm_jit_local_imported_global_get_bridge<runtime_wasm_i64>,
                                                   llvm_jit_local_imported_global_get_bridge<runtime_wasm_f32>,
                                                   llvm_jit_local_imported_global_get_bridge<runtime_wasm_f64>);
        }};

    [[maybe_unused]] auto const emit_local_imported_global_set_bridge_call{
        [&](runtime_global_access_info_t const& global_access_info, ::llvm::Type* llvm_value_type, ::llvm::Value* value) -> ::llvm::CallInst*
        {
            if(global_access_info.local_imported_module_ptr == nullptr || llvm_value_type == nullptr || value == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{
                ::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context), {llvm_intptr_type, llvm_intptr_type, llvm_value_type}, false)};
            auto const bridge_arguments{
                ::llvm::ArrayRef<::llvm::Value*>{
                                                 ::llvm::ConstantInt::get(llvm_intptr_type, reinterpret_cast<::std::uintptr_t>(global_access_info.local_imported_module_ptr)),
                                                 ::llvm::ConstantInt::get(llvm_intptr_type, global_access_info.local_imported_global_index),
                                                 value}
            };

            return emit_runtime_scalar_bridge_call(global_access_info.value_type,
                                                   bridge_function_type,
                                                   bridge_arguments,
                                                   llvm_jit_local_imported_global_set_bridge<runtime_wasm_i32>,
                                                   llvm_jit_local_imported_global_set_bridge<runtime_wasm_i64>,
                                                   llvm_jit_local_imported_global_set_bridge<runtime_wasm_f32>,
                                                   llvm_jit_local_imported_global_set_bridge<runtime_wasm_f64>);
        }};

    auto const get_llvm_host_pointer_constant{[&](::std::uintptr_t host_address, ::llvm::Type* pointer_type) -> ::llvm::Constant*
                                              {
                                                  if(pointer_type == nullptr) [[unlikely]] { return nullptr; }

                                                  auto llvm_intptr_type{
                                                      ::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
                                                  auto host_address_value{::llvm::ConstantInt::get(llvm_intptr_type, host_address)};
                                                  return ::llvm::ConstantExpr::getIntToPtr(host_address_value, pointer_type);
                                              }};

    auto const get_llvm_memory_access_alignment{[](::std::size_t access_size, validation_module_traits_t::wasm_u32 memarg_align) -> ::llvm::Align
                                                {
                                                    ::std::size_t natural_alignment{access_size == 0uz ? 1uz : access_size};
                                                    ::std::size_t requested_alignment{1uz};

                                                    if(memarg_align < static_cast<validation_module_traits_t::wasm_u32>(sizeof(::std::size_t) * 8u))
                                                    {
                                                        requested_alignment <<= static_cast<unsigned>(memarg_align);
                                                    }
                                                    else
                                                    {
                                                        requested_alignment = natural_alignment;
                                                    }

                                                    if(requested_alignment == 0uz) { requested_alignment = natural_alignment; }
                                                    if(requested_alignment > natural_alignment) { requested_alignment = natural_alignment; }
                                                    return ::llvm::Align(static_cast<std::uint64_t>(requested_alignment == 0uz ? 1uz : requested_alignment));
                                                }};

    auto const emit_direct_memory_byte_length_value{
        [&]() -> ::llvm::Value*
        {
            if(!ensure_memory0_access_info()) [[unlikely]] { return nullptr; }
            if(memory0_access_info.memory_p == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};

            if constexpr(runtime_native_memory_t::can_mmap)
            {
                auto memory_length_ptr{get_llvm_host_pointer_constant(reinterpret_cast<::std::uintptr_t>(memory0_access_info.stable_memory_length_p),
                                                                       get_llvm_pointer_type(llvm_intptr_type))};
                if(memory_length_ptr == nullptr) [[unlikely]] { return nullptr; }

                auto load_inst{ir_builder.CreateLoad(llvm_intptr_type, memory_length_ptr, "memory.length")};
                load_inst->setAtomic(::llvm::AtomicOrdering::Acquire);
                return load_inst;
            }
            else
            {
                if(memory0_access_info.stable_memory_length_value_p == nullptr) [[unlikely]] { return nullptr; }
                auto memory_length_ptr{get_llvm_host_pointer_constant(reinterpret_cast<::std::uintptr_t>(memory0_access_info.stable_memory_length_value_p),
                                                                       get_llvm_pointer_type(llvm_intptr_type))};
                if(memory_length_ptr == nullptr) [[unlikely]] { return nullptr; }
                return ir_builder.CreateLoad(llvm_intptr_type, memory_length_ptr, "memory.length");
            }
        }};

    auto const emit_direct_memory_page_count_from_byte_length{
        [&](::llvm::Value* memory_length_load, ::std::size_t page_size_bytes) -> ::llvm::Value*
        {
            if(memory_length_load == nullptr || page_size_bytes == 0uz) [[unlikely]] { return nullptr; }

            auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            ::llvm::Value* page_count{};
            if(::std::has_single_bit(page_size_bytes))
            {
                page_count = ir_builder.CreateLShr(memory_length_load, ::llvm::ConstantInt::get(llvm_intptr_type, ::std::countr_zero(page_size_bytes)));
            }
            else
            {
                page_count = ir_builder.CreateUDiv(memory_length_load, ::llvm::ConstantInt::get(llvm_intptr_type, page_size_bytes), "memory.page_count");
            }
            return ir_builder.CreateTrunc(page_count, llvm_i32_type);
        }};

    auto const emit_direct_memory_page_count_value{[&]() -> ::llvm::Value*
                                                   {
                                                       auto memory_length_load{emit_direct_memory_byte_length_value()};
                                                       if(memory_length_load == nullptr) [[unlikely]] { return nullptr; }
                                                       return emit_direct_memory_page_count_from_byte_length(memory_length_load,
                                                                                                             static_cast<::std::size_t>(1uz)
                                                                                                                 << memory0_access_info.custom_page_size_log2);
                                                   }};

    auto const emit_direct_mmap_memory_byte_pointer{
        [&](validation_module_traits_t::wasm_u32 static_offset, ::std::size_t access_size, ::llvm::Value* address_value) -> ::llvm::Value*
        {
            if constexpr(!(runtime_native_memory_t::can_mmap && ::std::endian::native == ::std::endian::little))
            {
                static_cast<void>(static_offset);
                static_cast<void>(access_size);
                static_cast<void>(address_value);
                return nullptr;
            }
            else
            {
                if(!ensure_memory0_access_info() || address_value == nullptr) [[unlikely]] { return nullptr; }

                auto llvm_i8_type{::llvm::Type::getInt8Ty(llvm_context)};
                auto llvm_i8_ptr_type{get_llvm_pointer_type(llvm_i8_type)};
                auto llvm_i64_type{::llvm::Type::getInt64Ty(llvm_context)};
                auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};

                auto effective_offset{ir_builder.CreateAdd(ir_builder.CreateSExt(address_value, llvm_i64_type),
                                                            ::llvm::ConstantInt::get(llvm_i64_type, static_cast<::std::uint_least32_t>(static_offset)))};
                auto stable_memory_begin{
                    get_llvm_host_pointer_constant(reinterpret_cast<::std::uintptr_t>(memory0_access_info.stable_memory_begin), llvm_i8_ptr_type)};
                if(stable_memory_begin == nullptr) [[unlikely]] { return nullptr; }

                if(memory0_access_info.mmap_requires_dynamic_bounds)
                {
                    auto memory_length_load{emit_direct_memory_byte_length_value()};
                    if(memory_length_load == nullptr) [[unlikely]] { return nullptr; }

                    auto effective_negative{ir_builder.CreateICmpSLT(effective_offset, ::llvm::ConstantInt::getSigned(llvm_i64_type, 0))};
                    auto effective_too_large{ir_builder.CreateICmpUGT(effective_offset, ::llvm::ConstantInt::get(llvm_i64_type, 0xffffffffull))};
                    auto memory_length_i64{ir_builder.CreateZExt(memory_length_load, llvm_i64_type)};
                    auto access_size_i64{::llvm::ConstantInt::get(llvm_i64_type, access_size)};
                    auto memory_too_small{ir_builder.CreateICmpULT(memory_length_i64, access_size_i64)};
                    auto max_access_offset{ir_builder.CreateSub(memory_length_i64, access_size_i64)};
                    auto access_oob{ir_builder.CreateICmpUGT(effective_offset, max_access_offset)};

                    emit_llvm_conditional_trap(
                        *llvm_module,
                        ir_builder,
                        ir_builder.CreateOr(ir_builder.CreateOr(effective_negative, effective_too_large), ir_builder.CreateOr(memory_too_small, access_oob)));
                }
                else if(memory0_access_info.mmap_uses_partial_protection)
                {
                    auto effective_negative{ir_builder.CreateICmpSLT(effective_offset, ::llvm::ConstantInt::getSigned(llvm_i64_type, 0))};
                    ::llvm::Value* partial_limit_escape{};
                    partial_limit_escape =
                        ir_builder.CreateICmpUGE(effective_offset,
                                                 ::llvm::ConstantInt::get(llvm_i64_type, get_runtime_partial_protection_limit_escape_offset()));

                    auto needs_dynamic_bounds_check{ir_builder.CreateOr(effective_negative, partial_limit_escape)};
                    auto current_block{ir_builder.GetInsertBlock()};
                    auto current_function{current_block == nullptr ? nullptr : current_block->getParent()};
                    if(current_function == nullptr) [[unlikely]] { return nullptr; }

                    auto partial_check_block{::llvm::BasicBlock::Create(llvm_context, "memory.partial.check", current_function)};
                    auto partial_continue_block{::llvm::BasicBlock::Create(llvm_context, "memory.partial.cont", current_function)};
                    ir_builder.CreateCondBr(needs_dynamic_bounds_check, partial_check_block, partial_continue_block);

                    ir_builder.SetInsertPoint(partial_check_block);
                    auto memory_length_load{emit_direct_memory_byte_length_value()};
                    if(memory_length_load == nullptr) [[unlikely]] { return nullptr; }

                    auto effective_too_large{ir_builder.CreateICmpUGT(effective_offset, ::llvm::ConstantInt::get(llvm_i64_type, 0xffffffffull))};
                    auto memory_length_i64{ir_builder.CreateZExt(memory_length_load, llvm_i64_type)};
                    auto access_size_i64{::llvm::ConstantInt::get(llvm_i64_type, access_size)};
                    auto memory_too_small{ir_builder.CreateICmpULT(memory_length_i64, access_size_i64)};
                    auto max_access_offset{ir_builder.CreateSub(memory_length_i64, access_size_i64)};
                    auto access_oob{ir_builder.CreateICmpUGT(effective_offset, max_access_offset)};

                    emit_llvm_conditional_trap(
                        *llvm_module,
                        ir_builder,
                        ir_builder.CreateOr(ir_builder.CreateOr(effective_negative, effective_too_large), ir_builder.CreateOr(memory_too_small, access_oob)));
                    ir_builder.CreateBr(partial_continue_block);
                    ir_builder.SetInsertPoint(partial_continue_block);
                }

                auto memory_begin_address{ir_builder.CreatePtrToInt(stable_memory_begin, llvm_intptr_type)};
                auto effective_offset_intptr{ir_builder.CreateIntCast(effective_offset, llvm_intptr_type, false)};
                auto memory_address{ir_builder.CreateAdd(memory_begin_address, effective_offset_intptr, "memory.addr.int")};
                return ir_builder.CreateIntToPtr(memory_address, llvm_i8_ptr_type, "memory.addr");
            }
        }};

    auto const emit_local_imported_memory_snapshot{
        [&]() -> llvm_jit_memory_snapshot_values_t
        {
            llvm_jit_memory_snapshot_values_t result{};
            if(!ensure_memory0_access_info() || memory0_access_info.local_imported_module_ptr == nullptr) [[unlikely]] { return result; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto memory_begin_slot{ir_builder.CreateAlloca(llvm_intptr_type, nullptr, "local_imported.memory.begin.addr.slot")};
            auto byte_length_slot{ir_builder.CreateAlloca(llvm_intptr_type, nullptr, "local_imported.memory.byte_length.slot")};
            auto bridge_function_type{::llvm::FunctionType::get(
                ::llvm::Type::getInt1Ty(llvm_context),
                {llvm_intptr_type, llvm_intptr_type, get_llvm_pointer_type(llvm_intptr_type), get_llvm_pointer_type(llvm_intptr_type)},
                false)};
            auto snapshot_ok{emit_runtime_bridge_call(
                llvm_jit_local_imported_memory_snapshot_bridge,
                bridge_function_type,
                {::llvm::ConstantInt::get(llvm_intptr_type, reinterpret_cast<::std::uintptr_t>(memory0_access_info.local_imported_module_ptr)),
                 ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.local_imported_memory_index),
                 memory_begin_slot,
                 byte_length_slot})};
            if(snapshot_ok == nullptr) [[unlikely]] { return result; }

            emit_llvm_conditional_trap(*llvm_module, ir_builder, ir_builder.CreateNot(snapshot_ok));
            result.memory_begin_address = ir_builder.CreateLoad(llvm_intptr_type, memory_begin_slot, "local_imported.memory.begin.addr");
            result.byte_length = ir_builder.CreateLoad(llvm_intptr_type, byte_length_slot, "local_imported.memory.byte_length");
            return result;
        }};

    auto const emit_local_imported_memory_page_count_value{
        [&]() -> ::llvm::Value*
        {
            if(!ensure_memory0_access_info() || memory0_access_info.local_imported_module_ptr == nullptr) [[unlikely]] { return nullptr; }
            if(memory0_access_info.local_imported_page_size_bytes == 0u) [[unlikely]] { return nullptr; }

            auto const snapshot{emit_local_imported_memory_snapshot()};
            if(snapshot.byte_length == nullptr) [[unlikely]] { return nullptr; }

            return emit_direct_memory_page_count_from_byte_length(snapshot.byte_length,
                                                                  static_cast<::std::size_t>(memory0_access_info.local_imported_page_size_bytes));
        }};

    auto const emit_direct_local_imported_memory_byte_pointer{
        [&](validation_module_traits_t::wasm_u32 static_offset, ::std::size_t access_size, ::llvm::Value* address_value) -> ::llvm::Value*
        {
            if constexpr(::std::endian::native != ::std::endian::little)
            {
                static_cast<void>(static_offset);
                static_cast<void>(access_size);
                static_cast<void>(address_value);
                return nullptr;
            }
            else
            {
                if(!ensure_memory0_access_info() || memory0_access_info.local_imported_module_ptr == nullptr || address_value == nullptr) [[unlikely]]
                {
                    return nullptr;
                }

                auto const snapshot{emit_local_imported_memory_snapshot()};
                if(snapshot.memory_begin_address == nullptr || snapshot.byte_length == nullptr) [[unlikely]] { return nullptr; }

                auto llvm_i8_ptr_type{get_llvm_pointer_type(::llvm::Type::getInt8Ty(llvm_context))};
                auto llvm_i64_type{::llvm::Type::getInt64Ty(llvm_context)};
                auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};

                auto effective_offset{ir_builder.CreateAdd(ir_builder.CreateSExt(address_value, llvm_i64_type),
                                                            ::llvm::ConstantInt::get(llvm_i64_type, static_cast<::std::uint_least32_t>(static_offset)))};
                auto effective_negative{ir_builder.CreateICmpSLT(effective_offset, ::llvm::ConstantInt::getSigned(llvm_i64_type, 0))};
                auto effective_too_large{ir_builder.CreateICmpUGT(effective_offset, ::llvm::ConstantInt::get(llvm_i64_type, 0xffffffffull))};
                auto memory_length_i64{ir_builder.CreateZExt(snapshot.byte_length, llvm_i64_type)};
                auto access_size_i64{::llvm::ConstantInt::get(llvm_i64_type, access_size)};
                auto memory_too_small{ir_builder.CreateICmpULT(memory_length_i64, access_size_i64)};
                auto max_access_offset{ir_builder.CreateSub(memory_length_i64, access_size_i64)};
                auto access_oob{ir_builder.CreateICmpUGT(effective_offset, max_access_offset)};

                emit_llvm_conditional_trap(
                    *llvm_module,
                    ir_builder,
                    ir_builder.CreateOr(ir_builder.CreateOr(effective_negative, effective_too_large), ir_builder.CreateOr(memory_too_small, access_oob)));

                auto effective_offset_intptr{ir_builder.CreateIntCast(effective_offset, llvm_intptr_type, false)};
                auto memory_address{ir_builder.CreateAdd(snapshot.memory_begin_address, effective_offset_intptr, "local_imported.memory.addr.int")};
                return ir_builder.CreateIntToPtr(memory_address, llvm_i8_ptr_type, "local_imported.memory.addr");
            }
        }};

    auto const emit_local_imported_memory_load_bridge_call_for_scalar{
        [&]<typename ScalarType>(::llvm::FunctionType* bridge_function_type,
                                 ::llvm::ArrayRef<::llvm::Value*> bridge_arguments,
                                 ::std::size_t load_bytes,
                                 bool signed_load) -> ::llvm::CallInst*
        {
            if constexpr(::std::same_as<ScalarType, runtime_wasm_i32>)
            {
                switch(load_bytes)
                {
                    case 1uz:
                        return emit_runtime_bridge_call(signed_load ? llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i32, 1uz, true>
                                                                    : llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i32, 1uz, false>,
                                                        bridge_function_type,
                                                        bridge_arguments);
                    case 2uz:
                        return emit_runtime_bridge_call(signed_load ? llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i32, 2uz, true>
                                                                    : llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i32, 2uz, false>,
                                                        bridge_function_type,
                                                        bridge_arguments);
                    case 4uz:
                        return emit_runtime_bridge_call(llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i32, 4uz, false>,
                                                        bridge_function_type,
                                                        bridge_arguments);
                    [[unlikely]] default:
                        return nullptr;
                }
            }
            else if constexpr(::std::same_as<ScalarType, runtime_wasm_i64>)
            {
                switch(load_bytes)
                {
                    case 1uz:
                        return emit_runtime_bridge_call(signed_load ? llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i64, 1uz, true>
                                                                    : llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i64, 1uz, false>,
                                                        bridge_function_type,
                                                        bridge_arguments);
                    case 2uz:
                        return emit_runtime_bridge_call(signed_load ? llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i64, 2uz, true>
                                                                    : llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i64, 2uz, false>,
                                                        bridge_function_type,
                                                        bridge_arguments);
                    case 4uz:
                        return emit_runtime_bridge_call(signed_load ? llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i64, 4uz, true>
                                                                    : llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i64, 4uz, false>,
                                                        bridge_function_type,
                                                        bridge_arguments);
                    case 8uz:
                        return emit_runtime_bridge_call(llvm_jit_local_imported_memory_load_bridge<runtime_wasm_i64, 8uz, false>,
                                                        bridge_function_type,
                                                        bridge_arguments);
                    [[unlikely]] default:
                        return nullptr;
                }
            }
            else if constexpr(::std::same_as<ScalarType, runtime_wasm_f32>)
            {
                if(load_bytes != 4uz) [[unlikely]] { return nullptr; }
                return emit_runtime_bridge_call(llvm_jit_local_imported_memory_load_bridge<runtime_wasm_f32, 4uz, false>,
                                                bridge_function_type,
                                                bridge_arguments);
            }
            else if constexpr(::std::same_as<ScalarType, runtime_wasm_f64>)
            {
                if(load_bytes != 8uz) [[unlikely]] { return nullptr; }
                return emit_runtime_bridge_call(llvm_jit_local_imported_memory_load_bridge<runtime_wasm_f64, 8uz, false>,
                                                bridge_function_type,
                                                bridge_arguments);
            }
            else
            {
                return nullptr;
            }
        }};

    auto const emit_local_imported_memory_store_bridge_call_for_scalar{
        [&]<typename ScalarType>(::llvm::FunctionType* bridge_function_type,
                                 ::llvm::ArrayRef<::llvm::Value*> bridge_arguments,
                                 ::std::size_t store_bytes) -> ::llvm::CallInst*
        {
            if constexpr(::std::same_as<ScalarType, runtime_wasm_i32>)
            {
                switch(store_bytes)
                {
                    case 1uz:
                        return emit_runtime_bridge_call(llvm_jit_local_imported_memory_store_bridge<runtime_wasm_i32, 1uz>,
                                                        bridge_function_type,
                                                        bridge_arguments);
                    case 2uz:
                        return emit_runtime_bridge_call(llvm_jit_local_imported_memory_store_bridge<runtime_wasm_i32, 2uz>,
                                                        bridge_function_type,
                                                        bridge_arguments);
                    case 4uz:
                        return emit_runtime_bridge_call(llvm_jit_local_imported_memory_store_bridge<runtime_wasm_i32, 4uz>,
                                                        bridge_function_type,
                                                        bridge_arguments);
                    [[unlikely]] default:
                        return nullptr;
                }
            }
            else if constexpr(::std::same_as<ScalarType, runtime_wasm_i64>)
            {
                switch(store_bytes)
                {
                    case 1uz:
                        return emit_runtime_bridge_call(llvm_jit_local_imported_memory_store_bridge<runtime_wasm_i64, 1uz>,
                                                        bridge_function_type,
                                                        bridge_arguments);
                    case 2uz:
                        return emit_runtime_bridge_call(llvm_jit_local_imported_memory_store_bridge<runtime_wasm_i64, 2uz>,
                                                        bridge_function_type,
                                                        bridge_arguments);
                    case 4uz:
                        return emit_runtime_bridge_call(llvm_jit_local_imported_memory_store_bridge<runtime_wasm_i64, 4uz>,
                                                        bridge_function_type,
                                                        bridge_arguments);
                    case 8uz:
                        return emit_runtime_bridge_call(llvm_jit_local_imported_memory_store_bridge<runtime_wasm_i64, 8uz>,
                                                        bridge_function_type,
                                                        bridge_arguments);
                    [[unlikely]] default:
                        return nullptr;
                }
            }
            else if constexpr(::std::same_as<ScalarType, runtime_wasm_f32>)
            {
                if(store_bytes != 4uz) [[unlikely]] { return nullptr; }
                return emit_runtime_bridge_call(llvm_jit_local_imported_memory_store_bridge<runtime_wasm_f32, 4uz>, bridge_function_type, bridge_arguments);
            }
            else if constexpr(::std::same_as<ScalarType, runtime_wasm_f64>)
            {
                if(store_bytes != 8uz) [[unlikely]] { return nullptr; }
                return emit_runtime_bridge_call(llvm_jit_local_imported_memory_store_bridge<runtime_wasm_f64, 8uz>, bridge_function_type, bridge_arguments);
            }
            else
            {
                return nullptr;
            }
        }};

    auto const emit_local_imported_memory_load_bridge_call{
        [&](validation_module_traits_t::wasm_u32 static_offset,
            runtime_operand_stack_value_type result_type,
            ::llvm::Type* llvm_result_type,
            ::std::size_t load_bytes,
            bool signed_load,
            ::llvm::Value* address_value) -> ::llvm::CallInst*
        {
            if(memory0_access_info.local_imported_module_ptr == nullptr || llvm_result_type == nullptr || address_value == nullptr) [[unlikely]]
            {
                return nullptr;
            }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{
                ::llvm::FunctionType::get(llvm_result_type,
                                          {llvm_intptr_type, llvm_intptr_type, ::llvm::Type::getInt32Ty(llvm_context), ::llvm::Type::getInt32Ty(llvm_context)},
                                          false)};
            auto const bridge_arguments{
                ::llvm::ArrayRef<::llvm::Value*>{
                                                 ::llvm::ConstantInt::get(llvm_intptr_type, reinterpret_cast<::std::uintptr_t>(memory0_access_info.local_imported_module_ptr)),
                                                 ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.local_imported_memory_index),
                                                 ::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), static_offset),
                                                 address_value}
            };

            switch(result_type)
            {
                case runtime_operand_stack_value_type::i32:
                    return emit_local_imported_memory_load_bridge_call_for_scalar.template operator()<runtime_wasm_i32>(bridge_function_type,
                                                                                                                        bridge_arguments,
                                                                                                                        load_bytes,
                                                                                                                        signed_load);
                case runtime_operand_stack_value_type::i64:
                    return emit_local_imported_memory_load_bridge_call_for_scalar.template operator()<runtime_wasm_i64>(bridge_function_type,
                                                                                                                        bridge_arguments,
                                                                                                                        load_bytes,
                                                                                                                        signed_load);
                case runtime_operand_stack_value_type::f32:
                    return emit_local_imported_memory_load_bridge_call_for_scalar.template operator()<runtime_wasm_f32>(bridge_function_type,
                                                                                                                        bridge_arguments,
                                                                                                                        load_bytes,
                                                                                                                        signed_load);
                case runtime_operand_stack_value_type::f64:
                    return emit_local_imported_memory_load_bridge_call_for_scalar.template operator()<runtime_wasm_f64>(bridge_function_type,
                                                                                                                        bridge_arguments,
                                                                                                                        load_bytes,
                                                                                                                        signed_load);
                [[unlikely]] default:
                {
                    return nullptr;
                }
            }
        }};

    auto const emit_local_imported_memory_store_bridge_call{
        [&](validation_module_traits_t::wasm_u32 static_offset,
            runtime_operand_stack_value_type value_type,
            ::llvm::Type* llvm_value_type,
            ::std::size_t store_bytes,
            ::llvm::Value* address_value,
            ::llvm::Value* value) -> ::llvm::CallInst*
        {
            if(memory0_access_info.local_imported_module_ptr == nullptr || llvm_value_type == nullptr || address_value == nullptr || value == nullptr)
                [[unlikely]]
            {
                return nullptr;
            }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{::llvm::FunctionType::get(
                ::llvm::Type::getVoidTy(llvm_context),
                {llvm_intptr_type, llvm_intptr_type, ::llvm::Type::getInt32Ty(llvm_context), ::llvm::Type::getInt32Ty(llvm_context), llvm_value_type},
                false)};
            auto const bridge_arguments{
                ::llvm::ArrayRef<::llvm::Value*>{
                                                 ::llvm::ConstantInt::get(llvm_intptr_type, reinterpret_cast<::std::uintptr_t>(memory0_access_info.local_imported_module_ptr)),
                                                 ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.local_imported_memory_index),
                                                 ::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), static_offset),
                                                 address_value, value}
            };

            switch(value_type)
            {
                case runtime_operand_stack_value_type::i32:
                    return emit_local_imported_memory_store_bridge_call_for_scalar.template operator()<runtime_wasm_i32>(bridge_function_type,
                                                                                                                         bridge_arguments,
                                                                                                                         store_bytes);
                case runtime_operand_stack_value_type::i64:
                    return emit_local_imported_memory_store_bridge_call_for_scalar.template operator()<runtime_wasm_i64>(bridge_function_type,
                                                                                                                         bridge_arguments,
                                                                                                                         store_bytes);
                case runtime_operand_stack_value_type::f32:
                    return emit_local_imported_memory_store_bridge_call_for_scalar.template operator()<runtime_wasm_f32>(bridge_function_type,
                                                                                                                         bridge_arguments,
                                                                                                                         store_bytes);
                case runtime_operand_stack_value_type::f64:
                    return emit_local_imported_memory_store_bridge_call_for_scalar.template operator()<runtime_wasm_f64>(bridge_function_type,
                                                                                                                         bridge_arguments,
                                                                                                                         store_bytes);
                [[unlikely]] default:
                {
                    return nullptr;
                }
            }
        }};

    auto const emit_direct_memory_byte_pointer{
        [&](validation_module_traits_t::wasm_u32 static_offset, ::std::size_t access_size, ::llvm::Value* address_value) -> ::llvm::Value*
        {
            if(memory0_access_info.memory_p != nullptr)
            {
                if constexpr(runtime_native_memory_t::can_mmap) { return emit_direct_mmap_memory_byte_pointer(static_offset, access_size, address_value); }
            }
            else if(memory0_access_info.local_imported_module_ptr != nullptr)
            {
                return emit_direct_local_imported_memory_byte_pointer(static_offset, access_size, address_value);
            }
            return nullptr;
        }};

    auto const emit_native_memory_load_bridge_call{
        [&](validation_module_traits_t::wasm_u32 static_offset, ::llvm::Type* llvm_result_type, ::llvm::Value* address_value, auto bridge_function)
            -> ::llvm::CallInst*
        {
            if(memory0_access_info.memory_p == nullptr || llvm_result_type == nullptr || address_value == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{
                ::llvm::FunctionType::get(llvm_result_type,
                                          {llvm_intptr_type, ::llvm::Type::getInt32Ty(llvm_context), ::llvm::Type::getInt32Ty(llvm_context)},
                                          false)};
            return emit_runtime_bridge_call(bridge_function,
                                            bridge_function_type,
                                            {::llvm::ConstantInt::get(llvm_intptr_type, reinterpret_cast<::std::uintptr_t>(memory0_access_info.memory_p)),
                                             ::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), static_offset),
                                             address_value});
        }};

    auto const emit_native_memory_store_bridge_call{
        [&](validation_module_traits_t::wasm_u32 static_offset,
            ::llvm::Type* llvm_value_type,
            ::llvm::Value* address_value,
            ::llvm::Value* value,
            auto bridge_function) -> ::llvm::CallInst*
        {
            if(memory0_access_info.memory_p == nullptr || llvm_value_type == nullptr || address_value == nullptr || value == nullptr) [[unlikely]]
            {
                return nullptr;
            }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{
                ::llvm::FunctionType::get(::llvm::Type::getVoidTy(llvm_context),
                                          {llvm_intptr_type, ::llvm::Type::getInt32Ty(llvm_context), ::llvm::Type::getInt32Ty(llvm_context), llvm_value_type},
                                          false)};
            return emit_runtime_bridge_call(bridge_function,
                                            bridge_function_type,
                                            {::llvm::ConstantInt::get(llvm_intptr_type, reinterpret_cast<::std::uintptr_t>(memory0_access_info.memory_p)),
                                             ::llvm::ConstantInt::get(::llvm::Type::getInt32Ty(llvm_context), static_offset),
                                             address_value,
                                             value});
        }};

    auto const emit_native_memory_page_count_bridge_call{
        [&]() -> ::llvm::CallInst*
        {
            if(memory0_access_info.memory_p == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto bridge_function_type{
                ::llvm::FunctionType::get(::llvm::Type::getInt32Ty(llvm_context), {llvm_intptr_type}, false)};
            return emit_runtime_bridge_call(llvm_jit_memory_size_bridge,
                                            bridge_function_type,
                                            {::llvm::ConstantInt::get(llvm_intptr_type, reinterpret_cast<::std::uintptr_t>(memory0_access_info.memory_p))});
        }};

    auto const emit_memory_load_bridge_fallback_call{
        [&](validation_module_traits_t::wasm_u32 static_offset,
            runtime_operand_stack_value_type result_type,
            ::llvm::Type* llvm_result_type,
            ::std::size_t load_bytes,
            bool signed_load,
            ::llvm::Value* address_value,
            auto native_bridge_function) -> ::llvm::CallInst*
        {
            if(memory0_access_info.memory_p != nullptr)
            {
                return emit_native_memory_load_bridge_call(static_offset, llvm_result_type, address_value, native_bridge_function);
            }
            return emit_local_imported_memory_load_bridge_call(static_offset, result_type, llvm_result_type, load_bytes, signed_load, address_value);
        }};

    auto const emit_memory_store_bridge_fallback_call{
        [&](validation_module_traits_t::wasm_u32 static_offset,
            runtime_operand_stack_value_type value_type,
            ::llvm::Type* llvm_value_type,
            ::std::size_t store_bytes,
            ::llvm::Value* address_value,
            ::llvm::Value* value,
            auto native_bridge_function) -> ::llvm::CallInst*
        {
            if(memory0_access_info.memory_p != nullptr)
            {
                return emit_native_memory_store_bridge_call(static_offset, llvm_value_type, address_value, value, native_bridge_function);
            }
            return emit_local_imported_memory_store_bridge_call(static_offset, value_type, llvm_value_type, store_bytes, address_value, value);
        }};

    auto const emit_direct_memory_load_value{
        [&](::llvm::Value* direct_memory_pointer,
            runtime_operand_stack_value_type result_type,
            ::std::size_t load_bytes,
            bool signed_load,
            ::llvm::Align memory_alignment) -> ::llvm::Value*
        {
            if(direct_memory_pointer == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_i8_type{::llvm::Type::getInt8Ty(llvm_context)};
            auto llvm_i16_type{::llvm::Type::getInt16Ty(llvm_context)};
            auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
            auto llvm_i64_type{::llvm::Type::getInt64Ty(llvm_context)};

            if(result_type == runtime_operand_stack_value_type::i32)
            {
                ::llvm::Type* llvm_load_type{};
                switch(load_bytes)
                {
                    case 1uz: llvm_load_type = llvm_i8_type; break;
                    case 2uz: llvm_load_type = llvm_i16_type; break;
                    case 4uz:
                        llvm_load_type = llvm_i32_type;
                        break;
                    [[unlikely]] default:
                        return nullptr;
                }

                auto load_inst{ir_builder.CreateLoad(llvm_load_type,
                                                      ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_load_type)),
                                                      "memory.load")};
                load_inst->setAlignment(memory_alignment);

                if(load_bytes == 4uz) { return load_inst; }
                return signed_load ? ir_builder.CreateSExt(load_inst, llvm_i32_type) : ir_builder.CreateZExt(load_inst, llvm_i32_type);
            }

            if(result_type == runtime_operand_stack_value_type::i64)
            {
                ::llvm::Type* llvm_load_type{};
                switch(load_bytes)
                {
                    case 1uz: llvm_load_type = llvm_i8_type; break;
                    case 2uz: llvm_load_type = llvm_i16_type; break;
                    case 4uz: llvm_load_type = llvm_i32_type; break;
                    case 8uz:
                        llvm_load_type = llvm_i64_type;
                        break;
                    [[unlikely]] default:
                        return nullptr;
                }

                auto load_inst{ir_builder.CreateLoad(llvm_load_type,
                                                      ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_load_type)),
                                                      "memory.load")};
                load_inst->setAlignment(memory_alignment);

                if(load_bytes == 8uz) { return load_inst; }
                return signed_load ? ir_builder.CreateSExt(load_inst, llvm_i64_type) : ir_builder.CreateZExt(load_inst, llvm_i64_type);
            }

            if(result_type == runtime_operand_stack_value_type::f32)
            {
                if(load_bytes != 4uz) [[unlikely]] { return nullptr; }

                auto load_inst{
                    ir_builder.CreateLoad(::llvm::Type::getFloatTy(llvm_context),
                                          ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(::llvm::Type::getFloatTy(llvm_context))),
                                          "memory.load")};
                load_inst->setAlignment(memory_alignment);
                return load_inst;
            }

            if(result_type == runtime_operand_stack_value_type::f64)
            {
                if(load_bytes != 8uz) [[unlikely]] { return nullptr; }

                auto load_inst{
                    ir_builder.CreateLoad(::llvm::Type::getDoubleTy(llvm_context),
                                          ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(::llvm::Type::getDoubleTy(llvm_context))),
                                          "memory.load")};
                load_inst->setAlignment(memory_alignment);
                return load_inst;
            }

            return nullptr;
        }};

    auto const emit_direct_memory_store_value{
        [&](::llvm::Value* direct_memory_pointer,
            runtime_operand_stack_value_type value_type,
            ::llvm::Value* value,
            ::std::size_t store_bytes,
            ::llvm::Align memory_alignment) -> ::llvm::StoreInst*
        {
            if(direct_memory_pointer == nullptr || value == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_i8_type{::llvm::Type::getInt8Ty(llvm_context)};
            auto llvm_i16_type{::llvm::Type::getInt16Ty(llvm_context)};
            auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
            auto llvm_i64_type{::llvm::Type::getInt64Ty(llvm_context)};

            if(value_type == runtime_operand_stack_value_type::i32)
            {
                switch(store_bytes)
                {
                    case 1uz:
                    {
                        auto truncated{ir_builder.CreateTrunc(value, llvm_i8_type)};
                        auto store_inst{
                            ir_builder.CreateStore(truncated, ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i8_type)))};
                        store_inst->setAlignment(memory_alignment);
                        return store_inst;
                    }
                    case 2uz:
                    {
                        auto truncated{ir_builder.CreateTrunc(value, llvm_i16_type)};
                        auto store_inst{
                            ir_builder.CreateStore(truncated, ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i16_type)))};
                        store_inst->setAlignment(memory_alignment);
                        return store_inst;
                    }
                    case 4uz:
                    {
                        auto store_inst{
                            ir_builder.CreateStore(value, ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i32_type)))};
                        store_inst->setAlignment(memory_alignment);
                        return store_inst;
                    }
                    [[unlikely]] default:
                        return nullptr;
                }
            }

            if(value_type == runtime_operand_stack_value_type::i64)
            {
                switch(store_bytes)
                {
                    case 1uz:
                    {
                        auto truncated{ir_builder.CreateTrunc(value, llvm_i8_type)};
                        auto store_inst{
                            ir_builder.CreateStore(truncated, ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i8_type)))};
                        store_inst->setAlignment(memory_alignment);
                        return store_inst;
                    }
                    case 2uz:
                    {
                        auto truncated{ir_builder.CreateTrunc(value, llvm_i16_type)};
                        auto store_inst{
                            ir_builder.CreateStore(truncated, ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i16_type)))};
                        store_inst->setAlignment(memory_alignment);
                        return store_inst;
                    }
                    case 4uz:
                    {
                        auto truncated{ir_builder.CreateTrunc(value, llvm_i32_type)};
                        auto store_inst{
                            ir_builder.CreateStore(truncated, ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i32_type)))};
                        store_inst->setAlignment(memory_alignment);
                        return store_inst;
                    }
                    case 8uz:
                    {
                        auto store_inst{
                            ir_builder.CreateStore(value, ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(llvm_i64_type)))};
                        store_inst->setAlignment(memory_alignment);
                        return store_inst;
                    }
                    [[unlikely]] default:
                        return nullptr;
                }
            }

            if(value_type == runtime_operand_stack_value_type::f32)
            {
                if(store_bytes != 4uz) [[unlikely]] { return nullptr; }

                auto store_inst{
                    ir_builder.CreateStore(value,
                                           ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(::llvm::Type::getFloatTy(llvm_context))))};
                store_inst->setAlignment(memory_alignment);
                return store_inst;
            }

            if(value_type == runtime_operand_stack_value_type::f64)
            {
                if(store_bytes != 8uz) [[unlikely]] { return nullptr; }

                auto store_inst{ir_builder.CreateStore(
                    value,
                    ir_builder.CreatePointerCast(direct_memory_pointer, get_llvm_pointer_type(::llvm::Type::getDoubleTy(llvm_context))))};
                store_inst->setAlignment(memory_alignment);
                return store_inst;
            }

            return nullptr;
        }};

    auto const emit_memory_page_count_value{[&]() -> ::llvm::Value*
                                            {
                                                if(!ensure_memory0_access_info()) [[unlikely]] { return nullptr; }
                                                if(memory0_access_info.local_imported_module_ptr != nullptr)
                                                {
                                                    return emit_local_imported_memory_page_count_value();
                                                }
                                                if constexpr(!runtime_native_memory_t::can_mmap && runtime_native_memory_t::support_multi_thread)
                                                {
                                                    return emit_native_memory_page_count_bridge_call();
                                                }
                                                return emit_direct_memory_page_count_value();
                                            }};

    auto const emit_memory_grow_result_value{
        [&](::llvm::Value* delta_value, ::llvm::Value* current_page_count, ::llvm::Value* definitely_fail, bool local_imported_path, auto&& emit_bridge_call)
            -> ::llvm::Value*
        {
            if(delta_value == nullptr || current_page_count == nullptr) [[unlikely]] { return nullptr; }

            auto current_block{ir_builder.GetInsertBlock()};
            auto current_function{current_block == nullptr ? nullptr : current_block->getParent()};
            if(current_function == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_i32_type{::llvm::Type::getInt32Ty(llvm_context)};
            auto delta_is_zero{ir_builder.CreateICmpEQ(delta_value, ::llvm::ConstantInt::get(llvm_i32_type, 0u))};
            auto grow_zero_block{
                ::llvm::BasicBlock::Create(llvm_context, local_imported_path ? "memory.grow.local_imported.zero" : "memory.grow.zero", current_function)};
            auto grow_fail_block{
                ::llvm::BasicBlock::Create(llvm_context, local_imported_path ? "memory.grow.local_imported.fail" : "memory.grow.fail", current_function)};
            auto grow_runtime_block{
                ::llvm::BasicBlock::Create(llvm_context, local_imported_path ? "memory.grow.local_imported.runtime" : "memory.grow.runtime", current_function)};
            auto grow_merge_block{
                ::llvm::BasicBlock::Create(llvm_context, local_imported_path ? "memory.grow.local_imported.merge" : "memory.grow.merge", current_function)};
            auto non_zero_target{definitely_fail == nullptr
                                      ? grow_runtime_block
                                      : ::llvm::BasicBlock::Create(llvm_context,
                                                                   local_imported_path ? "memory.grow.local_imported.nonzero" : "memory.grow.nonzero",
                                                                   current_function)};

            ir_builder.CreateCondBr(delta_is_zero, grow_zero_block, non_zero_target);

            ir_builder.SetInsertPoint(grow_zero_block);
            ir_builder.CreateBr(grow_merge_block);

            if(non_zero_target != grow_runtime_block)
            {
                ir_builder.SetInsertPoint(non_zero_target);
                ir_builder.CreateCondBr(definitely_fail, grow_fail_block, grow_runtime_block);
            }

            ir_builder.SetInsertPoint(grow_fail_block);
            ir_builder.CreateBr(grow_merge_block);

            ir_builder.SetInsertPoint(grow_runtime_block);
            auto bridge_call{emit_bridge_call()};
            if(bridge_call == nullptr) [[unlikely]] { return nullptr; }
            ir_builder.CreateBr(grow_merge_block);

            ir_builder.SetInsertPoint(grow_merge_block);
            auto grow_result_phi{ir_builder.CreatePHI(llvm_i32_type, 3u, local_imported_path ? "memory.grow.local_imported.result" : "memory.grow.result")};
            grow_result_phi->addIncoming(current_page_count, grow_zero_block);
            grow_result_phi->addIncoming(::llvm::ConstantInt::getSigned(llvm_i32_type, -1), grow_fail_block);
            grow_result_phi->addIncoming(bridge_call, grow_runtime_block);
            return grow_result_phi;
        }};

    auto const get_memory_page_size_bytes{[&]() -> ::std::size_t
                                          {
                                              if(memory0_access_info.local_imported_module_ptr != nullptr)
                                              {
                                                  return static_cast<::std::size_t>(memory0_access_info.local_imported_page_size_bytes);
                                              }
                                              return static_cast<::std::size_t>(1uz) << memory0_access_info.custom_page_size_log2;
                                          }};

    auto const emit_memory_grow_definitely_fail_value{
        [&](::llvm::Value* current_page_count, ::llvm::Value* delta_pages_unsigned) -> ::llvm::Value*
        {
            if(current_page_count == nullptr || delta_pages_unsigned == nullptr) [[unlikely]] { return nullptr; }

            auto const page_size_bytes{get_memory_page_size_bytes()};
            if(memory0_access_info.max_limit_memory_length == ::std::numeric_limits<::std::size_t>::max() || page_size_bytes == 0uz)
            {
                return ::llvm::ConstantInt::getFalse(llvm_context);
            }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
            auto current_page_count_unsigned{ir_builder.CreateZExt(current_page_count, llvm_intptr_type)};
            auto const limit_pages{memory0_access_info.max_limit_memory_length / page_size_bytes};
            auto limit_pages_value{::llvm::ConstantInt::get(llvm_intptr_type, limit_pages)};
            auto current_exceeds_limit{ir_builder.CreateICmpUGT(current_page_count_unsigned, limit_pages_value)};
            auto remaining_pages{ir_builder.CreateSelect(current_exceeds_limit,
                                                          ::llvm::ConstantInt::get(llvm_intptr_type, 0u),
                                                          ir_builder.CreateSub(limit_pages_value, current_page_count_unsigned))};
            return ir_builder.CreateOr(current_exceeds_limit, ir_builder.CreateICmpUGT(delta_pages_unsigned, remaining_pages));
        }};

    auto const emit_memory_grow_bridge_call{
        [&](::llvm::Value* delta_value) -> ::llvm::CallInst*
        {
            if(delta_value == nullptr) [[unlikely]] { return nullptr; }

            auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};

            if(memory0_access_info.local_imported_module_ptr != nullptr)
            {
                auto grow_bridge_function_type{
                    ::llvm::FunctionType::get(::llvm::Type::getInt32Ty(llvm_context),
                                              {llvm_intptr_type, llvm_intptr_type, llvm_intptr_type, ::llvm::Type::getInt32Ty(llvm_context)},
                                              false)};
                return emit_runtime_bridge_call(
                    llvm_jit_local_imported_memory_grow_bridge,
                    grow_bridge_function_type,
                    {::llvm::ConstantInt::get(llvm_intptr_type, reinterpret_cast<::std::uintptr_t>(memory0_access_info.local_imported_module_ptr)),
                     ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.local_imported_memory_index),
                     ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.max_limit_memory_length),
                     delta_value});
            }

            if(memory0_access_info.memory_p == nullptr) [[unlikely]] { return nullptr; }

            auto bridge_function_type{::llvm::FunctionType::get(::llvm::Type::getInt32Ty(llvm_context),
                                                                 {llvm_intptr_type, llvm_intptr_type, ::llvm::Type::getInt32Ty(llvm_context)},
                                                                 false)};
            return emit_runtime_bridge_call(llvm_jit_memory_grow_bridge,
                                            bridge_function_type,
                                            {::llvm::ConstantInt::get(llvm_intptr_type, reinterpret_cast<::std::uintptr_t>(memory0_access_info.memory_p)),
                                             ::llvm::ConstantInt::get(llvm_intptr_type, memory0_access_info.max_limit_memory_length),
                                             delta_value});
        }};

    auto const emit_memory_load_call{
        [&](validation_module_traits_t::wasm_u32 static_offset,
            validation_module_traits_t::wasm_u32 memarg_align,
            runtime_operand_stack_value_type result_type,
            ::llvm::Type* llvm_result_type,
            ::std::size_t load_bytes,
            bool signed_load,
            auto bridge_function) -> bool
        {
            if(!ensure_memory0_access_info() || llvm_result_type == nullptr || operand_stack.empty()) [[unlikely]] { return false; }

            auto const address{operand_stack.back()};
            operand_stack.pop_back();
            if(address.type != runtime_operand_stack_value_type::i32 || address.value == nullptr) [[unlikely]] { return false; }

            if constexpr(::std::endian::native == ::std::endian::little)
            {
                auto direct_memory_pointer{emit_direct_memory_byte_pointer(static_offset, load_bytes, address.value)};

                if(direct_memory_pointer != nullptr)
                {
                    auto const memory_alignment{get_llvm_memory_access_alignment(load_bytes, memarg_align)};
                    auto direct_value{emit_direct_memory_load_value(direct_memory_pointer, result_type, load_bytes, signed_load, memory_alignment)};
                    if(direct_value == nullptr) [[unlikely]] { return false; }

                    push_operand(result_type, direct_value);
                    return true;
                }
            }

            auto bridge_call{
                emit_memory_load_bridge_fallback_call(static_offset, result_type, llvm_result_type, load_bytes, signed_load, address.value, bridge_function)};
            if(bridge_call == nullptr) [[unlikely]] { return false; }

            push_operand(result_type, bridge_call);
            return true;
        }};

    auto const emit_memory_store_call{
        [&](validation_module_traits_t::wasm_u32 static_offset,
            validation_module_traits_t::wasm_u32 memarg_align,
            runtime_operand_stack_value_type value_type,
            ::llvm::Type* llvm_value_type,
            ::std::size_t store_bytes,
            auto bridge_function) -> bool
        {
            if(!ensure_memory0_access_info() || llvm_value_type == nullptr || operand_stack.size() < 2uz) [[unlikely]] { return false; }

            auto const value{operand_stack.back()};
            operand_stack.pop_back();
            auto const address{operand_stack.back()};
            operand_stack.pop_back();

            if(value.type != value_type || address.type != runtime_operand_stack_value_type::i32 || value.value == nullptr || address.value == nullptr)
                [[unlikely]]
            {
                return false;
            }

            if constexpr(::std::endian::native == ::std::endian::little)
            {
                auto direct_memory_pointer{emit_direct_memory_byte_pointer(static_offset, store_bytes, address.value)};

                if(direct_memory_pointer != nullptr)
                {
                    auto const memory_alignment{get_llvm_memory_access_alignment(store_bytes, memarg_align)};
                    return emit_direct_memory_store_value(direct_memory_pointer, value_type, value.value, store_bytes, memory_alignment) != nullptr;
                }
            }

            return emit_memory_store_bridge_fallback_call(static_offset,
                                                          value_type,
                                                          llvm_value_type,
                                                          store_bytes,
                                                          address.value,
                                                          value.value,
                                                          bridge_function) != nullptr;
        }};

    auto const emit_memory_size_call{[&]() -> bool
                                     {
                                         auto page_count{emit_memory_page_count_value()};
                                         if(page_count == nullptr) [[unlikely]] { return false; }

                                         push_operand(runtime_operand_stack_value_type::i32, page_count);
                                         return true;
                                     }};

    auto const emit_memory_grow_call{[&]() -> bool
                                     {
                                         if(!ensure_memory0_access_info() || operand_stack.empty()) [[unlikely]] { return false; }

                                         auto const delta{operand_stack.back()};
                                         operand_stack.pop_back();
                                         if(delta.type != runtime_operand_stack_value_type::i32 || delta.value == nullptr) [[unlikely]] { return false; }

                                         auto llvm_intptr_type{::llvm::Type::getIntNTy(llvm_context, static_cast<unsigned>(sizeof(::std::uintptr_t) * 8u))};
                                         auto current_page_count{emit_memory_page_count_value()};
                                         if(current_page_count == nullptr) [[unlikely]] { return false; }
                                         auto delta_pages_unsigned{ir_builder.CreateZExt(delta.value, llvm_intptr_type)};
                                         auto definitely_fail{emit_memory_grow_definitely_fail_value(current_page_count, delta_pages_unsigned)};
                                         if(definitely_fail == nullptr) [[unlikely]] { return false; }

                                         auto grow_result_phi{emit_memory_grow_result_value(delta.value,
                                                                                             current_page_count,
                                                                                             definitely_fail,
                                                                                             memory0_access_info.local_imported_module_ptr != nullptr,
                                                                                             [&]() -> ::llvm::CallInst*
                                                                                             { return emit_memory_grow_bridge_call(delta.value); })};
                                         if(grow_result_phi == nullptr) [[unlikely]] { return false; }

                                         push_operand(runtime_operand_stack_value_type::i32, grow_result_phi);
                                         return true;
                                     }};

    auto const emit_unary{[&](runtime_operand_stack_value_type expected_type, runtime_operand_stack_value_type result_type, auto&& create_value) -> bool
                          {
                              if(operand_stack.empty()) [[unlikely]] { return false; }

                              auto const operand{operand_stack.back()};
                              operand_stack.pop_back();

                              if(operand.type != expected_type || operand.value == nullptr) [[unlikely]] { return false; }

                              auto value{create_value(operand)};
                              if(value == nullptr) [[unlikely]] { return false; }

                              push_operand(result_type, value);
                              return true;
                          }};

    auto const emit_binary{[&](runtime_operand_stack_value_type expected_type, runtime_operand_stack_value_type result_type, auto&& create_value) -> bool
                           {
                               if(operand_stack.size() < 2uz) [[unlikely]] { return false; }

                               auto const right{operand_stack.back()};
                               operand_stack.pop_back();
                               auto const left{operand_stack.back()};
                               operand_stack.pop_back();

                               if(left.type != expected_type || right.type != expected_type || left.value == nullptr || right.value == nullptr) [[unlikely]]
                               {
                                   return false;
                               }

                               auto value{create_value(left, right)};
                               if(value == nullptr) [[unlikely]] { return false; }

                               push_operand(result_type, value);
                               return true;
                           }};

    if(control_stack.empty() || code_curr == code_end) [[unlikely]] { return false; }

    wasm1_code curr_opbase;
    ::std::memcpy(::std::addressof(curr_opbase), code_curr, sizeof(wasm1_code));

    if(!control_stack.back().is_reachable)
    {
        switch(curr_opbase)
        {
            case wasm1_code::block:
            case wasm1_code::loop:
            case wasm1_code::if_:
            {
                ++code_curr;
                runtime_block_result_type skipped_result{};
                if(!parse_wasm_block_result_type(code_curr, code_end, skipped_result)) [[unlikely]] { return false; }
                ++unreachable_control_depth;
                return code_curr == code_end;
            }
            case wasm1_code::else_:
            {
                if(unreachable_control_depth != 0uz)
                {
                    ++code_curr;
                    return code_curr == code_end;
                }
                break;
            }
            case wasm1_code::end:
            {
                if(unreachable_control_depth != 0uz)
                {
                    ++code_curr;
                    --unreachable_control_depth;
                    return code_curr == code_end;
                }
                break;
            }
            [[unlikely]] default:
            {
                if(!skip_wasm_unreachable_noncontrol_instruction(code_curr, code_end)) [[unlikely]] { return false; }
                return code_curr == code_end;
            }
        }
    }

    switch(curr_opbase)
    {
        case wasm1_code::unreachable:
        {
            ++code_curr;
            if(!try_emit_runtime_local_func_llvm_jit_unreachable(state)) [[unlikely]] { return false; }
            break;
        }
        case wasm1_code::nop:
        {
            ++code_curr;
            if(!try_emit_runtime_local_func_llvm_jit_nop(state)) [[unlikely]] { return false; }
            break;
        }
        case wasm1_code::block:
        {
            ++code_curr;

            runtime_block_result_type block_result{};
            if(!parse_wasm_block_result_type(code_curr, code_end, block_result) ||
               !try_emit_runtime_local_func_llvm_jit_block(state, block_result))
                [[unlikely]]
            {
                return false;
            }
            break;
        }
        case wasm1_code::loop:
        {
            ++code_curr;

            runtime_block_result_type block_result{};
            if(!parse_wasm_block_result_type(code_curr, code_end, block_result) ||
               !try_emit_runtime_local_func_llvm_jit_loop(state, block_result))
                [[unlikely]]
            {
                return false;
            }
            break;
        }
        case wasm1_code::if_:
        {
            ++code_curr;

            runtime_block_result_type block_result{};
            if(!parse_wasm_block_result_type(code_curr, code_end, block_result) ||
               !try_emit_runtime_local_func_llvm_jit_if(state, block_result))
                [[unlikely]]
            {
                return false;
            }
            break;
        }
        case wasm1_code::else_:
        {
            ++code_curr;
            if(!try_emit_runtime_local_func_llvm_jit_else(state)) [[unlikely]] { return false; }
            break;
        }
        case wasm1_code::end:
        {
            ++code_curr;
            if(!try_emit_runtime_local_func_llvm_jit_end(state)) [[unlikely]] { return false; }
            break;
        }
#include "opcode/memory_emit_cases.h"
#include "opcode/int_numeric_emit_cases.h"
        [[unlikely]] default:
        {
            return false;
        }
    }

    return code_curr == code_end;
}

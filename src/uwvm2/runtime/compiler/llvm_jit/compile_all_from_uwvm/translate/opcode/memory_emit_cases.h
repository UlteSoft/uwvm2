case wasm1_code::i32_load:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(
           offset, align, runtime_operand_stack_value_type::i32, ::llvm::Type::getInt32Ty(llvm_context), 4uz, false, llvm_jit_memory_load_bridge<runtime_wasm_i32, 4uz>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i64_load:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(
           offset, align, runtime_operand_stack_value_type::i64, ::llvm::Type::getInt64Ty(llvm_context), 8uz, false, llvm_jit_memory_load_bridge<runtime_wasm_i64, 8uz>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::f32_load:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(
           offset, align, runtime_operand_stack_value_type::f32, ::llvm::Type::getFloatTy(llvm_context), 4uz, false, llvm_jit_memory_load_bridge<runtime_wasm_f32, 4uz>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::f64_load:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(
           offset, align, runtime_operand_stack_value_type::f64, ::llvm::Type::getDoubleTy(llvm_context), 8uz, false, llvm_jit_memory_load_bridge<runtime_wasm_f64, 8uz>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i32_load8_s:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(offset,
                              align,
                              runtime_operand_stack_value_type::i32,
                              ::llvm::Type::getInt32Ty(llvm_context),
                              1uz,
                              true,
                              llvm_jit_memory_load_bridge<runtime_wasm_i32, 1uz, true>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i32_load8_u:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(offset,
                              align,
                              runtime_operand_stack_value_type::i32,
                              ::llvm::Type::getInt32Ty(llvm_context),
                              1uz,
                              false,
                              llvm_jit_memory_load_bridge<runtime_wasm_i32, 1uz, false>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i32_load16_s:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(offset,
                              align,
                              runtime_operand_stack_value_type::i32,
                              ::llvm::Type::getInt32Ty(llvm_context),
                              2uz,
                              true,
                              llvm_jit_memory_load_bridge<runtime_wasm_i32, 2uz, true>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i32_load16_u:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(offset,
                              align,
                              runtime_operand_stack_value_type::i32,
                              ::llvm::Type::getInt32Ty(llvm_context),
                              2uz,
                              false,
                              llvm_jit_memory_load_bridge<runtime_wasm_i32, 2uz, false>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i64_load8_s:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(offset,
                              align,
                              runtime_operand_stack_value_type::i64,
                              ::llvm::Type::getInt64Ty(llvm_context),
                              1uz,
                              true,
                              llvm_jit_memory_load_bridge<runtime_wasm_i64, 1uz, true>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i64_load8_u:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(offset,
                              align,
                              runtime_operand_stack_value_type::i64,
                              ::llvm::Type::getInt64Ty(llvm_context),
                              1uz,
                              false,
                              llvm_jit_memory_load_bridge<runtime_wasm_i64, 1uz, false>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i64_load16_s:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(offset,
                              align,
                              runtime_operand_stack_value_type::i64,
                              ::llvm::Type::getInt64Ty(llvm_context),
                              2uz,
                              true,
                              llvm_jit_memory_load_bridge<runtime_wasm_i64, 2uz, true>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i64_load16_u:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(offset,
                              align,
                              runtime_operand_stack_value_type::i64,
                              ::llvm::Type::getInt64Ty(llvm_context),
                              2uz,
                              false,
                              llvm_jit_memory_load_bridge<runtime_wasm_i64, 2uz, false>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i64_load32_s:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(offset,
                              align,
                              runtime_operand_stack_value_type::i64,
                              ::llvm::Type::getInt64Ty(llvm_context),
                              4uz,
                              true,
                              llvm_jit_memory_load_bridge<runtime_wasm_i64, 4uz, true>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i64_load32_u:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(offset,
                              align,
                              runtime_operand_stack_value_type::i64,
                              ::llvm::Type::getInt64Ty(llvm_context),
                              4uz,
                              false,
                              llvm_jit_memory_load_bridge<runtime_wasm_i64, 4uz, false>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i32_store:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(
           offset, align, runtime_operand_stack_value_type::i32, ::llvm::Type::getInt32Ty(llvm_context), 4uz, llvm_jit_memory_store_bridge<runtime_wasm_i32, 4uz>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i64_store:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(
           offset, align, runtime_operand_stack_value_type::i64, ::llvm::Type::getInt64Ty(llvm_context), 8uz, llvm_jit_memory_store_bridge<runtime_wasm_i64, 8uz>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::f32_store:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(
           offset, align, runtime_operand_stack_value_type::f32, ::llvm::Type::getFloatTy(llvm_context), 4uz, llvm_jit_memory_store_bridge<runtime_wasm_f32, 4uz>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::f64_store:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(
           offset, align, runtime_operand_stack_value_type::f64, ::llvm::Type::getDoubleTy(llvm_context), 8uz, llvm_jit_memory_store_bridge<runtime_wasm_f64, 8uz>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i32_store8:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(
           offset, align, runtime_operand_stack_value_type::i32, ::llvm::Type::getInt32Ty(llvm_context), 1uz, llvm_jit_memory_store_bridge<runtime_wasm_i32, 1uz>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i32_store16:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(
           offset, align, runtime_operand_stack_value_type::i32, ::llvm::Type::getInt32Ty(llvm_context), 2uz, llvm_jit_memory_store_bridge<runtime_wasm_i32, 2uz>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i64_store8:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(
           offset, align, runtime_operand_stack_value_type::i64, ::llvm::Type::getInt64Ty(llvm_context), 1uz, llvm_jit_memory_store_bridge<runtime_wasm_i64, 1uz>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i64_store16:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(
           offset, align, runtime_operand_stack_value_type::i64, ::llvm::Type::getInt64Ty(llvm_context), 2uz, llvm_jit_memory_store_bridge<runtime_wasm_i64, 2uz>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::i64_store32:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(
           offset, align, runtime_operand_stack_value_type::i64, ::llvm::Type::getInt64Ty(llvm_context), 4uz, llvm_jit_memory_store_bridge<runtime_wasm_i64, 4uz>))
        [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::memory_size:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 memory_index{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, memory_index) || memory_index != 0u || !emit_memory_size_call()) [[unlikely]]
    {
        return result;
    }
    break;
}
case wasm1_code::memory_grow:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 memory_index{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, memory_index) || memory_index != 0u || !emit_memory_grow_call()) [[unlikely]]
    {
        return result;
    }
    break;
}

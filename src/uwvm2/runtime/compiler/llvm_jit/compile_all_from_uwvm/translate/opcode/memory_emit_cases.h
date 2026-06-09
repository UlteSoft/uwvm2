// Memory opcode LLVM emission cases.
// These cases replay the byte range already accepted by validation: they advance past the opcode,
// decode the same `align` and `offset` memarg immediates, and hand the semantic parameters to the
// dispatcher-local emit helpers.  The helpers prefer direct linear-memory IR on supported little-
// endian configurations and fall back to typed runtime bridges when direct access is unavailable,
// unsafe, imported, or otherwise not profitable.

// i32.load
// Emits a 4-byte little-endian integer load from memory0 and pushes an i32.  The bridge fallback
// performs the same Wasm bounds check and byte-order handling as direct-memory IR.
case wasm1_code::i32_load:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(offset,
                              align,
                              runtime_operand_stack_value_type::i32,
                              ::llvm::Type::getInt32Ty(llvm_context),
                              4uz,
                              false,
                              llvm_jit_memory_load_bridge<runtime_wasm_i32, 4uz>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i64.load
// Emits an 8-byte little-endian integer load and pushes an i64.
case wasm1_code::i64_load:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(offset,
                              align,
                              runtime_operand_stack_value_type::i64,
                              ::llvm::Type::getInt64Ty(llvm_context),
                              8uz,
                              false,
                              llvm_jit_memory_load_bridge<runtime_wasm_i64, 8uz>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// f32.load
// Emits a 4-byte load whose bits are interpreted as f32; NaN payloads and signed-zero encodings are
// preserved because the load is bit-based rather than a numeric conversion.
case wasm1_code::f32_load:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(offset,
                              align,
                              runtime_operand_stack_value_type::f32,
                              ::llvm::Type::getFloatTy(llvm_context),
                              4uz,
                              false,
                              llvm_jit_memory_load_bridge<runtime_wasm_f32, 4uz>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// f64.load
// Emits an 8-byte load whose bits are interpreted as f64.
case wasm1_code::f64_load:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_load_call(offset,
                              align,
                              runtime_operand_stack_value_type::f64,
                              ::llvm::Type::getDoubleTy(llvm_context),
                              8uz,
                              false,
                              llvm_jit_memory_load_bridge<runtime_wasm_f64, 8uz>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i32.load8_s
// Emits a one-byte load and sign-extends the byte to i32.
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
                              llvm_jit_memory_load_bridge<runtime_wasm_i32, 1uz, true>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i32.load8_u
// Emits a one-byte load and zero-extends the byte to i32.
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
                              llvm_jit_memory_load_bridge<runtime_wasm_i32, 1uz, false>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i32.load16_s
// Emits a two-byte little-endian load and sign-extends the halfword to i32.
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
                              llvm_jit_memory_load_bridge<runtime_wasm_i32, 2uz, true>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i32.load16_u
// Emits a two-byte little-endian load and zero-extends the halfword to i32.
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
                              llvm_jit_memory_load_bridge<runtime_wasm_i32, 2uz, false>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i64.load8_s
// Emits a one-byte load and sign-extends the byte to i64.
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
                              llvm_jit_memory_load_bridge<runtime_wasm_i64, 1uz, true>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i64.load8_u
// Emits a one-byte load and zero-extends the byte to i64.
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
                              llvm_jit_memory_load_bridge<runtime_wasm_i64, 1uz, false>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i64.load16_s
// Emits a two-byte little-endian load and sign-extends the halfword to i64.
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
                              llvm_jit_memory_load_bridge<runtime_wasm_i64, 2uz, true>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i64.load16_u
// Emits a two-byte little-endian load and zero-extends the halfword to i64.
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
                              llvm_jit_memory_load_bridge<runtime_wasm_i64, 2uz, false>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i64.load32_s
// Emits a four-byte little-endian load and sign-extends the word to i64.
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
                              llvm_jit_memory_load_bridge<runtime_wasm_i64, 4uz, true>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i64.load32_u
// Emits a four-byte little-endian load and zero-extends the word to i64.
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
                              llvm_jit_memory_load_bridge<runtime_wasm_i64, 4uz, false>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i32.store
// Emits a full-width 4-byte integer store.  The value operand is popped above the i32 address,
// matching WebAssembly's `(address, value)` store stack order.
case wasm1_code::i32_store:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(offset,
                               align,
                               runtime_operand_stack_value_type::i32,
                               ::llvm::Type::getInt32Ty(llvm_context),
                               4uz,
                               llvm_jit_memory_store_bridge<runtime_wasm_i32, 4uz>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i64.store
// Emits a full-width 8-byte integer store.
case wasm1_code::i64_store:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(offset,
                               align,
                               runtime_operand_stack_value_type::i64,
                               ::llvm::Type::getInt64Ty(llvm_context),
                               8uz,
                               llvm_jit_memory_store_bridge<runtime_wasm_i64, 8uz>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// f32.store
// Emits a 4-byte store of the f32 payload bits, preserving the exact IEEE representation.
case wasm1_code::f32_store:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(offset,
                               align,
                               runtime_operand_stack_value_type::f32,
                               ::llvm::Type::getFloatTy(llvm_context),
                               4uz,
                               llvm_jit_memory_store_bridge<runtime_wasm_f32, 4uz>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// f64.store
// Emits an 8-byte store of the f64 payload bits.
case wasm1_code::f64_store:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(offset,
                               align,
                               runtime_operand_stack_value_type::f64,
                               ::llvm::Type::getDoubleTy(llvm_context),
                               8uz,
                               llvm_jit_memory_store_bridge<runtime_wasm_f64, 8uz>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i32.store8
// Emits a one-byte store containing the low 8 bits of the i32 value.
case wasm1_code::i32_store8:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(offset,
                               align,
                               runtime_operand_stack_value_type::i32,
                               ::llvm::Type::getInt32Ty(llvm_context),
                               1uz,
                               llvm_jit_memory_store_bridge<runtime_wasm_i32, 1uz>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i32.store16
// Emits a two-byte little-endian store containing the low 16 bits of the i32 value.
case wasm1_code::i32_store16:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(offset,
                               align,
                               runtime_operand_stack_value_type::i32,
                               ::llvm::Type::getInt32Ty(llvm_context),
                               2uz,
                               llvm_jit_memory_store_bridge<runtime_wasm_i32, 2uz>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i64.store8
// Emits a one-byte store containing the low 8 bits of the i64 value.
case wasm1_code::i64_store8:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(offset,
                               align,
                               runtime_operand_stack_value_type::i64,
                               ::llvm::Type::getInt64Ty(llvm_context),
                               1uz,
                               llvm_jit_memory_store_bridge<runtime_wasm_i64, 1uz>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i64.store16
// Emits a two-byte little-endian store containing the low 16 bits of the i64 value.
case wasm1_code::i64_store16:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(offset,
                               align,
                               runtime_operand_stack_value_type::i64,
                               ::llvm::Type::getInt64Ty(llvm_context),
                               2uz,
                               llvm_jit_memory_store_bridge<runtime_wasm_i64, 2uz>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// i64.store32
// Emits a four-byte little-endian store containing the low 32 bits of the i64 value.
case wasm1_code::i64_store32:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 align{};
    validation_module_traits_t::wasm_u32 offset{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, align) || !parse_wasm_leb128_immediate(code_curr, code_end, offset) ||
       !emit_memory_store_call(offset,
                               align,
                               runtime_operand_stack_value_type::i64,
                               ::llvm::Type::getInt64Ty(llvm_context),
                               4uz,
                               llvm_jit_memory_store_bridge<runtime_wasm_i64, 4uz>)) [[unlikely]]
    {
        return result;
    }
    break;
}

// memory.size
// Emits the current memory0 size in Wasm pages as an i32.  Validation already proved the MVP
// memory index is zero; this replay still decodes it to keep the emit cursor synchronized.
case wasm1_code::memory_size:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 memory_index{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, memory_index) || memory_index != 0u || !emit_memory_size_call()) [[unlikely]] { return result; }
    break;
}

// memory.grow
// Emits WebAssembly's grow result: old page count on success or -1 on failure.  The helper can
// synthesize definitely-failing cases in IR and otherwise delegates to the memory growth bridge.
case wasm1_code::memory_grow:
{
    ++code_curr;

    validation_module_traits_t::wasm_u32 memory_index{};
    if(!parse_wasm_leb128_immediate(code_curr, code_end, memory_index) || memory_index != 0u || !emit_memory_grow_call()) [[unlikely]] { return result; }
    break;
}

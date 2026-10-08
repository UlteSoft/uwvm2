/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
// Included after single_func_gc_emit.h in the existing translator namespace.
// Only the fused validation dispatcher calls this entry, after its actual opcode
// offset/provenance/numeric-plan/debug-safe-point prologue and Core 3 GC semantics.
[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_decoded_gc_aggregate(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded) noexcept
{
    namespace v3 = ::uwvm2::validation::standard::wasm3;
    if(!state.valid || state.control_stack.empty() || state.current_wasm_op_offset == SIZE_MAX ||
       decoded.error != v3::gc_immediate_error::ok || decoded.opcode > 19u) { return false; }
    // The sole bounded scanner already committed the full immediate in the
    // original expression. Neither raw byte pointers nor a second decoder enter
    // this lowering. Opcode FB was sealed_pure in the former raw prologue;
    // the existing aggregate helper retains all precise-root/retirement hooks.
    if(!state.control_stack.back().is_reachable) { return true; }
    return try_emit_runtime_local_func_llvm_jit_gc_aggregate(state, decoded);
}

// Original i31 IR, now entered only with the first bounded scanner's owned DATA.
[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_decoded_gc_i31(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded) noexcept
{
    namespace v3 = ::uwvm2::validation::standard::wasm3;
    if(!state.valid || state.ir_builder == nullptr || state.llvm_module == nullptr ||
       state.local_func_storage_ptr == nullptr || state.control_stack.empty() ||
       state.current_wasm_op_offset == SIZE_MAX || decoded.error != v3::gc_immediate_error::ok ||
       decoded.opcode < 28u || decoded.opcode > 30u) { return false; }
    if(!state.control_stack.back().is_reachable || state.unreachable_control_depth != 0uz) { return true; }
    auto llvm_module{state.llvm_module};
    auto& ir_builder{*state.ir_builder};
    auto& operand_stack{state.operand_stack};
    auto const push_operand{[&](runtime_operand_stack_value_type type, ::llvm::Value* value) constexpr noexcept
                            { operand_stack.push_back({.type = type, .value = value}); }};
    if(operand_stack.empty()) [[unlikely]] { return false; }
    using ref_type = runtime_wasm_global_ref;
    using ref_kind = ::uwvm2::object::global::wasm_ref_kind;
    constexpr auto ref_bytes{sizeof(ref_type)};
    constexpr auto payload_offset{offsetof(ref_type, storage)};
    constexpr auto i31_bytes{sizeof(::uwvm2::parser::wasm::standard::wasm3::type::wasm_i31)};
    constexpr auto storage_bytes{sizeof(decltype(ref_type::storage))};
    constexpr auto tag_offset{offsetof(ref_type, kind)};
    constexpr auto tag_bytes{sizeof(decltype(ref_type::kind))};
    static_assert(payload_offset + i31_bytes <= ref_bytes);
    static_assert(tag_offset + tag_bytes <= ref_bytes);
    constexpr auto ref_bits{static_cast<unsigned>(ref_bytes * CHAR_BIT)};
    bool const little_endian{llvm_module->getDataLayout().isLittleEndian()};
    auto const top{operand_stack.back()};
    if(top.value == nullptr) [[unlikely]] { return false; }
    if(decoded.opcode == 28u)
    {
        if(top.type != runtime_operand_stack_value_type::i32 || !top.value->getType()->isIntegerTy(32u)) [[unlikely]]
        { return false; }
        // The Wasm i31 payload occupies the first four bytes of the reference union.
        // On a big-endian pointer-width storage slot, shift into its high bytes before packing.
        auto masked{ir_builder.CreateAnd(top.value, ir_builder.getInt32(0x7fff'ffffu), get_llvm_string_ref(u8"i31.bits"))};
        auto payload{ir_builder.CreateZExt(masked, ir_builder.getIntNTy(static_cast<unsigned>(storage_bytes * CHAR_BIT)))};
        if(!little_endian && storage_bytes > i31_bytes)
        { payload = ir_builder.CreateShl(payload, static_cast<unsigned>((storage_bytes - i31_bytes) * CHAR_BIT)); }
        auto const packed{emit_llvm_jit_ref_from_payload(ir_builder, payload, ref_kind::wasm_i31, little_endian)};
        if(packed == nullptr) [[unlikely]] { return false; }
        // [SSA operand ...][i32] end -> [SSA operand ...][i31ref] end.
        // [safe                   ] empty() proved back(); copied before pop_back invalidates it.
        operand_stack.pop_back();
        push_operand(runtime_operand_stack_value_type::funcref, packed);
        return true;
    }
    if(decoded.opcode != 29u && decoded.opcode != 30u) [[unlikely]] { return false; }
    if(top.type != runtime_operand_stack_value_type::funcref || !top.value->getType()->isIntegerTy(ref_bits)) [[unlikely]]
    { return false; }
    auto const tag_shift{static_cast<unsigned>((little_endian ? tag_offset : ref_bytes - tag_offset - tag_bytes) * CHAR_BIT)};
    auto const tag_bits{ir_builder.CreateLShr(top.value, tag_shift, get_llvm_string_ref(u8"i31.tag.shift"))};
    auto const tag{ir_builder.CreateTrunc(tag_bits, ir_builder.getIntNTy(static_cast<unsigned>(tag_bytes * CHAR_BIT)),
        get_llvm_string_ref(u8"i31.tag"))};
    auto const wrong_kind{ir_builder.CreateICmpNE(tag,
        ::llvm::ConstantInt::get(tag->getType(), static_cast<unsigned>(ref_kind::wasm_i31)))};
    // A valid typed operand can only be i31 or null. Keep the null trap on dynamic references;
    // LLVM folds it entirely when ref.i31 produced a known tag in the same function.
    emit_llvm_conditional_trap(*llvm_module, ir_builder, wrong_kind,
        ::uwvm2::runtime::lib::llvm_jit_trap_kind::null_reference);
    auto const payload_shift{static_cast<unsigned>((little_endian ? payload_offset :
        ref_bytes - payload_offset - i31_bytes) * CHAR_BIT)};
    auto const payload_bits{ir_builder.CreateLShr(top.value, payload_shift, get_llvm_string_ref(u8"i31.payload.shift"))};
    auto const low{ir_builder.CreateTrunc(payload_bits, ir_builder.getInt32Ty(), get_llvm_string_ref(u8"i31.payload"))};
    ::llvm::Value* result_i32{};
    if(decoded.opcode == 29u)
    {
        auto const signed_bits{ir_builder.CreateShl(low, 1u, get_llvm_string_ref(u8"i31.sign.extend"))};
        result_i32 = ir_builder.CreateAShr(signed_bits, 1u, get_llvm_string_ref(u8"i31.get_s"));
    }
    else { result_i32 = ir_builder.CreateAnd(low, ir_builder.getInt32(0x7fff'ffffu), get_llvm_string_ref(u8"i31.get_u")); }
    // [SSA operand ...][i31ref] end -> [SSA operand ...][i32] end.
    // [safe                      ] copied top before pop_back; push_operand owns a new descriptor.
    operand_stack.pop_back();
    push_operand(runtime_operand_stack_value_type::i32, result_i32);
    return true;
}

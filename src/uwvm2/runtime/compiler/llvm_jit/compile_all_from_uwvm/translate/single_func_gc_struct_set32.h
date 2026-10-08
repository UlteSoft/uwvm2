#include "single_func_gc_struct_set32_bridge.h"

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_struct_set32(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded,
    runtime_operand_stack_value_type value_type) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    namespace type = ::uwvm2::parser::wasm::standard::wasm3::type;
    using wasm_type = runtime_operand_stack_value_type;
    using reference = storage::gc_reference;
    static_assert(::std::is_standard_layout_v<reference>);
    constexpr auto bytes{sizeof(reference)};
    constexpr auto payload_offset{offsetof(reference, storage)};
    constexpr auto payload_bytes{sizeof(reference::storage)};
    constexpr auto kind_offset{offsetof(reference, kind)};
    constexpr auto kind_bytes{sizeof(reference::kind)};
    static_assert(payload_bytes == sizeof(::std::uintptr_t));
    static_assert(kind_bytes == sizeof(::std::uint32_t));
    static_assert(payload_offset <= bytes && payload_bytes <= bytes - payload_offset);
    static_assert(kind_offset <= bytes && kind_bytes <= bytes - kind_offset);
    static_assert(payload_offset + payload_bytes <= kind_offset ||
        kind_offset + kind_bytes <= payload_offset);
    if(!state.valid || state.ir_builder == nullptr || state.llvm_module == nullptr ||
       state.local_func_storage_ptr == nullptr || state.operand_stack.size() < 2uz ||
       decoded.opcode != 5u ||
       (value_type != wasm_type::i32 && value_type != wasm_type::f32)) { return false; }
    auto& stack{state.operand_stack};
    // [live SSA prefix][complete struct reference][raw scalar] end
    // [safe ] size>=2 proves each subscript. Copy every descriptor before any
    // mutation; no pointer into the operand vector survives the native call.
    auto const operand{stack[stack.size() - 2uz]};
    auto const input{stack.back()};
    if(operand.type != wasm_type::funcref || operand.value == nullptr ||
       !operand.value->getType()->isIntegerTy(static_cast<unsigned>(bytes * CHAR_BIT)) ||
       input.type != value_type || input.value == nullptr ||
       (value_type == wasm_type::i32 ? !input.value->getType()->isIntegerTy(32u) :
        !input.value->getType()->isFloatTy())) { return false; }
    // [live compiler local function][retained runtime instance]
    // [safe ] checked local_func_storage_ptr names the retained compilation
    // record; its module pointer is borrowed only during synchronous emission.
    auto const* module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr || !module->gc_store || !module->gc_store->valid()) { return false; }
    type::composite_kind kind{};
    if(!module->gc_store->type_kind(decoded.first, kind) || kind != type::composite_kind::struct_)
    { return false; }
    // [actual store-owned canonical field array] end
    // [safe ] field_at bounds both type and field indices before returning this
    // immutable declaration borrow; no object address is derived from it.
    auto const* field{module->gc_store->field_at(decoded.first, decoded.second)};
    if(field == nullptr || !field->mutable_) { return false; }
    if(value_type == wasm_type::f32)
    {
        if(field->storage.packed != type::packed_kind::none ||
           field->storage.value.kind != type::value_kind::f32) { return false; }
    }
    else if(field->storage.value.kind != type::value_kind::i32 ||
            (field->storage.packed != type::packed_kind::none &&
             field->storage.packed != type::packed_kind::i8 &&
             field->storage.packed != type::packed_kind::i16)) { return false; }
    auto const& layout{state.llvm_module->getDataLayout()};
    // Every declinable native-layout/type condition precedes creation of IR.
    // Wider/reference structs continue to the complete original helper.
    if(state.llvm_module->getDataLayoutStr().empty() ||
       layout.getPointerSize() != sizeof(::std::uintptr_t) ||
       layout.isLittleEndian() != (::std::endian::native == ::std::endian::little)) { return false; }
    auto& builder{*state.ir_builder};
    auto const current_block{builder.GetInsertBlock()};
    auto const function{current_block == nullptr ? nullptr : current_block->getParent()};
    if(function == nullptr) { return false; }
    auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const i32{builder.getInt32Ty()};
    constexpr bool little_endian{::std::endian::native == ::std::endian::little};
    constexpr auto payload_shift{static_cast<unsigned>((little_endian ? payload_offset :
        bytes - payload_offset - payload_bytes) * CHAR_BIT)};
    constexpr auto kind_shift{static_cast<unsigned>((little_endian ? kind_offset :
        bytes - kind_offset - kind_bytes) * CHAR_BIT)};
    // [complete native 16B carrier SSA][two disjoint actual fields]
    // [safe ] shifts/truncations extract representation, never a dereferenced
    // native address or authority from padding/guest token arithmetic.
    auto const payload{builder.CreateTrunc(builder.CreateLShr(operand.value, payload_shift),
        intptr, get_llvm_string_ref(u8"gc.struct.set32.payload"))};
    auto const reference_kind{builder.CreateTrunc(builder.CreateLShr(operand.value, kind_shift),
        i32, get_llvm_string_ref(u8"gc.struct.set32.kind"))};
    auto const raw{value_type == wasm_type::f32 ?
        builder.CreateBitCast(input.value, i32, get_llvm_string_ref(u8"gc.struct.set32.raw.f32")) : input.value};
    auto const module_name{get_llvm_runtime_module_object_symbol_name(*module)};
    // [actual owned instance][compiler host-symbol relocation]
    // [safe ] binding retains the original native module lifetime; this is
    // not a direct unowned store pointer encoded into guest data.
    auto const module_address{get_llvm_external_host_object_address(builder,
        reinterpret_cast<::std::uintptr_t>(module),
        ::uwvm2::utils::container::u8string_view{module_name.data(), module_name.size()})};
    if(module_address == nullptr) { return false; }
    // [u32 Wasm kind/value][register-wide native C ABI]
    // [safe ] zero-extension explicitly preserves high-bit i32/f32/packed
    // representations; i386's equal-width conversion is an identity.
    auto const wide_kind{builder.CreateZExtOrTrunc(reference_kind, intptr,
        get_llvm_string_ref(u8"gc.struct.set32.kind.wide"))};
    auto const wide_raw{builder.CreateZExtOrTrunc(raw, intptr,
        get_llvm_string_ref(u8"gc.struct.set32.raw.wide"))};
    auto const bridge_type{::llvm::FunctionType::get(intptr, {intptr, intptr, intptr, intptr, intptr}, false)};
    auto const callee{get_llvm_runtime_bridge_function_symbol_value<uwvm2_llvm_jit_gc_struct_set32_wide_bridge_r1>(
        builder, bridge_type, ::uwvm2::utils::container::u8string_view{u8"gc_struct_set32_registerwide_v1"})};
    if(callee == nullptr) { return false; }
    auto& context{builder.getContext()};
    auto const trap_type{get_llvm_runtime_trap_bridge_function_type(context)};
    if(trap_type == nullptr) { return false; }
    auto const continue_block{::llvm::BasicBlock::Create(context,
        get_llvm_string_ref(u8"gc.struct.set32.ok"), function)};
    auto const error_block{::llvm::BasicBlock::Create(context,
        get_llvm_string_ref(u8"gc.struct.set32.error"), function)};
    // [original insertion cursor][new owned success/error blocks]
    // [safe ] resolve the original trap in its actual cold block before
    // emitting the side-effecting setter. RV64 address materialization stays
    // cold; a failed symbol resolution cannot leave an emitted native setter
    // that the generic fallback would execute a second time.
    auto const original_insertion{builder.saveIP()};
    builder.SetInsertPoint(error_block);
    auto const trap_callee{get_llvm_runtime_bridge_function_symbol_value<
        ::uwvm2::runtime::lib::llvm_jit_runtime_trap>(builder, trap_type)};
    if(trap_callee == nullptr)
    {
        builder.restoreIP(original_insertion);
        error_block->eraseFromParent();
        continue_block->eraseFromParent();
        return false;
    }
    builder.restoreIP(original_insertion);
    ::llvm::Value* arguments[]{module_address, wide_kind, payload, ::llvm::ConstantInt::get(intptr, decoded.second), wide_raw};
    // [five real C uintptr_t arguments][uintptr_t status-only return]
    // [safe ] no scratch input/output pointer is passed. The original
    // synchronous membership/lease/lock setter is noexcept and does not poll.
    auto const status{apply_llvm_jit_host_calling_conv(builder.CreateCall(bridge_type, callee, arguments))};
    status->setDoesNotThrow();
    // [actual register-wide store status][one success guard]
    // [safe ] only status==ok reaches retirement. Every nonzero/unknown status
    // enters the cold classifier; no native pointer, field or token check is
    // omitted from the original store call above. Weights describe the static
    // success-path preference, not measured profile data.
    auto const succeeded{builder.CreateICmpEQ(status, ::llvm::ConstantInt::get(intptr, 0u),
        get_llvm_string_ref(u8"gc.struct.set32.succeeded"))};
    auto const dispatch{builder.CreateCondBr(succeeded, continue_block, error_block)};
    dispatch->setMetadata(::llvm::LLVMContext::MD_prof, ::llvm::MDNode::get(context, {
        ::llvm::MDString::get(context, "branch_weights"),
        ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(i32, 2000u)),
        ::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(i32, 1u))}));
    // [success block][error block insertion cursor]
    // [safe ] all status classification and explicit Win64 frame/stack reads
    // remain on the failed edge. This calls the original trap directly from
    // the generated frame, preserving its return-address/unwind identity.
    builder.SetInsertPoint(error_block);
    auto const failed{[&](storage::gc_object_status value)
    { return builder.CreateICmpEQ(status, ::llvm::ConstantInt::get(intptr, static_cast<::std::uintptr_t>(value))); }};
    using trap_kind = ::uwvm2::runtime::lib::llvm_jit_trap_kind;
    auto const trap_kind_type{trap_type->getParamType(0u)};
    auto const trap_constant{[&](trap_kind value)
    { return ::llvm::ConstantInt::get(trap_kind_type, static_cast<::std::uint_least64_t>(value)); }};
    auto const allocation_failed{builder.CreateOr(failed(storage::gc_object_status::out_of_memory),
        failed(storage::gc_object_status::size_overflow))};
    auto const generic_error{builder.CreateSelect(allocation_failed,
        trap_constant(trap_kind::gc_allocation_failure), trap_constant(trap_kind::runtime_invariant_failure))};
    auto const bounds_or_generic{builder.CreateSelect(failed(storage::gc_object_status::out_of_bounds),
        trap_constant(trap_kind::array_out_of_bounds), generic_error)};
    auto const selected_trap{builder.CreateSelect(failed(storage::gc_object_status::null_reference),
        trap_constant(trap_kind::null_reference), bounds_or_generic,
        get_llvm_string_ref(u8"gc.struct.set32.error.kind"))};
    auto const context_intptr{::llvm::cast<::llvm::IntegerType>(trap_type->getParamType(1u))};
    ::llvm::Value* trap_arguments[]{selected_trap,
        emit_llvm_jit_current_frame_address(builder, context_intptr),
        emit_llvm_jit_current_stack_pointer(builder, context_intptr)};
    auto const trap_call{apply_llvm_jit_host_calling_conv(builder.CreateCall(trap_type, trap_callee, trap_arguments))};
    trap_call->setTailCallKind(::llvm::CallInst::TCK_NoTail);
    emit_llvm_jit_memory_clobber(builder);
    builder.CreateUnreachable();
    // [terminated cold block][success insertion cursor]
    // [safe ] the only predecessor is the actual status==0 edge; operands
    // cannot retire after a failed setter or before its synchronous return.
    builder.SetInsertPoint(continue_block);
    // [SSA prefix][two copied input descriptors] end -> [SSA prefix] end
    // [safe ] only status-success can retire inputs. There is no result slot,
    // native byte borrow or descriptor alias across this vector mutation.
    stack.pop_back();
    stack.pop_back();
    return true;
}

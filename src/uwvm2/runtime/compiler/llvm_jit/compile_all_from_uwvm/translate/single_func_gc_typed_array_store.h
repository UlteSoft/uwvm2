// Numeric array.set after one authenticated borrow. No native object pointer
// crosses a call, callback, safepoint or debugger boundary. Reference writes
// retain their original barriers and foreign-lease handling in the runtime.
[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_typed_array_store(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded,
    runtime_operand_stack_value_type value_type) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    namespace gc_type = ::uwvm2::parser::wasm::standard::wasm3::type;
    using wasm_type = runtime_operand_stack_value_type;
    using ref = storage::gc_reference;
    static_assert(sizeof(::std::uintptr_t) == 8uz && ::std::endian::native == ::std::endian::little);
    static_assert(sizeof(storage::gc_object_value) == 16uz &&
                  offsetof(storage::gc_object_value, bits) == 0uz);
    auto& builder{*state.ir_builder};
    auto& stack{state.operand_stack};
    if(decoded.opcode != 14u || stack.size() < 3uz) { return false; }
    auto const operand{stack[stack.size() - 3uz]};
    auto const index{stack[stack.size() - 2uz]};
    auto const input{stack.back()};
    if(operand.type != wasm_type::funcref || operand.value == nullptr ||
       !operand.value->getType()->isIntegerTy(sizeof(ref) * CHAR_BIT) ||
       index.type != wasm_type::i32 || index.value == nullptr ||
       !index.value->getType()->isIntegerTy(32u) || input.type != value_type || input.value == nullptr ||
       (value_type == wasm_type::i32 ? !input.value->getType()->isIntegerTy(32u) :
        value_type != wasm_type::f32 || !input.value->getType()->isFloatTy()))
    { return false; }
    auto const* module{state.local_func_storage_ptr->runtime_module_ptr};
    auto const* field{module->gc_store->field_at(decoded.first, 0uz)};
    if(field == nullptr || !field->mutable_) { return false; }
    auto const width{field->storage.packed == gc_type::packed_kind::i8 ? 1uz :
        field->storage.packed == gc_type::packed_kind::i16 ? 2uz : 4uz};
    llvm_jit_gc_status_dispatch_plan dispatch{};
    if(!dispatch.prepare(*state.llvm_module, builder)) { return false; }
    auto const intptr{builder.getInt64Ty()};
    auto const i32{builder.getInt32Ty()};
    auto const ptrtype{::llvm::PointerType::getUnqual(builder.getContext())};
    auto const payload{builder.CreateTrunc(builder.CreateLShr(operand.value, offsetof(ref, storage) * CHAR_BIT), intptr)};
    auto const kind{builder.CreateTrunc(builder.CreateLShr(operand.value, offsetof(ref, kind) * CHAR_BIT), i32)};
    auto const name{get_llvm_runtime_module_object_symbol_name(*module)};
    auto const address{get_llvm_external_host_object_address(builder, reinterpret_cast<::std::uintptr_t>(module),
        ::uwvm2::utils::container::u8string_view{name.data(), name.size()})};
    if(address == nullptr) { return false; }
    auto const abi{::llvm::FunctionType::get(builder.getInt128Ty(), {intptr, i32, intptr, i32}, false)};
    ::llvm::Value* arguments[]{address, kind, payload, builder.getInt32(decoded.first)};
    auto const emit_access{[&](::llvm::Value* pair, bool local_packed) noexcept -> ::llvm::Value*
    {
        if(!local_packed) { dispatch.commit(*builder.CreateTrunc(builder.CreateLShr(pair, 64u), i32)); }
        auto const header{builder.CreateIntToPtr(builder.CreateTrunc(pair, intptr), ptrtype)};
        auto const member{[&](::std::size_t offset)
        { return builder.CreateGEP(builder.getInt8Ty(), header, builder.getInt64(offset)); }};
        ::llvm::Value* length{builder.CreateLoad(intptr, member(storage::gc_object_store::jit_array_length_offset))};
#if defined(UWVM_EXPERIMENTAL_ARRAY_LENGTH_VALUES) && UWVM_EXPERIMENTAL_ARRAY_LENGTH_VALUES == 1
        // Use a logical length associated with THIS complete reference SSA.
        // Authentication/status success still dominates the native load and
        // bounds check. This is not a cached object address or a new lease.
        // Unknown calls/PHIs/selects/null/local assignments carry valid=false
        // through the existing length-fact protocol and retain the header load.
        // Known allocation lengths become constants after mem2reg, allowing
        // LLVM to remove the unused load and prove bounded numeric indices.
        auto const fact{llvm_jit_find_array_length_value(state, operand.value)};
        if(fact.length != nullptr && fact.valid != nullptr &&
           fact.length->getType()->isIntegerTy(32u) && fact.valid->getType()->isIntegerTy(1u))
        {
            length = builder.CreateSelect(fact.valid, builder.CreateZExt(fact.length, intptr),
                length, "gc.array.bounds.length");
        }
#endif
        auto const wide_index{builder.CreateZExt(index.value, intptr)};
        // All failure paths precede lock acquisition; trap/unwind never leaks it.
        emit_llvm_conditional_trap(*state.llvm_module, builder, builder.CreateICmpUGE(wide_index, length),
            ::uwvm2::runtime::lib::llvm_jit_trap_kind::array_out_of_bounds);
        auto const data{builder.CreateLoad(ptrtype, member(storage::gc_object_store::jit_array_values_offset))};
        ::llvm::Value* numeric{builder.getTrue()};
        if(!local_packed)
        {
            auto const numeric_flag{builder.CreateLoad(builder.getInt8Ty(), member(storage::gc_object_store::jit_array_numeric_offset))};
            numeric = builder.CreateICmpNE(numeric_flag, builder.getInt8(0u));
        }
        auto const stride{builder.CreateSelect(numeric, builder.getInt64(width), builder.getInt64(sizeof(storage::gc_object_value)))};
        auto const element{builder.CreateGEP(builder.getInt8Ty(), data, builder.CreateMul(wide_index, stride))};
        auto const raw{value_type == wasm_type::f32 ? builder.CreateBitCast(input.value, i32) : input.value};
        auto const bits{width == 4uz ? raw : builder.CreateTrunc(raw, builder.getIntNTy(static_cast<unsigned>(width * CHAR_BIT)))};
        auto const flag{member(storage::gc_object_store::jit_array_lock_offset)};
        auto const lock{::llvm::BasicBlock::Create(builder.getContext(), get_llvm_string_ref(u8"gc.array.write.lock"), state.llvm_function)};
        auto const write{::llvm::BasicBlock::Create(builder.getContext(), get_llvm_string_ref(u8"gc.array.write.value"), state.llvm_function)};
        auto const packed_write{::llvm::BasicBlock::Create(builder.getContext(), get_llvm_string_ref(u8"gc.array.write.packed"), state.llvm_function)};
        auto const carrier_write{::llvm::BasicBlock::Create(builder.getContext(), get_llvm_string_ref(u8"gc.array.write.carrier"), state.llvm_function)};
        auto const release{::llvm::BasicBlock::Create(builder.getContext(), get_llvm_string_ref(u8"gc.array.write.release"), state.llvm_function)};
#if defined(UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK) && UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK == 1
        auto const private_access{llvm_jit_private_numeric_array_access(state, operand.value)};
        builder.CreateCondBr(private_access, write, lock);
#else
        builder.CreateBr(lock);
#endif
        builder.SetInsertPoint(lock);
        auto const old{builder.CreateAtomicRMW(::llvm::AtomicRMWInst::Xchg, flag, builder.getInt8(1u),
            ::llvm::Align(1u), ::llvm::AtomicOrdering::Acquire)};
        builder.CreateCondBr(builder.CreateICmpNE(old, builder.getInt8(0u)), lock, write);
        builder.SetInsertPoint(write);
        builder.CreateCondBr(numeric, packed_write, carrier_write);
        builder.SetInsertPoint(packed_write);
        builder.CreateAlignedStore(bits, element, ::llvm::Align(1u));
        builder.CreateBr(release);
        builder.SetInsertPoint(carrier_write);
        // Foreign arrays may use the legacy representation. Match pack() and
        // gc_object_value::i32(): truncated low bits plus twelve zero bytes, even
        // for f32 NaN payloads. A narrow store alone would leave stale upper bytes.
        builder.CreateAlignedStore(builder.CreateZExt(bits, builder.getInt128Ty()), element, ::llvm::Align(1u));
        builder.CreateBr(release);
        builder.SetInsertPoint(release);
#if defined(UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK) && UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK == 1
        auto const unlocked{::llvm::BasicBlock::Create(builder.getContext(),
            get_llvm_string_ref(u8"gc.array.write.unlock"), state.llvm_function)};
        auto const done{::llvm::BasicBlock::Create(builder.getContext(),
            get_llvm_string_ref(u8"gc.array.write.done"), state.llvm_function)};
        builder.CreateCondBr(private_access, done, unlocked);builder.SetInsertPoint(unlocked);
#endif
        auto const unlock{builder.CreateAlignedStore(builder.getInt8(0u), flag, ::llvm::Align(1u))};
        unlock->setAtomic(::llvm::AtomicOrdering::Release);
#if defined(UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK) && UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK == 1
        builder.CreateBr(done);builder.SetInsertPoint(done);
#endif
        return builder.getInt8(0u);
    }};
    auto const call{[&]<::std::size_t W, bool F>() noexcept -> ::llvm::Value*
    {
#if defined(UWVM_EXPERIMENTAL_INLINE_ARRAY_AUTH) && UWVM_EXPERIMENTAL_INLINE_ARRAY_AUTH == 1
        return emit_llvm_jit_gc_inline_array_access<W, true, F>(state, abi, arguments, emit_access);
#else
        auto const callee{get_llvm_runtime_bridge_function_symbol_value<llvm_jit_gc_typed_array_borrow_bridge<W, true, F>>(builder, abi)};
        if(callee == nullptr) { return nullptr; }
        auto const value{apply_llvm_jit_host_calling_conv(builder.CreateCall(abi, callee, arguments))};
        value->setDoesNotThrow();
        return emit_access(value, false);
#endif
    }};
    auto const pair{value_type == wasm_type::f32 ? call.template operator()<4uz, true>() :
        width == 1uz ? call.template operator()<1uz, false>() :
        width == 2uz ? call.template operator()<2uz, false>() : call.template operator()<4uz, false>()};
    if(pair == nullptr) { return false; }
    stack.pop_back();
    stack.pop_back();
    stack.pop_back();
    return true;
}

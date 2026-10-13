// x86_64 Linux Clang native ABI is bound by the recorded abi.ll witness.
// A canonical object borrow exists only between authentication and this load.
template<::std::size_t Width, bool Mutable, bool Float>
[[nodiscard]] inline unsigned __int128 llvm_jit_gc_typed_array_borrow_bridge(
    ::std::uintptr_t module_address, ::std::uint32_t reference_kind,
    ::std::uintptr_t opaque_payload, ::std::uint32_t expected_type) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    auto const failure{[](storage::gc_object_status status) noexcept
    { return static_cast<unsigned __int128>(status) << 64u; }};
    if(module_address == 0u) { return failure(storage::gc_object_status::invalid_value); }
    auto const* module{reinterpret_cast<storage::wasm_module_storage_t const*>(module_address)};
    auto* store{module->gc_store.get()};
    if(store == nullptr || !store->valid()) { return failure(storage::gc_object_status::invalid_store); }
    storage::gc_reference reference{};
    reference.kind = static_cast<::uwvm2::object::global::wasm_ref_kind>(reference_kind);
    reference.storage.ptr = reinterpret_cast<void*>(opaque_payload);
    return store->template jit_borrow_numeric_array32<Width, Mutable, Float>(reference, expected_type);
}
[[nodiscard]] inline bool llvm_jit_typed_array_load_eligible(
    runtime_local_func_llvm_jit_emit_state_t const& state) noexcept
{
    // Debug bridge wrappers can run callbacks; never borrow across those.
    return state.gc_array_access_ordinary_full &&
        state.emit_precise_gc_root_frames && !state.route_wasm_calls_through_runtime_bridge &&
        !state.emit_debug_safe_points && !state.debug_activation_enabled &&
        state.debug_full_patchable_typed_target_base_address == 0u &&
        state.pending_numeric_plan == nullptr &&
        state.checkpoint_plan == nullptr;
}
#if defined(UWVM_EXPERIMENTAL_INLINE_ARRAY_AUTH) && UWVM_EXPERIMENTAL_INLINE_ARRAY_AUTH == 1
#include "single_func_gc_inline_array_auth.h"
#endif
[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_gc_typed_array_load(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::validation::standard::wasm3::gc_instruction_immediate const& decoded,
    runtime_operand_stack_value_type result_type) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    namespace gc_type = ::uwvm2::parser::wasm::standard::wasm3::type;
    using wasm_type = runtime_operand_stack_value_type;
    using ref = storage::gc_reference;
    static_assert(sizeof(::std::uintptr_t) == 8uz && ::std::endian::native == ::std::endian::little);
    auto& builder{*state.ir_builder};auto& stack{state.operand_stack};
    auto const operand{stack[stack.size()-2uz]};auto const index{stack.back()};
    if(operand.type != wasm_type::funcref || operand.value == nullptr ||
       !operand.value->getType()->isIntegerTy(sizeof(ref)*CHAR_BIT) ||
       index.type != wasm_type::i32 || index.value == nullptr || !index.value->getType()->isIntegerTy(32u)) { return false; }
    auto const* module{state.local_func_storage_ptr->runtime_module_ptr};
    auto const* field{module->gc_store->field_at(decoded.first,0uz)};
    auto const width{field->storage.packed == gc_type::packed_kind::i8 ? 1uz :
        field->storage.packed == gc_type::packed_kind::i16 ? 2uz : 4uz};
    llvm_jit_gc_status_dispatch_plan dispatch{};
    if(!dispatch.prepare(*state.llvm_module,builder)) { return false; }
    auto const intptr{builder.getInt64Ty()};auto const i32{builder.getInt32Ty()};
    auto const ptrtype{::llvm::PointerType::getUnqual(builder.getContext())};
    auto const payload{builder.CreateTrunc(builder.CreateLShr(operand.value,offsetof(ref,storage)*CHAR_BIT),intptr)};
    auto const kind{builder.CreateTrunc(builder.CreateLShr(operand.value,offsetof(ref,kind)*CHAR_BIT),i32)};
    auto const name{get_llvm_runtime_module_object_symbol_name(*module)};
    auto const address{get_llvm_external_host_object_address(builder,reinterpret_cast<::std::uintptr_t>(module),
        ::uwvm2::utils::container::u8string_view{name.data(),name.size()})};
    if(address == nullptr) { return false; }
    auto const abi{::llvm::FunctionType::get(builder.getInt128Ty(),{intptr,i32,intptr,i32},false)};
    ::llvm::Value* arguments[]{address,kind,payload,builder.getInt32(decoded.first)};
    auto const emit_access{[&](::llvm::Value* pair, bool local_packed) noexcept -> ::llvm::Value*
    {
        if(!local_packed)
        {
            auto const status{builder.CreateTrunc(builder.CreateLShr(pair,64u),i32)};
            dispatch.commit(*status);
        }
        auto const header{builder.CreateIntToPtr(builder.CreateTrunc(pair,intptr),ptrtype)};
        auto const member{[&](::std::size_t offset)
        { return builder.CreateGEP(builder.getInt8Ty(),header,builder.getInt64(offset)); }};
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
        auto const wide_index{builder.CreateZExt(index.value,intptr)};
        emit_llvm_conditional_trap(*state.llvm_module,builder,builder.CreateICmpUGE(wide_index,length),
            ::uwvm2::runtime::lib::llvm_jit_trap_kind::array_out_of_bounds);
        auto const data{builder.CreateLoad(ptrtype,member(storage::gc_object_store::jit_array_values_offset))};
        ::llvm::Value* stride{builder.getInt64(width)};
        if(!local_packed)
        {
            auto const numeric_flag{builder.CreateLoad(builder.getInt8Ty(),member(storage::gc_object_store::jit_array_numeric_offset))};
            stride = builder.CreateSelect(builder.CreateICmpNE(numeric_flag,builder.getInt8(0u)),
                builder.getInt64(width),builder.getInt64(sizeof(storage::gc_object_value)));
        }
        auto const element{builder.CreateGEP(builder.getInt8Ty(),data,builder.CreateMul(wide_index,stride))};
        auto const flag{member(storage::gc_object_store::jit_array_lock_offset)};
        if(field->mutable_)
        {
            auto const lock{::llvm::BasicBlock::Create(builder.getContext(),get_llvm_string_ref(u8"gc.array.read.lock"),state.llvm_function)};
            auto const read{::llvm::BasicBlock::Create(builder.getContext(),get_llvm_string_ref(u8"gc.array.read.value"),state.llvm_function)};
#if defined(UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK) && UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK == 1
            auto const private_access{llvm_jit_private_numeric_array_access(state, operand.value)};
            builder.CreateCondBr(private_access, read, lock);
#else
            builder.CreateBr(lock);
#endif
            builder.SetInsertPoint(lock);
            auto const old{builder.CreateAtomicRMW(::llvm::AtomicRMWInst::Xchg,flag,builder.getInt8(1u),::llvm::Align(1u),::llvm::AtomicOrdering::Acquire)};
            builder.CreateCondBr(builder.CreateICmpNE(old,builder.getInt8(0u)),lock,read);builder.SetInsertPoint(read);
        }
        // Alignment1 matches memcpy: packed byte tails and legacy carriers both work.
        auto const bits{builder.CreateAlignedLoad(builder.getIntNTy(static_cast<unsigned>(width*CHAR_BIT)),element,::llvm::Align(1u))};
        if(field->mutable_)
        {
#if defined(UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK) && UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK == 1
            auto const unlocked{::llvm::BasicBlock::Create(builder.getContext(),
                get_llvm_string_ref(u8"gc.array.read.unlock"), state.llvm_function)};
            auto const done{::llvm::BasicBlock::Create(builder.getContext(),
                get_llvm_string_ref(u8"gc.array.read.done"), state.llvm_function)};
            auto const private_release{llvm_jit_private_numeric_array_access(state, operand.value)};
            builder.CreateCondBr(private_release, done, unlocked);builder.SetInsertPoint(unlocked);
#endif
            auto const release{builder.CreateAlignedStore(builder.getInt8(0u),flag,::llvm::Align(1u))};
            release->setAtomic(::llvm::AtomicOrdering::Release);
#if defined(UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK) && UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK == 1
            builder.CreateBr(done);builder.SetInsertPoint(done);
#endif
        }
        auto const raw{width == 4uz ? static_cast<::llvm::Value*>(bits) : decoded.opcode == 12u ?
            builder.CreateSExt(bits,i32) : builder.CreateZExt(bits,i32)};
        return result_type == wasm_type::f32 ? builder.CreateBitCast(raw,builder.getFloatTy()) : raw;
    }};
    auto const call{[&]<::std::size_t W, bool M, bool F>() noexcept -> ::llvm::Value*
    {
#if defined(UWVM_EXPERIMENTAL_INLINE_ARRAY_AUTH) && UWVM_EXPERIMENTAL_INLINE_ARRAY_AUTH == 1
        return emit_llvm_jit_gc_inline_array_access<W, M, F>(state, abi, arguments, emit_access);
#else
        auto const callee{get_llvm_runtime_bridge_function_symbol_value<llvm_jit_gc_typed_array_borrow_bridge<W,M,F>>(builder,abi)};
        if(callee == nullptr) { return nullptr; }
        auto const value{apply_llvm_jit_host_calling_conv(builder.CreateCall(abi,callee,arguments))};
        value->setDoesNotThrow();return emit_access(value, false);
#endif
    }};
    auto const numeric{[&]<bool M>() noexcept -> ::llvm::Value*
    {
        if(result_type == wasm_type::f32) { return call.template operator()<4uz,M,true>(); }
        if(width == 1uz) { return call.template operator()<1uz,M,false>(); }
        if(width == 2uz) { return call.template operator()<2uz,M,false>(); }
        return call.template operator()<4uz,M,false>();
    }};
    auto const value{field->mutable_ ? numeric.template operator()<true>() : numeric.template operator()<false>()};
    if(value == nullptr) { return false; }
    stack.pop_back();stack.pop_back();stack.push_back({.type=result_type,.value=value});return true;
}

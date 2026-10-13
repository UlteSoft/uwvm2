// Index-domain acquisition carries no object pointer, allocates nothing and
// cannot invoke a guest callback. Resolve it once per genuine full activation;
// cached code still relocates the original runtime-module symbol at load time.
[[nodiscard]] inline unsigned __int128 llvm_jit_gc_numeric_array_index_domain_bridge(
    ::std::uintptr_t module_address) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    if(module_address == 0u) { return 0u; }
    auto const* module{reinterpret_cast<storage::wasm_module_storage_t const*>(module_address)};
    auto const* store{module->gc_store.get()};
    return store == nullptr ? 0u : store->jit_numeric_array_index_domain();
}

// All arrays of the exact local numeric type are packed by allocate() and
// allocate_general_slab() under this build policy. Subtypes and foreign stores
// keep the checked bridge and their actual representation. Specialize before
// merging control flow, so the optimizer sees a constant local element stride.
// Keep the original merged lowering for i8/i16: native measurements found that
// duplicating their access blocks lengthened live ranges and spilled loop state.
// The 32-bit access specialization is qualified independently of those paths.
#if !defined(UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS) || UWVM_EXPERIMENTAL_PACKED_NUMERIC_ARRAYS != 1
#error "inline numeric array access requires the packed allocator in every translation unit"
#endif
template<::std::size_t Width, bool Mutable, bool Float, typename EmitAccess>
[[nodiscard]] inline ::llvm::Value* emit_llvm_jit_gc_inline_array_access(
    runtime_local_func_llvm_jit_emit_state_t& state, ::llvm::FunctionType* abi,
    ::llvm::Value* (&arguments)[4], EmitAccess const& emit_access) noexcept
{
    namespace storage = ::uwvm2::uwvm::runtime::storage;
    namespace gc_type = ::uwvm2::parser::wasm::standard::wasm3::type;
    auto& builder{*state.ir_builder};
    auto const intptr{builder.getInt64Ty()};
    auto const ptrtype{::llvm::PointerType::getUnqual(builder.getContext())};
    auto const callee{get_llvm_runtime_bridge_function_symbol_value<
        llvm_jit_gc_typed_array_borrow_bridge<Width, Mutable, Float>>(builder, abi)};
    if(callee == nullptr) { return nullptr; }
    if(state.gc_numeric_array_index_domain == nullptr)
    {
        auto& entry{state.llvm_function->getEntryBlock()};
        ::llvm::IRBuilder<> entry_builder{&entry, entry.getFirstInsertionPt()};
        auto const* module{state.local_func_storage_ptr->runtime_module_ptr};
        auto const name{get_llvm_runtime_module_object_symbol_name(*module)};
        auto const address{get_llvm_external_host_object_address(entry_builder,
            reinterpret_cast<::std::uintptr_t>(module),
            ::uwvm2::utils::container::u8string_view{name.data(), name.size()})};
        auto const domain_abi{::llvm::FunctionType::get(builder.getInt128Ty(), {intptr}, false)};
        auto const domain_callee{get_llvm_runtime_bridge_function_symbol_value<
            llvm_jit_gc_numeric_array_index_domain_bridge>(entry_builder, domain_abi)};
        if(address == nullptr || domain_callee == nullptr) { return nullptr; }
        auto const domain{apply_llvm_jit_host_calling_conv(
            entry_builder.CreateCall(domain_abi, domain_callee, {address}))};
        domain->setDoesNotThrow();
        state.gc_numeric_array_index_domain = domain;
    }
    auto const domain{state.gc_numeric_array_index_domain};
    auto const owner{builder.CreateTrunc(domain, intptr)};
    auto const table{builder.CreateTrunc(builder.CreateLShr(domain, 64u), intptr)};
    auto const payload{arguments[2]};
    auto const make_block{[&](::llvm::StringRef name)
    { return ::llvm::BasicBlock::Create(builder.getContext(), name, state.llvm_function); }};
    auto const probe{make_block("gc.array.auth.probe")};
    auto const inspect{make_block("gc.array.auth.inspect")};
    auto const next{make_block("gc.array.auth.next")};
    auto const layout{make_block("gc.array.auth.layout")};
    auto const fast{make_block("gc.array.auth.local")};
    auto const slow{make_block("gc.array.auth.checked")};
    auto const done{make_block("gc.array.auth.done")};
    auto const nonzero{[&](::llvm::Value* value)
    { return builder.CreateICmpNE(value, ::llvm::Constant::getNullValue(value->getType())); }};
    auto const valid{builder.CreateAnd(builder.CreateAnd(nonzero(owner), nonzero(table)),
        builder.CreateAnd(nonzero(payload), builder.CreateICmpEQ(arguments[1], builder.getInt32(
            static_cast<unsigned>(::uwvm2::object::global::wasm_ref_kind::wasm_array)))))};
    builder.CreateCondBr(valid, probe, slow);
    builder.SetInsertPoint(probe);
    // Exact bucket_index(): operate on the opaque identity only, never address it.
    auto const mixed17{builder.CreateXor(payload, builder.CreateLShr(payload, 17u))};
    auto const mixed9{builder.CreateXor(mixed17, builder.CreateLShr(mixed17, 9u))};
    auto const bucket{builder.CreateAnd(mixed9, builder.getInt64(storage::gc_object_store::jit_array_bucket_mask))};
    auto const buckets{builder.CreateIntToPtr(table, ptrtype)};
    auto const head_address{builder.CreateGEP(ptrtype, buckets, bucket)};
    auto const head{builder.CreateAlignedLoad(ptrtype, head_address, ::llvm::Align(sizeof(void*)))};
    head->setAtomic(::llvm::AtomicOrdering::Acquire);
    builder.CreateCondBr(nonzero(head), inspect, slow);
    builder.SetInsertPoint(inspect);
    auto const node{builder.CreatePHI(ptrtype, 2u, "gc.array.auth.member")};
    auto const depth{builder.CreatePHI(builder.getInt32Ty(), 2u)};
    node->addIncoming(head, probe); depth->addIncoming(builder.getInt32(0u), probe);
    auto const field{[&](::std::size_t offset)
    { return builder.CreateGEP(builder.getInt8Ty(), node, builder.getInt64(offset)); }};
    auto const token{builder.CreateLoad(intptr, field(storage::gc_object_store::jit_array_token_offset))};
    builder.CreateCondBr(builder.CreateICmpEQ(token, payload), layout, next);
    builder.SetInsertPoint(next);
    // At most eight proved native nodes. Collisions beyond that bound use the
    // complete checked chain, preserving foreign leases and every status code.
    auto const successor{builder.CreateLoad(ptrtype, field(storage::gc_object_store::jit_array_hash_next_offset))};
    auto const next_depth{builder.CreateAdd(depth, builder.getInt32(1u))};
    builder.CreateCondBr(builder.CreateAnd(nonzero(successor), builder.CreateICmpULT(next_depth, builder.getInt32(8u))),
        inspect, slow);
    node->addIncoming(successor, next); depth->addIncoming(next_depth, next);
    builder.SetInsertPoint(layout);
    auto const actual_owner{builder.CreateLoad(intptr, field(storage::gc_object_store::jit_array_owner_offset))};
    auto const actual_type{builder.CreateLoad(builder.getInt32Ty(), field(storage::gc_object_store::jit_array_type_offset))};
    auto const actual_kind{builder.CreateLoad(builder.getInt32Ty(), field(storage::gc_object_store::jit_array_kind_offset))};
    auto const exact{builder.CreateAnd(builder.CreateICmpEQ(actual_owner, owner),
        builder.CreateAnd(builder.CreateICmpEQ(actual_type, arguments[3]),
            builder.CreateICmpEQ(actual_kind, builder.getInt32(static_cast<unsigned>(gc_type::composite_kind::array)))))};
    builder.CreateCondBr(exact, fast, slow);
    builder.SetInsertPoint(fast);
    auto const authenticated{builder.CreateZExt(builder.CreatePtrToInt(node, intptr), builder.getInt128Ty())};
    auto const local_value{[&]() noexcept -> ::llvm::Value*
    {
        if constexpr(Width == 4uz) { return emit_access(authenticated, true); }
        else { return authenticated; }
    }()};
    if(local_value == nullptr) { return nullptr; }
    auto const local_end{builder.GetInsertBlock()};
    builder.CreateBr(done);
    builder.SetInsertPoint(slow);
    auto const checked{apply_llvm_jit_host_calling_conv(builder.CreateCall(abi, callee, arguments))};
    checked->setDoesNotThrow();
    auto const checked_value{[&]() noexcept -> ::llvm::Value*
    {
        if constexpr(Width == 4uz) { return emit_access(checked, false); }
        else { return checked; }
    }()};
    if(checked_value == nullptr) { return nullptr; }
    auto const checked_end{builder.GetInsertBlock()};
    builder.CreateBr(done);
    builder.SetInsertPoint(done);
    auto const result{builder.CreatePHI(local_value->getType(), 2u)};
    result->addIncoming(local_value, local_end);
    result->addIncoming(checked_value, checked_end);
    if constexpr(Width == 4uz) { return result; }
    else { return emit_access(result, false); }
}

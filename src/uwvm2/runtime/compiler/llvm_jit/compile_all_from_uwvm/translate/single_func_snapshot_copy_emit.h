#pragma once
// Compiler-private snapshot copies. Include after the actual emit state and
// entry-alloca helper. An entry-owned pointer table with a bounded activation
// budget lets cached noinline bodies serve every opcode instead of cloning loads.
// Neither the table nor the helper is a Wasm/ASM/native-address capability.
inline constexpr ::std::size_t llvm_jit_snapshot_copy_local_limit{256u};

[[nodiscard]] inline bool prepare_runtime_local_func_llvm_jit_snapshot_local_pointers(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t count) noexcept
{
    if(count == 0u || state.llvm_function == nullptr || state.llvm_module == nullptr || state.ir_builder == nullptr ||
       state.llvm_function->getParent() != state.llvm_module || count > state.local_pointers.size() ||
       state.local_pointers.size() != state.local_types.size()) { return false; }
    bool const heap_table{state.checkpoint_observer_workspace != nullptr};
    auto const table_count{heap_table ? state.local_pointers.size() :
        (::std::min)(state.local_pointers.size(),llvm_jit_snapshot_copy_local_limit)};
    if(count > table_count || table_count > static_cast<::std::size_t>((::std::numeric_limits<unsigned>::max)()))
    { return false; }
    auto& context{state.llvm_function->getContext()};
    for(::std::size_t i{}; i != table_count; ++i)
    {
        auto const pointer{state.local_pointers.index_unchecked(i)};
        auto const type{get_llvm_type_from_wasm_value_type(context,state.local_types.index_unchecked(i))};
        if(pointer == nullptr || type == nullptr || pointer->getFunction() != state.llvm_function ||
           !pointer->isStaticAlloca() || pointer->isArrayAllocation() || pointer->getAllocatedType() != type)
        { return false; }
    }
    auto const array{::llvm::ArrayType::get(::llvm::PointerType::getUnqual(context),table_count)};
    auto const heap_owner{::llvm::dyn_cast_or_null<::llvm::IntToPtrInst>(state.checkpoint_observer_workspace)};
    auto const allocation{state.checkpoint_observer_workspace_allocation};
    auto const extent{allocation != nullptr && allocation->arg_size() == 1u ?
        ::llvm::dyn_cast<::llvm::ConstantInt>(allocation->getArgOperand(0u)) : nullptr};
    if(heap_table && (heap_owner == nullptr || allocation == nullptr || extent == nullptr ||
        heap_owner->getFunction() != state.llvm_function || allocation->getFunction() != state.llvm_function ||
        heap_owner->getParent() != ::std::addressof(state.llvm_function->getEntryBlock()) ||
        allocation->getParent() != heap_owner->getParent() || heap_owner->getOperand(0u) != allocation ||
        !allocation->comesBefore(heap_owner) ||
        !::uwvm2::runtime::checkpoint::observer_workspace_fits(table_count,table_count) ||
        extent->getZExtValue() < table_count *
            (::uwvm2::runtime::checkpoint::observer_local_metadata_bytes +
             ::uwvm2::runtime::checkpoint::native_slot_bytes))) { return false; }
    if(state.snapshot_local_pointers != nullptr)
    {
        if(state.snapshot_local_pointers_count != table_count) { return false; }
        if(heap_table)
        {
            auto const table{::llvm::dyn_cast<::llvm::GetElementPtrInst>(state.snapshot_local_pointers)};
            auto const offset{table != nullptr && table->getNumIndices() == 1u ?
                ::llvm::dyn_cast<::llvm::ConstantInt>(table->getOperand(1u)) : nullptr};
            return table != nullptr && table->getFunction() == state.llvm_function && table->getParent() == heap_owner->getParent() && table->isInBounds() &&
                table->getPointerOperand() == heap_owner && table->getSourceElementType()->isIntegerTy(8u) &&
                offset != nullptr && offset->getZExtValue() == table_count * 2u;
        }
        auto const table{::llvm::dyn_cast<::llvm::AllocaInst>(state.snapshot_local_pointers)};
        return table != nullptr && table->getFunction() == state.llvm_function && table->isStaticAlloca() &&
            !table->isArrayAllocation() && table->getAllocatedType() == array;
    }
    // Complete declarations use private storage inside the bounded observer
    // allocation; normal legacy/public prefixes retain a <=256 entry table.
    auto& entry{state.llvm_function->getEntryBlock()};
    ::llvm::IRBuilder<> initialize{context};
    auto const terminator{!entry.empty() && entry.back().isTerminator() ? ::std::addressof(entry.back()) : nullptr};
    if(terminator != nullptr) { initialize.SetInsertPoint(terminator); }
    else { initialize.SetInsertPoint(::std::addressof(entry)); }
    ::llvm::Value* table{};
    if(heap_table)
    {
        table = initialize.CreateInBoundsGEP(initialize.getInt8Ty(),heap_owner,
            initialize.getInt64(table_count * 2u),"debug.private.local.addresses");
    }
    else
    {
        table = create_llvm_jit_entry_block_alloca(*state.ir_builder,array,nullptr,
            get_llvm_string_ref(u8"debug.private.local.addresses"));
    }
    if(table == nullptr) { return false; }
    // Initialize before ordinary/resumed entry dispatch. Reading an address
    // never reads an uninitialized nondefaultable local's value. Heap table
    // alignment follows two N-byte flag ranges and can be just one byte.
    for(::std::size_t i{}; i != table_count; ++i)
    {
        auto const cell{initialize.CreateInBoundsGEP(array,table,{initialize.getInt32(0u),initialize.getInt32(static_cast<unsigned>(i))})};
        initialize.CreateStore(state.local_pointers.index_unchecked(i),cell)->setAlignment(::llvm::Align{1u});
    }
    state.snapshot_local_pointers = table; state.snapshot_local_pointers_count = table_count;
    return true;
}

[[nodiscard]] inline ::llvm::Function* prepare_runtime_local_func_llvm_jit_snapshot_copy(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t count, bool public_locals) noexcept
{
    if(!prepare_runtime_local_func_llvm_jit_snapshot_local_pointers(state,count)) { return nullptr; }
    // Numeric declarations have identical public/raw bit copies. Reuse ONE
    // body for both packets; references retain their separate nullness/raw ABI.
    bool shared_numeric{true};
    for(::std::size_t i{}; i != count; ++i)
    {
        auto const kind{get_runtime_wasm_value_type_encoding(state.local_types.index_unchecked(i))};
        if(kind != get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type::i32) &&
           kind != get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type::i64) &&
           kind != get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type::f32) &&
           kind != get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type::f64) &&
           kind != get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type::v128))
        { shared_numeric = false; break; }
    }
    auto& cached{public_locals ? state.debug_local_snapshot_copy : state.checkpoint_local_snapshot_copy};
    auto& cached_count{public_locals ? state.debug_local_snapshot_copy_count : state.checkpoint_local_snapshot_copy_count};
    auto& context{state.llvm_function->getContext()};
    auto const expected_kind{shared_numeric ? "numeric" : (public_locals ? "public-reference" : "raw-reference")};
    auto const pointer_type{::llvm::PointerType::getUnqual(context)};
    auto const signature{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context),
        {pointer_type,pointer_type,pointer_type,pointer_type},false)};
    if(cached != nullptr)
    {
        return cached_count == count && cached->getParent() == state.llvm_module && cached->getFunctionType() == signature &&
               cached->hasLocalLinkage() && cached->hasFnAttribute(::llvm::Attribute::NoInline) &&
               cached->hasFnAttribute("uwvm.debug.private.local-copy") && cached->getSubprogram() == nullptr &&
               cached->hasFnAttribute("uwvm.debug.private.local-copy.kind") &&
               cached->getFnAttribute("uwvm.debug.private.local-copy.kind").getValueAsString() == expected_kind ?
               cached : nullptr;
    }
    // Validate every physical byte extent before creating any function/body.
    // The C++ compiler ABI is not a semantic reference/nullability oracle.
    for(::std::size_t i{}; i != count; ++i)
    {
        auto const width{get_runtime_wasm_value_type_abi_size(state.local_types.index_unchecked(i))};
        if(width == 0u || width > ::uwvm2::runtime::lib::details::llvm_jit_debug_local_slot_bytes) { return nullptr; }
    }
    auto const name{::llvm::Twine{state.llvm_function->getName()} +
        (shared_numeric ? ".uwvm.private.numeric.locals" :
            (public_locals ? ".uwvm.private.debug.locals" : ".uwvm.private.checkpoint.locals")) +
        ".count." + ::llvm::Twine{static_cast<::std::uint64_t>(count)}};
    auto const function{::llvm::Function::Create(signature,::llvm::GlobalValue::InternalLinkage,name,state.llvm_module)};
    if(function == nullptr) { return nullptr; }
    function->addFnAttr(::llvm::Attribute::NoInline); function->setDoesNotThrow();
    function->addFnAttr("uwvm.debug.private.local-copy");
    function->addFnAttr("uwvm.debug.private.local-copy.kind",expected_kind);
    // No DISubprogram, Wasm provenance, activation or safepoint is attached:
    // instrumentation bodies must remain outside public ASM stepping ranges.
    ::llvm::IRBuilder<> copy{::llvm::BasicBlock::Create(context,"entry",function)};
    auto const values{function->getArg(0u)}, saved_flags{function->getArg(1u)};
    auto const addresses{function->getArg(2u)}, available_flags{function->getArg(3u)};
    auto const table_type{::llvm::ArrayType::get(pointer_type,state.snapshot_local_pointers_count)};
    auto const clear_type{::llvm::ArrayType::get(copy.getInt8Ty(),
        ::uwvm2::runtime::lib::details::llvm_jit_debug_local_slot_bytes)};
    // A false flag selects this immutable zero storage BEFORE the value load.
    // Entry-owned table cells contain addresses even for uninitialized locals;
    // reading an address does not read that local's value. Each leaf is straight-line:
    // no absent/invalid flag can cause an absent value to be dereferenced.
    auto const zero{new ::llvm::GlobalVariable(*state.llvm_module,clear_type,true,
        ::llvm::GlobalValue::PrivateLinkage,::llvm::ConstantAggregateZero::get(clear_type),
        ::llvm::Twine{function->getName()} + ".zero")};
    zero->setUnnamedAddr(::llvm::GlobalValue::UnnamedAddr::Global); zero->setAlignment(::llvm::Align{1u});
    auto const emit_range{[&](::llvm::IRBuilder<>& copy, ::llvm::Function* owner,
        ::std::size_t first, ::std::size_t last) noexcept
    {
        auto const values{owner->getArg(0u)}, saved_flags{owner->getArg(1u)};
        auto const addresses{owner->getArg(2u)}, available_flags{owner->getArg(3u)};
        for(::std::size_t i{first}; i != last; ++i)
        {
            auto const flag_cell{copy.CreateInBoundsGEP(copy.getInt8Ty(),available_flags,copy.getInt64(i))};
            auto const flag{copy.CreateLoad(copy.getInt8Ty(),flag_cell)}; flag->setAlignment(::llvm::Align{1u});
            auto const saved_cell{copy.CreateInBoundsGEP(copy.getInt8Ty(),saved_flags,copy.getInt64(i))};
            copy.CreateStore(flag,saved_cell)->setAlignment(::llvm::Align{1u});
            auto const slot{copy.CreateInBoundsGEP(copy.getInt8Ty(),values,
                copy.getInt64(i * ::uwvm2::runtime::lib::details::llvm_jit_debug_local_slot_bytes))};
            copy.CreateStore(::llvm::ConstantAggregateZero::get(clear_type),slot)->setAlignment(::llvm::Align{1u});
            auto const available{copy.CreateICmpEQ(flag,copy.getInt8(1u))};
            auto const address_cell{copy.CreateInBoundsGEP(table_type,addresses,{copy.getInt32(0u),copy.getInt32(static_cast<unsigned>(i))})};
            auto const address{copy.CreateLoad(pointer_type,address_cell)}; address->setAlignment(::llvm::Align{1u});
            auto const readable_address{copy.CreateSelect(available,address,zero)};
            auto const local_type{state.local_types.index_unchecked(i)};
            auto const native_type{get_llvm_type_from_wasm_value_type(context,local_type)};
            auto const kind{get_runtime_wasm_value_type_encoding(local_type)};
            if(public_locals && native_type->isIntegerTy() &&
               (kind == get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type::funcref) ||
                kind == get_runtime_wasm_value_type_encoding(runtime_operand_stack_value_type::externref)))
            {
                auto const value{copy.CreateLoad(native_type,readable_address)}; value->setAlignment(::llvm::Align{1u});
                auto const is_null{copy.CreateICmpEQ(value,::llvm::ConstantInt::get(native_type,0u))};
                copy.CreateStore(copy.CreateZExt(copy.CreateAnd(available,is_null),copy.getInt8Ty()),slot)->setAlignment(::llvm::Align{1u});
            }
            else
            {
                // Inline byte copies preserve scalar/v128 bits, including sNaNs.
                // No float argument/return or external memcpy call crosses an ABI.
                copy.CreateMemCpyInline(slot,::llvm::Align{1u},readable_address,::llvm::Align{1u},
                    copy.getInt64(get_runtime_wasm_value_type_abi_size(local_type)));
            }
        }
    }};
    if(count <= llvm_jit_snapshot_copy_local_limit)
    { emit_range(copy,function,0u,count); }
    else
    {
        // Bound each private copy block's lowering work, including at O0.
        // Entry capture and packet clearing must be bounded separately.
        // Each private leaf uses original indices and copies real live bytes;
        // the per-opcode caller still has just one cached wrapper invocation.
        for(::std::size_t first{}; first != count;)
        {
            auto const last{first + (::std::min)(count-first,llvm_jit_snapshot_copy_local_limit)};
            auto const part{::llvm::Function::Create(signature,::llvm::GlobalValue::InternalLinkage,
                ::llvm::Twine{function->getName()} + ".chunk." + ::llvm::Twine{static_cast<::std::uint64_t>(first)},state.llvm_module)};
            part->addFnAttr(::llvm::Attribute::NoInline); part->addFnAttr(::llvm::Attribute::NoMerge); part->setDoesNotThrow();
            part->addFnAttr("uwvm.debug.private.local-copy");
            part->addFnAttr("uwvm.debug.private.local-copy.kind",expected_kind);
            ::llvm::IRBuilder<> chunk{::llvm::BasicBlock::Create(context,"entry",part)};
            emit_range(chunk,part,first,last); chunk.CreateRetVoid();
            copy.CreateCall(part,{values,saved_flags,addresses,available_flags})->setDoesNotThrow();
            first = last;
        }
    }
    copy.CreateRetVoid(); cached = function; cached_count = count;
    auto const public_count{(::std::min)(state.local_types.size(),llvm_jit_snapshot_copy_local_limit)};
    // Sharing is safe only if the public prefix and complete packet have the
    // same extent. A 256-slot destination must NEVER receive a 10000-slot copy.
    if(shared_numeric && public_count == state.local_types.size() && count == public_count)
    {
        state.debug_local_snapshot_copy = function; state.debug_local_snapshot_copy_count = count;
        state.checkpoint_local_snapshot_copy = function; state.checkpoint_local_snapshot_copy_count = count;
    }
    return function;
}

[[nodiscard]] inline bool emit_runtime_local_func_llvm_jit_snapshot_copy(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t count,
    ::llvm::Value* values, ::llvm::Value* saved_flags, ::llvm::Value* available_flags,
    bool public_locals) noexcept
{
    if(values == nullptr || saved_flags == nullptr || available_flags == nullptr ||
       !values->getType()->isPointerTy() || !saved_flags->getType()->isPointerTy() || !available_flags->getType()->isPointerTy())
    { return false; }
    auto const function{prepare_runtime_local_func_llvm_jit_snapshot_copy(state,count,public_locals)};
    if(function == nullptr) { return false; }
    auto const call{state.ir_builder->CreateCall(function,{values,saved_flags,state.snapshot_local_pointers,available_flags})};
    if(call == nullptr) { return false; }
    call->setDoesNotThrow(); return true;
}

[[nodiscard]] inline ::llvm::GlobalVariable* prepare_runtime_local_func_llvm_jit_debug_snapshot_flags(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t count) noexcept
{
    if(state.ir_builder == nullptr || state.llvm_module == nullptr || count == 0u ||
       count > llvm_jit_snapshot_copy_local_limit || count > state.local_types.size() ||
       state.debug_local_initialization.context == nullptr || state.debug_local_initialization.initialized == nullptr)
    { return nullptr; }
    ::llvm::SmallVector<::std::uint8_t,llvm_jit_snapshot_copy_local_limit> flags{};
    for(::std::size_t i{}; i != count; ++i)
    {
        bool initialized{};
        if(!state.debug_local_initialization.initialized(state.debug_local_initialization.context,i,initialized)) { return nullptr; }
        flags.push_back(initialized ? 1u : 0u);
    }
    auto const bytes{::llvm::ConstantDataArray::get(state.ir_builder->getContext(),flags)};
    auto const found{state.debug_local_snapshot_flags.find(bytes)};
    if(found != state.debug_local_snapshot_flags.end()) { return found->second; }
    auto const global{new ::llvm::GlobalVariable(*state.llvm_module,bytes->getType(),true,
        ::llvm::GlobalValue::PrivateLinkage,bytes,"uwvm.private.debug.local.flags")};
    global->setUnnamedAddr(::llvm::GlobalValue::UnnamedAddr::Global); global->setAlignment(::llvm::Align{1u});
    state.debug_local_snapshot_flags[bytes] = global; return global;
}

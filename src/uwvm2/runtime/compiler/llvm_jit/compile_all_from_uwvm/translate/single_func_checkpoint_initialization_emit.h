#pragma once
// Included after the actual LLVM emit state and entry-alloca helpers.
// Selected only by the opt-in checkpoint compiler profile. Flags reflect
// executed local value stores, never validation-proof
// checkpoint rollback. Ordinary engines create no flag IR.
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_checkpoint_initialization_flags(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t actual_parameter_count,
    ::llvm::AllocaInst*& actual_flags) noexcept
{
    actual_flags = nullptr;
    if(state.checkpoint_plan == nullptr) { return true; }
    namespace checkpoint = ::uwvm2::runtime::checkpoint;
    auto const plan{state.checkpoint_plan};
    if(plan->producer_availability != ::uwvm2::runtime::checkpoint::status::ok) { return true; }
    if(!plan->profile || plan->compiler_failure != checkpoint::status::ok || plan->sites.empty() ||
       state.ir_builder == nullptr || state.llvm_function == nullptr || state.ir_builder->GetInsertBlock() == nullptr ||
       state.ir_builder->GetInsertBlock()->getParent() != state.llvm_function) { return false; }
    auto const& entry{plan->sites.front()};
    if(!(checkpoint_observer_workspace_selected(state) ?
         checkpoint::observer_workspace_fits(entry.local_count, entry.local_count) :
         checkpoint::native_workspace_fits(entry.local_count, entry.local_count)))
    { plan->producer_availability = checkpoint::status::quota_exceeded; return true; }
    if(checkpoint::validate_site(entry, *plan) != checkpoint::status::ok ||
       entry.phase != checkpoint::frame_phase::before_opcode || entry.operand_count != 0u ||
       entry.saved_parameter_count != 0u || entry.slots.size() != entry.local_count ||
       entry.local_count != state.local_pointers.size() || actual_parameter_count > entry.local_count ||
       entry.local_count > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)())) { return false; }
    for(::std::size_t i{}; i != entry.local_count; ++i)
    {
        auto const& local{entry.slots[i]};
        bool const expected{i < actual_parameter_count || local.type.kind != checkpoint::types::value_kind::reference || local.type.nullable};
        if(local.initialized != expected) { return false; }
    }
    if(entry.local_count == 0u) { return true; }
    auto& builder{*state.ir_builder};
    if(checkpoint_observer_workspace_selected(state))
    {
        auto const flags{state.checkpoint_observer_workspace};
        if(flags == nullptr) { return false; }
        builder.CreateMemSet(flags, builder.getInt8(0u), entry.local_count, ::llvm::MaybeAlign{1u});
        for(::std::size_t i{}; i != entry.local_count; ++i)
        {
            if(!entry.slots[i].initialized) { continue; }
            auto const cell{builder.CreateInBoundsGEP(builder.getInt8Ty(), flags, builder.getInt64(i))};
            builder.CreateStore(builder.getInt8(1u), cell)->setAlignment(::llvm::Align{1u});
        }
        return true; // no native flag alloca in the observer engine
    }
    auto const array{::llvm::ArrayType::get(builder.getInt8Ty(), entry.local_count)};
    auto const allocated{create_llvm_jit_entry_block_alloca(builder, array, nullptr,
        get_llvm_string_ref(u8"checkpoint.executed.local.init.v1"))};
    if(allocated == nullptr || allocated->getFunction() != state.llvm_function ||
       !allocated->isStaticAlloca() || allocated->isArrayAllocation()) { return false; }
    allocated->setAlignment(::llvm::Align{checkpoint::native_slot_bytes});
    // [actual N-byte compiler-owned local flags] flags_end
    // [safe                                   ] N<=PTRDIFF_MAX and actual
    // type/classification above BEFORE alloca/GEP construction. Not guest RAM.
    builder.CreateStore(::llvm::ConstantAggregateZero::get(array), allocated);
    for(::std::size_t i{}; i != entry.local_count; ++i)
    {
        if(!entry.slots[i].initialized) { continue; }
        // [flags0 ... i ... N] flags_end
        // [safe             ] i<N before advancing the LLVM byte offset.
        auto const cell{builder.CreateInBoundsGEP(array, allocated, {builder.getInt32(0u), builder.getInt64(i)})};
        auto const store{builder.CreateStore(builder.getInt8(1u), cell)};
        store->setAlignment(::llvm::Align{1u});
    }
    actual_flags = allocated; return true;
}
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_checkpoint_assignment_flag(
    runtime_local_func_llvm_jit_emit_state_t& state, ::llvm::AllocaInst* actual_flags, ::std::size_t original_local) noexcept
{
    if(state.checkpoint_plan == nullptr) { return actual_flags == nullptr; }
    auto const plan{state.checkpoint_plan};
    if(plan->producer_availability != ::uwvm2::runtime::checkpoint::status::ok) { return true; }
    if(!plan->profile || plan->compiler_failure != ::uwvm2::runtime::checkpoint::status::ok ||
       plan->sites.empty() || state.ir_builder == nullptr || state.llvm_function == nullptr || state.ir_builder->GetInsertBlock() == nullptr ||
       state.ir_builder->GetInsertBlock()->getParent() != state.llvm_function) { return false; }
    auto const count{plan->sites.front().local_count};
    if(checkpoint_observer_workspace_selected(state))
    {
        if(actual_flags != nullptr || state.checkpoint_observer_workspace == nullptr ||
           original_local >= count || original_local >= state.local_pointers.size()) { return false; }
        auto& builder{*state.ir_builder};
        auto const cell{builder.CreateInBoundsGEP(builder.getInt8Ty(), state.checkpoint_observer_workspace,
            builder.getInt64(original_local))};
        builder.CreateStore(builder.getInt8(1u), cell)->setAlignment(::llvm::Align{1u});
        return true;
    }
    if(original_local >= count || original_local >= state.local_pointers.size() || actual_flags == nullptr ||
       actual_flags->getFunction() != state.llvm_function ||
       !actual_flags->isStaticAlloca() || actual_flags->isArrayAllocation() || actual_flags->getAlign().value() > ::uwvm2::runtime::checkpoint::native_slot_bytes) { return false; }
    auto const array{::llvm::dyn_cast<::llvm::ArrayType>(actual_flags->getAllocatedType())};
    if(array == nullptr || !array->getElementType()->isIntegerTy(8u) || array->getNumElements() != count) { return false; }
    // Called ONLY after the actual reachable local.set/tee native value store.
    // Neither speculative validation nor a control-merge edge calls this.
    auto& builder{*state.ir_builder};
    // [actual owned byte flags0 ... original_local ... N] flags_end
    // [safe                                             ] original_local<N
    // and exact entry single-object native alloca/type/count BEFORE GEP.
    // A constant allocation count zero is static but has no writable extent.
    auto const cell{builder.CreateInBoundsGEP(array, actual_flags,
        {builder.getInt32(0u), builder.getInt64(original_local)})};
    auto const store{builder.CreateStore(builder.getInt8(1u), cell)};
    store->setAlignment(::llvm::Align{1u}); return true;
}
[[nodiscard]] inline constexpr bool emit_runtime_local_func_llvm_jit_checkpoint_reset_initialization_flags(
    runtime_local_func_llvm_jit_emit_state_t& state, ::llvm::AllocaInst* actual_flags) noexcept
{
    if(state.checkpoint_plan == nullptr) { return actual_flags == nullptr; }
    auto const plan{state.checkpoint_plan};
    if(plan->producer_availability != ::uwvm2::runtime::checkpoint::status::ok) { return true; }
    if(!plan->profile || plan->compiler_failure != ::uwvm2::runtime::checkpoint::status::ok ||
       plan->sites.empty() || state.ir_builder == nullptr || state.llvm_function == nullptr || state.ir_builder->GetInsertBlock() == nullptr ||
       state.ir_builder->GetInsertBlock()->getParent() != state.llvm_function) { return false; }
    auto const& entry{plan->sites.front()};
    if(entry.local_count == 0u) { return actual_flags == nullptr; }
    if(checkpoint_observer_workspace_selected(state))
    {
        if(actual_flags != nullptr || state.checkpoint_observer_workspace == nullptr ||
           entry.local_count != entry.slots.size()) { return false; }
        auto& builder{*state.ir_builder};
        auto const flags{state.checkpoint_observer_workspace};
        builder.CreateMemSet(flags, builder.getInt8(0u), entry.local_count, ::llvm::MaybeAlign{1u});
        for(::std::size_t i{}; i != entry.local_count; ++i)
        {
            if(!entry.slots[i].initialized) { continue; }
            auto const cell{builder.CreateInBoundsGEP(builder.getInt8Ty(), flags, builder.getInt64(i))};
            builder.CreateStore(builder.getInt8(1u), cell)->setAlignment(::llvm::Align{1u});
        }
        return true;
    }
    if(actual_flags == nullptr || actual_flags->getFunction() != state.llvm_function ||
       !actual_flags->isStaticAlloca() || actual_flags->isArrayAllocation() || actual_flags->getAlign().value() > ::uwvm2::runtime::checkpoint::native_slot_bytes ||
       entry.local_count != entry.slots.size()) { return false; }
    auto const array{::llvm::dyn_cast<::llvm::ArrayType>(actual_flags->getAllocatedType())};
    if(array == nullptr || !array->getElementType()->isIntegerTy(8u) || array->getNumElements() != entry.local_count) { return false; }
    auto& builder{*state.ir_builder};
    // Called ONLY when actual self-tail/new-activation local reset executes,
    // after new parameters/default local values are installed. Never on merge.
    // [actual owned N-byte flags ...] flags_end
    // [safe exact single-object entry array type] full reset uses that
    // same bounded owner; static count zero cannot grant a writable extent.
    builder.CreateStore(::llvm::ConstantAggregateZero::get(array), actual_flags);
    for(::std::size_t i{}; i != entry.local_count; ++i)
    {
        if(!entry.slots[i].initialized) { continue; }
        // [flags0 ... i ... N] flags_end
        // [safe             ] i<N before advancing this byte offset.
        auto const cell{builder.CreateInBoundsGEP(array, actual_flags, {builder.getInt32(0u), builder.getInt64(i)})};
        auto const store{builder.CreateStore(builder.getInt8(1u), cell)};
        store->setAlignment(::llvm::Align{1u});
    }
    return true;
}

#pragma once
// Included after the actual LLVM emitter state/type/entry-alloca helpers.
// Exact Core3 metadata comes from fused validation, never SSA projection.
// Observer packets use an actual activation-owned heap; resume packets retain
// their separately bounded native workspace.
struct llvm_jit_checkpoint_native_packet_emit_result
{
    ::llvm::Value* slots_address{};
    ::llvm::Value* flags_address{};
    ::std::size_t slots_bytes{}, local_flags{};
    bool valid{};
};

[[nodiscard]] inline constexpr runtime_operand_stack_value_type
    checkpoint_packet_physical_carrier(::uwvm2::runtime::checkpoint::types::core_value_type type) noexcept
{
    using kind = ::uwvm2::runtime::checkpoint::types::value_kind;
    switch(type.kind)
    {
        case kind::i32: return runtime_operand_stack_value_type::i32;
        case kind::i64: return runtime_operand_stack_value_type::i64;
        case kind::f32: return runtime_operand_stack_value_type::f32;
        case kind::f64: return runtime_operand_stack_value_type::f64;
        case kind::v128: return runtime_operand_stack_value_type::v128;
        case kind::reference: return runtime_operand_stack_value_type::funcref;
    }
    return runtime_operand_stack_value_type::i32; // known_type must precede use
}

[[nodiscard]] inline constexpr llvm_jit_checkpoint_native_packet_emit_result
    emit_runtime_local_func_llvm_jit_checkpoint_dynamic_packet(
        runtime_local_func_llvm_jit_emit_state_t& state,
        ::uwvm2::runtime::checkpoint::safepoint_layout const& site,
        ::llvm::AllocaInst* executed_flags, ::llvm::ArrayRef<::llvm::Value*> actual_nonlocals,
        ::llvm::AllocaInst** reusable_packet = nullptr, ::llvm::AllocaInst** reusable_flags = nullptr) noexcept
{
    if(state.checkpoint_plan == nullptr) { return {.valid = true}; } // no default IR
    namespace checkpoint = ::uwvm2::runtime::checkpoint;
    auto const plan{state.checkpoint_plan};
    if(plan->producer_availability != checkpoint::status::ok) { return {}; }
    if(!plan->profile || plan->compiler_failure != checkpoint::status::ok ||
       state.ir_builder == nullptr || state.llvm_function == nullptr || state.ir_builder->GetInsertBlock() == nullptr ||
       state.ir_builder->GetInsertBlock()->getParent() != state.llvm_function || site.identifier == 0u ||
       site.identifier > plan->sites.size()) { return {}; }
    auto const actual_block{state.ir_builder->GetInsertBlock()};
    if(!actual_block->empty() && actual_block->back().isTerminator()) { return {}; }
    // [actual compiler-owned site vector ... identifier-1 ... N] end
    // [safe                                                   ] dense ordinal
    // bounded BEFORE indexing/address comparison. Copied layouts cannot replace
    // the actual builder's metadata identity; sealing remains a later phase.
    if(::std::addressof(plan->sites[static_cast<::std::size_t>(site.identifier - 1u)]) != ::std::addressof(site) ||
       (state.checkpoint_observer_controls == nullptr ? checkpoint::validate_site(site,*plan) :
        state.checkpoint_observer_controls->validate_tentative(site)) != checkpoint::status::ok ||
       site.local_count != state.local_pointers.size() || site.local_count != state.local_types.size() ||
       site.slots.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / checkpoint::native_slot_bytes ||
       actual_nonlocals.size() != site.slots.size() - site.local_count) { return {}; }
    // Bound the WHOLE function's checkpoint storage before any new IR:
    // legacy entry N*16, max-live slots*16, two N-byte flags plus padding.
    if(!checkpoint_saved_control_workspace_fits(state,site.slots.size(),site.saved_parameter_count))
    { plan->producer_availability = checkpoint::status::quota_exceeded; return {}; }
    ::llvm::ArrayType* flags_type{};
    bool const observing{checkpoint_observer_workspace_selected(state)};
    if(observing && !grow_runtime_local_func_llvm_jit_checkpoint_observer_workspace(state, site.slots.size())) { return {}; }
    if(site.local_count != 0u)
    {
        if(observing)
        {
            if(executed_flags != nullptr || state.checkpoint_observer_workspace == nullptr) { return {}; }
            flags_type = ::llvm::ArrayType::get(state.ir_builder->getInt8Ty(), site.local_count);
        }
        else
        {
            if(executed_flags == nullptr || executed_flags->getFunction() != state.llvm_function ||
               !executed_flags->isStaticAlloca() || executed_flags->isArrayAllocation() || executed_flags->getAlign().value() > checkpoint::native_slot_bytes) { return {}; }
            flags_type = ::llvm::dyn_cast<::llvm::ArrayType>(executed_flags->getAllocatedType());
            if(flags_type == nullptr || !flags_type->getElementType()->isIntegerTy(8u) ||
               flags_type->getNumElements() != site.local_count) { return {}; }
        }
    }
    else if(executed_flags != nullptr) { return {}; }

    // Reuse is opt-in for the actual fused producer. Both lexical compiler
    // cells must belong to the same live emit-state lifetime; neither cell is
    // a serialized address, stop ticket or permission to read native storage.
    // The four-argument internal helper also uses this actual emitter's two
    // lexical owner cells. Repeated default-argument calls cannot allocate a
    // new native packet per opcode and evade the whole-function workspace cap.
    if(reusable_packet == nullptr && reusable_flags == nullptr)
    {
        reusable_packet = ::std::addressof(state.checkpoint_packet_values);
        reusable_flags = ::std::addressof(state.checkpoint_packet_flags);
    }
    if((reusable_packet == nullptr) != (reusable_flags == nullptr) ||
       (reusable_packet != nullptr && (reusable_packet == reusable_flags ||
        (*reusable_packet != nullptr && *reusable_packet == *reusable_flags) ||
        (executed_flags != nullptr &&
         (*reusable_packet == executed_flags || *reusable_flags == executed_flags))))) { return {}; }
    if(observing && (state.checkpoint_observer_workspace == nullptr ||
        (reusable_packet != nullptr && (*reusable_packet != nullptr || *reusable_flags != nullptr)))) { return {}; }
    if(reusable_packet != nullptr && !observing)
    {
        if(state.llvm_module == nullptr || state.llvm_module->getDataLayout().isDefault() ||
           state.llvm_module->getDataLayout().getPointerSizeInBits() != sizeof(::std::uintptr_t) * CHAR_BIT)
        { return {}; }
        auto const check_owner{[&](::llvm::AllocaInst* owner, ::std::size_t required, bool exact) noexcept
        {
            if(owner == nullptr) { return true; }
            if(owner->getFunction() != state.llvm_function || !owner->isStaticAlloca() || owner->isArrayAllocation() || owner->getAlign().value() > checkpoint::native_slot_bytes) { return false; }
            auto const array{::llvm::dyn_cast<::llvm::ArrayType>(owner->getAllocatedType())};
            return array != nullptr && array->getElementType()->isIntegerTy(8u) &&
                   (exact ? array->getNumElements() == required : array->getNumElements() >= required) &&
                   array->getNumElements() <= static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) &&
                   (exact || checkpoint_saved_control_workspace_fits(state,
                       array->getNumElements() / checkpoint::native_slot_bytes +
                       static_cast<::std::size_t>(array->getNumElements() % checkpoint::native_slot_bytes != 0u),
                       site.saved_parameter_count));
        }};
        // The payload may grow later in this same unfinished function; reject
        // foreign/dynamic/multiple-or-zero-object allocations BEFORE any
        // IR/type mutation. isStaticAlloca alone accepts constant count zero.
        // Payload and saved flags are disjoint from actual executed flags;
        // zeroing an aliased workspace must never erase live initialization.
        if(!check_owner(*reusable_packet, 0u, false) ||
           !check_owner(*reusable_flags, site.local_count, true) ||
           (site.local_count == 0u && *reusable_flags != nullptr)) { return {}; }
    }
    auto& builder{*state.ir_builder};
    // Prevalidate ALL original-index locals and actual live SSA handles before
    // adding any packet IR. Physical carrier equality checks ABI only; it never
    // infers/refines the exact heap/nullability/Core3 type in the logical plan.
    // Nonlocals are borrowed from the actual fused emitter operand stack.
    // Same-function/type checks are not an SSA dominance proof; the final
    // complete LLVM verifier must succeed before any engine publication.
    for(::std::size_t index{}; index != site.slots.size(); ++index)
    {
        auto const carrier{checkpoint_packet_physical_carrier(site.slots[index].type)};
        auto const native_type{get_llvm_type_from_wasm_value_type(builder.getContext(), carrier)};
        auto const width{get_runtime_wasm_value_type_abi_size(carrier)};
        if(native_type == nullptr || width == 0u || width > checkpoint::native_slot_bytes) { return {}; }
        if(index < site.local_count)
        {
            // [actual local pointer/type vectors ... index ... count] end
            // [safe                                                 ] index
            // belongs to both complete native local vectors BEFORE indexing.
            auto const local{::llvm::dyn_cast_or_null<::llvm::AllocaInst>(state.local_pointers.index_unchecked(index))};
            auto const declared{get_llvm_type_from_wasm_value_type(builder.getContext(), state.local_types.index_unchecked(index))};
            if(local == nullptr || local->getFunction() != state.llvm_function ||
               !local->isStaticAlloca() || local->isArrayAllocation() ||
               local->getAllocatedType() != native_type || declared != native_type)
            { return {}; }
        }
        else
        {
            auto const value{actual_nonlocals[index - site.local_count]};
            if(value == nullptr || value->getType() != native_type || !site.slots[index].initialized) { return {}; }
            if(auto const instruction{::llvm::dyn_cast<::llvm::Instruction>(value)}; instruction != nullptr &&
               instruction->getFunction() != state.llvm_function) { return {}; }
            if(auto const argument{::llvm::dyn_cast<::llvm::Argument>(value)}; argument != nullptr &&
               argument->getParent() != state.llvm_function) { return {}; }
        }
    }

    auto const integer{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const bytes{site.slots.size() * checkpoint::native_slot_bytes};
    llvm_jit_checkpoint_native_packet_emit_result out{
        .slots_address = ::llvm::ConstantInt::get(integer, 0u),
        .flags_address = ::llvm::ConstantInt::get(integer, 0u),
        .slots_bytes = bytes, .local_flags = site.local_count, .valid = true};
    // Only the genuine append-only fused observation producer may retain
    // numeric SSA stores. Its activation-owned packet is read-only to the
    // materialization bridge; local capture writes a disjoint prefix. Legacy
    // DATA/resumable callers keep their complete per-call overwrite contract.
    // SAME block (or the compiler-proven private shutdown successor) retains
    // dominance without guessing across Wasm loops, PHIs, EH or alternate entries.
    // Shrinking forgets popped
    // cells, so reusing the same constant after a pop still writes fresh bits.
    bool const incremental_nonlocals{observing && state.checkpoint_observer_controls != nullptr &&
        state.checkpoint_observer_controls->plan == plan && builder.GetInsertPoint() == actual_block->end()};
    auto& retained{state.checkpoint_observer_nonlocals};
    if(!incremental_nonlocals || state.checkpoint_observer_nonlocal_block != actual_block ||
       state.checkpoint_observer_nonlocal_workspace != state.checkpoint_observer_workspace ||
       state.checkpoint_observer_nonlocal_locals != site.local_count ||
       state.checkpoint_observer_nonlocal_anchor == nullptr ||
       state.checkpoint_observer_nonlocal_anchor->getFunction() != state.llvm_function)
    { retained.clear(); }
    if(incremental_nonlocals) { retained.resize(actual_nonlocals.size(),nullptr); }
    else { state.checkpoint_observer_nonlocal_anchor = nullptr; }
    if(site.slots.empty())
    {
        state.checkpoint_observer_nonlocal_anchor = nullptr;
        return out;
    }
    auto const packet_type{::llvm::ArrayType::get(builder.getInt8Ty(), bytes)};
    ::llvm::Value* packet{};
    if(observing)
    {
        packet = builder.CreateInBoundsGEP(builder.getInt8Ty(), state.checkpoint_observer_workspace,
            builder.getInt64(site.local_count * checkpoint::observer_local_metadata_bytes));
    }
    else
    {
        ::llvm::AllocaInst* native_packet{reusable_packet == nullptr ? nullptr : *reusable_packet};
        if(native_packet == nullptr)
        {
            native_packet = create_llvm_jit_entry_block_alloca(builder, packet_type, nullptr,
                get_llvm_string_ref(u8"checkpoint.dynamic.typed.slots.v4"));
            if(native_packet == nullptr || native_packet->getFunction() != state.llvm_function) { return {}; }
            native_packet->setAlignment(::llvm::Align{checkpoint::native_slot_bytes});
            if(reusable_packet != nullptr) { *reusable_packet = native_packet; }
        }
        else
        {
            auto const allocated{::llvm::cast<::llvm::ArrayType>(native_packet->getAllocatedType())};
            if(allocated->getNumElements() < bytes)
            {
                // [actual function entry allocation old extent ... new extent] end
                // [safe exact bytes<=PTRDIFF_MAX and owner preflight above     ]
                // Only its LLVM type grows before verification/optimization/native
                // publication. Earlier opaque-pointer byte GEPs describe a prefix
                // of this larger allocation; no native pointer is changed at run time.
                native_packet->setAllocatedType(packet_type);
            }
        }
        packet = native_packet;
    }
    // The shared observer copier overwrites EVERY local slot and saved flag,
    // including absent values and padding. Clear only the nonlocal suffix here;
    // clearing its local prefix again bloats every opcode's generated stores.
    // Legacy/resumable packets still clear the complete payload before their
    // conditional local loads. Neither path reads an unset local to clear it.
    bool const shared_locals{observing && site.local_count != 0u};
    auto const first_clear_byte{shared_locals ? site.local_count * checkpoint::native_slot_bytes : 0u};
    auto const clear_bytes{bytes - first_clear_byte};
    if(clear_bytes != 0u && !incremental_nonlocals)
    {
        auto const clear_begin{first_clear_byte == 0u ? packet :
            builder.CreateInBoundsGEP(builder.getInt8Ty(), packet, builder.getInt64(first_clear_byte))};
        // Keep large operand/saved tuples compact at O0 as well. An aggregate
        // store expands to a byte-sized SelectionDAG value for every element.
        builder.CreateMemSet(clear_begin, builder.getInt8(0u), clear_bytes, ::llvm::MaybeAlign{1u});
    }
    ::llvm::Value* captured_flags{};
    if(site.local_count != 0u)
    {
        if(observing)
        {
            captured_flags = builder.CreateInBoundsGEP(builder.getInt8Ty(), state.checkpoint_observer_workspace,
                builder.getInt64(site.local_count));
        }
        else { captured_flags = reusable_flags == nullptr ? nullptr : *reusable_flags; }
        if(captured_flags == nullptr)
        {
            auto const allocated = create_llvm_jit_entry_block_alloca(builder, flags_type, nullptr,
                get_llvm_string_ref(u8"checkpoint.dynamic.local.flags.v4"));
            if(allocated == nullptr || allocated->getFunction() != state.llvm_function) { return {}; }
            allocated->setAlignment(::llvm::Align{checkpoint::native_slot_bytes});
            if(reusable_flags != nullptr) { *reusable_flags = allocated; }
            captured_flags = allocated;
        }
        if(!shared_locals) { builder.CreateStore(::llvm::ConstantAggregateZero::get(flags_type), captured_flags); }
        out.flags_address = builder.CreatePtrToInt(captured_flags, integer);
    }
    out.slots_address = builder.CreatePtrToInt(packet, integer);
    // Snapshot the original-index local prefix once through this function's
    // private noinline copier. Values and executed flags are still read at
    // EVERY actual opcode; the nonlocal SSA packet remains site-specific.
    // Complete declarations share a body too; their private address table
    // lives in the already budgeted observation heap, not on the native stack.
    static_assert(checkpoint::native_slot_bytes == ::uwvm2::runtime::lib::details::llvm_jit_debug_local_slot_bytes);
    if(shared_locals && !emit_runtime_local_func_llvm_jit_snapshot_copy(
        state,site.local_count,packet,captured_flags,state.checkpoint_observer_workspace,false)) { return {}; }
    for(::std::size_t index{}; index != site.slots.size(); ++index)
    {
        if(shared_locals && index < site.local_count) { continue; }
        auto const carrier{checkpoint_packet_physical_carrier(site.slots[index].type)};
        auto const native_type{get_llvm_type_from_wasm_value_type(builder.getContext(), carrier)};
        bool const numeric{site.slots[index].type.kind != checkpoint::types::value_kind::reference};
        if(incremental_nonlocals && index >= site.local_count && numeric &&
           retained[index-site.local_count] == actual_nonlocals[index-site.local_count])
        { continue; } // the same actual SSA value already occupies this exact slot
        // [native packet0 ... index*16 ... N*16] packet_end
        // [safe                                ] index<N, N*16 bounded above
        // and the complete carrier width<=16 BEFORE creating the byte GEP.
        auto const slot{builder.CreateInBoundsGEP(packet_type, packet,
            {builder.getInt32(0u), builder.getInt64(index * checkpoint::native_slot_bytes)})};
        if(index >= site.local_count)
        {
            // [actual LLVM operand/control-parameter handles ... bounded index]
            // [safe] exact nonlocals count/type was checked before indexing.
            if(incremental_nonlocals)
            {
                // Overwrite the complete 16-byte cell when its value changes;
                // narrower scalars/references never inherit old high padding.
                // References are deliberately refreshed at every opcode, with
                // their original exact logical type/GC checks left intact.
                if(get_runtime_wasm_value_type_abi_size(carrier) != checkpoint::native_slot_bytes)
                {
                    builder.CreateStore(::llvm::ConstantAggregateZero::get(
                        ::llvm::ArrayType::get(builder.getInt8Ty(),checkpoint::native_slot_bytes)),slot)->setAlignment(::llvm::Align{1u});
                }
                retained[index-site.local_count] = numeric ? actual_nonlocals[index-site.local_count] : nullptr;
            }
            auto const store{builder.CreateStore(actual_nonlocals[index - site.local_count], slot)};
            store->setAlignment(::llvm::Align{1u}); continue;
        }
        // [actual N-local byte flag arrays ... index ... N] flags_end
        // [safe                                          ] index<N and same
        // actual function/array ownership BEFORE either byte-pointer GEP.
        auto const executed_cell{builder.CreateInBoundsGEP(flags_type,
            observing ? state.checkpoint_observer_workspace : executed_flags,
            {builder.getInt32(0u), builder.getInt64(index)})};
        auto const saved_cell{builder.CreateInBoundsGEP(flags_type, captured_flags,
            {builder.getInt32(0u), builder.getInt64(index)})};
        auto const available{builder.CreateLoad(builder.getInt8Ty(), executed_cell)};
        auto const saved_flag{builder.CreateStore(available, saved_cell)};
        saved_flag->setAlignment(::llvm::Align{1u});
        auto const& declaration{site.slots[index]};
        bool const nondefaultable{declaration.type.kind == checkpoint::types::value_kind::reference && !declaration.type.nullable};
        if(nondefaultable)
        {
            // The LOAD itself belongs only to the actually initialized edge.
            // Loading both values and selecting afterward would read poison
            // from a valid, still-uninitialized nondefaultable local alloca.
            auto const initialized{::llvm::BasicBlock::Create(builder.getContext(),
                get_llvm_string_ref(u8"checkpoint.local.initialized"), state.llvm_function)};
            auto const next{::llvm::BasicBlock::Create(builder.getContext(),
                get_llvm_string_ref(u8"checkpoint.local.next"), state.llvm_function)};
            builder.CreateCondBr(builder.CreateICmpEQ(available, builder.getInt8(1u)), initialized, next);
            builder.SetInsertPoint(initialized);
            auto const live{builder.CreateLoad(native_type, state.local_pointers.index_unchecked(index))};
            auto const store{builder.CreateStore(live, slot)}; store->setAlignment(::llvm::Align{1u});
            builder.CreateBr(next); builder.SetInsertPoint(next);
        }
        else
        {
            // Parameters/defaultable locals have genuine initialized values;
            // the runtime still validates every saved canonical flag before
            // interpreting ANY packet payload, and rejects an illegal zero.
            auto const live{builder.CreateLoad(native_type, state.local_pointers.index_unchecked(index))};
            auto const store{builder.CreateStore(live, slot)}; store->setAlignment(::llvm::Align{1u});
        }
    }
    if(incremental_nonlocals)
    {
        state.checkpoint_observer_nonlocal_block = actual_block;
        state.checkpoint_observer_nonlocal_workspace = state.checkpoint_observer_workspace;
        state.checkpoint_observer_nonlocal_locals = site.local_count;
        state.checkpoint_observer_nonlocal_anchor = actual_block->empty() ? nullptr : ::std::addressof(actual_block->back());
    }
    return out;
}

#pragma once
// Real LLVM logical-site landing helper selected by shared compilation.
// The native manager must provide a previously fully
// validated, immutable typed packet under its actual continuation authority.
// Integer labels, byte ranges and this IR component never grant that authority.
// Includes require actual emit state + checkpoint packet carrier/alloca helpers.
struct llvm_jit_checkpoint_resume_landing_emit_result
{
    bool valid{};
    // False for ordinary compilation: callers keep their original live handles
    // without copying/allocating a replacement vector or generating any IR.
    bool selected{};
    // Actual SSA PHIs for operands followed by saved control parameters. The
    // fused compiler must replace corresponding live handles with these; their
    // exact Core3 declarations stay independent of these physical carriers.
    ::std::vector<::llvm::Value*> actual_nonlocals{};
    // Restore-only waiting call path: caller owns this open LLVM block and
    // must invoke the actual child before merging into its post-call edge.
    ::llvm::BasicBlock* restored_predecessor{};
};
// Native typed packets are process-local execution objects, not wire bytes.
// Admit the real target layout BEFORE any selected allocation/GEP/call IR.
[[nodiscard]] inline bool checkpoint_resume_native_abi_layout_matches(::llvm::Module const* module) noexcept
{
    if(module == nullptr) { return false; }
    auto const& layout{module->getDataLayout()};
    return !layout.isDefault() && layout.getPointerSizeInBits() == sizeof(::std::uintptr_t) * CHAR_BIT &&
           layout.isLittleEndian() == (::std::endian::native == ::std::endian::little);
}
[[nodiscard]] inline bool checkpoint_resume_value_owned_by_function(
    ::llvm::Value* value, ::llvm::Function* function) noexcept
{
    if(value == nullptr || function == nullptr) { return false; }
    if(auto const instruction{::llvm::dyn_cast<::llvm::Instruction>(value)}; instruction != nullptr)
    { return instruction->getFunction() == function; }
    if(auto const argument{::llvm::dyn_cast<::llvm::Argument>(value)}; argument != nullptr)
    { return argument->getParent() == function; }
    // Constants have module-independent bit identity; no arbitrary global
    // address, block address, expression or foreign native owner is admitted.
    return ::llvm::isa<::llvm::ConstantInt>(value) || ::llvm::isa<::llvm::ConstantFP>(value) ||
           ::llvm::isa<::llvm::ConstantDataVector>(value) || ::llvm::isa<::llvm::ConstantAggregateZero>(value) ||
           ::llvm::isa<::llvm::ConstantPointerNull>(value);
}
[[nodiscard]] inline bool prepare_runtime_local_func_llvm_jit_checkpoint_resume_dispatch(
    runtime_local_func_llvm_jit_emit_state_t& state, ::llvm::AllocaInst* executed_flags,
    ::llvm::Value* logical_site, ::llvm::Value* actual_payload, ::llvm::Value* actual_payload_bytes,
    ::llvm::Value* actual_flags, ::llvm::Value* actual_flag_count, ::llvm::BasicBlock* reject,
    llvm_jit_checkpoint_resume_dispatch_emit_state& out)
{
    out = {};
    if(state.checkpoint_plan == nullptr) { return true; } // zero default IR
    namespace checkpoint = ::uwvm2::runtime::checkpoint;
    auto const plan{state.checkpoint_plan};
    if(plan->profile && plan->profile->purpose() == checkpoint::compilation_purpose::observe_values)
    { return true; } // observation generates no selector/restore IR
    // [same compiler-owned insertion block][possibly empty IR instruction list]
    // [safe ] the condition checks the builder/block before use and nonempty
    // before back(); this open-block query must not call LLVM23 getTerminator().
    if(!plan->profile || !checkpoint_resume_native_abi_layout_matches(state.llvm_module) ||
       checkpoint::validate_plan(*plan) != checkpoint::status::ok || state.llvm_function == nullptr ||
       state.ir_builder == nullptr || state.ir_builder->GetInsertBlock() == nullptr ||
       state.ir_builder->GetInsertBlock()->getParent() != state.llvm_function ||
       (!state.ir_builder->GetInsertBlock()->empty() && state.ir_builder->GetInsertBlock()->back().isTerminator()) || reject == nullptr || reject->getParent() != state.llvm_function)
    { return false; }
    auto& builder{*state.ir_builder}; auto const local_count{plan->sites.front().local_count};
    auto const integer{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    if(!checkpoint_resume_value_owned_by_function(logical_site, state.llvm_function) || !logical_site->getType()->isIntegerTy(64u) ||
       !checkpoint_resume_value_owned_by_function(actual_payload, state.llvm_function) || actual_payload->getType() != builder.getPtrTy() ||
       !checkpoint_resume_value_owned_by_function(actual_flags, state.llvm_function) || actual_flags->getType() != builder.getPtrTy() ||
       !checkpoint_resume_value_owned_by_function(actual_payload_bytes, state.llvm_function) || actual_payload_bytes->getType() != integer ||
       !checkpoint_resume_value_owned_by_function(actual_flag_count, state.llvm_function) || actual_flag_count->getType() != integer)
    { return false; }
    if(local_count != state.local_pointers.size() || local_count != state.local_types.size()) { return false; }
    if(local_count == 0u) { if(executed_flags != nullptr) { return false; } }
    else
    {
        if(executed_flags == nullptr || executed_flags->getFunction() != state.llvm_function) { return false; }
        auto const type{::llvm::dyn_cast<::llvm::ArrayType>(executed_flags->getAllocatedType())};
        if(type == nullptr || !type->getElementType()->isIntegerTy(8u) || type->getNumElements() != local_count) { return false; }
    }
    auto const normal{::llvm::BasicBlock::Create(builder.getContext(), "checkpoint.resume.normal", state.llvm_function)};
    auto const dispatch{builder.CreateSwitch(logical_site, reject)};
    dispatch->addCase(builder.getInt64(0u), normal); builder.SetInsertPoint(normal);
    out.actual_state = ::std::addressof(state); out.actual_plan = plan; out.dispatch = dispatch;
    out.payload = actual_payload; out.payload_bytes = actual_payload_bytes; out.original_flags = actual_flags;
    out.flag_count = actual_flag_count; out.executed_flags = executed_flags; out.reject = reject;
    return true;
}
[[nodiscard]] inline llvm_jit_checkpoint_resume_landing_emit_result
    emit_runtime_local_func_llvm_jit_checkpoint_resume_landing(runtime_local_func_llvm_jit_emit_state_t& state,
        llvm_jit_checkpoint_resume_dispatch_emit_state& resume,
        ::uwvm2::runtime::checkpoint::safepoint_layout const& site,
        ::llvm::ArrayRef<::llvm::Value*> actual_nonlocals,
        ::llvm::ArrayRef<::uwvm2::runtime::checkpoint::types::core_value_type> exact_nonlocal_types,
        bool awaiting_child_only = false)
{
    if(state.checkpoint_plan == nullptr) { return {.valid = true}; } // no default IR
    namespace checkpoint = ::uwvm2::runtime::checkpoint;
    auto const plan{state.checkpoint_plan};
    if(plan->profile && plan->profile->purpose() == checkpoint::compilation_purpose::observe_values)
    { return {.valid = true}; } // keep actual SSA handles; no restore PHIs
    // [same compiler-owned insertion block][possibly empty IR instruction list]
    // [safe ] the condition checks the builder/block before use and nonempty
    // before back(); this open-block query must not call LLVM23 getTerminator().
    if(!checkpoint_resume_native_abi_layout_matches(state.llvm_module) ||
       resume.actual_state != ::std::addressof(state) || resume.actual_plan != plan || resume.dispatch == nullptr ||
       resume.dispatch->getFunction() != state.llvm_function || resume.reject == nullptr || resume.reject->getParent() != state.llvm_function ||
       state.ir_builder == nullptr || state.ir_builder->GetInsertBlock() == nullptr ||
       state.ir_builder->GetInsertBlock()->getParent() != state.llvm_function || (!state.ir_builder->GetInsertBlock()->empty() && state.ir_builder->GetInsertBlock()->back().isTerminator()) ||
       site.identifier == 0u || site.identifier > plan->sites.size()) { return {}; }
    // [actual fused compiler site vector ... dense ordinal-1 ... N] end
    // [safe                                                       ] bound the
    // ordinal BEFORE address selection; copied plan labels cannot mint a site.
    if(::std::addressof(plan->sites[static_cast<::std::size_t>(site.identifier - 1u)]) != ::std::addressof(site) ||
       (state.checkpoint_observer_controls == nullptr ? checkpoint::validate_site(site,*plan) :
        state.checkpoint_observer_controls->validate_tentative(site)) != checkpoint::status::ok ||
       site.phase != (awaiting_child_only ? checkpoint::frame_phase::awaiting_call_return : checkpoint::frame_phase::before_opcode) ||
       (awaiting_child_only && state.checkpoint_observer_controls == nullptr &&
        (site.controls.size() != 1u || site.controls.front().kind != checkpoint::control_kind::function || site.saved_parameter_count != 0u)) ||
       site.local_count != state.local_pointers.size() || site.local_count != state.local_types.size() ||
       site.local_count != plan->sites.front().local_count || actual_nonlocals.size() != site.slots.size() - site.local_count ||
       exact_nonlocal_types.size() != actual_nonlocals.size() ||
       site.slots.size() > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) / checkpoint::native_slot_bytes)
    { return {}; }
    if(awaiting_child_only && state.checkpoint_observer_controls != nullptr)
    {
        // This map is the original fused validator's lexical borrow, not a
        // count/metadata capability. Restore-only child edges must match its
        // actual simultaneously live physical controls BEFORE adding any IR.
        if(state.checkpoint_observer_controls->plan != plan || site.controls.size() != state.control_stack.size()) { return {}; }
        for(::std::size_t i{};i != site.controls.size();++i)
        {
            // [actual semantic controls0..N] [same-function physical controls] end
            // [safe] both complete equal counts BEFORE ordinal selection.
            auto const kind{site.controls[i].kind};auto const native{state.control_stack.index_unchecked(i).type};bool matches{};
            switch(kind)
            {
                case checkpoint::control_kind::function: matches=native==llvm_jit_control_context_type::function;break;
                case checkpoint::control_kind::block: matches=native==llvm_jit_control_context_type::block;break;
                case checkpoint::control_kind::loop: matches=native==llvm_jit_control_context_type::loop;break;
                case checkpoint::control_kind::if_then: matches=native==llvm_jit_control_context_type::if_then;break;
                case checkpoint::control_kind::if_else: matches=native==llvm_jit_control_context_type::if_else;break;
            }
            if(!matches) { return {}; }
        }
    }
    // The fused walk installs ascending immutable IDs; avoid a quadratic
    // scan when recording a large straight-line function.
    if(!resume.installed_sites.empty() && resume.installed_sites.back() >= site.identifier) { return {}; }
    auto& builder{*state.ir_builder};
    for(::std::size_t i{}; i != site.slots.size(); ++i)
    {
        auto const carrier{checkpoint_packet_physical_carrier(site.slots[i].type)};
        auto const native{get_llvm_type_from_wasm_value_type(builder.getContext(), carrier)};
        auto const width{get_runtime_wasm_value_type_abi_size(carrier)};
        if(native == nullptr || width == 0u || width > checkpoint::native_slot_bytes) { return {}; }
        if(i < site.local_count)
        {
            auto const local{::llvm::dyn_cast_or_null<::llvm::AllocaInst>(state.local_pointers.index_unchecked(i))};
            if(local == nullptr || local->getFunction() != state.llvm_function || local->getAllocatedType() != native ||
               get_llvm_type_from_wasm_value_type(builder.getContext(), state.local_types.index_unchecked(i)) != native ||
               plan->sites.front().slots[i].type != site.slots[i].type) { return {}; }
        }
        else
        {
            auto const index{i - site.local_count}; auto const value{actual_nonlocals[index]};
            if(!site.slots[i].initialized || exact_nonlocal_types[index] != site.slots[i].type ||
               !checkpoint_resume_value_owned_by_function(value, state.llvm_function) || value->getType() != native) { return {}; }
        }
    }
    auto const normal{builder.GetInsertBlock()};
    auto const landing{::llvm::BasicBlock::Create(builder.getContext(), "checkpoint.resume.extent", state.llvm_function)};
    auto const flags_checked{::llvm::BasicBlock::Create(builder.getContext(), "checkpoint.resume.flags", state.llvm_function)};
    auto const values{::llvm::BasicBlock::Create(builder.getContext(), "checkpoint.resume.values", state.llvm_function)};
    auto const merged{awaiting_child_only ? nullptr :
        ::llvm::BasicBlock::Create(builder.getContext(), "checkpoint.resume.merge", state.llvm_function)};
    ::llvm::IRBuilder<> restore{landing};
    auto const integer{restore.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto extent{restore.CreateAnd(restore.CreateICmpEQ(resume.payload_bytes,
        ::llvm::ConstantInt::get(integer, site.slots.size() * checkpoint::native_slot_bytes)),
        restore.CreateICmpEQ(resume.flag_count, ::llvm::ConstantInt::get(integer, site.local_count)))};
    if(!site.slots.empty())
    { extent = restore.CreateAnd(extent, restore.CreateICmpNE(resume.payload, ::llvm::ConstantPointerNull::get(restore.getPtrTy()))); }
    if(site.local_count != 0u)
    { extent = restore.CreateAnd(extent, restore.CreateICmpNE(resume.original_flags, ::llvm::ConstantPointerNull::get(restore.getPtrTy()))); }
    // Extent equality/null checks precede EVERY input byte-pointer advance.
    // Actual immutable native ownership is a separate prerequisite of the
    // private runtime dispatcher; arithmetic alone is not pointer permission.
    restore.CreateCondBr(extent, flags_checked, resume.reject); restore.SetInsertPoint(flags_checked);
    ::std::vector<::llvm::Value*> markers{}; markers.reserve(site.local_count);
    ::llvm::Value* canonical{restore.getTrue()};
    for(::std::size_t i{}; i != site.local_count; ++i)
    {
        // [actual immutable original-index flags0 ... i ... N] flags_end
        // [safe                                             ] i<N and full
        // actual flag extent verified on the predecessor BEFORE byte GEP/load.
        auto const cell{restore.CreateInBoundsGEP(restore.getInt8Ty(), resume.original_flags, restore.getInt64(i))};
        auto const marker{restore.CreateLoad(restore.getInt8Ty(), cell)}; marker->setAlignment(::llvm::Align{1u});
        markers.push_back(marker);
        canonical = restore.CreateAnd(canonical, site.slots[i].initialized ?
            restore.CreateICmpEQ(marker, restore.getInt8(1u)) : restore.CreateICmpULE(marker, restore.getInt8(1u)));
    }
    // Prevalidate ALL flags before ANY value load or local/flag mutation.
    restore.CreateCondBr(canonical, values, resume.reject); restore.SetInsertPoint(values);
    auto const flags_type{site.local_count == 0u ? nullptr : ::llvm::cast<::llvm::ArrayType>(resume.executed_flags->getAllocatedType())};
    for(::std::size_t i{}; i != site.local_count; ++i)
    {
        // [actual same-function owned executed flags[N]] end
        // [safe                                       ] i<N and exact alloca
        // owner/type checked before pointer offset/store of the actual marker.
        auto const cell{restore.CreateInBoundsGEP(flags_type, resume.executed_flags,
            {restore.getInt32(0u), restore.getInt64(i)})};
        auto const store{restore.CreateStore(markers[i], cell)}; store->setAlignment(::llvm::Align{1u});
    }
    for(::std::size_t i{}; i != site.local_count; ++i)
    {
        auto const native{get_llvm_type_from_wasm_value_type(restore.getContext(), checkpoint_packet_physical_carrier(site.slots[i].type))};
        bool const nondefaultable{site.slots[i].type.kind == checkpoint::types::value_kind::reference && !site.slots[i].type.nullable};
        ::llvm::BasicBlock* next{};
        if(nondefaultable)
        {
            auto const initialized{::llvm::BasicBlock::Create(restore.getContext(), "checkpoint.resume.initialized", state.llvm_function)};
            next = ::llvm::BasicBlock::Create(restore.getContext(), "checkpoint.resume.local.next", state.llvm_function);
            restore.CreateCondBr(restore.CreateICmpEQ(markers[i], restore.getInt8(1u)), initialized, next);
            restore.SetInsertPoint(initialized);
        }
        // [actual immutable N*16 typed packet ... i*16 ...] packet_end
        // [safe                                          ] i<local_count<=N,
        // N*16<=PTRDIFF_MAX and exact extent verified BEFORE GEP/typed load.
        // A nondefaultable unset slot NEVER reaches this load/store edge.
        auto const slot{restore.CreateInBoundsGEP(restore.getInt8Ty(), resume.payload, restore.getInt64(i * checkpoint::native_slot_bytes))};
        auto const value{restore.CreateLoad(native, slot)}; value->setAlignment(::llvm::Align{1u});
        restore.CreateStore(value, state.local_pointers.index_unchecked(i));
        if(next != nullptr) { restore.CreateBr(next); restore.SetInsertPoint(next); }
    }
    ::std::vector<::llvm::Value*> restored{}; restored.reserve(actual_nonlocals.size());
    for(::std::size_t i{}; i != actual_nonlocals.size(); ++i)
    {
        auto const index{site.local_count + i};
        auto const native{get_llvm_type_from_wasm_value_type(restore.getContext(), checkpoint_packet_physical_carrier(site.slots[index].type))};
        // [actual immutable complete typed packet ... index*16 ... N*16] end
        // [safe                                                      ] exact
        // nonlocal count and width<=16 BEFORE advancing the packet byte offset.
        auto const slot{restore.CreateInBoundsGEP(restore.getInt8Ty(), resume.payload, restore.getInt64(index * checkpoint::native_slot_bytes))};
        auto const value{restore.CreateLoad(native, slot)}; value->setAlignment(::llvm::Align{1u}); restored.push_back(value);
    }
    // Reconstruct the actual if continuation storage before ANY resumed root
    // publication or future else/implicit-identity edge uses these values.
    // The original lexical EH targets remain attached to this LLVM function;
    // future invokes/throw_ref enter those real PHIs, never a fake handler stack.
    if(!restore_runtime_local_func_llvm_jit_checkpoint_saved_if(state,restore,site,restored)) { return {}; }
    auto const restored_predecessor{restore.GetInsertBlock()};
    if(awaiting_child_only)
    {
        // The normal call operands were computed before this selector edge and
        // do not dominate its restored path. Keep the real normal builder at
        // its original insertion point; ONLY the restored path may enter the
        // private nested child bridge using its independently owned child frame.
        resume.dispatch->addCase(builder.getInt64(site.identifier), landing);
        resume.installed_sites.push_back(site.identifier);
        return {.valid = true, .selected = true, .actual_nonlocals = ::std::move(restored),
                .restored_predecessor = restored_predecessor};
    }
    restore.CreateBr(merged);
    builder.CreateBr(merged); builder.SetInsertPoint(merged);
    llvm_jit_checkpoint_resume_landing_emit_result out{.valid = true, .selected = true}; out.actual_nonlocals.reserve(actual_nonlocals.size());
    for(::std::size_t i{}; i != actual_nonlocals.size(); ++i)
    {
        auto const phi{builder.CreatePHI(actual_nonlocals[i]->getType(), 2u, "checkpoint.resume.logical.value")};
        phi->addIncoming(actual_nonlocals[i], normal); phi->addIncoming(restored[i], restored_predecessor);
        out.actual_nonlocals.push_back(phi);
    }
    resume.dispatch->addCase(builder.getInt64(site.identifier), landing);
    resume.installed_sites.push_back(site.identifier);
    return out;
}

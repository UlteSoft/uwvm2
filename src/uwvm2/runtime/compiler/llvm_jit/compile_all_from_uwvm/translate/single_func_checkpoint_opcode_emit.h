#pragma once
// Compiler-only tentative exact semantic site. Include after actual emit state,
// execution-flag and dynamic-packet helpers. This is selected only by an immutable
// checkpoint plan; ordinary engines emit no site, flag, callback or packet IR.
class llvm_jit_checkpoint_opcode_transaction
{
    runtime_local_func_llvm_jit_emit_state_t* state_{};
    ::uwvm2::runtime::checkpoint::function_plan* plan_{};
    ::std::size_t appended_index_{}, original_size_{};
    bool appended_{}, committed_{};
public:
    explicit llvm_jit_checkpoint_opcode_transaction(runtime_local_func_llvm_jit_emit_state_t& state) noexcept :
        state_{::std::addressof(state)}, plan_{state.checkpoint_plan}, original_size_{plan_ == nullptr ? 0u : plan_->sites.size()}
    {
        if(plan_ != nullptr) { state.checkpoint_current_site = 0u; state.checkpoint_call = {}; }
    }
    llvm_jit_checkpoint_opcode_transaction(llvm_jit_checkpoint_opcode_transaction const&) = delete;
    llvm_jit_checkpoint_opcode_transaction& operator=(llvm_jit_checkpoint_opcode_transaction const&) = delete;
    ~llvm_jit_checkpoint_opcode_transaction()
    {
        if(!appended_ || committed_) { return; }
        // This single fused walk owns the plan exclusively. Roll back only its
        // last tentative metadata cell, never a native publication/code address.
        // Validator/emitter failure means generated fragment is not published.
        if(plan_->sites.size() >= original_size_) { plan_->sites.resize(original_size_); }
        if(state_->checkpoint_observer_controls != nullptr)
        { state_->checkpoint_observer_controls->rollback_sites_from(original_size_); }
        state_->checkpoint_current_site = 0u;
    }
    // The caller constructed exact locals/operand declarations BEFORE the opcode
    // consumed them. A physical LLVM carrier never supplies this semantic type.
    // Unsupported active controls/EH/Bot decline recording, not Wasm validation.
    [[nodiscard]] bool stage_before_opcode(::uwvm2::runtime::checkpoint::safepoint_layout site)
    {
        if(plan_ == nullptr) { return true; }
        namespace checkpoint = ::uwvm2::runtime::checkpoint;
        if(appended_ || committed_) { return false; }
        bool const observing{state_->checkpoint_observer_controls != nullptr};
        if(plan_->compiler_failure != checkpoint::status::ok || plan_->producer_availability != checkpoint::status::ok || plan_->sites.empty() ||
           state_->control_stack.empty() || !state_->control_stack.back().is_reachable ||
           (!observing && (state_->control_stack.size() != 1u ||
            state_->control_stack.back().type != llvm_jit_control_context_type::function ||
            !state_->control_stack.back().exception_handlers.empty() || site.controls.size() != 1u ||
            site.controls[0u].kind != checkpoint::control_kind::function || !site.handlers.empty() || site.saved_parameter_count != 0u)) ||
           (observing && site.controls.size() != state_->control_stack.size()) || site.phase != checkpoint::frame_phase::before_opcode ||
           site.local_count != state_->local_pointers.size() || site.operand_count != state_->operand_stack.size())
        { return true; } // No current materialization; a later stop reports unavailable.
        if(!checkpoint_saved_control_workspace_fits(*state_,site.slots.size(),site.saved_parameter_count))
        { plan_->producer_availability = checkpoint::status::quota_exceeded; return true; }
        if(site.opcode_offset == 0u)
        {
            // [actual nonempty site vector] first is known entry metadata.
            // [safe] exact typed equality before reuse; no second entry snapshot
            // or extra body scan is invented for this original opcode.
            site.identifier = 1u;
            if(site == plan_->sites.front()) { state_->checkpoint_current_site=1u; return true; }
            if(!observing) { return true; }
            // The first real loop/else structural point may share byte0 with
            // entry but has actual nested PHIs/control state. Give it its own
            // dense logical ID; never relabel entry DATA as that state.
        }
        if(!plan_->sites.empty() && plan_->sites.back().phase == checkpoint::frame_phase::before_opcode &&
           plan_->sites.back().opcode_offset == site.opcode_offset)
        {
            site.identifier = plan_->sites.back().identifier;
            if(site == plan_->sites.back()) { state_->checkpoint_current_site=site.identifier; return true; }
            if(!observing) { return true; }
        }
        // At most one pre-op plus two call-edge sites per source opcode.
        // Divide before comparing so a large expression cannot overflow 3*N.
        if(plan_->sites.size()/3u >= plan_->expression_bytes || plan_->sites.size() == UINT64_MAX)
        { return true; }
        site.identifier = plan_->sites.size() + 1u;
        if((observing ? state_->checkpoint_observer_controls->validate_tentative(site) : checkpoint::validate_site(site,*plan_)) != checkpoint::status::ok)
        { return true; }
        appended_index_ = plan_->sites.size(); plan_->sites.push_back(::std::move(site));
        appended_ = true; state_->checkpoint_current_site = appended_index_ + 1u;
        if(observing && !state_->checkpoint_observer_controls->link_site(appended_index_)) { return false; }
        return true;
    }
    [[nodiscard]] bool append_same_opcode_call_sites(
        ::uwvm2::runtime::checkpoint::safepoint_layout waiting,
        ::uwvm2::runtime::checkpoint::safepoint_layout after)
    {
        namespace checkpoint = ::uwvm2::runtime::checkpoint;
        if(plan_ == nullptr) { return true; }
        if(committed_ || waiting.phase != checkpoint::frame_phase::awaiting_call_return ||
           after.phase != checkpoint::frame_phase::before_opcode || plan_->sites.size() > UINT64_MAX-2u ||
           waiting.identifier != plan_->sites.size()+1u || after.identifier != waiting.identifier+1u ||
           waiting.caller_return_offset != after.opcode_offset || waiting.opcode_offset >= after.opcode_offset ||
           (state_->checkpoint_observer_controls == nullptr ? checkpoint::validate_site(waiting,*plan_) :
            state_->checkpoint_observer_controls->validate_tentative(waiting)) != checkpoint::status::ok ||
           (state_->checkpoint_observer_controls == nullptr ? checkpoint::validate_site(after,*plan_) :
            state_->checkpoint_observer_controls->validate_tentative(after)) != checkpoint::status::ok)
        { return false; }
        // This same lexical opcode transaction owns BOTH new cells. Mark its
        // rollback range before the first allocation; a failed second push or
        // later lowering must erase every tentative call cell together.
        if(!appended_) { appended_index_ = plan_->sites.size(); appended_ = true; }
        plan_->sites.push_back(::std::move(waiting)); plan_->sites.push_back(::std::move(after));
        if(state_->checkpoint_observer_controls != nullptr &&
           (!state_->checkpoint_observer_controls->link_site(plan_->sites.size()-2u) ||
            !state_->checkpoint_observer_controls->link_site(plan_->sites.size()-1u))) { return false; }
        return true;
    }
    [[nodiscard]] bool commit_after_fused_opcode_validation() noexcept
    {
        if(plan_ == nullptr || !appended_) { return true; }
        if(plan_->sites.size() <= appended_index_ || state_->checkpoint_current_site < appended_index_ + 1u ||
           state_->checkpoint_current_site > plan_->sites.size())
        { return false; }
        // Called only AFTER this same opcode's authoritative validation and
        // original lowering succeed. No tentative plan can be sealed/published
        // before the entire fused walk completes; failure unwinds this transaction.
        committed_ = true; return true;
    }
};
[[nodiscard]] inline bool emit_runtime_local_func_llvm_jit_checkpoint_current_opcode(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t offset) noexcept
{
    namespace checkpoint = ::uwvm2::runtime::checkpoint;
    auto const plan{state.checkpoint_plan};
    if(plan == nullptr || plan->producer_availability != checkpoint::status::ok || state.checkpoint_current_site == 0u) { return true; }
    if(state.checkpoint_current_site > plan->sites.size()) { return false; }
    // [actual mutable compiler-owned site cells ... id-1 ... N] sites_end
    // [safe] complete dense ordinal BEFORE indexing; sealing/publication occurs
    // only after this fused walk, and runtime resolves its retained immutable plan.
    auto const& site{plan->sites[static_cast<::std::size_t>(state.checkpoint_current_site - 1u)]};
    if(site.opcode_offset != offset) { return true; } // structural/unsupported point stays unavailable
    if(!state.debug_activation_enabled || state.debug_activation_token == nullptr || state.ir_builder == nullptr ||
       site.local_count > site.slots.size() || site.operand_count > site.slots.size()-site.local_count ||
       site.saved_parameter_count != site.slots.size()-site.local_count-site.operand_count ||
       site.operand_count != state.operand_stack.size() ||
       (site.saved_parameter_count != 0u && state.checkpoint_observer_controls == nullptr))
    { return false; }
    ::std::vector<::llvm::Value*> actual{}; actual.reserve(site.slots.size()-site.local_count);
    for(::std::size_t i{}; i != state.operand_stack.size(); ++i)
    {
        // [actual pre-op physical operand handles0 ... i ... N] end
        // [safe] i<N before borrow, actual exact semantic metadata comes from
        // the staged fused validator site rather than these physical handles.
        actual.push_back(state.operand_stack.index_unchecked(i).value);
    }
    if(state.checkpoint_observer_controls != nullptr)
    {
        for(auto const& control : state.control_stack)
        {
            if(control.type != llvm_jit_control_context_type::if_then && control.type != llvm_jit_control_context_type::if_else) { continue; }
            for(::std::size_t i{};i != control.entry_params.size();++i)
            {
                auto const value{read_runtime_local_func_llvm_jit_checkpoint_saved_if(state,*state.ir_builder,control,i)};
                if(value == nullptr) { return false; }actual.push_back(value);
            }
        }
        if(actual.size() != site.operand_count+site.saved_parameter_count) { return false; }
    }
    auto const packet{emit_runtime_local_func_llvm_jit_checkpoint_dynamic_packet(
        state, site, state.checkpoint_executed_local_flags, actual,
        ::std::addressof(state.checkpoint_packet_values), ::std::addressof(state.checkpoint_packet_flags))};
    if(!packet.valid) { return false; }
    auto& builder{*state.ir_builder};
    auto const integer{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const type{::llvm::FunctionType::get(builder.getVoidTy(),
        {builder.getInt64Ty(), builder.getInt64Ty(), integer, integer, integer, integer}, false)};
    // Pure internal control bridge: never suspend the genuine activation or
    // form a foreign-host island around its actual frame-owned packet.
    auto const bridge{get_llvm_runtime_bridge_function_symbol_value_unwrapped<
        ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_materialize_dynamic_abi_bridge>(builder, type)};
    if(bridge == nullptr) { return false; }
    if(auto const declaration{::llvm::dyn_cast<::llvm::Function>(bridge)}; declaration != nullptr)
    { declaration->setDoesNotThrow(); }
    auto const call{apply_llvm_jit_host_calling_conv(builder.CreateCall(type, bridge,
        {state.debug_activation_token, builder.getInt64(site.identifier), packet.slots_address,
         ::llvm::ConstantInt::get(integer, packet.slots_bytes), packet.flags_address,
         ::llvm::ConstantInt::get(integer, packet.local_flags)}))};
    if(call == nullptr) { return false; }
    call->setDoesNotThrow(); // Actual owned TLS DATA mutation; no readonly/speculatable promise.
    return true;
}

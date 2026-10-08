#pragma once
[[nodiscard]] inline bool checkpoint_observer_workspace_selected(runtime_local_func_llvm_jit_emit_state_t const& state) noexcept
{
    namespace cp = ::uwvm2::runtime::checkpoint;
    return state.checkpoint_plan != nullptr && state.checkpoint_plan->profile &&
        state.checkpoint_plan->profile->purpose() == cp::compilation_purpose::observe_values;
}
[[nodiscard]] inline bool prepare_runtime_local_func_llvm_jit_checkpoint_observer_workspace(
    runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    if(!checkpoint_observer_workspace_selected(state) ||
       state.checkpoint_plan->producer_availability != ::uwvm2::runtime::checkpoint::status::ok) { return true; }
    if(!state.emit_debug_safe_points || !state.debug_activation_enabled || state.debug_activation_token == nullptr ||
       state.ir_builder == nullptr || state.checkpoint_observer_workspace != nullptr) { return false; }
    auto& builder{*state.ir_builder};
    auto const integer{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const type{::llvm::FunctionType::get(integer, {integer}, false)};
    auto const bridge{get_llvm_runtime_bridge_function_symbol_value_unwrapped<
        ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_observer_workspace_abi_bridge>(builder, type)};
    if(bridge == nullptr) { return false; }
    auto const locals{state.local_pointers.size()};
    if(!::uwvm2::runtime::checkpoint::observer_workspace_fits(locals, locals)) { return false; }
    auto const bytes{locals * ::uwvm2::runtime::checkpoint::observer_local_metadata_bytes + locals * ::uwvm2::runtime::checkpoint::native_slot_bytes};
    auto const call{apply_llvm_jit_host_calling_conv(builder.CreateCall(type, bridge,
        {::llvm::ConstantInt::get(integer, bytes == 0u ? 1u : bytes)}))};
    state.checkpoint_observer_workspace_allocation = call;
    call->setDoesNotThrow();
    state.checkpoint_observer_workspace = builder.CreateIntToPtr(call, ::llvm::PointerType::get(builder.getContext(), 0u));
    return state.checkpoint_observer_workspace != nullptr;
}
// Grow only this actual entry CallInst's compiler-owned constant. The sealed
// object is emitted AFTER validation of every site; no run-time reallocation,
// guest-provided extent or diagnostic ledger identity participates.
[[nodiscard]] inline bool grow_runtime_local_func_llvm_jit_checkpoint_observer_workspace(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t slots) noexcept
{
    namespace cp = ::uwvm2::runtime::checkpoint;
    auto const locals{state.local_pointers.size()};
    auto const allocation{state.checkpoint_observer_workspace_allocation};
    if(!cp::observer_workspace_fits(locals, slots) || allocation == nullptr || allocation->getFunction() != state.llvm_function)
    { return false; }
    auto const old{::llvm::dyn_cast<::llvm::ConstantInt>(allocation->getArgOperand(0u))};
    if(old == nullptr) { return false; }
    auto const bytes{locals * cp::observer_local_metadata_bytes + slots * cp::native_slot_bytes};
    if(bytes > old->getZExtValue()) { allocation->setArgOperand(0u, ::llvm::ConstantInt::get(old->getType(), bytes)); }
    return true;
}
// Only the privately selected resumable profile needs persistent if entry
// parameters. A direct logical landing bypasses their original SSA producers;
// using those old values on a later else/identity edge would violate dominance.
// One bounded function-owned workspace is reused by disjoint lexical controls.
// Ordinary and observation-only compilation retain their original SSA/IR.
[[nodiscard]] inline bool checkpoint_saved_control_selected(runtime_local_func_llvm_jit_emit_state_t const& state) noexcept
{
    namespace cp = ::uwvm2::runtime::checkpoint;
    return state.checkpoint_plan != nullptr && state.checkpoint_plan->profile &&
           state.checkpoint_plan->profile->purpose() == cp::compilation_purpose::resumable &&
           state.checkpoint_plan->producer_availability == cp::status::ok;
}
// Replaced/resumed physical values must not inherit original SSA-origin-only
// optimization witnesses. In particular a restored funcref has its actual
// registered identity checked at call_ref, never the old ref.func constant.
inline void invalidate_runtime_local_func_llvm_jit_checkpoint_value_witness(
    runtime_local_func_llvm_jit_emit_state_t const& state, llvm_jit_stack_value_t& value) noexcept
{
    if(!checkpoint_saved_control_selected(state)) { return; }
    value.known_ref_func_index=SIZE_MAX;
    value.immutable_gc_values_witness=nullptr;value.immutable_gc_values_witness_block=nullptr;
    value.immutable_gc_values_witness_type=0u;value.immutable_gc_values_witness_next_offset=SIZE_MAX;
}
[[nodiscard]] inline bool checkpoint_saved_control_workspace_fits(runtime_local_func_llvm_jit_emit_state_t const& state,
    ::std::size_t slots, ::std::size_t saved) noexcept
{
    namespace cp = ::uwvm2::runtime::checkpoint;
    if(checkpoint_observer_workspace_selected(state))
    { return cp::observer_workspace_fits(state.local_pointers.size(), slots); }
    if(!checkpoint_saved_control_selected(state)) { return cp::native_workspace_fits(state.local_pointers.size(),slots); }
    return cp::native_workspace_with_saved_fits(state.local_pointers.size(),slots,
        (::std::max)(saved,state.checkpoint_saved_control_max_slots));
}
[[nodiscard]] inline bool prepare_runtime_local_func_llvm_jit_checkpoint_saved_if(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> const& parameters,
    ::std::size_t& first) noexcept
{
    first = SIZE_MAX;
    if(!checkpoint_saved_control_selected(state)) { return true; }
    namespace cp = ::uwvm2::runtime::checkpoint;
    if(state.ir_builder == nullptr || state.llvm_function == nullptr || state.checkpoint_plan->sites.empty()) { return false; }
    ::std::size_t prefix{};
    for(auto const& control : state.control_stack)
    {
        if(control.type != llvm_jit_control_context_type::if_then && control.type != llvm_jit_control_context_type::if_else) { continue; }
        if(control.checkpoint_saved_parameter_first != prefix || control.entry_params.size() > SIZE_MAX-prefix) { return false; }
        prefix += control.entry_params.size();
    }
    if(parameters.size() > SIZE_MAX-prefix) { return false; }
    auto const count{prefix+parameters.size()};
    auto slots{state.checkpoint_plan->sites.front().local_count};
    if(auto const existing{state.checkpoint_packet_values}; existing != nullptr)
    {
        auto const array{::llvm::dyn_cast<::llvm::ArrayType>(existing->getAllocatedType())};
        if(existing->getFunction() != state.llvm_function || !existing->isStaticAlloca() || existing->isArrayAllocation() ||
           array == nullptr || !array->getElementType()->isIntegerTy(8u) || array->getNumElements()%cp::native_slot_bytes != 0u)
        { return false; }
        slots = static_cast<::std::size_t>(array->getNumElements()/cp::native_slot_bytes);
    }
    // Five actual checkpoint native owners, including this saved tuple array.
    // Check jointly against the already allocated max packet BEFORE growth.
    if(!checkpoint_saved_control_workspace_fits(state,slots,count))
    { state.checkpoint_plan->producer_availability=cp::status::quota_exceeded; return true; }
    auto& builder{*state.ir_builder};
    for(auto const& parameter : parameters)
    {
        auto const native{get_llvm_type_from_wasm_value_type(builder.getContext(),parameter.type)};
        auto const width{get_runtime_wasm_value_type_abi_size(parameter.type)};
        if(parameter.value == nullptr || native == nullptr || parameter.value->getType() != native ||
           width == 0u || width > cp::native_slot_bytes) { return false; }
        if(auto const instruction{::llvm::dyn_cast<::llvm::Instruction>(parameter.value)}; instruction != nullptr &&
           instruction->getFunction() != state.llvm_function) { return false; }
    }
    first=prefix;
    if(count == 0u) { return true; }
    auto const bytes{count*cp::native_slot_bytes}; // quota proved complete product <= PTRDIFF_MAX
    auto const type{::llvm::ArrayType::get(builder.getInt8Ty(),bytes)};
    auto& owner{state.checkpoint_saved_control_storage};
    if(owner == nullptr)
    {
        owner=create_llvm_jit_entry_block_alloca(builder,type,nullptr,get_llvm_string_ref(u8"checkpoint.control.if.saved.v1"));
        if(owner == nullptr || owner->getFunction() != state.llvm_function) { return false; }
        owner->setAlignment(::llvm::Align{cp::native_slot_bytes});
    }
    else
    {
        auto const array{::llvm::dyn_cast<::llvm::ArrayType>(owner->getAllocatedType())};
        if(owner->getFunction() != state.llvm_function || !owner->isStaticAlloca() || owner->isArrayAllocation() ||
           array == nullptr || !array->getElementType()->isIntegerTy(8u) ||
           array->getNumElements() != state.checkpoint_saved_control_max_slots*cp::native_slot_bytes) { return false; }
        if(array->getNumElements() < bytes) { owner->setAllocatedType(type); }
    }
    state.checkpoint_saved_control_max_slots=(::std::max)(state.checkpoint_saved_control_max_slots,count);
    for(::std::size_t i{};i != parameters.size();++i)
    {
        // [actual function-owned saved tuples0 ... (prefix+i)*16 ... max*16] end
        // [safe] complete prefix+N <= max and bounded carrier width BEFORE GEP.
        // Store ONLY this new inner tuple; outer saved parameters remain live.
        auto const cell{builder.CreateInBoundsGEP(builder.getInt8Ty(),owner,builder.getInt64((prefix+i)*cp::native_slot_bytes))};
        auto const store{builder.CreateStore(parameters.index_unchecked(i).value,cell)};
        store->setAlignment(::llvm::Align{1u});
    }
    return true;
}
[[nodiscard]] inline ::llvm::Value* read_runtime_local_func_llvm_jit_checkpoint_saved_if(
    runtime_local_func_llvm_jit_emit_state_t& state, ::llvm::IRBuilder<>& builder,
    llvm_jit_control_context_t const& control, ::std::size_t index) noexcept
{
    if(index >= control.entry_params.size()) { return nullptr; }
    // [actual original tuple0 ... index ... N] end
    // [safe] index<N BEFORE selecting its unchanged physical carrier metadata.
    auto const& parameter{control.entry_params.index_unchecked(index)};
    if(!checkpoint_saved_control_selected(state)) { return parameter.value; }
    namespace cp=::uwvm2::runtime::checkpoint;
    auto const owner{state.checkpoint_saved_control_storage};auto const first{control.checkpoint_saved_parameter_first};
    auto const native{get_llvm_type_from_wasm_value_type(builder.getContext(),parameter.type)};
    auto const width{get_runtime_wasm_value_type_abi_size(parameter.type)};
    if(owner == nullptr || owner->getFunction() != state.llvm_function || first > state.checkpoint_saved_control_max_slots ||
       control.entry_params.size() > state.checkpoint_saved_control_max_slots-first || native == nullptr ||
       width == 0u || width > cp::native_slot_bytes) { return nullptr; }
    auto const array{::llvm::dyn_cast<::llvm::ArrayType>(owner->getAllocatedType())};
    if(!owner->isStaticAlloca() || owner->isArrayAllocation() || array == nullptr ||
       !array->getElementType()->isIntegerTy(8u) ||
       state.checkpoint_saved_control_max_slots > static_cast<::std::size_t>(PTRDIFF_MAX)/cp::native_slot_bytes ||
       array->getNumElements() != state.checkpoint_saved_control_max_slots*cp::native_slot_bytes) { return nullptr; }
    // [same-function saved native storage0 ... (first+index)*16 ... max*16] end
    // [safe] full tuple extent and carrier <=16 checked BEFORE byte GEP/load.
    auto const cell{builder.CreateInBoundsGEP(builder.getInt8Ty(),owner,builder.getInt64((first+index)*cp::native_slot_bytes))};
    auto const value{builder.CreateLoad(native,cell,"checkpoint.control.saved.value")};value->setAlignment(::llvm::Align{1u});return value;
}
[[nodiscard]] inline bool restore_runtime_local_func_llvm_jit_checkpoint_saved_if(
    runtime_local_func_llvm_jit_emit_state_t& state, ::llvm::IRBuilder<>& builder,
    ::uwvm2::runtime::checkpoint::safepoint_layout const& site, ::llvm::ArrayRef<::llvm::Value*> restored) noexcept
{
    namespace cp=::uwvm2::runtime::checkpoint;
    if(!checkpoint_saved_control_selected(state) || restored.size() != site.operand_count+site.saved_parameter_count) { return false; }
    if(site.saved_parameter_count == 0u) { return true; } // no native saved tuple to reconstruct
    if(site.controls.size() != state.control_stack.size()) { return false; }
    auto const owner{state.checkpoint_saved_control_storage};::std::size_t prefix{};
    for(::std::size_t c{};c != site.controls.size();++c)
    {
        auto const& semantic{site.controls[c]};auto const& physical{state.control_stack.index_unchecked(c)};
        if(semantic.first_saved_parameter != prefix || semantic.saved_parameter_count > site.saved_parameter_count-prefix) { return false; }
        if(semantic.saved_parameter_count != 0u)
        {
            if(owner == nullptr || owner->getFunction() != state.llvm_function ||
               (physical.type != llvm_jit_control_context_type::if_then && physical.type != llvm_jit_control_context_type::if_else) ||
               physical.checkpoint_saved_parameter_first != prefix || physical.entry_params.size() != semantic.saved_parameter_count ||
               prefix > state.checkpoint_saved_control_max_slots || semantic.saved_parameter_count > state.checkpoint_saved_control_max_slots-prefix)
            { return false; }
            for(::std::size_t i{};i != semantic.saved_parameter_count;++i)
            {
                // [exact restored operands | saved tuple prefix ... prefix+i] end
                // [safe] complete counts checked BEFORE either value/type selection.
                auto const value{restored[site.operand_count+prefix+i]};
                auto const native{get_llvm_type_from_wasm_value_type(builder.getContext(),physical.entry_params.index_unchecked(i).type)};
                if(value == nullptr || native == nullptr || value->getType() != native) { return false; }
            }
        }
        prefix+=semantic.saved_parameter_count;
    }
    if(prefix != site.saved_parameter_count) { return false; }
    auto const array{::llvm::dyn_cast<::llvm::ArrayType>(owner->getAllocatedType())};
    if(!owner->isStaticAlloca() || owner->isArrayAllocation() || array == nullptr ||
       !array->getElementType()->isIntegerTy(8u) ||
       state.checkpoint_saved_control_max_slots > static_cast<::std::size_t>(PTRDIFF_MAX)/cp::native_slot_bytes ||
       array->getNumElements() != state.checkpoint_saved_control_max_slots*cp::native_slot_bytes) { return false; }
    for(::std::size_t i{};i != prefix;++i)
    {
        // [actual restored saved suffix0..prefix] [owned native saved array] end
        // [safe] exact cumulative count <= actual max native slots BEFORE GEP.
        auto const cell{builder.CreateInBoundsGEP(builder.getInt8Ty(),owner,builder.getInt64(i*cp::native_slot_bytes))};
        auto const store{builder.CreateStore(restored[site.operand_count+i],cell)};store->setAlignment(::llvm::Align{1u});
    }
    return true;
}

// Logical landings replace every live operand, including values below a
// structured construct. Carry that prefix through real CFG edges as well as
// the declared label tuple; otherwise an else/end/backedge can retain an SSA
// handle produced exclusively by another arm or iteration. Ordinary and
// observation-only compilation add no prefix PHIs.
[[nodiscard]] inline bool prepare_runtime_local_func_llvm_jit_checkpoint_control_prefix(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::size_t count,
    ::llvm::BasicBlock* end, ::llvm::BasicBlock* loop, ::llvm::BasicBlock* predecessor,
    llvm_jit_checkpoint_control_prefix_t& out) noexcept
{
    if(!checkpoint_saved_control_selected(state)) { return true; }
    if(count > state.operand_stack.size() || end == nullptr || end->getParent() != state.llvm_function ||
       (loop != nullptr && (loop->getParent() != state.llvm_function || predecessor == nullptr))) { return false; }
    out.loop_block=loop;
    for(::std::size_t i{}; i != count; ++i)
    {
        auto const value{state.operand_stack.index_unchecked(i)};
        auto const type{get_llvm_type_from_wasm_value_type(end->getContext(),value.type)};
        if(value.value == nullptr || type == nullptr || value.value->getType() != type) { return false; }
        out.entry.push_back(value);
        out.end_phis.push_back(::llvm::PHINode::Create(type,2u,"checkpoint.control.prefix.end",end));
        if(loop != nullptr)
        {
            auto const phi{::llvm::PHINode::Create(type,2u,"checkpoint.control.prefix.loop",loop)};
            phi->addIncoming(value.value,predecessor);out.loop_phis.push_back(phi);
        }
    }
    return true;
}
[[nodiscard]] inline bool add_runtime_local_func_llvm_jit_checkpoint_control_prefix_incoming(
    runtime_local_func_llvm_jit_emit_state_t& state, llvm_jit_branch_target_t const& target,
    ::llvm::BasicBlock* predecessor) noexcept
{
    if(target.control_stack_index >= state.control_stack.size()) { return false; }
    auto const& control{state.control_stack.index_unchecked(target.control_stack_index)};
    auto const& prefix{control.checkpoint_prefix};
    if(prefix.entry.empty()) { return true; }
    auto const& phis{target.block == control.end_block ? prefix.end_phis : prefix.loop_phis};
    if((target.block != control.end_block && target.block != prefix.loop_block) ||
       prefix.entry.size() != control.outer_stack_size || phis.size() != prefix.entry.size() ||
       state.operand_stack.size() < phis.size()) { return false; }
    for(::std::size_t i{}; i != phis.size(); ++i)
    {
        auto const& value{state.operand_stack.index_unchecked(i)};auto const phi{phis.index_unchecked(i)};
        if(value.type != prefix.entry.index_unchecked(i).type || value.value == nullptr || phi == nullptr ||
           value.value->getType() != phi->getType()) { return false; }
        phi->addIncoming(value.value,predecessor);
    }
    return true;
}
[[nodiscard]] inline bool use_runtime_local_func_llvm_jit_checkpoint_control_prefix(
    runtime_local_func_llvm_jit_emit_state_t& state, llvm_jit_checkpoint_control_prefix_t const& prefix,
    bool use_entry, bool use_loop = false) noexcept
{
    if(prefix.entry.empty()) { return true; }
    auto const& phis{use_loop ? prefix.loop_phis : prefix.end_phis};
    if(state.operand_stack.size() < prefix.entry.size() || (!use_entry && phis.size() != prefix.entry.size())) { return false; }
    for(::std::size_t i{}; i != prefix.entry.size(); ++i)
    {
        auto value{prefix.entry.index_unchecked(i)};
        if(!use_entry)
        {
            auto const phi{phis.index_unchecked(i)};
            if(phi == nullptr || phi->getNumIncomingValues() == 0u) { return false; }
            value.value=phi;
        }
        invalidate_runtime_local_func_llvm_jit_checkpoint_value_witness(state,value);
        state.operand_stack.index_unchecked(i)=value;
    }
    return true;
}

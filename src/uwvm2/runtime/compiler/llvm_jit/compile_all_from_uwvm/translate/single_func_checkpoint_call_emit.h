#pragma once
// Selected one-walk compiler adapter for a normally returning direct Wasm call.
// Exact result declarations are supplied by the same fused validator after its
// authoritative argument/result stack transition. Physical SSA never supplies
// a Core3 heap/nullability declaration. This metadata grants no resume authority.
[[nodiscard]] inline bool prepare_runtime_local_func_llvm_jit_checkpoint_direct_call(
    runtime_local_func_llvm_jit_emit_state_t& state,
    llvm_jit_checkpoint_opcode_transaction& transaction,
    ::std::size_t parameter_count, ::std::size_t result_count, ::std::size_t return_offset,
    ::std::span<::uwvm2::runtime::checkpoint::types::core_value_type const> exact_after_operands)
{
    namespace checkpoint = ::uwvm2::runtime::checkpoint;
    state.checkpoint_call = {};
    auto const plan{state.checkpoint_plan};
    if(plan == nullptr || plan->producer_availability != checkpoint::status::ok || state.checkpoint_current_site == 0u)
    { return true; }
    if(state.checkpoint_current_site > plan->sites.size() ||
       (state.checkpoint_resume.dispatch == nullptr && (!plan->profile ||
        plan->profile->purpose() != checkpoint::compilation_purpose::observe_values)) ||
       plan->sites.size() > UINT64_MAX - 2u)
    { return false; }
    // [same fused compiler's complete site vector0 ... id-1 ... N] end
    // [safe] nonzero ordinal <= N BEFORE taking an owning metadata copy. The
    // vector may grow below; no borrowed reference is retained across growth.
    auto const before{plan->sites[static_cast<::std::size_t>(state.checkpoint_current_site - 1u)]};
    if(before.phase != checkpoint::frame_phase::before_opcode ||
       (state.checkpoint_observer_controls == nullptr && (!before.handlers.empty() ||
        before.controls.size() != 1u || before.controls.front().kind != checkpoint::control_kind::function || before.saved_parameter_count != 0u)) ||
       parameter_count > before.operand_count ||
       before.operand_count != state.operand_stack.size() || return_offset <= before.opcode_offset ||
       return_offset >= plan->expression_bytes)
    { return true; } // Valid unsupported guest code continues through its ordinary lowering.
    auto const prefix{before.operand_count - parameter_count};
    if(result_count > SIZE_MAX - prefix || exact_after_operands.size() != prefix + result_count)
    { return false; }
    for(::std::size_t i{}; i != prefix; ++i)
    {
        // [complete original local-prefix + operand slots] [after operands] end
        // [safe] i < prefix <= both operand counts BEFORE either exact type
        // selection; no SSA or native reference token is inspected here.
        if(before.slots[before.local_count + i].type != exact_after_operands[i]) { return false; }
    }
    for(auto const type : exact_after_operands) { if(!checkpoint::known_type(type)) { return true; } }
    checkpoint::safepoint_layout waiting{before};
    waiting.identifier = plan->sites.size() + 1u;
    waiting.phase = checkpoint::frame_phase::awaiting_call_return;
    waiting.caller_return_offset = return_offset; waiting.operand_count = prefix;
    // [owning exact slots local-prefix + pre-call operand tuple] end
    // [safe] prefix <= before.operand_count and validated slot sum BEFORE
    // truncation. Removed argument values never remain a waiting caller input.
    waiting.slots.resize(waiting.local_count + prefix);
    for(::std::size_t i{}; i != before.saved_parameter_count; ++i)
    {
        // [validated exact local+original operand+saved parameter slots] end
        // [safe] full layout was checked before selecting the saved suffix.
        waiting.slots.push_back(before.slots[before.local_count+before.operand_count+i]);
    }
    checkpoint::safepoint_layout after{waiting};
    after.identifier = waiting.identifier + 1u; after.phase = checkpoint::frame_phase::before_opcode;
    after.opcode_offset = return_offset; after.caller_return_offset = 0u;
    after.operand_count = exact_after_operands.size();
    after.slots.resize(after.local_count+prefix);
    for(::std::size_t i{prefix}; i != exact_after_operands.size(); ++i)
    { after.slots.push_back({exact_after_operands[i], true}); }
    for(::std::size_t i{}; i != before.saved_parameter_count; ++i)
    { after.slots.push_back(before.slots[before.local_count+before.operand_count+i]); }
    if((state.checkpoint_observer_controls == nullptr ? checkpoint::validate_site(waiting,*plan) :
        state.checkpoint_observer_controls->validate_tentative(waiting)) != checkpoint::status::ok ||
       (state.checkpoint_observer_controls == nullptr ? checkpoint::validate_site(after,*plan) :
        state.checkpoint_observer_controls->validate_tentative(after)) != checkpoint::status::ok)
    { return false; }
    if(!checkpoint_saved_control_workspace_fits(state,after.slots.size(),after.saved_parameter_count))
    { plan->producer_availability = checkpoint::status::quota_exceeded; return true; }
    auto const before_id{state.checkpoint_current_site}; auto const waiting_id{waiting.identifier}; auto const after_id{after.identifier};
    if(!transaction.append_same_opcode_call_sites(::std::move(waiting), ::std::move(after))) { return false; }
    state.checkpoint_call = {before_id, waiting_id, after_id, before.opcode_offset, return_offset, prefix, false};
    return true;
}

// Both selected profiles need an exact waiting parent across indirect/ref
// calls. Observation records values only; its nested-child emitter still emits
// no resume selector/bridge. Ordinary/profile-zero bodies remain unchanged.
// The same fused validator supplies all declarations; no body is rescanned.
[[nodiscard]] inline bool runtime_local_func_llvm_jit_checkpoint_dynamic_call_selected(
    runtime_local_func_llvm_jit_emit_state_t const& state) noexcept
{
    auto const plan{state.checkpoint_plan};
    if(plan == nullptr || !plan->profile) { return false; }
    auto const purpose{plan->profile->purpose()};
    return purpose == ::uwvm2::runtime::checkpoint::compilation_purpose::resumable ||
           purpose == ::uwvm2::runtime::checkpoint::compilation_purpose::observe_values;
}

// A direct import index is neither proof of a host leaf nor proof of a Wasm
// child. This cold compiler witness checks the ACTUAL initialized alias chain
// against exact active-registry storage elements. It only selects DATA sites;
// the original runtime effect bridge still authenticates canonical full-source,
// publication, generation, profile and ABI before an actual provider entry.
[[nodiscard]] inline bool runtime_local_func_llvm_jit_checkpoint_import_has_defined_owner(
    runtime_local_func_llvm_jit_emit_state_t const& state,
    validation_module_traits_t::wasm_u32 function_index) noexcept
{
    if(!runtime_local_func_llvm_jit_checkpoint_dynamic_call_selected(state) || state.local_func_storage_ptr == nullptr)
    { return false; }
    auto const module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr) { return false; }
    // This compiler-only borrow is pinned by its actual native source owner.
    // It never selects a global runtime world or admits an executable child.
    auto const explicit_registry{state.local_func_storage_ptr->compiler_registry};
    auto const& registry{explicit_registry == nullptr ?
        ::uwvm2::uwvm::runtime::storage::active_runtime_registry() : *explicit_registry};
    bool caller_member{};
    for(auto const& entry : registry)
    { if(::std::addressof(entry.second) == module) { caller_member=true; break; } }
    if(!caller_member) { return false; } // comparison only; no unowned module dereference
    auto const imports{module->imported_function_vec_storage.size()};
    if(static_cast<::std::size_t>(function_index) >= imports) { return false; }
    using imported = ::uwvm2::uwvm::runtime::storage::imported_function_storage_t;
    using kind = ::uwvm2::uwvm::runtime::storage::imported_function_link_kind;
    auto const registered_import{[&](imported const* candidate) noexcept
    {
        for(auto const& entry : registry)
        {
            auto const& storage{entry.second.imported_function_vec_storage}; ::std::size_t ignored{};
            auto const membership{classify_runtime_storage_pointer(storage.data(), storage.size(), candidate, ignored)};
            if(membership == runtime_storage_pointer_membership::invalid) { return false; }
            if(membership == runtime_storage_pointer_membership::element) { return true; }
        }
        return false;
    }};
    // [actual initialized caller imports0 ... function_index ... count] end
    // [safe] registry membership and index<count BEFORE taking the element.
    auto const* current{::std::addressof(module->imported_function_vec_storage.index_unchecked(function_index))};
    auto const bound{get_runtime_imported_function_link_walk_bound(*module, explicit_registry)};
    for(::std::size_t steps{};;)
    {
        if(steps > bound || !registered_import(current)) { return false; }
        // [complete checked registered import record] end
        // [safe] no interior/one-past/outside record is dereferenced here.
        switch(current->link_kind)
        {
            case kind::imported:
            {
                // [checked current record] [borrowed possible next record] end
                // [safe] copy a comparison-only pointer from the live current
                // record; no next-record permission follows from its address.
                auto const* next{current->target.imported_ptr};
                if(steps == SIZE_MAX || !registered_import(next)) { return false; }
                // [actual registered next record] end
                // [safe] exact element membership checked BEFORE moving the
                // borrowed alias cursor; the initialized registry stays pinned.
                current = next; ++steps;
                // [actual registered next record] end
                // [safe] current remains a complete element at the next read.
                break;
            }
            case kind::defined:
            {
                // [checked import record] [comparison-only final pointer] end
                // [safe] copy the final leaf identity; validate its complete
                // local-storage membership BEFORE any final record read.
                auto const* target{current->target.defined_ptr};
                for(auto const& entry : registry)
                {
                    auto const& owner{entry.second}; auto const& functions{owner.local_defined_function_vec_storage};
                    ::std::size_t local{};
                    auto const membership{classify_runtime_storage_pointer(functions.data(), functions.size(), target, local)};
                    if(membership == runtime_storage_pointer_membership::invalid) { return false; }
                    if(membership != runtime_storage_pointer_membership::element) { continue; }
                    // [actual owner's complete defined function records0..N] end
                    // [safe] exact element equality precedes its module/type
                    // borrow. No executable native address is admitted/copied.
                    auto const& actual{functions.index_unchecked(local)};
                    return actual.function_type_ptr != nullptr && actual.wasm_code_ptr != nullptr;
                }
                return false;
            }
            case kind::local_imported:
            case kind::unresolved: return false;
#if defined(UWVM_SUPPORT_PRELOAD_DL)
            case kind::dl: return false;
#endif
#if defined(UWVM_SUPPORT_WEAK_SYMBOL)
            case kind::weak_symbol: return false;
#endif
            default: return false;
        }
    }
}

[[nodiscard]] inline bool prepare_runtime_local_func_llvm_jit_checkpoint_dynamic_call(
    runtime_local_func_llvm_jit_emit_state_t& state,
    llvm_jit_checkpoint_opcode_transaction& transaction,
    ::std::size_t parameter_count, ::std::size_t result_count, ::std::size_t return_offset,
    ::std::span<::uwvm2::runtime::checkpoint::types::core_value_type const> exact_after_operands)
{
    if(!runtime_local_func_llvm_jit_checkpoint_dynamic_call_selected(state)) { return true; }
    if(parameter_count == SIZE_MAX) { return false; }
    // [prefix | declared arguments | table selector OR function reference] end
    // [safe] checked N+1 count BEFORE the existing bounded prefix subtraction.
    // The same fused validator already consumed the trailing value and proved
    // arguments/results. Its before-site still owns the complete input tuple;
    // a waiting parent retains neither consumed arguments nor that selector.
    return prepare_runtime_local_func_llvm_jit_checkpoint_direct_call(state, transaction,
        parameter_count+1uz, result_count, return_offset, exact_after_operands);
}

[[nodiscard]] inline bool emit_runtime_local_func_llvm_jit_checkpoint_nested_child_path(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& callee_type) noexcept;

[[nodiscard]] inline bool emit_runtime_local_func_llvm_jit_checkpoint_awaiting_direct_call(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& callee_type)
{
    auto& call{state.checkpoint_call};
    if(state.checkpoint_plan == nullptr || call.waiting_site == 0u) { return true; }
    if(call.waiting_published || state.current_wasm_op_offset != call.opcode_offset ||
       state.operand_stack.size() != call.prefix_operands) { return false; }
    state.checkpoint_current_site = call.waiting_site;
    // The physical call preparer has consumed every argument here. The exact
    // owning waiting site was produced before lowering by the same validator.
    if(!emit_runtime_local_func_llvm_jit_checkpoint_current_opcode(state, call.opcode_offset)) { return false; }
    call.waiting_published = true;
    return emit_runtime_local_func_llvm_jit_checkpoint_nested_child_path(state, callee_type);
}

[[nodiscard]] inline bool finish_runtime_local_func_llvm_jit_checkpoint_direct_call(
    runtime_local_func_llvm_jit_emit_state_t& state)
{
    auto& call{state.checkpoint_call};
    if(state.checkpoint_plan == nullptr || call.after_site == 0u) { return true; }
    if(!call.waiting_published || state.checkpoint_current_site != call.waiting_site) { return false; }
    if(call.nested_return_predecessor != nullptr)
    {
        if(state.ir_builder == nullptr || state.llvm_function == nullptr) { return false; }
        auto& builder{*state.ir_builder}; auto const normal{builder.GetInsertBlock()};
        if(normal == nullptr || normal->getParent() != state.llvm_function ||
           (!normal->empty() && normal->back().isTerminator()) ||
           call.nested_return_predecessor->getParent() != state.llvm_function ||
           (!call.nested_return_predecessor->empty() && call.nested_return_predecessor->back().isTerminator()) ||
           call.nested_after_values.size() != state.operand_stack.size()) { return false; }
        for(::std::size_t i{}; i != state.operand_stack.size(); ++i)
        {
            // [exact normal/restored result+prefix tuples0..N] end
            // [safe] equal counts BEFORE either selection; both physical values
            // belong to THIS function and must have the same native carrier.
            auto const value{state.operand_stack.index_unchecked(i).value};
            if(!checkpoint_resume_value_owned_by_function(value, state.llvm_function) ||
               !checkpoint_resume_value_owned_by_function(call.nested_after_values[i], state.llvm_function) ||
               value->getType() != call.nested_after_values[i]->getType()) { return false; }
        }
        auto const joined{::llvm::BasicBlock::Create(builder.getContext(), "checkpoint.child.return.merge", state.llvm_function)};
        ::llvm::IRBuilder<> resumed{call.nested_return_predecessor}; resumed.CreateBr(joined);
        builder.CreateBr(joined); builder.SetInsertPoint(joined);
        for(::std::size_t i{}; i != state.operand_stack.size(); ++i)
        {
            // [bounded same-function physical handles0..N] end
            // [safe] full tuple equality precedes replacing the live SSA handle.
            auto& value{state.operand_stack.index_unchecked(i)};
            auto const phi{builder.CreatePHI(value.value->getType(), 2u, "checkpoint.child.return.value")};
            phi->addIncoming(value.value, normal); phi->addIncoming(call.nested_after_values[i], call.nested_return_predecessor);
            value.value = phi;
            invalidate_runtime_local_func_llvm_jit_checkpoint_value_witness(state,value);
        }
    }
    state.checkpoint_current_site = call.after_site;
    auto const before_offset{state.current_wasm_op_offset};
    state.current_wasm_op_offset = call.return_offset;
    // Install the real post-call PHIs after actual normal return. A restored
    // caller jumps here with its private child-result projection and never
    // repeats the call, including a call immediately before function `end`.
    bool const installed{emit_runtime_local_func_llvm_jit_checkpoint_current_landing(state)};
    if(!installed) { state.current_wasm_op_offset = before_offset; return false; }
    // Publish roots for newly returned references before materialization. The
    // actual generation/root owners still govern every GC access; a packet
    // cannot turn a copied native address into a registered reference root.
    bool const roots{snapshot_runtime_local_func_llvm_jit_gc_roots(state)};
    bool const materialized{roots && emit_runtime_local_func_llvm_jit_checkpoint_current_opcode(state, call.return_offset)};
    state.current_wasm_op_offset = before_offset;
    // Lexical call state ends with THIS successfully lowered opcode. A later
    // call_ref known-index fast path can reuse the generic direct call emitter;
    // it must not inherit this earlier waiting/returned publication witness.
    if(materialized) { call = {}; }
    return materialized;
}

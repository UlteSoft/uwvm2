// Included INSIDE the original fused validator, after its exact type/local
// helpers. This callback borrows only that walk's owned semantic stacks and the
// actual emitter's live LLVM handles; it never decodes a body a second time.
auto checkpoint_stage_observer_site{
    [&](llvm_jit_checkpoint_opcode_transaction& transaction,
        runtime_local_func_llvm_jit_emit_state_t& state) -> bool
    {
        namespace cp = ::uwvm2::runtime::checkpoint;
        auto const plan{state.checkpoint_plan};
        if(state.checkpoint_observer_controls == nullptr || plan == nullptr ||
           plan->compiler_failure != cp::status::ok || plan->producer_availability != cp::status::ok || plan->sites.empty()) { return true; }
        // The implicit function-end callback runs after the semantic frame was
        // popped. Its real pre-end snapshot was staged while that frame still
        // existed; it may only retain that same current lexical materialization.
        if(control_flow_stack.empty()) { return true; }
        if(is_polymorphic || state.control_stack.empty() || !state.control_stack.back().is_reachable) { return true; }
        if(state.control_stack.size() != control_flow_stack.size() || state.operand_stack.size() != operand_stack.size()) { return false; }
        auto const& cap{plan->profile->limits()};
        if(control_flow_stack.size() > cap.controls_per_frame || local_virtual_registers.size() > cap.slots_per_frame ||
           operand_stack.size() > cap.slots_per_frame-local_virtual_registers.size())
        { plan->producer_availability = cp::status::quota_exceeded; return true; }
        if(plan->sites.front().controls.size() != 1u ||
           plan->sites.front().controls.front().kind != cp::control_kind::function ||
           debug_local_initialization.initialized == nullptr || debug_local_initialization.context == nullptr) { return false; }
        cp::safepoint_layout site{plan->sites.front()};
        site.opcode_offset = state.current_wasm_op_offset; site.operand_count = operand_stack.size();
        site.controls.clear(); site.handlers.clear(); site.saved_parameter_count = 0u;
        if(site.slots.size() != local_virtual_registers.size()) { return false; }
        for(::std::size_t i{}; i != local_virtual_registers.size(); ++i)
        {
            bool readable{};
            // [actual complete locals and entry declaration cells0 ... i ... N]
            // [safe] i<N and equal cell count BEFORE preserving original index.
            if(!debug_local_initialization.initialized(debug_local_initialization.context,i,readable)) { return false; }
            site.slots[i].initialized = readable;
        }
        for(::std::size_t i{}; i != operand_stack.size(); ++i)
        {
            // [actual semantic pre-stop operands0 ... i ... N] end
            // [safe] i<N BEFORE reading the full Core3 witness, never LLVM type.
            auto const& operand{operand_stack[i]};
            if(operand.is_unknown) { return true; } // Bot is never an executable value.
            auto const type{operand_core_type(operand)};
            if(!cp::known_type(type)) { return true; }
            site.slots.push_back({type,true});
        }
        for(::std::size_t i{}; i != control_flow_stack.size(); ++i)
        {
            // [actual semantic controls0 ... i ... N] [physical controls0..N]
            // [safe] both exact complete counts proved before either borrow.
            auto const& semantic{control_flow_stack.index_unchecked(i)};
            auto const& physical{state.control_stack.index_unchecked(i)};
            if(semantic.checkpoint_scope >= checkpoint_observer_controls.scopes.size()) { return false; }
            auto const& scope{checkpoint_observer_controls.scopes[semantic.checkpoint_scope]};
            cp::control_layout control{};
            switch(semantic.type)
            {
                case block_type::function: control.kind=cp::control_kind::function; break;
                case block_type::block: control.kind=cp::control_kind::block; break;
                case block_type::loop: control.kind=cp::control_kind::loop; break;
                case block_type::if_: control.kind=cp::control_kind::if_then; break;
                case block_type::else_: control.kind=cp::control_kind::if_else; break;
            }
            bool same_kind{};
            switch(control.kind)
            {
                case cp::control_kind::function: same_kind=physical.type==llvm_jit_control_context_type::function; break;
                case cp::control_kind::block: same_kind=physical.type==llvm_jit_control_context_type::block; break;
                case cp::control_kind::loop: same_kind=physical.type==llvm_jit_control_context_type::loop; break;
                case cp::control_kind::if_then: same_kind=physical.type==llvm_jit_control_context_type::if_then; break;
                case cp::control_kind::if_else: same_kind=physical.type==llvm_jit_control_context_type::if_else; break;
            }
            if(!same_kind) { return false; }
            control.entry_offset = scope.entry;
            control.end_offset = i == 0u ? plan->sites.front().controls.front().end_offset : scope.end;
            control.outer_operand_height = semantic.operand_stack_base;
            control.first_saved_parameter = site.saved_parameter_count;
            auto append_tuple{[&](runtime_block_result_type carriers,bool results,auto& output) -> bool
            {
                auto const count{get_runtime_block_result_count(carriers)};
                if(count > cap.slots_per_frame) { plan->producer_availability=cp::status::quota_exceeded; return false; }
                for(::std::size_t j{}; j != count; ++j)
                {
                    // [actual finalized carrier signature0 ... j ... count] end
                    // [safe] j<count BEFORE selecting its retained exact type.
                    auto const exact{block_core_type_at(carriers,semantic.signature_type_index,results,
                        semantic.has_singleton_result_core_type,semantic.singleton_result_core_type,j)};
                    auto const type{exact.has_type ? exact.type :
                        ::uwvm2::validation::standard::wasm3::core3_legacy_carrier_type(carriers.begin[j])};
                    if(!cp::known_type(type)) { return false; }
                    output.push_back(type);
                }
                return true;
            }};
            if(!append_tuple(semantic.params,false,control.declared_parameters) || !append_tuple(semantic.result,true,control.declared_results)) { return true; }
            if(control.kind == cp::control_kind::if_then || control.kind == cp::control_kind::if_else)
            {
                if(physical.entry_params.size() != control.declared_parameters.size()) { return false; }
                if(physical.entry_params.size() > cap.slots_per_frame-site.slots.size())
                { plan->producer_availability=cp::status::quota_exceeded; return true; }
                control.saved_parameter_count = physical.entry_params.size();
                for(auto const type : control.declared_parameters) { site.slots.push_back({type,true}); }
                site.saved_parameter_count += control.saved_parameter_count; // bounded by checked whole-slot cap
            }
            site.controls.push_back(::std::move(control));
        }
        // Core3 try_table clauses are searched INNER-to-OUTER, preserving each
        // source clause's order. They are lexical DATA; native catch objects
        // never enter the snapshot. catch_ref values are actual owning exnref
        // operands from the original landing/branch PHIs after end_catch.
        for(::std::size_t depth{state.control_stack.size()}; depth != 0u; --depth)
        {
            auto const& physical{state.control_stack.index_unchecked(depth-1u)};
            auto const& semantic{control_flow_stack.index_unchecked(depth-1u)};
            if(physical.exception_handlers.size() != semantic.exception_handlers.size()) { return false; }
            if(physical.exception_handlers.size() > cap.handlers_per_frame-site.handlers.size())
            { plan->producer_availability=cp::status::quota_exceeded; return true; }
            for(auto const& actual : physical.exception_handlers)
            {
                if(actual.control_stack_index >= depth-1u || actual.control_stack_index >= site.controls.size()) { return false; }
                cp::handler_layout handler{};
                handler.catch_all=actual.catch_all; handler.with_reference=actual.with_reference;
                handler.target_control=actual.control_stack_index;
                auto const& target{site.controls[handler.target_control]};
                handler.target_offset=target.kind == cp::control_kind::loop ? target.entry_offset : target.end_offset;
                if(!actual.catch_all)
                {
                    handler.tag_index=actual.tag_index;
                    auto const imports{curr_module.imported_tag_vec_storage.size()};
                    ::std::size_t type_index{SIZE_MAX};
                    if(actual.tag_index < imports)
                    {
                        auto const& tag{curr_module.imported_tag_vec_storage.index_unchecked(actual.tag_index)};
                        if(tag.resolved_tag == nullptr || tag.import_type_ptr == nullptr) { return false; }
                        type_index=tag.import_type_ptr->imports.storage.tag_type_index;
                    }
                    else
                    {
                        auto const local{actual.tag_index-imports};
                        if(local >= curr_module.local_defined_tag_vec_storage.size()) { return false; }
                        type_index=curr_module.local_defined_tag_vec_storage.index_unchecked(local).type_index;
                    }
                    if(type_index >= typesec.owned_signatures.size() || typesec.owned_signatures.size() != typesec.types.size()) { return false; }
                    auto const& declared{typesec.owned_signatures.index_unchecked(type_index).parameters};
                    if(declared.size() > cap.slots_per_frame) { plan->producer_availability=cp::status::quota_exceeded; return true; }
                    for(auto const type : declared) { if(!cp::known_type(type)) { return false; } handler.parameters.push_back(type); }
                }
                site.handlers.push_back(::std::move(handler));
            }
        }
        if(!cp::observer_workspace_fits(site.local_count,site.slots.size()))
        { plan->producer_availability=cp::status::quota_exceeded; return true; }
        return transaction.stage_before_opcode(::std::move(site));
    }};

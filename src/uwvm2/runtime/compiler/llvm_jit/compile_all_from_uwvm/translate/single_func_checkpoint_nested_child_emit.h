#pragma once
// Include only after actual native may-throw/cleanup emitters. This is a second
// selector edge of the SAME validated LLVM body, never a second bytecode pass.
[[nodiscard]] inline bool emit_runtime_local_func_llvm_jit_checkpoint_nested_child_path(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::uwvm::runtime::storage::wasm_binfmt1_final_function_type_t const& callee_type) noexcept
{
    namespace cp = ::uwvm2::runtime::checkpoint;
    auto const plan{state.checkpoint_plan};
    if(plan == nullptr || (plan->profile && plan->profile->purpose() == cp::compilation_purpose::observe_values))
    { return true; } // ordinary/observation: no selector case, child bridge or allocation IR
    auto& call{state.checkpoint_call};
    if(call.waiting_site == 0u) { return true; } // non-direct/unsupported original opcode
#if defined(UWVM_CPP_EXCEPTIONS) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    if(!plan->profile || state.ir_builder == nullptr || state.llvm_function == nullptr ||
       !state.native_guest_exceptions || state.pending_numeric_plan != nullptr || state.gc_root_frame.frame == nullptr ||
       !state.debug_activation_enabled || state.debug_activation_token == nullptr || !call.waiting_published ||
       call.nested_return_predecessor != nullptr || state.checkpoint_resume.dispatch == nullptr ||
       call.waiting_site == UINT64_MAX || call.waiting_site > plan->sites.size() ||
       call.after_site == 0u || call.after_site > plan->sites.size() ||
       call.after_site != call.waiting_site+1u || state.operand_stack.size() != call.prefix_operands)
    { return false; }
    // [actual same-opcode dense waiting | post-call site vector0..N] end
    // [safe] both nonzero full ordinals precede metadata selection. Only this
    // fused validator supplied the exact result and saved-prefix declarations.
    auto const& waiting{plan->sites[static_cast<::std::size_t>(call.waiting_site-1u)]};
    auto const& after{plan->sites[static_cast<::std::size_t>(call.after_site-1u)]};
    auto const abi{get_runtime_wasm_call_abi_layout(callee_type)};
    if(!abi.valid || abi.result_bytes > static_cast<::std::size_t>(PTRDIFF_MAX) ||
       waiting.phase != cp::frame_phase::awaiting_call_return || waiting.operand_count != call.prefix_operands ||
       waiting.local_count > waiting.slots.size() || waiting.operand_count > waiting.slots.size()-waiting.local_count ||
       waiting.saved_parameter_count != waiting.slots.size()-waiting.local_count-waiting.operand_count ||
       after.operand_count < waiting.operand_count || after.operand_count-waiting.operand_count != abi.result_count ||
       after.local_count != waiting.local_count || after.local_count > after.slots.size() ||
       after.operand_count > after.slots.size()-after.local_count ||
       after.saved_parameter_count != after.slots.size()-after.local_count-after.operand_count ||
       after.saved_parameter_count != waiting.saved_parameter_count || waiting.caller_return_offset != after.opcode_offset)
    { return false; }
    ::std::vector<::llvm::Value*> prefix{}; ::std::vector<cp::types::core_value_type> exact{};
    prefix.reserve(call.prefix_operands); exact.reserve(call.prefix_operands);
    for(::std::size_t i{}; i != call.prefix_operands; ++i)
    {
        // [bounded physical operand prefix] [exact locals|prefix slots] end
        // [safe] full tuple counts BEFORE either index. Actual SSA type does
        // not replace the independently authoritative Core3 declaration.
        prefix.push_back(state.operand_stack.index_unchecked(i).value);
        exact.push_back(waiting.slots[waiting.local_count+i].type);
    }
    for(auto const& control : state.control_stack)
    {
        if(control.type != llvm_jit_control_context_type::if_then && control.type != llvm_jit_control_context_type::if_else) { continue; }
        for(::std::size_t i{};i != control.entry_params.size();++i)
        {
            if(prefix.size() >= waiting.slots.size()-waiting.local_count) { return false; }
            auto const value{read_runtime_local_func_llvm_jit_checkpoint_saved_if(state,*state.ir_builder,control,i)};
            if(value == nullptr) { return false; }
            exact.push_back(waiting.slots[waiting.local_count+prefix.size()].type);prefix.push_back(value);
        }
    }
    if(prefix.size() != waiting.operand_count+waiting.saved_parameter_count) { return false; }
    auto& builder{*state.ir_builder}; auto const original_ip{builder.saveIP()};
    auto const landing{emit_runtime_local_func_llvm_jit_checkpoint_resume_landing(state,
        state.checkpoint_resume, waiting, prefix, exact, true)};
    if(!landing.valid || !landing.selected || landing.restored_predecessor == nullptr ||
       landing.actual_nonlocals.size() != call.prefix_operands+waiting.saved_parameter_count) { return false; }
    // [same-function restored open block] retained by actual LLVM Function.
    // [safe] helper verified this owner, site and immutable native ABI layout
    // before moving the compiler builder; no instruction is executed yet.
    builder.SetInsertPoint(landing.restored_predecessor);
    for(::std::size_t i{}; i != call.prefix_operands; ++i)
    { state.operand_stack.index_unchecked(i).value = landing.actual_nonlocals[i]; }
    bool const roots{snapshot_runtime_local_func_llvm_jit_gc_roots(state)};
    bool const packet{roots && emit_runtime_local_func_llvm_jit_checkpoint_current_opcode(state, call.opcode_offset)};
    if(!packet) { return false; }
    auto const integer{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const type{::llvm::FunctionType::get(integer, {builder.getInt64Ty(), builder.getInt64Ty(), integer}, false)};
    // An unwrapped internal control bridge retains the real parent activation
    // and GC root frame. It returns a bounded borrowed view of a private owner,
    // NEVER accepts a destination host pointer or a guest/file native address.
    auto const bridge{get_llvm_runtime_bridge_function_symbol_value_unwrapped<
        ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_resume_child_abi_bridge>(builder, type)};
    if(bridge == nullptr) { return false; }
    auto const child_bytes{apply_llvm_jit_host_calling_conv(emit_runtime_local_func_llvm_jit_may_throw_call(state, type, bridge,
        {state.debug_activation_token, builder.getInt64(waiting.identifier),
         ::llvm::ConstantInt::get(integer, abi.result_bytes)}, true))};
    // Actual child guest exceptions/private cleanup control signals unwind
    // through THIS parent's original lexical try_table clauses and activation/root/trace cleanup.
    // The native typed landing distinguishes actual guest exceptions from the
    // private control signals before any Wasm tag match; no noexcept wrapper
    // or foreign-host island replaces this exceptional edge.
    if(child_bytes == nullptr || !::llvm::isa<::llvm::InvokeInst>(child_bytes) || child_bytes->doesNotThrow()) { return false; }
    // [complete owned restored prefix | saved suffix] end
    // [safe] prefix <= full count <= PTRDIFF_MAX above BEFORE iterator advance;
    // hidden saved values remain native control storage, never child results.
    call.nested_after_values.assign(landing.actual_nonlocals.begin(),
        landing.actual_nonlocals.begin()+static_cast<::std::ptrdiff_t>(call.prefix_operands));
    ::std::size_t offset{};
    for(::std::size_t i{}; i != abi.result_count; ++i)
    {
        // [complete actual callee result tuple] [exact after-call slots] end
        // [safe] result count==after-prefix count BEFORE either indexed type.
        auto const physical{callee_type.result.begin[i]};
        auto const width{get_runtime_wasm_value_type_abi_size(physical)};
        auto const declared{after.slots[after.local_count+call.prefix_operands+i].type};
        auto const native{get_llvm_type_from_wasm_value_type(builder.getContext(), physical)};
        if(native == nullptr || width == 0u || width > cp::native_slot_bytes ||
           checkpoint_packet_physical_carrier(declared) != static_cast<runtime_operand_stack_value_type>(physical) ||
           offset > abi.result_bytes || width > abi.result_bytes-offset) { return false; }
        // [genuine child normal-return owned output0 ... offset ... bytes] end
        // [safe] the private canonical TLS bridge proved exact child result
        // extent, normal return, complete typed carrier types and registered result roots;
        // static full ABI bounds precede pointer formation and EACH byte GEP.
        auto const base{builder.CreateIntToPtr(child_bytes, builder.getPtrTy())};
        auto const cell{builder.CreateInBoundsGEP(builder.getInt8Ty(), base, builder.getInt64(offset))};
        auto const value{builder.CreateLoad(native, cell)}; value->setAlignment(::llvm::Align{1u});
        call.nested_after_values.push_back(value); offset += width;
    }
    if(offset != abi.result_bytes) { return false; }
    auto const release_type{::llvm::FunctionType::get(builder.getVoidTy(),{builder.getInt64Ty(),builder.getInt64Ty()},false)};
    auto const release{get_llvm_runtime_bridge_function_symbol_value_unwrapped<
        ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_release_child_roots_abi_bridge>(builder,release_type)};
    if(release == nullptr) { return false; }
    auto const handoff{apply_llvm_jit_host_calling_conv(builder.CreateCall(release_type,release,
        {state.debug_activation_token,builder.getInt64(waiting.identifier)}))};
    if(handoff == nullptr) { return false; } handoff->setDoesNotThrow();
    // A complete real native root frame protected every returned carrier through
    // these LLVM loads. After its pure no-poll detach, IMMEDIATELY publish the
    // exact returned tuple together with the actual restored parent prefix into
    // its OWN native precise roots. No guest allocation/debug callback intervenes.
    // Compiler-only extra roots do not change the lexical operand model or its
    // separately owned saved-if suffix. Normal-path original arguments stay intact.
    ::std::vector<llvm_jit_stack_value_t> returned_roots{};
    returned_roots.reserve(abi.result_count);
    if(call.prefix_operands > call.nested_after_values.size() ||
       abi.result_count != call.nested_after_values.size()-call.prefix_operands) { return false; }
    for(::std::size_t i{}; i != abi.result_count; ++i)
    {
        // [prefix | complete validated child returned SSA tuple] end
        // [safe] full counts verified BEFORE either indexed selection.
        returned_roots.push_back({.type=static_cast<runtime_operand_stack_value_type>(callee_type.result.begin[i]),
            .value=call.nested_after_values[call.prefix_operands+i]});
    }
    if(!snapshot_runtime_local_func_llvm_jit_gc_roots(state,{returned_roots.data(),returned_roots.size()})) { return false; }
    call.nested_return_predecessor = builder.GetInsertBlock();
    for(::std::size_t i{}; i != call.prefix_operands; ++i)
    { state.operand_stack.index_unchecked(i).value = prefix[i]; }
    // [saved original same-function nonterminated insertion point] end
    // [safe] normal path uses only its dominating original call arguments;
    // restored child return is merged later by the SAME lexical call finish.
    builder.restoreIP(original_ip); return true;
#else
    static_cast<void>(callee_type); return true; // no native child-entry capability
#endif
}

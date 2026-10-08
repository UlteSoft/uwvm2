// PRIVATE closed numeric-cohort experiment, included beside the existing
// pending EH emitter only when UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH=1.
// A native take commits only after genuine tag/numeric/extent admission. This
// does not change the catch path for reference payloads or public C++ throws.
[[nodiscard]] inline bool emit_pending_numeric_fused_catch_clause(
    runtime_local_func_llvm_jit_emit_state_t& state,
    llvm_jit_exception_handler_t const& handler) noexcept
{
    namespace native = ::uwvm2::runtime::exception::pending_experiment::numeric_guest_bridge;
    namespace pending = ::uwvm2::runtime::exception::pending_experiment;
    if(handler.catch_all || handler.with_reference || state.ir_builder == nullptr ||
       state.pending_numeric_context == nullptr || state.pending_numeric_plan == nullptr ||
       !state.pending_numeric_plan->shape_valid_for_compilation() || state.llvm_function == nullptr ||
       state.llvm_public_entry_function == nullptr ||
       state.llvm_function == state.llvm_public_entry_function ||
       state.llvm_function->arg_empty() ||
       state.llvm_function->getArg(0u) != state.pending_numeric_context ||
       state.llvm_function->getMetadata("uwvm2.pending.core.index") == nullptr)
    { return false; }
    auto& builder{*state.ir_builder};
    // [actual builder insertion block][same sealed numeric core]
    // [safe                                                   ] Borrow LLVM
    // handles only while the compiler owns this complete, unpublished module.
    auto const current{builder.GetInsertBlock()};
    if(current == nullptr || current->getParent() != state.llvm_function) { return false; }
    auto const word{state.pending_numeric_context->getType()};
    if(!word->isIntegerTy(sizeof(native::word) * CHAR_BIT)) { return false; }
    ::std::size_t bytes{};
    auto const count{get_runtime_block_result_count(handler.params)};
    if(count > pending::max_payload_fields ||
       !llvm_jit_exception_numeric_tuple_bytes(handler.params, bytes)) { return false; }

    // [optional real caller entry alloca][complete numeric tuple extent]
    // [safe                                                         ] The
    // zero-byte tuple uses integer zero and never forms a byte pointer/GEP.
    ::llvm::AllocaInst* buffer{};
    ::llvm::Value* address{::llvm::ConstantInt::get(word, 0u)};
    if(bytes != 0uz)
    {
        // [new owned native byte array: bytes][one-past]
        // [safe                                       ] This is native stack
        // storage, never a Wasm memory word or a pointer supplied by the guest.
        buffer = create_llvm_jit_entry_block_alloca(builder, builder.getInt8Ty(),
            ::llvm::ConstantInt::get(word, bytes), "pending.catch.fused.tuple");
        if(buffer == nullptr) { return false; }
        // [same complete array][native integer address, no dereference]
        // [safe                                                      ] Lifetime
        // extends across the entire leaf call and every successful typed load.
        address = builder.CreatePtrToInt(buffer, word, "pending.catch.fused.address");
    }
    // The genuine context header and validator-created tag index are the same
    // ones as the original three-call path. A no-match leaves tuple, pending
    // payload, trace and header intact. Full preflight precedes every write.
    // A success copies native bits and commits numeric-empty in one noexcept
    // call, without C++ unwinding, allocation, callbacks or collection.
    auto const taken{emit_pending_numeric_leaf<native::uwvm2_pending_numeric_take_r3>(builder,
        {native::take_semantic}, {state.pending_numeric_context,
            ::llvm::ConstantInt::get(word, handler.tag_index), address,
            ::llvm::ConstantInt::get(word, bytes)})};
    if(taken == nullptr) { return false; }
    auto const ok{builder.CreateICmpEQ(taken, ::llvm::ConstantInt::get(word, 0u))};
    auto const no_match{builder.CreateICmpEQ(taken,
        ::llvm::ConstantInt::get(word, static_cast<unsigned>(pending::status::no_match)))};
    emit_llvm_conditional_trap(*state.llvm_module, builder,
        builder.CreateNot(builder.CreateOr(ok, no_match)));
    // [owned successful clause block][owned unmatched clause block]
    // [safe                                                       ] Both
    // handles live in the actual core; no host/guest address is reinterpreted.
    auto const selected{::llvm::BasicBlock::Create(builder.getContext(), "pending.eh.fused.selected", state.llvm_function)};
    auto const next{::llvm::BasicBlock::Create(builder.getContext(), "pending.eh.fused.next", state.llvm_function)};
    builder.CreateCondBr(ok, selected, next);
    // [selected success edge][initialized tuple][same core lifetime]
    // [safe                                                      ] The only
    // tuple-loading edge follows status::ok, never no_match or an error.
    builder.SetInsertPoint(selected);
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> values{};
    values.reserve(count);
    ::std::size_t offset{};
    for(::std::size_t index{}; index != count; ++index)
    {
        // [validator-created count-element numeric parameter span]
        // [safe                                                   ] index is
        // less than the checked count; no span pointer is advanced or retained.
        auto const type{handler.params.begin[index]};
        auto const llvm_type{get_llvm_type_from_wasm_value_type(builder.getContext(), type)};
        auto const width{get_runtime_wasm_value_type_abi_size(type)};
        if(llvm_type == nullptr || width == 0uz || offset > bytes || width > bytes - offset)
        { return false; }
        // [initialized byte array][offset .. offset+width][end]
        // [safe                                               ] Proved bounds
        // authorize this native GEP. Align-one integer loads/bitcasts preserve
        // all numeric bit patterns, including signaling NaNs and v128 lanes.
        auto const slot{builder.CreateInBoundsGEP(builder.getInt8Ty(), buffer,
            ::llvm::ConstantInt::get(word, offset), "pending.catch.fused.field")};
        auto const carrier{get_llvm_jit_scalar_bits_type(llvm_type)};
        if(carrier == nullptr) { return false; }
        auto const loaded{builder.CreateLoad(carrier, slot, "pending.catch.fused.bits")};
        loaded->setAlignment(::llvm::Align{1u});
        // [current typed numeric SSA value][same module-owned instruction]
        // [safe                                                        ] This
        // converts LLVM handles; it does not turn payload bits into a pointer.
        auto const value{carrier == llvm_type ? static_cast<::llvm::Value*>(loaded) :
            builder.CreateBitCast(loaded, llvm_type, "pending.catch.fused.value")};
        values.push_back({.type = type, .value = value});
        offset += width;
    }
    if(offset != bytes || !snapshot_runtime_local_func_llvm_jit_gc_roots(state, {values.data(), values.size()}))
    { return false; }
    // Numeric-only admission proves there is no heap reference/owner to retire
    // before these loads. The real caller array owns all bits after take. This
    // does NOT permit reference catches to clear before their SSA roots exist;
    // those continue to use the original copy -> snapshot -> clear sequence.
    llvm_jit_branch_target_t const target{.params = handler.params, .block = handler.block,
        .phis = handler.phis, .control_stack_index = handler.control_stack_index};
    if(!try_add_runtime_local_func_llvm_jit_branch_target_incoming(state, target, values,
        builder.GetInsertBlock())) { return false; }
    builder.CreateBr(target.block);
    // [same core's unmatched clause block][unchanged operand model]
    // [safe                                                     ] Only lexical
    // handler search resumes here; it cannot consume an uninitialized tuple.
    builder.SetInsertPoint(next);
    return true;
}

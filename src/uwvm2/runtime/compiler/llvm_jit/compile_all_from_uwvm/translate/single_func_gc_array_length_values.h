// Logical immutable values, not object addresses or a retained native lease.
// Fact validity follows the same runtime local assignment as the reference.
[[nodiscard]] inline llvm_jit_array_length_value_fact_t llvm_jit_find_array_length_value(
    runtime_local_func_llvm_jit_emit_state_t const& state, ::llvm::Value* reference) noexcept
{
    if(!state.array_length_values_enabled || reference == nullptr) { return {}; }
    for(auto i{state.array_length_value_facts.size()}; i != 0uz;)
    {
        auto const& fact{state.array_length_value_facts[--i]};
        ::llvm::Value* actual{fact.reference};
        if(actual == reference) { return fact; }
    }
    return {};
}
inline void llvm_jit_remember_array_length_value(runtime_local_func_llvm_jit_emit_state_t& state,
    ::llvm::Value* reference, ::llvm::Value* length, ::llvm::Value* valid) noexcept
{
    if(!state.array_length_values_enabled || reference == nullptr || length == nullptr || valid == nullptr ||
       !length->getType()->isIntegerTy(32u) || !valid->getType()->isIntegerTy(1u)) { return; }
    // Bounded cold compiler metadata; if saturated, new values remain unknown.
    if(state.array_length_value_facts.size() >= 8192uz) { return; }
    state.array_length_value_facts.push_back({reference, length, valid});
}
[[nodiscard]] inline bool llvm_jit_prepare_array_length_value_locals(
    runtime_local_func_llvm_jit_emit_state_t& state, runtime_module_storage_t const& module) noexcept
{
    // Closed, precise-rooted ordinary full code only. Every external binding,
    // debugger mutation, checkpoint, OSR/lazy/tiered entry keeps checked helpers.
    state.array_length_values_enabled = state.gc_array_access_ordinary_full &&
        state.emit_precise_gc_root_frames && !state.route_wasm_calls_through_runtime_bridge &&
        !state.emit_debug_safe_points && !state.debug_activation_enabled &&
        state.debug_full_patchable_typed_target_base_address == 0u &&
        state.pending_numeric_plan == nullptr &&
        state.checkpoint_plan == nullptr && state.local_types.size() <= 1024uz &&
        module.imported_function_vec_storage.empty() && module.imported_table_vec_storage.empty() &&
        module.imported_memory_vec_storage.empty() && module.imported_global_vec_storage.empty() &&
        module.imported_tag_vec_storage.empty();
    if(!state.array_length_values_enabled) { return true; }
    auto& builder{*state.ir_builder};
#if defined(UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK) && UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK == 1
    state.private_numeric_function_slot = create_llvm_jit_entry_block_alloca(builder,
        builder.getInt1Ty(), nullptr, get_llvm_string_ref(u8"gc.private.numeric.function"));
    if(state.private_numeric_function_slot == nullptr) { return false; }
    // Seal only at complete function finalization; no early escape inference or count race.
    state.private_numeric_function_decision = builder.CreateStore(builder.getFalse(),
        state.private_numeric_function_slot);
#endif
    state.array_length_value_locals.reserve(state.local_types.size());
    for(auto type : state.local_types)
    {
        llvm_jit_array_length_value_local_t slots{};
        if(type == runtime_operand_stack_value_type::funcref)
        {
            slots.length = create_llvm_jit_entry_block_alloca(builder, builder.getInt32Ty(), nullptr,
                get_llvm_string_ref(u8"gc.array.length.local"));
            slots.valid = create_llvm_jit_entry_block_alloca(builder, builder.getInt1Ty(), nullptr,
                get_llvm_string_ref(u8"gc.array.length.known"));
            if(slots.length == nullptr || slots.valid == nullptr) { return false; }
            // Eager initialization dominates every normal body and tail edge.
            builder.CreateStore(builder.getInt32(0u), slots.length);
            builder.CreateStore(builder.getFalse(), slots.valid);
        }
        state.array_length_value_locals.push_back(slots);
    }
    return true;
}
inline void llvm_jit_array_length_value_local_get(runtime_local_func_llvm_jit_emit_state_t& state,
    ::std::size_t index, ::llvm::Value* reference) noexcept
{
    if(!state.array_length_values_enabled || index >= state.array_length_value_locals.size()) { return; }
    auto const slots{state.array_length_value_locals[index]};
    if(slots.length == nullptr || slots.valid == nullptr) { return; }
    auto& builder{*state.ir_builder};
    auto const length{builder.CreateLoad(builder.getInt32Ty(), slots.length)};
    auto const valid{builder.CreateLoad(builder.getInt1Ty(), slots.valid)};
    llvm_jit_remember_array_length_value(state, reference, length, valid);
}
inline void llvm_jit_array_length_value_local_set(runtime_local_func_llvm_jit_emit_state_t& state,
    ::std::size_t index, ::llvm::Value* reference) noexcept
{
    if(!state.array_length_values_enabled || index >= state.array_length_value_locals.size()) { return; }
    auto const slots{state.array_length_value_locals[index]};
    if(slots.length == nullptr || slots.valid == nullptr) { return; }
    auto const fact{llvm_jit_find_array_length_value(state, reference)};
    ::llvm::Value* length{fact.length};
    ::llvm::Value* valid{fact.valid};
    auto& builder{*state.ir_builder};
    // Unknown call/PHI/select/null/parameter writes clear authority every time.
    builder.CreateStore(length == nullptr ? builder.getInt32(0u) : length, slots.length);
    builder.CreateStore(valid == nullptr ? builder.getFalse() : valid, slots.valid);
}
inline void llvm_jit_invalidate_array_length_value_tail_locals(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    if(!state.array_length_values_enabled) { return; }
    for(auto const slots : state.array_length_value_locals)
    {
        if(slots.valid != nullptr)
        {
            state.ir_builder->CreateStore(state.ir_builder->getInt32(0u), slots.length);
            state.ir_builder->CreateStore(state.ir_builder->getFalse(), slots.valid);
        }
    }
}

#if defined(UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK) && UWVM_EXPERIMENTAL_PRIVATE_NUMERIC_ARRAY_LOCK == 1
// Valid length facts originate ONLY in this activation's checked array construction.
// Unknown parameters/calls/PHIs clear authority. Whole-function safety separately
// rejects publication, calls, EH and extern conversion. No native pointer is cached.
[[nodiscard]] inline ::llvm::Value* llvm_jit_private_numeric_array_access(
    runtime_local_func_llvm_jit_emit_state_t& state, ::llvm::Value* reference) noexcept
{
    auto& builder{*state.ir_builder};
    if(!state.array_length_values_enabled || state.private_numeric_function_slot == nullptr)
    { return builder.getFalse(); }
    auto const fact{llvm_jit_find_array_length_value(state, reference)};
    if(fact.valid == nullptr) { return builder.getFalse(); }
    auto const safe{builder.CreateLoad(builder.getInt1Ty(), state.private_numeric_function_slot)};
    return builder.CreateAnd(safe, fact.valid, "gc.private.numeric.access");
}
inline void llvm_jit_private_numeric_observe_original_opcode(
    runtime_local_func_llvm_jit_emit_state_t& state, ::std::uint_least8_t opcode) noexcept
{
    if(state.private_numeric_function_slot == nullptr) { return; }
    if(opcode == 0xfbu)
    {
        // Only the authoritative decoded GC observer may permit this family.
        if(!state.private_numeric_gc_opcode_observed) { state.private_numeric_function_safe = false; }
        state.private_numeric_gc_opcode_observed = false;
        return;
    }
    // Numeric/local/control/memory ops cannot publish GC references. Calls,
    // tail calls, global/table writes, EH and bulk/atomic families all deny.
    bool const allowed{opcode <= 0x05u || opcode == 0x0bu || opcode == 0x0cu ||
        opcode == 0x0du || opcode == 0x0eu || opcode == 0x0fu ||
        opcode == 0x1au || opcode == 0x1bu || opcode == 0x1cu ||
        (opcode >= 0x20u && opcode <= 0x23u) || opcode == 0x25u ||
        (opcode >= 0x28u && opcode <= 0xc4u) ||
        (opcode >= 0xd0u && opcode <= 0xd6u) || opcode == 0xfdu};
    if(!allowed) { state.private_numeric_function_safe = false; }
}
#endif

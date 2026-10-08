// Internal precise native-frame lowering. Included after the concrete emit
// state, and before preparation/return/call helpers. This is not a GC handshake:
// the runtime may request it only with a complete cohort/root/reader protocol.
// Disabled lowering emits no alloca, external symbol, call, load or store.

[[nodiscard]] inline constexpr bool llvm_jit_gc_root_carrier_type(runtime_operand_stack_value_type type) noexcept
{
    auto const encoding{get_runtime_wasm_value_type_encoding(type)};
    return encoding == 0x70u || encoding == 0x6fu || encoding == 0x69u;
}

inline void retain_runtime_llvm_jit_gc_root_instruction(
    llvm_jit_gc_root_frame_emit_state_t const& roots, ::llvm::Value* value) noexcept
{
    if(auto const instruction{::llvm::dyn_cast<::llvm::Instruction>(value)}; instruction != nullptr)
    {
        // [this generated function's live LLVM instruction] module end
        // [safe                                          ] retain the handle
        // only until finalization; no optimizer has run and no guest owns it.
        roots.instructions.push_back(instruction);
    }
}

[[nodiscard]] inline bool prepare_runtime_llvm_jit_gc_root_frame(
    ::llvm::IRBuilder<>& body, ::llvm::Function& function,
    llvm_jit_gc_root_frame_emit_state_t& roots, bool enabled) noexcept
{
    if(!enabled) { return true; }
    if(roots.frame != nullptr || roots.slots != nullptr || roots.enter != nullptr) { return false; }
    namespace native = ::uwvm2::runtime::gc;
    static_assert(::std::is_trivially_copyable_v<native::root_reference>);
    static_assert(offsetof(native::root_frame, previous) == 0uz);
    static_assert(offsetof(native::root_frame, slots) == sizeof(void*));
    static_assert(offsetof(native::root_frame, capacity_and_active) == 2uz * sizeof(void*));
    static_assert(offsetof(native::root_frame, live_count) == 3uz * sizeof(void*));
    auto const& layout{function.getParent()->getDataLayout()};
    if(function.getParent()->getDataLayoutStr().empty() || layout.getPointerSize() != sizeof(::std::uintptr_t) ||
       layout.isLittleEndian() != (::std::endian::native == ::std::endian::little)) { return false; }
    auto const intptr{body.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const carrier{body.getIntNTy(static_cast<unsigned>(sizeof(native::root_reference) * CHAR_BIT))};
    if(layout.getTypeStoreSize(carrier).getFixedValue() != sizeof(native::root_reference)) { return false; }
    // [LLVM-owned entry alloca] function end
    // [safe                  ] frame handle is assigned only to the complete
    // aligned native record allocation; failure is checked before any use.
    //  ^^ roots.frame remains owned by this one generated native function.
    roots.frame = create_llvm_jit_entry_block_alloca(body, body.getInt8Ty(),
        ::llvm::ConstantInt::get(intptr, sizeof(native::root_frame)), "gc.root.frame");
    // [LLVM-owned complete carrier slots] final extent
    // [safe                             ] initial extent is one carrier;
    // finalization installs the checked maximum before verification/codegen.
    //  ^^ roots.slots never aliases guest linear memory or a caller's alloca.
    roots.slots = create_llvm_jit_entry_block_alloca(body, carrier,
        ::llvm::ConstantInt::get(intptr, 1u), "gc.root.slots");
    if(roots.frame == nullptr || roots.slots == nullptr) { return false; }
    retain_runtime_llvm_jit_gc_root_instruction(roots, roots.frame);
    retain_runtime_llvm_jit_gc_root_instruction(roots, roots.slots);
    roots.frame->setAlignment(::llvm::Align{alignof(native::root_frame)});
    roots.slots->setAlignment(::llvm::Align{alignof(native::root_reference)});
    auto& entry{function.getEntryBlock()};
    auto position{entry.getFirstInsertionPt()};
    // [LLVM-owned entry instructions ...] end
    // [safe                         ] advance past actual allocas only; record
    // and slot addresses must dominate the native placement-construction call.
    while(position != entry.end() && ::llvm::isa<::llvm::AllocaInst>(*position)) { ++position; }
    ::llvm::IRBuilder<> builder{::std::addressof(entry), position};
    auto const type{::llvm::FunctionType::get(intptr, {intptr, intptr, intptr}, false)};
    auto const bridge{get_llvm_runtime_bridge_function_symbol_value<native::uwvm_gc_root_frame_enter_checked_abi>(builder, type)};
    if(bridge == nullptr) { return false; }
    // [fresh aligned native record][complete native root slots] entry lifetime
    // [safe                       ] enter executes once in the real native
    // entry, before normal-init/OSR dispatch, never on a self-tail body backedge.
    // The internal enter/leave ABI must not request VM collection, park its
    // participant or call guest/host code: incoming reference arguments have
    // not yet been copied into these slots. Native TLS initialization is not
    // a guest allocation or a collection point. Never give this call nosync.
    //  ^^ roots.enter borrows the function-owned, nonthrowing call instruction.
    auto const frame_address{builder.CreatePtrToInt(roots.frame, intptr)};
    auto const slots_address{builder.CreatePtrToInt(roots.slots, intptr)};
    retain_runtime_llvm_jit_gc_root_instruction(roots, frame_address);
    retain_runtime_llvm_jit_gc_root_instruction(roots, slots_address);
    roots.enter = apply_llvm_jit_host_calling_conv(builder.CreateCall(type, bridge,
        {frame_address, slots_address, ::llvm::ConstantInt::get(intptr, 0u)}));
    retain_runtime_llvm_jit_gc_root_instruction(roots, roots.enter);
    roots.enter->setDoesNotThrow();
    return true;
}

// A full-width integer store writes raw LLVM slot bytes. Only the runtime
// visitor constructs a C++ reference and memcpy-copies those bytes. Numeric
// operands are never inferred to be roots based on their bit pattern.
[[nodiscard]] inline bool emit_runtime_llvm_jit_gc_root_slot(
    [[maybe_unused]] ::llvm::IRBuilder<>& builder, llvm_jit_gc_root_frame_emit_state_t& roots,
    ::llvm::Value* value, ::std::size_t index) noexcept
{
    namespace native = ::uwvm2::runtime::gc;
    if(roots.frame == nullptr || roots.slots == nullptr || value == nullptr ||
       index >= native::frame_root_details::max_capacity ||
       !value->getType()->isIntegerTy(static_cast<unsigned>(sizeof(native::root_reference) * CHAR_BIT))) { return false; }
    // Record the logical write without creating quadratic LLVM store/GEP IR.
    // Tracking handles follow any PHI replacement before final materialization.
    roots.pending_root_stores.push_back({::llvm::WeakTrackingVH{value}, index});
    return true;
}

[[nodiscard]] inline bool publish_runtime_llvm_jit_gc_root_count(
    ::llvm::IRBuilder<>& builder, llvm_jit_gc_root_frame_emit_state_t& roots,
    ::std::size_t count) noexcept
{
    namespace native = ::uwvm2::runtime::gc;
    if(roots.frame == nullptr || count > native::frame_root_details::max_capacity) { return false; }
    roots.max_slots = (::std::max)(roots.max_slots, count);
    auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    // [genuine native root_frame constructed by enter_abi] record end
    // [safe                                            ] offsetof names the
    // standard-layout live_count member. All carrier stores precede publication.
    // The participant pause supplies synchronization; no concurrent reader is
    // permitted here. No host call or extra TLS lookup is needed at this site.
    auto const address{builder.CreateInBoundsGEP(builder.getInt8Ty(), roots.frame,
        ::llvm::ConstantInt::get(intptr, offsetof(native::root_frame, live_count)), "gc.root.live")};
    auto const store{builder.CreateStore(::llvm::ConstantInt::get(intptr, count), address)};
    store->setAlignment(::llvm::Align{alignof(::std::size_t)});
    retain_runtime_llvm_jit_gc_root_instruction(roots, address);
    retain_runtime_llvm_jit_gc_root_instruction(roots, store);
    roots.snapshots.push_back({::llvm::WeakTrackingVH{store}, ::std::move(roots.pending_root_stores)});
    roots.pending_root_stores.clear();
    return true;
}

[[nodiscard]] inline bool snapshot_runtime_local_func_llvm_jit_gc_roots(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::llvm::ArrayRef<llvm_jit_stack_value_t> extra = {}) noexcept
{
    if(!state.emit_precise_gc_root_frames) { return true; }
    if(state.ir_builder == nullptr || state.local_types.size() != state.local_pointers.size()) { return false; }
    auto& builder{*state.ir_builder};
    ::std::size_t count{};
    for(::std::size_t index{}; index != state.local_types.size(); ++index)
    {
        if(!llvm_jit_gc_root_carrier_type(state.local_types[index])) { continue; }
        auto const pointer{state.local_pointers.index_unchecked(index)};
        if(pointer == nullptr) { return false; }
        auto const value{builder.CreateLoad(pointer->getAllocatedType(), pointer, "gc.root.local")};
        if(!emit_runtime_llvm_jit_gc_root_slot(builder, state.gc_root_frame, value, count++)) { return false; }
    }
    for(auto const& operand: state.operand_stack)
    {
        if(!llvm_jit_gc_root_carrier_type(operand.type)) { continue; }
        if(!emit_runtime_llvm_jit_gc_root_slot(builder, state.gc_root_frame, operand.value, count++)) { return false; }
    }
    if(state.checkpoint_observer_controls != nullptr)
    {
        for(auto const& control : state.control_stack)
        {
            // Hidden saved parameters belong only to if/else. Block/loop values
            // are current operands/header PHIs; their context entry tuple is empty.
            // Match the exact site/packet suffix partition before any storage load.
            if(control.type != llvm_jit_control_context_type::if_then && control.type != llvm_jit_control_context_type::if_else) { continue; }
            for(::std::size_t i{};i != control.entry_params.size();++i)
            {
                auto const& operand{control.entry_params.index_unchecked(i)};
                if(!llvm_jit_gc_root_carrier_type(operand.type)) { continue; }
                auto const actual{read_runtime_local_func_llvm_jit_checkpoint_saved_if(state,builder,control,i)};
                if(actual == nullptr || !emit_runtime_llvm_jit_gc_root_slot(builder,state.gc_root_frame,actual,count++)) { return false; }
            }
        }
    }
    for(auto const& operand: extra)
    {
        if(!llvm_jit_gc_root_carrier_type(operand.type)) { continue; }
        if(!emit_runtime_llvm_jit_gc_root_slot(builder, state.gc_root_frame, operand.value, count++)) { return false; }
    }
    return publish_runtime_llvm_jit_gc_root_count(builder, state.gc_root_frame, count);
}

[[nodiscard]] inline bool leave_runtime_llvm_jit_gc_root_frame(
    ::llvm::IRBuilder<>& builder, llvm_jit_gc_root_frame_emit_state_t const& roots) noexcept
{
    if(roots.frame == nullptr) { return true; }
    auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const type{::llvm::FunctionType::get(intptr, {intptr}, false)};
    auto const bridge{get_llvm_runtime_bridge_function_symbol_value<
        ::uwvm2::runtime::gc::uwvm_gc_root_frame_leave_checked_abi>(builder, type)};
    if(bridge == nullptr) { return false; }
    // [one live native activation record] leave precedes its storage retirement,
    // including cold exceptional exits and genuine musttail call instructions.
    auto const frame_address{builder.CreatePtrToInt(roots.frame, intptr)};
    auto const call{apply_llvm_jit_host_calling_conv(builder.CreateCall(type, bridge, {frame_address}))};
    call->setDoesNotThrow();
    retain_runtime_llvm_jit_gc_root_instruction(roots, frame_address);
    retain_runtime_llvm_jit_gc_root_instruction(roots, call);
    return true;
}

// Snapshot descriptors hold tracking handles until the complete CFG exists.
// Emit only exact changed slot values; joins/backedges start unknown. The native
// const-slot ABI and nonmoving collector cannot rewrite this private storage.
[[nodiscard]] inline bool materialize_runtime_llvm_jit_gc_root_snapshots(
    llvm_jit_gc_root_frame_emit_state_t& roots) noexcept
{
    if(!roots.pending_root_stores.empty()) { return false; }
    if(roots.snapshots.empty()) { return true; }
    if(roots.slots == nullptr) { return false; }
    using values_type = ::llvm::SmallVector<::llvm::Value*, 8u>;
    ::llvm::DenseMap<::llvm::Instruction*, llvm_jit_gc_root_snapshot_t*> plans{};
    for(auto& snapshot: roots.snapshots)
    {
        auto* anchor{::llvm::dyn_cast_or_null<::llvm::Instruction>(snapshot.anchor)};
        if(anchor == nullptr || anchor->getFunction() != roots.slots->getFunction() ||
           !plans.insert({anchor, ::std::addressof(snapshot)}).second) { return false; }
    }
    ::llvm::DenseMap<::llvm::BasicBlock const*, values_type> outgoing{};
    for(auto& block: *roots.slots->getFunction())
    {
        values_type known{};
        if(auto const* predecessor{block.getSinglePredecessor()}; predecessor != nullptr)
        {
            auto const found{outgoing.find(predecessor)};
            if(found != outgoing.end()) { known = found->second; }
        }
        for(auto& instruction: block)
        {
            auto const found{plans.find(::std::addressof(instruction))};
            if(found == plans.end()) { continue; }
            ::llvm::IRBuilder<> builder{::std::addressof(instruction)};
            auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
            for(auto const& slot: found->second->stores)
            {
                ::llvm::Value* value{slot.value};
                if(value == nullptr || slot.index >= roots.max_slots ||
                   !value->getType()->isIntegerTy(static_cast<unsigned>(
                       sizeof(::uwvm2::runtime::gc::root_reference) * CHAR_BIT))) { return false; }
                if(known.size() <= slot.index) { known.resize(slot.index + 1uz, nullptr); }
                if(known[slot.index] == value) { continue; }
                auto const address{builder.CreateInBoundsGEP(value->getType(), roots.slots,
                    ::llvm::ConstantInt::get(intptr, slot.index), "gc.root.slot")};
                auto const store{builder.CreateStore(value, address)};
                store->setAlignment(::llvm::Align{alignof(::uwvm2::runtime::gc::root_reference)});
                retain_runtime_llvm_jit_gc_root_instruction(roots, address);
                retain_runtime_llvm_jit_gc_root_instruction(roots, store);
                known[slot.index] = value;
            }
        }
        outgoing[::std::addressof(block)] = ::std::move(known);
    }
    roots.snapshots.clear();
    return true;
}

[[nodiscard]] inline bool finalize_runtime_llvm_jit_gc_root_frame(
    llvm_jit_gc_root_frame_emit_state_t& roots, bool remove_empty = false) noexcept
{
    if(roots.frame == nullptr) { return roots.slots == nullptr && roots.enter == nullptr; }
    if(roots.slots == nullptr || roots.enter == nullptr || roots.enter->arg_size() != 3u ||
       roots.max_slots > ::uwvm2::runtime::gc::frame_root_details::max_capacity) { return false; }
    if(!materialize_runtime_llvm_jit_gc_root_snapshots(roots)) { return false; }
    auto const intptr{roots.enter->getArgOperand(2u)->getType()};
    // [entry native slots] finalize the complete immutable extent before any
    // LLVM verifier, optimizer, object cache or native entry publication runs.
    roots.slots->setOperand(0u, ::llvm::ConstantInt::get(intptr, (::std::max)(roots.max_slots, 1uz)));
    roots.enter->setArgOperand(2u, ::llvm::ConstantInt::get(intptr, roots.max_slots));
    if(remove_empty && roots.max_slots == 0uz)
    {
        // All typed snapshots, including restored OSR locals and cold handler
        // payloads, have been emitted. A zero maximum proves that this native
        // activation never owns a Wasm reference across any collection point.
        // Numeric/v128 values are not roots. Remove only our own IR, after
        // proving that each use is also ours; never erase arbitrary user IR.
        ::llvm::SmallPtrSet<::llvm::Instruction*, 32u> owned{};
        for(auto const instruction: roots.instructions)
        {
            if(instruction == nullptr || instruction->getFunction() != roots.frame->getFunction() ||
               !owned.insert(instruction).second) { return false; }
        }
        for(auto const instruction: roots.instructions)
        {
            for(auto const user: instruction->users())
            {
                auto const user_instruction{::llvm::dyn_cast<::llvm::Instruction>(user)};
                if(user_instruction == nullptr || !owned.contains(user_instruction)) { return false; }
            }
        }
        for(::std::size_t remaining{roots.instructions.size()}; remaining != 0uz;)
        {
            // [retained compiler-owned instruction handles] first ... last
            // [safe                                       ] reverse creation
            // order retires every user before its defining allocation/address.
            auto const instruction{roots.instructions.index_unchecked(--remaining)};
            if(!instruction->use_empty()) { return false; }
            instruction->eraseFromParent();
        }
        roots.instructions.clear();
        // [retired IR handles] no instruction is read through these aliases.
        // [safe              ] replace every dangling handle with native null.
        //  ^^ the subsequent verifier sees no root record or root ABI call.
        roots.frame = nullptr;
        roots.slots = nullptr;
        roots.enter = nullptr;
    }
    return true;
}

// A tail-entered host adapter owns its parameter roots after the outgoing
// Wasm frame is gone. Retire that independent record if the host call throws;
// catching in another Wasm frame must never leave this native stack link live.
[[nodiscard]] inline ::llvm::CallBase* emit_runtime_llvm_jit_rooted_host_adapter_call(
    runtime_local_func_llvm_jit_emit_state_t const& state,
    ::llvm::IRBuilder<>& builder, ::llvm::FunctionType* type, ::llvm::Value* callee,
    ::llvm::ArrayRef<::llvm::Value*> arguments,
    llvm_jit_gc_root_frame_emit_state_t const& roots) noexcept
{
    if(roots.frame == nullptr) { return builder.CreateCall(type, callee, arguments); }
#ifdef UWVM_CPP_EXCEPTIONS
    if(!state.native_guest_exceptions) { return nullptr; }
    auto const function{builder.GetInsertBlock()->getParent()};
    function->setPersonalityFn(state.native_exception_imports.personality);
    auto const origin{builder.saveIP()};
    // [this adapter's native lifetime] continuation and cleanup remain owned
    // by that function; no caller-frame alloca is reused across musttail.
    auto const normal{::llvm::BasicBlock::Create(builder.getContext(), "gc.host.return", function)};
    auto const unwind{::llvm::BasicBlock::Create(builder.getContext(), "gc.host.cleanup", function)};
    builder.SetInsertPoint(unwind);
    auto const record{builder.CreateLandingPad(::llvm::StructType::get(builder.getContext(),
        {builder.getPtrTy(), builder.getInt32Ty()}), 0u)};
    record->setCleanup(true);
    if(!leave_runtime_llvm_jit_gc_root_frame(builder, roots)) { return nullptr; }
    builder.CreateResume(record);
    // [original nonterminated host-call block] restore its live LLVM cursor.
    builder.restoreIP(origin);
    auto const invoke{builder.CreateInvoke(type, callee, normal, unwind, arguments)};
    // [successful host-return continuation] load outputs only on this edge.
    builder.SetInsertPoint(normal);
    return invoke;
#else
    static_cast<void>(state);
    return builder.CreateCall(type, callee, arguments);
#endif
}

// Included inside the single-function emitter namespace after branch/stack helpers.
// This is execution lowering, not a second exception object or ownership model.

[[nodiscard]] inline constexpr bool llvm_jit_exception_numeric_tuple_bytes(runtime_block_result_type params,
                                                                           ::std::size_t& bytes) noexcept
{
    bytes = 0uz;
    auto const count{get_runtime_block_result_count(params)};
    for(::std::size_t index{}; index != count; ++index)
    {
        auto const type{params.begin[index]};
        if(type != runtime_operand_stack_value_type::i32 && type != runtime_operand_stack_value_type::i64 &&
           type != runtime_operand_stack_value_type::f32 && type != runtime_operand_stack_value_type::f64 &&
           type != runtime_operand_stack_value_type::v128) { return false; }
        auto const width{get_runtime_wasm_value_type_abi_size(type)};
        if(width == 0uz || width > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) - bytes) { return false; }
        bytes += width;
    }
    return true;
}

[[nodiscard]] inline constexpr bool llvm_jit_exception_tuple_layout(runtime_block_result_type params,
    ::std::size_t count, ::std::size_t& bytes, bool& contains_reference) noexcept
{
    if(count > get_runtime_block_result_count(params)) { return false; }
    bytes = 0uz;
    contains_reference = false;
    for(::std::size_t index{}; index != count; ++index)
    {
        // [validated immutable tuple: count entries] one-past
        // [safe                              ] index < count was checked above.
        //                                   ^^ begin[index] borrows one complete carrier.
        auto const type{params.begin[index]};
        if(!is_runtime_wasm_value_type_llvm_storage_supported(type)) { return false; }
        auto const encoding{get_runtime_wasm_value_type_encoding(type)};
        contains_reference |= encoding == 0x70u || encoding == 0x6fu || encoding == 0x69u;
        auto const width{get_runtime_wasm_value_type_abi_size(type)};
        if(width == 0uz || width > static_cast<::std::size_t>((::std::numeric_limits<::std::ptrdiff_t>::max)()) - bytes)
        { return false; }
        bytes += width;
    }
    return true;
}

[[nodiscard]] inline constexpr bool llvm_jit_exception_owns_logical_frame(runtime_local_func_llvm_jit_emit_state_t const& state) noexcept
{
    return state.emit_call_stack_frames && (!state.emit_tiered_loop_reentry_entries ||
        state.llvm_function->getCallingConv() == ::llvm::CallingConv::Tail);
}

[[nodiscard]] inline constexpr bool try_record_runtime_local_func_llvm_jit_exception_handlers(
    runtime_local_func_llvm_jit_emit_state_t& state,
    ::uwvm2::runtime::compiler::shared::wasm_exception_control::handlers const& handlers,
    ::std::size_t outer_frame_count) noexcept
{
    if(!state.valid || state.control_stack.empty()) { return false; }
    if(state.unreachable_control_depth != 0uz || !state.control_stack.back().is_reachable) { return true; }
    if(state.branch_target_stack.size() != outer_frame_count + 1uz || state.local_func_storage_ptr == nullptr) { return false; }
    auto const module{state.local_func_storage_ptr->runtime_module_ptr};
    if(module == nullptr) { return false; }
    auto& output{state.control_stack.back().exception_handlers};
    output.reserve(handlers.size());
    for(auto const& handler: handlers)
    {
        if(state.pending_numeric_plan != nullptr && handler.with_reference) { return false; }
        if(handler.target_frame >= outer_frame_count) { return false; }
        // [live outer branch targets][new try target] end
        // [safe                                    ] target_frame < outer_frame_count.
        // The record copies LLVM-owned handles; no vector-element address escapes.
        auto const& target{state.branch_target_stack.index_unchecked(handler.target_frame)};
        ::std::size_t tag_index{SIZE_MAX};
        if(handler.identity != nullptr)
        {
            for(::std::size_t index{}; index != module->imported_tag_vec_storage.size(); ++index)
            {
                if(module->imported_tag_vec_storage.index_unchecked(index).resolved_tag == handler.identity)
                { tag_index = index; break; }
            }
            if(tag_index == SIZE_MAX)
            {
                for(::std::size_t index{}; index != module->local_defined_tag_vec_storage.size(); ++index)
                {
                    // [local tag records] end; index < size, so addressof borrows
                    // [safe             ] an initialized record in the pinned module.
                    if(::std::addressof(module->local_defined_tag_vec_storage.index_unchecked(index)) == handler.identity)
                    { tag_index = module->imported_tag_vec_storage.size() + index; break; }
                }
            }
            if(tag_index == SIZE_MAX) { return false; }
        }
        output.push_back({.catch_all = handler.identity == nullptr, .with_reference = handler.with_reference, .tag_index = tag_index,
            .params = target.params, .block = target.block, .phis = target.phis, .control_stack_index = target.control_stack_index});
    }
    return true;
}

#ifdef UWVM_CPP_EXCEPTIONS
// Emit one no-throw cold copy while the typed C++ catch still owns its exception.
// Reference payloads and catch_ref use complete native Wasm reference carriers, never a
// native exception activation pointer or a pointer-width canonical identity.
[[nodiscard]] inline bool emit_llvm_jit_caught_tuple(runtime_local_func_llvm_jit_emit_state_t& state,
    llvm_jit_exception_handler_t const& handler, ::llvm::Value* guest_object,
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t>& values) noexcept
{
    auto const target_count{get_runtime_block_result_count(handler.params)};
    if(handler.with_reference &&
       (target_count == 0uz || get_runtime_wasm_value_type_encoding(handler.params.begin[target_count - 1uz]) != 0x69u))
    { return false; }
    auto const payload_count{target_count - static_cast<::std::size_t>(handler.with_reference)};
    if(handler.catch_all && payload_count != 0uz) { return false; }
    ::std::size_t bytes{};
    bool contains_reference{};
    if(!llvm_jit_exception_tuple_layout(handler.params, payload_count, bytes, contains_reference)) { return false; }
    auto& builder{*state.ir_builder};
    auto& context{builder.getContext()};
    auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    ::llvm::Value* address{::llvm::ConstantInt::get(intptr, 0u)};
    ::llvm::AllocaInst* buffer{};
    if(bytes != 0uz)
    {
        // [private caller alloca: bytes] end; the checked layout bounds every
        // [safe                      ] cold helper write and typed load below.
        buffer = create_llvm_jit_entry_block_alloca(builder, builder.getInt8Ty(), ::llvm::ConstantInt::get(intptr, bytes), "guest.eh.payload");
        if(buffer == nullptr) { return false; }
        // [same private alloca] converting to the bridge's integer carrier does
        // [safe              ] not dereference or widen its actual byte extent.
        address = builder.CreatePtrToInt(buffer, intptr);
    }
    if(!handler.catch_all)
    {
        auto const copy_type{::llvm::FunctionType::get(builder.getVoidTy(), {intptr, intptr, intptr, intptr, intptr}, false)};
        auto const copy{contains_reference ?
            get_llvm_runtime_bridge_function_symbol_value<
                ::uwvm2::runtime::lib::details::llvm_jit_exception_copy_tuple_abi_bridge>(builder, copy_type) :
            get_llvm_runtime_bridge_function_symbol_value<
                ::uwvm2::runtime::lib::details::llvm_jit_exception_copy_numeric_payload_abi_bridge>(builder, copy_type)};
        if(copy == nullptr) { return false; }
        auto const copied{apply_llvm_jit_host_calling_conv(builder.CreateCall(copy_type, copy,
            {builder.CreatePtrToInt(guest_object, intptr), ::llvm::ConstantInt::get(intptr, state.local_func_storage_ptr->module_id),
             ::llvm::ConstantInt::get(intptr, handler.tag_index), address, ::llvm::ConstantInt::get(intptr, bytes)}))};
        copied->setDoesNotThrow();
    }
    values.reserve(target_count);
    ::std::size_t offset{};
    for(::std::size_t index{}; index != payload_count; ++index)
    {
        auto const type{handler.params.begin[index]};
        auto const llvm_type{get_llvm_type_from_wasm_value_type(context, type)};
        auto const width{get_runtime_wasm_value_type_abi_size(type)};
        if(llvm_type == nullptr || width > bytes - offset) { return false; }
        // [payload prefix: offset][field: width][remaining] end
        // [safe                                        ] offset + width <= bytes.
        auto const field{builder.CreateInBoundsGEP(builder.getInt8Ty(), buffer, ::llvm::ConstantInt::get(intptr, offset))};
        auto const carrier{get_llvm_jit_scalar_bits_type(llvm_type)};
        auto const loaded{builder.CreateLoad(carrier, field)};
        loaded->setAlignment(::llvm::Align{1u});
        auto const value{carrier == llvm_type ? static_cast<::llvm::Value*>(loaded) : builder.CreateBitCast(loaded, llvm_type)};
        values.push_back({.type = type, .value = value});
        offset += width;
    }
    if(offset != bytes) { return false; }
    if(handler.with_reference)
    {
        // Typed payload carriers are temporarily in `values`, outside the
        // normal operand model. Preserve them before the exnref issuer runs.
        if(!snapshot_runtime_local_func_llvm_jit_gc_roots(state, {values.data(), values.size()})) { return false; }
        auto const reference_type{get_llvm_type_from_wasm_value_type(context,
            static_cast<runtime_operand_stack_value_type>(0x69u))};
        if(reference_type == nullptr ||
           !reference_type->isIntegerTy(static_cast<unsigned>(sizeof(::uwvm2::uwvm::runtime::storage::gc_reference) * CHAR_BIT)))
        { return false; }
        // [entry alloca: one complete Wasm reference] end
        // [safe                                    ] the bridge writes exactly sizeof(gc_reference) bytes before the load.
        auto const reference_slot{create_llvm_jit_entry_block_alloca(builder, reference_type, nullptr, "guest.eh.reference")};
        if(reference_slot == nullptr) { return false; }
        auto const make_type{::llvm::FunctionType::get(builder.getVoidTy(), {intptr, intptr, intptr}, false)};
        auto const make{get_llvm_runtime_bridge_function_symbol_value<
            ::uwvm2::runtime::lib::details::llvm_jit_exception_make_ref_abi_bridge>(builder, make_type)};
        if(make == nullptr) { return false; }
        auto const issued{apply_llvm_jit_host_calling_conv(builder.CreateCall(make_type, make,
            {builder.CreatePtrToInt(guest_object, intptr), ::llvm::ConstantInt::get(intptr, state.local_func_storage_ptr->module_id),
             builder.CreatePtrToInt(reference_slot, intptr)}))};
        issued->setDoesNotThrow();
        auto const reference{builder.CreateLoad(reference_type, reference_slot)};
        values.push_back({.type = static_cast<runtime_operand_stack_value_type>(0x69u), .value = reference});
    }
    return values.size() == target_count;
}
#endif

// This hook is only for potentially throwing Wasm calls. Trap/memory helpers
// and musttail dispatch never use it. An invoke changes exceptional control flow
// without installing a runtime handler stack on the normal edge.
[[nodiscard]] inline ::llvm::CallBase* emit_runtime_local_func_llvm_jit_may_throw_call(
    runtime_local_func_llvm_jit_emit_state_t& state, ::llvm::FunctionType* type,
    ::llvm::Value* callee, ::llvm::ArrayRef<::llvm::Value*> arguments,
    bool include_lexical_handlers = true) noexcept
{
    if(!state.valid || state.ir_builder == nullptr || type == nullptr || callee == nullptr) { return nullptr; }
    auto& builder{*state.ir_builder};
#ifdef UWVM_CPP_EXCEPTIONS
    if(state.native_guest_exceptions)
    {
        bool has_handlers{};
        if(include_lexical_handlers)
        {
            for(auto const& frame: state.control_stack) { has_handlers |= !frame.exception_handlers.empty(); }
        }
        auto const owns_frame{llvm_jit_exception_owns_logical_frame(state)};
        auto const owns_gc_frame{state.gc_root_frame.frame != nullptr};
        auto const owns_debug_activation{state.debug_activation_enabled};
        if(has_handlers || owns_frame || owns_gc_frame || owns_debug_activation)
        {
            namespace landing = ::uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad;
            auto const origin{builder.saveIP()};
            auto const function{builder.GetInsertBlock()->getParent()};
            auto const normal{::llvm::BasicBlock::Create(builder.getContext(), "guest.call.return", function)};
            ::llvm::BasicBlock* unwind{};
            bool cleanup_valid{true};
            auto const cleanup{[&](::llvm::IRBuilder<>& cold) noexcept
            {
                if(owns_debug_activation)
                { cleanup_valid &= emit_runtime_local_func_llvm_jit_debug_activation_leave(cold, state, 3u); }
                if(owns_gc_frame) { cleanup_valid &= leave_runtime_llvm_jit_gc_root_frame(cold, state.gc_root_frame); }
                if(owns_frame) { cleanup_valid &= emit_runtime_local_func_llvm_jit_call_stack_pop(cold); }
            }};
            if(!has_handlers)
            {
                function->setPersonalityFn(state.native_exception_imports.personality);
                // [function-owned cleanup block] builder moves into its live
                // [safe                        ] first insertion point.
                unwind = ::llvm::BasicBlock::Create(builder.getContext(), "guest.call.cleanup", function);
                builder.SetInsertPoint(unwind);
                auto const record{builder.CreateLandingPad(::llvm::StructType::get(builder.getContext(),
                    {builder.getPtrTy(), builder.getInt32Ty()}), 0u)};
                record->setCleanup(true);
                cleanup(builder);
                builder.CreateResume(record);
            }
            else
            {
                auto const entry{landing::emit_typed_entry(builder, *function, state.native_exception_imports,
                    state.native_exception_runtime, cleanup)};
                if(!entry) { return nullptr; }
                // [function-owned landing block] retained only in this LLVM
                // [safe                       ] function and its invoke edges.
                unwind = entry.landing;
                bool terminal{};
                for(auto depth{state.control_stack.size()}; depth != 0uz && !terminal; --depth)
                {
                    for(auto const& handler: state.control_stack.index_unchecked(depth - 1uz).exception_handlers)
                    {
                        ::llvm::BasicBlock* next{};
                        if(!handler.catch_all)
                        {
                            auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
                            auto const match_type{::llvm::FunctionType::get(intptr, {intptr, intptr, intptr}, false)};
                            auto const match{get_llvm_runtime_bridge_function_symbol_value<
                                ::uwvm2::runtime::lib::details::llvm_jit_exception_matches_tag_abi_bridge>(builder, match_type)};
                            if(match == nullptr) { return nullptr; }
                            auto const matched{apply_llvm_jit_host_calling_conv(builder.CreateCall(match_type, match,
                                {builder.CreatePtrToInt(entry.guest_object, intptr),
                                 ::llvm::ConstantInt::get(intptr, state.local_func_storage_ptr->module_id),
                                 ::llvm::ConstantInt::get(intptr, handler.tag_index)}))};
                            matched->setDoesNotThrow();
                            auto const selected{::llvm::BasicBlock::Create(builder.getContext(), "guest.eh.selected", function)};
                            // [function-owned next clause block] valid until
                            // [safe                            ] this function is destroyed.
                            next = ::llvm::BasicBlock::Create(builder.getContext(), "guest.eh.next", function);
                            builder.CreateCondBr(builder.CreateICmpNE(matched, ::llvm::ConstantInt::get(intptr, 0u)), selected, next);
                            // [function-owned selected block] the checked match
                            // [safe                         ] edge is its sole predecessor.
                            builder.SetInsertPoint(selected);
                        }
                        ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> values{};
                        if(!emit_llvm_jit_caught_tuple(state, handler, entry.guest_object, values)) { return nullptr; }
                        // The catch can now release its native value owner.
                        // Publish both payload references and a new exnref
                        // before end_catch/branch ownership moves to PHIs.
                        if(!snapshot_runtime_local_func_llvm_jit_gc_roots(state, {values.data(), values.size()})) { return nullptr; }
                        // Payload values are now owning LLVM SSA bits. End the native catch
                        // before entering a Wasm continuation that can throw again.
                        landing::emit_end_catch(builder, state.native_exception_runtime);
                        llvm_jit_branch_target_t const target{.params = handler.params, .block = handler.block,
                            .phis = handler.phis, .control_stack_index = handler.control_stack_index};
                        if(!try_add_runtime_local_func_llvm_jit_branch_target_incoming(state, target, values, builder.GetInsertBlock())) { return nullptr; }
                        builder.CreateBr(target.block);
                        if(handler.catch_all) { terminal = true; break; }
                        // [function-owned unmatched-clause block] resume only the
                        // [safe                                ] cold clause search, not the normal stack model.
                        builder.SetInsertPoint(next);
                    }
                }
                if(!terminal)
                {
                    auto const caught_cleanup{landing::emit_caught_exit_cleanup(builder, *function, state.native_exception_runtime, cleanup)};
                    landing::emit_rethrow(builder, state.native_exception_runtime, caught_cleanup);
                }
            }
            if(!cleanup_valid) { return nullptr; }
            // [original nonterminated call block] saved before cold IR emission;
            // [safe                            ] LLVM still owns its insertion position.
            builder.restoreIP(origin);
            auto const invoke{builder.CreateInvoke(type, callee, normal, unwind, arguments)};
            // [normal return block] every result load and following guest opcode
            // [safe              ] is emitted only on the normal invoke edge.
            builder.SetInsertPoint(normal);
            return invoke;
        }
    }
#else
    static_cast<void>(include_lexical_handlers);
#endif
    return builder.CreateCall(type, callee, arguments);
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_throw_tuple(runtime_local_func_llvm_jit_emit_state_t& state,
    ::std::size_t tag_index, runtime_block_result_type params) noexcept
{
#ifdef UWVM_CPP_EXCEPTIONS
    if(!state.valid || !state.native_guest_exceptions || state.ir_builder == nullptr || state.control_stack.empty()) { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.pending_numeric_plan != nullptr) { return try_emit_pending_numeric_throw_tuple(state, tag_index, params); }
    auto const count{get_runtime_block_result_count(params)};
    ::std::size_t bytes{};
    bool contains_reference{};
    if(state.operand_stack.size() < count ||
       !llvm_jit_exception_tuple_layout(params, count, bytes, contains_reference)) { return false; }
    // [current typed locals/prefix/complete throw arguments] bridge entry
    // [safe                                                ] the runtime
    // allocates immutable payload owners before native propagation. Preserve
    // roots before that bridge can enter a collection/pause protocol; it does
    // not pass through ordinary call-operand preparation.
    if(!snapshot_runtime_local_func_llvm_jit_gc_roots(state)) { return false; }
    auto& builder{*state.ir_builder};
    auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    ::llvm::AllocaInst* buffer{};
    ::llvm::Value* address{::llvm::ConstantInt::get(intptr, 0u)};
    if(bytes != 0uz)
    {
        // [private throw tuple: bytes] end; checked value widths fit PTRDIFF_MAX.
        // [safe                     ] Only the synchronous throw bridge borrows it.
        buffer = create_llvm_jit_entry_block_alloca(builder, builder.getInt8Ty(), ::llvm::ConstantInt::get(intptr, bytes), "guest.throw.payload");
        if(buffer == nullptr) { return false; }
        // [same tuple allocation] no range change, only ABI integer conversion.
        address = builder.CreatePtrToInt(buffer, intptr);
    }
    auto const first{state.operand_stack.size() - count};
    ::std::size_t offset{};
    for(::std::size_t index{}; index != count; ++index)
    {
        auto const& source{state.operand_stack.index_unchecked(first + index)};
        if(source.type != params.begin[index] || source.value == nullptr) { return false; }
        auto const type{get_llvm_type_from_wasm_value_type(builder.getContext(), source.type)};
        if(type == nullptr || source.value->getType() != type) { return false; }
        auto const width{get_runtime_wasm_value_type_abi_size(source.type)};
        if(width > bytes - offset) { return false; }
        // [tuple prefix: offset][field: width][remaining] end
        // [safe                                      ] offset + width <= bytes.
        auto const destination{builder.CreateInBoundsGEP(builder.getInt8Ty(), buffer, ::llvm::ConstantInt::get(intptr, offset))};
        auto const carrier{get_llvm_jit_scalar_bits_type(type)};
        auto const value{carrier == type ? source.value : builder.CreateBitCast(source.value, carrier)};
        auto const stored{builder.CreateStore(value, destination)};
        stored->setAlignment(::llvm::Align{1u});
        offset += width;
    }
    auto const throw_type{::llvm::FunctionType::get(builder.getVoidTy(), {intptr, intptr, intptr, intptr}, false)};
    // These debug-only observers retain the current Wasm activation until
    // unwind. A foreign-host wrapper would clear its real wait point first.
    auto const raise{state.emit_debug_safe_points ? (contains_reference ?
        get_llvm_runtime_bridge_function_symbol_value_unwrapped<
            ::uwvm2::runtime::lib::details::llvm_jit_debug_throw_tuple_abi_bridge>(builder, throw_type) :
        get_llvm_runtime_bridge_function_symbol_value_unwrapped<
            ::uwvm2::runtime::lib::details::llvm_jit_debug_throw_numeric_abi_bridge>(builder, throw_type)) : (contains_reference ?
        get_llvm_runtime_bridge_function_symbol_value<
            ::uwvm2::runtime::lib::details::llvm_jit_throw_tuple_abi_bridge>(builder, throw_type) :
        get_llvm_runtime_bridge_function_symbol_value<
            ::uwvm2::runtime::lib::details::llvm_jit_throw_numeric_abi_bridge>(builder, throw_type))};
    if(raise == nullptr) { return false; }
    // A direct no-reference lexical catch was compiled as a branch. Any
    // *_ref catch needs the actual exception value, so keep the lexical
    // landingpad for the remaining dynamic native edge.
    auto const thrown{apply_llvm_jit_host_calling_conv(emit_runtime_local_func_llvm_jit_may_throw_call(state, throw_type, raise,
        {::llvm::ConstantInt::get(intptr, state.local_func_storage_ptr->module_id), ::llvm::ConstantInt::get(intptr, tag_index),
         address, ::llvm::ConstantInt::get(intptr, bytes)}, true))};
    if(thrown == nullptr) { return false; }
    thrown->setDoesNotReturn();
    builder.CreateUnreachable();
    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
#else
    static_cast<void>(state); static_cast<void>(tag_index); static_cast<void>(params);
    return false;
#endif
}

[[nodiscard]] inline bool try_emit_runtime_local_func_llvm_jit_throw_ref(
    runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
#ifdef UWVM_CPP_EXCEPTIONS
    if(!state.valid || !state.native_guest_exceptions || state.ir_builder == nullptr || state.control_stack.empty() ||
       state.local_func_storage_ptr == nullptr) { return false; }
    if(!state.control_stack.back().is_reachable) { return true; }
    if(state.pending_numeric_plan != nullptr) { return false; }
    if(state.operand_stack.empty()) { return false; }
    // [live SSA operand stack ...][exnref] end
    // [safe                          ] back() borrows one proven entry; the vector is not modified until control is terminal.
    auto const operand{state.operand_stack.back()};
    if(get_runtime_wasm_value_type_encoding(operand.type) != 0x69u || operand.value == nullptr) { return false; }
    auto& builder{*state.ir_builder};
    auto const reference_type{get_llvm_type_from_wasm_value_type(builder.getContext(), operand.type)};
    if(reference_type == nullptr || operand.value->getType() != reference_type ||
       !reference_type->isIntegerTy(static_cast<unsigned>(sizeof(::uwvm2::uwvm::runtime::storage::gc_reference) * CHAR_BIT)))
    { return false; }
    if(auto const constant{::llvm::dyn_cast<::llvm::ConstantInt>(operand.value)}; constant != nullptr && constant->isZero())
    { return try_emit_runtime_local_func_llvm_jit_null_throw_ref(state); }
    // [current complete exnref and surrounding typed roots] bridge entry
    // [safe                                               ] publishing the
    // previous allocation snapshot would omit a newly produced exception.
    // Inflight native payload owners still require independent enumeration.
    if(!snapshot_runtime_local_func_llvm_jit_gc_roots(state)) { return false; }
    // [private entry alloca: complete native-width exnref] end
    // [safe                                         ] the synchronous bridge only borrows this initialized slot.
    auto const slot{create_llvm_jit_entry_block_alloca(builder, reference_type, nullptr, "guest.throw.reference")};
    if(slot == nullptr) { return false; }
    builder.CreateStore(operand.value, slot);
    auto const intptr{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const throw_type{::llvm::FunctionType::get(builder.getVoidTy(), {intptr, intptr}, false)};
    auto const raise{state.emit_debug_safe_points ? get_llvm_runtime_bridge_function_symbol_value_unwrapped<
        ::uwvm2::runtime::lib::details::llvm_jit_debug_throw_ref_abi_bridge>(builder, throw_type) :
        get_llvm_runtime_bridge_function_symbol_value<
            ::uwvm2::runtime::lib::details::llvm_jit_throw_ref_abi_bridge>(builder, throw_type)};
    if(raise == nullptr) { return false; }
    auto const thrown{apply_llvm_jit_host_calling_conv(emit_runtime_local_func_llvm_jit_may_throw_call(state,
        throw_type, raise, {::llvm::ConstantInt::get(intptr, state.local_func_storage_ptr->module_id),
            builder.CreatePtrToInt(slot, intptr)}, true))};
    if(thrown == nullptr) { return false; }
    thrown->setDoesNotReturn();
    builder.CreateUnreachable();
    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
#else
    static_cast<void>(state);
    return false;
#endif
}

#include "single_func_pending_numeric_emit.h"

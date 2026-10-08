// PRIVATE actual validated-stream numeric exception experiment.
// Included after ordinary EH/branch helpers; no production import.
// An owned numeric cohort binds every native declaration in its own actual
// engine before finalization. Never publish its generation-bound plan address
// in the process DynamicLibrary symbol map, even for an unused declaration.
template<auto Native>
[[nodiscard]] inline ::llvm::Value* get_owned_pending_numeric_native_declaration(
    ::llvm::IRBuilder<>& builder, ::llvm::FunctionType* type,
    ::uwvm2::utils::container::u8string_view semantic) noexcept
{
    if(type == nullptr || semantic.empty()) { return nullptr; }
    auto const block{builder.GetInsertBlock()};
    auto const function{block == nullptr ? nullptr : block->getParent()};
    auto const module{function == nullptr ? nullptr : function->getParent()};
    if(module == nullptr || get_llvm_runtime_bridge_function_address(Native) == 0u) { return nullptr; }
    auto const name{get_llvm_runtime_bridge_function_symbol_name<Native>(type, semantic)};
    auto const spelling{get_llvm_string_ref(name)};
    if(auto const named{module->getNamedValue(spelling)}; named != nullptr)
    {
        auto const declaration{::llvm::dyn_cast<::llvm::Function>(named)};
        if(declaration == nullptr || !declaration->isDeclaration() ||
           declaration->getFunctionType() != type || declaration->getCallingConv() != ::llvm::CallingConv::C)
        { return nullptr; }
        return declaration;
    }
    return ::llvm::Function::Create(type, ::llvm::Function::ExternalLinkage, spelling, module);
}

[[nodiscard]] inline ::llvm::Value* get_owned_pending_numeric_plan_address(
    ::llvm::IRBuilder<>& builder, ::uwvm2::utils::container::u8string_view name) noexcept
{
    if(name.empty()) { return nullptr; }
    auto const block{builder.GetInsertBlock()};
    auto const function{block == nullptr ? nullptr : block->getParent()};
    auto const module{function == nullptr ? nullptr : function->getParent()};
    if(module == nullptr) { return nullptr; }
    auto const spelling{get_llvm_string_ref(name)};
    ::llvm::GlobalVariable* declaration{};
    if(auto const named{module->getNamedValue(spelling)}; named != nullptr)
    {
        // [actual module-owned named IR value][checked GlobalVariable borrow]
        // [safe                                                         ] No
        // range changes; dyn_cast rejects a function/alias with the same name.
        declaration = ::llvm::dyn_cast<::llvm::GlobalVariable>(named);
        if(declaration == nullptr || !declaration->isDeclaration() ||
           declaration->getValueType() != builder.getInt8Ty() || declaration->getAddressSpace() != 0u)
        { return nullptr; }
    }
    else
    {
        // [actual module-owned new extern global][same module lifetime]
        // [safe                                                     ] Handle
        // assignment does not move/alias the separately owned native plan.
        declaration = ::llvm::dyn_cast<::llvm::GlobalVariable>(module->getOrInsertGlobal(spelling, builder.getInt8Ty()));
        if(declaration == nullptr || declaration->getAddressSpace() != 0u) { return nullptr; }
        declaration->setLinkage(::llvm::GlobalValue::ExternalLinkage);
        declaration->setInitializer(nullptr);
    }
    // [actual engine's extern plan declaration][same retained canonical owner]
    // [safe                                                            ] No
    // dereference or range change: the engine-local map resolves this integer
    // carrier before any generated entry becomes executable/published.
    auto const word{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    return builder.CreatePtrToInt(declaration, word, "pending.owned.plan.address");
}
template<auto Native>
[[nodiscard]] inline ::llvm::CallInst* emit_pending_numeric_leaf(
    ::llvm::IRBuilder<>& builder, ::uwvm2::utils::container::u8string_view semantic,
    ::llvm::ArrayRef<::llvm::Value*> arguments) noexcept
{
    namespace bridge = ::uwvm2::runtime::exception::pending_experiment::numeric_guest_bridge;
    static_assert(::std::is_same_v<decltype(Native), bridge::unary_abi> ||
        ::std::is_same_v<decltype(Native), bridge::binary_abi> ||
        ::std::is_same_v<decltype(Native), bridge::ternary_abi> ||
        ::std::is_same_v<decltype(Native), bridge::quaternary_abi>);
    constexpr ::std::size_t count{::std::is_same_v<decltype(Native), bridge::unary_abi> ? 1uz :
        ::std::is_same_v<decltype(Native), bridge::binary_abi> ? 2uz :
        ::std::is_same_v<decltype(Native), bridge::ternary_abi> ? 3uz : 4uz};
    if(arguments.size() != count) { return nullptr; }
    auto const word{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    ::llvm::Type* parameters[4u]{word, word, word, word};
    for(auto const argument: arguments) { if(argument == nullptr || argument->getType() != word) { return nullptr; } }
    auto const type{::llvm::FunctionType::get(word, {parameters, count}, false)};
    auto const function{get_owned_pending_numeric_native_declaration<Native>(builder, type, semantic)};
    if(function == nullptr) { return nullptr; }
    auto const call{apply_llvm_jit_host_calling_conv(builder.CreateCall(type, function, arguments))};
    call->setDoesNotThrow(); // Native's complete function-pointer type is noexcept.
    return call;
}

inline void emit_pending_numeric_status_check(runtime_local_func_llvm_jit_emit_state_t& state,
    ::llvm::Value* result) noexcept
{
    // Bridge success is exactly status::ok == 0. A numeric guest cannot supply
    // a context pointer/tag layout; nonzero is a native compiler/admission fault.
    auto& builder{*state.ir_builder};
    emit_llvm_conditional_trap(*state.llvm_module, builder,
        builder.CreateICmpNE(result, ::llvm::ConstantInt::get(result->getType(), 0u)));
}

[[nodiscard]] inline bool emit_pending_numeric_catch_tuple(runtime_local_func_llvm_jit_emit_state_t& state,
    llvm_jit_exception_handler_t const& handler,
    ::uwvm2::utils::container::vector<llvm_jit_stack_value_t>& values) noexcept
{
    namespace native = ::uwvm2::runtime::exception::pending_experiment::numeric_guest_bridge;
    if(handler.with_reference || state.pending_numeric_context == nullptr) { return false; }
    ::std::size_t bytes{};
    if(!llvm_jit_exception_numeric_tuple_bytes(handler.params, bytes) ||
       get_runtime_block_result_count(handler.params) >
           ::uwvm2::runtime::exception::pending_experiment::max_payload_fields ||
       (handler.catch_all && bytes != 0uz)) { return false; }
    auto& builder{*state.ir_builder};
    auto const word{state.pending_numeric_context->getType()};
    ::llvm::AllocaInst* buffer{};
    ::llvm::Value* address{::llvm::ConstantInt::get(word, 0u)};
    if(bytes != 0uz)
    {
        // [real caller entry byte array: bytes][one-past]
        // [safe                                       ] typed loads below fit
        // the complete native array; there is no guest memory pointer.
        buffer = create_llvm_jit_entry_block_alloca(builder, builder.getInt8Ty(),
            ::llvm::ConstantInt::get(word, bytes), "pending.catch.numeric.tuple");
        if(buffer == nullptr) { return false; }
        // [same owned array][its exact address] no dereference/range change.
        // [safe                             ] leaf copy borrows until return.
        address = builder.CreatePtrToInt(buffer, word);
    }
    if(!handler.catch_all)
    {
        auto const copied{emit_pending_numeric_leaf<native::uwvm2_pending_numeric_copy_r2>(builder,
            {native::copy_semantic}, {state.pending_numeric_context,
                ::llvm::ConstantInt::get(word, handler.tag_index), address, ::llvm::ConstantInt::get(word, bytes)})};
        if(copied == nullptr) { return false; }
        emit_pending_numeric_status_check(state, copied);
    }
    auto const count{get_runtime_block_result_count(handler.params)};
    values.reserve(count);
    ::std::size_t offset{};
    for(::std::size_t index{}; index != count; ++index)
    {
        auto const type{handler.params.begin[index]};
        auto const llvm_type{get_llvm_type_from_wasm_value_type(builder.getContext(), type)};
        auto const width{get_runtime_wasm_value_type_abi_size(type)};
        if(llvm_type == nullptr || width > bytes - offset) { return false; }
        // [initialized tuple prefix: offset][complete width-byte field][end]
        // [safe                                                           ]
        // copy succeeded BEFORE any load; typed numeric bits never evaluate FP.
        auto const slot{builder.CreateInBoundsGEP(builder.getInt8Ty(), buffer,
            ::llvm::ConstantInt::get(word, offset))};
        auto const carrier{get_llvm_jit_scalar_bits_type(llvm_type)};
        auto const loaded{builder.CreateLoad(carrier, slot)};
        loaded->setAlignment(::llvm::Align{1u});
        auto const value{carrier == llvm_type ? static_cast<::llvm::Value*>(loaded) : builder.CreateBitCast(loaded, llvm_type)};
        values.push_back({.type = type, .value = value});
        offset += width;
    }
    if(offset != bytes || !snapshot_runtime_local_func_llvm_jit_gc_roots(state, {values.data(), values.size()}))
    { return false; }
    // Typed SSA/root ownership is established BEFORE pending retirement.
    auto const cleared{emit_pending_numeric_leaf<native::uwvm2_pending_numeric_clear_r2>(builder,
        {native::clear_semantic}, {state.pending_numeric_context})};
    if(cleared == nullptr) { return false; }
    emit_pending_numeric_status_check(state, cleared);
    return true;
}

#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && (UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1)
#include "single_func_pending_numeric_fused_catch.h"
#endif
// Emit only the pending edge. It searches actual validator-created handlers in
// lexical order, without calling the C++ unwinder or a thread handler stack.
[[nodiscard]] inline bool emit_pending_numeric_exception_edge(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    namespace native = ::uwvm2::runtime::exception::pending_experiment::numeric_guest_bridge;
    namespace pending = ::uwvm2::runtime::exception::pending_experiment;
    auto& builder{*state.ir_builder};
    auto const word{state.pending_numeric_context->getType()};
    auto const function{builder.GetInsertBlock()->getParent()};
    for(auto depth{state.control_stack.size()}; depth != 0uz; --depth)
    {
        for(auto const& handler: state.control_stack.index_unchecked(depth - 1uz).exception_handlers)
        {
            if(handler.with_reference) { return false; }
#if defined(UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH) && (UWVM_EXPERIMENTAL_PENDING_NUMERIC_FUSED_CATCH == 1)
            if(!handler.catch_all)
            {
                if(!emit_pending_numeric_fused_catch_clause(state, handler)) { return false; }
                continue;
            }
#endif
            ::llvm::BasicBlock* next{};
            if(!handler.catch_all)
            {
                auto const match{emit_pending_numeric_leaf<native::uwvm2_pending_numeric_matches_r2>(builder,
                    {native::matches_semantic}, {state.pending_numeric_context, ::llvm::ConstantInt::get(word, handler.tag_index)})};
                if(match == nullptr) { return false; }
                auto const ok{builder.CreateICmpEQ(match, ::llvm::ConstantInt::get(word, 0u))};
                auto const no_match{builder.CreateICmpEQ(match,
                    ::llvm::ConstantInt::get(word, static_cast<unsigned>(pending::status::no_match)))};
                emit_llvm_conditional_trap(*state.llvm_module, builder, builder.CreateNot(builder.CreateOr(ok, no_match)));
                auto const selected{::llvm::BasicBlock::Create(builder.getContext(), "pending.eh.selected", function)};
                // [same-function owned next clause block] handle lifetime is
                // [safe                                ] the complete LLVM function.
                next = ::llvm::BasicBlock::Create(builder.getContext(), "pending.eh.next", function);
                builder.CreateCondBr(ok, selected, next);
                // [selected match edge][owned block] no guest address changes.
                // [safe                           ] only a real tag match enters.
                builder.SetInsertPoint(selected);
            }
            ::uwvm2::utils::container::vector<llvm_jit_stack_value_t> values{};
            if(!emit_pending_numeric_catch_tuple(state, handler, values)) { return false; }
            llvm_jit_branch_target_t const target{.params = handler.params, .block = handler.block,
                .phis = handler.phis, .control_stack_index = handler.control_stack_index};
            if(!try_add_runtime_local_func_llvm_jit_branch_target_incoming(state, target, values, builder.GetInsertBlock()))
            { return false; }
            builder.CreateBr(target.block);
            if(handler.catch_all) { return true; }
            // [same-function unmatched block] resume only cold lexical search.
            // [safe                        ] the normal operand model is retained.
            builder.SetInsertPoint(next);
        }
    }
    auto const appended{emit_pending_numeric_leaf<native::uwvm2_pending_numeric_append_exit_r2>(builder,
        {native::append_exit_semantic}, {state.pending_numeric_context,
            ::llvm::ConstantInt::get(word, state.local_func_storage_ptr->module_id),
            ::llvm::ConstantInt::get(word, state.local_func_storage_ptr->function_index)})};
    if(appended == nullptr) { return false; }
    emit_pending_numeric_status_check(state, appended);
    if(!leave_runtime_llvm_jit_gc_root_frame(builder, state.gc_root_frame)) { return false; }
    if(state.emit_call_stack_frames && !emit_runtime_local_func_llvm_jit_call_stack_pop(builder)) { return false; }
    // Internal protocol return. Callers MUST check pending BEFORE consuming a
    // scalar or reading multi-results. A multi-result output buffer is untouched.
    auto const result{state.llvm_function->getReturnType()};
    if(result->isVoidTy()) { builder.CreateRetVoid(); }
    else { builder.CreateRet(::llvm::Constant::getNullValue(result)); }
    return true;
}

[[nodiscard]] inline bool emit_pending_numeric_route(runtime_local_func_llvm_jit_emit_state_t& state,
    validation_module_traits_t::wasm_u32 target_index) noexcept
{
    namespace native = ::uwvm2::runtime::exception::pending_experiment::numeric_guest_bridge;
    if(state.pending_numeric_context == nullptr || state.ir_builder == nullptr || state.control_stack.empty()) { return false; }
    auto& builder{*state.ir_builder};
    // A plain native-word member load is authorized ONLY by this actual
    // sealed numeric core and the same explicit hidden FIRST argument. The
    // constructor has proved empty activation; r2 numeric leaves alone publish
    // 0/1, and public/native reentry creates an independent owner/header.
    // A raw pointer/shape or an arbitrary generated function is not admission.
    if(state.pending_numeric_plan == nullptr || !state.pending_numeric_plan->shape_valid_for_compilation() ||
       state.llvm_function == nullptr || state.llvm_public_entry_function == nullptr ||
       state.llvm_function == state.llvm_public_entry_function ||
       state.llvm_function->arg_empty() || state.llvm_function->getArg(0u) != state.pending_numeric_context ||
       builder.GetInsertBlock()->getParent() != state.llvm_function ||
       state.llvm_function->getMetadata("uwvm2.pending.core.index") == nullptr)
    { return false; }
    static_assert(::std::is_standard_layout_v<native::numeric_header>);
    static_assert(native::numeric_header::phase_offset() == 0uz);
    auto const word{state.pending_numeric_context->getType()};
    if(!word->isIntegerTy(sizeof(native::word) * CHAR_BIT)) { return false; }
    // [actual live numeric_header][first standard-layout word phase]
    // [safe                                                   ] Header lifetime
    // covers this complete core activation; no slow-context offsetof/type pun.
    auto const header_pointer{builder.CreateIntToPtr(state.pending_numeric_context,
        builder.getPtrTy(), "pending.header.ptr")};
    auto const present{builder.CreateLoad(word, header_pointer, "pending.header.phase")};
    present->setAlignment(::llvm::Align{alignof(native::word)});

    auto const identity{::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::get(
        ::llvm::Type::getInt64Ty(builder.getContext()), static_cast<::std::uint_least64_t>(target_index)))};
    present->setMetadata("uwvm2.pending.presence.target", ::llvm::MDNode::get(builder.getContext(), {identity}));
    // Successful live activation publishes 0/1, but retain the R1 fail-closed
    // guard: ANY phase >=2 traps before exceptional/normal result consumption.
    // Cold numeric leaf admission still checks the real slow context. The
    // proved NoGuestEscape fold can erase this load AND its constant-false
    // guard; removing the unfurled guard needs a separately qualified change.
    emit_llvm_conditional_trap(*state.llvm_module, builder,
        builder.CreateICmpUGT(present, ::llvm::ConstantInt::get(present->getType(), 1u)));
    auto const function{builder.GetInsertBlock()->getParent()};
    auto const normal{::llvm::BasicBlock::Create(builder.getContext(), "pending.call.empty", function)};
    auto const exceptional{::llvm::BasicBlock::Create(builder.getContext(), "pending.call.propagate", function)};
    builder.CreateCondBr(builder.CreateICmpEQ(present, ::llvm::ConstantInt::get(present->getType(), 1u)), exceptional, normal);
    // [pending branch in same function] only this edge may copy/clear payload.
    // [safe                          ] no normal multi-result load has happened.
    builder.SetInsertPoint(exceptional);
    if(!emit_pending_numeric_exception_edge(state)) { return false; }
    // [empty-pending edge in same function] this is the ONLY normal successor.
    // [safe                              ] result consumers are emitted here.
    builder.SetInsertPoint(normal);
    return true;
}

[[nodiscard]] inline bool emit_pending_numeric_public_wrapper(runtime_local_func_llvm_jit_emit_state_t& state) noexcept
{
    namespace entry = ::uwvm2::runtime::exception::pending_experiment::numeric_entry;
    if(state.pending_numeric_plan == nullptr || state.llvm_public_entry_function == nullptr ||
       !state.llvm_public_entry_function->empty() || !state.native_guest_exceptions ||
       state.llvm_function == nullptr || state.llvm_function == state.llvm_public_entry_function)
    { return false; }
    auto& context{*state.llvm_context_holder};
    auto const function{state.llvm_public_entry_function};
    auto const start{::llvm::BasicBlock::Create(context, "pending.owner.entry", function)};
    ::llvm::IRBuilder<> builder{start};
    auto const word{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    auto const owner_type{::llvm::ArrayType::get(builder.getInt8Ty(), sizeof(entry::entry_owner))};
    // [public caller-owned aligned byte ARRAY: sizeof(entry_owner)][end]
    // [safe                                                          ] native
    // placement constructor starts a real owner lifetime in this full extent.
    auto const owner{builder.CreateAlloca(owner_type, nullptr, "pending.native.owner")};
    owner->setAlignment(::llvm::Align{alignof(entry::entry_owner)});
    auto const owner_address{builder.CreatePtrToInt(owner, word)};
    auto const name{::uwvm2::utils::container::u8concat_uwvm(
        get_llvm_runtime_module_symbol_prefix(*state.local_func_storage_ptr->runtime_module_ptr), u8"_pending_numeric_plan_r2")};
    auto const plan{get_owned_pending_numeric_plan_address(builder,
        ::uwvm2::utils::container::u8string_view{name.data(), name.size()})};
    if(plan == nullptr) { return false; }
    auto const construct_type{::llvm::FunctionType::get(word, {word, word}, false)};
    auto const construct{get_owned_pending_numeric_native_declaration<entry::uwvm2_pending_numeric_owner_construct_r2>(
        builder, construct_type, {entry::construct_semantic})};
    auto const destroy_type{::llvm::FunctionType::get(builder.getVoidTy(), {word}, false)};
    auto const destroy{get_owned_pending_numeric_native_declaration<entry::uwvm2_pending_numeric_owner_destroy_r2>(
        builder, destroy_type, {entry::destroy_semantic})};
    auto const finish{get_owned_pending_numeric_native_declaration<entry::uwvm2_pending_numeric_owner_finish_r2>(
        builder, destroy_type, {entry::finish_semantic})};
    if(construct == nullptr || destroy == nullptr || finish == nullptr) { return false; }
    auto const native_context{apply_llvm_jit_host_calling_conv(builder.CreateCall(construct_type, construct, {owner_address, plan}))};
    native_context->setDoesNotThrow();
    // Construction failure has ALREADY destroyed its native object. The zero
    // edge traps before creating an invoke and NEVER destroys/finishes it again.
    emit_llvm_conditional_trap(*state.llvm_module, builder,
        builder.CreateICmpEQ(native_context, ::llvm::ConstantInt::get(word, 0u)));
    auto const origin{builder.saveIP()};
    auto const cleanup{::llvm::BasicBlock::Create(context, "pending.owner.cleanup", function)};
    function->setPersonalityFn(state.native_exception_imports.personality);
    // [same LLVM wrapper][foreign/native exceptional cleanup block]
    // [safe                                                    ] both invokes
    // below exist only after a successful genuine native construction.
    builder.SetInsertPoint(cleanup);
    auto const record{builder.CreateLandingPad(::llvm::StructType::get(context,
        {builder.getPtrTy(), builder.getInt32Ty()}), 0u)};
    record->setCleanup(true);
    auto const destroyed_cold{apply_llvm_jit_host_calling_conv(builder.CreateCall(destroy_type, destroy, {owner_address}))};
    destroyed_cold->setDoesNotThrow();
    builder.CreateResume(record);
    // [saved success continuation][same live wrapper] restore after cold IR.
    // [safe                                                       ] no owner
    // or native lifetime changes while compiling the unreachable cleanup edge.
    builder.restoreIP(origin);
    ::uwvm2::utils::container::vector<::llvm::Value*> arguments{};
    arguments.reserve(function->arg_size() + 1uz);
    arguments.push_back(native_context);
    for(auto& argument: function->args()) { arguments.push_back(::std::addressof(argument)); }
    auto const returned{::llvm::BasicBlock::Create(context, "pending.owner.core.return", function)};
    auto const core{apply_llvm_jit_wasm_calling_conv(builder.CreateInvoke(state.llvm_function->getFunctionType(),
        state.llvm_function, returned, cleanup, {arguments.data(), arguments.size()}))};
    // [core-return block] owner/context are still live on this edge.
    // [safe             ] unchanged public result is not returned before finish.
    builder.SetInsertPoint(returned);
    auto const done{::llvm::BasicBlock::Create(context, "pending.owner.done", function)};
    apply_llvm_jit_host_calling_conv(builder.CreateInvoke(destroy_type, finish, done, cleanup, {owner_address}));
    // [normal completed edge] finish cleared/materialized before this point.
    // [safe                 ] retire the unique native owner exactly once.
    builder.SetInsertPoint(done);
    auto const destroyed{apply_llvm_jit_host_calling_conv(builder.CreateCall(destroy_type, destroy, {owner_address}))};
    destroyed->setDoesNotThrow();
    if(function->getReturnType()->isVoidTy()) { builder.CreateRetVoid(); }
    else { builder.CreateRet(core); }
    return verify_llvm_jit_function(*function, state.verify_llvm_jit_ir);
}

[[nodiscard]] inline bool try_emit_pending_numeric_throw_tuple(runtime_local_func_llvm_jit_emit_state_t& state,
    ::std::size_t tag_index, runtime_block_result_type params) noexcept
{
    namespace native = ::uwvm2::runtime::exception::pending_experiment::numeric_guest_bridge;
    auto const count{get_runtime_block_result_count(params)};
    ::std::size_t bytes{};
    if(state.pending_numeric_context == nullptr || state.operand_stack.size() < count ||
       count > ::uwvm2::runtime::exception::pending_experiment::max_payload_fields ||
       !llvm_jit_exception_numeric_tuple_bytes(params, bytes)) { return false; }
    if(!snapshot_runtime_local_func_llvm_jit_gc_roots(state)) { return false; }
    auto& builder{*state.ir_builder};
    auto const word{state.pending_numeric_context->getType()};
    ::llvm::AllocaInst* tuple{};
    ::llvm::Value* address{::llvm::ConstantInt::get(word, 0u)};
    if(bytes != 0uz)
    {
        // [caller-owned complete tuple byte ARRAY: bytes][end]
        // [safe                                             ] native leaf borrows
        // it synchronously. No retired alloca is stored in pending_context.
        tuple = create_llvm_jit_entry_block_alloca(builder, builder.getInt8Ty(),
            ::llvm::ConstantInt::get(word, bytes), "pending.throw.numeric.tuple");
        if(tuple == nullptr) { return false; }
        // [same complete tuple][integer ABI representation] no range changes.
        // [safe                                           ] no guest pointer.
        address = builder.CreatePtrToInt(tuple, word);
    }
    ::std::size_t offset{};
    auto const first{state.operand_stack.size() - count};
    for(::std::size_t index{}; index != count; ++index)
    {
        auto const operand{state.operand_stack.index_unchecked(first + index)};
        auto const llvm_type{get_llvm_type_from_wasm_value_type(builder.getContext(), params.begin[index])};
        if(operand.type != params.begin[index] || operand.value == nullptr || llvm_type == nullptr ||
           operand.value->getType() != llvm_type) { return false; }
        auto const width{get_runtime_wasm_value_type_abi_size(operand.type)};
        if(width > bytes - offset) { return false; }
        // [owned tuple prefix][initialized width-byte destination][end]
        // [safe                                                      ]
        // offset+width <= bytes; start every typed field before native publish.
        auto const slot{builder.CreateInBoundsGEP(builder.getInt8Ty(), tuple,
            ::llvm::ConstantInt::get(word, offset))};
        auto const carrier{get_llvm_jit_scalar_bits_type(llvm_type)};
        auto const value{carrier == llvm_type ? operand.value : builder.CreateBitCast(operand.value, carrier)};
        auto const stored{builder.CreateStore(value, slot)};
        stored->setAlignment(::llvm::Align{1u});
        offset += width;
    }
    if(offset != bytes) { return false; }
    auto const published{emit_pending_numeric_leaf<native::uwvm2_pending_numeric_publish_r2>(builder,
        {native::publish_semantic}, {state.pending_numeric_context, ::llvm::ConstantInt::get(word, tag_index),
            address, ::llvm::ConstantInt::get(word, bytes)})};
    if(published == nullptr) { return false; }
    // Conservative local guest effect: a genuine validated throw can escape
    // unless a real catch_all consumes it. Typed catches need a separate tag
    // set proof before being treated as a function-wide no-escape guarantee.
    if(pending_numeric_call_can_escape_guest(state))
    {
        auto const escaping{::llvm::ConstantAsMetadata::get(::llvm::ConstantInt::getTrue(builder.getContext()))};
        state.llvm_function->setMetadata("uwvm2.pending.local.guest", ::llvm::MDNode::get(builder.getContext(), {escaping}));
    }
    emit_pending_numeric_status_check(state, published);
    if(!emit_pending_numeric_exception_edge(state)) { return false; }
    enter_runtime_local_func_llvm_jit_unreachable_control_context(state);
    return true;
}

#pragma once
// Selected immutable full-JIT checkpoint profile only. Generates a separate
// logical continuation entry from the one validated/translated body; cloning
// LLVM IR does not decode or validate guest bytes a second time.
[[nodiscard]] inline bool prepare_runtime_local_func_llvm_jit_checkpoint_resume_entry(
    runtime_local_func_llvm_jit_emit_state_t& state)
{
    if(state.checkpoint_plan == nullptr) { return true; }
    namespace checkpoint = ::uwvm2::runtime::checkpoint;
    auto const plan{state.checkpoint_plan};
    if(plan != nullptr && plan->profile &&
       plan->profile->purpose() == checkpoint::compilation_purpose::observe_values) { return true; }
    if(plan->producer_availability != checkpoint::status::ok) { return true; }
    if(!checkpoint_resume_native_abi_layout_matches(state.llvm_module) ||
       state.llvm_function == nullptr || state.ir_builder == nullptr ||
       state.llvm_function != state.llvm_public_entry_function || state.emit_tiered_loop_reentry_entries ||
       state.pending_numeric_plan != nullptr || !state.debug_activation_enabled ||
       state.debug_activation_token == nullptr || !plan->resume_sites.empty() || plan->resume_abi_revision != 0u) { return false; }
    auto& builder{*state.ir_builder};
    auto const integer{builder.getIntNTy(static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    ::std::array<::llvm::Value*, 5u> zero{
        builder.getInt64(0u), ::llvm::ConstantPointerNull::get(builder.getPtrTy()),
        ::llvm::ConstantInt::get(integer, 0u), ::llvm::ConstantPointerNull::get(builder.getPtrTy()),
        ::llvm::ConstantInt::get(integer, 0u)};
    ::std::array<::llvm::FreezeInst*, 5u> placeholders{};
    for(::std::size_t i{}; i != zero.size(); ++i)
    {
        // [compiler-owned five zero values0 ... i ... 5] end
        // [safe] i<5 before selecting either handle. Explicit instructions,
        // rather than shared constants, identify just these clone inputs.
        placeholders[i] = builder.Insert(new ::llvm::FreezeInst(zero[i]), "checkpoint.normal.zero");
    }
    auto const reject{::llvm::BasicBlock::Create(builder.getContext(), "checkpoint.resume.reject", state.llvm_function)};
    ::llvm::IRBuilder<> rejected{reject};
    // The private dispatcher prevalidates owner/site/typed packet. A contract
    // violation traps rather than interpreting bytes as a PC or native stack.
    auto const trap{::llvm::Intrinsic::getOrInsertDeclaration(state.llvm_module, ::llvm::Intrinsic::trap)};
    rejected.CreateCall(trap); rejected.CreateUnreachable();
    if(!prepare_runtime_local_func_llvm_jit_checkpoint_resume_dispatch(state,
        state.checkpoint_executed_local_flags, placeholders[0u], placeholders[1u], placeholders[2u],
        placeholders[3u], placeholders[4u], reject, state.checkpoint_resume)) { return false; }
    state.checkpoint_resume.placeholders = placeholders;
    return true;
}

[[nodiscard]] inline bool emit_runtime_local_func_llvm_jit_checkpoint_current_landing(
    runtime_local_func_llvm_jit_emit_state_t& state)
{
    namespace checkpoint = ::uwvm2::runtime::checkpoint;
    auto const plan{state.checkpoint_plan};
    if(plan != nullptr && plan->profile &&
       plan->profile->purpose() == checkpoint::compilation_purpose::observe_values) { return true; }
    if(plan == nullptr || plan->producer_availability != checkpoint::status::ok || state.checkpoint_current_site == 0u)
    { return true; }
    if(state.checkpoint_resume.dispatch == nullptr || state.checkpoint_current_site > plan->sites.size()) { return false; }
    if(!state.checkpoint_resume.installed_sites.empty())
    {
        auto const previous{state.checkpoint_resume.installed_sites.back()};
        if(previous == state.checkpoint_current_site) { return true; }
        if(previous > state.checkpoint_current_site) { return false; }
    }
    // [actual fused plan sites0 ... ordinal-1 ... N] end
    // [safe] nonzero ordinal<=N above BEFORE the metadata borrow.
    auto const& site{plan->sites[static_cast<::std::size_t>(state.checkpoint_current_site - 1u)]};
    if(site.opcode_offset != state.current_wasm_op_offset || site.operand_count != state.operand_stack.size() ||
       site.local_count > site.slots.size() || site.operand_count > site.slots.size()-site.local_count ||
       site.saved_parameter_count != site.slots.size()-site.local_count-site.operand_count) { return false; }
    ::std::vector<::llvm::Value*> actual{}; actual.reserve(site.operand_count+site.saved_parameter_count);
    ::std::vector<checkpoint::types::core_value_type> exact{}; exact.reserve(site.operand_count+site.saved_parameter_count);
    for(::std::size_t i{}; i != site.operand_count; ++i)
    {
        // [same-walk operand handles0 ... i ... N] [local-prefix+N slots] end
        // [safe] complete counts/subtraction above BEFORE either selection.
        actual.push_back(state.operand_stack.index_unchecked(i).value);
        exact.push_back(site.slots[site.local_count + i].type);
    }
    for(auto const& control : state.control_stack)
    {
        if(control.type != llvm_jit_control_context_type::if_then && control.type != llvm_jit_control_context_type::if_else) { continue; }
        for(::std::size_t i{};i != control.entry_params.size();++i)
        {
            if(actual.size() >= site.slots.size()-site.local_count) { return false; }
            auto const value{read_runtime_local_func_llvm_jit_checkpoint_saved_if(state,*state.ir_builder,control,i)};
            if(value == nullptr) { return false; }
            exact.push_back(site.slots[site.local_count+actual.size()].type);actual.push_back(value);
        }
    }
    if(actual.size() != site.operand_count+site.saved_parameter_count) { return false; }
    auto const landing{emit_runtime_local_func_llvm_jit_checkpoint_resume_landing(state,
        state.checkpoint_resume, site, actual, exact)};
    if(!landing.valid || !landing.selected || landing.actual_nonlocals.size() != site.operand_count+site.saved_parameter_count) { return false; }
    for(::std::size_t i{}; i != site.operand_count; ++i)
    {
        // [same actual operand deque and replacement PHIs0..N] end
        // [safe] equal counts proved before preserving semantic type while
        // replacing ONLY the live physical value with its real merge PHI.
        auto& value{state.operand_stack.index_unchecked(i)};value.value = landing.actual_nonlocals[i];
        invalidate_runtime_local_func_llvm_jit_checkpoint_value_witness(state,value);
    }
    return true;
}

[[nodiscard]] inline bool finalize_runtime_local_func_llvm_jit_checkpoint_resume_entry(
    runtime_local_func_llvm_jit_emit_state_t& state)
{
    namespace checkpoint = ::uwvm2::runtime::checkpoint;
    auto const plan{state.checkpoint_plan};
    if(plan != nullptr && plan->profile &&
       plan->profile->purpose() == checkpoint::compilation_purpose::observe_values) { return true; }
    if(plan == nullptr || plan->producer_availability != checkpoint::status::ok) { return true; }
    auto const original{state.llvm_function}; auto const module{state.llvm_module};
    auto& resume{state.checkpoint_resume};
    if(original == nullptr || !checkpoint_resume_native_abi_layout_matches(module) || original != state.llvm_public_entry_function ||
       resume.actual_state != ::std::addressof(state) || resume.actual_plan != plan || resume.dispatch == nullptr ||
       checkpoint::validate_plan(*plan) != checkpoint::status::ok) { return false; }
    if(resume.installed_sites.empty()) { return true; } // A legal empty/unreachable body is not rejected by instrumentation.
    auto& context{original->getContext()};
    auto const integer{::llvm::Type::getIntNTy(context, static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT))};
    ::std::vector<::llvm::Type*> parameters{};
    parameters.reserve(original->arg_size());
    for(auto& argument : original->args()) { parameters.push_back(argument.getType()); }
    for(auto const placeholder : resume.placeholders)
    {
        if(placeholder == nullptr || placeholder->getFunction() != original) { return false; }
    }
    auto const original_name{original->getName()};
    // [LLVM-owned generated ASCII symbol name] name_end
    // [safe] StringRef owns the exact byte count; format that bounded view,
    // never scan beyond its end or assume a trailing zero.
    auto const name{::uwvm2::utils::container::u8concat_uwvm(
        ::fast_io::mnp::code_cvt(::fast_io::basic_io_scatter_t<char>{original_name.data(), original_name.size()}),
        u8".checkpoint.resume.v2")};
    auto const raw_name{::uwvm2::utils::container::u8concat_uwvm(name, u8".raw")};
    if(module->getFunction(get_llvm_string_ref(name)) != nullptr || module->getFunction(get_llvm_string_ref(raw_name)) != nullptr)
    { return false; }
    // Exact original ABI, including every parameter and varargs property.
    // Appending private inputs invalidates non-tailcc musttail prototypes.
    // The separate raw entry authenticates a manager-owned TLS binding instead.
    auto const type{original->getFunctionType()};
    auto const clone{::llvm::Function::Create(type, ::llvm::GlobalValue::InternalLinkage,
        get_llvm_string_ref(name), module)};
    ::llvm::ValueToValueMapTy mapping{};
    for(auto& argument : original->args())
    {
        // [old args0..P] [same exact clone args0..P] end
        // [safe] identical FunctionType and full count before selecting the
        // unchanged original index; no private parameter changes the ABI.
        mapping[::std::addressof(argument)] = clone->getArg(argument.getArgNo());
    }
    ::llvm::SmallVector<::llvm::ReturnInst*, 8u> returns{};
    ::llvm::CloneFunctionInto(clone, original, mapping, ::llvm::CloneFunctionChangeType::LocalChangesOnly, returns);
    clone->setCallingConv(original->getCallingConv());
    clone->setLinkage(::llvm::GlobalValue::InternalLinkage);
    clone->addFnAttr("uwvm.checkpoint.logical-resume-v2");
    // Preserve the physical continuation boundary even when an original
    // component/provider does not carry the common NoInline policy. LLVM's
    // inliner can weaken an inlined musttail under a non-tail raw call.
    clone->addFnAttr(::llvm::Attribute::NoInline);
    // This private multi-entry continuation retains one typed landing per
    // recorded opcode. InstCombine repeatedly scans the shared packet/local
    // allocas across thousands of such entries. Keep its verified compiler IR
    // intact; the public normal Wasm body still uses the configured pipeline.
    // NoInline above satisfies OptimizeNone's LLVM contract. Verification and
    // native code generation still run for this real executable continuation.
    clone->addFnAttr(::llvm::Attribute::OptimizeNone);
    // CloneFunctionInto copies original personality, return and parameter ABI
    // attributes. Leave original unmapped: a subsequent ordinary recursive
    // call must enter its public normal body, never replay this saved selector.
    auto const activation{mapping.find(state.debug_activation_token)};
    if(activation == mapping.end() || !checkpoint_resume_value_owned_by_function(activation->second, clone) ||
       activation->second->getType() != ::llvm::Type::getInt64Ty(context))
    { clone->eraseFromParent(); return false; }
    for(::std::size_t i{}; i != resume.placeholders.size(); ++i)
    {
        // [same-module actual five instruction mappings0..5] end
        // [safe] i<5 before either mapping/handle selection. A private bridge
        // independently authenticates the NEW real incarnation and active owner.
        auto const found{mapping.find(resume.placeholders[i])};
        if(found == mapping.end()) { clone->eraseFromParent(); return false; }
        auto const copied{::llvm::dyn_cast_or_null<::llvm::FreezeInst>(found->second)};
        if(copied == nullptr || copied->getFunction() != clone) { clone->eraseFromParent(); return false; }
        ::llvm::IRBuilder<> inputs{copied};
        auto const input_type{::llvm::FunctionType::get(inputs.getInt64Ty(), {inputs.getInt64Ty(), integer}, false)};
        auto const bridge{get_llvm_runtime_bridge_function_symbol_value_unwrapped<
            ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_resume_input_abi_bridge>(inputs,input_type)};
        if(bridge == nullptr) { clone->eraseFromParent(); return false; }
        auto const actual{apply_llvm_jit_host_calling_conv(inputs.CreateCall(input_type,bridge,
            {activation->second,::llvm::ConstantInt::get(integer,i)}))};
        actual->setDoesNotThrow(); // side-effecting owner check; never readonly/speculatable
        ::llvm::Value* replacement{actual};
        if(copied->getType()->isPointerTy()) { replacement=inputs.CreateIntToPtr(inputs.CreateZExtOrTrunc(actual,integer),copied->getType()); }
        else if(copied->getType() != actual->getType()) { replacement=inputs.CreateZExtOrTrunc(actual,copied->getType()); }
        copied->replaceAllUsesWith(replacement); copied->eraseFromParent();
    }
    // Every existing musttail remains unchanged, immediately followed by its
    // original return. The exact clone type/CC/ABI attributes satisfy LangRef
    // on ordinary C/Fast as well as tailcc; verification below proves the IR.
    auto const local{state.local_func_storage_ptr};
    if(local == nullptr || local->function_type_ptr == nullptr) { clone->eraseFromParent(); return false; }
    auto const abi{get_runtime_wasm_call_abi_layout(*local->function_type_ptr)};
    if(!abi.valid || original->arg_size() != abi.parameter_count + (abi.result_count > 1u ? 1u : 0u))
    { clone->eraseFromParent(); return false; }
    // Private native ABI only: void(site, payload, payload_bytes, flags,
    // flag_count, result_buffer_address, result_bytes). Native addresses are
    // provided by the manager's owned reconstructed packet, NEVER wire data.
    auto const raw_type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context),
        {::llvm::Type::getInt64Ty(context), ::llvm::PointerType::getUnqual(context), integer,
         ::llvm::PointerType::getUnqual(context), integer, integer, integer}, false)};
    auto const raw{::llvm::Function::Create(raw_type, ::llvm::GlobalValue::ExternalLinkage,
        get_llvm_string_ref(raw_name), module)};
    apply_llvm_jit_host_calling_conv(*raw);
    if(state.emit_unwind_call_stack_frames) { apply_llvm_jit_unwind_call_stack_function_attrs(*raw); }
    else if(state.native_guest_exceptions) { raw->setUWTableKind(::llvm::UWTableKind::Sync); }
    auto const entry{::llvm::BasicBlock::Create(context, "entry", raw)}; ::llvm::IRBuilder<> builder{entry};
    emit_llvm_conditional_trap(*module, builder, builder.CreateICmpEQ(raw->getArg(0u), builder.getInt64(0u)));
    emit_llvm_conditional_trap(*module, builder, builder.CreateICmpNE(raw->getArg(6u),
        ::llvm::ConstantInt::get(integer, abi.result_bytes)));
    if(abi.result_bytes != 0u)
    { emit_llvm_conditional_trap(*module, builder, builder.CreateICmpEQ(raw->getArg(5u), ::llvm::ConstantInt::get(integer, 0u))); }
    auto const bind_type{::llvm::FunctionType::get(builder.getVoidTy(),
        {integer,integer,builder.getInt64Ty(),builder.getInt64Ty(),builder.getPtrTy(),integer,
         builder.getPtrTy(),integer,integer,integer},false)};
    auto const bind_bridge{get_llvm_runtime_bridge_function_symbol_value_unwrapped<
        ::uwvm2::runtime::lib::details::llvm_jit_checkpoint_bind_resume_entry_abi_bridge>(builder,bind_type)};
    if(bind_bridge == nullptr) { raw->eraseFromParent(); clone->eraseFromParent(); return false; }
    auto const bound{apply_llvm_jit_host_calling_conv(builder.CreateCall(bind_type,bind_bridge,
        {::llvm::ConstantInt::get(integer,plan->module),::llvm::ConstantInt::get(integer,plan->function),
         builder.getInt64(plan->function_generation),raw->getArg(0u),raw->getArg(1u),raw->getArg(2u),
         raw->getArg(3u),raw->getArg(4u),raw->getArg(5u),raw->getArg(6u)}))};
    bound->setDoesNotThrow(); // no host callback, allocation, poll, park or exception
    if(abi.parameter_count > plan->sites.front().local_count ||
       abi.parameter_count > static_cast<::std::size_t>(PTRDIFF_MAX)/checkpoint::native_slot_bytes)
    { raw->eraseFromParent(); clone->eraseFromParent(); return false; }
    if(abi.parameter_count != 0u)
    {
        emit_llvm_conditional_trap(*module,builder,builder.CreateICmpULT(raw->getArg(2u),
            ::llvm::ConstantInt::get(integer,abi.parameter_count*checkpoint::native_slot_bytes)));
        emit_llvm_conditional_trap(*module,builder,builder.CreateICmpULT(raw->getArg(4u),
            ::llvm::ConstantInt::get(integer,abi.parameter_count)));
        emit_llvm_conditional_trap(*module,builder,builder.CreateICmpEQ(raw->getArg(1u),
            ::llvm::ConstantPointerNull::get(builder.getPtrTy())));
        emit_llvm_conditional_trap(*module,builder,builder.CreateICmpEQ(raw->getArg(3u),
            ::llvm::ConstantPointerNull::get(builder.getPtrTy())));
    }
    // Bind already proved actual owned ranges. Check ALL original parameter
    // initialization markers before ANY parameter payload load, preserving
    // nondefaultable parameter and ABI noundef/nonnull promises if present.
    for(::std::size_t i{};i != abi.parameter_count;++i)
    {
        // [actual bounded original-index flags0 ... i ... P<=flag_count] end
        // [safe] full count/null and owner equality BEFORE GEP/marker load.
        auto const flag{builder.CreateInBoundsGEP(builder.getInt8Ty(),raw->getArg(3u),builder.getInt64(i))};
        auto const initialized{builder.CreateLoad(builder.getInt8Ty(),flag)};initialized->setAlignment(::llvm::Align{1u});
        emit_llvm_conditional_trap(*module,builder,builder.CreateICmpNE(initialized,builder.getInt8(1u)));
    }
    ::std::vector<::llvm::Value*> arguments{}; arguments.reserve(parameters.size());
    for(::std::size_t i{}; i != abi.parameter_count; ++i)
    {
        // [actual canonical native packet ... i*16 ... P*16<=bytes] end
        // [safe] original parameter count/product, exact physical ABI type and
        // actual range owner BEFORE GEP/typed load. The saved current parameter
        // supplies a legitimate entry value instead of an invented null value.
        auto const physical{get_llvm_type_from_wasm_value_type(context,local->function_type_ptr->parameter.begin[i])};
        if(physical == nullptr || physical != original->getArg(i)->getType() ||
           get_runtime_wasm_value_type_abi_size(local->function_type_ptr->parameter.begin[i]) > checkpoint::native_slot_bytes)
        { raw->eraseFromParent(); clone->eraseFromParent(); return false; }
        auto const slot{builder.CreateInBoundsGEP(builder.getInt8Ty(),raw->getArg(1u),
            builder.getInt64(i*checkpoint::native_slot_bytes))};
        auto const value{builder.CreateLoad(physical,slot)};value->setAlignment(::llvm::Align{1u});arguments.push_back(value);
    }
    if(abi.result_count > 1u) { arguments.push_back(raw->getArg(5u)); }
    auto const call{builder.CreateCall(clone,arguments)};
    call->setCallingConv(clone->getCallingConv());
    ::llvm::SmallVector<::llvm::AttributeSet,8u> argument_attributes{};
    for(unsigned i{};i != clone->arg_size();++i) { argument_attributes.push_back(clone->getAttributes().getParamAttrs(i)); }
    call->setAttributes(::llvm::AttributeList::get(context,::llvm::AttributeSet{},
        clone->getAttributes().getRetAttrs(),argument_attributes));
    if(abi.result_count == 1u && !emit_store_runtime_wasm_call_result_to_raw_buffer(builder,
        *local->function_type_ptr, call, raw->getArg(5u), "checkpoint.resume.result"))
    { raw->eraseFromParent(); clone->eraseFromParent(); return false; }
    builder.CreateRetVoid();
    ::uwvm2::runtime::compiler::llvm_jit::native_provenance::restrict_public_code(*clone);
    if(!verify_llvm_jit_function(*clone, true) || !verify_llvm_jit_function(*raw, true))
    { raw->eraseFromParent(); clone->eraseFromParent(); return false; }
    // This internal entry is also invoked through the runtime's exact typed
    // continuation ABI. NoInline/OptimizeNone do not prevent LLVM GlobalOpt
    // from changing C to fastcc, or IPO from removing unused ABI parameters.
    // Retain only this verified compiler-owned function in metadata; the list
    // creates no public address, native memory or execution permission.
    ::llvm::appendToCompilerUsed(*module, {clone});
    // Exact immutable compiler DATA. Runtime must still resolve this symbol in
    // its actual engine and bind the resulting entry to its private publication.
    plan->resume_sites = resume.installed_sites; plan->resume_abi_revision = 2u;
    return true;
}

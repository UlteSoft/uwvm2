// Original runtime anonymous namespace, after registry/target/source helpers.
inline ::uwvm2::utils::container::delete_owned_ptr<owned_native_eh_private_leaf_publication>
    owned_native_eh_private_leaf_publication::prepare_actual_record(compiled_module_record& rec) noexcept
{
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1 && \
    defined(UWVM_CPP_EXCEPTIONS) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    auto const& compiled{rec.llvm_jit_compiled};
    auto const& stage{compiled.staged_native_eh_private_leaf};
    auto const generation{current_runtime_generation()};
    auto const id{find_runtime_module_id_from_storage_ptr(rec.runtime_module)};
    auto const selected_source{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
    if(!stage || !stage->matches_actual_full_result(compiled) || !source_type::has_canonical_owner(selected_source) ||
       !selected_source->native_eh_private_leaf_requested() || generation == 0u || id != 0uz ||
       g_runtime.modules.size() != 1uz || ::std::addressof(g_runtime.modules.index_unchecked(id)) != ::std::addressof(rec) ||
       selected_source->initialized_main_module() != rec.runtime_module || selected_source->bound_initialized_main_module_id() != id ||
       rec.runtime_module == nullptr || !rec.runtime_module->imported_function_vec_storage.empty() ||
       !rec.runtime_module->imported_table_vec_storage.empty() || !rec.runtime_module->imported_memory_vec_storage.empty() ||
       !rec.runtime_module->imported_global_vec_storage.empty() || !rec.runtime_module->imported_tag_vec_storage.empty() ||
       rec.llvm_jit_ready || g_runtime.debug_pause_control || !rec.llvm_jit_debug_full_typed_entry_targets.empty() ||
       rec.llvm_jit_compiled.llvm_jit_task_modules_pre_link_optimized || !runtime_llvm_jit_unwind_call_stack_requested() ||
       ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_mode != ::uwvm2::uwvm::runtime::runtime_mode::runtime_mode_t::full_compile ||
       ::uwvm2::uwvm::runtime::runtime_mode::global_runtime_compiler != ::uwvm2::uwvm::runtime::runtime_mode::runtime_compiler_t::llvm_jit_only ||
       (rec.llvm_jit_full_publication && (rec.llvm_jit_full_publication->plan || rec.llvm_jit_full_publication->engine ||
                                       rec.llvm_jit_full_publication->context)) ||
       !compiled.llvm_jit_module.emitted || !compiled.llvm_jit_module.llvm_module || !compiled.llvm_jit_module.llvm_context_holder)
    { return {}; }
    auto const& observed_source{compiled.native_eh_leaf_observations.attempt->source()};
    if(observed_source.get() != selected_source.get() || observed_source.owner_before(selected_source) ||
       selected_source.owner_before(observed_source) ||
       (rec.llvm_jit_full_publication && (rec.llvm_jit_full_publication->source.get() != selected_source.get() ||
           rec.llvm_jit_full_publication->source.owner_before(selected_source) ||
           selected_source.owner_before(rec.llvm_jit_full_publication->source)))) { return {}; }
    try
    {
        ::llvm::SmallVector<char, 0u> bytes{};
        ::llvm::raw_svector_ostream stream{bytes};
        auto const ir{stage->ir_for_cold_inspection()};
        if(ir == nullptr || stage->clones() > 256uz || stage->clones() == 0uz) { return {}; }
        ::llvm::WriteBitcodeToFile(*ir, stream);
        compiler::llvm_jit_module_storage_t candidate{};
        candidate.llvm_context_holder = ::uwvm2::utils::container::make_delete_owned<::llvm::LLVMContext>();
        // [actual staged IR-owned serialized bytes][checked complete size] end
        // [safe] The new candidate context/module own all parsed nodes. Original
        // public IR/context remain untouched and retained for every failure.
        auto parsed{::llvm::parseBitcodeFile(::llvm::MemoryBufferRef{
            ::llvm::StringRef{bytes.data(), bytes.size()}, "uwvm-native-private-leaf-publication-r1"}, *candidate.llvm_context_holder)};
        if(!parsed) { ::llvm::consumeError(parsed.takeError()); return {}; }
        candidate.llvm_module.reset(parsed->release());
        if(::llvm::verifyModule(*candidate.llvm_module)) { return {}; }
        candidate.emitted = true;
        auto owner{::uwvm2::utils::container::delete_owned_ptr<owned_native_eh_private_leaf_publication>{
            new owned_native_eh_private_leaf_publication{selected_source, stage, id, generation}}};
        // [real record owns original full IR][separate verified private IR]
        // [safe] Administrative publication lock excludes every entry reader.
        // Only this original private factory moves these native owners.
        owner->original_ = ::std::move(rec.llvm_jit_compiled.llvm_jit_module);
        rec.llvm_jit_compiled.llvm_jit_module = ::std::move(candidate);
        return owner;
    }
    catch(...) { return {}; } // Own staging allocation only; no decoder or guest operation.
#else
    static_cast<void>(rec); return {};
#endif
}

inline bool owned_native_eh_private_leaf_publication::bind_actual_numeric_bridge(
    ::llvm::ExecutionEngine& engine, ::llvm::LLVMContext& context) noexcept
{
    namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    auto const type{::llvm::FunctionType::get(::llvm::Type::getVoidTy(context),
        {::llvm::IntegerType::get(context, sizeof(::std::uintptr_t) * CHAR_BIT),
         ::llvm::IntegerType::get(context, sizeof(::std::uintptr_t) * CHAR_BIT),
         ::llvm::IntegerType::get(context, sizeof(::std::uintptr_t) * CHAR_BIT),
         ::llvm::IntegerType::get(context, sizeof(::std::uintptr_t) * CHAR_BIT)}, false)};
    auto const name{compiler::details::get_llvm_runtime_bridge_function_symbol_name<
        ::uwvm2::runtime::lib::details::llvm_jit_throw_numeric_private_leaf_no_trace_wide_r1>(type)};
    auto const function{engine.FindFunctionNamed(compiler::details::get_llvm_string_ref(name))};
    auto const address{compiler::details::get_llvm_runtime_bridge_function_address(
        ::uwvm2::runtime::lib::details::llvm_jit_throw_numeric_private_leaf_no_trace_wide_r1)};
    if(function == nullptr || !function->isDeclaration() || function->getFunctionType() != type ||
       function->getCallingConv() != ::llvm::CallingConv::C || function->hasFnAttribute(::llvm::Attribute::NoUnwind) || address == 0u)
    { return false; }
    // [actual private engine-owned ABI declaration][actual retained host bridge]
    // [safe] Binding occurs before native relocation/finalization. No guest
    // pointer or stale cached address can select this versioned host target.
    engine.addGlobalMapping(function, reinterpret_cast<void*>(address));
    return engine.getPointerToGlobalIfAvailable(function) == reinterpret_cast<void*>(address);
}

inline bool owned_native_eh_private_leaf_publication::verify_actual_selected_calls(
    compiled_module_record const& rec, ::llvm::ExecutionEngine& engine) const noexcept
{
#ifdef UWVM_CPP_EXCEPTIONS
    namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    namespace private_ir = compiler::native_eh_private_leaf;
    try // Candidate-owned names/identity storage only, never Wasm decoding or execution.
    {
        auto const staged{stage_->ir_for_cold_inspection()};
        auto const total{stage_->selected_edges()};
        if(staged == nullptr || total == 0uz || total > 4'096uz || rec.llvm_jit_compiled.local_funcs.size() > 256uz) { return false; }
        ::std::vector<private_ir::call_identity> seen{};
        seen.reserve(total);
        for(::std::size_t target{}; target != rec.llvm_jit_compiled.local_funcs.size(); ++target)
        {
            if(!stage_->observed_clone_for_local_index(target)) { continue; }
            auto const public_name{compiler::details::get_llvm_wasm_function_name(*rec.runtime_module,
                static_cast<compiler::details::validation_module_traits_t::wasm_u32>(target))};
            auto const private_name{::uwvm2::utils::container::u8concat_uwvm(public_name, u8".native_eh_private_leaf.r1")};
            auto const clone{engine.FindFunctionNamed(compiler::details::get_llvm_string_ref(private_name))};
            auto const original_clone{staged->getFunction(compiler::details::get_llvm_string_ref(private_name))};
            if(clone == nullptr || original_clone == nullptr || clone->isDeclaration() || original_clone->isDeclaration()) { return false; }
            // [actual engine-owned compiler-only ABI-retention initializer]
            // [safe] Only this exact metadata array may keep a clone address;
            // every executable use still needs its original selected-call proof.
            auto const abi_retention{private_ir::exact_private_abi_retention(*clone, stage_->clones())};
            if(abi_retention == nullptr) { return false; }
            unsigned retention_uses{};
            for(auto const user: clone->users())
            {
                if(user == abi_retention)
                {
                    if(++retention_uses != 1u) { return false; }
                    continue;
                }
                // [actual engine-owned clone use][synchronous publisher borrow]
                // [safe] No IR pointer survives this method. Other address-taking,
                // aliases, indirect casts and newly unproved callers decline.
                auto const actual{::llvm::dyn_cast<::llvm::CallBase>(user)};
                private_ir::call_identity identity{};
                if(actual == nullptr || actual->getCalledFunction() != clone ||
                   !private_ir::read_actual_call_metadata(*actual, identity) || identity.target_index != target ||
                   identity.function_index >= rec.llvm_jit_compiled.local_funcs.size() ||
                   actual->getFunctionType() != clone->getFunctionType() || actual->getCallingConv() != clone->getCallingConv() ||
                   actual->arg_size() != clone->arg_size() || seen.size() == total) { return false; }
                for(unsigned arg{}; arg != actual->arg_size(); ++arg)
                { if(actual->getArgOperand(arg)->getType() != clone->getFunctionType()->getParamType(arg)) { return false; } }
                for(auto const& earlier: seen)
                { if(earlier.function_index == identity.function_index && earlier.callsite_index == identity.callsite_index) { return false; } }
                auto const caller_name{compiler::details::get_llvm_wasm_function_name(*rec.runtime_module,
                    static_cast<compiler::details::validation_module_traits_t::wasm_u32>(identity.function_index))};
                auto const actual_caller{engine.FindFunctionNamed(compiler::details::get_llvm_string_ref(caller_name))};
                auto const original_caller{staged->getFunction(compiler::details::get_llvm_string_ref(caller_name))};
                if(actual_caller == nullptr || original_caller == nullptr || actual->getFunction() != actual_caller) { return false; }
                ::std::size_t originals{};
                for(auto const& block: *original_caller) for(auto const& instruction: block)
                {
                    auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                    if(call == nullptr || call->getMetadata(private_ir::call_metadata_name) == nullptr) { continue; }
                    private_ir::call_identity expected{};
                    if(!private_ir::read_actual_call_metadata(*call, expected)) { return false; }
                    if(expected != identity) { continue; }
                    if(call->getCalledFunction() != original_clone || ++originals != 1uz) { return false; }
                }
                if(originals != 1uz) { return false; }
                seen.push_back(identity);
            }
            // Exactly one metadata element per selected clone. Combined with
            // the exact array size, this also rejects extra retained addresses.
            if(retention_uses != 1u) { return false; }
        }
        // Exact preserved selection set: no lost/duplicated/inlined call or
        // foreign caller is silently turned into private native permission.
        return seen.size() == total;
    }
    catch(...) { return false; }
#else
    static_cast<void>(rec); static_cast<void>(engine); return false;
#endif
}

inline bool owned_native_eh_private_leaf_publication::seal_actual_loaded(compiled_module_record const& rec,
    ::llvm::ExecutionEngine& engine, ::uwvm2::runtime::lib::details::pending_llvm_jit_code_ranges const& ranges,
    ::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager const& manager) noexcept
{
#if defined(UWVM_CPP_EXCEPTIONS) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    namespace compiler = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
    if(loaded_binding_ || committed_ || runtime_generation_ != current_runtime_generation() ||
       !stage_->matches_actual_full_result(rec.llvm_jit_compiled) ||
       source_->initialized_main_module() != rec.runtime_module || source_->bound_initialized_main_module_id() != module_id_ ||
       rec.llvm_jit_full_publication == nullptr || rec.llvm_jit_full_publication->source.get() != source_.get() ||
       rec.llvm_jit_full_publication->source.owner_before(source_) || source_.owner_before(rec.llvm_jit_full_publication->source) ||
       manager.has_finalization_failure() || engine.hasError() || ranges.empty() || g_runtime.debug_pause_control)
    { return false; }
    if(!verify_actual_selected_calls(rec, engine)) { return false; }
    clone_count_ = 0uz;
    for(::std::size_t index{}; index != rec.llvm_jit_compiled.local_funcs.size(); ++index)
    {
        if(!stage_->observed_clone_for_local_index(index)) { continue; }
        auto const public_name{compiler::details::get_llvm_wasm_function_name(*rec.runtime_module,
            static_cast<compiler::details::validation_module_traits_t::wasm_u32>(index))};
        auto const private_name{::uwvm2::utils::container::u8concat_uwvm(public_name, u8".native_eh_private_leaf.r1")};
        auto const function{engine.FindFunctionNamed(compiler::details::get_llvm_string_ref(private_name))};
        auto const public_leaf{engine.FindFunctionNamed(compiler::details::get_llvm_string_ref(public_name))};
        if(function == nullptr || public_leaf == nullptr || public_leaf->isDeclaration() ||
           function->getFunctionType() != public_leaf->getFunctionType() || function->getCallingConv() != public_leaf->getCallingConv() ||
           function->isDeclaration() || !function->hasLocalLinkage() ||
           !function->hasFnAttribute(::llvm::Attribute::NoInline) || function->hasFnAttribute(::llvm::Attribute::NoUnwind) ||
           !function->hasFnAttribute(::llvm::Attribute::UWTable)) { return false; }
        // [actual engine owns the still-private exact clone function]
        // [safe] LLVM resolves native code. The original runtime's descriptor
        // conversion recovers code PC before inspecting relocated object ranges.
        auto const pointer{engine.getPointerToFunction(function)};
        auto const begin{::uwvm2::runtime::lib::details::native_function_code_address(reinterpret_cast<::std::uintptr_t>(pointer))};
        auto const size{ranges.private_leaf_exact_loaded_function_size(begin)};
        if(begin == 0u || size == 0u || !manager.private_leaf_has_actual_registered_cfi(begin, size) || clone_count_ == 256uz)
        { return false; }
        for(::std::size_t previous{}; previous != clone_count_; ++previous)
        { if(loaded_[previous].begin == begin) { return false; } }
        loaded_[clone_count_++] = {index, begin, size};
    }
    if(clone_count_ != stage_->clones() || runtime_generation_ != current_runtime_generation()) { return false; }
#if defined(UWVM2TEST_NATIVE_EH_PRIVATE_LEAF_CFI_FAILURE)
    // Test-only failure AFTER every real clone extent/registered FDE matched.
    // This cannot supply a missing proof or select code; it only rejects it.
    if(::uwvm2::runtime::lib::uwvm2test_private_leaf_reject_after_actual_cfi()) { return false; }
#endif
    if(!source_->record_actual_full_validation(module_id_, runtime_generation_, rec.runtime_module)) { return false; }
    // [actual full publication will retain this exact engine/context]
    // [safe] Borrow only after every clone extent/FDE and source/generation match.
    // The containing publication retires engine/CFI before this data owner/source.
    actual_engine_ = ::std::addressof(engine);
    loaded_binding_ = true;
    return true;
#else
    static_cast<void>(rec); static_cast<void>(engine); static_cast<void>(ranges); static_cast<void>(manager);
    return false;
#endif
}

inline bool owned_native_eh_private_leaf_publication::actual_loaded_binding_matches(compiled_module_record const& rec) const noexcept
{
    auto const& publication{rec.llvm_jit_full_publication};
    if(!loaded_binding_ || !committed_ || actual_engine_ == nullptr || clone_count_ == 0uz || clone_count_ > 256uz ||
       runtime_generation_ == 0u || runtime_generation_ != current_runtime_generation() ||
       module_id_ >= g_runtime.modules.size() ||
       ::std::addressof(g_runtime.modules.index_unchecked(module_id_)) != ::std::addressof(rec) ||
       !rec.llvm_jit_ready || !publication || publication->private_eh_leaf.get() != this ||
       publication->engine.get() != actual_engine_ || !publication->context || publication->plan ||
       publication->runtime_epoch != runtime_generation_ ||
       !source_type::has_canonical_owner(source_) || publication->source.get() != source_.get() ||
       publication->source.owner_before(source_) || source_.owner_before(publication->source) ||
       source_->initialized_main_module() != rec.runtime_module || source_->bound_initialized_main_module_id() != module_id_ ||
       source_->actual_full_validation_epoch() != runtime_generation_ ||
       !stage_ || !stage_->matches_actual_full_result(rec.llvm_jit_compiled) || stage_->clones() != clone_count_)
    { return false; }
    // The retained full publication owns this exact engine and its registered
    // FDEs until actual stop/drain. This is cold data observation, not a new
    // attach/replacement permission or a caller-supplied epoch authority.
    return true;
}

[[nodiscard]] inline bool runtime_native_eh_private_leaf_management_blocked() noexcept
{
    // Requires the existing runtime_state_publication_guard or its exclusive
    // administrative owner. No domain lock, callback or guest entry is acquired.
    // Any private owner blocks management, including stale/malformed bindings;
    // only an actual stop/drain can retire that owner before normal setup.
    for(auto const& rec : g_runtime.modules)
    { if(rec.llvm_jit_full_publication && rec.llvm_jit_full_publication->private_eh_leaf) { return true; } }
    return false;
}

inline void owned_native_eh_private_leaf_publication::restore_original_after_failed_private_engine(compiled_module_record& rec) noexcept
{
    if(committed_ || (rec.llvm_jit_full_publication && rec.llvm_jit_full_publication->engine)) { ::fast_io::fast_terminate(); }
    // [candidate native scope already detached listener and destroyed engine/CFI]
    // [safe] Restore the original public module/context pair, not redirected IR.
    rec.llvm_jit_compiled.llvm_jit_module = ::std::move(original_);
    rec.llvm_jit_compiled.staged_native_eh_private_leaf.reset();
    rec.llvm_jit_local_entry_addresses.clear(); rec.llvm_jit_local_raw_entry_addresses.clear();
    actual_engine_ = nullptr; loaded_binding_ = false; clone_count_ = 0uz;
}

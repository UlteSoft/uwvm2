// Independent owned staging IR only. The ordinary runtime publisher does not
// consume this field. Loaded code, CFI/extent/generation and drain are unbound.
namespace native_eh_private_leaf
{
    // This compiler-only list preserves the original typed ABI across IPO. It
    // is not executable address publication. DeadArgumentElimination otherwise
    // removes an unused implicit context parameter from an internal clone;
    // NoInline and NoIPA do not preserve that signature in LLVM 23.
    // https://llvm.org/docs/LangRef.html#the-llvm-compiler-used-global-variable
    [[nodiscard]] inline ::llvm::ConstantArray const* exact_private_abi_retention(
        ::llvm::Function const& function, ::std::size_t clones) noexcept
    {
        // [live private engine-owned function/module][synchronous inspection]
        // [safe] Borrow only during publication; never retain a native IR pointer.
        auto const module{function.getParent()};
        if(module == nullptr || function.getAddressSpace() != 0u || clones == 0uz || clones > 256uz) { return nullptr; }
        auto const global{module->getNamedGlobal("llvm.compiler.used")};
        if(global == nullptr || !global->hasAppendingLinkage() || global->getAddressSpace() != 0u ||
           global->getSection() != "llvm.metadata" || global->isThreadLocal() || global->isConstant() ||
           global->hasDLLImportStorageClass() || global->hasDLLExportStorageClass() ||
           !global->hasInitializer() || !global->use_empty()) { return nullptr; }
        // [exact compiler-owned initializer][one metadata-global use only]
        // [safe] No load, store, alias or guest-visible address consumes this list.
        auto const values{::llvm::dyn_cast<::llvm::ConstantArray>(global->getInitializer())};
        if(values == nullptr || values->getNumOperands() != clones || !values->hasOneUse() ||
           *values->user_begin() != global || !values->getType()->getElementType()->isPointerTy() ||
           values->getType()->getElementType()->getPointerAddressSpace() != 0u) { return nullptr; }
        return values;
    }

    class staged_module final
    {
        friend inline constexpr full_function_symbol_t
            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_all_from_uwvm(
                ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const&, compile_option&,
                ::uwvm2::validation::error::code_validation_error_impl&, ::std::size_t, compile_task_split_config) UWVM_THROWS;
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
        native_eh_leaf_observer::module_attempt::owner actual_attempt_{};
#endif
        // Declared in destruction order: IR retires before its OWN context.
        ::std::unique_ptr<::llvm::LLVMContext> context_{};
        ::std::unique_ptr<::llvm::Module> module_{};
        ::std::size_t selected_edges_{}, clones_{}, replaced_numeric_throws_{};
        bool cloned_leaf_indices_[256uz]{};
        staged_module() noexcept = default;
        [[nodiscard]] static bool is_exact_numeric_throw(::llvm::CallBase const& call) noexcept
        {
            auto const type{call.getFunctionType()};
            auto const width{static_cast<unsigned>(sizeof(::std::uintptr_t) * CHAR_BIT)};
            if(!type->getReturnType()->isVoidTy() || type->isVarArg() || type->getNumParams() != 4u) { return false; }
            for(unsigned index{}; index != 4u; ++index)
            { if(!type->getParamType(index)->isIntegerTy(width)) { return false; } }
            auto const target{call.getCalledOperand()->stripPointerCasts()};
            if(auto const function{::llvm::dyn_cast<::llvm::Function>(target)}; function != nullptr)
            {
                auto const name{details::get_llvm_runtime_bridge_function_symbol_name<
                    ::uwvm2::runtime::lib::details::llvm_jit_throw_numeric_abi_bridge>(type)};
                auto const view{details::get_llvm_string_ref(name)};
                auto const address{details::get_llvm_runtime_bridge_function_address(
                    ::uwvm2::runtime::lib::details::llvm_jit_throw_numeric_abi_bridge)};
                return function->getFunctionType() == type && function->getName() == view &&
                    ::llvm::sys::DynamicLibrary::SearchForAddressOfSymbol(reinterpret_cast<char const*>(name.c_str())) ==
                        reinterpret_cast<void*>(address);
            }
            // Initial staging safely declines pointer materializations it cannot
            // prove. No target-name whitelist changes native exception support.
            return false;
        }
        [[nodiscard]] static ::std::shared_ptr<staged_module const> build_after_actual_full_fusion(
            full_function_symbol_t const& original, compile_option const& options) noexcept
        {
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1 && defined(UWVM_CPP_EXCEPTIONS)
            auto const& observed{original.native_eh_leaf_observations};
            auto const& attempt{observed.attempt};
            if(!options.stage_native_eh_private_leaf || !observed.complete || !attempt || !attempt->stage_private_leaf ||
               attempt->private_call_metadata_failed.load(::std::memory_order_relaxed) ||
               options.llvm_jit_task_module_pre_link_callback != nullptr ||
               !original.llvm_jit_module.emitted || !original.llvm_jit_module.llvm_module ||
               !native_eh_leaf_observer::source_type::has_canonical_owner(attempt->source()) ||
               attempt->source()->initialized_main_module() != attempt->module() ||
               attempt->source()->bound_initialized_main_module_id() != attempt->domain()->module_index()) { return {}; }
            try // Only private staging work. Original IR/module/source never move.
            {
                ::std::shared_ptr<staged_module> result{new staged_module{}};
                result->actual_attempt_ = attempt;
                result->context_ = ::std::make_unique<::llvm::LLVMContext>();
                ::llvm::SmallVector<char, 0u> bytes{};
                ::llvm::raw_svector_ostream stream{bytes};
                ::llvm::WriteBitcodeToFile(*original.llvm_jit_module.llvm_module, stream);
                // [owned serialized ACTUAL complete fused IR][bytes.size] end
                // [safe] Borrow only until parse returns; the private module has
                // its independent context and never retains this native span.
                auto parsed{::llvm::parseBitcodeFile(::llvm::MemoryBufferRef{
                    ::llvm::StringRef{bytes.data(), bytes.size()}, "uwvm-private-leaf-staged-r1"}, *result->context_)};
                if(!parsed) { ::llvm::consumeError(parsed.takeError()); return {}; }
                result->module_ = ::std::move(*parsed);
                // The first publisher creates its own single retention list.
                // An earlier unknown list cannot be reused as ABI provenance.
                if(result->module_->getNamedValue("llvm.compiler.used") != nullptr ||
                   ::llvm::verifyModule(*result->module_)) { return {}; }
                ::std::array<::llvm::Function*, 256uz> private_clones{};
                for(::std::size_t index{}; index != original.local_funcs.size(); ++index)
                {
                    // [actual closed source function slots][index < size <= 256]
                    // [safe] All workers joined and the original owner is retained.
                    auto const& local{original.local_funcs.index_unchecked(index)};
                    auto const& caller{*local.native_eh_leaf_observation};
                    auto const name{details::get_llvm_wasm_function_name(*attempt->module(),
                        static_cast<details::validation_module_traits_t::wasm_u32>(index))};
                    auto const function{result->module_->getFunction(details::get_llvm_string_ref(name))};
                    if(function == nullptr || function->isDeclaration()) { return {}; }
                    ::std::array<::llvm::CallBase*, 4'096uz> sites{};
                    for(auto& block: *function) for(auto& instruction: block)
                    {
                        auto const call{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                        if(call == nullptr || call->getMetadata(call_metadata_name) == nullptr) { continue; }
                        call_identity actual{};
                        if(!read_actual_call_metadata(*call, actual) || actual.function_index != index ||
                           actual.callsite_index >= caller.witnesses.size() || sites[actual.callsite_index] != nullptr) { return {}; }
                        auto const& witness{caller.witnesses[actual.callsite_index]};
                        call_identity const expected{index, witness.expression_offset, witness.event_ordinal,
                            witness.observation_callsite_index, witness.actual_public_target_index};
                        if(actual != expected) { return {}; }
                        // [private module owns this exact current call]
                        // [safe] Temporary native borrow only during staging;
                        // it never becomes code admission or an external record.
                        sites[actual.callsite_index] = call;
                    }
                    for(auto const& witness: caller.witnesses)
                    {
                        auto const site{witness.observation_callsite_index};
                        auto const target{witness.actual_public_target_index};
                        if(site >= sites.size() || target >= original.local_funcs.size() || sites[site] == nullptr) { return {}; }
                        auto const& callee{*original.local_funcs.index_unchecked(target).native_eh_leaf_observation};
                        auto const callee_name{details::get_llvm_wasm_function_name(*attempt->module(),
                            static_cast<details::validation_module_traits_t::wasm_u32>(target))};
                        auto const public_leaf{result->module_->getFunction(details::get_llvm_string_ref(callee_name))};
                        auto const call{sites[site]};
                        if(public_leaf == nullptr || public_leaf->isDeclaration() || call->getCalledFunction() != public_leaf ||
                           call->getFunctionType() != public_leaf->getFunctionType() ||
                           call->getCallingConv() != public_leaf->getCallingConv() || call->arg_size() != public_leaf->arg_size()) { return {}; }
                        for(unsigned arg{}; arg != call->arg_size(); ++arg)
                        { if(call->getArgOperand(arg)->getType() != public_leaf->getFunctionType()->getParamType(arg)) { return {}; } }
                        if(native_eh_leaf_observer::effect::observe_consumed_direct_call(*caller.observation, site,
                            *callee.observation) != native_eh_leaf_observer::effect::selection::observed_consumed) { continue; }
                        if(public_leaf->hasFnAttribute(::llvm::Attribute::NoUnwind)) { return {}; }
                        auto& clone{private_clones[target]};
                        if(clone == nullptr)
                        {
                            ::llvm::ValueToValueMapTy map{};
                            // [actual private module-owned complete public IR]
                            // [safe] LLVM clones only IR; no Wasm bytes are read.
                            clone = ::llvm::CloneFunction(public_leaf, map);
                            if(clone == nullptr || clone->getParent() != result->module_.get() ||
                               clone->getFunctionType() != public_leaf->getFunctionType() ||
                               clone->getCallingConv() != public_leaf->getCallingConv()) { return {}; }
                            clone->setName(public_leaf->getName() + ".native_eh_private_leaf.r1");
                            clone->setLinkage(::llvm::GlobalValue::InternalLinkage);
                            // An actual standalone native extent/FDE is required
                            // by the first publisher. Do not let inlining hide
                            // this logical callee behind the caller's PC range.
                            clone->addFnAttr(::llvm::Attribute::NoInline);
                            ::std::size_t raises{};
                            for(auto& block: *clone) for(auto& instruction: block)
                            {
                                auto const raised{::llvm::dyn_cast<::llvm::CallBase>(::std::addressof(instruction))};
                                if(raised == nullptr || !is_exact_numeric_throw(*raised)) { continue; }
                                auto const module_id{::llvm::dyn_cast<::llvm::ConstantInt>(raised->getArgOperand(0u))};
                                auto const tag_id{::llvm::dyn_cast<::llvm::ConstantInt>(raised->getArgOperand(1u))};
                                if(module_id == nullptr || tag_id == nullptr || module_id->getZExtValue() != attempt->domain()->module_index() ||
                                   tag_id->getZExtValue() >= attempt->module()->local_defined_tag_vec_storage.size() || raised->doesNotThrow()) { return {}; }
                                ::llvm::IRBuilder<> builder{raised};
                                auto const bridge{details::get_llvm_runtime_bridge_function_symbol_value_unwrapped<
                                    ::uwvm2::runtime::lib::details::llvm_jit_throw_numeric_private_leaf_no_trace_wide_r1>(builder, raised->getFunctionType())};
                                if(bridge == nullptr) { return {}; }
                                // [private clone-owned exact numeric call][same four-intptr ABI]
                                // [safe] Replace only this callee operand; original public IR,
                                // tuple/handler/cleanup operands and native unwind stay intact.
                                raised->setCalledFunction(raised->getFunctionType(), bridge);
                                ++raises;
                            }
                            if(raises == 0uz) { return {}; }
                            result->replaced_numeric_throws_ += raises;
                            result->cloned_leaf_indices_[target] = true;
                            ++result->clones_;
                        }
                        // [witnessed private-module call][same typed clone ABI]
                        // [safe] This independent IR owns both endpoints. No guest
                        // function-index table or published native pointer changes.
                        call->setCalledFunction(clone);
                        ++result->selected_edges_;
                    }
                }
                if(result->selected_edges_ == 0uz) { return {}; }
                ::llvm::SmallVector<::llvm::GlobalValue*, 16u> abi_retained{};
                for(auto const clone: private_clones)
                {
                    // [private module-owned actual selected clone][bounded 256 slots]
                    // [safe] This compiler metadata adds no runtime call or load.
                    if(clone != nullptr) { abi_retained.push_back(clone); }
                }
                if(abi_retained.size() != result->clones_) { return {}; }
                // Add once after all clones: repeated list reconstruction can
                // leave unused uniqued ConstantArray users of earlier clones.
                ::llvm::appendToCompilerUsed(*result->module_, abi_retained);
                if(::llvm::verifyModule(*result->module_)) { return {}; }
                return result;
            }
            catch(...) { return {}; } // Staging failure never resets/retries original fusion.
#else
            static_cast<void>(original); static_cast<void>(options); return {};
#endif
        }
    public:
        staged_module(staged_module const&) = delete;
        [[nodiscard]] ::llvm::Module const* ir_for_cold_inspection() const noexcept { return module_.get(); }
        [[nodiscard]] ::std::size_t selected_edges() const noexcept { return selected_edges_; }
        [[nodiscard]] ::std::size_t clones() const noexcept { return clones_; }
        [[nodiscard]] ::std::size_t replaced_numeric_throws() const noexcept { return replaced_numeric_throws_; }
        [[nodiscard]] bool observed_clone_for_local_index(::std::size_t index) const noexcept
        { return index < 256uz && cloned_leaf_indices_[index]; }
        [[nodiscard]] bool matches_actual_full_result(full_function_symbol_t const& actual) const noexcept
        {
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
            auto const& current{actual.native_eh_leaf_observations};
            auto const& stage{actual.staged_native_eh_private_leaf};
            return stage.get() == this && current.complete && actual_attempt_ &&
                current.attempt.get() == actual_attempt_.get() && !current.attempt.owner_before(actual_attempt_) &&
                !actual_attempt_.owner_before(current.attempt) && actual.local_funcs.size() == actual_attempt_->function_count() &&
                current.completed_functions == actual.local_funcs.size() && selected_edges_ != 0uz && clones_ != 0uz;
#else
            static_cast<void>(actual); return false;
#endif
        }
        [[nodiscard]] static constexpr bool has_loaded_code_permission() noexcept { return false; }
        [[nodiscard]] static constexpr bool has_registered_native_cfi() noexcept { return false; }
        [[nodiscard]] static constexpr bool runtime_selected() noexcept { return false; }
    };
}

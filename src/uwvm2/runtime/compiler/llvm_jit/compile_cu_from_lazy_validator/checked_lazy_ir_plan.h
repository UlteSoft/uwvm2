#pragma once
// Lexical implementation: included inside the lazy compiler's details
// namespace after its actual types/LLVM dependencies are visible. It is a .h,
// never a raw-byte replay, and has no independent guest admission authority.
class checked_lazy_ir_plan final
{
    struct function_record
    {
        void const* original_code{};
        void const* original_signature{};
        ::std::unique_ptr<char[]> bitcode{};
        ::std::size_t bitcode_bytes{};
        ::std::unique_ptr<::uwvm2::validation::standard::wasm3::validated_call_dependency[]> dependencies{};
        ::std::size_t dependency_count{};
        ::uwvm2::utils::container::array<::std::byte, 32uz> bitcode_digest{};
        // Original fused emitter's cold outcome, not a Wasm validation error,
        // native entry, interpreter permit, or authority to replay the source.
        ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::llvm_jit_compiler_first_decline native_decline{};
    };
    class bounded_bitcode_stream final : public ::llvm::raw_ostream
    {
        ::std::unique_ptr<char[]> buffer_{};
        ::std::size_t limit_{};
        ::std::size_t bytes_{};
        bool overflow_{};
        void write_impl(char const* source, ::std::size_t count) override
        {
            if(overflow_) { return; }
            if(bytes_ > limit_ || count > limit_ - bytes_)
            { overflow_ = true; return; }
            if(count == 0uz) { return; }
            // [one owned scratch allocation ... limit_] | one-past
            // [safe: count<=limit_-bytes_             ] | unsafe
            //                         ^^ buffer_+bytes_ AFTER the subtraction check.
            ::fast_io::freestanding::my_memcpy(buffer_.get() + bytes_, source, count);
            bytes_ += count;
        }
        ::std::uint64_t current_pos() const override { return bytes_; }
    public:
        explicit bounded_bitcode_stream(::std::size_t limit)
            : ::llvm::raw_ostream{true}, buffer_{limit == 0uz ? nullptr : new char[limit]}, limit_{limit} {}
        [[nodiscard]] bool complete() const noexcept { return !overflow_; }
        [[nodiscard]] ::std::size_t bytes() const noexcept { return bytes_; }
        [[nodiscard]] ::std::unique_ptr<char[]> take_owned_bytes()
        {
            if(overflow_ || bytes_ == 0uz) { return {}; }
            ::std::unique_ptr<char[]> result{new char[bytes_]};
            // [both complete owned allocations: bytes_ bytes] | one-past
            // [safe copy; neither pointer is advanced       ] | unsafe
            ::fast_io::freestanding::my_memcpy(result.get(), buffer_.get(), bytes_);
            return result;
        }
    };
    runtime_module_storage_t const* original_module_{};
    ::std::uint64_t runtime_generation_{};
    compile_option emission_options_{};
    ::std::unique_ptr<function_record[]> functions_{};
    // Only tiered admission owns this exact same-walk metadata projection.
    // Ordinary lazy keeps null, with no per-function allocation/copy overhead.
    // The existing full local_func/options layouts are never enlarged.
    ::std::unique_ptr<::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::local_func_storage_t[]> whole_semantic_metadata_{};
    ::std::size_t function_count_{};
    ::std::size_t retained_bytes_{};
    bool validation_complete_{};
    bool resource_exhausted_{};
    explicit checked_lazy_ir_plan(runtime_module_storage_t const& module, compile_option const& options)
        : original_module_{::std::addressof(module)},
          runtime_generation_{::uwvm2::runtime::lib::observe_compiler_runtime_generation_host_api()}, emission_options_{options} {}

    [[nodiscard]] bool same_emission_options(compile_option const& candidate) const noexcept
    {
        auto const& original{emission_options_};
        // Optimization and verification strength may be changed when consuming
        // checked IR. Every option affecting emitted body/call ABI must match.
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
        // This lazy owner cannot authenticate full-only observer publications.
        if(candidate.record_native_eh_leaf_observations || candidate.native_eh_leaf_active_attempt)
        { return false; }
#endif
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
        if(candidate.stage_native_eh_private_leaf) { return false; }
#endif
        return original.curr_wasm_id == candidate.curr_wasm_id &&
            original.compiler_registry == candidate.compiler_registry &&
            original.compilation_mode == candidate.compilation_mode &&
            original.debug_compiled_function_generation == candidate.debug_compiled_function_generation &&
            original.llvm_jit_task_module_pre_link_callback == candidate.llvm_jit_task_module_pre_link_callback &&
            original.llvm_jit_task_module_pre_link_callback_context == candidate.llvm_jit_task_module_pre_link_callback_context &&
            original.validator_feature_parameter == candidate.validator_feature_parameter &&
            original.route_wasm_calls_through_runtime_bridge == candidate.route_wasm_calls_through_runtime_bridge &&
            original.lazy_defined_raw_call_target_base_address == candidate.lazy_defined_raw_call_target_base_address &&
            original.lazy_defined_raw_call_target_count == candidate.lazy_defined_raw_call_target_count &&
            original.lazy_defined_typed_entry_target_base_address == candidate.lazy_defined_typed_entry_target_base_address &&
            original.lazy_defined_typed_entry_target_count == candidate.lazy_defined_typed_entry_target_count &&
            original.lazy_defined_targets_are_atomic == candidate.lazy_defined_targets_are_atomic &&
            original.emit_tiered_loop_reentry_entries == candidate.emit_tiered_loop_reentry_entries &&
            original.emit_call_stack_frames == candidate.emit_call_stack_frames &&
            original.emit_unwind_call_stack_frames == candidate.emit_unwind_call_stack_frames &&
            original.emit_precise_gc_root_frames == candidate.emit_precise_gc_root_frames &&
            original.native_exception_target_machine == candidate.native_exception_target_machine &&
            original.pending_numeric_fold_proven_empty_calls == candidate.pending_numeric_fold_proven_empty_calls &&
            !candidate.emit_debug_safe_points && !candidate.checkpoint_profile && candidate.pending_numeric_plan == nullptr &&
            candidate.debug_full_patchable_typed_target_base_address == 0u && candidate.debug_full_patchable_typed_target_count == 0uz;
    }
public:
    // These quotas bound RETAINED compiler bitcode/indices and one bitcode
    // writer scratch. Existing one-function LLVM emitter/validator scratch and
    // LLVM object-code compilation remain separate resources; this is not RSS.
    static constexpr ::std::size_t maximum_functions{65536uz};
    static constexpr ::std::size_t maximum_function_bitcode_bytes{8uz * 1024uz * 1024uz};
    static constexpr ::std::size_t maximum_retained_bytes{128uz * 1024uz * 1024uz};
    checked_lazy_ir_plan(checked_lazy_ir_plan const&) = delete;
    checked_lazy_ir_plan& operator=(checked_lazy_ir_plan const&) = delete;

    [[nodiscard]] bool matches_source(runtime_module_storage_t const& module) const noexcept
    {
        if(!validation_complete_ || original_module_ != ::std::addressof(module) ||
           runtime_generation_ != ::uwvm2::runtime::lib::observe_compiler_runtime_generation_host_api() ||
           function_count_ != module.local_defined_function_vec_storage.size()) { return false; }
        for(::std::size_t index{}; index != function_count_; ++index)
        {
            // [actual current module local records] end; index<size before read.
            // The worker-drain contract pins this module and its immutable source.
            auto const& current{module.local_defined_function_vec_storage.index_unchecked(index)};
            auto const& original{functions_[index]};
            if(current.wasm_code_ptr != original.original_code || current.function_type_ptr != original.original_signature)
            { return false; }
        }
        return true;
    }
    [[nodiscard]] bool matches_emission_options(compile_option const& options) const noexcept
    { return same_emission_options(options); }
    // All contained bodies were checked by the original fused traversal. Legal
    // native lowering decline is per-function availability, not module validity.
    // Resource exhaustion can NEVER manufacture successful admission.
    [[nodiscard]] bool admission_available() const noexcept
    { return validation_complete_ && !resource_exhausted_; }
    [[nodiscard]] bool native_ir_available(::std::size_t index) const noexcept
    {
        if(!admission_available() || index >= function_count_) { return false; }
        // [actual owned function records0 ... index ... count] records_end
        // [safe] complete admission and index<count BEFORE record selection.
        auto const& record{functions_[index]};
        return record.bitcode != nullptr && record.bitcode_bytes != 0uz &&
            record.bitcode_bytes <= maximum_function_bitcode_bytes;
    }
    [[nodiscard]] bool native_lowering_decline(::std::size_t index,
        ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::llvm_jit_compiler_first_decline& output) const noexcept
    {
        if(!admission_available() || index >= function_count_ || native_ir_available(index)) { return false; }
        // [exact owned record0 ... index ... count] end
        // [safe] bounded cold DATA read; it grants no execution/fallback rights.
        auto const& record{functions_[index]};
        if(!record.native_decline.available()) { return false; }
        output = record.native_decline;
        return true;
    }
    [[nodiscard]] ::std::size_t retained_bytes() const noexcept { return retained_bytes_; }
    [[nodiscard]] bool resource_exhausted() const noexcept { return resource_exhausted_; }
    [[nodiscard]] compile_option const& emission_options() const noexcept { return emission_options_; }

    [[nodiscard]] ::uwvm2::utils::container::u8string artifact_hash(::std::size_t index) const noexcept
    {
        if(!native_ir_available(index)) { ::fast_io::fast_terminate(); }
        ::uwvm2::utils::container::u8string result{};
        result.reserve(64uz);
        ::uwvm2::utils::container::u8string_ref_uwvm output{::std::addressof(result)};
        for(auto const byte: functions_[index].bitcode_digest)
        { ::fast_io::io::print(output, ::fast_io::mnp::hex<false, true>(::std::to_integer<::std::uint_least8_t>(byte))); }
        return result;
    }

    [[nodiscard]] static ::std::unique_ptr<checked_lazy_ir_plan> admit(
        runtime_module_storage_t const& module, lazy_module_storage_t& storage,
        compile_option options, ::uwvm2::validation::error::code_validation_error_impl& error)
    {
        // Explicit off-world compiler registries are full-only temporary borrows;
        // this lazy artifact has no owner that could pin them until later demand.
        if(options.compiler_registry != nullptr || options.emit_debug_safe_points || options.checkpoint_profile || options.pending_numeric_plan != nullptr ||
           options.debug_full_patchable_typed_target_base_address != 0u || options.debug_full_patchable_typed_target_count != 0uz)
        { ::fast_io::fast_terminate(); } // Full-only policy already rejected by runtime.
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER) && UWVM_EXPERIMENTAL_NATIVE_EH_LEAF_OBSERVER == 1
        if(options.record_native_eh_leaf_observations || options.native_eh_leaf_active_attempt)
        { ::fast_io::fast_terminate(); }
#endif
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
        if(options.stage_native_eh_private_leaf) { ::fast_io::fast_terminate(); }
#endif
        // Per-function verification is mandatory before retaining a compiler
        // artifact, even when later object-code optimization verification is off.
        options.verify_llvm_jit_ir = true;
        ::std::unique_ptr<checked_lazy_ir_plan> pending{new checked_lazy_ir_plan{module, options}};
        static_assert(maximum_functions <= maximum_retained_bytes / sizeof(function_record));
        auto const count{module.local_defined_function_vec_storage.size()};
        if(count <= maximum_functions)
        { pending->functions_.reset(new function_record[count]); pending->function_count_ = count; pending->retained_bytes_ = count * sizeof(function_record); }
        else { pending->resource_exhausted_ = true; }
        if(count <= maximum_functions && options.compilation_mode ==
            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::llvm_jit_compilation_mode::tiered)
        {
            using record = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::local_func_storage_t;
            if(count > maximum_retained_bytes / sizeof(record)) { pending->resource_exhausted_ = true; }
            else
            {
                // Quotient bounds precede multiplication; fixed record bytes
                // and all variable loop bytes share the complete retained quota.
                auto const bytes{count * sizeof(record)};
                if(pending->retained_bytes_ > maximum_retained_bytes || bytes > maximum_retained_bytes - pending->retained_bytes_)
                { pending->resource_exhausted_ = true; }
                else
                {
                    if(count != 0uz) { pending->whole_semantic_metadata_.reset(new record[count]); }
                    pending->retained_bytes_ += bytes;
                }
            }
        }
        // All bodies must be typed by THIS fused traversal, including unused
        // bodies after retention exhausts. No pure validation prepass or fallback
        // to the original byte stream is performed at materialization.
        for(::std::size_t index{}; index != count; ++index)
        {
            llvm_jit_module_storage_t ir{};
            auto const prepared{all_details::try_prepare_runtime_llvm_jit_module_storage(module, ir,
                options.emit_unwind_call_stack_frames, options.native_exception_target_machine)};
            if(!prepared) { ir.note_first_decline(::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::llvm_jit_compiler_decline_stage::module_prepare); }
            // Only this admission invocation owns the bounded transient sink.
            // Default full/ROS calls pass null and add no metadata allocation.
            ::std::vector<::uwvm2::validation::standard::wasm3::validated_call_dependency> dependencies{};
            bool dependencies_overflow{};
            auto local{all_details::compile_all_from_uwvm_local_func(module, storage.validation_module,
                options, index, error, prepared ? ::std::addressof(ir) : nullptr,
                ::std::addressof(dependencies), ::std::addressof(dependencies_overflow))};
            // Authoritative validation is complete before checking IR/resource
            // availability. Failure to lower a legal body is not a type error.
            // A function-prepare decline can leave a declaration-only module.
            // Even successful verification of that empty IR is NOT an emitted
            // body. Preserve the original decline and reject that native artifact
            // BEFORE finalization, while continuing the actual typed traversal.
            bool const complete{prepared && !ir.first_decline.available() &&
                ir.llvm_module != nullptr && ir.llvm_context_holder != nullptr &&
                all_details::finalize_runtime_llvm_jit_module_storage(ir, true) && ir.emitted};
            if(count <= maximum_functions)
            {
                auto& retained{pending->functions_[index]};
                auto const& actual{module.local_defined_function_vec_storage.index_unchecked(index)};
                retained.original_code = actual.wasm_code_ptr;
                retained.original_signature = actual.function_type_ptr;
                retained.native_decline = ir.first_decline;
                // This exact vector was produced by the original operand/type
                // checks. No callee immediate is decoded from source a second time.
                if(dependencies_overflow ||
                   dependencies.size() > maximum_retained_bytes / sizeof(::uwvm2::validation::standard::wasm3::validated_call_dependency))
                { pending->resource_exhausted_ = true; }
                if(!pending->resource_exhausted_)
                {
                    // Multiplication follows the quotient bound; subtraction
                    // follows retained_bytes_<=the complete quota.
                    auto const bytes{dependencies.size() * sizeof(::uwvm2::validation::standard::wasm3::validated_call_dependency)};
                    if(pending->retained_bytes_ > maximum_retained_bytes || bytes > maximum_retained_bytes - pending->retained_bytes_)
                    { pending->resource_exhausted_ = true; }
                    else
                    {
                        retained.dependency_count = dependencies.size();
                        if(bytes != 0uz)
                        {
                            retained.dependencies.reset(new ::uwvm2::validation::standard::wasm3::validated_call_dependency[retained.dependency_count]);
                            // [owned typed DATA vector / exact owned array] bytes
                            // [safe: count quotient + equal extents proved above]
                            ::fast_io::freestanding::my_memcpy(retained.dependencies.get(), dependencies.data(), bytes);
                        }
                        pending->retained_bytes_ += bytes;
                    }
                }
                if(!pending->resource_exhausted_ && pending->whole_semantic_metadata_ != nullptr)
                {
                    // Debug/checkpoint/experimental full-only metadata is absent
                    // by the actual policy gates above. Account every variable
                    // retained loop descriptor BEFORE copying its owned vector.
                    // FastIO vector copy allocates exactly size(), not capacity().
                    auto const loops{local.tiered_loop_reentries.size()};
                    using loop_record = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::tiered_loop_reentry_storage_t;
                    if(loops > maximum_retained_bytes / sizeof(loop_record))
                    { pending->resource_exhausted_ = true; }
                    else
                    {
                        auto const bytes{loops * sizeof(loop_record)};
                        if(pending->retained_bytes_ > maximum_retained_bytes || bytes > maximum_retained_bytes - pending->retained_bytes_)
                        { pending->resource_exhausted_ = true; }
                        else
                        {
                            pending->whole_semantic_metadata_[index] = local;
                            pending->retained_bytes_ += bytes;
                        }
                    }
                }
                if(complete && !pending->resource_exhausted_)
                {
                    auto const available{maximum_retained_bytes - pending->retained_bytes_};
                    auto const limit{available < maximum_function_bitcode_bytes ? available : maximum_function_bitcode_bytes};
                    bounded_bitcode_stream stream{limit};
                    ::llvm::WriteBitcodeToFile(*ir.llvm_module, stream);
                    if(!stream.complete() || stream.bytes() == 0uz)
                    { pending->resource_exhausted_ = true; }
                    else
                    {
                        retained.bitcode_bytes = stream.bytes();
                        retained.bitcode = stream.take_owned_bytes();
                        pending->retained_bytes_ += retained.bitcode_bytes;
                        ::fast_io::sha256_context digest{};
                        auto const first{reinterpret_cast<::std::byte const*>(retained.bitcode.get())};
                        // [exact owned bitcode ... bitcode_bytes] | one-past
                        // [safe: nonzero bounded writer length  ] | only end formed
                        // ^^ first; first+length is bounded by its actual allocation.
                        digest.update(first, first + retained.bitcode_bytes);
                        digest.do_final(); digest.digest_to_byte_ptr(retained.bitcode_digest.data());
                    }
                }
            }
            // The transient sink dies at this iteration's end; only the exact
            // bounded array above remains in the plan quota. Local metadata has
            // no new vector/member, even in ordinary full and ROS builds.
            storage.materialized_functions.index_unchecked(index).local_func = ::std::move(local);
            // `ir` destroys its module BEFORE its context at this iteration's end.
            // Failed/allocation-unwound attempts retain no dangling LLVM values.
        }
        pending->validation_complete_ = error.err_code == ::uwvm2::validation::error::code_validation_error_code::ok;
        return pending;
    }

    [[nodiscard]] bool append_callees(runtime_module_storage_t const& module, ::std::size_t index,
        ::uwvm2::utils::container::vector<::std::size_t>& out, bool unwind) const
    {
        // The group entry checks the entire source once. Per-edge cold lookup
        // checks only this original immutable descriptor, avoiding N squared
        // module identity walks through a large transitive call graph.
        if(!validation_complete_ || original_module_ != ::std::addressof(module) ||
           runtime_generation_ != ::uwvm2::runtime::lib::observe_compiler_runtime_generation_host_api() ||
           function_count_ != module.local_defined_function_vec_storage.size() || index >= function_count_) { return false; }
        auto const& actual{module.local_defined_function_vec_storage.index_unchecked(index)};
        if(actual.wasm_code_ptr != functions_[index].original_code || actual.function_type_ptr != functions_[index].original_signature) { return false; }
        auto const imported{module.imported_function_vec_storage.size()};
        auto const locals{function_count_};
        auto const& retained{functions_[index]};
        for(::std::size_t event{}; event != retained.dependency_count; ++event)
        {
            // [exact owned dependency array ... count] | end
            // [safe: event<count before read        ] | one-past never read
            auto const dependency{retained.dependencies[event]};
            using kind = ::uwvm2::validation::standard::wasm3::validated_call_dependency_kind;
            if(dependency.kind == kind::direct_function)
            {
                if(dependency.index >= imported && dependency.index - imported < locals)
                { append_unique_local_function_index(out, dependency.index - imported); }
            }
            else if(unwind)
            {
                // A table or reference is mutable guest state. Retained DATA must
                // not freeze only the initial targets and miss a later table.set.
                // The conservative local cohort preserves native-unwind typed
                // call availability without rescanning guest code/table tokens.
                out.clear(); out.reserve(locals);
                for(::std::size_t target{}; target != locals; ++target) { out.push_back(target); }
                return true; // The complete cohort subsumes subsequent edges.
            }
        }
        return true;
    }

    template<typename Indices>
    [[nodiscard]] bool select_complete_cohort_unwind_import_routes(runtime_module_storage_t const& module,
        lazy_module_storage_t const& scheduler, Indices const& indices, llvm_jit_module_storage_t& ir) const
    {
        // The exact private plan allocation survives a legitimate storage move;
        // its unique owner and all actual scheduler/source records below must
        // agree. A temporary factory stack address is never an identity token.
        // This is still unpublished IR DATA, never a native-entry publication.
        if(!matches_source(module) || scheduler.checked_ir_plan.get() != this ||
           scheduler.functions.size() != function_count_ || scheduler.materialized_functions.size() != function_count_ || !emission_options_.emit_unwind_call_stack_frames ||
           !emission_options_.route_wasm_calls_through_runtime_bridge || ir.llvm_module == nullptr ||
           ir.llvm_context_holder == nullptr || indices.empty()) { return false; }
        auto const imported{module.imported_function_vec_storage.size()};
        auto const maximum_index{static_cast<::std::size_t>((::std::numeric_limits<all_details::validation_module_traits_t::wasm_u32>::max)())};
        if(imported > maximum_index || (function_count_ != 0uz && function_count_ - 1uz > maximum_index - imported))
        { return false; }
        for(auto const index: indices)
        {
            if(!native_ir_available(index) || index >= function_count_) { return false; }
            // [this exact moved-or-original scheduler vectors] count
            // [safe] index<count BEFORE function/materialized/module access;
            // public index addition was subtraction-bounded above.
            auto const& function{scheduler.functions.index_unchecked(index)};
            auto const& actual{module.local_defined_function_vec_storage.index_unchecked(index)};
            auto const& metadata{scheduler.materialized_functions.index_unchecked(index).local_func};
            if(function.local_function_index != index || function.function_index != imported + index ||
               function.primary_cu_index >= scheduler.compile_units.size() || actual.wasm_code_ptr == nullptr ||
               actual.function_type_ptr == nullptr || metadata.runtime_module_ptr != original_module_ ||
               metadata.module_id != emission_options_.curr_wasm_id || metadata.function_index != imported + index ||
               metadata.wasm_code_ptr != actual.wasm_code_ptr || metadata.function_type_ptr != actual.function_type_ptr ||
               // [retained source byte views] compare complete native addresses only;
               // [safe] no pointer advance or dereference of either byte representation.
               static_cast<void const*>(metadata.code_begin) != static_cast<void const*>(actual.wasm_code_ptr->body.expr_begin) ||
               static_cast<void const*>(metadata.code_end) != static_cast<void const*>(actual.wasm_code_ptr->body.code_end) ||
               function.materialization_state.state.load(::std::memory_order_acquire) !=
                   ::uwvm2::utils::thread::lazy_compile_state::compiling) { return false; }
            // [actual primary-CU array ... bounded primary index] end
            // [safe] bounds above BEFORE CU reference; borrowed code is only
            // compared to the already retained descriptor, never read/advanced.
            auto const& cu{scheduler.compile_units.index_unchecked(function.primary_cu_index)};
            if(cu.function_index != function.function_index || cu.local_function_index != index ||
               cu.kind != lazy_compile_unit_kind::function || cu.code_begin != metadata.code_begin || cu.code_end != metadata.code_end ||
               cu.state.state.load(::std::memory_order_acquire) != ::uwvm2::utils::thread::lazy_compile_state::compiling)
            { return false; }
        }
        return select_retained_unwind_import_routes(module, indices, ir, false);
    }
private:
    template<typename Indices>
    [[nodiscard]] bool select_retained_unwind_import_routes(runtime_module_storage_t const& module,
        Indices const& indices, llvm_jit_module_storage_t& ir, bool all_owned_definitions) const
    {
        if(!matches_source(module) || !admission_available() || ir.llvm_module == nullptr ||
           ir.llvm_context_holder == nullptr) { return false; }
        if(!emission_options_.emit_unwind_call_stack_frames ||
           !emission_options_.route_wasm_calls_through_runtime_bridge) { return true; }
        auto const imported{module.imported_function_vec_storage.size()};
        auto const maximum_index{static_cast<::std::size_t>((::std::numeric_limits<all_details::validation_module_traits_t::wasm_u32>::max)())};
        if(imported > maximum_index || (function_count_ != 0uz && function_count_ - 1uz > maximum_index - imported))
        { return false; }
        auto const selected{[&](::std::size_t index) noexcept
        { for(auto const actual: indices) { if(actual == index) { return true; } } return false; }};
        constexpr auto key{"uwvm.checked.lazy.unwind.import.route.v1"};
        auto const integer{[](::llvm::Metadata* value, ::std::uint64_t& out) noexcept
        {
            auto const wrapper{::llvm::dyn_cast_or_null<::llvm::ConstantAsMetadata>(value)};
            auto const actual{wrapper == nullptr ? nullptr : ::llvm::dyn_cast<::llvm::ConstantInt>(wrapper->getValue())};
            if(actual == nullptr || actual->getBitWidth() != 64u) { return false; }
            out = actual->getZExtValue(); return true;
        }};
        ::std::vector<::llvm::BranchInst*> ready{};
        constexpr ::std::size_t maximum_routes{65536uz};
        for(auto& function: *ir.llvm_module)
        {
            for(auto& block: function)
            {
                // Safe for an empty or unfinished block across LLVM versions.
                auto const terminal{block.empty() || !block.back().isTerminator() ? nullptr : ::std::addressof(block.back())};
                auto const identity{terminal == nullptr ? nullptr : terminal->getMetadata(key)};
                if(identity == nullptr) { continue; }
                auto const branch{::llvm::dyn_cast<::llvm::BranchInst>(terminal)};
                if(branch == nullptr || !branch->isConditional() || identity->getNumOperands() != 6u ||
                   ready.size() == maximum_routes) { return false; }
                auto const condition{::llvm::dyn_cast<::llvm::ConstantInt>(branch->getCondition())};
                ::std::uint64_t module_id{}, caller_index{}, import_index{}, target_index{};
                if(condition == nullptr || !condition->getType()->isIntegerTy(1u) || !condition->isZero() ||
                   !integer(identity->getOperand(0u).get(), module_id) || module_id != emission_options_.curr_wasm_id ||
                   !integer(identity->getOperand(1u).get(), caller_index) || caller_index < imported ||
                   caller_index - imported >= function_count_ || !selected(static_cast<::std::size_t>(caller_index - imported)) ||
                   !integer(identity->getOperand(2u).get(), import_index) || import_index >= imported ||
                   !integer(identity->getOperand(3u).get(), target_index) || target_index < imported ||
                   target_index - imported >= function_count_)
                { return false; }
                auto const caller_constant{::llvm::dyn_cast_or_null<::llvm::ConstantAsMetadata>(identity->getOperand(5u).get())};
                auto const target_constant{::llvm::dyn_cast_or_null<::llvm::ConstantAsMetadata>(identity->getOperand(4u).get())};
                auto const target{target_constant == nullptr ? nullptr : ::llvm::dyn_cast<::llvm::Function>(target_constant->getValue())};
                if(caller_constant == nullptr || caller_constant->getValue() != ::std::addressof(function) ||
                   target == nullptr || target->getParent() != ir.llvm_module.get() || target->isIntrinsic() ||
                   branch->getSuccessor(0u)->getParent() != ::std::addressof(function) ||
                   branch->getSuccessor(1u)->getParent() != ::std::addressof(function)) { return false; }
                // Bounds above prove u64->wasm_u32 narrowing before the resolver.
                // Original retained actual source+runtime generation precedes
                // this import-chain walk; no Wasm byte cursor is consulted.
                auto const resolved{all_details::resolve_runtime_direct_callee(module,
                    static_cast<all_details::validation_module_traits_t::wasm_u32>(import_index))};
                auto const signature{all_details::resolve_runtime_callee_function_type(module,
                    static_cast<all_details::validation_module_traits_t::wasm_u32>(import_index))};
                if(!resolved.state_valid || !resolved.direct_callable || resolved.function_type_ptr == nullptr ||
                   signature == nullptr || resolved.func_index != target_index ||
                   !all_details::checked_retained_import_signature_equivalent(module, *resolved.function_type_ptr, *signature)) { return false; }
                auto const expected_type{all_details::get_llvm_function_type_from_wasm_function_type(*ir.llvm_context_holder, *signature)};
                auto const expected_name{all_details::get_llvm_wasm_function_name(module,
                    static_cast<all_details::validation_module_traits_t::wasm_u32>(target_index))};
                if(expected_type == nullptr || target->getFunctionType() != expected_type ||
                   target->getCallingConv() != all_details::get_llvm_jit_typed_calling_conv(*expected_type) ||
                   target->getName() != all_details::get_llvm_string_ref(expected_name)) { return false; }
                auto const local{static_cast<::std::size_t>(target_index - imported)};
                if(!selected(local)) { continue; } // Preserve original raw fallback.
                // Grouped T1 selection required the actual function/CU claims
                // above. Whole consumption instead proved ALL factory-owned
                // typed/native fragments; the real same merged module must own
                // the selected callee body, not only an external declaration.
                if(all_owned_definitions && target->isDeclaration()) { return false; }
                if(local >= emission_options_.lazy_defined_typed_entry_target_count ||
                   local >= emission_options_.lazy_defined_raw_call_target_count ||
                   emission_options_.lazy_defined_typed_entry_target_base_address == 0u ||
                   emission_options_.lazy_defined_raw_call_target_base_address == 0u) { return false; }
                auto const original_signature{module.local_defined_function_vec_storage.index_unchecked(local).function_type_ptr};
                if(original_signature == nullptr || original_signature != functions_[local].original_signature ||
                   !all_details::checked_retained_import_signature_equivalent(module, *original_signature, *signature)) { return false; }
                ready.push_back(branch);
            }
        }
        // Complete preflight first. Each condition is replaced by a compiler
        // constant, so codegen can delete the unused original bridge CFG.
        for(auto const branch: ready) { branch->setCondition(::llvm::ConstantInt::getTrue(*ir.llvm_context_holder)); }
        return all_details::finalize_runtime_llvm_jit_module_storage(ir, true);
    }

    [[nodiscard]] bool specialize_owned_tiered_local_targets(runtime_module_storage_t const& module,
        llvm_jit_module_storage_t& ir) const
    {
        namespace target_ir = ::uwvm2::runtime::compiler::llvm_jit::checked_whole_local_targets;
        if(!matches_source(module) || !admission_available() || ir.llvm_module == nullptr || ir.llvm_context_holder == nullptr ||
           emission_options_.compilation_mode !=
               ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::llvm_jit_compilation_mode::tiered ||
           emission_options_.compiler_registry != nullptr ||
           (function_count_ != 0uz && whole_semantic_metadata_ == nullptr)) { return false; }
        if(function_count_ == 0uz) { return true; }
        auto const imported{module.imported_function_vec_storage.size()};
        using wasm_u32 = all_details::validation_module_traits_t::wasm_u32;
        auto const maximum{static_cast<::std::size_t>((::std::numeric_limits<wasm_u32>::max)())};
        if(imported > maximum || function_count_ - 1uz > maximum - imported ||
           emission_options_.lazy_defined_typed_entry_target_base_address == 0u ||
           emission_options_.lazy_defined_typed_entry_target_count < function_count_) { return false; }
        auto const& layout{ir.llvm_module->getDataLayout()};
        if(layout.isDefault() || layout.getPointerSizeInBits(0u) != sizeof(::std::uintptr_t) * 8u) { return false; }
        // Retained source/feature/emission-options/runtime epoch were authenticated
        // by this private plan and actual worker callback. Projection is cold
        // compiler DATA: it supplies neither a native pointer nor publication.
        ::std::vector<target_ir::function_binding> bindings{};
        bindings.reserve(function_count_); // Parent admission's <=65536 owner bound.
        for(::std::size_t index{}; index != function_count_; ++index)
        {
            if(!native_ir_available(index)) { return false; }
            // [factory metadata / actual source0 ... index ... count] end
            // [safe] index<count BEFORE source/metadata lookup or u32 narrowing.
            auto const& local{whole_semantic_metadata_[index]};
            auto const& actual{module.local_defined_function_vec_storage.index_unchecked(index)};
            if(local.runtime_module_ptr != original_module_ || local.module_id != emission_options_.curr_wasm_id ||
               local.function_index != imported + index || local.wasm_code_ptr != actual.wasm_code_ptr ||
               local.function_type_ptr != actual.function_type_ptr || local.function_type_ptr == nullptr ||
               local.compiler_registry != nullptr || local.checkpoint_plan || !local.debug_safe_point_bits.empty()) { return false; }
            auto const public_index{static_cast<wasm_u32>(imported + index)};
            auto const type{all_details::get_llvm_function_type_from_wasm_function_type(*ir.llvm_context_holder, *local.function_type_ptr)};
            auto const name{all_details::get_llvm_wasm_function_name(module, public_index)};
            auto const definition{ir.llvm_module->getFunction(all_details::get_llvm_string_ref(name))};
            if(type == nullptr || definition == nullptr || definition->isDeclaration() ||
               definition->getParent() != ir.llvm_module.get() || definition->getFunctionType() != type ||
               definition->getCallingConv() != all_details::get_llvm_jit_typed_calling_conv(*type)) { return false; }
            ::llvm::Function* core{};
            if(emission_options_.emit_tiered_loop_reentry_entries)
            {
                auto const core_name{all_details::get_llvm_wasm_tiered_core_function_name(module, public_index)};
                core = ir.llvm_module->getFunction(all_details::get_llvm_string_ref(core_name));
                if constexpr(::std::numeric_limits<::std::size_t>::digits < ::std::numeric_limits<unsigned>::digits)
                {
                    if(type->getNumParams() > (::std::numeric_limits<::std::size_t>::max)() - 2uz) { return false; }
                }
                if(type->getNumParams() > (::std::numeric_limits<unsigned>::max)() - 2u) { return false; }
                ::std::vector<::llvm::Type*> parameters{};
                // Both size_t/LLVM unsigned additions proved BEFORE reserve/type creation.
                parameters.reserve(type->getNumParams() + 2uz);
                parameters.push_back(::llvm::Type::getInt32Ty(*ir.llvm_context_holder));
                parameters.push_back(layout.getIntPtrType(*ir.llvm_context_holder));
                for(auto const parameter: type->params()) { parameters.push_back(parameter); }
                auto const core_type{::llvm::FunctionType::get(type->getReturnType(), parameters, false)};
                if(core == nullptr || core->getParent() != ir.llvm_module.get() || core->isDeclaration() ||
                   !core->hasInternalLinkage() || core->getFunctionType() != core_type ||
                   core->getCallingConv() != all_details::get_llvm_jit_typed_calling_conv(*core_type)) { return false; }
            }
            bindings.push_back({definition, core, static_cast<::std::uint64_t>(local.module_id),
                static_cast<::std::uint64_t>(local.function_index)});
        }
        // The generated symbol's owned string remains live throughout this
        // synchronous read/transform. Neither StringRef nor target DATA escapes.
        auto const table_name{all_details::get_llvm_lazy_typed_entry_target_table_symbol_name(module)};
        auto const result{target_ir::specialize(*ir.llvm_module, bindings,
            {static_cast<::std::uint64_t>(emission_options_.lazy_defined_typed_entry_target_base_address),
             all_details::get_llvm_string_ref(table_name), emission_options_.lazy_defined_targets_are_atomic})};
        if(result.state == target_ir::status::rejected_before_mutation ||
           result.state == target_ir::status::rejected_after_verification) { return false; }
        return all_details::finalize_runtime_llvm_jit_module_storage(ir, true);
    }

public:

    // Compiler-artifact consumption only. The caller still owes the actual
    // execution-domain/source/generation lease, target-machine lifetime, native
    // codegen/CFI closure and transactional ALL-entry publication. Neither this
    // full-shaped result nor its metadata creates those permissions.
    //
    // This preserves the ORIGINAL admitted call/frame/OSR ABI. It does not claim
    // that NoInline OSR wrappers have become the old direct-call T2 layout.
    // Only tagged static LOCAL call slot reads are specialized after ALL actual
    // owned definitions/ABI/source preflight; mutable ref/table and foreign
    // import dispatch remain unchanged. No reject replays the raw Wasm body.
    [[nodiscard]] bool consume_whole_checked_symbol(runtime_module_storage_t const& module,
        full_function_symbol_t& output) const
    {
        if(!matches_source(module) || !admission_available() ||
           (function_count_ != 0uz && whole_semantic_metadata_ == nullptr) ||
           emission_options_.compilation_mode !=
               ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::llvm_jit_compilation_mode::tiered ||
           emission_options_.llvm_jit_task_module_pre_link_callback != nullptr ||
           !output.local_funcs.empty() || output.llvm_jit_module.llvm_module != nullptr ||
           output.llvm_jit_module.llvm_context_holder != nullptr || output.llvm_jit_module.emitted)
        { return false; }
        auto const imported{module.imported_function_vec_storage.size()};
        using wasm_u32 = all_details::validation_module_traits_t::wasm_u32;
        auto const maximum_index{static_cast<::std::size_t>((::std::numeric_limits<wasm_u32>::max)())};
        if(imported > maximum_index || (function_count_ != 0uz && function_count_ - 1uz > maximum_index - imported))
        { return false; }
        // Complete read-only preflight BEFORE any output/LLVM mutation. A legal
        // unavailable native body makes this whole-IR optimization unavailable,
        // while the already admitted module and its T0/T1 remain valid.
        for(::std::size_t index{}; index != function_count_; ++index)
        {
            if(!native_ir_available(index)) { return false; }
            // [private factory-owned metadata0 ... index ... count] records_end
            // [safe] index<count BEFORE lookup; public/local index addition was
            // quotient/subtraction-checked above. No source cursor advances.
            auto const& actual{module.local_defined_function_vec_storage.index_unchecked(index)};
            auto const& local{whole_semantic_metadata_[index]};
            if(local.runtime_module_ptr != original_module_ || local.module_id != emission_options_.curr_wasm_id ||
               local.function_index != imported + index || local.wasm_code_ptr != actual.wasm_code_ptr ||
               local.function_type_ptr != actual.function_type_ptr || local.compiler_registry != nullptr || local.checkpoint_plan ||
               !local.debug_safe_point_bits.empty()) { return false; }
        }
        full_function_symbol_t staged{};
        ::uwvm2::utils::container::vector<::std::size_t> indices{};
        indices.reserve(function_count_);
        for(::std::size_t index{}; index != function_count_; ++index) { indices.push_back(index); }
        // Only LLVM's reader consumes exact bounded owned compiler bitcode.
        // Failure destroys staged module BEFORE its LLVM context by RAII; the
        // caller output remains unchanged until ALL fragments verified below.
        if(!materialize_checked_ir(module, emission_options_, indices, staged.llvm_jit_module) ||
           !select_retained_unwind_import_routes(module, indices, staged.llvm_jit_module, true) ||
           !specialize_owned_tiered_local_targets(module, staged.llvm_jit_module)) { return false; }
        staged.local_funcs.reserve(function_count_);
        for(::std::size_t index{}; index != function_count_; ++index)
        {
            // [owned immutable same-walk metadata0 ... index ... count] end
            // [safe] complete index-bound check from this loop BEFORE copy.
            // No reconstructed locals, raw matcher, or Wasm decoder is used.
            staged.local_funcs.push_back(whole_semantic_metadata_[index]);
        }
        // Per-task optimization callbacks were explicitly rejected above. No
        // guessed callback-complete flag suppresses the real final optimizer.
        staged.llvm_jit_task_modules_pre_link_optimized = false;
        output = ::std::move(staged);
        return true;
    }

    template<typename Indices>
    [[nodiscard]] bool materialize_checked_ir(runtime_module_storage_t const& module, compile_option const& options,
        Indices const& indices, llvm_jit_module_storage_t& merged) const
    {
        if(!matches_source(module) || !same_emission_options(options) || !admission_available()) { return false; }
        // Reject an unavailable selection BEFORE creating/mutating the merged
        // IR owner. Callers may skip an unused failed member; this is never a
        // permission to compile it from the original Wasm source again.
        for(auto const index: indices) { if(!native_ir_available(index)) { return false; } }
        if(!all_details::try_prepare_runtime_llvm_jit_module_storage(module, merged,
            options.emit_unwind_call_stack_frames, options.native_exception_target_machine) || merged.llvm_context_holder == nullptr ||
            merged.llvm_module == nullptr) { return false; }
        ::llvm::Linker linker{*merged.llvm_module};
        for(auto const index: indices)
        {
            if(index >= function_count_) { return false; }
            auto const& source{functions_[index]};
            if(source.bitcode == nullptr || source.bitcode_bytes == 0uz ||
               source.bitcode_bytes > maximum_function_bitcode_bytes) { return false; }
            // [exact owned checked BITCODE allocation ... bytes] | one-past
            // [safe: immutable compiler output, explicit length] | unsafe
            // No Wasm pointer/LEB/type matcher is consulted by this LLVM reader.
            auto fragment{::llvm::parseBitcodeFile(::llvm::MemoryBufferRef{
                ::llvm::StringRef{source.bitcode.get(), source.bitcode_bytes}, "checked-lazy-body"}, *merged.llvm_context_holder)};
            if(!fragment) { ::llvm::consumeError(fragment.takeError()); return false; }
            if(linker.linkInModule(::std::move(*fragment))) { return false; }
        }
        return all_details::finalize_runtime_llvm_jit_module_storage(merged, true);
    }
};

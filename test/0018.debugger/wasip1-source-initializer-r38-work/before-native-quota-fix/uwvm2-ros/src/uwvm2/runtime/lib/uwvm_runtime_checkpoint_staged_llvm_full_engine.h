// Native cold compilation DATA only; included inside uwvm2::runtime::lib
// after the real full-materializer/owned replacement parser helpers.
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && \
    defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE)
#include "uwvm_runtime_checkpoint_staged_native_endpoints.h"
extern "C++"
{
    class runtime_checkpoint_world_transaction;
    // No public construction, entry getter or publication operation. The real
    // world transaction alone consumes this DATA after its independent graph,
    // resources, aliases, generation, source/cache/product and drain proofs.
    class runtime_checkpoint_staged_llvm_full_engine final
    {
        friend class runtime_checkpoint_world_transaction;
        using translator = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::compile_option;
        using module_owner = ::uwvm2::uwvm::runtime::initializer::staged_compiler_module_owner;
        using local_storage = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::local_func_storage_t;
        using profile_owner = ::uwvm2::runtime::checkpoint::compilation_profile::owner;
        struct effective_function_body
        {
            // Strict local order, with the genuine graph's public index and
            // actual committed generation. Generation one uses owned FINAL
            // parser code; later generations contain the complete body encoding
            // (locals + instructions + end), never an erased native pointer.
            ::std::size_t public_function_index{};
            ::std::uint64_t generation{};
            ::std::vector<::std::byte> body{};
        };
        enum class call_stack_policy : unsigned { instruction, unwind };
        enum class preparation_status : unsigned
        { ok, invalid_input, invalid_body, unsupported_native, lowering_declined, exhausted, native_failure, native_endpoint_failure };
        struct native_entries
        { ::std::uintptr_t typed{}, raw{}, resume_typed{}, resume_raw{}; };

        module_owner owner_;
        profile_owner profile_{};
        ::std::size_t module_id_{};
        // Stable typed parser owners precede all borrowed descriptors. Their
        // original section bytes and local declarations survive native teardown.
        ::std::vector<::std::unique_ptr<llvm_jit_debug_replace_transaction>> effective_bodies_{};
        ::std::vector<::std::uint64_t> generations_{};
        ::std::vector<local_storage> locals_{};
        // The actual runtime record owns this SAME container type. A real
        // world commit moves its allocation; copying into a new buffer would
        // invalidate target addresses already embedded in the private LLVM IR.
        ::uwvm2::utils::container::vector<::std::uintptr_t> typed_targets_{};
        ::std::vector<native_entries> entries_{};
        unsigned debug_shutdown_abi_revision_{};
        // Borrowed from THIS engine-owned real memory manager until native teardown.
        ::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager const* debug_cfi_manager_{};
        // Reverse member destruction detaches the pending listener first, then
        // destroys the engine/CFI allocations before context/types/body/source.
        ::uwvm2::utils::container::delete_owned_ptr<::llvm::LLVMContext> context_{};
        ::uwvm2::utils::container::delete_owned_ptr<::llvm::ExecutionEngine> engine_{};
        ::uwvm2::utils::container::delete_owned_ptr<details::pending_llvm_jit_code_ranges> ranges_{};
#include "uwvm_runtime_checkpoint_staged_native_endpoint_declarations.h"

        explicit runtime_checkpoint_staged_llvm_full_engine(module_owner const& owner,
            profile_owner profile, ::std::size_t module_id)
            : owner_{owner}, profile_{::std::move(profile)}, module_id_{module_id} {}
        runtime_checkpoint_staged_llvm_full_engine(runtime_checkpoint_staged_llvm_full_engine const&) = delete;
        runtime_checkpoint_staged_llvm_full_engine& operator=(runtime_checkpoint_staged_llvm_full_engine const&) = delete;
        runtime_checkpoint_staged_llvm_full_engine(runtime_checkpoint_staged_llvm_full_engine&&) = delete;
        runtime_checkpoint_staged_llvm_full_engine& operator=(runtime_checkpoint_staged_llvm_full_engine&&) = delete;
        struct preparation_result
        {
            ::std::unique_ptr<runtime_checkpoint_staged_llvm_full_engine> data{};
            preparation_status status{preparation_status::invalid_input};
        };

        [[nodiscard]] static bool extent_contains(::std::byte const* begin, ::std::size_t size,
            ::std::byte const* body_begin, ::std::byte const* expression_begin, ::std::byte const* body_end) noexcept
        {
            // No pointer arithmetic or foreign pointee read: compare only native
            // identities after the strong owner established one actual image.
            auto const base{reinterpret_cast<::std::uintptr_t>(begin)};
            auto const body{reinterpret_cast<::std::uintptr_t>(body_begin)};
            auto const expression{reinterpret_cast<::std::uintptr_t>(expression_begin)};
            auto const end{reinterpret_cast<::std::uintptr_t>(body_end)};
            return begin != nullptr && body_begin != nullptr && expression_begin != nullptr && body_end != nullptr &&
                body >= base && expression >= body && end > expression && end >= base &&
                body - base < size && expression - base < size && end - base <= size;
        }

        [[nodiscard]] static preparation_result prepare(module_owner const& actual_owner,
            ::std::size_t actual_module_id, profile_owner actual_profile,
            ::std::vector<effective_function_body> const& effective,
            call_stack_policy stack_policy,
            ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::llvm_jit_debug_safe_point_granularity granularity,
            void* native_budget_owner,native_budget_charge native_charge) noexcept
        {
            namespace compile = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm;
            namespace emit = ::uwvm2::runtime::compiler::llvm_jit::compile_all_from_uwvm::details;
            namespace checkpoint = ::uwvm2::runtime::checkpoint;
            if(!actual_owner.matches_unpublished_file() || actual_module_id == SIZE_MAX || !actual_profile ||
                actual_profile->purpose() != checkpoint::compilation_purpose::resumable ||
                actual_profile->cache_identity()[1u] != 17u || checkpoint::native_resume_abi_revision != 2u ||
                (stack_policy != call_stack_policy::instruction && stack_policy != call_stack_policy::unwind) ||
                (granularity != compile::llvm_jit_debug_safe_point_granularity::entry_loop &&
                 granularity != compile::llvm_jit_debug_safe_point_granularity::instruction))
            { return {}; }
            if(stack_policy == call_stack_policy::unwind && !runtime_llvm_jit_unwind_can_replace_instruction_frames())
            { return {{}, preparation_status::unsupported_native}; }
            try
            {
                auto data{::std::unique_ptr<runtime_checkpoint_staged_llvm_full_engine>{
                    new runtime_checkpoint_staged_llvm_full_engine{actual_owner, ::std::move(actual_profile), actual_module_id}}};
                data->native_budget_owner_=native_budget_owner;data->native_charge_=native_charge;
                // [native context-minted source/module/FINAL file] end
                // [safe] exact membership was rechecked BEFORE borrowing parser
                // or runtime fields; the record retains every source allocation.
                auto const& runtime_module{*data->owner_.module()};
                auto const& source_file{*data->owner_.file()};
                auto const& parser_module{source_file.wasm_module_storage.wasm_binfmt_ver1_storage};
                auto const& features{source_file.wasm_parameter.binfmt1_para};
                auto const imported{runtime_module.imported_function_vec_storage.size()};
                auto const count{runtime_module.local_defined_function_vec_storage.size()};
                if(count != effective.size() || count > SIZE_MAX - imported)
                { return {}; }
                data->effective_bodies_.resize(count);
                data->generations_.resize(count);
                data->locals_.resize(count);
                // This allocation never moves with the nonmoving owner. Zero
                // private targets remain inaccessible until every symbol resolves.
                data->typed_targets_.resize(count, 0u);
                data->entries_.resize(count);
                auto validation_module{emit::build_runtime_validation_module(runtime_module)};
                auto& code_section{::uwvm2::parser::wasm::concepts::operation::get_first_type_in_tuple<
                    emit::validation_module_traits_t::code_section_storage_t>(validation_module.sections)};
                if(code_section.codes.size() != count) { return {}; }
                ::uwvm2::validation::error::code_validation_error_impl validation_error{};
                emit::require_runtime_module_declaration_policy(runtime_module, features, validation_error);
                for(::std::size_t index{}; index != count; ++index)
                {
                    // [0..count actual ordered body DATA] end
                    // [safe] index<count selects DATA, module declaration and
                    // private code slot before any raw expression pointer borrow.
                    auto const& body{effective[index]};
                    if(body.public_function_index != imported + index || body.generation == 0u ||
                        (body.generation == 1u ? !body.body.empty() : (body.body.size() < 2u || body.body.size() > 65536u)))
                    { return {}; }
                    auto local{emit::get_runtime_local_func_storage(runtime_module, index, validation_error)};
                    auto const* original{runtime_module.local_defined_function_vec_storage.index_unchecked(index).wasm_code_ptr};
                    if(original != ::std::addressof(code_section.codes.index_unchecked(index)) &&
                        (original == nullptr || original->body.code_begin != code_section.codes.index_unchecked(index).body.code_begin ||
                         original->body.expr_begin != code_section.codes.index_unchecked(index).body.expr_begin ||
                         original->body.code_end != code_section.codes.index_unchecked(index).body.code_end))
                    { return {}; }
                    if(!extent_contains(reinterpret_cast<::std::byte const*>(source_file.source_cbegin()), source_file.source_size(),
                        reinterpret_cast<::std::byte const*>(original->body.code_begin), local.code_begin, local.code_end))
                    { return {}; }
                    if(body.generation != 1u)
                    {
                        auto parsed{::std::make_unique<llvm_jit_debug_replace_transaction>()};
                        parsed->source = data->owner_.source(); parsed->local_index = index;
                        // The real feature-aware original code-section parser
                        // owns the complete saved body; no semantic/body prepass.
                        parse_llvm_jit_replacement_body(*parsed, parser_module, features, body.body.data(), body.body.size());
                        auto const expression{reinterpret_cast<::std::byte const*>(parsed->code.body.expr_begin)};
                        auto const end{reinterpret_cast<::std::byte const*>(parsed->code.body.code_end)};
                        if(!extent_contains(parsed->section_bytes.data(), parsed->section_bytes.size(),
                            reinterpret_cast<::std::byte const*>(parsed->code.body.code_begin), expression, end))
                        { return {{}, preparation_status::invalid_body}; }
                        code_section.codes.index_unchecked(index) = parsed->code;
                        code_section.locals_require_function_references |= parsed->locals_require_function_references;
                        // [actual parsed owned complete section] end
                        // [safe] both expression/end were checked inside that
                        // owner before replacing the private descriptor pointers.
                        local.wasm_code_ptr = ::std::addressof(parsed->code);
                        local.code_begin = expression; local.code_end = end;
                        data->effective_bodies_[index] = ::std::move(parsed);
                    }
                    local.module_id = actual_module_id;
                    auto const begin{reinterpret_cast<::std::uintptr_t>(local.code_begin)};
                    auto const end{reinterpret_cast<::std::uintptr_t>(local.code_end)};
                    auto const bytes{end - begin}; // same-owned extent proof above
                    local.debug_safe_point_bits.resize(bytes / 8u + (bytes % 8u != 0u));
                    for(auto& octet : local.debug_safe_point_bits) { octet = 0u; }
                    data->generations_[index] = body.generation;
                    data->locals_[index] = ::std::move(local);
                }

                if(!ensure_llvm_jit_native_target_initialized())
                { return {{}, preparation_status::unsupported_native}; }
                auto const cpu{get_llvm_jit_host_cpu_name_storage()};
                auto const attributes{get_llvm_jit_host_target_attribute_storage()};
                ::llvm::SmallVector<::llvm::StringRef, 16> attribute_refs{};
                append_llvm_jit_host_target_attribute_refs(attributes, attribute_refs);
                auto const policy{resolve_runtime_llvm_jit_full_materialize_strategy(::llvm::CodeGenOptLevel::Aggressive)};
                ::llvm::EngineBuilder target_builder{};
                target_builder.setEngineKind(::llvm::EngineKind::JIT).setOptLevel(policy.codegen_opt_level)
                    .setMCPU(emit::get_llvm_string_ref(cpu)).setMAttrs(attribute_refs);
                auto target{select_runtime_llvm_jit_target(target_builder, cpu, attributes)};
                if(target == nullptr) { return {{}, preparation_status::unsupported_native}; }
                if(policy.codegen_opt_level == ::llvm::CodeGenOptLevel::None) { target->setFastISel(true); }
                translator options{};
                options.curr_wasm_id = actual_module_id;
                options.compiler_registry = ::std::addressof(data->owner_.source()->registry());
                options.compilation_mode = compile::llvm_jit_compilation_mode::full;
                options.emit_debug_safe_points = true; options.checkpoint_profile = data->profile_;
                options.debug_safe_point_granularity = granularity;
                options.debug_full_patchable_typed_target_base_address = count == 0u ? 0u :
                    reinterpret_cast<::std::uintptr_t>(data->typed_targets_.data());
                options.debug_full_patchable_typed_target_count = count;
                options.validator_feature_parameter = ::std::addressof(features);
                options.verify_llvm_jit_ir = true;
                options.emit_call_stack_frames = stack_policy == call_stack_policy::instruction;
                options.emit_unwind_call_stack_frames = stack_policy == call_stack_policy::unwind;
                options.emit_precise_gc_root_frames = true;
                if constexpr(details::native_exception_host::available)
                {
                    if(::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::supports_itanium_dwarf_object(*target))
                    { options.native_exception_target_machine = target.get(); }
                }
                compile::llvm_jit_module_storage_t ir{};
                if(!emit::try_prepare_runtime_llvm_jit_module_storage(runtime_module, ir,
                    options.emit_unwind_call_stack_frames, options.native_exception_target_machine))
                { return {{}, preparation_status::lowering_declined}; }
                {
                emit::private_host_object_emission_scope owned_objects{data->owner_, *ir.llvm_module};
                for(::std::size_t index{}; index != count; ++index)
                {
                    auto& local{data->locals_[index]};
                    checkpoint::function_plan work{}; work.profile = data->profile_;
                    // [actual strong source-owned immutable registry] end
                    // [safe] cold borrow for this single fused emit only; the
                    // record/source and all aliases remain retained above.
                    local.compiler_registry = options.compiler_registry;
                    // Exactly one instruction walk validates this effective
                    // body while emitting LLVM and its rich continuation plan.
                    emit::validate_runtime_local_func(validation_module, local, validation_error, ::std::addressof(ir),
                        true, false,
                        options.emit_call_stack_frames, options.emit_unwind_call_stack_frames,
                        options.validator_feature_parameter,
                        true, options.compilation_mode, options.debug_safe_point_granularity,
                        options.debug_full_patchable_typed_target_base_address, count,
                        ::std::addressof(local.debug_safe_point_bits), true, nullptr,
                        data->generations_[index], ::std::addressof(work));
                    // [finished emitter state] no remaining registry borrow
                    // [safe] clear cold metadata before returning owned DATA.
                    local.compiler_registry = nullptr;
                    if(ir.llvm_module == nullptr || ir.llvm_context_holder == nullptr)
                    { return {{}, preparation_status::lowering_declined}; }
                    local.checkpoint_plan = checkpoint::sealed_function_plan::seal_compiler_metadata(::std::move(work));
                    if(!local.checkpoint_plan) { return {{}, preparation_status::lowering_declined}; }
                    auto const& plan{local.checkpoint_plan->get()};
                    if(plan.module != actual_module_id || plan.function != imported + index ||
                        plan.function_generation != data->generations_[index] || plan.profile.get() != data->profile_.get() ||
                        plan.profile.owner_before(data->profile_) || data->profile_.owner_before(plan.profile) ||
                        plan.producer_availability != checkpoint::status::ok || plan.resume_abi_revision != 2u || plan.resume_sites.empty())
                    { return {{}, preparation_status::lowering_declined}; }
                }
                if(!owned_objects.valid() || !emit::finalize_runtime_llvm_jit_module_storage(ir, true) ||
                    ir.llvm_module == nullptr || ir.llvm_context_holder == nullptr)
                { return {{}, preparation_status::lowering_declined}; }
                } // Restore compiler TLS before native optimization/materialization.
                auto module{::std::move(ir.llvm_module)};
                auto context{::std::move(ir.llvm_context_holder)};
                set_llvm_module_target_triple_from_machine(*module, *target);
                module->setDataLayout(target->createDataLayout());
                apply_runtime_llvm_jit_native_target_function_attrs(*module, cpu, cpu, *target);
                if(!optimize_runtime_llvm_jit_module(*module, *target, policy.pipeline, policy.codegen_opt_level, true, false))
                { return {{}, preparation_status::native_failure}; }
                for(auto& local : data->locals_) { for(auto& octet : local.debug_safe_point_bits) { octet = 0u; } }
                if(!visit_optimized_llvm_jit_debug_safe_points(*module,
                    [&](::std::uint64_t module_id, ::std::uint64_t function, ::std::uint64_t offset) noexcept
                    {
                        if(module_id != actual_module_id || function < imported || function - imported >= count) { return false; }
                        auto& local{data->locals_[static_cast<::std::size_t>(function - imported)]};
                        auto const& plan{local.checkpoint_plan->get()};
                        auto& bits{local.debug_safe_point_bits};
                        if(offset >= plan.expression_bytes || offset / 8u >= bits.size()) { return false; }
                        // [actual optimized expression bitmap] end
                        // [safe] checked local/byte index before writing retained
                        // DATA; neither Wasm pointer nor a published PC advances.
                        auto& octet{bits.index_unchecked(static_cast<::std::size_t>(offset / 8u))};
                        octet = static_cast<::std::uint_least8_t>(octet | (1u << (offset % 8u)));
                        return true;
                    })) { return {{}, preparation_status::native_failure}; }
                data->debug_shutdown_abi_revision_ = optimized_llvm_jit_debug_shutdown_ready(*module) ? 1u : 0u;
                if(count != 0u && data->debug_shutdown_abi_revision_ != 1u) { return {{}, preparation_status::lowering_declined}; }
                ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::declarations imports{};
                ::uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad::catch_runtime catches{};
                auto const native_eh_requested{module->getNamedValue(
                    ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::type_info_symbol) != nullptr};
                if constexpr(details::native_exception_host::available)
                {
                    if(::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::supports_itanium_dwarf_object(*target))
                    {
                        imports = ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::declare_itanium_dwarf_symbols(*module, *target);
                        catches = ::uwvm2::runtime::compiler::llvm_jit::native_exception_landingpad::declare_catch_runtime(*module);
                        if(imports.status != ::uwvm2::runtime::compiler::llvm_jit::native_exception_symbols::error::ok || !static_cast<bool>(catches))
                        { return {{}, preparation_status::native_failure}; }
                    }
                    else if(native_eh_requested) { return {{}, preparation_status::unsupported_native}; }
                }
                else if(native_eh_requested) { return {{}, preparation_status::unsupported_native}; }
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
                ::std::vector<details::pending_actual_native_endpoints::expected_function> endpoint_declarations{};
                if(!data->prepare_native_endpoint_declarations(*module,*context,endpoint_declarations))
                { return {{},preparation_status::native_endpoint_failure}; }
#endif
                // Stable host-function mappings are the same process ABI in
                // both worlds; owned mutable objects use only private pointers.
                ::uwvm2::runtime::compiler::shared::strict_float_jit::register_symbols();
                auto memory{::uwvm2::utils::container::make_delete_owned<
                    ::uwvm2::runtime::compiler::llvm_jit::details::runtime_llvm_jit_section_memory_manager>()};
                if(memory == nullptr) { return {{}, preparation_status::exhausted}; }
                auto* const memory_observer{memory.get()};
                auto* const engine{::llvm::EngineBuilder(llvm_module_owner_t{module.release()})
                    .setEngineKind(::llvm::EngineKind::JIT).setOptLevel(policy.codegen_opt_level)
                    .setMCPU(emit::get_llvm_string_ref(cpu)).setMAttrs(attribute_refs)
                    .setMCJITMemoryManager(llvm_jit_memory_manager_owner_t{memory.release()}).create(target.release())};
                if(engine == nullptr) { return {{}, preparation_status::native_failure}; }
                data->context_ = ::std::move(context); data->engine_.reset(engine);
                data->debug_cfi_manager_=memory_observer;
                if(imports.type_info != nullptr && !details::native_exception_host::bind<::uwvm2::runtime::exception::guest_exception>(
                    *data->engine_, imports, catches,
                    +[]() UWVM_THROWS { return ::uwvm2::runtime::exception::guest_exception{
                        ::uwvm2::runtime::exception::value::make(::std::make_shared<unsigned char const>(0), {})}; }
#if defined(UWVM_RUNTIME_NATIVE_EXCEPTION_HOST_GNU_CXX) && defined(UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION) && \
    UWVM_EXPERIMENTAL_EXCEPTION_SOURCE_RETENTION == 1
                    , &runtime_source_exception_end_catch
#endif
                    )) { return {{}, preparation_status::native_failure}; }
                // No object cache, old-code mapping, pending numeric plan,
                // diagnostic/permission registration or future epoch exists.
                data->ranges_ = ::uwvm2::utils::container::make_delete_owned<details::pending_llvm_jit_code_ranges>(*data->engine_, true);
                if(data->ranges_ == nullptr) { return {{}, preparation_status::exhausted}; }
                data->ranges_->configure_debug_full_capture(true);
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
                if(count!=0u)
                {
                    data->native_endpoints_.reset(new details::pending_actual_native_endpoints{
                        *data->engine_,data.get(),data->owner_,data->profile_,actual_module_id,
                        ::std::move(endpoint_declarations),native_budget_owner,native_charge});
                    if(!data->native_endpoints_->valid_ || !data->native_endpoints_->attached_)
                    { return {{},preparation_status::native_endpoint_failure}; }
                }
#endif
                data->engine_->finalizeObject();
                if(memory_observer->has_finalization_failure() || data->engine_->hasError() ||
                   data->ranges_->has_observation_failure())
                { return {{}, preparation_status::native_failure}; }
                auto resolve = [&](::uwvm2::utils::container::u8string const& name) -> ::std::uintptr_t
                {
                    // Real engine-owned LLVM definition BEFORE native lookup.
                    // Never accept a same-name process-global/old-world symbol
                    // for a missing declaration in this private generation.
                    auto* const function{data->engine_->FindFunctionNamed(emit::get_llvm_string_ref(name))};
                    if(function == nullptr || function->isDeclaration()) { return 0u; }
                    auto const callable{reinterpret_cast<::std::uintptr_t>(data->engine_->getPointerToFunction(function))};
                    if(callable == 0u) { return 0u; }
                    // This callable was obtained synchronously from this
                    // engine's actual defined LLVM Function, never wire DATA,
                    // an arbitrary integer or a process-global missing symbol.
                    // Only this real loader-owned callable may name an ELFv1
                    // entry/TOC/environment descriptor or carry a Thumb tag.
                    auto const code{details::native_function_code_address(callable)};
                    // [same private loaded object function entry/text extent]
                    // [safe] the actual code address must exactly match this
                    // engine listener's observed function and loaded text,
                    // before retaining any callable address as private DATA.
                    if(!data->ranges_->owns_pending_loaded_function_entry(code)) { return 0u; }
                    return callable;
                };
                for(::std::size_t index{}; index != count; ++index)
                {
                    auto const public_index{imported + index};
                    auto const typed_name{get_runtime_llvm_jit_wasm_function_name(runtime_module, public_index)};
                    auto& entries{data->entries_[index]};
                    entries.typed = resolve(typed_name);
                    entries.raw = resolve(get_runtime_llvm_jit_wasm_raw_function_name(runtime_module, public_index));
                    auto const resume_name{::uwvm2::utils::container::u8concat_uwvm(typed_name, u8".checkpoint.resume.v2")};
                    entries.resume_typed = resolve(resume_name);
                    entries.resume_raw = resolve(::uwvm2::utils::container::u8concat_uwvm(resume_name, u8".raw"));
                    if(entries.typed == 0u || entries.raw == 0u || entries.resume_typed == 0u || entries.resume_raw == 0u)
                    { return {{}, preparation_status::native_failure}; }
                }
                if(data->ranges_->has_observation_failure()) { return {{}, preparation_status::native_failure}; }
#if defined(UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2) && UWVM_EXPERIMENTAL_NATIVE_OWNER_TABLE_V2 == 1
                if(!data->freeze_native_endpoint_capture())
                { return {{},preparation_status::native_endpoint_failure}; }
#endif
                if(!data->owner_.matches_unpublished_file()) { return {}; }
                for(::std::size_t index{}; index != count; ++index)
                { data->typed_targets_[index] = data->entries_[index].typed; }
                // DATA stays exclusively in the returned private nonmoving
                // owner. No listener commit/provenance epoch/source validation
                // seal/global slot/publication/readiness call is made here.
                return {::std::move(data), preparation_status::ok};
            }
            catch(::fast_io::error const&) { return {{}, preparation_status::invalid_body}; }
            catch(::std::bad_alloc const&) { return {{}, preparation_status::exhausted}; }
            catch(...) { return {{}, preparation_status::native_failure}; }
        }
    public:
        ~runtime_checkpoint_staged_llvm_full_engine() = default;
    };
}
#endif

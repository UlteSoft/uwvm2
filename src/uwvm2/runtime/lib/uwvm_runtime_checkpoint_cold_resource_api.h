// Private implementation for actual host-only immutable-input diagnostics.
// Include inside the runtime namespace after actual capture/resource/manager
// definitions and real host admission API. No guest code/IR calls this leaf.
#pragma once
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
# if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_CPP_EXCEPTIONS)
    class llvm_jit_checkpoint_thread_capture final
    {
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_with_source_memory_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_source_binding_owner const&,
            void*, llvm_jit_debug_source_memory_callback) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_with_source_frame_memory_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_source_binding_owner const&,
            ::std::uint_least64_t, void*, llvm_jit_debug_source_memory_callback) noexcept -> bool;
        // The public type is incomplete. Only the genuine before-park API can
        // construct a wrapper; payload authority remains in its private producer.
        llvm_jit_checkpoint_thread_capture() = default;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_continuation_dispatcher;
        runtime_checkpoint_thread_capture::owner actual_{};
        inline static ::std::mutex registry_mutex_{};
        inline static ::std::weak_ptr<llvm_jit_checkpoint_thread_capture const> registry_[256u]{};
        // Qualified function friends nominate the real global-module APIs
        // first declared with C++ linkage in runtime.h. This class keeps its
        // runtime module attachment; no new friend overload is introduced.
        friend auto ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_thread_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&) noexcept -> llvm_jit_checkpoint_capture_result;
        friend auto ::uwvm2::runtime::lib::llvm_jit_checkpoint_query_resource_inputs_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>) noexcept -> llvm_jit_checkpoint_resource_observation;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_query_wasm_state_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            ::uwvm2::uwvm::debugger::wasm_state::request const&) noexcept -> ::uwvm2::uwvm::debugger::wasm_state::view;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_mutate_wasm_state_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            ::uwvm2::uwvm::debugger::wasm_mutation::request const&) noexcept -> ::uwvm2::uwvm::debugger::wasm_mutation::result;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_query_wasip1_state_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            ::uwvm2::uwvm::debugger::wasip1_state::request const&) noexcept -> ::uwvm2::uwvm::debugger::wasip1_state::view;
        friend auto ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_wasip1_environment_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            llvm_jit_wasip1_environment_capsule_request const&) noexcept -> llvm_jit_wasip1_environment_capsule_result;
        friend auto ::uwvm2::runtime::lib::llvm_jit_checkpoint_retire_saved_native_workers_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>, ::std::chrono::steady_clock::time_point) noexcept
            -> llvm_jit_checkpoint_native_retirement_result;
        friend auto ::uwvm2::runtime::lib::llvm_jit_checkpoint_prepare_retire_instance_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            llvm_jit_checkpoint_prepare_request const&, ::std::chrono::steady_clock::time_point) noexcept
            -> llvm_jit_checkpoint_native_retirement_result;
        friend auto ::uwvm2::runtime::lib::llvm_jit_checkpoint_retire_saved_execution_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>) noexcept -> llvm_jit_checkpoint_execution_retirement_status;
        friend auto ::uwvm2::runtime::lib::llvm_jit_checkpoint_prepare_instance_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            llvm_jit_checkpoint_prepare_request const&) noexcept -> llvm_jit_checkpoint_prepare_result;
        friend auto ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_instance_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            ::std::array<::std::byte, 16u> const&,
            ::uwvm2::uwvm::debugger::checkpoint::limits const&) noexcept -> llvm_jit_checkpoint_instance_capture_result;
        friend auto ::uwvm2::runtime::lib::llvm_jit_checkpoint_restore_wasip1_environment_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            llvm_jit_wasip1_environment_capsule_owner const&, llvm_jit_wasip1_environment_restore_request const&) noexcept
            -> llvm_jit_wasip1_environment_capsule_status;
        friend auto ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_instance_with_wasip1_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            ::std::array<::std::byte,16u> const&, ::uwvm2::uwvm::debugger::checkpoint::limits const&) noexcept
            -> llvm_jit_checkpoint_instance_capture_result;
        friend auto ::uwvm2::runtime::lib::llvm_jit_checkpoint_restore_wasip1_environment_group_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            ::std::span<llvm_jit_wasip1_environment_capsule_owner const>,bool) noexcept
            -> llvm_jit_wasip1_environment_capsule_status;
        friend auto ::uwvm2::runtime::lib::llvm_jit_checkpoint_restore_portable_wasip1_environment_group_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            ::std::span<llvm_jit_wasip1_environment_capsule_request const>) noexcept
            -> llvm_jit_wasip1_environment_capsule_result;
        friend auto ::uwvm2::runtime::lib::llvm_jit_checkpoint_capture_portable_wasip1_environment_group_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
            ::std::span<llvm_jit_wasip1_environment_capsule_request const>) noexcept
            -> llvm_jit_wasip1_portable_environment_group_result;
        [[nodiscard]] static llvm_jit_checkpoint_thread_capture_owner canonical(
            llvm_jit_checkpoint_thread_capture_owner const& supplied) noexcept
        {
            if(!supplied) { return {}; }
            ::std::lock_guard lock{registry_mutex_};
            for(auto const& weak : registry_)
            {
                auto actual{weak.lock()};
                // [private weak registry -> real owned object] [opaque request]
                // [safe] BOTH pointer and control block compared BEFORE any
                // supplied pointee access; alias ownership cannot mint a capture.
                if(actual && actual.get() == supplied.get() && !actual.owner_before(supplied) && !supplied.owner_before(actual))
                { return actual; }
            }
            return {};
        }
    public:
        llvm_jit_checkpoint_thread_capture(llvm_jit_checkpoint_thread_capture const&) = delete;
        llvm_jit_checkpoint_thread_capture& operator=(llvm_jit_checkpoint_thread_capture const&) = delete;
        ~llvm_jit_checkpoint_thread_capture() = default;
    };
    extern "C++" llvm_jit_checkpoint_capture_result llvm_jit_checkpoint_capture_thread_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket) noexcept
    {
        // The actual producer checks the live generated entry/depth/observer,
        // private episode, activation chain, exact sealed plan and typed DATA.
        // It does not expose a constructor or accept a caller's copied frame.
        auto captured{runtime_checkpoint_thread_capture::mint_current_before_park(ticket)};
        llvm_jit_checkpoint_capture_status status{llvm_jit_checkpoint_capture_status::not_selected};
        switch(captured.status)
        {
#define UWVM_CHECKPOINT_CAPTURE_STATUS(x) case checkpoint_thread_capture_status::x: status = llvm_jit_checkpoint_capture_status::x; break
            UWVM_CHECKPOINT_CAPTURE_STATUS(captured); UWVM_CHECKPOINT_CAPTURE_STATUS(not_selected);
            UWVM_CHECKPOINT_CAPTURE_STATUS(not_actual_before_park); UWVM_CHECKPOINT_CAPTURE_STATUS(stale_ticket);
            UWVM_CHECKPOINT_CAPTURE_STATUS(invalid_activation); UWVM_CHECKPOINT_CAPTURE_STATUS(incomplete_logical_frames);
            UWVM_CHECKPOINT_CAPTURE_STATUS(stale_code_generation); UWVM_CHECKPOINT_CAPTURE_STATUS(invalid_publication);
            UWVM_CHECKPOINT_CAPTURE_STATUS(invalid_typed_packet); UWVM_CHECKPOINT_CAPTURE_STATUS(unavailable_reference_roots);
            UWVM_CHECKPOINT_CAPTURE_STATUS(unavailable_exception_continuation); UWVM_CHECKPOINT_CAPTURE_STATUS(registry_exhausted);
            UWVM_CHECKPOINT_CAPTURE_STATUS(allocation_failed); UWVM_CHECKPOINT_CAPTURE_STATUS(non_replayable_import); UWVM_CHECKPOINT_CAPTURE_STATUS(activation_resource_exhausted);
#undef UWVM_CHECKPOINT_CAPTURE_STATUS
        }
        if(captured.status != checkpoint_thread_capture_status::captured || !captured.capture) { return {status, {}}; }
        try
        {
            ::std::shared_ptr<llvm_jit_checkpoint_thread_capture> wrapper{new llvm_jit_checkpoint_thread_capture};
            wrapper->actual_ = ::std::move(captured.capture);
            ::std::lock_guard lock{llvm_jit_checkpoint_thread_capture::registry_mutex_};
            for(auto& weak : llvm_jit_checkpoint_thread_capture::registry_)
            {
                if(weak.expired()) { weak = wrapper; return {llvm_jit_checkpoint_capture_status::captured, ::std::move(wrapper)}; }
            }
            return {llvm_jit_checkpoint_capture_status::registry_exhausted, {}};
        }
        catch(...) { return {llvm_jit_checkpoint_capture_status::allocation_failed, {}}; }
    }
    extern "C++" llvm_jit_checkpoint_resource_observation llvm_jit_checkpoint_query_resource_inputs_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied) noexcept
    {
        using manager = runtime_checkpoint_coherent_manager;
        using resources = runtime_checkpoint_resource_inputs;
        llvm_jit_checkpoint_resource_observation out{};
        if(!ticket || supplied.empty() || supplied.size() > 256u)
        { out.status = llvm_jit_checkpoint_query_status::incomplete_cohort; return out; }
        // [request-owned complete owner span] end; counts<=256 BEFORE indexing.
        // Canonical wrappers are independent copied DATA lifetimes. No runtime
        // publication/host-state field is read until the manager's actual lease.
        runtime_checkpoint_thread_capture::owner captures[256u]{};
        for(::std::size_t i{}; i != supplied.size(); ++i)
        {
            auto canonical{llvm_jit_checkpoint_thread_capture::canonical(supplied[i])};
            if(!canonical) { out.status = llvm_jit_checkpoint_query_status::invalid_capture_owner; return out; }
            // [privately canonical wrapper owner] no native executable authority
            // [safe] actual_ read only AFTER real registry get+control equality.
            captures[i] = canonical->actual_;
        }
        auto actual{manager::inspect_current(ticket, {captures, supplied.size()})};
        switch(actual.observation)
        {
#define UWVM_CHECKPOINT_QUERY_STATUS(x) case manager::status::x: out.status = llvm_jit_checkpoint_query_status::x; break
            UWVM_CHECKPOINT_QUERY_STATUS(coherent_typed_data); UWVM_CHECKPOINT_QUERY_STATUS(not_selected);
            UWVM_CHECKPOINT_QUERY_STATUS(invalid_management_entry); UWVM_CHECKPOINT_QUERY_STATUS(execution_admission_denied);
            UWVM_CHECKPOINT_QUERY_STATUS(execution_stopping); UWVM_CHECKPOINT_QUERY_STATUS(invalid_capture_owner);
            UWVM_CHECKPOINT_QUERY_STATUS(incomplete_cohort); UWVM_CHECKPOINT_QUERY_STATUS(wrong_control_or_profile);
            UWVM_CHECKPOINT_QUERY_STATUS(stale_episode); UWVM_CHECKPOINT_QUERY_STATUS(stale_location_or_generation);
            UWVM_CHECKPOINT_QUERY_STATUS(host_busy); UWVM_CHECKPOINT_QUERY_STATUS(host_untracked);
            UWVM_CHECKPOINT_QUERY_STATUS(host_admission_refused); UWVM_CHECKPOINT_QUERY_STATUS(invalid_current_publication);
            UWVM_CHECKPOINT_QUERY_STATUS(invalid_typed_data); UWVM_CHECKPOINT_QUERY_STATUS(budget_exceeded);
            UWVM_CHECKPOINT_QUERY_STATUS(allocation_failed); UWVM_CHECKPOINT_QUERY_STATUS(gc_admission_denied);
            UWVM_CHECKPOINT_QUERY_STATUS(invalid_gc_population);
#undef UWVM_CHECKPOINT_QUERY_STATUS
        }
        if(actual.observation != manager::status::coherent_typed_data) { return out; }
        try
        {
            // All below reads are detached owned DATA, not live VM resources.
            // Manager budgets bound <=256 threads and <=2^20 slots; the immutable
            // profile supplies the frame quota (zero means unlimited observation).
            llvm_jit_checkpoint_resource_observation candidate{}; candidate.status = out.status;
            candidate.observed_runtime_epoch = actual.observed_runtime_epoch;
            switch(actual.immutable_inputs.observation)
            {
#define UWVM_CHECKPOINT_RESOURCE_STATUS(x) case resources::status::x: candidate.resource_status = llvm_jit_checkpoint_resource_status::x; break
                UWVM_CHECKPOINT_RESOURCE_STATUS(immutable_inputs_only); UWVM_CHECKPOINT_RESOURCE_STATUS(invalid_management_scope);
                UWVM_CHECKPOINT_RESOURCE_STATUS(invalid_cohort); UWVM_CHECKPOINT_RESOURCE_STATUS(invalid_publication);
                UWVM_CHECKPOINT_RESOURCE_STATUS(unsupported_source_origin); UWVM_CHECKPOINT_RESOURCE_STATUS(unsupported_registry);
                UWVM_CHECKPOINT_RESOURCE_STATUS(unknown_import_census); UWVM_CHECKPOINT_RESOURCE_STATUS(stale_function_generation);
                UWVM_CHECKPOINT_RESOURCE_STATUS(invalid_segment_origin); UWVM_CHECKPOINT_RESOURCE_STATUS(quota_exceeded);
                UWVM_CHECKPOINT_RESOURCE_STATUS(allocation_failed);
#undef UWVM_CHECKPOINT_RESOURCE_STATUS
            }
            candidate.threads.reserve(actual.threads.size());
            for(auto const& thread : actual.threads)
            {
                llvm_jit_checkpoint_thread_data_observation data{}; data.participant = thread.participant;
                data.location = thread.observed_location; data.frame_count = thread.frames.size();
                for(auto const& frame : thread.frames)
                {
                    if(frame.values.size() > 1048576u - data.typed_slots)
                    { out.status = llvm_jit_checkpoint_query_status::budget_exceeded; return out; }
                    data.typed_slots += frame.values.size(); // bounded BEFORE advance
                    for(auto const& value : frame.values)
                    { if(value.declaration.initialized) { ++data.initialized_slots; } else { ++data.unavailable_slots; } }
                }
                candidate.threads.push_back(data);
            }
            candidate.modules.reserve(actual.immutable_inputs.modules.size());
            for(auto& module : actual.immutable_inputs.modules)
            {
                llvm_jit_checkpoint_module_input_copy data{}; data.module = module.module;
                data.declaration_counts = module.declaration_counts;
                data.original_module = ::std::move(module.original_module);
                data.data.reserve(module.data.size());
                for(auto const& segment : module.data)
                { data.data.push_back({segment.index, segment.source_offset, segment.byte_count, segment.dropped}); }
                candidate.modules.push_back(::std::move(data));
            }
            return candidate; // No source/code/domain/closed-admission pin escapes.
        }
        catch(...)
        { out = {}; out.status = llvm_jit_checkpoint_query_status::allocation_failed; return out; }
    }
    extern "C++" llvm_jit_checkpoint_execution_retirement_status llvm_jit_checkpoint_retire_saved_execution_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied) noexcept
    {
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        using outcome = llvm_jit_checkpoint_execution_retirement_status;
        if(mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only)
        { return outcome::requires_llvm_jit_full; }
        if(!ticket || supplied.empty() || supplied.size() > 256u)
        { return outcome::rejected_current_cohort; }
        runtime_checkpoint_thread_capture::owner captured[256u]{};
        for(::std::size_t index{}; index != supplied.size(); ++index)
        {
            // [request-owned wrapper span ... index<size<=256] end
            // [safe] real registry compares get AND control block BEFORE any
            // supplied pointee access; never cast a public capture-owner span.
            auto const wrapper{llvm_jit_checkpoint_thread_capture::canonical(supplied[index])};
            if(!wrapper) { return outcome::rejected_current_cohort; }
            captured[index] = wrapper->actual_; // Canonical retained wrapper only.
        }
        // Actual private capture registry is rechecked by the manager under
        // its real lease/ONE complete cohort; wrappers are not retire authority.
        return runtime_checkpoint_coherent_manager::retire_current(ticket, {captured, supplied.size()});
    }
    extern "C++" llvm_jit_wasip1_environment_capsule_status llvm_jit_checkpoint_restore_wasip1_environment_group_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied,
        ::std::span<llvm_jit_wasip1_environment_capsule_owner const> saved, bool strict) noexcept
    {
        using output=llvm_jit_wasip1_environment_capsule_status;
        if(!ticket || supplied.empty() || supplied.size()>256u || saved.empty() || saved.size()>16u)
        { return output::invalid_capsule_owner; }
        runtime_checkpoint_thread_capture::owner captured[256u]{};
        for(::std::size_t index{};index!=supplied.size();++index)
        {
            auto const actual{llvm_jit_checkpoint_thread_capture::canonical(supplied[index])};
            if(!actual) { return output::unavailable_capture; }
            captured[index]=actual->actual_;
        }
        return runtime_checkpoint_coherent_manager::restore_current_wasip1_environment_group(
            ticket,{captured,supplied.size()},saved,strict);
    }
    extern "C++" llvm_jit_wasip1_environment_capsule_result llvm_jit_checkpoint_restore_portable_wasip1_environment_group_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied,
        ::std::span<llvm_jit_wasip1_environment_capsule_request const> selected) noexcept
    {
        using status=llvm_jit_wasip1_environment_capsule_status;
        namespace mode=::uwvm2::uwvm::runtime::runtime_mode;
        if(mode::global_runtime_mode!=mode::runtime_mode_t::full_compile || mode::global_runtime_compiler!=mode::runtime_compiler_t::llvm_jit_only)
        { return {status::requires_llvm_jit_full,{}}; }
        if(!ticket || supplied.empty() || supplied.size()>256u)
        { return {status::unavailable_capture,{}}; }
        if(selected.empty() || selected.size()>16u)
        { return {status::invalid_portable_snapshot,{}}; }
        runtime_checkpoint_thread_capture::owner captured[256u]{};
        for(::std::size_t index{};index!=supplied.size();++index)
        {
            auto const actual{llvm_jit_checkpoint_thread_capture::canonical(supplied[index])};
            if(!actual) { return {status::unavailable_capture,{}}; }
            captured[index]=actual->actual_;
        }
        return runtime_checkpoint_coherent_manager::restore_current_portable_wasip1_environment_group(
            ticket,{captured,supplied.size()},selected);
    }
    extern "C++" llvm_jit_wasip1_portable_environment_group_result llvm_jit_checkpoint_capture_portable_wasip1_environment_group_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied,
        ::std::span<llvm_jit_wasip1_environment_capsule_request const> selected) noexcept
    {
        using status=llvm_jit_wasip1_environment_capsule_status;
        namespace mode=::uwvm2::uwvm::runtime::runtime_mode;
        if(mode::global_runtime_mode!=mode::runtime_mode_t::full_compile || mode::global_runtime_compiler!=mode::runtime_compiler_t::llvm_jit_only)
        { return {status::requires_llvm_jit_full,{}}; }
        if(!ticket || supplied.empty() || supplied.size()>256u)
        { return {status::unavailable_capture,{}}; }
        if(selected.empty() || selected.size()>16u)
        { return {status::invalid_portable_snapshot,{}}; }
        runtime_checkpoint_thread_capture::owner captured[256u]{};
        for(::std::size_t index{};index!=supplied.size();++index)
        {
            auto const actual{llvm_jit_checkpoint_thread_capture::canonical(supplied[index])};
            if(!actual) { return {status::unavailable_capture,{}}; }
            captured[index]=actual->actual_;
        }
        return runtime_checkpoint_coherent_manager::capture_current_portable_wasip1_environment_group(
            ticket,{captured,supplied.size()},selected);
    }
    extern "C++" llvm_jit_checkpoint_instance_capture_result llvm_jit_checkpoint_capture_instance_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied,
        ::std::array<::std::byte, 16u> const& label,
        ::uwvm2::uwvm::debugger::checkpoint::limits const& budget) noexcept
    {
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        using output_status = llvm_jit_checkpoint_instance_capture_status;
        llvm_jit_checkpoint_instance_capture_result out{};
        if(mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only)
        { out.status = output_status::requires_llvm_jit_full; return out; }
        runtime_checkpoint_coherent_manager::complete_instance_request requested{};
        requested.recording_id = label; requested.budget = budget;
        if(!ticket || supplied.empty() || supplied.size() > 256u)
        { out.status = output_status::unavailable_capture; return out; }
        runtime_checkpoint_thread_capture::owner captures[256u]{};
        for(::std::size_t index{}; index != supplied.size(); ++index)
        {
            // [request-owned wrapper span ... index<size<=256] end
            // [safe] canonical registry compares get AND control block BEFORE
            // reading supplied payload; no public/private span cast is allowed.
            auto const wrapper{llvm_jit_checkpoint_thread_capture::canonical(supplied[index])};
            if(!wrapper) { out.status = output_status::unavailable_capture; return out; }
            captures[index] = wrapper->actual_; // Only the real retained wrapper is read.
        }
        // The manager resolves the second private capture registry again under
        // its genuine lease/ONE complete cohort before any native field access.
        auto actual{runtime_checkpoint_coherent_manager::capture_complete_current_instance(
            ticket, {captures, supplied.size()}, requested)};
        out.data_error = actual.data_error;
        out.wasip1_checkpoint_required=actual.wasip1_checkpoint_required;
        out.wasip1_captured_together=actual.wasip1_captured_together;
        out.wasip1_environments=::std::move(actual.wasip1_environments);
        if(actual.captured)
        {
            // Zero-copy const graph alias retains the immutable logical graph
            // owner. It exposes copied DATA only, never native
            // carriers/source/engine/leases or a restore-issuer credential.
            out.graph = {actual.captured, ::std::addressof(actual.captured->graph())};
            out.status = output_status::captured;
        }
        else if(actual.data_error == ::uwvm2::uwvm::debugger::checkpoint::error::malformed)
        { out.status = output_status::invalid_recording_label; }
        else if(actual.observation == runtime_checkpoint_coherent_manager::status::allocation_failed)
        { out.status = output_status::allocation_failed; }
        else if(actual.observation != runtime_checkpoint_coherent_manager::status::not_selected)
        { out.status = output_status::unavailable_capture; }
        if(out.status==output_status::captured && out.wasip1_checkpoint_required)
        {
            // The structured reminder survives a failed diagnostic sink.
            try { ::fast_io::io::perrln("checkpoint reminder: capture WASIp1 together with Wasm at the same cooperative stop; Wasm-only snapshot omits FD/environment state."); }
            catch(...) {}
        }
        return out;
    }
    extern "C++" llvm_jit_checkpoint_instance_capture_result llvm_jit_checkpoint_capture_instance_with_wasip1_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const& ticket,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const> supplied,
        ::std::array<::std::byte, 16u> const& label,
        ::uwvm2::uwvm::debugger::checkpoint::limits const& budget) noexcept
    {
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        using output_status = llvm_jit_checkpoint_instance_capture_status;
        llvm_jit_checkpoint_instance_capture_result out{};
        if(mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only)
        { out.status = output_status::requires_llvm_jit_full; return out; }
        runtime_checkpoint_coherent_manager::complete_instance_request requested{};
        requested.recording_id = label; requested.budget = budget; requested.include_wasip1=true;
        if(!ticket || supplied.empty() || supplied.size() > 256u)
        { out.status = output_status::unavailable_capture; return out; }
        runtime_checkpoint_thread_capture::owner captures[256u]{};
        for(::std::size_t index{}; index != supplied.size(); ++index)
        {
            // [request-owned wrapper span ... index<size<=256] end
            // [safe] canonical registry compares get AND control block BEFORE
            // reading supplied payload; no public/private span cast is allowed.
            auto const wrapper{llvm_jit_checkpoint_thread_capture::canonical(supplied[index])};
            if(!wrapper) { out.status = output_status::unavailable_capture; return out; }
            captures[index] = wrapper->actual_; // Only the real retained wrapper is read.
        }
        // The manager resolves the second private capture registry again under
        // its genuine lease/ONE complete cohort before any native field access.
        auto actual{runtime_checkpoint_coherent_manager::capture_complete_current_instance(
            ticket, {captures, supplied.size()}, requested)};
        out.data_error = actual.data_error;
        out.wasip1_checkpoint_required=actual.wasip1_checkpoint_required;
        out.wasip1_captured_together=actual.wasip1_captured_together;
        out.wasip1_environments=::std::move(actual.wasip1_environments);
        if(actual.captured)
        {
            // Zero-copy const graph alias retains the immutable logical graph
            // owner. It exposes copied DATA only, never native
            // carriers/source/engine/leases or a restore-issuer credential.
            out.graph = {actual.captured, ::std::addressof(actual.captured->graph())};
            out.status = output_status::captured;
        }
        else if(actual.data_error == ::uwvm2::uwvm::debugger::checkpoint::error::malformed)
        { out.status = output_status::invalid_recording_label; }
        else if(actual.observation == runtime_checkpoint_coherent_manager::status::allocation_failed)
        { out.status = output_status::allocation_failed; }
        else if(actual.observation != runtime_checkpoint_coherent_manager::status::not_selected)
        { out.status = output_status::unavailable_capture; }
        return out;
    }
# else
    extern "C++" llvm_jit_wasip1_environment_capsule_result llvm_jit_checkpoint_restore_portable_wasip1_environment_group_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        ::std::span<llvm_jit_wasip1_environment_capsule_request const>) noexcept
    { return {llvm_jit_wasip1_environment_capsule_status::unavailable_capture,{}}; }
    extern "C++" llvm_jit_wasip1_portable_environment_group_result llvm_jit_checkpoint_capture_portable_wasip1_environment_group_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        ::std::span<llvm_jit_wasip1_environment_capsule_request const>) noexcept
    { return {llvm_jit_wasip1_environment_capsule_status::unavailable_capture,{}}; }
    extern "C++" llvm_jit_wasip1_environment_capsule_status llvm_jit_checkpoint_restore_wasip1_environment_group_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        ::std::span<llvm_jit_wasip1_environment_capsule_owner const>,bool) noexcept
    { return llvm_jit_wasip1_environment_capsule_status::unavailable_capture; }
    extern "C++" llvm_jit_checkpoint_capture_result llvm_jit_checkpoint_capture_thread_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&) noexcept { return {}; }
    extern "C++" llvm_jit_checkpoint_resource_observation llvm_jit_checkpoint_query_resource_inputs_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>) noexcept { return {}; }
extern "C++" llvm_jit_checkpoint_instance_capture_result llvm_jit_checkpoint_capture_instance_host_api(
    ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
    ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
    ::std::array<::std::byte, 16u> const&,
    ::uwvm2::uwvm::debugger::checkpoint::limits const&) noexcept
{
    llvm_jit_checkpoint_instance_capture_result out{};
#if defined(UWVM_RUNTIME_LLVM_JIT)
    out.status = llvm_jit_checkpoint_instance_capture_status::unavailable_capture;
#else
    out.status = llvm_jit_checkpoint_instance_capture_status::requires_llvm_jit_full;
#endif
    return out;
}
    extern "C++" llvm_jit_checkpoint_instance_capture_result llvm_jit_checkpoint_capture_instance_with_wasip1_host_api(
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
        ::std::span<llvm_jit_checkpoint_thread_capture_owner const>,
        ::std::array<::std::byte,16u> const&, ::uwvm2::uwvm::debugger::checkpoint::limits const&) noexcept
    { llvm_jit_checkpoint_instance_capture_result result{};result.status=llvm_jit_checkpoint_instance_capture_status::requires_llvm_jit_full;return result; }

# endif
#endif

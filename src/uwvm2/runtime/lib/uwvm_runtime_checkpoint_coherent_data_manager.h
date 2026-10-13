// Private production coherent checkpoint and WASIp1 manager. Included inside the real
// runtime namespace AFTER actual host bridge/API, private thread capture, full
// publications, current-generation helpers and publication guard definitions.
// Native resource operations require the authenticated current cohort, host
// closure, source/provider/profile and publication guards below.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
    extern "C++" { class runtime_checkpoint_world_transaction; }
    // Runtime module attachment follows the public opaque forward declaration.
    // This token contains no native entry, source, lease or frame state.
    class llvm_jit_checkpoint_native_retirement final
    {
        friend class runtime_checkpoint_coherent_manager;
        llvm_jit_checkpoint_native_retirement() = default;
    public:
        ~llvm_jit_checkpoint_native_retirement() = default;
        llvm_jit_checkpoint_native_retirement(llvm_jit_checkpoint_native_retirement const&) = delete;
        llvm_jit_checkpoint_native_retirement& operator=(llvm_jit_checkpoint_native_retirement const&) = delete;
    };
extern "C++"
{
    runtime_checkpoint_host_bridge::status runtime_checkpoint_host_bridge::try_close_coherent_state(
        owner const& supplied, gate_type::closed_admission& out) noexcept
    {
        // Sole private manager caller holds its REAL execution-generation lease.
        // Plain strong slot writers require maintenance/admission-quiescent
        // publication; no caller report or copied epoch substitutes for a lease.
        auto const installed{g_runtime.checkpoint_host_state};
        // [actual installed retained state] versus [comparison-only supplied owner]
        // [safe] compare both address AND control block BEFORE any supplied read.
        if(!installed || !supplied || installed.get() != supplied.get() ||
           installed.owner_before(supplied) || supplied.owner_before(installed) ||
           !g_runtime.checkpoint_host_enabled.load(::std::memory_order_acquire)) { return status::invalid_context; }
        auto const& actual{*installed}; // This real local pin owns the complete immutable state.
        if(actual.epoch_ != current_runtime_generation() || !actual.profile_ || !actual.control_ ||
           actual.profile_.get() != g_runtime.checkpoint_profile.get() ||
           actual.profile_.owner_before(g_runtime.checkpoint_profile) || g_runtime.checkpoint_profile.owner_before(actual.profile_) ||
           actual.control_.get() != g_runtime.debug_pause_control.get() ||
           actual.control_.owner_before(g_runtime.debug_pause_control) || g_runtime.debug_pause_control.owner_before(actual.control_))
        { return status::invalid_context; }
        // PRIVATE sole-friend caller already owns a genuine execution-domain
        // lease and ONE complete cooperative cohort guard. No caller bool or
        // public roster/status may invoke this private entry. Nonwaiting close:
        // active setup/provider/reentry and sticky escape decline, never drain.
        return actual.gate_->try_close(out);
    }
    bool runtime_checkpoint_host_bridge::current_coherent_state(
        owner const& supplied, gate_type::closed_admission const& closed) noexcept
    {
        // Sole private manager caller holds its REAL execution-generation lease.
        // Plain strong slot writers require maintenance/admission-quiescent
        // publication; no caller report or copied epoch substitutes for a lease.
        auto const installed{g_runtime.checkpoint_host_state};
        if(!installed || !supplied || installed.get() != supplied.get() ||
           installed.owner_before(supplied) || supplied.owner_before(installed) ||
           !g_runtime.checkpoint_host_enabled.load(::std::memory_order_acquire)) { return false; }
        auto const& actual{*installed}; // Never borrow a supplied alias as a retained state.
        return actual.epoch_ == current_runtime_generation() && actual.profile_ && actual.control_ &&
            actual.profile_.get() == g_runtime.checkpoint_profile.get() &&
            !actual.profile_.owner_before(g_runtime.checkpoint_profile) && !g_runtime.checkpoint_profile.owner_before(actual.profile_) &&
            actual.control_.get() == g_runtime.debug_pause_control.get() &&
            !actual.control_.owner_before(g_runtime.debug_pause_control) && !g_runtime.debug_pause_control.owner_before(actual.control_) &&
            actual.gate_->current(closed);
        // current() holds only its short gate mutex. No host/provider path holds
        // that mutex while entering publication; reset destroys after real drain.
    }

    class runtime_checkpoint_coherent_manager final
    {
        using capture = runtime_checkpoint_thread_capture;
        using domain = ::uwvm2::utils::thread::cooperative_pause_domain;
        using host_bridge = runtime_checkpoint_host_bridge;
        using source_owner = ::uwvm2::uwvm::runtime::full::full_source_instance::owner;
        using logical_frame = ::uwvm2::runtime::checkpoint::logical_frame;
        static constexpr ::std::size_t thread_limit{256u}, total_slot_limit{1048576u};
        runtime_checkpoint_coherent_manager() = delete;
        template<typename A, typename B>
        [[nodiscard]] static bool same_owner(A const& a, B const& b) noexcept
        { return a && b && a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a); }
        // PRIVATE readonly callback recipe. Only the host-only source reader
        // supplies this pointer from its canonical activation owner; it is not
        // a guest/file/public proof or a token returned after this transaction.
        struct source_memory_read_request
        {
            domain::pause_ticket const* actual_source_ticket{};
            void* context{};
            bool (*copy)(void*, ::std::span<domain::stopped_participant const>) noexcept{};
        };
        struct environment_group_restore_request
        {
            ::std::span<llvm_jit_wasip1_environment_capsule_owner const> saved{};
            bool strict{};
            ::std::span<llvm_jit_wasip1_environment_capsule_request const> portable{};
            llvm_jit_wasip1_environment_capsule_result portable_result{};
            bool capture_portable{};
            llvm_jit_wasip1_portable_environment_group_result captured_portable{};
            llvm_jit_wasip1_environment_capsule_status outcome{llvm_jit_wasip1_environment_capsule_status::unavailable_capture};
        };
    public:
        enum class status : unsigned char
        {
            coherent_typed_data, not_selected, invalid_management_entry, execution_admission_denied, execution_stopping,
            invalid_capture_owner, incomplete_cohort, wrong_control_or_profile, stale_episode,
            stale_location_or_generation, host_busy, host_untracked, host_admission_refused,
            invalid_current_publication, invalid_typed_data, budget_exceeded, allocation_failed, gc_admission_denied, invalid_gc_population
        };
        enum class resource_census : unsigned char { unavailable_complete_instance_census };
        struct thread_data
        {
            ::std::uint_least64_t participant{};
            ::uwvm2::utils::thread::cooperative_pause_location observed_location{};
            ::std::vector<logical_frame> frames{};
        };
        struct result
        {
            status observation{status::not_selected};
            resource_census resources{resource_census::unavailable_complete_instance_census};
            ::std::uint_least64_t observed_runtime_epoch{};
            ::std::vector<thread_data> threads{};
            runtime_checkpoint_resource_inputs::result immutable_inputs{};
            // Lifetime DATA only: these pins do not seal source.file() backing,
            // grant guest-FD exclusion or retain current native executable rights.
            ::std::vector<source_owner> source_pins{};
            [[nodiscard]] constexpr bool snapshot_or_restore_authority() const noexcept { return false; }
        };
        // This immutable graph is copied DATA. It never replaces a new
        // resource-world issuer, actual engine/root publication or a real lease.
        class complete_instance_capture final
        {
            friend class runtime_checkpoint_coherent_manager;
            ::uwvm2::uwvm::debugger::checkpoint::state graph_{};
            ::std::uint_least64_t epoch_{};
            // All source bytes, effective function bodies, declarations and
            // logical values are copied into graph_. Native source/store/plan/
            // capture pins remain ONLY in the real lexical census work. A saved
            // graph must not keep old cohort registrations/native worlds alive.
            complete_instance_capture() = default;
        public:
            using owner = ::std::shared_ptr<complete_instance_capture const>;
            complete_instance_capture(complete_instance_capture const&) = delete;
            complete_instance_capture& operator=(complete_instance_capture const&) = delete;
            ~complete_instance_capture() = default;
            [[nodiscard]] auto const& graph() const noexcept { return graph_; }
            [[nodiscard]] ::std::uint_least64_t observed_epoch() const noexcept { return epoch_; }
        };
        struct complete_instance_request
        {
            // Host-chosen recording label only, never runtime identity or
            // authority. This snapshot begins a new recording origin: checkpoint
            // 1, no parent, zero subsequently recorded instructions/events.
            ::std::array<::std::byte, 16u> recording_id{};
            ::uwvm2::uwvm::debugger::checkpoint::limits budget{};
            bool include_wasip1{};
        };
        struct complete_instance_result
        {
            status observation{status::not_selected};
            ::uwvm2::uwvm::debugger::checkpoint::error data_error{
                ::uwvm2::uwvm::debugger::checkpoint::error::unavailable_capability};
            complete_instance_capture::owner captured{};
            ::std::vector<llvm_jit_wasip1_environment_capsule_owner> wasip1_environments{};
            bool wasip1_checkpoint_required{}, wasip1_captured_together{};
        };
        [[nodiscard]] static complete_instance_result capture_complete_current_instance(
            domain::pause_ticket const& ticket, ::std::span<capture::owner const> supplied,
            complete_instance_request const& requested) noexcept
        {
            complete_instance_result copied{};
            bool label{}; for(auto byte : requested.recording_id) { label = label || byte != ::std::byte{}; }
            if(!label) { copied.data_error = ::uwvm2::uwvm::debugger::checkpoint::error::malformed; return copied; }
            auto const checked{inspect_current_impl(ticket, supplied, nullptr, nullptr, nullptr, nullptr, nullptr,
                ::std::addressof(requested), ::std::addressof(copied))};
            copied.observation = checked.observation;
            if(checked.observation != status::coherent_typed_data)
            { copied.captured.reset();copied.wasip1_environments.clear();copied.wasip1_captured_together=false; }
            return copied;
        }

        // Single synchronous read in the SAME complete actual-cohort/closed
        // host/N/publication proof used by VM state observation. The recipe's
        // callback returns only detached bytes; it cannot retain any proof.
        [[nodiscard]] static result with_current_source_memory(
            domain::pause_ticket const& ticket, ::std::span<capture::owner const> supplied,
            domain::pause_ticket const& actual_source_ticket, void* context,
            bool (*copy)(void*, ::std::span<domain::stopped_participant const>) noexcept) noexcept
        {
            source_memory_read_request const request{::std::addressof(actual_source_ticket), context, copy};
            return inspect_current_impl(ticket, supplied, nullptr, nullptr, nullptr, nullptr,
                nullptr, nullptr, nullptr, nullptr, nullptr, ::std::addressof(request));
        }

        // Internal management DATA query only. These owner values are resolved
        // through the private producer registry before field access. There is no
        // guest/native command exposing this method and no consumer accepts its
        // result, boolean, source pin, epoch or payload as a permission token.
        [[nodiscard]] static result inspect_current(domain::pause_ticket const& ticket,
            ::std::span<capture::owner const> supplied) noexcept
        { return inspect_current_impl(ticket, supplied, nullptr, nullptr, nullptr, nullptr); }

        [[nodiscard]] static ::uwvm2::uwvm::debugger::wasm_state::view query_current_wasm_state(
            domain::pause_ticket const& ticket, ::std::span<capture::owner const> supplied,
            ::uwvm2::uwvm::debugger::wasm_state::request const& requested) noexcept
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
            ws::view copied{}; copied.requested = ws::unavailable_request(requested);
            if(!ws::valid(requested)) { copied.result = ws::status::invalid_selection; return copied; }
            auto const checked{inspect_current_impl(ticket, supplied, ::std::addressof(requested), ::std::addressof(copied), nullptr, nullptr)};
            if(checked.observation == status::coherent_typed_data) { return copied; }
            // Proof failure returns an empty detached view. No partially copied
            // carrier/GC object or old-episode native address can escape.
            copied = {}; copied.requested = ws::unavailable_request(requested);
            switch(checked.observation)
            {
                case status::not_selected: copied.result = ws::status::not_selected; break;
                case status::incomplete_cohort: copied.result = ws::status::incomplete_cohort; break;
                case status::invalid_capture_owner: copied.result = ws::status::unavailable_activation; break;
                case status::stale_episode: case status::stale_location_or_generation:
                case status::execution_stopping: case status::invalid_current_publication:
                    copied.result = ws::status::stale_stop_or_generation; break;
                case status::host_busy: case status::host_untracked: case status::host_admission_refused:
                    copied.result = ws::status::unavailable_foreign_host_state; break;
                case status::gc_admission_denied: case status::invalid_gc_population:
                    copied.result = ws::status::unavailable_gc_roots; break;
                case status::invalid_typed_data: copied.result = ws::status::unavailable_typed_site; break;
                case status::budget_exceeded: copied.result = ws::status::resource_limit; break;
                case status::allocation_failed: copied.result = ws::status::allocation_failed; break;
                default: copied.result = ws::status::requires_current_cooperative_stop; break;
            }
            return copied;
        }
        [[nodiscard]] static ::uwvm2::uwvm::debugger::wasm_mutation::result mutate_current_wasm_state(
            domain::pause_ticket const& ticket, ::std::span<capture::owner const> supplied,
            ::uwvm2::uwvm::debugger::wasm_mutation::request const& requested) noexcept
        {
            namespace wm = ::uwvm2::uwvm::debugger::wasm_mutation;
            namespace ws = ::uwvm2::uwvm::debugger::wasm_state;
            wm::result copied{};copied.target=requested.target;copied.module=requested.module;copied.index=requested.index;copied.element=requested.element;
            if(!wm::valid(requested)) { copied.status=ws::status::invalid_selection;return copied; }
            auto const checked{inspect_current_impl(ticket,supplied,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,
                ::std::addressof(requested),::std::addressof(copied))};
            if(checked.observation==status::coherent_typed_data || copied.applied) { return copied; }
            switch(checked.observation)
            {
                case status::not_selected:copied.status=ws::status::not_selected;break;
                case status::incomplete_cohort:copied.status=ws::status::incomplete_cohort;break;
                case status::invalid_capture_owner:copied.status=ws::status::unavailable_activation;break;
                case status::stale_episode:case status::stale_location_or_generation:case status::execution_stopping:
                case status::invalid_current_publication:copied.status=ws::status::stale_stop_or_generation;break;
                case status::host_busy:case status::host_untracked:case status::host_admission_refused:
                    copied.status=ws::status::unavailable_foreign_host_state;break;
                case status::gc_admission_denied:case status::invalid_gc_population:copied.status=ws::status::unavailable_gc_roots;break;
                case status::invalid_typed_data:copied.status=ws::status::unavailable_typed_site;break;
                case status::budget_exceeded:copied.status=ws::status::resource_limit;break;
                case status::allocation_failed:copied.status=ws::status::allocation_failed;break;
                default:copied.status=ws::status::requires_current_cooperative_stop;break;
            }
            return copied;
        }
        [[nodiscard]] static ::uwvm2::uwvm::debugger::wasip1_state::view query_current_wasip1_state(
            domain::pause_ticket const& ticket, ::std::span<capture::owner const> supplied,
            ::uwvm2::uwvm::debugger::wasip1_state::request const& selected) noexcept
        {
            namespace ws = ::uwvm2::uwvm::debugger::wasip1_state;
            ws::view copied{}; copied.module = selected.module; copied.operation = selected.operation;
            if(!ws::valid(selected)) { copied.result = ws::status::invalid_request; return copied; }
            auto const checked{inspect_current_impl(ticket, supplied, nullptr, nullptr, ::std::addressof(selected), ::std::addressof(copied))};
            if(checked.observation == status::coherent_typed_data) { return copied; }
            copied = {}; copied.module = selected.module; copied.operation = selected.operation;
            switch(checked.observation)
            {
                case status::not_selected: copied.result = ws::status::not_selected; break;
                case status::incomplete_cohort: case status::invalid_capture_owner: copied.result = ws::status::incomplete_cohort; break;
                case status::stale_episode: case status::stale_location_or_generation: case status::execution_stopping:
                case status::invalid_current_publication: copied.result = ws::status::stale_stop_or_generation; break;
                case status::host_busy: case status::host_untracked: case status::host_admission_refused:
                    copied.result = ws::status::unavailable_foreign_host_state; break;
                case status::budget_exceeded: copied.result = ws::status::resource_limit; break;
                case status::allocation_failed: copied.result = ws::status::allocation_failed; break;
                default: copied.result = ws::status::requires_current_cooperative_stop; break;
            }
            return copied;
        }
        [[nodiscard]] static llvm_jit_wasip1_environment_capsule_result capture_current_wasip1_environment(
            domain::pause_ticket const& ticket,::std::span<capture::owner const> supplied,
            llvm_jit_wasip1_environment_capsule_request const& selected) noexcept
        {
            llvm_jit_wasip1_environment_capsule_result copied{};
            auto const checked{inspect_current_impl(ticket,supplied,nullptr,nullptr,nullptr,nullptr,nullptr,
                nullptr,nullptr,nullptr,nullptr,nullptr,::std::addressof(selected),::std::addressof(copied))};
            if(checked.observation==status::coherent_typed_data) { return copied; }
            copied.capsule.reset();copied.portable.reset();
            using output=llvm_jit_wasip1_environment_capsule_status;
            copied.status=checked.observation==status::allocation_failed ? output::allocation_failed :
                checked.observation==status::budget_exceeded ? output::resource_limit :
                checked.observation==status::not_selected ? output::not_selected : output::unavailable_capture;
            return copied;
        }
        [[nodiscard]] static llvm_jit_wasip1_environment_capsule_status restore_current_wasip1_environment(
            domain::pause_ticket const& ticket, ::std::span<capture::owner const> supplied,
            llvm_jit_wasip1_environment_capsule_owner const& saved, llvm_jit_wasip1_environment_restore_request const& selected) noexcept
        {
            auto const canonical{llvm_jit_wasip1_environment_capsule::canonical(saved)};
            if(!canonical) { return llvm_jit_wasip1_environment_capsule_status::invalid_capsule_owner; }
            llvm_jit_wasip1_environment_capsule_request request{};request.module=selected.module;
            request.recording_label=canonical->data_.recording_label;
            llvm_jit_wasip1_environment_capsule_result result{};
            auto const checked{inspect_current_impl(ticket,supplied,nullptr,nullptr,nullptr,nullptr,nullptr,
                nullptr,nullptr,nullptr,nullptr,nullptr,::std::addressof(request),::std::addressof(result),
                ::std::addressof(canonical),selected.require_managed_resources)};
            if(checked.observation==status::coherent_typed_data) { return result.status; }
            return checked.observation==status::allocation_failed ? llvm_jit_wasip1_environment_capsule_status::allocation_failed :
                llvm_jit_wasip1_environment_capsule_status::unavailable_capture;
        }
        [[nodiscard]] static llvm_jit_wasip1_portable_environment_group_result capture_current_portable_wasip1_environment_group(
            domain::pause_ticket const& ticket,::std::span<capture::owner const> supplied,
            ::std::span<llvm_jit_wasip1_environment_capsule_request const> selected) noexcept
        {
            environment_group_restore_request request{};request.portable=selected;request.capture_portable=true;
            auto const checked{inspect_current_impl(ticket,supplied,nullptr,nullptr,nullptr,nullptr,
                nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,false,::std::addressof(request))};
            if(checked.observation==status::coherent_typed_data) { return ::std::move(request.captured_portable); }
            request.captured_portable.status=checked.observation==status::allocation_failed ? llvm_jit_wasip1_environment_capsule_status::allocation_failed :
                llvm_jit_wasip1_environment_capsule_status::unavailable_capture;
            request.captured_portable.portable.environments.clear();
            return ::std::move(request.captured_portable);
        }
        [[nodiscard]] static llvm_jit_wasip1_environment_capsule_result restore_current_portable_wasip1_environment_group(
            domain::pause_ticket const& ticket,::std::span<capture::owner const> supplied,
            ::std::span<llvm_jit_wasip1_environment_capsule_request const> selected) noexcept
        {
            environment_group_restore_request request{};request.portable=selected;
            auto const checked{inspect_current_impl(ticket,supplied,nullptr,nullptr,nullptr,nullptr,
                nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,false,::std::addressof(request))};
            if(checked.observation==status::coherent_typed_data) { return ::std::move(request.portable_result); }
            request.portable_result.status=checked.observation==status::allocation_failed ? llvm_jit_wasip1_environment_capsule_status::allocation_failed :
                llvm_jit_wasip1_environment_capsule_status::unavailable_capture;
            return ::std::move(request.portable_result);
        }
        [[nodiscard]] static llvm_jit_wasip1_environment_capsule_status restore_current_wasip1_environment_group(
            domain::pause_ticket const& ticket,::std::span<capture::owner const> supplied,
            ::std::span<llvm_jit_wasip1_environment_capsule_owner const> saved,bool strict) noexcept
        {
            environment_group_restore_request request{saved,strict};
            auto const checked{inspect_current_impl(ticket,supplied,nullptr,nullptr,nullptr,nullptr,
                nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,false,::std::addressof(request))};
            if(checked.observation==status::coherent_typed_data) { return request.outcome; }
            return checked.observation==status::allocation_failed ? llvm_jit_wasip1_environment_capsule_status::allocation_failed :
                llvm_jit_wasip1_environment_capsule_status::unavailable_capture;
        }
    private:
        using retirement = runtime_checkpoint_retirement_bridge;
        struct native_retirement_state;
        struct retirement_handoff
        {
            retirement::owner request{};
            native_retirement_state* native{}; // manager-owned state only
            // Declaration order releases the actual pause hold before the
            // retained control owner in request. No borrowed domain outlives it.
            domain::paused_transition transition{};
            llvm_jit_checkpoint_execution_retirement_status outcome{
                llvm_jit_checkpoint_execution_retirement_status::rejected_current_cohort};
        };
    public:
        [[nodiscard]] static llvm_jit_checkpoint_execution_retirement_status retire_current(
            domain::pause_ticket const& ticket, ::std::span<capture::owner const> supplied) noexcept
        {
            namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
            if(mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
               mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only)
            { return llvm_jit_checkpoint_execution_retirement_status::requires_llvm_jit_full; }
            retirement_handoff handoff{};
            auto const checked{inspect_current_impl(ticket, supplied, nullptr, nullptr, nullptr, nullptr, ::std::addressof(handoff))};
            if(checked.observation == status::not_selected)
            { return llvm_jit_checkpoint_execution_retirement_status::not_selected; }
            return handoff.outcome;
        }
        [[nodiscard]] static llvm_jit_checkpoint_prepare_result prepare_current_instance(
            domain::pause_ticket const& ticket,::std::span<capture::owner const> supplied,
            llvm_jit_checkpoint_prepare_request const& request) noexcept
        {
            llvm_jit_checkpoint_prepare_result prepared{};bool label{};
            for(auto byte:request.recording_label) { label=label || byte!=::std::byte{}; }
            if(!label || request.maximum_native_payload_bytes==0u || request.maximum_original_source_bytes==0u)
            { prepared.status=llvm_jit_checkpoint_prepare_status::invalid_request;return prepared; }
            complete_instance_request census{};census.recording_id=request.recording_label;census.budget=request.graph_budget;
            census.include_wasip1=request.include_wasip1;
            complete_instance_result copied{};
            auto const checked{inspect_current_impl(ticket,supplied,nullptr,nullptr,nullptr,nullptr,nullptr,
                ::std::addressof(census),::std::addressof(copied),nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,false,nullptr,
                ::std::addressof(request),::std::addressof(prepared))};
            prepared.data_error=copied.data_error;
            if(checked.observation!=status::coherent_typed_data)
            {
                prepared={};prepared.data_error=copied.data_error;
                prepared.status=checked.observation==status::allocation_failed ? llvm_jit_checkpoint_prepare_status::allocation_failed :
                    checked.observation==status::not_selected ? llvm_jit_checkpoint_prepare_status::not_selected :
                    llvm_jit_checkpoint_prepare_status::unavailable_capture;
            }
            return prepared;
        }
    private:
 #include "uwvm_runtime_checkpoint_native_retirement_state.h"
        // Defined after the private world class. ONLY called inside this
        // manager's real lease/cohort/closed-host/N/publication scope.
        static void prepare_and_discard_actual_world(
            ::uwvm2::uwvm::debugger::checkpoint::state const&,source_owner const&,
            ::std::uint_least64_t,::uwvm2::runtime::checkpoint::compilation_profile::owner const&,
            llvm_jit_checkpoint_prepare_request const&,llvm_jit_checkpoint_prepare_result&,
            ::std::span<llvm_jit_wasip1_environment_capsule_owner const>,
            ::std::shared_ptr<runtime_checkpoint_world_transaction>* retained=nullptr) noexcept;
        [[nodiscard]] static result inspect_current_impl(domain::pause_ticket const& ticket,
            ::std::span<capture::owner const> supplied,
            ::uwvm2::uwvm::debugger::wasm_state::request const* requested,
            ::uwvm2::uwvm::debugger::wasm_state::view* observation,
            ::uwvm2::uwvm::debugger::wasip1_state::request const* wasip1_requested,
            ::uwvm2::uwvm::debugger::wasip1_state::view* wasip1_observation,
            retirement_handoff* retiring = nullptr,
            complete_instance_request const* complete_requested = nullptr,
            complete_instance_result* complete_observation = nullptr,
            ::uwvm2::uwvm::debugger::wasm_mutation::request const* mutating = nullptr,
            ::uwvm2::uwvm::debugger::wasm_mutation::result* mutated = nullptr,
            source_memory_read_request const* source_read = nullptr,
            llvm_jit_wasip1_environment_capsule_request const* capsule_requested = nullptr,
            llvm_jit_wasip1_environment_capsule_result* capsule_observation = nullptr,
            llvm_jit_wasip1_environment_capsule_owner const* capsule_restore = nullptr, bool capsule_strict = false,
            environment_group_restore_request* group_restore = nullptr,
            llvm_jit_checkpoint_prepare_request const* preparing = nullptr,
            llvm_jit_checkpoint_prepare_result* prepared = nullptr) noexcept
        {
            namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
            result out{};
            if(mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
               mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only ||
               !g_runtime.checkpoint_host_enabled.load(::std::memory_order_acquire)) { return out; }
            if(!details::runtime_execution_entry_reset_allowed(get_runtime_execution_entry_depth()) ||
               !details::runtime_state_publication_access_allowed(get_runtime_state_publication_depth()) ||
               !details::runtime_compilation_metadata_callback_access_allowed(get_runtime_compilation_metadata_callback_depth()) ||
               get_llvm_jit_generated_wasm_bridge_entry_depth() != 0u || g_debug_observer_active)
            { out.observation = status::invalid_management_entry; return out; }
            if((preparing==nullptr)!=(prepared==nullptr) ||
               (preparing!=nullptr && (complete_requested==nullptr || complete_observation==nullptr ||
                requested!=nullptr || observation!=nullptr || wasip1_requested!=nullptr || wasip1_observation!=nullptr ||
                (retiring!=nullptr && !(retiring->native!=nullptr && retiring->native->prepare_candidate &&
                    preparing==::std::addressof(retiring->native->preparation_request) &&
                    prepared==::std::addressof(retiring->native->result.preparation))) ||
                mutating!=nullptr || mutated!=nullptr || source_read!=nullptr ||
                capsule_requested!=nullptr || capsule_observation!=nullptr || capsule_restore!=nullptr || capsule_strict || group_restore!=nullptr)))
            { out.observation=status::invalid_management_entry;return out; }
            if(group_restore!=nullptr && (requested!=nullptr || observation!=nullptr || wasip1_requested!=nullptr ||
                wasip1_observation!=nullptr || retiring!=nullptr || complete_requested!=nullptr || complete_observation!=nullptr ||
                mutating!=nullptr || mutated!=nullptr || source_read!=nullptr || capsule_requested!=nullptr ||
                capsule_observation!=nullptr || capsule_restore!=nullptr || capsule_strict))
            { out.observation=status::invalid_management_entry;return out; }
            if((capsule_requested==nullptr)!=(capsule_observation==nullptr) ||
               (capsule_requested!=nullptr && (requested!=nullptr || observation!=nullptr || wasip1_requested!=nullptr ||
                wasip1_observation!=nullptr || retiring!=nullptr || complete_requested!=nullptr || complete_observation!=nullptr ||
                mutating!=nullptr || mutated!=nullptr || source_read!=nullptr)))
            { out.observation=status::invalid_management_entry;return out; }
            if(source_read != nullptr && (source_read->actual_source_ticket == nullptr || source_read->copy == nullptr ||
                requested != nullptr || observation != nullptr || wasip1_requested != nullptr || wasip1_observation != nullptr ||
                retiring != nullptr || complete_requested != nullptr || complete_observation != nullptr || mutating != nullptr || mutated != nullptr))
            { out.observation = status::invalid_management_entry; return out; }
            if(!ticket || supplied.empty() || supplied.size() > thread_limit)
            { out.observation = status::incomplete_cohort; return out; }
            try
            {
                // Management takes a REAL generation lease directly. Do not
                // instantiate runtime_execution_entry_scope: its counted setup
                // would make this manager refuse itself, and waiting for parked
                // guest leases to leave would deadlock/cancel the live instance.
                ::uwvm2::utils::thread::execution_domain::maintenance_transition maintenance{};
                if(retiring != nullptr)
                {
                    // Actual cold mutex ownership, not caller depth/epoch. It
                    // precedes admission and remains through real drain; reset
                    // cannot steal the lease.reset -> drain interval.
                    maintenance = g_runtime.execution_domain.try_maintenance_transition();
                    if(!maintenance) { out.observation = status::execution_admission_denied; return out; }
                    if(retiring->native != nullptr)
                    {
                        retiring->native->generation = g_runtime.execution_domain.prepare_replacement_generation(maintenance);
                        if(!retiring->native->generation) { out.observation = status::allocation_failed; return out; }
                    }
                }
                auto lease{g_runtime.execution_domain.try_enter()};
                if(!lease) { out.observation = status::execution_admission_denied; return out; }
                // The admitted generation remains alive, but a concurrent trusted
                // stop can already forbid positive current-generation DATA.
                // stop_requested() is this REAL lease's acquire atomic read.
                if(lease.stop_requested()) { out.observation = status::execution_stopping; return out; }
                auto const epoch{current_runtime_generation()};
                // Copy only AFTER the actual lease admitted this generation. The
                // cold configure writer excludes all active generation leases.
                auto const configuration{g_runtime.checkpoint_host_state};
                if(!configuration || configuration->epoch_ != epoch || !configuration->control_ || !configuration->profile_ ||
                   !same_owner(configuration->control_, g_runtime.debug_pause_control) ||
                   !same_owner(configuration->profile_, g_runtime.checkpoint_profile))
                { out.observation = status::wrong_control_or_profile; return out; }
                capture::owner canonical[thread_limit]{};
                for(::std::size_t i{}; i != supplied.size(); ++i)
                {
                    // [request-owned owner span ... i<supplied.size()<=256] end
                    // [safe] read only the shared_ptr object, then private-registry
                    // compare get+control block before ANY supplied pointee read.
                    canonical[i] = capture::canonical(supplied[i]);
                    if(!canonical[i]) { out.observation = status::invalid_capture_owner; return out; }
                    for(::std::size_t j{}; j != i; ++j)
                    {
                        // [fully canonical fixed owner cells 0..i] no raw payload
                        // [safe] j<i<=256; both owners already registry-qualified.
                        if(same_owner(canonical[j], canonical[i]) || canonical[j]->participant_ == canonical[i]->participant_)
                        { out.observation = status::incomplete_cohort; return out; }
                    }
                }
                domain::stopped_participant scratch[thread_limit]{};
                result candidate{}; bool coherent{};
                // One lexical proof body serves DATA and actual retirement.
                // Retirement uses the indivisible transfer API below; never a
                // successful query followed by a second pause authentication.
                auto const inspect{[&](::std::span<domain::stopped_participant const> actual_slots, auto const& current_ticket)
                {
                    // ONE real domain mutex owns both actual location and every
                    // before-park capture episode. Never separately authenticate
                    // captures and later join them by a same PC/serial/depth.
                    if(actual_slots.size() != supplied.size()) { out.observation = status::incomplete_cohort; return; }
                    for(::std::size_t i{}; i != supplied.size(); ++i)
                    {
                        auto const& actual{*canonical[i]}; // Private canonical owner remains pinned throughout lease.
                        if(!same_owner(actual.control_, configuration->control_) ||
                           !same_owner(actual.profile_, configuration->profile_))
                        { out.observation = status::wrong_control_or_profile; return; }
                        if(!current_ticket(actual.ticket_)) { out.observation = status::stale_episode; return; }
                        // In-flight waits retain queue/deadline state that a
                        // before-opcode packet cannot restore. Live Wasm
                        // reads/writes are safe; neither world census
                        // or native retirement may treat it as replayable code.
                        if(actual.suspended_wait_ && (retiring != nullptr || complete_requested != nullptr ||
                           wasip1_requested != nullptr || capsule_requested != nullptr ||
                           (requested == nullptr && source_read == nullptr && mutating == nullptr)))
                        { out.observation = status::invalid_typed_data; return; }
                        bool found{};
                        for(auto const& slot : actual_slots)
                        {
                            if(slot.id != actual.participant_) { continue; }
                            if(slot.location != actual.location_ || slot.location.code_generation != epoch)
                            { out.observation = status::stale_location_or_generation; return; }
                            found = true; break;
                        }
                        if(!found || actual.frames_.empty())
                        { out.observation = status::incomplete_cohort; return; }
                    }
                    // The selected source activation's OWN private ticket must
                    // be current under THIS exact mutex. Equal PCs/incarnations
                    // from a former episode cannot be stitched into the reader.
                    if(source_read != nullptr && !current_ticket(*source_read->actual_source_ticket))
                    { out.observation = status::stale_episode; return; }
                    ::uwvm2::utils::thread::checkpoint_host_admission::closed_admission closed{};
                    auto const close_status{host_bridge::try_close_coherent_state(configuration, closed)};
                    if(close_status != host_bridge::status::ok)
                    {
                        out.observation = close_status == host_bridge::status::busy ? status::host_busy :
                            close_status == host_bridge::status::untracked_host ? status::host_untracked : status::host_admission_refused;
                        return;
                    }
                    // Actual lease pointers are borrowed ONLY after canonical
                    // capture/control/participant and SAME current ticket checks
                    // above, while every genuine participant is still parked.
                    using gc_admission = ::uwvm2::runtime::gc::managed_entry_admission;
                    gc_admission::shared_lease const* actual_gc_leases[thread_limit]{};
                    for(::std::size_t i{}; i != supplied.size(); ++i)
                    {
                        auto const& actual{*canonical[i]};
                        if(actual.actual_gc_lease_ == nullptr)
                        { out.observation = status::gc_admission_denied; return; }
                        actual_gc_leases[i] = actual.actual_gc_lease_;
                    }
                    auto gc_exclusive{::uwvm2::runtime::gc::runtime_gc_entry_admission.try_exclusive_owned(
                        {actual_gc_leases, supplied.size()})};
                    if(!gc_exclusive) { out.observation = status::gc_admission_denied; return; }
                    // Exclusion is not a root/census permission. Preflight each
                    // actually borrowed generated chain while native lifetimes
                    // and complete SAME-episode parks are genuinely retained.
                    for(::std::size_t i{}; i != supplied.size(); ++i)
                    {
                        auto const population{::uwvm2::runtime::gc::visit_quiescent_frame_roots(
                            canonical[i]->actual_gc_frames_, [](auto) noexcept { return true; })};
                        if(population.status != ::uwvm2::runtime::gc::frame_root_status::ok)
                        { out.observation = status::invalid_gc_population; return; }
                    }
                    // Declaration order: publication unlocks before the closed
                    // host guard reopens; BOTH precede the ONE domain unlock.
                    // lease -> domain -> nonwaiting gate close -> genuine N GC
                    // exclusion -> publication. Reverse release precedes resume.
                    runtime_state_publication_guard publication{};
                    // No lifetime mutex is nested below publication: this real
                    // token reads only its owned generation's stopping atomic.
                    if(lease.stop_requested()) { out.observation = status::execution_stopping; return; }
                    if(!host_bridge::current_coherent_state(configuration, closed) ||
                       !g_runtime.compiled_all.load(::std::memory_order_acquire) || current_runtime_generation() != epoch)
                    { out.observation = status::invalid_current_publication; return; }
#if defined(UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF) && UWVM_EXPERIMENTAL_NATIVE_EH_PRIVATE_LEAF == 1
                    if(runtime_native_eh_private_leaf_management_blocked())
                    { out.observation = status::invalid_current_publication; return; }
#endif
                    ::std::size_t frames{}, slots{};
                    // Preflight the complete actual cohort before any result
                    // frame/value copy. Failure never returns partial thread DATA.
                    for(::std::size_t i{}; i != supplied.size(); ++i)
                    {
                        auto const& actual{*canonical[i]};
                        auto const configured_frames{configuration->profile_->limits().frames};
                        auto const frame_budget{configured_frames == 0u ? (::std::numeric_limits<::std::size_t>::max)() : configured_frames};
                        if(frames > frame_budget || actual.frames_.size() > frame_budget - frames)
                        { out.observation = status::budget_exceeded; return; }
                        frames += actual.frames_.size(); // Addition checked BEFORE advance.
                        for(::std::size_t j{}; j != actual.frames_.size(); ++j)
                        {
                            auto const& saved{actual.frames_[j]};
                            auto const& event{saved.activation};
                            if(event.module >= g_runtime.modules.size() || event.runtime_epoch != epoch ||
                               event.incarnation == 0u || event.continuation == 0u || event.function_generation == 0u ||
                               event.parent != (j == 0u ? 0u : actual.frames_[j - 1u].activation.incarnation) ||
                               saved.logical.identity != ::uwvm2::runtime::checkpoint::activation_identity{
                                   event.incarnation, event.parent, event.continuation, event.runtime_epoch})
                            { out.observation = status::stale_location_or_generation; return; }
                            // [actual dense generation module array ... event.module] end
                            // [safe] module<N above, current real lease+publication.
                            // Saved publication_identity is comparison ONLY, never
                            // dereferenced as code: resolve the actual current owner.
                            auto const& record{g_runtime.modules.index_unchecked(static_cast<::std::size_t>(event.module))};
                            auto const module{record.runtime_module};
                            auto const code{record.llvm_jit_full_publication.get()};
                            if(module == nullptr || code == nullptr || code != saved.publication_identity ||
                               !record.llvm_jit_ready || !code->engine || !code->context || code->plan ||
                               code->debug_source_runtime_epoch != epoch || record.llvm_jit_debug_source_fused_epoch != epoch ||
                               !same_owner(code->checkpoint_profile, configuration->profile_) || !same_owner(code->source, saved.source))
                            { out.observation = status::invalid_current_publication; return; }
                            auto const& source{code->source}; // This actual publication owns the canonical source.
                            if(!::uwvm2::uwvm::runtime::full::full_source_instance::has_canonical_owner(source) ||
                               !source->initialized_from_actual_state() ||
                               source->actual_validated_file(static_cast<::std::size_t>(event.module), epoch, module) == nullptr)
                            { out.observation = status::invalid_current_publication; return; }
                            auto const member{source->registry().find(record.module_name)};
                            if(member == source->registry().end() || ::std::addressof(member->second) != module)
                            { out.observation = status::invalid_current_publication; return; }
                            auto const imports{module->imported_function_vec_storage.size()};
                            if(event.function < imports || event.function - imports >= record.llvm_jit_compiled.local_funcs.size() ||
                               event.function - imports >= record.llvm_jit_debug_full_entry_generations.size())
                            { out.observation = status::invalid_current_publication; return; }
                            auto const local{static_cast<::std::size_t>(event.function - imports)};
                            auto const plan{checkpoint_current_generation_plan(record, local, event.function_generation)};
                            if(!plan || record.llvm_jit_debug_full_entry_generations[local] != event.function_generation ||
                               !same_owner(plan, saved.logical.plan) || !same_owner(plan->get().profile, configuration->profile_) ||
                               plan->get().module != event.module || plan->get().function != event.function ||
                               plan->get().function_generation != event.function_generation)
                            { out.observation = status::stale_location_or_generation; return; }
                            if(capture::check_owned_values(saved.logical, requested == nullptr && wasip1_requested == nullptr && complete_requested == nullptr && mutating == nullptr && source_read == nullptr && capsule_requested == nullptr && group_restore == nullptr &&
                                (retiring == nullptr || retiring->native == nullptr) ? capture::value_use::executable_scalar :
                                capture::value_use::live_observation) != checkpoint_thread_capture_status::captured)
                            { out.observation = status::invalid_typed_data; return; }
                            // [sealed exact plan sites ... site-1 ...] end
                            // [safe] check_owned_values proved 0<site<=N before index.
                            auto const& site{plan->get().sites[static_cast<::std::size_t>(saved.logical.site - 1u)]};
                            bool const leaf{j + 1u == actual.frames_.size()};
                            if((leaf && (site.phase != ::uwvm2::runtime::checkpoint::frame_phase::before_opcode ||
                                 event.module != actual.location_.code_unit || event.function != actual.location_.function ||
                                 site.opcode_offset != actual.location_.offset)) ||
                               (!leaf && site.phase != ::uwvm2::runtime::checkpoint::frame_phase::awaiting_call_return))
                            { out.observation = status::invalid_typed_data; return; }
                            if(saved.logical.values.size() > total_slot_limit - slots)
                            { out.observation = status::budget_exceeded; return; }
                            slots += saved.logical.values.size(); // No sum overflow or unchecked reserve multiplication.
                        }
                    }
                    if(group_restore!=nullptr)
                    {
                        if(lease.stop_requested()) { out.observation=status::execution_stopping;return; }
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
                        if(group_restore->capture_portable)
                        {
                            group_restore->captured_portable=llvm_jit_wasip1_environment_capsule::capture_portable_group_current(
                                llvm_jit_wasip1_environment_capsule::native_capture_key{},group_restore->portable,epoch,configuration->profile_);
                            if(lease.stop_requested() || !host_bridge::current_coherent_state(configuration,closed))
                            { out.observation=status::execution_stopping;return; }
                            candidate.observed_runtime_epoch=epoch;candidate.observation=status::coherent_typed_data;coherent=true;return;
                        }
                        llvm_jit_wasip1_environment_capsule::prepared_environment_group prepared{};
                        if(!group_restore->portable.empty())
                        {
                            group_restore->portable_result=llvm_jit_wasip1_environment_capsule::prepare_portable_group_current(
                                llvm_jit_wasip1_environment_capsule::native_capture_key{},group_restore->portable,
                                epoch,configuration->profile_,prepared);
                            group_restore->outcome=group_restore->portable_result.status;
                        }
                        else
                        {
                            group_restore->outcome=llvm_jit_wasip1_environment_capsule::prepare_group_current(
                                llvm_jit_wasip1_environment_capsule::native_capture_key{},group_restore->saved,
                                group_restore->strict,epoch,configuration->profile_,prepared);
                        }
                        if(group_restore->outcome==llvm_jit_wasip1_environment_capsule_status::captured)
                        {
                            if(lease.stop_requested() || !host_bridge::current_coherent_state(configuration,closed))
                            { out.observation=status::execution_stopping;return; }
                            for(auto& environment:prepared)
                            { llvm_jit_wasip1_environment_capsule::publish_current_prepared(*environment); }
                            group_restore->outcome=llvm_jit_wasip1_environment_capsule_status::restored;
                            if(!group_restore->portable.empty())
                            { group_restore->portable_result.status=group_restore->outcome; }
                        }
#else
                        group_restore->outcome=llvm_jit_wasip1_environment_capsule_status::unavailable_environment;
                        group_restore->portable_result.status=group_restore->outcome;
                        group_restore->captured_portable.status=group_restore->outcome;
#endif
                        candidate.observed_runtime_epoch=epoch;candidate.observation=status::coherent_typed_data;
                        coherent=true;return;
                    }
                    if(capsule_requested!=nullptr)
                    {
                        // SOLE native capsule issuer, AFTER every current
                        // canonical before-park capture/root, actual ONE cohort,
                        // closed host gate, N exclusion and publication above.
                        // No second query, scalar status or saved-file capability
                        // can enter this private native-resource producer.
                        if(lease.stop_requested()) { out.observation=status::execution_stopping;return; }
                        if(capsule_requested->portable_metadata_only || capsule_requested->portable_restore)
                        {
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
                            llvm_jit_wasip1_environment_capsule::prepared_current_environment prepared{};
                            *capsule_observation=llvm_jit_wasip1_environment_capsule::portable_current(
                                llvm_jit_wasip1_environment_capsule::native_capture_key{},*capsule_requested,epoch,configuration->profile_,
                                capsule_requested->portable_restore ? ::std::addressof(prepared) : nullptr);
                            if(lease.stop_requested() || !host_bridge::current_coherent_state(configuration,closed))
                            { out.observation=status::execution_stopping;return; }
                            if(capsule_requested->portable_restore && capsule_observation->status==llvm_jit_wasip1_environment_capsule_status::captured)
                            {
                                llvm_jit_wasip1_environment_capsule::publish_current_prepared(prepared);
                                capsule_observation->status=llvm_jit_wasip1_environment_capsule_status::restored;
                            }
#else
                            capsule_observation->status=llvm_jit_wasip1_environment_capsule_status::unavailable_environment;
#endif
                            candidate.observed_runtime_epoch=epoch;candidate.observation=status::coherent_typed_data;
                            coherent=true;return;
                        }
                        if(capsule_restore!=nullptr)
                        {
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
                            llvm_jit_wasip1_environment_capsule::prepared_current_environment prepared{};
                            capsule_observation->status=llvm_jit_wasip1_environment_capsule::prepare_current(
                                llvm_jit_wasip1_environment_capsule::native_capture_key{},*capsule_restore,*capsule_requested,
                                capsule_strict,epoch,configuration->profile_,prepared);
                            if(lease.stop_requested() || !host_bridge::current_coherent_state(configuration,closed))
                            { out.observation=status::execution_stopping;return; }
                            if(capsule_observation->status==llvm_jit_wasip1_environment_capsule_status::captured)
                            {
                                llvm_jit_wasip1_environment_capsule::publish_current_prepared(prepared);
                                capsule_observation->status=llvm_jit_wasip1_environment_capsule_status::restored;
                            }
#else
                            capsule_observation->status=llvm_jit_wasip1_environment_capsule_status::unavailable_environment;
#endif
                            candidate.observed_runtime_epoch=epoch;candidate.observation=status::coherent_typed_data;
                            coherent=true;return; // Committed mutation remains reported if cancellation follows.
                        }
                        *capsule_observation=llvm_jit_wasip1_environment_capsule::capture_current(
                            llvm_jit_wasip1_environment_capsule::native_capture_key{},*capsule_requested,epoch,configuration->profile_);
                        if(lease.stop_requested() || !host_bridge::current_coherent_state(configuration,closed))
                        {
                            capsule_observation->capsule.reset();
                            out.observation=status::execution_stopping;return;
                        }
                        candidate.observed_runtime_epoch=epoch;candidate.observation=status::coherent_typed_data;
                        coherent=true;return;
                    }
                    if(source_read != nullptr)
                    {
                        // ALL canonical captures, actual roots, source-owned
                        // files/current plans and real epoch passed above while
                        // closedhost/N/publication remain lexical owners. No
                        // second pause/publication/admission query occurs.
                        if(!source_read->copy(source_read->context, actual_slots))
                        { out.observation = status::invalid_typed_data; return; }
                        candidate.observed_runtime_epoch = epoch;
                        candidate.observation = status::coherent_typed_data;
                        coherent = true; return; // owned readonly output only
                    }
                    if(complete_requested != nullptr)
                    {
                        namespace cp = ::uwvm2::uwvm::debugger::checkpoint;
                        if(complete_observation == nullptr || requested != nullptr || wasip1_requested != nullptr ||
                           (retiring != nullptr && preparing == nullptr) || mutating != nullptr)
                        { out.observation = status::invalid_management_entry; return; }
                        // Same real lease/ONE park/host close/N/publication after
                        // every current canonical capture/root preflight above.
                        // No debugger VIEW, copied bool or second callback can
                        // upgrade a value observation into this actual census.
                        runtime_checkpoint_gc_state_borrow borrow{lease, closed, gc_exclusive, publication,
                            actual_slots, {canonical, supplied.size()}, configuration->profile_, epoch};
                        runtime_checkpoint_gc_state_borrow::complete_census_work census{};
                        census.cap = complete_requested->budget;
                        census.snapshot.recording_id = complete_requested->recording_id;
                        census.snapshot.checkpoint_id = 1u;
                        // Actual per-module resolved syntax policy and its
                        // compatibility requirements are copied by the genuine
                        // source-bound resource producer, never all-capabilities.
                        if(!borrow.census_copy_complete_graph(census))
                        {
                            complete_observation->data_error = census.status == cp::error::none ? cp::error::incompatible_type : census.status;
                            out.observation = status::invalid_typed_data; return;
                        }
                        ::std::vector<llvm_jit_wasip1_environment_capsule_owner> environments{};
                        ::std::vector<void const*> seen_environments{};
                        #if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
                        for(::std::size_t id{};id!=g_runtime.modules.size();++id)
                        {
                            if(!is_wasip1_import_visible_for_runtime_module_id_slow(id)) { continue; }
                            complete_observation->wasip1_checkpoint_required=true;
                            if(prepared!=nullptr) { prepared->wasip1_checkpoint_required=true; }
                            if(!complete_requested->include_wasip1) { continue; }
                            auto const* identity{::std::addressof(resolve_wasip1_env_for_runtime_module_id(id))};
                            if(::std::find(seen_environments.begin(),seen_environments.end(),identity)!=seen_environments.end()) { continue; }
                            llvm_jit_wasip1_environment_capsule_request request{};request.module=id;
                            request.recording_label=complete_requested->recording_id;
                            auto native{llvm_jit_wasip1_environment_capsule::capture_current(
                                llvm_jit_wasip1_environment_capsule::native_capture_key{},request,epoch,configuration->profile_)};
                            if(native.status!=llvm_jit_wasip1_environment_capsule_status::captured || !native.capsule)
                            {
                                complete_observation->data_error=cp::error::unavailable_capability;
                                if(prepared!=nullptr)
                                {
                                    prepared->status=llvm_jit_checkpoint_prepare_status::wasip1_preparation_declined;
                                    prepared->wasip1_status=native.status;
                                    candidate.observed_runtime_epoch=epoch;candidate.observation=status::coherent_typed_data;
                                    coherent=true;return; // No candidate pair or registered owner escapes.
                                }
                                out.observation=status::invalid_typed_data;return;
                            }
                            seen_environments.push_back(identity);environments.push_back(::std::move(native.capsule));
                        }
#endif
                        if(preparing!=nullptr)
                        {
                            if(lease.stop_requested() || !borrow.current_scope())
                            { out.observation=status::execution_stopping;return; }
                            if(retiring && complete_observation->wasip1_checkpoint_required && !preparing->include_wasip1)
                            {
                                // A joint candidate may not omit an actually visible
                                // WASIp1 environment. Refuse BEFORE native retirement.
                                prepared->status=llvm_jit_checkpoint_prepare_status::wasip1_preparation_declined;
                                out.observation=status::invalid_typed_data;return;
                            }
                            prepare_and_discard_actual_world(census.snapshot,
                                ::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin(),epoch,configuration->profile_,*preparing,*prepared,environments,
                                retiring?::std::addressof(retiring->native->candidate):nullptr);
                            if(lease.stop_requested() || !borrow.current_scope())
                            { out.observation=status::execution_stopping;return; }
                            complete_observation->data_error=cp::error::none;
                            candidate.observed_runtime_epoch=epoch;candidate.observation=status::coherent_typed_data;
                            if(retiring)
                            {
                                auto& native{*retiring->native};
                                if(prepared->status!=llvm_jit_checkpoint_prepare_status::prepared_and_retained || !native.candidate)
                                { out.observation=status::invalid_typed_data;return; }
                                // Preparation, capsule installation and ALL private
                                // worker joins finished in THIS exact proof scope.
                                // Only now arm the real registered OLD native cohort.
                                retiring->request=retirement::arm_current_while_cohort_owned(configuration,
                                    {canonical,supplied.size()},actual_slots,closed,retirement::value_policy::live_native_roots);
                                if(!retiring->request || !runtime_checkpoint_guest_worker_bridge::seal_from_actual_retirement_while_cohort_owned(native.joined,retiring->request))
                                { out.observation=status::invalid_typed_data;return; }
                                native.request=retiring->request;native.host=configuration;
                                native.source=::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin();
                                native.result.runtime_epoch=epoch;native.result.workers=supplied.size();
                                native.result.candidate_world_retained=true;
                            }
                            coherent=true;return; // Candidate stays only in the private manager operation.
                        }
                        ::std::shared_ptr<complete_instance_capture> captured{new complete_instance_capture};
                        captured->epoch_ = epoch;
                        captured->graph_ = ::std::move(census.snapshot);
                        // No partial graph or transient reference key is returned
                        // after an allocation, validation or concurrent stop.
                        if(lease.stop_requested() || !borrow.current_scope())
                        { out.observation = status::execution_stopping; return; }
                        complete_observation->data_error = cp::error::none;
                        complete_observation->captured = ::std::move(captured);
                        complete_observation->wasip1_environments=::std::move(environments);
                        complete_observation->wasip1_captured_together=complete_requested->include_wasip1;
                        candidate.observed_runtime_epoch = epoch;
                        candidate.observation = status::coherent_typed_data;
                        coherent = true; return;
                    }
                    if(mutating != nullptr)
                    {
                        if(mutated==nullptr || requested!=nullptr || wasip1_requested!=nullptr || retiring!=nullptr || complete_requested!=nullptr)
                        { out.observation=status::invalid_management_entry;return; }
                        // Sole real mutation issuer, AFTER the complete same
                        // cohort/capture, actual hostclose, N-root and source/
                        // current compiled-generation preflight. No VIEW or
                        // caller bool can construct this lexical borrow.
                        runtime_checkpoint_gc_state_borrow borrow{lease,closed,gc_exclusive,publication,
                            actual_slots,{canonical,supplied.size()},configuration->profile_,epoch};
                        *mutated=borrow.mutate_selected(*mutating);
                        candidate.observed_runtime_epoch=epoch;candidate.observation=status::coherent_typed_data;
                        coherent=true;return;
                    }
                    if(wasip1_requested != nullptr)
                    {
                        // SAME actual canonical cohort / closed host gate / N /
                        // publication scope. This adapter is private to this sole
                        // issuer; no second domain callback or scalar status is
                        // accepted as admission. Every capture was preflighted.
                        if(lease.stop_requested()) { out.observation = status::execution_stopping; return; }
                        *wasip1_observation = runtime_wasip1_debug_environment::apply_current(*wasip1_requested);
                        candidate.observed_runtime_epoch = epoch;
                        candidate.observation = status::coherent_typed_data;
                        coherent = true;
                        return;
                    }
                    if(requested != nullptr)
                    {
                        // SAME proof scope; no second domain/reader admission.
                        // The context cannot escape this callback. Only copied
                        // typed rows/GC graph DATA leave the actual N/publication
                        // borrow. In particular no raw logical-frame carriers
                        // enter result.threads or an executable continuation.
                        runtime_checkpoint_gc_state_borrow borrow{lease, closed, gc_exclusive, publication,
                            actual_slots, {canonical, supplied.size()}, configuration->profile_, epoch};
                        *observation = runtime_checkpoint_debug_state_reader::copy(borrow, *requested);
                        candidate.observed_runtime_epoch = epoch;
                        candidate.observation = status::coherent_typed_data;
                        coherent = true;
                        return;
                    }
                    // Sole lexical issuer: every actual capture owner/current
                    // ticket/frame has passed this ONE cohort preflight; the
                    // REAL lease, nonwaiting host closure and publication guard
                    // remain alive. No borrow/callback/closed token escapes.
                    {
                        runtime_checkpoint_resource_inputs immutable_inputs{lease, publication, closed,
                            actual_slots, configuration->profile_, epoch};
                        candidate.immutable_inputs = immutable_inputs.copy_immutable_inputs();
                    }
                    // Immutable-input refusal does not pretend the actual
                    // scalar thread DATA is unavailable or reject legal Wasm.
                    // The complete mutable-resource/host census stays false.
                    candidate.threads.reserve(supplied.size()); candidate.source_pins.reserve(frames);
                    for(::std::size_t i{}; i != supplied.size(); ++i)
                    {
                        auto const& actual{*canonical[i]}; thread_data data{};
                        data.participant = actual.participant_; data.observed_location = actual.location_;
                        data.frames.reserve(actual.frames_.size());
                        for(auto const& saved : actual.frames_)
                        {
                            data.frames.push_back(saved.logical); candidate.source_pins.push_back(saved.source);
                        }
                        candidate.threads.push_back(::std::move(data));
                    }
                    // The separate input result may contain bounded original
                    // immutable module bytes and actual data-drop metadata.
                    // Memory/table/global/GC/extern/EH/thread effects and every
                    // FD/preopen/plugin/inherited-handle exposure remain without
                    // a complete stable census. Source pins alone cannot seal a
                    // mapping; do not publish assets, save or restore an instance.
                    candidate.observed_runtime_epoch = epoch;
                    candidate.observation = status::coherent_typed_data;
                    if(retiring != nullptr)
                    {
                        // Exact canonical owners/current episode, real closed
                        // host gate, owned N exclusion and publication all still
                        // live. Arm only AFTER every frame/DATA copy succeeded.
                        // The private request retains the actual host guard;
                        // no public VIEW, bool, address or saved file mints it.
                        retiring->request = retirement::arm_current_while_cohort_owned(configuration,
                            {canonical, supplied.size()}, actual_slots, closed,
                            retiring->native != nullptr ? retirement::value_policy::live_native_roots : retirement::value_policy::executable_scalar);
                        if(!retiring->request) { out.observation = status::invalid_typed_data; return; }
                        if(retiring->native != nullptr)
                        {
                            auto& native{*retiring->native};
                            if(!runtime_checkpoint_guest_worker_bridge::seal_from_actual_retirement_while_cohort_owned(native.joined, retiring->request))
                            { out.observation = status::invalid_typed_data; return; }
                            native.request = retiring->request; native.host = configuration;
                            native.source = ::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin();
                            native.result.runtime_epoch = epoch; native.result.workers = supplied.size();
                        }
                    }
                    coherent = true;
                }};
                bool guarded{};
                if(retiring != nullptr)
                {
                    guarded = configuration->control_->transfer_cooperatively_stopped_cohort(ticket, scratch,
                        retiring->transition, [&](auto actual_slots, auto const& current_ticket)
                        {
                            inspect(actual_slots, current_ticket);
                            // Actual domain still owns THIS complete roster.
                            // The move-only hold is minted before unlock only
                            // after real manager arm; borrows end on return.
                            return coherent && static_cast<bool>(retiring->request);
                        });
                }
                else { guarded = configuration->control_->with_cooperatively_stopped_cohort(ticket, scratch, inspect); }
                if(!guarded || !coherent)
                {
                    if(out.observation == status::not_selected) { out.observation = status::incomplete_cohort; }
                    if(retiring != nullptr && retiring->request)
                    {
                        // Preparation only; actual poll flag was never active.
                        // Cancellation runs after every proof lock unlocked.
                        if(!retirement::cancel_unactivated_after_cohort_unlock(retiring->request))
                        { ::fast_io::fast_terminate(); }
                        retiring->transition.reset(); retiring->request.reset();
                    }
                    return out; // candidate partial storage dies without publication.
                }
                if(retiring != nullptr)
                {
                    // Publication and N exclusion have genuinely left; holding
                    // N while old shared leases unwind would terminate. The
                    // actual transition still prevents resume/native-step.
                    // Close execution admission and publish private poll flag
                    // BEFORE consuming that hold and waking the old frames.
                    if(!retirement::activate_after_cohort_unlock(retiring->request))
                    { ::fast_io::fast_terminate(); } // genuine sealed request inconsistency
                    if(retiring->native != nullptr) { retiring->native->activated = true; }
                    auto const resumed{configuration->control_->resume_transition(retiring->transition)};
                    if(!resumed)
                    {
                        // A trusted concurrent close invalidates resume, but
                        // the actual held transition still delays guest wakeup.
                        // Its reset runs only AFTER active private poll setup;
                        // do not invent a ticket or clear the retirement state.
                        retiring->transition.reset();
                    }
                    // The manager's real generation lease must leave BEFORE
                    // drain; keeping it through reset would wait for itself.
                    lease.reset();
                    if(retiring->native != nullptr)
                    {
                        finish_native_retirement_until(*retiring->native, maintenance);
                        return candidate; // pending retains real code/gate/cohort/lifetime
                    }
                    retirement::drain_result drained{retirement::drain_result::invalid_request};
                    auto const reset{g_runtime.execution_domain.reset_owned_transition(maintenance, [&]
                    {
                        // Sole real execution-domain maintenance callback:
                        // every old guest/native/root/host lease actually left.
                        // Neither signal ACK nor a participant bool proves it.
                        drained = retirement::release_after_actual_execution_drain(retiring->request);
                        if(drained == retirement::drain_result::invalid_request) { ::fast_io::fast_terminate(); }
                    })};
                    if(!reset) { ::fast_io::fast_terminate(); } // canonical actual domain/token inconsistency
                    // This resets only execution admission for the retained
                    // SAME instance/epoch. It does not publish new memories,
                    // GC graph, source, WASI resources or restore authority.
                    retiring->outcome = resumed && drained == retirement::drain_result::all_frames_signalled
                        ? llvm_jit_checkpoint_execution_retirement_status::execution_retired
                        : llvm_jit_checkpoint_execution_retirement_status::execution_drained_without_all_signals;
                    retiring->request.reset();
                    return candidate;
                }
                // The actual lease is still live after the ONE domain callback.
                // A newly requested stop retires any candidate storage here;
                // this does not cancel/wait/drain or manufacture a stop token.
                // A transaction committed while ALL original proof guards were
                // held remains applied if a trusted stop is requested afterward.
                // Do not hide that completed mutation behind a later stop error.
                if(lease.stop_requested() && !(group_restore!=nullptr &&
                    group_restore->outcome==llvm_jit_wasip1_environment_capsule_status::restored) && !(capsule_requested!=nullptr && capsule_requested->portable_restore &&
                   capsule_observation!=nullptr && capsule_observation->status==llvm_jit_wasip1_environment_capsule_status::restored) && !(capsule_restore!=nullptr && capsule_observation!=nullptr &&
                   capsule_observation->status==llvm_jit_wasip1_environment_capsule_status::restored) &&
                   !(wasip1_observation != nullptr && wasip1_observation->mutation_applied) &&
                   !(mutated != nullptr && mutated->applied))
                { out.observation = status::execution_stopping; return out; }
                // Detached numeric/null/i31 logical DATA only; no ticket, domain,
                // host-closed guard, engine/native PC or management issuer escapes.
                return candidate;
            }
            catch(...)
            {
                if(retiring != nullptr)
                {
                    // After activation any allocation failure leaves execution
                    // admission closed. Do not reopen or claim successful resume.
                    retiring->outcome = llvm_jit_checkpoint_execution_retirement_status::failed_closed;
                }
                out = {}; out.observation = status::allocation_failed; return out;
            }
        }
    };
}
#elif defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
extern "C++" llvm_jit_checkpoint_execution_retirement_status llvm_jit_checkpoint_retire_saved_execution_host_api(
    ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
    ::std::span<llvm_jit_checkpoint_thread_capture_owner const>) noexcept
{ return llvm_jit_checkpoint_execution_retirement_status::requires_llvm_jit_full; }
#endif

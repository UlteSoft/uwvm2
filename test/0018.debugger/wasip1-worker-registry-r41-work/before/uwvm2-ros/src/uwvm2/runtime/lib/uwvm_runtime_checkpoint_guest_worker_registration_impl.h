/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
// Private definitions, included after actual runtime helpers/entries and
// canonical checkpoint captures, before the sole coherent manager.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
extern "C++"
{
    runtime_checkpoint_guest_worker_bridge::mutable_owner runtime_checkpoint_guest_worker_bridge::resolve(
        llvm_jit_debug_guest_worker_owner const& supplied) noexcept
    {
        if(!supplied) { return {}; }
        ::std::lock_guard lock{registry_mutex_};
        for(auto const& slot : registry_)
        {
            auto actual{slot.canonical.lock()};
            // Compare canonical strong control block BEFORE supplied pointee is
            // read. An alias, native address or copied participant cannot enter.
            if(same_owner(actual, supplied)) { return actual; }
        }
        return {};
    }
    bool runtime_checkpoint_guest_worker_bridge::actual_tuple_current(worker const& actual) noexcept
    {
        return actual.epoch_ != 0u && actual.epoch_ == current_runtime_generation() &&
            same_owner(actual.control_, g_runtime.debug_pause_control) &&
            same_owner(actual.profile_, g_runtime.checkpoint_profile) &&
            same_owner(actual.host_, g_runtime.checkpoint_host_state) &&
            same_owner(actual.source_, ::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin());
    }
    llvm_jit_debug_guest_worker_launch_result runtime_checkpoint_guest_worker_bridge::launch(
        ::std::shared_ptr<domain> const& supplied_control, ::std::shared_ptr<void> state,
        llvm_jit_debug_guest_worker_body body) noexcept
    {
        using result = llvm_jit_debug_guest_worker_launch_status;
        namespace mode = ::uwvm2::uwvm::runtime::runtime_mode;
        if(mode::global_runtime_mode != mode::runtime_mode_t::full_compile ||
           mode::global_runtime_compiler != mode::runtime_compiler_t::llvm_jit_only)
        { return {result::unsupported_mode, {}}; }
        if(get_runtime_execution_entry_depth() != 0u || get_runtime_state_publication_depth() != 0u ||
           get_runtime_compilation_metadata_callback_depth() != 0u || g_debug_observer_active ||
           actual_worker_ != nullptr || !state || state.use_count() == 0 || body == nullptr)
        { return {result::invalid_context, {}}; }
        auto execution{g_runtime.execution_domain.try_enter()};
        if(!execution) { return {result::admission_closed, {}}; }
        // The genuine admitted lifetime pins immutable configuration while we
        // derive all actual owners. No supplied source/profile/epoch is read.
        auto control{g_runtime.debug_pause_control};
        if(!same_owner(control, supplied_control)) { return {result::stale_control, {}}; }
        if(control->is_closed() || execution.stop_requested()) { return {result::admission_closed, {}}; }
        auto source{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
        auto profile{g_runtime.checkpoint_profile};
        auto host{g_runtime.checkpoint_host_state};
        if(!source || !source_type::has_canonical_owner(source) || !source->initialized_from_actual_state() ||
           !profile || !host || !g_runtime.compiled_all.load(::std::memory_order_acquire) ||
           !g_runtime.checkpoint_host_enabled.load(::std::memory_order_acquire))
        { return {result::invalid_context, {}}; }
        mutable_owner actual{};
        ::std::size_t slot_index{256u};
#ifdef UWVM_CPP_EXCEPTIONS
        try
#endif
        {
            actual = mutable_owner{new worker{::std::move(control), ::std::move(source), ::std::move(profile),
                ::std::move(host), current_runtime_generation(), ::std::move(state), body}};
            actual->execution_ = ::std::move(execution);
            auto const admitted{runtime_checkpoint_host_bridge::begin_registered_worker_startup(actual->host_, actual->startup_)};
            if(admitted != runtime_checkpoint_host_bridge::status::ok)
            { return {result::admission_closed, {}}; }
            {
                ::std::lock_guard lock{registry_mutex_};
                for(::std::size_t index{}; index != 256u; ++index)
                {
                    if(!registry_[index].live && registry_[index].canonical.expired())
                    {
                        registry_[index].live = actual; registry_[index].canonical = actual;
                        slot_index = index; break;
                    }
                }
            }
            if(slot_index == 256u) { return {result::quota, {}}; }
            // Allocate/construct the final unique native owner directly. The
            // selected FastIO constructors throw only before a thread is owned;
            // after OS create succeeds their remaining assignments do not throw.
            // The worker gates its body until this owner and registry are ready.
            actual->native_.reset(new ::fast_io::native_thread{[actual]() noexcept { actual_worker_body(actual); }});
            actual->published_.store(true, ::std::memory_order_release);
            return {result::started, actual};
        }
#ifdef UWVM_CPP_EXCEPTIONS
        catch(...)
        {
            // There is no fallible operation after successful native ownership.
            // Any future post-launch error must retain the true live owner; never
            // synchronously join/detach or drop it as rollback compensation.
            if(actual && actual->native_)
            {
                actual->published_.store(true, ::std::memory_order_release);
                return {result::started, actual};
            }
            if(slot_index != 256u)
            {
                ::std::lock_guard lock{registry_mutex_};
                registry_[slot_index].live.reset(); registry_[slot_index].canonical.reset();
            }
            return {result::allocation_failure, {}};
        }
#endif
    }
    void runtime_checkpoint_guest_worker_bridge::actual_worker_body(mutable_owner actual) noexcept
    {
        while(!actual->published_.load(::std::memory_order_acquire)) { ::fast_io::this_thread::yield(); }
        if(actual_worker_ != nullptr) [[unlikely]] { ::fast_io::fast_terminate(); }
        actual_worker_ = actual.get();
        // A stop during startup is a negative rejection, never permission to
        // enter a target. The whole-worker lease still needs actual release.
        if(actual->origin_ == worker::origin::restored_world || !actual->execution_.stop_requested())
        { actual->body_(actual->launch_state_.get()); }
        {
            ::std::lock_guard lock{registry_mutex_};
            if(actual->participant_ != nullptr || actual->participant_id_ != 0u) [[unlikely]]
            { ::fast_io::fast_terminate(); } // real outer runtime scope must have left
        }
        actual->startup_ = {}; // also covers no-entry or failed enrollment
        actual_worker_ = nullptr; // clear TLS borrow before genuine lease release
        actual->execution_.reset();
        // Callable tuple destruction, thread-local destructors and kernel death
        // can still follow. Only join_actual can publish physical retirement.
    }
    void runtime_checkpoint_guest_worker_bridge::scope_admitted(domain::participant const& participant) noexcept
    {
        auto const actual{actual_worker_};
        if(actual == nullptr) { return; } // external/ordinary path gains no owner
        auto const lease{get_runtime_execution_lease()};
        if(!participant || participant.identifier() == 0u || lease == nullptr || !*lease ||
           get_runtime_execution_entry_depth() != 1u || !actual_tuple_current(*actual) ||
           g_debug_pause_participant != ::std::addressof(participant) ||
           g_debug_activation_ledger == nullptr || g_checkpoint_shadow_ledger == nullptr)
        { return; } // retained startup host count makes incomplete enrollment ineligible
        {
            ::std::lock_guard lock{registry_mutex_};
            if(actual->participant_ != nullptr || actual->participant_id_ != 0u) [[unlikely]]
            { ::fast_io::fast_terminate(); }
            actual->participant_ = ::std::addressof(participant);
            actual->participant_id_ = participant.identifier();
        }
        // AFTER actual registry binding, before a guest can park. Closed-host
        // preflight cannot slip between startup accounting and this real scope.
        actual->startup_ = {};
    }
    void runtime_checkpoint_guest_worker_bridge::scope_leaving(domain::participant const& participant) noexcept
    {
        auto const actual{actual_worker_};
        if(actual == nullptr) { return; }
        ::std::lock_guard lock{registry_mutex_};
        if(actual->participant_ == ::std::addressof(participant))
        { actual->participant_ = nullptr; actual->participant_id_ = 0u; }
        // Clear before participant unregister/ID retirement. A sealed cohort
        // separately owns its exact native objects and canonical old request.
    }
    bool runtime_checkpoint_guest_worker_bridge::stopping_current() noexcept
    {
        auto const actual{actual_worker_};
        return actual != nullptr && actual->origin_ == worker::origin::initial_current &&
            (actual->entry_refused_ || actual->guest_exit_ || actual->execution_.stop_requested());
    }
    ::std::uint_least32_t runtime_checkpoint_guest_worker_bridge::completion_code() noexcept
    {
        auto const actual{actual_worker_};
        if(actual == nullptr || actual->origin_ != worker::origin::initial_current ||
           get_runtime_execution_entry_depth() != 0u || actual->participant_ != nullptr ||
           !actual->execution_ || !actual_tuple_current(*actual)) { return 0u; }
        return actual->guest_exit_ ? actual->guest_exit_code_ : 0u;
    }
#if defined(UWVM_CPP_EXCEPTIONS)
    void runtime_checkpoint_guest_worker_bridge::try_builtin_proc_exit(::std::size_t module_id,
        ::std::size_t import_index, void const* expected_cache, ::std::byte const* params, ::std::size_t bytes) UWVM_THROWS
    {
#if !defined(UWVM_DISABLE_LOCAL_IMPORTED_WASIP1) && defined(UWVM_IMPORT_WASI_WASIP1)
        auto const actual{actual_worker_};
        if(actual == nullptr || actual->origin_ != worker::origin::initial_current) { return; }
        if(!actual_tuple_current(*actual) || get_runtime_execution_entry_depth() != 1u ||
           get_llvm_jit_generated_bridge_scope_depth() == 0u || g_debug_observer_active ||
           actual->participant_ == nullptr || actual->participant_id_ == 0u ||
           module_id >= g_runtime.modules.size() || module_id >= g_import_call_cache.size()) { return; }
        auto const& cache{g_import_call_cache.index_unchecked(module_id)};
        if(import_index >= cache.size()) { return; }
        auto const& target{cache.index_unchecked(import_index)};
        if(expected_cache != ::std::addressof(target) || target.k != cached_import_target::kind::local_imported ||
           target.frame.module_id != module_id || target.frame.function_index != import_index ||
           target.param_bytes != bytes || bytes != 4u || target.result_bytes != 0u || params == nullptr ||
           target.reference_params || target.reference_results) { return; }
        auto const& rec{g_runtime.modules.index_unchecked(module_id)};
        ::uwvm2::uwvm::runtime::full::builtin_wasip1_function_data binding{};
        if(rec.runtime_module == nullptr || rec.llvm_jit_full_publication == nullptr ||
           !same_owner(rec.llvm_jit_full_publication->source,actual->source_) ||
           !actual->source_->actual_builtin_wasip1_function(module_id,actual->epoch_,rec.runtime_module,import_index,binding) ||
           !binding.exits_guest || binding.function_index != target.u.local_imported.index ||
           binding.parameter_count != 1u || binding.parameters[0u] != 0x7fu || binding.result_count != 0u) { return; }
        // [actual validated raw ABI buffer0..4] end
        // [safe] exact i32 count/extent before copying a scalar; no guest offset
        // is interpreted as a host pointer, and no native provider is called.
        ::std::uint32_t raw_code{}; ::std::memcpy(::std::addressof(raw_code),params,4u);
        auto& env{resolve_wasip1_env_for_runtime_module_id(module_id)};
        if(env.trace_wasip1_call)
        {
            ::uwvm2::imported::wasi::wasip1::func::print_wasip1_trace_message(env,
                binding.exit_wasm64 ? ::fast_io::u8string_view{u8"proc_exit_wasm64("} : ::fast_io::u8string_view{u8"proc_exit("},
                raw_code,u8")");
        }
        constexpr ::std::uint32_t wasi_proc_exit_code_upper_bound{126u};
        if(::uwvm2::uwvm::io::show_wasip1_warning && raw_code >= wasi_proc_exit_code_upper_bound) [[unlikely]]
        {
            // Output the main information and memory indication
            ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                // 1
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                u8"uwvm: ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                u8"[warn]  ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8"WASI Preview 1 proc_exit exit code out of range [0, 126): ",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_YELLOW),
                                raw_code,
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                u8".",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                u8" (wasip1)\n",
                                ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));

            if(::uwvm2::uwvm::io::wasip1_warning_fatal) [[unlikely]]
            {
                ::fast_io::io::perr(::uwvm2::uwvm::io::u8log_output,
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE),
                                    u8"uwvm: ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_LT_RED),
                                    u8"[fatal] ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_WHITE),
                                    u8"Convert warnings to fatal errors. ",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_ORANGE),
                                    u8"(wasip1)\n\n",
                                    ::uwvm2::uwvm::utils::ansies::diagnostic_color(UWVM_COLOR_U8_RST_ALL));
                ::fast_io::fast_terminate();
            }
        }

        throw guest_exit_signal{actual,actual->participant_id_,actual->epoch_,raw_code};
#else
        static_cast<void>(module_id);static_cast<void>(import_index);static_cast<void>(expected_cache);
        static_cast<void>(params);static_cast<void>(bytes);
#endif
    }
    bool runtime_checkpoint_guest_worker_bridge::accept_guest_exit(guest_exit_signal const& caught) noexcept
    {
        auto const actual{actual_worker_};auto const participant{g_debug_pause_participant};auto const ledger{g_debug_activation_ledger};
        if(actual == nullptr || actual != caught.actual_ || actual->origin_ != worker::origin::initial_current ||
           !actual_tuple_current(*actual) || get_runtime_execution_entry_depth() != 1u ||
           actual->epoch_ != caught.epoch_ || actual->guest_exit_ ||
           participant == nullptr || participant != actual->participant_ || participant->identifier() != caught.participant_ ||
           actual->participant_id_ != caught.participant_ || ledger == nullptr || !ledger->valid() || ledger->count() != 0u ||
           g_debug_observer_active || ::uwvm2::runtime::gc::current_root_frames() != nullptr) { return false; }
        if(g_checkpoint_shadow_ledger != nullptr)
        {
            if(g_checkpoint_shadow_ledger->failure() != ::uwvm2::runtime::checkpoint::status::ok &&
               !g_checkpoint_shadow_ledger->discard_failed_recording()) { return false; }
            if(g_checkpoint_shadow_ledger->size() != 0u) { return false; }
        }
        actual->guest_exit_=true;actual->guest_exit_code_=caught.code_;return true;
    }
#endif
    runtime_checkpoint_guest_worker_bridge::execution_domain::lease runtime_checkpoint_guest_worker_bridge::take_current_initial_execution() noexcept
    {
        auto const actual{actual_worker_};
        if(actual == nullptr || actual->origin_ != worker::origin::initial_current) { return {}; }
        if(!actual->execution_ || !actual_tuple_current(*actual)) [[unlikely]] { ::fast_io::fast_terminate(); }
        // Actual worker alone moves its own lease. The canonical registry and
        // trampoline keep this object alive until the outer entry returns it.
        return ::std::move(actual->execution_);
    }
    void runtime_checkpoint_guest_worker_bridge::return_current_initial_execution(execution_domain::lease& execution,
        bool actually_refused) noexcept
    {
        auto const actual{actual_worker_};
        if(actual == nullptr || actual->origin_ != worker::origin::initial_current || actual->execution_ || !execution)
        [[unlikely]] { ::fast_io::fast_terminate(); }
        actual->execution_ = ::std::move(execution);
        actual->entry_refused_ = actual->entry_refused_ || actually_refused;
        // This actual lease stays admitted through entry member/TLS/native-stack
        // cleanup and subsequent graph phases. Physical retirement still joins.
    }
    bool runtime_checkpoint_guest_worker_bridge::note_actual_initial_host_refusal() noexcept
    {
        auto const actual{actual_worker_};
        if(actual == nullptr || actual->origin_ != worker::origin::initial_current) { return false; }
        auto const entry{get_runtime_execution_lease()};
        if(entry == nullptr || !*entry || !actual->execution_ || get_runtime_execution_entry_depth() != 1u ||
           !actual_tuple_current(*actual)) { return false; }
        // Only the real entry constructor's refused-host branch calls this.
        // This is a negative dispatch outcome, never a body/OS retirement ACK.
        actual->entry_refused_ = true; return true;
    }
    ::uwvm2::utils::thread::physical_join_result runtime_checkpoint_guest_worker_bridge::join_actual(
        mutable_owner const& actual, ::std::chrono::steady_clock::time_point deadline) noexcept
    {
        if(!actual || actual_worker_ == actual.get()) { return {{::fast_io::thread_join_status::failed, 0u}, false}; }
        ::std::unique_lock lock{actual->join_mutex_, ::std::try_to_lock};
        if(!lock.owns_lock()) { return {{::fast_io::thread_join_status::pending, 0u}, false}; }
        if(actual->physically_joined_) { return {{::fast_io::thread_join_status::joined, 0u}, false}; }
        if(!actual->published_.load(::std::memory_order_acquire) || !actual->native_)
        { return {{::fast_io::thread_join_status::pending, 0u}, false}; }
        auto const result{::uwvm2::utils::thread::join_native_thread_until(*actual->native_, deadline)};
        ::std::shared_ptr<void> retired_launch{};
        if(result.actual.status == ::fast_io::thread_join_status::joined)
        {
            actual->physically_joined_ = true;
            ::std::lock_guard registry_lock{registry_mutex_};
            retired_launch = ::std::move(actual->launch_state_);
            for(auto& slot : registry_) { if(same_owner(slot.live, actual)) { slot.live.reset(); break; } }
        }
        lock.unlock();
        // An actual restored world can own this record while the launch state
        // owns that world. Break that cycle ONLY after OS/TLS physical join, and
        // destroy launch/controller/world owners outside every join/registry lock.
        retired_launch.reset();
        // failed/unsupported/not_joinable/pending keep the same native wrapper
        // AND canonical registry owner. Native-handle/detach cannot escape.
        return result;
    }
    ::uwvm2::utils::thread::physical_join_result runtime_checkpoint_guest_worker_bridge::join_until(
        llvm_jit_debug_guest_worker_owner const& supplied, ::std::chrono::steady_clock::time_point deadline) noexcept
    { return join_actual(resolve(supplied), deadline); }
    bool runtime_checkpoint_guest_worker_bridge::matches_control(llvm_jit_debug_guest_worker_owner const& supplied,
        ::std::shared_ptr<domain> const& control) noexcept
    {
        auto actual{resolve(supplied)};
        return actual && same_owner(actual->control_, control);
    }
#if defined(UWVM_CPP_EXCEPTIONS)
    bool runtime_checkpoint_guest_worker_bridge::current_restored_launch_state(void const* expected) noexcept
    {
        auto const actual{actual_worker_};
        // [actual trampoline record] strong callable/registry ownership already
        // lives; expected is compared only, never dereferenced as a credential.
        return expected != nullptr && actual != nullptr && actual->origin_ == worker::origin::restored_world &&
            actual->launch_state_.get() == expected && actual_tuple_current(*actual);
    }
    bool runtime_checkpoint_guest_worker_bridge::current_restored_worker(mutable_owner const& supplied,
        void const* expected, ::std::uint_least64_t participant) noexcept
    {
        auto actual{resolve(supplied)};
        if(!actual || expected == nullptr || participant == 0u ||
           actual->origin_ != worker::origin::restored_world || !actual_tuple_current(*actual)) { return false; }
        ::std::lock_guard lock{registry_mutex_};
        return actual->launch_state_.get() == expected && actual->participant_ != nullptr &&
            actual->participant_id_ == participant && actual->published_.load(::std::memory_order_acquire) && actual->native_;
        // Sole world's ALLseed callback still independently validates each
        // actual receipt, ledger/source/profile/root and private closed token.
    }
    runtime_checkpoint_guest_worker_bridge::mutable_owner runtime_checkpoint_guest_worker_bridge::construct_registered_restored_worker(
        source_type::owner const& source,
        ::uwvm2::runtime::checkpoint::compilation_profile::owner const& profile,
        ::std::shared_ptr<domain> const& control, runtime_checkpoint_host_bridge::owner const& host,
        ::std::uint_least64_t epoch, ::std::shared_ptr<void> state, llvm_jit_debug_guest_worker_body body,
        restored_world_launch_key) noexcept
    {
        // This PRIVATE helper is called solely by the late canonical actual-
        // world factory. It derives no future generation or startup permission.
        // The complete world definition independently verifies its genuine closed
        // token and installed source/code/host/control/epoch before coming here.
        if(get_runtime_execution_entry_depth() != 0u || get_runtime_state_publication_depth() != 0u ||
           get_runtime_compilation_metadata_callback_depth() != 0u || g_debug_observer_active || actual_worker_ != nullptr ||
           !state || state.use_count() == 0 || body == nullptr || epoch == 0u || epoch != current_runtime_generation() ||
           !same_owner(source, ::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()) ||
           !same_owner(profile, g_runtime.checkpoint_profile) || !same_owner(control, g_runtime.debug_pause_control) ||
           !same_owner(host, g_runtime.checkpoint_host_state) ||
           !source_type::has_canonical_owner(source) || !source->initialized_from_actual_state()) { return {}; }
        mutable_owner actual{};
        ::std::size_t slot_index{256u};
        try
        {
            actual = mutable_owner{new worker{control, source, profile, host, epoch, ::std::move(state), body,
                worker::origin::restored_world}};
            // No ordinary try_enter and NO initial startup host operation. The
            // actual resume entry creates its closed-token lease and both ledgers.
            {
                ::std::lock_guard lock{registry_mutex_};
                for(::std::size_t index{}; index != 256u; ++index)
                {
                    if(!registry_[index].live && registry_[index].canonical.expired())
                    {
                        registry_[index].live = actual; registry_[index].canonical = actual;
                        slot_index = index; break;
                    }
                }
            }
            if(slot_index == 256u) { return {}; }
            actual->native_.reset(new ::fast_io::native_thread{[actual]() noexcept { actual_worker_body(actual); }});
            actual->published_.store(true, ::std::memory_order_release); return actual;
        }
        catch(...)
        {
            if(actual && actual->native_)
            { actual->published_.store(true, ::std::memory_order_release); return actual; }
            if(slot_index != 256u)
            {
                ::std::lock_guard lock{registry_mutex_};
                registry_[slot_index].live.reset(); registry_[slot_index].canonical.reset();
            }
            return {};
        }
    }
    runtime_checkpoint_guest_worker_bridge::pending_join_owner runtime_checkpoint_guest_worker_bridge::prepare_join_storage() noexcept
    {
        try
        {
            auto storage{pending_join_owner{new join_cohort{}}};
            storage->canonical_ = storage; return storage;
        }
        catch(...) { return {}; }
    }
    bool runtime_checkpoint_guest_worker_bridge::seal_from_actual_retirement_while_cohort_owned(
        pending_join_owner const& storage, retirement::owner const& supplied) noexcept
    {
        if(!storage || !same_owner(storage->canonical_.lock(), storage)) { return false; }
        ::std::unique_lock state_lock{storage->mutex_, ::std::try_to_lock};
        if(!state_lock.owns_lock() || storage->phase_ != join_cohort::phase::empty) { return false; }
        ::std::lock_guard request_lock{retirement::mutex_};
        // Authentic actual request control block checked before supplied pointee.
        auto const& request{retirement::actual_request_};
        if(!same_owner(request, supplied) || request->phase != retirement::request_phase::prepared ||
           request->epoch != current_runtime_generation() || request->members.empty() ||
           request->members.size() > 256u ||
           !runtime_checkpoint_host_bridge::current_coherent_state(request->configuration, request->closed))
        { return false; }
        auto source{::uwvm2::uwvm::runtime::full::selected_full_source_owner_pin()};
        if(!source || !source_type::has_canonical_owner(source) ||
           !same_owner(request->control, g_runtime.debug_pause_control) ||
           !same_owner(request->profile, g_runtime.checkpoint_profile)) { return false; }
        mutable_owner tentative[256u]{};
        ::std::size_t count{};
        ::std::lock_guard registry_lock{registry_mutex_};
        // Enumerate our OWN actual native registry, never a supplied thread span.
        // Every old actual native worker must map to exactly one authenticated
        // full-cohort capture; startup/in-between scopes refuse replacement.
        for(auto const& slot : registry_)
        {
            auto const& actual{slot.live};
            if(!actual || actual->epoch_ != request->epoch) { continue; }
            if(!actual_tuple_current(*actual) || !same_owner(actual->source_, source) ||
               actual->participant_ == nullptr || actual->participant_id_ == 0u ||
               !actual->published_.load(::std::memory_order_acquire) || !actual->native_ || count == 256u)
            { return false; }
            ::std::size_t matches{};
            for(auto const& member : request->members)
            {
                // arm_current already authenticated these canonical captures
                // against ONE actual stopped roster/ticket and native owners.
                if(member.captured->participant_ == actual->participant_id_) { ++matches; }
            }
            if(matches != 1u) { return false; }
            tentative[count++] = actual;
        }
        if(count != request->members.size()) { return false; }
        for(auto const& member : request->members)
        {
            ::std::size_t matches{};
            for(::std::size_t index{}; index != count; ++index)
            { if(tentative[index]->participant_id_ == member.captured->participant_) { ++matches; } }
            if(matches != 1u) { return false; }
        }
        for(::std::size_t index{}; index != count; ++index) { storage->members_[index] = ::std::move(tentative[index]); }
        storage->size_ = count; storage->request_ = request; storage->phase_ = join_cohort::phase::sealed;
        return true;
    }
    runtime_checkpoint_guest_worker_bridge::actual_physical_join_owner runtime_checkpoint_guest_worker_bridge::join_after_actual_drain_until(
        pending_join_owner const& storage, execution_domain::maintenance_transition const& maintenance,
        execution_domain::prepared_generation const& prepared, execution_domain::drained_generation const& drained,
        ::std::chrono::steady_clock::time_point deadline) noexcept
    {
        if(get_runtime_execution_entry_depth() != 0u || get_runtime_state_publication_depth() != 0u ||
           get_llvm_jit_generated_bridge_scope_depth() != 0u || g_debug_observer_active ||
           !storage || !same_owner(storage->canonical_.lock(), storage)) { return {}; }
        ::std::unique_lock state_lock{storage->mutex_, ::std::try_to_lock};
        if(!state_lock.owns_lock() || storage->phase_ == join_cohort::phase::empty || !storage->request_ || storage->size_ == 0u)
        { return {}; }
        // World passes its ACTUAL private prepared token and lexical drain
        // guard. A status bit/count/epoch/foreign maintenance cannot substitute.
        if(!g_runtime.execution_domain.current_prepared_drain(maintenance, prepared, drained)) { return {}; }
        {
            ::std::lock_guard request_lock{retirement::mutex_};
            if(!same_owner(retirement::actual_request_, storage->request_) ||
               storage->request_->phase != retirement::request_phase::active ||
               !runtime_checkpoint_host_bridge::current_coherent_state(storage->request_->configuration, storage->request_->closed))
            { return {}; }
        }
        // No domain admission/cohort/N/publication/request/registry lock spans
        // the OS wait. Every pending member, launch and old code/source remains
        // strongly owned by storage AND its real native registry.
        for(::std::size_t index{}; index != storage->size_; ++index)
        {
            auto const& actual{storage->members_[index]};
            if(!actual || !actual_tuple_current(*actual)) { return {}; }
            auto const result{join_actual(actual, deadline)};
            if(result.actual.status != ::fast_io::thread_join_status::joined) { return {}; }
        }
        if(!g_runtime.execution_domain.current_prepared_drain(maintenance, prepared, drained)) { return {}; }
        storage->phase_ = join_cohort::phase::joined;
        // Aliasing ownership pins the constructor-private receipt AND complete
        // old cohort without allocation or a separately forged physical bool.
        return actual_physical_join_owner{storage, ::std::addressof(storage->physical_)};
    }
    bool runtime_checkpoint_guest_worker_bridge::current_physical_join(pending_join_owner const& storage,
        actual_physical_join_owner const& physical, retirement::owner const& request) noexcept
    {
        if(!storage || !same_owner(storage->canonical_.lock(), storage) || !physical ||
           physical.get() != ::std::addressof(storage->physical_) || storage.owner_before(physical) || physical.owner_before(storage))
        { return false; }
        ::std::unique_lock state_lock{storage->mutex_, ::std::try_to_lock};
        if(!state_lock.owns_lock() || storage->phase_ != join_cohort::phase::joined || storage->size_ == 0u ||
           !same_owner(storage->request_, request)) { return false; }
        for(::std::size_t index{}; index != storage->size_; ++index)
        {
            auto const& actual{storage->members_[index]};
            if(!actual || !actual_tuple_current(*actual)) { return false; }
            ::std::unique_lock join_lock{actual->join_mutex_, ::std::try_to_lock};
            if(!join_lock.owns_lock() || !actual->physically_joined_) { return false; }
        }
        return true;
    }
#endif
}
#endif
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
extern "C++" llvm_jit_debug_guest_worker_launch_result runtime_launch_llvm_jit_debug_guest_worker_host_api(
    ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const& control,
    ::std::shared_ptr<void> state, llvm_jit_debug_guest_worker_body body) noexcept
{
#if defined(UWVM_RUNTIME_LLVM_JIT)
    return runtime_checkpoint_guest_worker_bridge::launch(control, ::std::move(state), body);
#else
    static_cast<void>(control); static_cast<void>(state); static_cast<void>(body); return {};
#endif
}
extern "C++" ::uwvm2::utils::thread::physical_join_result runtime_join_llvm_jit_debug_guest_worker_until_host_api(
    llvm_jit_debug_guest_worker_owner const& owner, ::std::chrono::steady_clock::time_point deadline) noexcept
{
#if defined(UWVM_RUNTIME_LLVM_JIT)
    return runtime_checkpoint_guest_worker_bridge::join_until(owner, deadline);
#else
    static_cast<void>(owner); static_cast<void>(deadline); return {{::fast_io::thread_join_status::unsupported, 0u}, false};
#endif
}
extern "C++" bool runtime_llvm_jit_debug_guest_worker_matches_control_host_api(
    llvm_jit_debug_guest_worker_owner const& owner, ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const& control) noexcept
{
#if defined(UWVM_RUNTIME_LLVM_JIT)
    return runtime_checkpoint_guest_worker_bridge::matches_control(owner, control);
#else
    static_cast<void>(owner); static_cast<void>(control); return false;
#endif
}
extern "C++" ::std::uint_least32_t runtime_llvm_jit_debug_registered_worker_exit_code_host_api() noexcept
{
#if defined(UWVM_RUNTIME_LLVM_JIT)
    return runtime_checkpoint_guest_worker_bridge::completion_code();
#else
    return 0u;
#endif
}
extern "C++" bool runtime_llvm_jit_debug_registered_worker_stopping_host_api() noexcept
{
#if defined(UWVM_RUNTIME_LLVM_JIT)
    return runtime_checkpoint_guest_worker_bridge::stopping_current();
#else
    return false;
#endif
}
#endif

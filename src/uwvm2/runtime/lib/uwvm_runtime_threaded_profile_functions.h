// Include INSIDE uwvm2::runtime::lib after the actual bridge class definition.
#if defined(__linux__) && defined(UWVM_USE_THREAD_LOCAL) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_MODULE) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE) && (defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT))
    inline ::std::int32_t runtime_threaded_profile_bridge::spawn_actual(lease& caller, ::std::uint32_t argument) noexcept
    {
        if(!actual_lease(caller, caller.world()) || caller.cancellation().stop_requested()) { return -1; }
        ::std::shared_ptr<profile_state> state;
        {
            ::std::unique_lock registry{control_owner().registry, ::std::try_to_lock};
            if(!registry.owns_lock()) { return -1; }
            state = control_owner().profile;
            if(!state || state->joined || state->native.get() != caller.group_ ||
               state->admitted.get() != &caller.admission()) { return -1; }
        }
        auto reservation{state->native->reserve()};
        if(!reservation) { return -1; }
        ::std::unique_ptr<concrete_compile_work> job;
        try { job = ::std::make_unique<concrete_compile_work>(state, false, 0u, argument, ::std::span<::std::byte const>{}); }
        catch(...) { return -1; }
        auto submitted{state->native->submit_compilation(::std::move(reservation), ::std::move(job))};
        if(!submitted.ticket) { return -1; }
        for(;;)
        {
            auto const observed{submitted.ticket.wait_until(::std::chrono::steady_clock::now() + ::std::chrono::milliseconds{100})};
            if(observed.status == group::compiler_wait_status::pending) { continue; }
            if(observed.status != group::compiler_wait_status::completed || caller.cancellation().stop_requested() ||
               observed.result.completion != group::compiler_completion::launched) { return -1; }
            // A positive tid exists only after actual pthread creation/launch.
            return observed.result.launched.tid;
        }
    }

    extern "C++" bool runtime_threaded_profile_configure_host_api(
        ::uwvm2::runtime::wasm_threads::threaded_profile_limits const& limits) noexcept
    { return runtime_threaded_profile_bridge::configure(limits); }
    extern "C++" bool runtime_threaded_profile_builtin_requested_host_api() noexcept
    { return runtime_threaded_profile_bridge::builtin_requested(); }
    extern "C++" threaded_policy_status runtime_threaded_profile_policy_host_api(
        ::uwvm2::runtime::wasm_threads::threaded_profile_limits& out) noexcept
    { return runtime_threaded_profile_bridge::policy(out); }
    extern "C++" threaded_management_exclusion runtime_threaded_profile_try_management_host_api(threaded_management_kind kind) noexcept
    { return runtime_threaded_profile_bridge::try_management(kind); }
    extern "C++" threaded_management_exclusion runtime_threaded_profile_try_management_under_publication_host_api(threaded_management_kind kind) noexcept
    { return runtime_threaded_profile_bridge::try_management_under_publication(kind); }
    extern "C++" bool runtime_threaded_profile_recheck_management_host_api(
        threaded_management_exclusion const& actual, threaded_management_kind kind) noexcept
    { return runtime_threaded_profile_bridge::recheck_management(actual, kind); }
    extern "C++" threaded_main_status runtime_threaded_profile_start_main_host_api(
        ::uwvm2::utils::container::u8string_view name, ::std::size_t entry,
        ::std::span<::std::byte const> parameters, ::std::size_t result_bytes) noexcept
    { return runtime_threaded_profile_bridge::start_main(name, entry, parameters, result_bytes); }
    extern "C++" threaded_retirement runtime_threaded_profile_retire_host_api(
        ::std::chrono::steady_clock::time_point deadline, threaded_management_kind purpose) noexcept
    { return runtime_threaded_profile_bridge::retire(deadline, purpose); }
    extern "C++" threaded_retirement runtime_threaded_profile_wait_host_api(::std::chrono::steady_clock::time_point deadline) noexcept
    { return runtime_threaded_profile_bridge::wait(deadline); }
    extern "C++" threaded_main_status runtime_threaded_profile_result_host_api(
        ::std::span<::std::byte> bytes, ::std::uint32_t* code, unsigned* kind) noexcept
    { return runtime_threaded_profile_bridge::result(bytes, code, kind); }
    namespace threaded_profile
    {
        inline runtime_module_storage_t const* active_host_module() noexcept
        { return runtime_threaded_profile_bridge::active_host_module(); }
        inline bool has_active_host_call() noexcept { return runtime_threaded_profile_bridge::has_active_host_call(); }
        inline bool active_host_stop_requested() noexcept { return runtime_threaded_profile_bridge::active_host_stop_requested(); }
#if defined(UWVM_IMPORT_ROS1) || defined(UWVM_IMPORT_ROS2)
        inline bool observed_caller(ros_caller_identity& out) noexcept { return runtime_threaded_profile_bridge::observed_caller(out); }
#endif
        inline bool memory_descriptor(::std::size_t index, preload_memory_descriptor_t* out) noexcept
        { return runtime_threaded_profile_bridge::memory_descriptor(index, out); }
        inline ::std::size_t memory_descriptor_count() noexcept { return runtime_threaded_profile_bridge::memory_descriptor_count(); }
        inline bool memory_copy(::std::size_t index, ::std::uint_least64_t offset, void* buffer, ::std::size_t bytes, bool write) noexcept
        { return runtime_threaded_profile_bridge::memory_copy(index, offset, buffer, bytes, write); }
        inline void request_stop() noexcept { runtime_threaded_profile_bridge::request_stop(); }
        inline threaded_retirement retire_before_management(::std::chrono::steady_clock::time_point deadline,
            threaded_management_kind purpose) noexcept
        { return runtime_threaded_profile_bridge::retire(deadline, purpose); }
        inline void after_actual_source_reset() noexcept { runtime_threaded_profile_bridge::after_actual_source_reset(); }
        inline void after_actual_source_reset(threaded_management_exclusion const& actual) noexcept
        { runtime_threaded_profile_bridge::after_actual_source_reset(actual); }
        inline void process_shutdown() noexcept { runtime_threaded_profile_bridge::process_shutdown(); }
        inline bool source_destruction_completed() noexcept { return runtime_threaded_profile_bridge::source_destruction_completed(); }
        inline bool source_destruction_completed(threaded_management_exclusion const& actual) noexcept
        { return runtime_threaded_profile_bridge::source_destruction_completed(actual); }
    }
#endif

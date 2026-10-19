#pragma once
// Included inside namespace uwvm2::runtime::lib after Runtime aliases. These
// cold Host/management hooks are declarations of the actual bridge, not user
// callbacks or default successful providers. Unsupported builds exclude them.
#if defined(__linux__) && defined(UWVM_USE_THREAD_LOCAL) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_MODULE) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE) && (defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT))
namespace threaded_profile
{
    [[nodiscard]] ::uwvm2::uwvm::runtime::storage::wasm_module_storage_t const* active_host_module() noexcept;
    [[nodiscard]] bool has_active_host_call() noexcept;
    [[nodiscard]] bool active_host_stop_requested() noexcept;
#if defined(UWVM_IMPORT_ROS1) || defined(UWVM_IMPORT_ROS2)
    [[nodiscard]] bool observed_caller(ros_caller_identity&) noexcept;
#endif
    [[nodiscard]] bool memory_descriptor(::std::size_t, ::uwvm2::uwvm::wasm::type::uwvm_preload_memory_descriptor_t*) noexcept;
    [[nodiscard]] ::std::size_t memory_descriptor_count() noexcept;
    [[nodiscard]] bool memory_copy(::std::size_t, ::std::uint_least64_t, void*, ::std::size_t, bool) noexcept;
    void request_stop() noexcept;
    [[nodiscard]] threaded_retirement retire_before_management(::std::chrono::steady_clock::time_point,
        threaded_management_kind) noexcept;
    void after_actual_source_reset() noexcept;
    void after_actual_source_reset(threaded_management_exclusion const&) noexcept;
    void process_shutdown() noexcept;
    [[nodiscard]] bool source_destruction_completed() noexcept;
    [[nodiscard]] bool source_destruction_completed(threaded_management_exclusion const&) noexcept;
}
#endif

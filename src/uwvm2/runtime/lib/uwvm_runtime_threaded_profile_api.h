#pragma once
#ifndef UWVM_MODULE
# include <uwvm2/utils/macro/push_macros.h>
# include <uwvm2/uwvm/runtime/macro/push_macros.h>
#endif
#if defined(__linux__) && defined(UWVM_USE_THREAD_LOCAL) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS) && !defined(UWVM_MODULE) && !defined(UWVM_TERMINATE_IMME_WHEN_PARSE) && (defined(UWVM_RUNTIME_UWVM_INTERPRETER) || defined(UWVM_RUNTIME_LLVM_JIT))
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <memory>
#include <mutex>
#include <thread>
#include <exception>
#include <uwvm2/runtime/wasm_threads/admission.h>
#include <uwvm2/utils/container/impl.h>
namespace uwvm2::uwvm::runtime::full { class full_source_instance; class full_source_destruction_completion; }

# define UWVM2_HAS_THREADED_PROFILE_HOST_API 1
namespace uwvm2::runtime::lib
{
    class runtime_threaded_profile_bridge;
    enum class threaded_main_status : unsigned { disabled, running, finished, rejected, retirement_pending };
    enum class threaded_policy_status : unsigned { disabled, configured, busy };
    enum class threaded_management_kind : unsigned { stop_only, full_source_reset, incremental_mutation };
    // Exclusive Native lifecycle ownership, not Source/restore/execution
    // authority. The issuer captures the actual selected source/control block
    // and generation itself under genuine maintenance/publication. External
    // callers cannot supply an owner, epoch, readiness bit or lock to mint it.
    class threaded_management_exclusion final
    {
        friend class runtime_threaded_profile_bridge;
        ::std::unique_lock<::std::mutex> actual_lock_;
        ::std::mutex* actual_mutex_{};
        ::std::weak_ptr<::uwvm2::uwvm::runtime::full::full_source_instance const> observed_source_;
        ::std::shared_ptr<::uwvm2::uwvm::runtime::full::full_source_destruction_completion const> source_completion_;
        ::std::uint_least64_t observed_generation_{};
        ::std::thread::id native_owner_{};
        threaded_management_kind purpose_{};
        bool requires_reset_{};
        threaded_management_exclusion(::std::unique_lock<::std::mutex> lock,
            ::std::shared_ptr<::uwvm2::uwvm::runtime::full::full_source_instance const> const& source,
            ::std::shared_ptr<::uwvm2::uwvm::runtime::full::full_source_destruction_completion const> completion,
            ::std::uint_least64_t generation, threaded_management_kind purpose, bool requires_reset) noexcept
            : actual_lock_{::std::move(lock)}, actual_mutex_{actual_lock_.mutex()}, observed_source_{source},
              source_completion_{::std::move(completion)}, observed_generation_{generation},
              native_owner_{::std::this_thread::get_id()}, purpose_{purpose}, requires_reset_{requires_reset} {}
    public:
        threaded_management_exclusion() noexcept = default;
        threaded_management_exclusion(threaded_management_exclusion const&) = delete;
        threaded_management_exclusion& operator=(threaded_management_exclusion const&) = delete;
        // Lexical, same-native-thread ownership. C++17 guaranteed prvalue
        // construction returns this without a movable mutex owner. Prepared
        // transactions obtain a NEW lexical exclusion on their commit thread.
        threaded_management_exclusion(threaded_management_exclusion&&) = delete;
        threaded_management_exclusion& operator=(threaded_management_exclusion&&) = delete;
        ~threaded_management_exclusion()
        {
            if(actual_lock_.owns_lock() && native_owner_ != ::std::this_thread::get_id()) { ::std::terminate(); }
        }
        [[nodiscard]] explicit operator bool() const noexcept
        { return actual_mutex_ != nullptr && actual_lock_.owns_lock() && actual_lock_.mutex() == actual_mutex_ &&
            native_owner_ == ::std::this_thread::get_id(); }
        [[nodiscard]] bool full_source_reset_required() const noexcept { return requires_reset_; }
        [[nodiscard]] ::std::uint_least64_t observed_generation() const noexcept { return observed_generation_; }
    };
    struct threaded_retirement
    {
        bool physically_joined{}, requires_full_source_reset{};
        ::std::size_t preparing{}, native_pending{};
        int native_error{};
        ::std::size_t compiler_tickets_pending{}, native_cleanup_pending{};
        [[nodiscard]] bool permits(threaded_management_kind kind) const noexcept
        { return physically_joined && native_error == 0 && preparing == 0u && native_pending == 0u &&
            compiler_tickets_pending == 0u && native_cleanup_pending == 0u &&
            (!requires_full_source_reset || kind != threaded_management_kind::incremental_mutation); }
    };
    // Host-only explicit positive finite policy. All-zero default grants no
    // spawn/profile; this has no Guest ABI, Restore-token or ROS grant input.
    extern "C++" [[nodiscard]] bool runtime_threaded_profile_configure_host_api(
        ::uwvm2::runtime::wasm_threads::threaded_profile_limits const&) noexcept;
    // Cold loader namespace selection only, never execution permission.
    extern "C++" [[nodiscard]] bool runtime_threaded_profile_builtin_requested_host_api() noexcept;
    extern "C++" [[nodiscard]] threaded_policy_status runtime_threaded_profile_policy_host_api(
        ::uwvm2::runtime::wasm_threads::threaded_profile_limits&) noexcept;
    extern "C++" [[nodiscard]] threaded_management_exclusion runtime_threaded_profile_try_management_host_api(
        threaded_management_kind) noexcept;
    extern "C++" [[nodiscard]] threaded_management_exclusion runtime_threaded_profile_try_management_under_publication_host_api(
        threaded_management_kind) noexcept;
    extern "C++" [[nodiscard]] bool runtime_threaded_profile_recheck_management_host_api(
        threaded_management_exclusion const&, threaded_management_kind) noexcept;
    extern "C++" [[nodiscard]] threaded_main_status runtime_threaded_profile_start_main_host_api(
        ::uwvm2::utils::container::u8string_view, ::std::size_t,
        ::std::span<::std::byte const>, ::std::size_t) noexcept;
    extern "C++" [[nodiscard]] threaded_retirement runtime_threaded_profile_retire_host_api(
        ::std::chrono::steady_clock::time_point, threaded_management_kind) noexcept;
    // Wait/reap actual native handles without requesting cancellation.
    extern "C++" [[nodiscard]] threaded_retirement runtime_threaded_profile_wait_host_api(
        ::std::chrono::steady_clock::time_point) noexcept;
    extern "C++" [[nodiscard]] threaded_main_status runtime_threaded_profile_result_host_api(
        ::std::span<::std::byte>, ::std::uint32_t*, unsigned*) noexcept;
}
#else
# define UWVM2_HAS_THREADED_PROFILE_HOST_API 0
#endif

#ifndef UWVM_MODULE
# include <uwvm2/uwvm/runtime/macro/pop_macros.h>
# include <uwvm2/utils/macro/pop_macros.h>
#endif

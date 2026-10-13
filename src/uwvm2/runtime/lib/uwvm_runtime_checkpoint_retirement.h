// Private actual-runtime retirement contract. Include INSIDE runtime::lib,
// after the host gate declaration and before full entry. No guest interface,
// wire token, signal injection, native-PC write or native-stack image exists.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
namespace { class runtime_checkpoint_thread_capture; }
extern "C++"
{
    class runtime_checkpoint_retirement_bridge final
    {
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_coherent_manager;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_guest_worker_bridge;
        using capture_owner = ::std::shared_ptr<runtime_checkpoint_thread_capture const>;
        using domain = ::uwvm2::utils::thread::cooperative_pause_domain;
        using gate = ::uwvm2::utils::thread::checkpoint_host_admission;
        enum class value_policy : unsigned char { executable_scalar, live_native_roots };
        enum class request_phase : unsigned char { prepared, activating, active };
        enum class member_phase : unsigned char { parked, signalled, native_frames_cleaned };
        struct member final
        {
            capture_owner captured{};
            member_phase phase{member_phase::parked};
        };
        struct request final
        {
            // All construction is private and occurs under the ONE real cohort,
            // closed host gate, actual GC exclusion and publication guard.
            // The retained closed guard must outlive old guest/provider RAII.
            runtime_checkpoint_host_bridge::owner const configuration;
            ::std::shared_ptr<domain> const control;
            ::uwvm2::runtime::checkpoint::compilation_profile::owner const profile;
            ::std::uint_least64_t const epoch;
            ::std::vector<member> members{};
            gate::closed_admission closed{};
            request_phase phase{request_phase::prepared}; // guarded by the real request mutex
            request(runtime_checkpoint_host_bridge::owner state, ::std::shared_ptr<domain> actual_control,
                ::uwvm2::runtime::checkpoint::compilation_profile::owner actual_profile,
                ::std::uint_least64_t actual_epoch) noexcept
                : configuration{::std::move(state)}, control{::std::move(actual_control)},
                  profile{::std::move(actual_profile)}, epoch{actual_epoch} {}
            request(request const&) = delete;
            request& operator=(request const&) = delete;
            request(request&&) = delete;
            request& operator=(request&&) = delete;
        };
        using owner = ::std::shared_ptr<request>;
        inline static ::std::mutex mutex_{};
        inline static owner actual_request_{};
        inline static ::std::atomic<bool> management_blocked_{};
        // Only selected resumable generated functions poll this bit. Ordinary
        // and observation-only engines emit no retirement call/probe/Invoke.
        inline static ::std::atomic<bool> requested_{};
        template<typename A, typename B>
        [[nodiscard]] static bool same_owner(A const& a, B const& b) noexcept
        { return a && b && a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a); }

        // Sole private manager seam. No public bool/epoch/pointer authorizes it.
        // Caller has already checked exact ticket provenance for every current
        // canonical capture, complete actual cohort and current native owners.
        // It must not resume any guest while its N/publication/cohort locks live.
        [[nodiscard]] static owner arm_current_while_cohort_owned(
            runtime_checkpoint_host_bridge::owner const&,
            ::std::span<capture_owner const>, ::std::span<domain::stopped_participant const>,
            gate::closed_admission&, value_policy = value_policy::executable_scalar) noexcept;
        // AFTER cohort, publication and N have all unlocked, while guests are
        // still parked. Close execution admission before waking that cohort.
        // Never request_stop under ONE cohort/pub/N or the request mutex.
        [[nodiscard]] static bool activate_after_cohort_unlock(owner const&) noexcept;
        // Failed preparation may release an unactivated request only. Once
        // activating begins, old leases must retire through actual drain.
        [[nodiscard]] static bool cancel_unactivated_after_cohort_unlock(owner const&) noexcept;
        // Called only inside the actual execution-domain drain/reset callback,
        // after the manager's own lease and ALL old guest/provider leases left.
        // It does not mint new-epoch/source/whole-instance restore authority.
        enum class drain_result : unsigned char { invalid_request, all_frames_signalled, actual_drain_without_all_signals };
        [[nodiscard]] static drain_result release_after_actual_execution_drain(owner const&) noexcept;
    public:
        // Cold NEGATIVE management gate only. False grants no publication,
        // owner, cohort or restore permission. This prevents a concurrent old
        // debug replacement commit after the actual retirement cohort sealed.
        [[nodiscard]] static bool management_blocked() noexcept
        { return management_blocked_.load(::std::memory_order_acquire); }
        class signal final
        {
            friend class runtime_checkpoint_retirement_bridge;
            owner request_{};
            ::std::size_t member_{};
            signal(owner actual, ::std::size_t member) noexcept : request_{::std::move(actual)}, member_{member} {}
        public:
            signal(signal const&) noexcept = default;
            signal(signal&&) noexcept = default;
            ~signal() = default;
        };
        // Separate potentially throwing bridge AFTER the existing noexcept
        // pause/observer bridge returns. No naked native-step wrapper is crossed
        // by this signal; emitted LLVM Invoke owns real cleanup-only unwinding.
        static void poll_current_after_park(::std::uint64_t incarnation,
            ::std::uintptr_t module, ::std::uintptr_t function, ::std::size_t opcode_offset) UWVM_THROWS;
        // Actual full host raw-entry catch only, before its noexcept boundary.
        // The signal has a private constructor and matches the actual installed
        // request/participant. Native debug/GC/logical cleanup has already run.
        [[nodiscard]] static bool accept_at_full_entry(signal const&) noexcept;
    };
}
#endif

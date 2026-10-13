/*************************************************************
 * UlteSoft WebAssembly Virtual Machine (Version 2)          *
 * Copyright (c) 2025-present UlteSoft. All rights reserved. *
 * Licensed under the APL-2.0 License (see LICENSE file).    *
 *************************************************************/
// Private actual native guest owner. Include INSIDE runtime::lib after the
// host/retirement declarations. No guest, wire, native-handle or attach API.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
namespace { class runtime_debug_execution_scope; class runtime_execution_entry_scope; }
extern "C++"
{
    class runtime_checkpoint_guest_worker_bridge;
    class runtime_checkpoint_world_transaction;
}
    // Its first public forward declaration belongs to the runtime API module.
    // Keep that attachment, like the existing debug source/shutdown owner.
    class llvm_jit_debug_guest_worker final
    {
        friend class runtime_checkpoint_guest_worker_bridge;
        using domain = ::uwvm2::utils::thread::cooperative_pause_domain;
        using source_type = ::uwvm2::uwvm::runtime::full::full_source_instance;
        using gate = ::uwvm2::utils::thread::checkpoint_host_admission;
        ::std::shared_ptr<domain> const control_;
        source_type::owner const source_;
        ::uwvm2::runtime::checkpoint::compilation_profile::owner const profile_;
        runtime_checkpoint_host_bridge::owner const host_;
        ::std::uint_least64_t const epoch_;
        ::std::shared_ptr<void> launch_state_;
        llvm_jit_debug_guest_worker_body const body_;
        enum class origin : unsigned char { initial_current, restored_world };
        origin const origin_;
        // Entire launch body, including intervals BETWEEN real runtime entries,
        // pins the actual old generation. This is not physical/TLS retirement.
        ::uwvm2::utils::thread::execution_domain::lease execution_{};
        gate::host_operation startup_{};
        ::std::unique_ptr<::fast_io::native_thread> native_{};
        ::std::atomic<bool> published_{};
        ::std::mutex join_mutex_{};
        // Registry mutex guards the live genuine scope binding. This pointer is
        // never exposed or dereferenced by a manager; domain enrollment supplies
        // the actual identifier. A stored capture cannot replace this producer.
        domain::participant const* participant_{};
        ::std::uint_least64_t participant_id_{};
        bool physically_joined_{}; // join mutex; written only on actual OS joined
        bool entry_refused_{}; // this actual worker only; negative graph stop
        bool guest_exit_{}; // actual private terminal unwind, not physical join
        ::std::uint_least32_t guest_exit_code_{};
        llvm_jit_debug_guest_worker(::std::shared_ptr<domain> control,
            source_type::owner source, ::uwvm2::runtime::checkpoint::compilation_profile::owner profile,
            runtime_checkpoint_host_bridge::owner host, ::std::uint_least64_t epoch,
            ::std::shared_ptr<void> state, llvm_jit_debug_guest_worker_body body,
            origin selected_origin = origin::initial_current) noexcept
            : control_{::std::move(control)}, source_{::std::move(source)}, profile_{::std::move(profile)},
              host_{::std::move(host)}, epoch_{epoch}, launch_state_{::std::move(state)}, body_{body}, origin_{selected_origin} {}
    public:
        // Canonical registry owns every live native owner. Pending never reaches
        // this destructor by dropping an external owner. Completed objects own
        // a genuinely reaped/nonjoinable wrapper, not a detached substitution.
        ~llvm_jit_debug_guest_worker() = default;
        llvm_jit_debug_guest_worker(llvm_jit_debug_guest_worker const&) = delete;
        llvm_jit_debug_guest_worker& operator=(llvm_jit_debug_guest_worker const&) = delete;
    };
extern "C++"
{
    class runtime_checkpoint_guest_worker_bridge final
    {
        friend class runtime_debug_execution_scope;
        friend class runtime_execution_entry_scope;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_coherent_manager;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_world_transaction;
        using worker = llvm_jit_debug_guest_worker;
        using mutable_owner = ::std::shared_ptr<worker>;
        using domain = ::uwvm2::utils::thread::cooperative_pause_domain;
        using execution_domain = ::uwvm2::utils::thread::execution_domain;
        using source_type = ::uwvm2::uwvm::runtime::full::full_source_instance;
        struct registry_slot
        {
            // Both smart pointers default-construct empty. Avoid nested default
            // member initializers before the enclosing static array is complete.
            mutable_owner live; // keeps actual thread/code/state on every NACK
            ::std::weak_ptr<worker> canonical; // completed retry identity too
        };
        inline static ::std::mutex registry_mutex_{};
        inline static registry_slot registry_[256u]{};
        inline static constinit thread_local worker* actual_worker_{};
        template<typename A, typename B>
        [[nodiscard]] static bool same_owner(A const& a, B const& b) noexcept
        { return a && b && a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a); }
        [[nodiscard]] static mutable_owner resolve(llvm_jit_debug_guest_worker_owner const&) noexcept;
        static void actual_worker_body(mutable_owner) noexcept;
        static void scope_admitted(domain::participant const&) noexcept;
        static void scope_leaving(domain::participant const&) noexcept;
        // Genuine initial trampoline only, after normal admission REFUSES.
        // Move the SAME real launch lease into the entry's existing first member
        // for its negative return, then back before member teardown; its actual
        // generation admission never drops. No supplied pointer/token/bool.
        [[nodiscard]] static execution_domain::lease take_current_initial_execution() noexcept;
        static void return_current_initial_execution(execution_domain::lease&, bool actually_refused) noexcept;
        [[nodiscard]] static bool note_actual_initial_host_refusal() noexcept;
        [[nodiscard]] static bool actual_tuple_current(worker const&) noexcept;
        [[nodiscard]] static ::uwvm2::utils::thread::physical_join_result join_actual(
            mutable_owner const&, ::std::chrono::steady_clock::time_point) noexcept;
    public:
        // Implemented by the actual factory, not caller registration/attachment.
        [[nodiscard]] static llvm_jit_debug_guest_worker_launch_result launch(
            ::std::shared_ptr<domain> const&, ::std::shared_ptr<void>, llvm_jit_debug_guest_worker_body) noexcept;
        [[nodiscard]] static ::uwvm2::utils::thread::physical_join_result join_until(
            llvm_jit_debug_guest_worker_owner const&, ::std::chrono::steady_clock::time_point) noexcept;
        [[nodiscard]] static bool matches_control(llvm_jit_debug_guest_worker_owner const&,
            ::std::shared_ptr<domain> const&) noexcept;
        [[nodiscard]] static bool stopping_current() noexcept;
        [[nodiscard]] static ::std::uint_least32_t completion_code() noexcept;
#if defined(UWVM_CPP_EXCEPTIONS)
        class guest_exit_signal final
        {
            friend class runtime_checkpoint_guest_worker_bridge;
            worker const* actual_{}; // comparison only, actual trampoline pins it
            ::std::uint_least64_t participant_{}, epoch_{};
            ::std::uint_least32_t code_{};
            guest_exit_signal(worker const* actual, ::std::uint_least64_t participant,
                ::std::uint_least64_t epoch, ::std::uint_least32_t code) noexcept
                : actual_{actual}, participant_{participant}, epoch_{epoch}, code_{code} {}
        public:
            guest_exit_signal(guest_exit_signal const&) noexcept = default;
            ~guest_exit_signal() = default;
        };
        // Only a real owned initial worker and its canonical builtin cache can
        // turn WASIp1 proc_exit into this private terminal native unwind.
        static void try_builtin_proc_exit(::std::size_t module, ::std::size_t import_index,
            void const* expected_cache, ::std::byte const* params, ::std::size_t bytes) UWVM_THROWS;
        [[nodiscard]] static bool accept_guest_exit(guest_exit_signal const&) noexcept;
#endif
    private:
#if defined(UWVM_CPP_EXCEPTIONS)
        class restored_world_launch_key final
        {
            friend class runtime_checkpoint_guest_worker_bridge;
            restored_world_launch_key() = default;
        };
        // The late definition lives after the authentic world and dispatcher.
        // It must resolve the actual installed world BEFORE supplied reads and
        // use the world-owned closed startup token. No initial factory reuse.
        [[nodiscard]] static mutable_owner launch_actual_restored_world(
            ::std::shared_ptr<runtime_checkpoint_world_transaction> const&, ::std::size_t) noexcept;
        [[nodiscard]] static mutable_owner construct_registered_restored_worker(
            source_type::owner const&,
            ::uwvm2::runtime::checkpoint::compilation_profile::owner const&,
            ::std::shared_ptr<domain> const&, runtime_checkpoint_host_bridge::owner const&,
            ::std::uint_least64_t, ::std::shared_ptr<void>, llvm_jit_debug_guest_worker_body,
            restored_world_launch_key) noexcept;
        // Cold private checks used by the authentic world's real startup
        // coordinator. Expected state/participant are comparison DATA only;
        // actual trampoline and live producer binding supply the ownership.
        [[nodiscard]] static bool current_restored_launch_state(void const*) noexcept;
        [[nodiscard]] static bool current_restored_worker(mutable_owner const&,
            void const*, ::std::uint_least64_t) noexcept;
        using retirement = runtime_checkpoint_retirement_bridge;
        struct join_cohort;
        class actual_physical_join final
        {
            friend class runtime_checkpoint_guest_worker_bridge;
            friend struct join_cohort;
            join_cohort* actual_{};
            explicit actual_physical_join(join_cohort& actual) noexcept : actual_{::std::addressof(actual)} {}
        };
        struct join_cohort final
        {
            friend class runtime_checkpoint_guest_worker_bridge;
        private:
            ::std::weak_ptr<join_cohort> canonical_{};
            retirement::owner request_{};
            mutable_owner members_[256u]{};
            ::std::size_t size_{};
            enum class phase : unsigned char { empty, sealed, joined } phase_{};
            actual_physical_join physical_{*this};
            ::std::mutex mutex_{}; // one manager joins; never held by guest body
            join_cohort() = default;
        public:
            ~join_cohort() = default;
        };
        using pending_join_owner = ::std::shared_ptr<join_cohort>;
        using actual_physical_join_owner = ::std::shared_ptr<actual_physical_join const>;
        // Fallible preallocation precedes old retirement. Empty storage has no
        // permission. The sole manager fills it only from the authentic request
        // that already owns ONE complete cohort and the still-closed host gate.
        [[nodiscard]] static pending_join_owner prepare_join_storage() noexcept;
        [[nodiscard]] static bool seal_from_actual_retirement_while_cohort_owned(
            pending_join_owner const&, retirement::owner const&) noexcept;
        [[nodiscard]] static actual_physical_join_owner join_after_actual_drain_until(
            pending_join_owner const&, execution_domain::maintenance_transition const&,
            execution_domain::prepared_generation const&, execution_domain::drained_generation const&,
            ::std::chrono::steady_clock::time_point) noexcept;
        // Lower adoption rechecks this outside domain admission/publication.
        // No caller roster, physical bool or thread ID enters this method.
        [[nodiscard]] static bool current_physical_join(pending_join_owner const&,
            actual_physical_join_owner const&, retirement::owner const&) noexcept;
#endif
    };
}
#endif

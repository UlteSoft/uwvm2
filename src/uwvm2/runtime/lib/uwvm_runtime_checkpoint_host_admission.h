// Private runtime leaf, included INSIDE uwvm2::runtime::lib before runtime_global_state.
// Merely including this file emits no ordinary guest IR, memory guard or TLS probe.
// This is a host-operation gate, NEVER a source/world-stop/registry/restore credential.
#pragma once
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
extern "C++"
{
    class runtime_checkpoint_coherent_manager; // Same actual nonexported global-module attachment.
    class runtime_checkpoint_retirement_bridge;
    class runtime_checkpoint_guest_worker_bridge;
    class runtime_checkpoint_host_bridge final
    {
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_coherent_manager;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_retirement_bridge;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_guest_worker_bridge;
        using gate_type = ::uwvm2::utils::thread::checkpoint_host_admission;
        struct retained_state final
        {
            friend class runtime_checkpoint_host_bridge;
            friend class ::uwvm2::runtime::lib::runtime_checkpoint_coherent_manager;
            friend class ::uwvm2::runtime::lib::runtime_checkpoint_retirement_bridge;
        friend class ::uwvm2::runtime::lib::runtime_checkpoint_guest_worker_bridge;
        private:
            ::uwvm2::runtime::checkpoint::compilation_profile::owner const profile_;
            ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const control_;
            ::std::uint_least64_t const epoch_;
            ::std::unique_ptr<gate_type> const gate_;
            retained_state(::uwvm2::runtime::checkpoint::compilation_profile::owner profile,
                ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> control,
                ::std::uint_least64_t epoch, ::std::unique_ptr<gate_type> gate) noexcept
                : profile_{::std::move(profile)}, control_{::std::move(control)}, epoch_{epoch}, gate_{::std::move(gate)} {}
        public:
            // shared_ptr owns destruction; only the actual runtime bridge may construct.
            ~retained_state() = default;
        };
    public:
        // The pointee constructor is private. The runtime never accepts a public
        // shared_ptr alias/caller owner as proof: only its own atomic slot issues it.
        using owner = ::std::shared_ptr<retained_state const>;
        using status = ::uwvm2::utils::thread::checkpoint_host_admission_status;
        class entry_scope final
        {
            friend class runtime_checkpoint_host_bridge;
            // State and profile survive the operation; the actual runtime entry
            // separately keeps the execution lease alive until AFTER this member.
            owner state_{};
            gate_type::host_operation setup_{};
            entry_scope* previous_{};
            ::std::size_t depth_{};
            bool linked_{};
        public:
            entry_scope() noexcept = default; // Empty conveys no permission.
            entry_scope(entry_scope const&) = delete;
            entry_scope& operator=(entry_scope const&) = delete;
            entry_scope(entry_scope&&) = delete;
            entry_scope& operator=(entry_scope&&) = delete;
            ~entry_scope();
        };
        class foreign_operation_scope final
        {
            friend class runtime_checkpoint_host_bridge;
            // Release the counted operation before releasing its actual state.
            owner state_{};
            gate_type::host_operation operation_{};
            status status_{status::invalid_context};
        public:
            // Only checkpoint-selected native dispatch creates this scope. No
            // caller module, descriptor, profile bool or serialized token is read.
            foreign_operation_scope() noexcept;
            foreign_operation_scope(foreign_operation_scope const&) = delete;
            foreign_operation_scope& operator=(foreign_operation_scope const&) = delete;
            foreign_operation_scope(foreign_operation_scope&&) = delete;
            foreign_operation_scope& operator=(foreign_operation_scope&&) = delete;
            [[nodiscard]] bool admitted() const noexcept
            { return status_ == status::ok && static_cast<bool>(operation_); }
            [[nodiscard]] status result() const noexcept { return status_; } // Diagnostic only.
        };
        // Called only by the real setter under quiescent execution admission and
        // publication. No native callback/compile occurs while those locks live.
        [[nodiscard]] static owner create_for_actual_configuration(
            ::uwvm2::runtime::checkpoint::compilation_profile::owner const&) noexcept;
        // Cold public execution entry only: ONE enabled read when ordinary.
        // Must run after real lease/depth admission, before cache/target/enrollment.
        [[nodiscard]] static status begin_actual_entry(entry_scope&) noexcept;
        // Called AFTER runtime_debug_execution_scope genuinely enrolls/reuses its
        // actual participant. Preparing code without a guest retains setup_ to exit.
        static void actual_participant_admitted(entry_scope&) noexcept;
    private:
        // Factory owns the genuine startup lease; derived installed state only.
        [[nodiscard]] static status begin_registered_worker_startup(owner const&, gate_type::host_operation&) noexcept;
        inline static constinit thread_local entry_scope* actual_entry_{};
        [[nodiscard]] static bool matches_actual_state(entry_scope const&) noexcept;
        [[nodiscard]] static status enter_actual_foreign_operation(foreign_operation_scope&) noexcept;
        // Sole private manager: actual lease + ONE cooperative roster required;
        // these methods never accept a bool, epoch, FD or public census credential.
        [[nodiscard]] static status try_close_coherent_state(owner const&, gate_type::closed_admission&) noexcept;
        [[nodiscard]] static bool current_coherent_state(owner const&, gate_type::closed_admission const&) noexcept;
    };
}
#endif

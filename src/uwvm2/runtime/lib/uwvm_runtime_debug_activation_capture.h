// Runtime-private, immutable exact-event capture. No public constructor;
// control-block identity is checked against the private bounded registry before
// any caller-provided shared_ptr is dereferenced. Owns only copies/ticket/control,
// never an executable stack address or an independent executable-code pin.
#if defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    class llvm_jit_debug_activation_capture final
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
        llvm_jit_debug_activation_capture() = default;
        friend class llvm_jit_debug_native_activation_cursor;
        friend class llvm_jit_debug_native_call_continuation;
        friend class llvm_jit_debug_native_return_continuation;
        // Qualified function friends nominate the real global-module APIs
        // first declared with C++ linkage in runtime.h. This class keeps its
        // runtime module attachment; no new friend overload is introduced.
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_mint_native_activation_host_api(
            llvm_jit_debug_activation_capture_owner const&, void const*) noexcept -> llvm_jit_debug_native_activation_cursor_owner;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_native_caller_host_api(
            llvm_jit_debug_native_activation_cursor_owner const&, void const*, llvm_jit_debug_native_caller_view&) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_native_backtrace_host_api(
            llvm_jit_debug_native_activation_cursor_owner const&, void const*, llvm_jit_debug_native_backtrace_view&, ::std::size_t) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_native_activation_host_api(
            llvm_jit_debug_native_activation_cursor_owner const&, void const*) noexcept -> bool;
        ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> control_{};
        ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket ticket_{};
        llvm_jit_debug_activation_snapshot snapshot_{};
        // Immutable native-only parent identity, separately anchored to the
        // original actual domain ticket. Never a cooperative/source capture.
        llvm_jit_debug_activation_capture_owner native_return_anchor_{};
        llvm_jit_debug_native_step_site native_site_{}; // runtime-private actual before-park native owner/PC
        llvm_jit_debug_native_code_site native_code_site_{}; // private authentic true-endpoint cooperative code origin
        ::std::shared_ptr<details::debug_activation::ledger const> native_ledger_owner_{}; // actual dynamic ledger control block; never a code/stack pin
        details::debug_native_stack::owner native_stack_owner_{}; // actual worker scope; no public stack permission
        ::std::vector<void const*> code_owners_{}; // comparison-only actual full/retained code-owner borrows
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_capture_activation_host_api(
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&) noexcept -> llvm_jit_debug_activation_capture_owner;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_query_activation_host_api(
            llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_activation_snapshot&) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_copy_source_object_host_api(
            llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_source_binding_owner const&,
            ::std::uint_least64_t, ::std::size_t, ::std::uint_least8_t, llvm_jit_debug_source_object_copy&) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_query_source_activation_host_api(
            llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_source_binding_owner const&,
            llvm_jit_debug_source_activation_snapshot&) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_copy_native_code_host_api(
            llvm_jit_debug_activation_capture_owner const&, void const*, llvm_jit_debug_native_code_bytes&) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_copy_native_function_host_api(
            llvm_jit_debug_activation_capture_owner const&, void const*, llvm_jit_debug_native_function_image&) noexcept -> bool;
        // The position query verifies this exact private capture, execution
        // generation and real trap; no caller-supplied PC or field is accepted.
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_native_position_host_api(
            llvm_jit_debug_activation_capture_owner const&, void const*, llvm_jit_debug_native_position&) noexcept -> bool;
        // Publication-guarded validation of the real stored event chain. Never
        // accepts a caller-supplied chain or independently manufactures a pin.
        [[nodiscard]] bool matches_publication_locked(
            ::uwvm2::utils::thread::cooperative_pause_location, bool native = false) const noexcept;
        [[nodiscard]] bool matches_native_publication_locked(
            ::uwvm2::utils::thread::cooperative_pause_location& actual, bool external) const noexcept
        {
            if((native_return_anchor_ && !external) || !matches_publication_locked(actual, true)) { return false; }
            actual = snapshot_.location; return true;
        }
        // Called only inside the real native snapshot gate. Bounds alone do
        // not identify a recursive activation; require its registered witness.
        [[nodiscard]] bool matches_native_cursor_locked(void const*, ::std::uint_least64_t,
            ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t, ::std::uintptr_t) const noexcept;
    public:
        llvm_jit_debug_activation_capture(llvm_jit_debug_activation_capture const&) = delete;
        llvm_jit_debug_activation_capture& operator=(llvm_jit_debug_activation_capture const&) = delete;
    };
#endif

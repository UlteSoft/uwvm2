// Included inside uwvm2::runtime::lib after the native-only API declarations.
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD)
    class llvm_jit_debug_source_binding final
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
        // Qualified function friends nominate the real global-module APIs
        // first declared with C++ linkage in runtime.h. This class keeps its
        // runtime module attachment; no new friend overload is introduced.
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_bind_source_host_api(::std::size_t) noexcept -> llvm_jit_debug_source_binding_owner;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_copy_source_image_host_api(llvm_jit_debug_source_binding_owner const&,
            llvm_jit_debug_source_image&) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_source_position_host_api(llvm_jit_debug_source_binding_owner const&,
            ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const&,
            ::uwvm2::utils::thread::cooperative_pause_domain::pause_ticket const&,
            ::std::uint_least64_t, llvm_jit_debug_source_position&) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_copy_source_object_host_api(
            llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_source_binding_owner const&,
            ::std::uint_least64_t, ::std::size_t, ::std::uint_least8_t, llvm_jit_debug_source_object_copy&) noexcept -> bool;
        friend auto ::uwvm2::runtime::lib::llvm_jit_debug_query_source_activation_host_api(
            llvm_jit_debug_activation_capture_owner const&, llvm_jit_debug_source_binding_owner const&,
            llvm_jit_debug_source_activation_snapshot&) noexcept -> bool;
        // Caller already owns a real execution lease and publication guard.
        // Compare public owner/control block before dereferencing any supplied
        // pointer; only return the runtime's privately published canonical copy.
        [[nodiscard]] static llvm_jit_debug_source_binding_owner canonical_locked(
            llvm_jit_debug_source_binding_owner const&) noexcept;
        [[nodiscard]] static bool position_locked(llvm_jit_debug_source_binding_owner const&,
            ::uwvm2::utils::thread::cooperative_pause_location, llvm_jit_debug_source_position&) noexcept;
        ::uwvm2::uwvm::runtime::full::full_source_instance::owner source_{};
        // Comparison-only private borrow. No cast/dereference grants authority;
        // the real generation lease and locked record retain/check this owner.
        void const* publication_{};
        ::std::size_t module_id_{};
        ::std::uint_least64_t runtime_epoch_{};
        llvm_jit_debug_source_image image_{};
        llvm_jit_debug_source_binding() = default;
    public:
        llvm_jit_debug_source_binding(llvm_jit_debug_source_binding const&) = delete;
        llvm_jit_debug_source_binding& operator=(llvm_jit_debug_source_binding const&) = delete;
        ~llvm_jit_debug_source_binding() = default;
    };
#endif

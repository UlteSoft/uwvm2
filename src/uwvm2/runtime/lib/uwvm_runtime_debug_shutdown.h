#pragma once
// Private process-local managed CLI stop contract. Include INSIDE runtime::lib
// before full native entry. No guest import, saved state or public constructor.
#if defined(UWVM_RUNTIME_LLVM_JIT) && defined(UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD) && defined(UWVM_CPP_EXCEPTIONS)
class runtime_debug_shutdown_bridge;
class llvm_jit_debug_shutdown_request final
{
    friend class runtime_debug_shutdown_bridge;
    using domain = ::uwvm2::utils::thread::cooperative_pause_domain;
    using source_owner = ::uwvm2::uwvm::runtime::full::full_source_instance::owner;
    struct module_pin
    {
        source_owner source{};
        void const* publication{}; // comparison borrow; actual maintenance pins g_runtime
        ::std::size_t function_count{}; // immutable parsed declaration count
        // Current body generations are resolved by the genuine RUNNING guest
        // activation below. Never read an ALL-park mutable vector in begin().
    };
    mutable ::std::mutex operation_{};
    mutable ::uwvm2::utils::thread::execution_domain::maintenance_transition maintenance_{};
    ::std::shared_ptr<domain> control_{};
    llvm_jit_debug_observer observer_{};
    ::std::uint_least64_t epoch_{}, issuer_{}; // actual cold native-thread incarnation, never reused
    ::std::vector<module_pin> modules_{};
    explicit llvm_jit_debug_shutdown_request(
        ::uwvm2::utils::thread::execution_domain::maintenance_transition owned) noexcept
        : maintenance_{::std::move(owned)} {}
public:
    llvm_jit_debug_shutdown_request(llvm_jit_debug_shutdown_request const&) = delete;
    llvm_jit_debug_shutdown_request& operator=(llvm_jit_debug_shutdown_request const&) = delete;
    ~llvm_jit_debug_shutdown_request() = default;
};
class runtime_debug_shutdown_bridge final
{
    using owner = llvm_jit_debug_shutdown_request_owner;
    using result = llvm_jit_debug_shutdown_status;
    inline static ::std::mutex mutex_{};
    inline static owner actual_request_{}; // timeout/caller-drop never release actual ownership
    inline static ::std::atomic<bool> requested_{};
    inline static ::std::atomic<::std::uint_least64_t> next_issuer_{};
    inline static thread_local ::std::uint_least64_t actual_issuer_{};
    [[nodiscard]] static ::std::uint_least64_t issuer() noexcept;
    inline static thread_local bool catchable_full_entry_{};
    template<typename A, typename B>
    [[nodiscard]] static bool same_owner(A const& a, B const& b) noexcept
    { return a && b && a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a); }
    [[nodiscard]] static bool management_entry() noexcept;
public:
    [[nodiscard]] static bool management_blocked() noexcept
    { return requested_.load(::std::memory_order_acquire); }
    // Cold trusted compiler borrow: process-lifetime atomic storage only.
    // No debugger request obtains this pointer or a native memory capability.
    [[nodiscard]] static void const* requested_address() noexcept
    { return ::std::addressof(requested_); }
    class full_entry_scope final
    {
        bool const previous_{catchable_full_entry_};
    public:
        full_entry_scope() noexcept { catchable_full_entry_ = true; }
        full_entry_scope(full_entry_scope const&) = delete;
        ~full_entry_scope() { catchable_full_entry_ = previous_; }
    };
    class signal final
    {
        friend class runtime_debug_shutdown_bridge;
        owner request_{};
        ::std::uint_least64_t participant_{};
        signal(owner actual, ::std::uint_least64_t participant) noexcept
            : request_{::std::move(actual)}, participant_{participant} {}
    public:
        signal(signal const&) noexcept = default;
        signal(signal&&) noexcept = default;
        ~signal() = default;
    };
    [[nodiscard]] static llvm_jit_debug_shutdown_start begin(
        ::std::shared_ptr<::uwvm2::utils::thread::cooperative_pause_domain> const&) noexcept;
    [[nodiscard]] static result poll(owner const&, ::std::uint_least64_t wait_milliseconds) noexcept;
    [[nodiscard]] static bool release(owner const&) noexcept;
    [[nodiscard]] static bool skip_cancelled_entry() noexcept;
    static void poll_after_safe_point(::std::uint64_t incarnation, ::std::uintptr_t module,
        ::std::uintptr_t function, ::std::size_t opcode_offset) UWVM_THROWS;
    [[nodiscard]] static bool accept_at_full_entry(signal const&) noexcept;
};
#endif

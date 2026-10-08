// Actual kernel component witness only. This does not authorize a Wasm stop,
// issue a runtime cursor, or enable NI/finish in a production debugger.
#define UWVM_DEBUG_NATIVE_CONTINUATION_COMPONENT_TEST 1
#include <uwvm2/uwvm/debugger/native_continuation_linux.h>
#include <uwvm2/uwvm/debugger/native_step.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <fast_io.h>
#if defined(__linux__) && defined(__x86_64__) && __SIZEOF_POINTER__ == 8 && \
    defined(PERF_ATTR_SIZE_VER7) && !defined(__ILP32__)
#include <ucontext.h>
namespace dbg = ::uwvm2::uwvm::debugger;
namespace event = dbg::native_continuation_linux;
namespace uwvm2::uwvm::debugger::native_continuation_linux
{
    struct trusted_component_probe
    {
        static bool open(event_owner& owner, ::std::uint64_t thread,
            ::std::uintptr_t target, ::std::uint64_t cookie) noexcept
        { return owner.open_disabled(thread, target, cookie); }
        static bool enable(event_owner& owner) noexcept { return owner.enable(); }
        static int descriptor(event_owner const& owner) noexcept { return owner.file_.native_handle(); }
        static void retire(event_owner& owner) noexcept { owner.disable_close(); }
    };
}
static constexpr ::std::uint64_t cookie{0x00002304001403u};
static ::std::uintptr_t target{};
static volatile ::std::sig_atomic_t active_descriptor{-1}, hits{}, body_called{}, wrong_context{}, blocked_probe{};
static_assert(::std::atomic<::std::uint64_t>::is_always_lock_free);
static ::std::atomic<::std::uint64_t> actual_worker{};
static ::std::atomic<unsigned> gate{};
[[gnu::noinline]] static void continuation_body() noexcept
{
    // The hardware execute breakpoint must fire BEFORE this first effect.
    body_called = body_called + 1;
}
static void fail(char const* message) noexcept
{
    ::fast_io::io::perrln("native_continuation_linux_event: ", ::fast_io::mnp::os_c_str(message));
    ::fast_io::fast_terminate();
}
static void check(bool yes, char const* message) noexcept { if(!yes) { fail(message); } }
static void handler(int number, ::siginfo_t* info, void* opaque) noexcept
{
    auto const payload{dbg::native_perf_signal_linux::read(info)};
    if(number != SIGTRAP || opaque == nullptr || !payload.valid ||
       payload.type != PERF_TYPE_BREAKPOINT || payload.cookie != cookie ||
       reinterpret_cast<::std::uintptr_t>(info->si_addr) != target)
    { dbg::posix_abi::_exit_noexcept(94); }
    auto const& context{*static_cast<::ucontext_t const*>(opaque)};
    auto const pc{static_cast<::std::uintptr_t>(context.uc_mcontext.gregs[REG_RIP])};
    auto const flags{static_cast<::std::uintptr_t>(context.uc_mcontext.gregs[REG_EFL])};
    auto const thread{dbg::native_step::details::raw_system_call(SYS_gettid)};
    if((flags & 0x100u) != 0u ||
       static_cast<::std::uint64_t>(thread) != actual_worker.load(::std::memory_order_relaxed)) { wrong_context = 1; }
    if(blocked_probe)
    {
        // This is actual DEFERRED kernel delivery, not a precise Wasm/native
        // stop. Its saved PC must never authorize the production continuation.
        if((payload.flags & dbg::native_perf_signal_linux::asynchronous_flag) == 0u || body_called != 1) { wrong_context = 1; }
    }
    else if(pc != target || payload.flags != 0u || body_called != 0) { wrong_context = 1; }
    hits = hits + 1;
    // No C++ IO, allocation, mutex, original code read or PC/TF mutation in
    // the signal handler. Disable the owned event with the direct kernel ABI.
    if(dbg::native_step::details::raw_system_call(SYS_ioctl, active_descriptor,
        static_cast<long>(PERF_EVENT_IOC_DISABLE), 0u) != 0)
    { dbg::posix_abi::_exit_noexcept(95); }
}
struct action_owner
{
    struct ::sigaction previous{};
    bool installed{};
    bool install() noexcept
    {
        struct ::sigaction next{};
        next.sa_sigaction = handler; next.sa_flags = SA_SIGINFO;
        dbg::posix_abi::sigemptyset_noexcept(::std::addressof(next.sa_mask));
        if(dbg::posix_abi::sigaction_noexcept(SIGTRAP, ::std::addressof(next), ::std::addressof(previous)) != 0) { return false; }
        installed = true; return true;
    }
    ~action_owner()
    {
        if(installed && dbg::posix_abi::sigaction_noexcept(SIGTRAP, ::std::addressof(previous), nullptr) != 0)
        { dbg::posix_abi::_exit_noexcept(96); }
    }
};
int main(int argc, char** argv)
{
    if(argc != 1 && argc != 2) { return 2; }
    if(argc == 2)
    {
        if(::fast_io::cstring_view{::fast_io::mnp::os_c_str(argv[1])} != "blocked") { return 2; }
        blocked_probe = 1;
    }
    if(!event::sdk_available || !dbg::native_perf_signal_linux::kernel_supports_async_delivery())
    {
        ::fast_io::io::println("native_continuation_linux_event: SKIP incompatible siginfo ABI or pre-5.19 kernel; no runtime NI qualification");
        return 77;
    }
    ::perf_event_attr attributes{};
    target = reinterpret_cast<::std::uintptr_t>(::std::addressof(continuation_body));
    check(event::make_attributes(target, cookie, attributes), "SDK attribute construction");
    check(attributes.type == PERF_TYPE_BREAKPOINT && attributes.size == PERF_ATTR_SIZE_VER7 &&
          attributes.bp_type == HW_BREAKPOINT_X && attributes.bp_addr == target && attributes.bp_len == sizeof(long) &&
          attributes.sigtrap && attributes.remove_on_exec && attributes.disabled && attributes.pinned &&
          attributes.exclude_kernel && attributes.exclude_hv && !attributes.inherit &&
          attributes.sample_period == 1u && attributes.sample_type == PERF_SAMPLE_ADDR && attributes.sig_data == cookie,
          "precise native kernel execute event contract, not a data breakpoint or sampling counter");
    action_owner action{}; check(action.install(), "owned temporary SIGTRAP disposition");
    ::std::thread worker{[]
    {
        ::std::uint64_t original_mask{};
        if(blocked_probe)
        {
            ::std::uint64_t block{1ull << (SIGTRAP - 1u)};
            // [actual worker-owned complete eight-byte kernel masks] end
            // [safe] kernel copies block and writes original_mask, before target
            // publication; this finite component never uses a guest address.
            check(dbg::native_step::details::raw_system_call(SYS_rt_sigprocmask, SIG_BLOCK,
                static_cast<long>(reinterpret_cast<::std::uintptr_t>(::std::addressof(block))),
                static_cast<long>(reinterpret_cast<::std::uintptr_t>(::std::addressof(original_mask))), sizeof(block)) == 0 &&
                (original_mask & block) == 0u, "actual worker SIGTRAP block before perf event admission");
        }
        actual_worker.store(static_cast<::std::uint64_t>(dbg::native_step::details::raw_system_call(SYS_gettid)),
            ::std::memory_order_release);
        while(gate.load(::std::memory_order_acquire) == 0u) { ::std::this_thread::yield(); }
        if(gate.load(::std::memory_order_acquire) == 1u) { continuation_body(); }
        if(blocked_probe)
        {
            // Genuine return-to-userspace delivers the queued ASYNC signal
            // after the target's effect. Restore this same worker's old mask;
            // a manager sleep/FD close does not count as event delivery ACK.
            check(dbg::native_step::details::raw_system_call(SYS_rt_sigprocmask, SIG_SETMASK,
                static_cast<long>(reinterpret_cast<::std::uintptr_t>(::std::addressof(original_mask))), 0u, sizeof(original_mask)) == 0,
                "actual worker restores signal mask before scope exit");
        }
    }};
    auto const deadline{::std::chrono::steady_clock::now() + ::std::chrono::seconds{5}};
    while(actual_worker.load(::std::memory_order_acquire) == 0u)
    { check(::std::chrono::steady_clock::now() < deadline, "actual owned worker startup"); ::std::this_thread::yield(); }
    event::event_owner owner{};
    if(!event::trusted_component_probe::open(owner, actual_worker.load(::std::memory_order_acquire), target, cookie))
    {
        gate.store(2u, ::std::memory_order_release); worker.join();
        ::fast_io::io::println("native_continuation_linux_event: SKIP hardware execute event unavailable errno=",
            ::fast_io::mnp::dec(owner.native_error()), "; no production NI qualification");
        return 77;
    }
    auto const descriptor{event::trusted_component_probe::descriptor(owner)};
    check((dbg::posix_abi::fcntl_noexcept(descriptor, F_GETFD) & FD_CLOEXEC) != 0,
          "actual descriptor is owned and close-on-exec");
    active_descriptor = descriptor;
    check(event::trusted_component_probe::enable(owner), "enable actual per-worker hardware event");
    gate.store(1u, ::std::memory_order_release); worker.join();
    check(hits == 1 && body_called == 1 && wrong_context == 0,
          "actual kernel event identity/TF and precise-before-effect or flagged-deferred delivery contract");
    event::trusted_component_probe::retire(owner);
    check(!owner.owns_descriptor(), "FastIO resource retired after actual worker exit ACK");
    active_descriptor = -1;
    continuation_body();
    check(hits == 1 && body_called == 2, "retired hardware event cannot trap another entry");
    ::fast_io::io::println("native_continuation_linux_event: PASS actual per-worker kernel hardware execute event; ",
        ::fast_io::mnp::os_c_str(blocked_probe ? "blocked-ASYNC-flag-after-effect=yes " : "synchronous-zero-flags-before-effect=yes "),
        "TF-off=yes FastIO-FD-retired-after-worker-ACK=yes runtime-cursor/NI/finish-qualified=no");
    return 0;
}
#else
int main()
{
    ::fast_io::io::println("native_continuation_linux_event: SKIP required native Linux LP64 x86-64 SDK absent; no runtime NI qualification");
    return 77;
}
#endif

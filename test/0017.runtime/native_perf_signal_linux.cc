// DATA/layout/parser fixtures only. No manufactured signal is dispatched and
// no fixture authorizes a kernel trap, Wasm owner, register read or NI cursor.
#include <uwvm2/uwvm/debugger/native_perf_signal_linux.h>
#include <fast_io.h>
namespace perf = ::uwvm2::uwvm::debugger::native_perf_signal_linux;
static unsigned checks{};
static void check(bool result, char const* message) noexcept
{
    ++checks;
    if(!result)
    {
        ::fast_io::io::perrln("native_perf_signal_linux: FAIL ", ::fast_io::mnp::os_c_str(message));
        ::fast_io::fast_terminate();
    }
}
#if defined(__linux__) && defined(__x86_64__) && __SIZEOF_POINTER__ == 8 && !defined(__ILP32__)
template<typename T, ::std::size_t Offset>
static void fixture_field(::siginfo_t& info, T value) noexcept
{
    static_assert(Offset + sizeof(T) <= sizeof(info));
    auto const bytes{::std::bit_cast<::std::array<unsigned char, sizeof(T)>>(value)};
    auto* target{reinterpret_cast<unsigned char*>(::std::addressof(info))};
    for(::std::size_t i{}; i != bytes.size(); ++i) { target[Offset + i] = bytes[i]; }
}
#endif
int main()
{
    for(auto release : {"5.19.0", "5.19.1-backport", "5.20.0+", "6.0.0", "6.17.0-29-generic", "7.0.0"})
    { check(perf::kernel_release_supported(::fast_io::string_view{::fast_io::mnp::os_c_str(release)}), "supported release DATA"); }
    for(auto release : {"", "4.99.0", "5.18.999", "5.0.0", "5.19", "5.19x.0", "5.19.0bad", "+6.0.0",
        " 6.0.0", "6.0.-1", "6.0.0 trailing", "6.0.0-\n", "4294967296.0.0", "6.4294967296.0", "6.0.4294967296"})
    { check(!perf::kernel_release_supported(::fast_io::string_view{::fast_io::mnp::os_c_str(release)}), "old/malformed/overflow release DATA"); }
    char oversized[65]{};
    for(auto& value : oversized) { value = '0'; }
    check(!perf::kernel_release_supported(::fast_io::string_view{oversized, sizeof(oversized)}), "bounded release extent");
#if defined(__linux__) && defined(__x86_64__) && __SIZEOF_POINTER__ == 8 && !defined(__ILP32__)
    if(!perf::available)
    {
        ::fast_io::io::println("native_perf_signal_linux: SKIP incompatible actual libc siginfo ABI; DATA version checks=", checks);
        return 77;
    }
    check(!perf::read(nullptr).valid, "null signal DATA");
    ::siginfo_t info{};
    info.si_signo = SIGTRAP; info.si_code = perf::trap_code;
    constexpr ::std::uint64_t cookie{0xFEDCBA9876543210ull};
    constexpr ::std::uint32_t type{0x12345678u};
    fixture_field<::std::uint64_t, 24u>(info, cookie);
    fixture_field<::std::uint32_t, 32u>(info, type);
    fixture_field<::std::uint32_t, 36u>(info, 0u);
    auto decoded{perf::read(::std::addressof(info))};
    check(decoded.valid && decoded.cookie == cookie && decoded.type == type && decoded.flags == 0u,
        "complete 64-bit cookie and distinct type/flags offsets");
    for(::std::uint32_t flags : {perf::asynchronous_flag, 2u, 0x80000000u, 0xFFFFFFFFu})
    {
        fixture_field<::std::uint32_t, 36u>(info, flags);
        decoded = perf::read(::std::addressof(info));
        check(decoded.valid && decoded.flags == flags && decoded.cookie == cookie && decoded.type == type,
            "deferred and unknown flags preserved for fail-closed consumer");
    }
# if defined(si_perf_data) && defined(si_perf_type) && defined(si_perf_flags)
    check(static_cast<::std::uint64_t>(info.si_perf_data) == decoded.cookie &&
        info.si_perf_type == decoded.type && info.si_perf_flags == decoded.flags, "actual libc accessor agreement");
# endif
    for(int code : ::std::array<int, 6u>{SI_USER, SI_QUEUE, SI_TKILL, TRAP_TRACE, TRAP_BRKPT, SI_KERNEL})
    {
        info.si_code = code;
        check(!perf::read(::std::addressof(info)).valid, "unrelated and user-origin signal DATA rejected");
    }
    info.si_code = perf::trap_code; info.si_signo = SIGSEGV;
    check(!perf::read(::std::addressof(info)).valid, "wrong signal number DATA rejected");
    ::fast_io::io::println("native_perf_signal_linux: PASS DATA checks=", checks,
        " actual-libc-layout=yes kernel-events/runtime-NI/finish-qualified=no");
    return 0;
#else
    check(!perf::available && !perf::kernel_supports_async_delivery(), "other native ABIs unavailable");
    ::fast_io::io::println("native_perf_signal_linux: PASS release DATA checks=", checks,
        " native-ABI-unavailable=yes kernel-events/runtime-NI/finish-qualified=no");
    return 0;
#endif
}

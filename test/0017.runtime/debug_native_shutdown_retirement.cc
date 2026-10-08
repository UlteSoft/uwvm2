// Actual HOST domain/empty-native retirement seam component.
// This does not simulate a JIT trap or prove physical worker/execution drain.
#include <uwvm2/uwvm/debugger/controller.h>
#include <chrono>
#include <cstdint>
#include <fast_io.h>
namespace dbg = ::uwvm2::uwvm::debugger;
namespace ctl = ::uwvm2::utils::control;
static void check(bool value, char const* message) noexcept
{
    if(!value) { ::fast_io::io::perrln(::fast_io::err(), "debug_native_shutdown_retirement: FAIL ",
        ::fast_io::mnp::os_c_str(message)); ::fast_io::fast_terminate(); }
}
int main()
{
    ctl::launch_config config{}; config.debug_enabled = true;
    config.compiler = ctl::backend::llvm; config.mode = ctl::compile_mode::full;
    config.origin = ctl::launch_origin::console; config.instance[0] = 1u;
#if defined(_WIN32) && !defined(__CYGWIN__)
    config.vm_process = ::fast_io::win32::GetCurrentProcessId();
#elif defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
    config.vm_process = static_cast<::std::uint64_t>(dbg::posix_abi::getpid_noexcept());
#else
# error actual host process identity adapter required
#endif
    auto owner{dbg::controller::create(config, 1u)};
    check(owner && owner->status() == ctl::error::none, "actual console launch authority");
    using result = dbg::controller::native_shutdown_retirement;
    auto deadline{::std::chrono::steady_clock::now()};
    check(owner->try_retire_native_for_shutdown(deadline) == result::invalid_state,
          "open domain cannot authorize shutdown native retirement");
    auto domain{owner->domain()}; auto participant{domain->enter()};
    check(static_cast<bool>(participant), "actual participant admission");
    domain->close();
    check(domain->is_closed(), "actual domain closed before release probe");
    check(owner->try_retire_native_for_shutdown(deadline) == result::retired,
          "actual empty native backend can retire even at elapsed deadline");
    check(static_cast<bool>(participant), "native retirement does not manufacture execution participant retirement");
    check(!domain->enter(), "closed domain retains closed admission");
    check(owner->try_retire_native_for_shutdown(deadline) == result::retired,
          "genuine already-retired native probe remains idempotent");
    participant.reset();
    ::fast_io::io::println(::fast_io::out(), "debug_native_shutdown_retirement: PASS empty-native seam only");
}

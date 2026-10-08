#include <cerrno>
#include <cstdint>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#if !defined(__linux__) || !defined(SYS_pidfd_open) || !defined(__GLIBC__) || defined(__UCLIBC__) || defined(__BIONIC__) || !defined(__USE_MISC)
# error "Fresh module oracle needs the actual GNU Linux SDK pidfd provider"
#endif
import fast_io;
import uwvm2.utils.control;
// Do not inject FastIO headers to repair missing exported syscall/copy APIs.
static_assert(noexcept(::syscall(static_cast<long>(SYS_pidfd_open), ::pid_t{}, 0u)));
static_assert(noexcept(::fast_io::system_call<SYS_pidfd_open, int>(::pid_t{}, 0u)));
int main()
{
    errno = 0;
    auto const native{::syscall(static_cast<long>(SYS_pidfd_open), ::pid_t{}, 0u)};
    int const native_error{errno};
    if(native != -1 || native_error <= 0 ||
       ::fast_io::system_call<SYS_pidfd_open, int>(::pid_t{}, 0u) != -native_error)
    { ::fast_io::io::perrln("FAIL fresh named-module pidfd SDK error"); return 1; }
    ::uwvm2::utils::control::error failure{};
    ::uwvm2::utils::control::launch_config config{};
    config.debug_enabled = config.replacement_enabled = true;
    config.compiler = ::uwvm2::utils::control::backend::llvm;
    config.instance[0u] = 42u;
    auto channel{::uwvm2::utils::control::linux_launch_channel::create(config, failure)};
    if(!channel || failure != ::uwvm2::utils::control::error::none)
    { ::fast_io::io::perrln("UNQUALIFIED fresh module actual launch provider"); return 2; }
    if(channel->select_launcher() != ::uwvm2::utils::control::error::none)
    { ::fast_io::io::perrln("FAIL fresh module launch role"); return 1; }
    channel->close();
    if(channel->native_handle() != -1)
    { ::fast_io::io::perrln("FAIL fresh module owned launch retirement"); return 1; }
    ::fast_io::io::println("PASS fresh named-module FastIO pidfd launch creation");
}

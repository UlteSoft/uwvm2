#include "posix_test_abi.h"
#include <uwvm2/utils/control/impl.h>
#include <initializer_list>
#include <limits>
#include <type_traits>
#if !defined(__linux__) || !defined(SYS_pidfd_open) || !defined(__GLIBC__) || defined(__UCLIBC__) || defined(__BIONIC__) || !defined(__USE_MISC)
# error "This independent fixture requires actual GNU Linux SDK and pidfd_open"
#endif
// Independent oracle calls the genuine target SDK declaration. It does not
// reuse the production alias or FastIO's errno conversion.
using sdk_syscall_signature = long (*)(long, ...);
static_assert(::std::is_convertible_v<decltype(&::syscall), sdk_syscall_signature>);
static_assert(noexcept(::syscall(static_cast<long>(SYS_pidfd_open), ::pid_t{}, 0u)));
static_assert(noexcept(::getpid()));
static_assert(noexcept(::fast_io::system_call<SYS_pidfd_open, int>(::pid_t{}, 0u)));
namespace ctl = ::uwvm2::utils::control;
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); ::fast_io::fast_terminate(); } } while(false)
int main()
{
    auto const self{::getpid()};
    errno = 0;
    long const native{::syscall(static_cast<long>(SYS_pidfd_open), self, 0u)};
    if(native == -1)
    {
        ::fast_io::io::perrln("UNQUALIFIED actual pidfd_open provider errno=", errno);
        return 2;
    }
    CHECK(native >= 0 && native <= (::std::numeric_limits<int>::max)());
    ::fast_io::posix_file native_owner{static_cast<int>(native)};
    int const flags{::control_test::posix_abi::control_test_native_fcntl(native_owner.native_handle(), F_GETFD, 0)};
    CHECK(flags >= 0 && (flags & FD_CLOEXEC) != 0);
    int const actual{::fast_io::system_call<SYS_pidfd_open, int>(self, 0u)};
    CHECK(actual >= 0);
    ::fast_io::posix_file actual_owner{actual};
    int const actual_flags{::control_test::posix_abi::control_test_native_fcntl(actual_owner.native_handle(), F_GETFD, 0)};
    CHECK(actual_flags == flags);
    for(auto const invalid_pid: {::pid_t{}, ::pid_t{-1}})
    {
        errno = 0;
        CHECK(::syscall(static_cast<long>(SYS_pidfd_open), invalid_pid, 0u) == -1);
        int const actual_error{errno}; CHECK(actual_error > 0);
        int const result{::fast_io::system_call<SYS_pidfd_open, int>(invalid_pid, 0u)};
        CHECK(result == -actual_error);
    }
    errno = 0;
    CHECK(::syscall(static_cast<long>(SYS_pidfd_open), self, (::std::numeric_limits<unsigned int>::max)()) == -1);
    int const flag_error{errno}; CHECK(flag_error > 0);
    CHECK((::fast_io::system_call<SYS_pidfd_open, int>(self, (::std::numeric_limits<unsigned int>::max)()) == -flag_error));
    ctl::error failure{};
    auto disabled{ctl::linux_launch_channel::create({}, failure)};
    CHECK(!disabled && failure == ctl::error::disabled);
    ctl::launch_config config{};
    config.debug_enabled = config.replacement_enabled = true;
    config.compiler = ctl::backend::llvm; config.instance[0u] = 42u;
    auto channel{ctl::linux_launch_channel::create(config, failure)};
    CHECK(channel && failure == ctl::error::none);
    CHECK(channel->select_receiver({}) == ctl::error::same_process);
    CHECK(channel->select_launcher() == ctl::error::none);
    int const endpoint{channel->native_handle()}; CHECK(endpoint >= 0);
    CHECK((::control_test::posix_abi::control_test_native_fcntl(endpoint, F_GETFD, 0) & FD_CLOEXEC) != 0);
    auto const status{::fast_io::posix_getfl_nothrow(::fast_io::posix_io_observer{endpoint})};
    CHECK(status && (status.flags & O_NONBLOCK) != 0);
    channel->close(); CHECK(channel->native_handle() == -1);
    // No FD opens between real close and independent stale-descriptor query.
    errno = 0;
    CHECK(::control_test::posix_abi::control_test_native_fcntl(endpoint, F_GETFD, 0) == -1 && errno == EBADF);
    CHECK(::fast_io::posix_close_nothrow(native_owner));
    CHECK(::fast_io::posix_close_nothrow(actual_owner));
    ::fast_io::io::println("PASS actual FastIO pidfd + launch creation/ownership checks: ", checks);
}

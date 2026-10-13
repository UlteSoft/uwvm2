// Real Linux process/endpoint test; no controller/runtime/credential stubs.
#include <uwvm2/utils/macro/push_macros.h>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#if !defined(__linux__) || !defined(__x86_64__) || !defined(__GLIBC__) || defined(__UCLIBC__) || defined(__BIONIC__) || !defined(__USE_MISC) || !defined(SYS_pidfd_open) || !defined(__NR_prctl) || !defined(__NR_seccomp) || !defined(__X32_SYSCALL_BIT)
# error "This finite real-kernel fixture needs the actual native GNU x86_64 LP64 SDK."
#endif
static_assert(sizeof(long) == 8u && sizeof(void*) == 8u);
namespace debugger_endpoint_sdk
{
using syscall_signature = long (*)(long, ...);
using fcntl_signature = int (*)(int, int, ...);
static_assert(::std::is_convertible_v<decltype(&::syscall), syscall_signature>);
static_assert(::std::is_convertible_v<decltype(&::fcntl), fcntl_signature>);
// These independent aliases bind to the actual SDK C ABI; no production
// FastIO error conversion or debugger alias is reused by the oracle.
extern long sdk_syscall(long, ...) noexcept __asm__("syscall");
extern ::pid_t sdk_fork() noexcept __asm__("fork");
extern ::pid_t sdk_waitpid(::pid_t, int*, int) noexcept __asm__("waitpid");
extern int sdk_socketpair(int, int, int, int*) noexcept __asm__("socketpair");
extern ::pid_t sdk_getpid() noexcept __asm__("getpid");
[[noreturn]] extern void sdk_exit(int) noexcept __asm__("_exit");
#if defined(__USE_TIME64_REDIRECTS) || (defined(__USE_TIME_BITS64) && defined(__TIMESIZE) && __TIMESIZE == 32)
extern int sdk_fcntl(int, int, ...) noexcept __asm__("__fcntl_time64");
#elif defined(__USE_FILE_OFFSET64)
extern int sdk_fcntl(int, int, ...) noexcept __asm__("fcntl64");
#else
extern int sdk_fcntl(int, int, ...) noexcept __asm__("fcntl");
#endif
}
#if defined(UWVM_TEST_DEBUG_CONTROL_FD_MODULE)
import fast_io;
import uwvm2.utils.control;
import uwvm2.uwvm.debugger;
// SDK-only global fragment above: never preinclude a FastIO header to fix PCM.
#else
#include <fast_io.h>
#include <uwvm2/uwvm/debugger/linux_control_fd.h>
#endif
namespace dbg = ::uwvm2::uwvm::debugger;
namespace ctl = ::uwvm2::utils::control;
namespace sdk = debugger_endpoint_sdk;
static_assert(noexcept(::fast_io::system_call<SYS_pidfd_open, int>(::pid_t{}, 0u)));
static bool install_pidfd_emfile() noexcept
{
    // Child only. Kernel copies this complete, owned SDK filter on one call;
    // the launcher/keeper and sibling positive test never receive the filter.
    ::std::array<struct ::sock_filter, 9u> filter{{
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct ::seccomp_data, arch)),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1u, 0u),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, offsetof(struct ::seccomp_data, nr)),
        BPF_JUMP(BPF_JMP | BPF_JSET | BPF_K, __X32_SYSCALL_BIT, 0u, 1u),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, SYS_pidfd_open, 0u, 1u),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | EMFILE),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)
    }};
    // [nine complete initialized native SDK filter cells] end
    //  ^^ data(); size9 fits the actual sock_fprog unsigned-short length.
    struct ::sock_fprog program{static_cast<unsigned short>(filter.size()), filter.data()};
    if(sdk::sdk_syscall(static_cast<long>(__NR_prctl), PR_SET_NO_NEW_PRIVS, 1ul, 0ul, 0ul, 0ul) != 0) return false;
    return sdk::sdk_syscall(static_cast<long>(__NR_seccomp), SECCOMP_SET_MODE_FILTER, 0u, ::std::addressof(program)) == 0;
}
static int run_child(int transferred, ::pid_t peer, bool inject_error) noexcept
{
    // Exclusively adopt the transferred socket immediately. Later release()
    // transfers once into the real product adopter, including failure paths.
    ::fast_io::native_file supplied{transferred};
    if(transferred < 3 || ctl::console_input_sealed()) return 1;
    if(inject_error && !install_pidfd_emfile()) return 2;
    errno = 0;
    long const oracle{sdk::sdk_syscall(static_cast<long>(SYS_pidfd_open), peer, 0u)};
    int const oracle_error{errno};
    if(inject_error)
    {
        if(oracle != -1 || oracle_error != EMFILE) return 1;
        errno = 0;
        auto rejected{dbg::linux_control_fd::adopt(supplied.release())};
        int const rejection_error{errno}; // capture before any independent fcntl
        if(rejected || rejection_error != oracle_error || ctl::console_input_sealed()) return 1;
        errno = 0;
        if(sdk::sdk_fcntl(transferred, F_GETFD, 0) != -1 || errno != EBADF) return 1;
        return 0;
    }
    if(oracle < 0) return 2; // Actual unavailable/denied pidfd is not PASS.
    if(oracle > (::std::numeric_limits<int>::max)()) return 1;
    ::fast_io::native_file oracle_owner{static_cast<int>(oracle)};
    int const oracle_flags{sdk::sdk_fcntl(oracle_owner.native_handle(), F_GETFD, 0)};
    if(oracle_flags < 0 || (oracle_flags & FD_CLOEXEC) == 0) return 1;
    auto channel{dbg::linux_control_fd::adopt(supplied.release())};
    if(!channel || !ctl::console_input_sealed()) return 1;
    int const endpoint_flags{sdk::sdk_fcntl(transferred, F_GETFD, 0)};
    if(endpoint_flags < 0 || (endpoint_flags & FD_CLOEXEC) == 0) return 1;
    auto const flags{::fast_io::posix_getfl_nothrow(::fast_io::posix_io_observer{transferred})};
    if(!flags || (flags.flags & O_NONBLOCK) == 0) return 1;
    channel.reset();
    // No guest has ever entered; all admitted operations are absent/drained.
    ctl::unseal_console_input_after_guest_drain_host_api();
    if(ctl::console_input_sealed()) return 1;
    errno = 0;
    if(sdk::sdk_fcntl(transferred, F_GETFD, 0) != -1 || errno != EBADF) return 1;
    return 0;
}
static int run_pair(bool inject_error) noexcept
{
    // Parent stdin/stdout/stderr must be distinct real supervisor stdio,
    // never aliases of these fresh socketpair capabilities.
    for(int fd{}; fd != 3; ++fd) if(sdk::sdk_fcntl(fd, F_GETFD, 0) < 0) return 2;
    int endpoints[2]{-1, -1};
    if(sdk::sdk_socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, endpoints) != 0) return 2;
    ::fast_io::native_file launcher{endpoints[0]}, receiver{endpoints[1]};
    if(endpoints[0] < 3 || endpoints[1] < 3) return 1;
    ::pid_t const parent{sdk::sdk_getpid()};
    ::pid_t const child{sdk::sdk_fork()};
    if(child < 0) return 2;
    if(child == 0)
    {
        launcher.reset();
        int const result{run_child(receiver.release(), parent, inject_error)};
        sdk::sdk_exit(result); // child locals were retired by run_child's return
    }
    receiver.reset();
    int status{}; ::pid_t reaped;
    do { reaped = sdk::sdk_waitpid(child, ::std::addressof(status), 0); } while(reaped == -1 && errno == EINTR);
    if(reaped != child || !WIFEXITED(status)) return 1;
    return WEXITSTATUS(status);
}
static int run_own_pid_rejection() noexcept
{
    int endpoints[2]{-1, -1};
    if(sdk::sdk_socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, endpoints) != 0) return 2;
    ::fast_io::native_file peer{endpoints[0]}, receiver{endpoints[1]};
    if(endpoints[0] < 3 || endpoints[1] < 3 || ctl::console_input_sealed()) return 1;
    // The independent SDK query proves this otherwise valid endpoint's peer
    // is the current VM process, rather than an invalid type or closed FD.
    ::ucred observed{}; ::socklen_t size{sizeof(observed)};
    if(::getsockopt(receiver.native_handle(), SOL_SOCKET, SO_PEERCRED, &observed, &size) != 0 ||
       size != sizeof(observed) || observed.pid != sdk::sdk_getpid()) return 1;
    int const transferred{receiver.release()};
    auto rejected{dbg::linux_control_fd::adopt(transferred)};
    if(rejected || ctl::console_input_sealed()) return 1;
    errno = 0;
    if(sdk::sdk_fcntl(transferred, F_GETFD, 0) != -1 || errno != EBADF) return 1;
    return 0;
}
int main()
{
    int const self{run_own_pid_rejection()};
    if(self != 0) { ::fast_io::io::perrln("UNQUALIFIED/FAIL real debugger endpoint own-PID refusal status=", self); return self; }
    int const positive{run_pair(false)};
    if(positive != 0) { ::fast_io::io::perrln("UNQUALIFIED/FAIL real debugger endpoint positive status=", positive); return positive; }
    int const failure{run_pair(true)};
    if(failure != 0) { ::fast_io::io::perrln("UNQUALIFIED/FAIL real debugger endpoint pidfd EMFILE status=", failure); return failure; }
    ::fast_io::io::println("PASS real debugger endpoint own-PID refusal, external PID/ownership and child-only kernel pidfd EMFILE");
}

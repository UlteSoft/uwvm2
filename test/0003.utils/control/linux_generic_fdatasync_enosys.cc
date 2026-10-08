#include "linux_generic_test_abi.h"
#include <cstdint>
#include <limits>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#if defined(UWVM_TEST_IMPORT_FAST_IO)
import fast_io;
#else
#include <fast_io.h>
#endif

#if !defined(__NR_fdatasync) || !defined(__NR_fsync) || !defined(__NR_prctl) || !defined(__NR_seccomp)
# error "Actual generic target SDK syscall and seccomp declarations are required"
#endif
#if defined(__i386__) && defined(AUDIT_ARCH_I386)
inline constexpr ::std::uint32_t native_audit_arch{AUDIT_ARCH_I386};
#elif defined(__powerpc64__) && defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
      __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__ && defined(AUDIT_ARCH_PPC64)
inline constexpr ::std::uint32_t native_audit_arch{AUDIT_ARCH_PPC64};
#elif defined(__powerpc64__) && defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
      __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ && defined(AUDIT_ARCH_PPC64LE)
inline constexpr ::std::uint32_t native_audit_arch{AUDIT_ARCH_PPC64LE};
#else
# error "This real kernel filter needs a separately bound i386/PPC64 SDK audit ABI; never guess one"
#endif
// This filter belongs ONLY to a new short-lived fixture child scheduled by
// the existing sole keeper. Never install it in an uwvm/server/supervisor.
// QEMU user mode may not implement guest seccomp: failed installation cannot
// be called a passed ENOSYS path. A real target kernel/compat process is needed.
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); return 1; } } while(false)
int main(int argc, char** argv)
{
    CHECK(argc == 2);
    namespace oracle = ::linux_generic_test::abi;
    auto opened{::fast_io::posix_openat_nothrow(::fast_io::posix_at_fdcwd(), argv[1],
        O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW, static_cast<::fast_io::perms>(0600))};
    CHECK(opened);
    ::fast_io::native_unlinkat(::fast_io::posix_at_fdcwd(), argv[1]);
    auto const observer{::fast_io::posix_io_observer{opened.file.native_handle()}};
    CHECK(::fast_io::posix_fdatasync_nothrow(observer));
    CHECK(::fast_io::posix_fsync_nothrow(observer));
    struct ::sock_filter filter[]{
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, static_cast<::std::uint32_t>(offsetof(struct ::seccomp_data, arch))),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, native_audit_arch, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, static_cast<::std::uint32_t>(offsetof(struct ::seccomp_data, nr))),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_fdatasync, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | ENOSYS),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)};
    constexpr auto filter_count{sizeof(filter) / sizeof(filter[0])};
    static_assert(filter_count <= (::std::numeric_limits<unsigned short>::max)());
    struct ::sock_fprog program{static_cast<unsigned short>(filter_count), filter};
    CHECK(oracle::syscall(__NR_prctl, static_cast<long>(PR_SET_NO_NEW_PRIVS), 1L, 0L, 0L, 0L) == 0);
    // [safe complete program] -> [safe filter[0..filter_count)]
    //       ^ ONE SDK seccomp syscall copies these owned cells synchronously.
    CHECK(oracle::syscall(__NR_seccomp, static_cast<unsigned long>(SECCOMP_SET_MODE_FILTER), 0L, __builtin_addressof(program)) == 0);
    errno = 0;
    CHECK(oracle::syscall(__NR_fdatasync, static_cast<long>(observer.native_handle())) == -1);
    int const native_error{errno};
    CHECK(native_error == ENOSYS);
    errno = EDOM;
    CHECK((::fast_io::system_call<__NR_fdatasync, long>(static_cast<long>(observer.native_handle())) == -static_cast<long>(native_error)));
    CHECK(errno == EDOM);
    errno = EDOM;
    auto const rejected{::fast_io::posix_fdatasync_nothrow(observer)};
    CHECK(!rejected && rejected.error == native_error && errno == EDOM);
    CHECK(opened.file.native_handle() == observer.native_handle());
    // API12 must not hide ENOSYS with fsync. Explicit existing caller policy is
    // exercised as a separate public call, preserving the original descriptor.
    errno = EDOM;
    CHECK(::fast_io::posix_fsync_nothrow(observer));
    CHECK(errno == EDOM);
    CHECK(::fast_io::posix_close_nothrow(opened.file));
    ::fast_io::io::println("PASS actual generic kernel-filter ENOSYS and API12 explicit fallback: ", checks);
}

#ifndef _LARGEFILE64_SOURCE
# define _LARGEFILE64_SOURCE 1
#endif
#include <cstddef>
#include <cstdint>
#include <limits>
#include <asm/unistd.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/memfd.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <fast_io.h>

#if !defined(__linux__) || !defined(__x86_64__) || defined(__ILP32__) || !defined(__NR_memfd_create) || \
    !defined(__NR_fadvise64) || !defined(__NR_fsync) || !defined(__NR_seccomp) || !defined(__NR_prctl) || !defined(__X32_SYSCALL_BIT)
# error "This isolated real-ENOSYS oracle requires native Linux x86_64 SDK declarations"
#endif
namespace fadvise_enosys_test::abi
{
#if defined(__UCLIBC__)
# error "API13 does not qualify uClibc optional large-file advisory symbols"
#elif (defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 4))) || \
    (defined(__BIONIC__) && defined(__ANDROID_API__) && __ANDROID_API__ >= 21)
using offset_type = ::off64_t;
extern "C" int native_fadvise(int, offset_type, offset_type, int) noexcept __asm__("posix_fadvise64");
using sdk_signature = int(*)(int, offset_type, offset_type, int);
static_assert(::std::is_convertible_v<decltype(&::posix_fadvise64), sdk_signature>);
#else
# error "API13 requires independent actual SDK/target binding before enabling another native libc"
#endif
}

// Short-lived single-threaded process under the sole SSH Linux keeper's
// shared 64 GiB cgroup. Install the filter in this process only, never in a
// supervisor, another agent, a product runtime or a running Wasm server.
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); ::fast_io::fast_terminate(); } } while(false)

int main()
{
    using namespace ::fast_io;
    static_assert(noexcept(posix_fadvise_nothrow(posix_io_observer{-1}, 0, 0, POSIX_FADV_NORMAL)));
    int const raw{::fast_io::system_call<__NR_memfd_create, int>("uwvm-fadvise-enosys", MFD_CLOEXEC)};
    CHECK(!::fast_io::linux_system_call_fails(raw));
    posix_file owner{raw};
    auto const observer{posix_io_observer{owner.native_handle()}};
    CHECK(posix_fadvise_nothrow(observer, 0, 0, POSIX_FADV_NORMAL));

    // Actual target SDK field offsets, audit architecture, syscall number and
    // actions. The qualified x86_64 LP64 libc uses the SDK fadvise64 syscall;
    // this recipe deliberately makes no inference about another architecture.
    struct ::sock_filter filter[]{
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, static_cast<::std::uint32_t>(offsetof(struct ::seccomp_data, arch))),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, static_cast<::std::uint32_t>(offsetof(struct ::seccomp_data, nr))),
        BPF_JUMP(BPF_JMP | BPF_JSET | BPF_K, __X32_SYSCALL_BIT, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_fadvise64, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | ENOSYS),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)};
    constexpr auto filter_count{sizeof(filter) / sizeof(filter[0])};
    static_assert(filter_count <= ::std::numeric_limits<unsigned short>::max());
    struct ::sock_fprog program{static_cast<unsigned short>(filter_count), filter};
    CHECK((::fast_io::system_call<__NR_prctl, int>(PR_SET_NO_NEW_PRIVS, 1ul, 0ul, 0ul, 0ul) == 0));
    // [safe complete sock_fprog] -> [safe complete filter[0..9)]
    //       ^ ONE synchronous kernel copy; neither pointer advances/escapes.
    CHECK((::fast_io::system_call<__NR_seccomp, int>(SECCOMP_SET_MODE_FILTER, 0u, __builtin_addressof(program)) == 0));

    errno = EDOM;
    int const sdk_error{::fadvise_enosys_test::abi::native_fadvise(raw, 0, 0, POSIX_FADV_NORMAL)};
    CHECK(sdk_error == ENOSYS && errno == EDOM);
    auto const rejected{posix_fadvise_nothrow(observer, 0, 0, POSIX_FADV_NORMAL)};
    CHECK(!rejected && rejected.error == sdk_error && errno == EDOM);
    auto const invalid{posix_fadvise_nothrow(posix_io_observer{-1}, -1, -1, POSIX_FADV_NORMAL)};
    CHECK(!invalid && invalid.error == ENOSYS && errno == EDOM);
    CHECK(owner.native_handle() == raw);
    // This explicit unrelated request remains unfiltered. An API that silently
    // drops ENOSYS or upgrades a hint to fsync would fail the assertions above.
    CHECK(posix_fsync_nothrow(observer));
    CHECK(posix_close_nothrow(owner) && owner.native_handle() == -1);
    ::fast_io::io::println("PASS real kernel fadvise POSIX-ENOSYS preservation: ", checks);
}

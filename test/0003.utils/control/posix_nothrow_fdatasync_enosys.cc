#include <cstddef>
#include <cstdint>
#include <limits>
#include <asm/unistd.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/memfd.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <fast_io.h>

#if !defined(__linux__) || !defined(__x86_64__) || defined(__ILP32__) || !defined(__NR_memfd_create) || \
    !defined(__NR_fdatasync) || !defined(__NR_fsync) || !defined(__NR_seccomp) || !defined(__NR_prctl) || !defined(__X32_SYSCALL_BIT)
# error "This isolated real-ENOSYS fixture requires native Linux x86_64 SDK syscall/seccomp declarations"
#endif

namespace datasync_test::abi
{
extern "C" int native_fdatasync(int) noexcept __asm__("fdatasync");
}

// This is one short-lived, single-threaded test process. Its seccomp filter
// applies only to itself and ends with process retirement; it does not modify
// the supervisor, another thread/process, the cgroup or any product runtime.
// Execute only under the sole SSH Linux keeper, never on the local Mac.
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); ::fast_io::fast_terminate(); } } while(false)

int main()
{
    using namespace ::fast_io;
    static_assert(noexcept(posix_fdatasync_nothrow(posix_io_observer{-1})));
    int const raw{::fast_io::system_call<__NR_memfd_create, int>("uwvm-fdatasync-enosys", MFD_CLOEXEC)};
    CHECK(!::fast_io::linux_system_call_fails(raw));
    posix_file owner{raw};
    auto const observer{posix_io_observer{owner.native_handle()}};
    CHECK(posix_fdatasync_nothrow(observer));
    CHECK(posix_fsync_nothrow(observer));

    // All field offsets, syscall numbers, audit architecture and action values
    // come from this target SDK. Reject another ABI, deny only actual fdatasync
    // with ENOSYS, and allow fsync for the explicit caller-owned fallback below.
    struct ::sock_filter filter[]{
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, static_cast<::std::uint32_t>(offsetof(struct ::seccomp_data, arch))),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 1, 0),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, static_cast<::std::uint32_t>(offsetof(struct ::seccomp_data, nr))),
        BPF_JUMP(BPF_JMP | BPF_JSET | BPF_K, __X32_SYSCALL_BIT, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS),
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_fdatasync, 0, 1),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ERRNO | ENOSYS),
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW)};
    constexpr auto filter_count{sizeof(filter) / sizeof(filter[0])};
    static_assert(filter_count <= ::std::numeric_limits<unsigned short>::max());
    // The SDK sock_fprog pointer is non-const although the kernel reads it;
    // the complete array is already owned/mutable and needs no const_cast.
    struct ::sock_fprog program{static_cast<unsigned short>(filter_count), filter};
    CHECK((::fast_io::system_call<__NR_prctl, int>(PR_SET_NO_NEW_PRIVS, 1ul, 0ul, 0ul, 0ul) == 0));
    // [safe complete sock_fprog] -> [safe filter[0..filter_count)]
    //       ^ the seccomp syscall synchronously borrows/copies these owned SDK
    //         cells. Their pointers are not advanced or retained in user code.
    CHECK((::fast_io::system_call<__NR_seccomp, int>(SECCOMP_SET_MODE_FILTER, 0u, __builtin_addressof(program)) == 0));

    errno = 0;
    int const native_result{::datasync_test::abi::native_fdatasync(raw)};
    int const native_error{errno};
    CHECK(native_result == -1 && native_error == ENOSYS);
    errno = EDOM;
    auto const rejected{posix_fdatasync_nothrow(observer)};
    CHECK(!rejected && rejected.error == native_error);
    CHECK(errno == EDOM);
    CHECK(owner.native_handle() == raw);

    // This separate call models the consumer's existing explicit ENOSYS branch.
    // If the new API silently substituted fsync it would return success above
    // and fail the preserved-ENOSYS assertion before reaching this fallback.
    auto const fallback{posix_fsync_nothrow(observer)};
    CHECK(fallback && fallback.error == 0);
    CHECK(posix_close_nothrow(owner) && owner.native_handle() == -1);
    ::fast_io::io::println("PASS real kernel-filter ENOSYS preservation and explicit fsync fallback: ", checks);
}

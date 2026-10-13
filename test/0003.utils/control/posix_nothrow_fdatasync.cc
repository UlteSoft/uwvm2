#include "posix_test_abi.h"
#include <linux/memfd.h>
#include <fast_io.h>

#if !defined(__linux__) || !defined(__NR_memfd_create) || !defined(__NR_fdatasync)
# error "This real-kernel fdatasync oracle requires target Linux syscall declarations; other SDKs need separate qualification"
#endif

namespace datasync_test::abi
{
// The Linux SDK fdatasync ABI is int(int), without FOB64/time64 payloads or
// symbol redirects. This libc oracle never calls the production raw wrapper.
extern "C" int native_fdatasync(int) noexcept __asm__("fdatasync");
}

// Execute only under the sole SSH Linux keeper's owned 64 GiB cgroup.
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); ::fast_io::fast_terminate(); } } while(false)

namespace
{
inline void compare_with_sdk(int descriptor, int expected_error) noexcept
{
    errno = 0;
    int const native_result{::datasync_test::abi::native_fdatasync(descriptor)};
    int const native_error{native_result == 0 ? 0 : errno};
    CHECK(native_result == 0 || native_result == -1);
    CHECK(native_error == expected_error);
    errno = EDOM;
    auto const actual{::fast_io::posix_fdatasync_nothrow(::fast_io::posix_io_observer{descriptor})};
    CHECK(actual.error == native_error);
    CHECK(static_cast<bool>(actual) == (native_error == 0));
    CHECK(errno == EDOM); // The qualified Linux raw branch never writes errno.
}
}

int main()
{
    using namespace ::fast_io;
    static_assert(noexcept(posix_fdatasync_nothrow(posix_io_observer{-1})));
    compare_with_sdk(-1, EBADF);

    int const raw{::fast_io::system_call<__NR_memfd_create, int>("uwvm-posix-fdatasync", MFD_CLOEXEC)};
    CHECK(!::fast_io::linux_system_call_fails(raw));
    posix_file owner{raw}; // Adopt the actual successful result exactly once.
    auto const observer{posix_io_observer{owner.native_handle()}};
    constexpr char payload[]{"sync"};
    // [safe payload[0..4)] [safe NUL]
    //       ^ borrow exactly four initialized bytes through one write; the
    //         terminator is excluded and no pointer is advanced or retained.
    auto const seeded{posix_write_nothrow(observer, payload, sizeof(payload) - 1u)};
    CHECK(seeded && seeded.transferred == sizeof(payload) - 1u);
    struct ::stat before{};
    // [safe complete SDK stat cell]
    //       ^ the independent fstat oracle borrows this one owned cell only.
    CHECK(::control_test::posix_abi::fstat_noexcept(raw, __builtin_addressof(before)) == 0 && before.st_size == 4);
    auto const position{::control_test::posix_abi::lseek_noexcept(raw, 0, SEEK_CUR)};
    CHECK(position == 4);
    int const flags{::control_test::posix_abi::control_test_native_fcntl(raw, F_GETFL, 0)};
    int const descriptor_flags{::control_test::posix_abi::control_test_native_fcntl(raw, F_GETFD, 0)};
    CHECK(flags >= 0 && descriptor_flags >= 0);
    compare_with_sdk(raw, 0);
    CHECK(owner.native_handle() == raw);
    struct ::stat after{};
    // [safe complete SDK stat cell]
    //       ^ the independent fstat oracle borrows this second owned cell only.
    CHECK(::control_test::posix_abi::fstat_noexcept(raw, __builtin_addressof(after)) == 0);
    CHECK(after.st_size == before.st_size && after.st_dev == before.st_dev && after.st_ino == before.st_ino);
    CHECK(::control_test::posix_abi::lseek_noexcept(raw, 0, SEEK_CUR) == position);
    CHECK(::control_test::posix_abi::control_test_native_fcntl(raw, F_GETFL, 0) == flags);
    CHECK(::control_test::posix_abi::control_test_native_fcntl(raw, F_GETFD, 0) == descriptor_flags);

    int descriptors[2]{-1, -1};
    // [safe descriptors[0] descriptors[1]]
    //       ^ pipe2 borrows exactly this complete pair through return. Both
    //         successful descriptors enter separate RAII owners immediately.
    CHECK(::control_test::posix_abi::pipe2_noexcept(descriptors, O_CLOEXEC) == 0);
    posix_file reader{descriptors[0]}, writer{descriptors[1]};
    compare_with_sdk(reader.native_handle(), EINVAL);
    compare_with_sdk(writer.native_handle(), EINVAL);

    int const retired{owner.native_handle()};
    CHECK(posix_close_nothrow(owner) && owner.native_handle() == -1);
    // No native open/creation occurs between close and the SDK/raw error probes;
    // this numeric descriptor cannot refer to a subsequently reused fixture FD.
    compare_with_sdk(retired, EBADF);
    CHECK(posix_close_nothrow(reader));
    CHECK(posix_close_nothrow(writer));
    ::fast_io::io::println("PASS independent SDK/kernel fdatasync checks: ", checks);
}

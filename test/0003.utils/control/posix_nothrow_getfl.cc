#include "posix_test_abi.h"
#include <fast_io.h>
#if !defined(__linux__) || !defined(__NR_memfd_create)
# error "This real-kernel getfl fixture requires Linux memfd; other providers need their own qualification"
#endif
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); ::fast_io::fast_terminate(); } } while(false)
int main()
{
    using namespace ::fast_io;
    static_assert(noexcept(posix_getfl_nothrow(posix_io_observer{-1})));
    auto flags{posix_getfl_nothrow(posix_io_observer{-1})};
    CHECK(!flags && flags.error == EBADF && flags.flags == 0);
    int const raw{::fast_io::system_call<__NR_memfd_create, int>("uwvm-posix-getfl", 1u /*MFD_CLOEXEC*/)};
    CHECK(!::fast_io::linux_system_call_fails(raw));
    posix_file owner{raw}; // Unique ownership starts immediately after actual creation.
    auto const observer{posix_io_observer{owner.native_handle()}};
    int const before{::control_test::posix_abi::control_test_native_fcntl(raw, F_GETFL, 0)};
    CHECK(before >= 0);
    flags = posix_getfl_nothrow(observer);
    CHECK(flags && flags.flags == before);
    CHECK((flags.flags & O_ACCMODE) == O_RDWR);
    CHECK(::control_test::posix_abi::control_test_native_fcntl(raw, F_SETFL, before | O_NONBLOCK | O_APPEND) == 0);
    int const changed{::control_test::posix_abi::control_test_native_fcntl(raw, F_GETFL, 0)};
    CHECK(changed >= 0 && (changed & (O_NONBLOCK | O_APPEND)) == (O_NONBLOCK | O_APPEND));
    flags = posix_getfl_nothrow(observer);
    CHECK(flags && flags.flags == changed);
    int descriptors[2]{-1, -1};
    // pipe2 borrows exactly this complete two-FD cell through return; both
    // successful descriptors immediately transfer to distinct native owners.
    CHECK(::control_test::posix_abi::pipe2_noexcept(descriptors, O_CLOEXEC) == 0);
    posix_file reader{descriptors[0]}, writer{descriptors[1]};
    flags = posix_getfl_nothrow(posix_io_observer{reader.native_handle()});
    CHECK(flags && (flags.flags & O_ACCMODE) == O_RDONLY); // Zero flags are valid, not failure.
    CHECK(flags.flags == ::control_test::posix_abi::control_test_native_fcntl(reader.native_handle(), F_GETFL, 0));
    flags = posix_getfl_nothrow(posix_io_observer{writer.native_handle()});
    CHECK(flags && (flags.flags & O_ACCMODE) == O_WRONLY);
    CHECK(posix_close_nothrow(owner));
    // Only the retired numeric descriptor is checked; no file is reopened or
    // borrowed after close, so an unrelated reused FD cannot enter this case.
    flags = posix_getfl_nothrow(observer);
    CHECK(!flags && flags.error == EBADF);
    ::fast_io::io::println("PASS independent SDK/kernel F_GETFL checks: ", checks);
}

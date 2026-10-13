#include "linux_generic_test_abi.h"
#include <limits>
#if defined(UWVM_TEST_IMPORT_FAST_IO)
import fast_io;
#else
#include <fast_io.h>
#endif

#if !defined(__NR_getpid) || !defined(__NR_close) || !defined(__NR_fdatasync) || !defined(__NR_lseek)
# error "Actual target SDK syscall declarations are required; no number guessing"
#endif
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); return 1; } } while(false)
int main(int argc, char** argv)
{
    CHECK(argc == 2);
    namespace oracle = ::linux_generic_test::abi;
    static_assert(noexcept(::fast_io::system_call<__NR_getpid, long>()));
    static_assert(noexcept(::fast_io::system_call<__NR_close, long>(-1L)));
    errno = ERANGE;
    long const native_pid{oracle::syscall(__NR_getpid)};
    CHECK(native_pid > 0);
    errno = ERANGE;
    CHECK((::fast_io::system_call<__NR_getpid, long>() == native_pid));
    CHECK(errno == ERANGE);

    errno = 0;
    CHECK(oracle::syscall(__NR_close, -1L) == -1);
    int const badfd_error{errno};
    CHECK(badfd_error == EBADF);
    errno = EDOM;
    CHECK((::fast_io::system_call<__NR_close, long>(-1L) == -static_cast<long>(badfd_error)));
    CHECK(errno == EDOM);

    // -1 is a deliberately invalid Linux syscall number, not a guessed
    // architecture-specific valid syscall. The independent real SDK must
    // establish ENOSYS; template conversion is the same native-long contract.
    constexpr auto invalid_number{static_cast<::std::size_t>(-1L)};
    static_assert(static_cast<long>(invalid_number) == -1L);
    errno = 0;
    CHECK(oracle::syscall(-1L) == -1);
    int const absent_error{errno};
    CHECK(absent_error == ENOSYS);
    errno = EDOM;
    CHECK((::fast_io::system_call<invalid_number, long>() == -static_cast<long>(absent_error)));
    CHECK(errno == EDOM);

    auto opened{::fast_io::posix_openat_nothrow(::fast_io::posix_at_fdcwd(), argv[1],
        O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW, static_cast<::fast_io::perms>(0600))};
    CHECK(opened);
    auto retired{::fast_io::posix_openat_nothrow(::fast_io::posix_at_fdcwd(), argv[1],
        O_RDONLY | O_CLOEXEC | O_NOFOLLOW, static_cast<::fast_io::perms>(0))};
    CHECK(retired);
    ::fast_io::native_unlinkat(::fast_io::posix_at_fdcwd(), argv[1]);
    auto const observer{::fast_io::posix_io_observer{opened.file.native_handle()}};
    constexpr char payload[]{"bridge"};
    // [safe initialized payload[0..6)] [NUL]
    //       ^ ONE fast_io write borrows exactly six bytes through return.
    errno = ERANGE;
    auto const written{::fast_io::posix_write_nothrow(observer, payload, sizeof(payload) - 1u)};
    CHECK(written && written.transferred == sizeof(payload) - 1u && errno == ERANGE);
    errno = 0;
    long const native_sync{oracle::syscall(__NR_fdatasync, static_cast<long>(observer.native_handle()))};
    CHECK(native_sync == 0);
    errno = ERANGE;
    CHECK(::fast_io::posix_fdatasync_nothrow(observer));
    CHECK(errno == ERANGE);
    errno = 0;
    long const native_position{oracle::syscall(__NR_lseek, static_cast<long>(observer.native_handle()), 17L, static_cast<long>(SEEK_SET))};
    CHECK(native_position == 17L);
    errno = ERANGE;
    CHECK((::fast_io::system_call<__NR_lseek, long>(static_cast<long>(observer.native_handle()), 17L, static_cast<long>(SEEK_SET)) == native_position));
    CHECK(errno == ERANGE);
    if constexpr(::std::numeric_limits<long>::digits >= 40)
    {
        constexpr long large_position{static_cast<long>((1ull << 39u) + 17u)};
        // Seeking alone changes neither size nor storage. This tests a true
        // successful native long value above INT_MAX on an actual long64 SDK.
        errno = 0;
        CHECK(oracle::syscall(__NR_lseek, static_cast<long>(observer.native_handle()), large_position, static_cast<long>(SEEK_SET)) == large_position);
        errno = ERANGE;
        CHECK((::fast_io::system_call<__NR_lseek, long>(static_cast<long>(observer.native_handle()), large_position, static_cast<long>(SEEK_SET)) == large_position));
        CHECK(errno == ERANGE);
    }
    auto const metadata{::fast_io::posix_status_nothrow(observer)};
    CHECK(metadata && metadata.value.size == sizeof(payload) - 1u);
    int const retired_number{retired.file.native_handle()};
    CHECK(::fast_io::posix_close_nothrow(retired.file));
    // No descriptor creation follows retirement; the number cannot be reused.
    errno = 0;
    CHECK(oracle::syscall(__NR_fdatasync, static_cast<long>(retired_number)) == -1);
    int const retired_error{errno};
    CHECK(retired_error == EBADF);
    errno = EDOM;
    auto const actual_retired{::fast_io::posix_fdatasync_nothrow(::fast_io::posix_io_observer{retired_number})};
    CHECK(!actual_retired && actual_retired.error == retired_error && errno == EDOM);
    errno = EDOM;
    auto const invalid_sync{::fast_io::posix_fdatasync_nothrow(::fast_io::posix_io_observer{-1})};
    CHECK(!invalid_sync && invalid_sync.error == badfd_error && errno == EDOM);
    CHECK(::fast_io::posix_close_nothrow(opened.file));
    ::fast_io::io::println("PASS actual generic SDK bridge checks ", checks,
        " native-long-bytes ", sizeof(long), " pointer-bytes ", sizeof(void*));
}

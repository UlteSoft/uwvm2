#include "posix_timestamp_test_abi.h"
#include <cstdint>
#include <limits>
#include <memory>
#if defined(UWVM_TEST_IMPORT_FAST_IO)
import fast_io;
#else
#include <fast_io.h>
#endif

#if (defined(__APPLE__) && defined(__MACH__) && defined(UTIME_NOW) && defined(UTIME_OMIT)) || (defined(__linux__) && defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__BIONIC__) && defined(__USE_ATFILE) && defined(__USE_XOPEN2K8) && ((__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 6)) && (!defined(__USE_TIME_BITS64) || defined(__USE_TIME64_REDIRECTS) || defined(__TIMESIZE)))
static unsigned checks{}, native_kernel_checks{}, wide_kernel_checks{}, wide_successful_updates{}, wide_metadata_checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); return 1; } } while(false)
static bool same_timestamp(::fast_io::unix_timestamp lhs, ::fast_io::unix_timestamp rhs) noexcept
{ return lhs.seconds == rhs.seconds && lhs.subseconds == rhs.subseconds; }
static bool compare_fd_sdk(int descriptor, struct ::timespec const (&times)[2]) noexcept
{
    errno = 0;
    int const result{::timestamp_test::abi::native_futimens(descriptor, times)};
    int const error{result == 0 ? 0 : errno};
    auto const actual{::fast_io::posix_futimens_nothrow(::fast_io::posix_io_observer{descriptor}, times)};
    return (result == 0 || result == -1) && actual.error == error && static_cast<bool>(actual) == (error == 0);
}
static bool compare_at_sdk(int descriptor, char const* path, struct ::timespec const (&times)[2], int flags) noexcept
{
    errno = 0;
    int const result{::timestamp_test::abi::native_utimensat(descriptor, path, times, flags)};
    int const error{result == 0 ? 0 : errno};
    auto const actual{::fast_io::posix_utimensat_nothrow(::fast_io::posix_at_entry{descriptor}, path, times, flags)};
    return (result == 0 || result == -1) && actual.error == error && static_cast<bool>(actual) == (error == 0);
}
#if defined(__linux__) && defined(UWVM_TIMESTAMP_ORACLE_HAS_LINUX_UAPI)
template<typename Source>
static bool compare_native_kernel(int descriptor, char const* path, Source const (&times)[2], int flags) noexcept
{
#if defined(__NR_utimensat)
    if constexpr(requires { ::fast_io::linux_utimensat_nothrow(::fast_io::posix_at_entry{descriptor}, path, times, flags); })
    {
        struct ::__kernel_old_timespec native[2]{};
        for(::std::size_t i{}; i != 2u; ++i)
        {
            // Both input and independent actual-UAPI oracle have two complete
            // owned elements; i<2 before member access. Never alias a fake ABI.
            native[i].tv_sec = static_cast<decltype(native[i].tv_sec)>(times[i].tv_sec);
            native[i].tv_nsec = static_cast<decltype(native[i].tv_nsec)>(times[i].tv_nsec);
        }
        errno = 0;
        long const result{::timestamp_test::abi::native_syscall(__NR_utimensat, descriptor, path, native, flags)};
        int const error{result == 0 ? 0 : errno};
        errno = EDOM;
        auto const actual{::fast_io::linux_utimensat_nothrow(::fast_io::posix_at_entry{descriptor}, path, times, flags)};
        ++native_kernel_checks;
        return (result == 0 || result == -1) && actual.error == error && errno == EDOM;
    }
#endif
    return true; // Count stays zero: this target did not qualify the raw API.
}
template<typename Source>
static bool compare_wide_kernel(int descriptor, char const* path, Source const (&times)[2], int flags) noexcept
{
#if defined(__NR_utimensat_time64)
    if constexpr(requires { ::fast_io::linux_utimensat_time64_nothrow(::fast_io::posix_at_entry{descriptor}, path, times, flags); })
    {
        struct ::__kernel_timespec native[2]{};
        for(::std::size_t i{}; i != 2u; ++i)
        {
            native[i].tv_sec = static_cast<decltype(native[i].tv_sec)>(times[i].tv_sec);
            native[i].tv_nsec = static_cast<decltype(native[i].tv_nsec)>(times[i].tv_nsec);
        }
        errno = 0;
        long const result{::timestamp_test::abi::native_syscall(__NR_utimensat_time64, descriptor, path, native, flags)};
        int const error{result == 0 ? 0 : errno};
        errno = EDOM;
        auto const actual{::fast_io::linux_utimensat_time64_nothrow(::fast_io::posix_at_entry{descriptor}, path, times, flags)};
        ++wide_kernel_checks;
        if(descriptor >= 0 && result == 0) ++wide_successful_updates;
        return (result == 0 || result == -1) && actual.error == error && errno == EDOM;
    }
#endif
    return true;
}
#endif

int main(int argc, char** argv)
{
    // Supervisor supplies two NEW absolute names in its isolated sandbox;
    // external cleanup runs after owner retirement. No old pathname is removed.
    CHECK(argc == 3);
    auto opened{::fast_io::posix_openat_nothrow(::fast_io::posix_at_fdcwd(), argv[1], O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC,
        ::fast_io::perms::owner_read | ::fast_io::perms::owner_write)};
    CHECK(opened);
    auto const observer{::fast_io::posix_io_observer{opened.file.native_handle()}};
    auto const original_position{::fast_io::posix_seek_nothrow(observer, 0, ::fast_io::seekdir::cur)};
    auto const original_flags{::fast_io::posix_getfl_nothrow(observer)};
    CHECK(original_position && original_flags);
    struct ::timespec exact[2]{{1700000000, 123456789L}, {1700000001, 987654321L}};
    CHECK(::fast_io::posix_futimens_nothrow(observer, exact));
    auto const seeded{::fast_io::posix_status_nothrow(observer)};
    CHECK(seeded);
    constexpr auto factor{::fast_io::uint_least64_subseconds_per_second / 1000000000u};
    CHECK(seeded.value.atim.seconds == 1700000000 && seeded.value.atim.subseconds == 123456789u * factor);
    CHECK(seeded.value.mtim.seconds == 1700000001 && seeded.value.mtim.subseconds == 987654321u * factor);
    struct ::timespec omit[2]{{0, UTIME_OMIT}, {0, UTIME_OMIT}};
    CHECK(::fast_io::posix_futimens_nothrow(observer, omit));
    auto const untouched{::fast_io::posix_status_nothrow(observer)};
    CHECK(untouched && same_timestamp(untouched.value.atim, seeded.value.atim) && same_timestamp(untouched.value.mtim, seeded.value.mtim));
    struct ::timespec now[2]{{0, UTIME_NOW}, {0, UTIME_OMIT}};
    auto const earliest{::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::realtime)};
    CHECK(::fast_io::posix_futimens_nothrow(observer, now));
    auto const latest{::fast_io::posix_clock_gettime(::fast_io::posix_clock_id::realtime)};
    auto const current{::fast_io::posix_status_nothrow(observer)};
    CHECK(earliest.seconds > (::std::numeric_limits<decltype(earliest.seconds)>::min)());
    // The kernel may use its coarse realtime clock for NOW. An exact explicit
    // nanosecond write above checks this fixture filesystem's precision; NOW
    // need not equal a high-resolution userspace clock sample to the nanosecond.
    CHECK(current && current.value.atim.seconds >= earliest.seconds - 1 && current.value.atim.seconds <= latest.seconds &&
        same_timestamp(current.value.mtim, seeded.value.mtim));
    struct ::timespec invalid[2]{{0, 1000000000L}, {0, UTIME_OMIT}};
    CHECK(compare_fd_sdk(observer.native_handle(), invalid));
    CHECK(compare_fd_sdk(-1, invalid));
    CHECK(compare_fd_sdk(-1, omit));
    CHECK(compare_at_sdk(AT_FDCWD, argv[1], invalid, 0));
    CHECK(compare_at_sdk(-1, "relative-missing", invalid, 0));
    CHECK(compare_at_sdk(-1, "relative-missing", omit, 0));
    CHECK(compare_at_sdk(AT_FDCWD, argv[1], exact, ~0));
    ::fast_io::native_symlinkat(argv[1], ::fast_io::posix_at_fdcwd(), argv[2]);
    CHECK(::fast_io::posix_utimensat_nothrow(::fast_io::posix_at_fdcwd(), argv[2], exact, AT_SYMLINK_NOFOLLOW));
    auto const link{::fast_io::posix_fstatat_nothrow(::fast_io::posix_at_fdcwd(), argv[2], AT_SYMLINK_NOFOLLOW)};
    auto const target{::fast_io::posix_status_nothrow(observer)};
    CHECK(link && target && link.value.type == ::fast_io::file_type::symlink);
    CHECK(link.value.atim.seconds == 1700000000 && link.value.mtim.seconds == 1700000001);
    CHECK(same_timestamp(target.value.atim, current.value.atim) && same_timestamp(target.value.mtim, current.value.mtim));
#if defined(__linux__) && defined(UWVM_TIMESTAMP_ORACLE_HAS_LINUX_UAPI)
    // The actual SDK advertises the AT_EMPTY_PATH value; never copy a guessed
    // numeric flag into the oracle. These compare EACH operation's own native
    // precedence; futimens is not substituted for the raw empty-path variant.
#if defined(AT_EMPTY_PATH)
    CHECK(compare_native_kernel(observer.native_handle(), "", exact, AT_EMPTY_PATH));
    CHECK(compare_native_kernel(-1, "", invalid, AT_EMPTY_PATH));
    CHECK(compare_native_kernel(-1, "", omit, AT_EMPTY_PATH));
    struct ::__kernel_timespec wide[2]{{0x100000001LL, 321L}, {0, UTIME_OMIT}};
    CHECK(compare_wide_kernel(observer.native_handle(), "", wide, AT_EMPTY_PATH));
    CHECK(compare_wide_kernel(-1, "", wide, AT_EMPTY_PATH));
#if defined(UWVM_TIMESTAMP_ORACLE_HAS_LINUX_STATX) && defined(__NR_statx) && defined(STATX_ATIME)
    if(wide_successful_updates != 0u)
    {
        // A narrow libc stat result cannot represent this >2038 timestamp.
        // Use the independent actual SDK/UAPI statx payload to inspect the
        // file's real value, never infer a successful 64-bit write from rc0.
        struct ::statx observed{};
        CHECK(::timestamp_test::abi::native_syscall(__NR_statx, observer.native_handle(), "", AT_EMPTY_PATH,
            STATX_ATIME, ::std::addressof(observed)) == 0);
        CHECK((observed.stx_mask & STATX_ATIME) != 0u && observed.stx_atime.tv_sec == wide[0].tv_sec &&
            observed.stx_atime.tv_nsec == static_cast<::std::uint32_t>(wide[0].tv_nsec));
        ++wide_metadata_checks;
    }
#endif
#endif
#endif
    // Restore a representable value before asking the target's actual native
    // stat/time_t ABI for final metadata. An i386 default-time32 fstat may
    // legitimately reject a correct >2038 file time; it must not make the raw
    // time64 operation fixture fail or be mistaken for a time64 write defect.
    CHECK(::fast_io::posix_futimens_nothrow(observer, exact));
    auto const after{::fast_io::posix_status_nothrow(observer)};
    CHECK(after && after.value.dev == seeded.value.dev && after.value.ino == seeded.value.ino && after.value.size == seeded.value.size);
    auto const final_position{::fast_io::posix_seek_nothrow(observer, 0, ::fast_io::seekdir::cur)};
    auto const final_flags{::fast_io::posix_getfl_nothrow(observer)};
    CHECK(final_position && final_flags && final_position.position == original_position.position && final_flags.flags == original_flags.flags);
    CHECK(::fast_io::posix_close_nothrow(opened.file));
    ::fast_io::println("PASS timestamp SDK checks ", checks, " raw-native selected ", native_kernel_checks, " raw-time64 selected ", wide_kernel_checks, " wide writes verified ", wide_metadata_checks);
}
#else
int main() { return 77; }
#endif

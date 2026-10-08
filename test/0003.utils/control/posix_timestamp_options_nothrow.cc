// Actual selected SDK/UAPI types, not simulated structs or forged syscalls.
// Source-only candidate: new absolute regular-file name owned by supervisor.
#include "posix_timestamp_test_abi.h"
#include <cstdint>
#include <limits>
#include <utility>
#if defined(UWVM_TEST_IMPORT_FAST_IO)
import fast_io;
#else
#include <fast_io.h>
#endif

#if (defined(__linux__) && defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__BIONIC__) && \
    __has_include(<linux/time_types.h>) && defined(__USE_ATFILE) && defined(__USE_XOPEN2K8) && \
    ((__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 6))) || \
    (defined(__APPLE__) && defined(__MACH__) && defined(UTIME_NOW) && defined(UTIME_OMIT))
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); return 1; } } while(false)

// Independent native oracle consumes literal actual target ABI values. Its
// assembly linkage and time64 redirect are governed by the real target SDK.
static int explicit_oracle(int directory, char const* path, int flags) noexcept
{
#if defined(__linux__) && defined(__NR_utimensat_time64)
    struct ::__kernel_timespec const times[2]{{1700000000LL, 123456789LL}, {1700000001LL, 987654321LL}};
    auto const result{::timestamp_test::abi::native_syscall(__NR_utimensat_time64, directory, path, times, flags)};
#elif defined(__linux__) && defined(__NR_utimensat)
    struct ::__kernel_old_timespec const times[2]{{1700000000, 123456789L}, {1700000001, 987654321L}};
    auto const result{::timestamp_test::abi::native_syscall(__NR_utimensat, directory, path, times, flags)};
#else
    struct ::timespec const times[2]{{1700000000, 123456789L}, {1700000001, 987654321L}};
    auto const result{::timestamp_test::abi::native_utimensat(directory, path, times, flags)};
#endif
    return result == 0 ? 0 : errno;
}

#if !defined(UWVM_TEST_IMPORT_FAST_IO)
// A real narrow SDK/UAPI producer is inspected only on a target that has it.
// No fake 32-bit type/macro is substituted on LP64. This conversion evidence
// does not itself qualify a native syscall or filesystem operation.
template<typename Conversion>
static bool verify_owned_narrow_conversion() noexcept
{
    Conversion native[2]{};
    constexpr auto maximum{(::std::numeric_limits<decltype(Conversion::tv_sec)>::max)()};
    if constexpr(maximum < (::std::numeric_limits<::std::int64_t>::max)())
    {
        ::fast_io::unix_timestamp_option const outside{::fast_io::unix_timestamp{static_cast<::std::int64_t>(maximum) + 1, 0}};
        constexpr ::fast_io::unix_timestamp_option omit{::fast_io::utime_flags::omit};
        if(::fast_io::details::posix_timestamp_options_nothrow::convert_pair(outside, omit, native) != EOVERFLOW) return false;
        auto ignored{outside}; ignored.flags = ::fast_io::utime_flags::omit;
        if(::fast_io::details::posix_timestamp_options_nothrow::convert_pair(ignored, omit, native) != 0 ||
            native[0].tv_sec != 0 || native[0].tv_nsec != UTIME_OMIT) return false;
        ignored.flags = ::fast_io::utime_flags::now;
        return ::fast_io::details::posix_timestamp_options_nothrow::convert_pair(ignored, omit, native) == 0 &&
            native[0].tv_sec == 0 && native[0].tv_nsec == UTIME_NOW;
    }
    else return true;
}
#endif

int main(int argc, char** argv)
{
    CHECK(argc == 2);
    auto opened{::fast_io::posix_openat_nothrow(::fast_io::posix_at_fdcwd(), argv[1],
        O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC | O_NOFOLLOW, ::fast_io::perms::owner_read | ::fast_io::perms::owner_write)};
    CHECK(opened);
    auto const observer{::fast_io::posix_io_observer{opened.file}};
    auto const original{::fast_io::posix_status_nothrow(observer)};
    CHECK(original);
    constexpr auto scale{::fast_io::uint_least64_subseconds_per_second / 1000000000u};
    constexpr ::fast_io::unix_timestamp_option omit{::fast_io::utime_flags::omit};
    constexpr ::fast_io::unix_timestamp_option access{::fast_io::unix_timestamp{1700000000, 123456789u * scale}};
    constexpr ::fast_io::unix_timestamp_option modification{::fast_io::unix_timestamp{1700000001, 987654321u * scale}};
    auto const exact{::fast_io::posix_utime_options_nothrow(::fast_io::posix_at_fdcwd(), argv[1], omit, access, modification, AT_SYMLINK_NOFOLLOW)};
    CHECK(exact);
    auto const seeded{::fast_io::posix_status_nothrow(observer)};
    CHECK(seeded && seeded.value.atim.seconds == 1700000000 && seeded.value.atim.subseconds == 123456789u * scale &&
        seeded.value.mtim.seconds == 1700000001 && seeded.value.mtim.subseconds == 987654321u * scale);

    // The creation option is rejected before touching a native capability, as
    // in the existing throwing interface. No syscall should be generated here.
    auto const unsupported_creation{::fast_io::posix_utime_options_nothrow(::fast_io::posix_at_entry{-1}, "missing",
        ::fast_io::unix_timestamp_option{::fast_io::utime_flags::now}, access, modification, AT_SYMLINK_NOFOLLOW)};
    CHECK(unsupported_creation.error == EINVAL);
    auto const omitted{::fast_io::posix_utime_options_nothrow(::fast_io::posix_at_fdcwd(), argv[1], omit, omit, omit, AT_SYMLINK_NOFOLLOW)};
    CHECK(omitted);
    auto const unchanged{::fast_io::posix_status_nothrow(observer)};
    CHECK(unchanged && unchanged.value.atim.seconds == seeded.value.atim.seconds && unchanged.value.atim.subseconds == seeded.value.atim.subseconds &&
        unchanged.value.mtim.seconds == seeded.value.mtim.seconds && unchanged.value.mtim.subseconds == seeded.value.mtim.subseconds);

    // Native FD/path/flags errors must match this exact operation's own
    // independent ABI oracle. There is no raw-time64 to SDK fallback/retry.
    int const missing_error{explicit_oracle(-1, "relative-missing", AT_SYMLINK_NOFOLLOW)};
    auto const missing{::fast_io::posix_utime_options_nothrow(::fast_io::posix_at_entry{-1}, "relative-missing", omit,
        access, modification, AT_SYMLINK_NOFOLLOW)};
    CHECK(missing_error != 0 && missing.error == missing_error);
    int const flags_error{explicit_oracle(AT_FDCWD, argv[1], ~0)};
    auto const invalid_flags{::fast_io::posix_utime_options_nothrow(::fast_io::posix_at_fdcwd(), argv[1], omit, access, modification, ~0)};
    CHECK(flags_error != 0 && invalid_flags.error == flags_error);

#if !defined(UWVM_TEST_IMPORT_FAST_IO)
    // Private converter evidence is header-only, explicitly separate from the
    // importer's public-operation checks. No private module name is exposed.
# if defined(__linux__)
    CHECK(verify_owned_narrow_conversion<::__kernel_old_timespec>());
# else
    CHECK(verify_owned_narrow_conversion<::timespec>());
# endif
#endif
    auto const final{::fast_io::posix_status_nothrow(observer)};
    CHECK(final && final.value.dev == original.value.dev && final.value.ino == original.value.ino && final.value.size == original.value.size);
    CHECK(::fast_io::posix_close_nothrow(opened.file));
    ::fast_io::println("PASS timestamp options checks ", checks);
}
#else
int main() { return 77; }
#endif

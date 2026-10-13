#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <limits>
#include <initializer_list>
#include <type_traits>
#include "posix_test_abi.h"
#include <fast_io.h>

#if !defined(__APPLE__) || !defined(__MACH__) || !defined(F_RDAHEAD) || !defined(F_RDADVISE)
# error "API14 native oracle needs the actual full Darwin SDK command/record declarations"
#endif

namespace darwin_advice_test::abi
{
// This independent alias follows the actual SDK symbol suffixes. It is not
// fast_io's private production alias and never handpacks a native structure.
extern "C" int native_fcntl(int, int, ...) noexcept __DARWIN_ALIAS_C(fcntl);
using sdk_signature = int(*)(int, int, ...);
static_assert(::std::is_convertible_v<decltype(&::fcntl), sdk_signature>);
static_assert(::std::is_same_v<decltype(::radvisory{}.ra_offset), ::off_t>);
static_assert(::std::is_same_v<decltype(::radvisory{}.ra_count), int>);
static_assert(::std::numeric_limits<::off_t>::is_signed);
static_assert(::std::numeric_limits<::off_t>::digits >= ::std::numeric_limits<::fast_io::intfpos_t>::digits);
}

// SOURCE ONLY. Keeper must first establish fresh Linux resource receipts;
// protected macOS execution, if separately scheduled, must remain below 4 GiB.
// Pass one fresh pathname inside a caller-owned isolated directory. No names
// are guessed, temporary-file C APIs used, subprocesses spawned or huge reads.
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); ::fast_io::fast_terminate(); } } while(false)

namespace
{
void compare_readahead(int descriptor, bool enabled, int expected_error) noexcept
{
    errno = EDOM;
    int const sdk_result{::darwin_advice_test::abi::native_fcntl(descriptor, F_RDAHEAD, static_cast<int>(enabled))};
    int const sdk_error{sdk_result < 0 ? errno : 0};
    CHECK(sdk_error == expected_error);
    // XNU's F_RDAHEAD changes FNORDAHEAD in the open file description; F_GETFL
    // returns OFLAGS(f_flag), so its *full* flags may change legitimately.
    int const sdk_flags{expected_error == 0 ? ::darwin_advice_test::abi::native_fcntl(descriptor, F_GETFL, 0) : -1};
    errno = ERANGE;
    auto const actual{::fast_io::posix_readahead_nothrow(::fast_io::posix_io_observer{descriptor}, enabled)};
    CHECK(actual.error == sdk_error && static_cast<bool>(actual) == (sdk_error == 0));
    if(expected_error == 0)
        CHECK(::darwin_advice_test::abi::native_fcntl(descriptor, F_GETFL, 0) == sdk_flags);
}

// expected_error==-1 deliberately accepts a real filesystem's native advisory
// result. Supported/unsupported hints are compared to the independent SDK,
// never promoted to successful physical prefetch or an allocation guarantee.
void compare_readadvise(int descriptor, ::fast_io::intfpos_t offset,
    int byte_count, int expected_error) noexcept
{
    struct ::radvisory native{};
    // [safe complete owned SDK record byte representation]
    //       ^ clear exactly sizeof(native); no pointer advances or escapes.
    ::fast_io::freestanding::bytes_clear_n(reinterpret_cast<::std::byte*>(__builtin_addressof(native)), sizeof(native));
    native.ra_offset = static_cast<::off_t>(offset);
    native.ra_count = byte_count;
    errno = EDOM;
    // [safe complete native radvisory cell]
    //       ^ independent SDK fcntl borrows this cell synchronously only.
    int const sdk_result{::darwin_advice_test::abi::native_fcntl(descriptor, F_RDADVISE, __builtin_addressof(native))};
    int const sdk_error{sdk_result < 0 ? errno : 0};
    CHECK(sdk_error >= 0 && (expected_error == -1 || sdk_error == expected_error));
    errno = ERANGE;
    auto const actual{::fast_io::posix_readadvise_nothrow(::fast_io::posix_io_observer{descriptor}, offset, byte_count)};
    CHECK(actual.error == sdk_error && static_cast<bool>(actual) == (sdk_error == 0));
    CHECK(native.ra_offset == static_cast<::off_t>(offset) && native.ra_count == byte_count);
}
}

int main(int argc, char** argv)
{
    using namespace ::fast_io;
    CHECK(argc == 2);
    static_assert(noexcept(posix_readahead_nothrow(posix_io_observer{-1}, true)));
    static_assert(noexcept(posix_readadvise_nothrow(posix_io_observer{-1}, 0, 0)));
    constexpr intfpos_t wide{(intfpos_t{1} << 40u) + 13};
    constexpr intfpos_t negative_wide{-(intfpos_t{1} << 40u) + 13};
    constexpr intfpos_t largest{(::std::numeric_limits<intfpos_t>::max)()};
    compare_readahead(-1, false, EBADF);
    compare_readahead(-1, true, EBADF);
    compare_readadvise(-1, negative_wide, -1, EBADF);
    compare_readadvise(-1, largest, (::std::numeric_limits<int>::max)(), EBADF);

    // argv[1] borrows one complete caller-owned NUL-terminated fresh name.
    // O_EXCL prevents overwriting an existing path; successful fast_io result
    // owns the descriptor immediately. Unlink through fast_io while it is live.
    auto created{posix_openat_nothrow(posix_at_fdcwd(), argv[1], O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
        static_cast<perms>(0600))};
    CHECK(created);
    ::fast_io::native_unlinkat(posix_at_fdcwd(), argv[1]);
    int const descriptor{created.file.native_handle()};
    auto const observer{posix_io_observer{descriptor}};
    constexpr char payload[]{"hint"};
    // [safe initialized payload[0..4)] NUL
    //       ^ ONE fast_io write borrows exactly four bytes through return.
    auto const seeded{posix_write_nothrow(observer, payload, sizeof(payload) - 1u)};
    CHECK(seeded && seeded.transferred == sizeof(payload) - 1u);
    struct ::stat before{};
    // [safe complete owned SDK stat cell] independent output borrow only.
    CHECK(::control_test::posix_abi::fstat_noexcept(descriptor, __builtin_addressof(before)) == 0 && before.st_size == 4);
    auto const position{::control_test::posix_abi::lseek_noexcept(descriptor, 0, SEEK_CUR)};
    int const flags{::darwin_advice_test::abi::native_fcntl(descriptor, F_GETFL, 0)};
    int const descriptor_flags{::darwin_advice_test::abi::native_fcntl(descriptor, F_GETFD, 0)};
    CHECK(position == 4 && flags >= 0 && descriptor_flags >= 0 && (descriptor_flags & FD_CLOEXEC) != 0);
    constexpr int unrelated_flags{O_ACCMODE | O_APPEND | O_NONBLOCK | O_SYNC | O_DSYNC};
    for(bool const enabled : {false, true, false, true})
    {
        compare_readahead(descriptor, enabled, 0);
        CHECK((::darwin_advice_test::abi::native_fcntl(descriptor, F_GETFL, 0) & unrelated_flags) == (flags & unrelated_flags));
    }
    // Positive native hints use tiny or zero lengths. Large offsets do not
    // create sparse files or ask the kernel to read unbounded quantities.
    compare_readadvise(descriptor, 0, 4, -1);
    compare_readadvise(descriptor, 0, 0, -1);
    compare_readadvise(descriptor, wide, 0, -1);
    compare_readadvise(descriptor, largest, 0, -1);
    // Losing the high word would turn this negative offset into +13, bypassing
    // the kernel EINVAL before filesystem dispatch: an independent ABI oracle.
    compare_readadvise(descriptor, negative_wide, 0, EINVAL);
    compare_readadvise(descriptor, 0, -1, EINVAL);
    struct ::stat after{};
    CHECK(::control_test::posix_abi::fstat_noexcept(descriptor, __builtin_addressof(after)) == 0);
    CHECK(after.st_dev == before.st_dev && after.st_ino == before.st_ino && after.st_size == before.st_size);
    CHECK(created.file.native_handle() == descriptor);
    CHECK(::control_test::posix_abi::lseek_noexcept(descriptor, 0, SEEK_CUR) == position);
    CHECK(::darwin_advice_test::abi::native_fcntl(descriptor, F_GETFD, 0) == descriptor_flags);
    CHECK((::darwin_advice_test::abi::native_fcntl(descriptor, F_GETFL, 0) & unrelated_flags) == (flags & unrelated_flags));

    posix_pipe channel{}; // fast_io owns both pipe descriptors immediately.
    compare_readahead(channel.in().native_handle(), false, EBADF);
    compare_readahead(channel.out().native_handle(), true, EBADF);
    compare_readadvise(channel.in().native_handle(), 0, 0, EBADF);
    compare_readadvise(channel.out().native_handle(), negative_wide, -1, EBADF);
    int const retired{created.file.native_handle()};
    CHECK(posix_close_nothrow(created.file) && created.file.native_handle() == -1);
    // No file/pipe creation intervenes, so the retired FD cannot be reused.
    compare_readahead(retired, true, EBADF);
    compare_readadvise(retired, negative_wide, -1, EBADF);
    CHECK(posix_close_nothrow(channel.in()));
    CHECK(posix_close_nothrow(channel.out()));
    ::fast_io::io::println("PASS independent SDK Darwin advisory/error/state checks: ", checks);
}

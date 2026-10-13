// Expose the independent SDK wide declaration without changing off_t's
// selected native width. Build once natively and once with FOB64/time64.
#ifndef _LARGEFILE64_SOURCE
# define _LARGEFILE64_SOURCE 1
#endif
#include "posix_test_abi.h"
#include <linux/memfd.h>
#include <fast_io.h>

#if !defined(__linux__) || !defined(__NR_memfd_create)
# error "This independent advisory oracle requires the real target Linux SDK"
#endif

namespace fadvise_test::abi
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
# error "API13 requires independent actual SDK/target binding before enabling another Linux libc"
#endif
static_assert(::std::numeric_limits<offset_type>::is_signed);
static_assert(::std::numeric_limits<offset_type>::digits >= ::std::numeric_limits<::fast_io::intfpos_t>::digits);
}

// Source-only until the sole SSH Linux keeper executes fresh EH/noEH builds
// in its owned 64 GiB cgroup. No huge sparse allocation or background process.
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); ::fast_io::fast_terminate(); } } while(false)

namespace
{
void compare_with_sdk(int descriptor, ::fast_io::intfpos_t offset,
    ::fast_io::intfpos_t length, int advice, int expected_error) noexcept
{
    errno = EDOM;
    int const sdk_error{::fadvise_test::abi::native_fadvise(descriptor,
        static_cast<::fadvise_test::abi::offset_type>(offset), static_cast<::fadvise_test::abi::offset_type>(length), advice)};
    CHECK(sdk_error == expected_error);
    CHECK(errno == EDOM); // Real SDK POSIX error-return, not -1/errno.
    auto const actual{::fast_io::posix_fadvise_nothrow(::fast_io::posix_io_observer{descriptor}, offset, length, advice)};
    CHECK(actual.error == sdk_error);
    CHECK(static_cast<bool>(actual) == (sdk_error == 0));
    CHECK(errno == EDOM);
}
}

int main()
{
    using namespace ::fast_io;
    static_assert(noexcept(posix_fadvise_nothrow(posix_io_observer{-1}, 0, 0, POSIX_FADV_NORMAL)));
    constexpr int advices[]{POSIX_FADV_NORMAL, POSIX_FADV_RANDOM, POSIX_FADV_SEQUENTIAL,
        POSIX_FADV_WILLNEED, POSIX_FADV_DONTNEED, POSIX_FADV_NOREUSE};
    constexpr int bad_advice{(::std::numeric_limits<int>::max)()};
    constexpr intfpos_t wide{(intfpos_t{1} << 40u) + 13};
    constexpr intfpos_t negative_wide{-(intfpos_t{1} << 40u) + 13};
    constexpr intfpos_t largest{(::std::numeric_limits<intfpos_t>::max)()};
    // Native EBADF precedes negative/range/advice errors. A wrapper must not
    // replace this result with its own premature EINVAL/EOVERFLOW check.
    compare_with_sdk(-1, negative_wide, negative_wide, bad_advice, EBADF);
    compare_with_sdk(-1, largest, largest, POSIX_FADV_NORMAL, EBADF);

    int const raw{::fast_io::system_call<__NR_memfd_create, int>("uwvm-fadvise-oracle", MFD_CLOEXEC)};
    CHECK(!::fast_io::linux_system_call_fails(raw));
    posix_file owner{raw}; // Adopt the successful descriptor exactly once.
    auto const observer{posix_io_observer{owner.native_handle()}};
    constexpr char payload[]{"hint"};
    // [safe complete payload[0..4)] NUL
    //       ^ ONE write borrows four initialized bytes; no pointer advances.
    auto const seeded{posix_write_nothrow(observer, payload, sizeof(payload) - 1u)};
    CHECK(seeded && seeded.transferred == sizeof(payload) - 1u);
    struct ::stat before{};
    // [safe complete native SDK stat cell]
    //       ^ ONE independent fstat borrows this owned cell through return.
    CHECK(::control_test::posix_abi::fstat_noexcept(raw, __builtin_addressof(before)) == 0 && before.st_size == 4);
    auto const position{::control_test::posix_abi::lseek_noexcept(raw, 0, SEEK_CUR)};
    int const flags{::control_test::posix_abi::control_test_native_fcntl(raw, F_GETFL, 0)};
    int const fd_flags{::control_test::posix_abi::control_test_native_fcntl(raw, F_GETFD, 0)};
    CHECK(position == 4 && flags >= 0 && fd_flags >= 0);
    for(int const advice : advices) { compare_with_sdk(raw, 0, 4, advice, 0); }
    compare_with_sdk(raw, 0, 0, POSIX_FADV_NORMAL, 0);
    compare_with_sdk(raw, wide, wide, POSIX_FADV_NORMAL, 0);
    compare_with_sdk(raw, largest, largest, POSIX_FADV_NORMAL, 0);
    // A lost high 32-bit word would turn these invalid negative values into
    // small positive 13 and return success; these are ABI truncation oracles.
    compare_with_sdk(raw, negative_wide, 0, POSIX_FADV_NORMAL, EINVAL);
    compare_with_sdk(raw, 0, negative_wide, POSIX_FADV_NORMAL, EINVAL);
    compare_with_sdk(raw, 0, 0, bad_advice, EINVAL);
    CHECK(owner.native_handle() == raw);
    struct ::stat after{};
    // [safe complete second native SDK stat cell] synchronous output borrow.
    CHECK(::control_test::posix_abi::fstat_noexcept(raw, __builtin_addressof(after)) == 0);
    CHECK(after.st_dev == before.st_dev && after.st_ino == before.st_ino && after.st_size == before.st_size);
    CHECK(::control_test::posix_abi::lseek_noexcept(raw, 0, SEEK_CUR) == position);
    CHECK(::control_test::posix_abi::control_test_native_fcntl(raw, F_GETFL, 0) == flags);
    CHECK(::control_test::posix_abi::control_test_native_fcntl(raw, F_GETFD, 0) == fd_flags);

    int descriptors[2]{-1, -1};
    // [safe complete descriptor pair] ONE pipe2 output borrow. Successful
    // descriptors acquire separate fast_io RAII owners immediately below.
    CHECK(::control_test::posix_abi::pipe2_noexcept(descriptors, O_CLOEXEC) == 0);
    posix_file reader{descriptors[0]}, writer{descriptors[1]};
    compare_with_sdk(reader.native_handle(), 0, 0, POSIX_FADV_NORMAL, ESPIPE);
    compare_with_sdk(writer.native_handle(), negative_wide, negative_wide, bad_advice, ESPIPE);

    int const retired{owner.native_handle()};
    CHECK(posix_close_nothrow(owner) && owner.native_handle() == -1);
    // No creation/open intervenes: retired cannot be another fixture's reused FD.
    compare_with_sdk(retired, negative_wide, negative_wide, bad_advice, EBADF);
    CHECK(posix_close_nothrow(reader));
    CHECK(posix_close_nothrow(writer));
    ::fast_io::io::println("PASS independent SDK Linux fadvise wide/error/state checks: ", checks);
}

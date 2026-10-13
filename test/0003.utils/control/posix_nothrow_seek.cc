#include "posix_test_abi.h"
#include <limits>
#include <fast_io.h>
#if !defined(__linux__) || !defined(__NR_memfd_create)
# error "This real-kernel seek fixture requires the Linux memfd provider; other platforms need their own fixture"
#endif
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); ::fast_io::fast_terminate(); } } while(false)
int main()
{
    using namespace ::fast_io;
    static_assert(noexcept(posix_seek_nothrow(posix_io_observer{-1}, 0, seekdir::beg)));
    CHECK(posix_seek_nothrow(posix_io_observer{-1}, 0, seekdir::beg).error == EBADF);
    // A real anonymous kernel file is not a pathname/capability or a guest
    // resource. Ownership transfers once after the native control operation.
    int const raw{::fast_io::system_call<__NR_memfd_create, int>("uwvm-posix-nothrow-seek", 1u /*MFD_CLOEXEC*/)};
    CHECK(!::fast_io::linux_system_call_fails(raw));
    posix_file file{raw};
    auto const observer{posix_io_observer{file.native_handle()}}; // Borrow ends before the owned file is closed.
    CHECK(posix_truncate_nothrow(observer, 5));
    CHECK(::control_test::posix_abi::lseek_noexcept(raw, 0, SEEK_END) == 5);
    auto position{posix_seek_nothrow(observer, 0, seekdir::cur)};
    CHECK(position && position.position == 5);
    position = posix_seek_nothrow(observer, 2, seekdir::beg);
    CHECK(position && position.position == 2);
    CHECK(::control_test::posix_abi::lseek_noexcept(raw, 0, SEEK_CUR) == 2);
    position = posix_seek_nothrow(observer, -1, seekdir::cur);
    CHECK(position && position.position == 1);
    CHECK(::control_test::posix_abi::lseek_noexcept(raw, 0, SEEK_CUR) == 1);
    position = posix_seek_nothrow(observer, -1, seekdir::end);
    CHECK(position && position.position == 4);
    CHECK(::control_test::posix_abi::lseek_noexcept(raw, 0, SEEK_CUR) == 4);
    CHECK(posix_seek_nothrow(observer, -1, seekdir::beg).error == EINVAL);
    CHECK(posix_seek_nothrow(observer, 0, static_cast<seekdir>(255u)).error == EINVAL);
    CHECK(::control_test::posix_abi::lseek_noexcept(raw, 0, SEEK_CUR) == 4);
    // A glibc/Bionic off_t32 build must still pass this real >4GiB check.
    using oracle_offset_type = ::control_test::posix_abi::seek_offset_t;
    static_assert(::std::numeric_limits<oracle_offset_type>::digits >= 63);
    if constexpr (::std::numeric_limits<oracle_offset_type>::digits >= 34)
    {
        constexpr ::fast_io::intfpos_t sparse{8589934592LL};
        position = posix_seek_nothrow(observer, sparse, seekdir::beg);
        CHECK(position && position.position == sparse);
        CHECK(::control_test::posix_abi::lseek_noexcept(raw, 0, SEEK_CUR) == static_cast<oracle_offset_type>(sparse));
        // Seeking allocates no memory/file contents and does not grow the file.
        struct ::stat actual{};
        CHECK(::control_test::posix_abi::fstat_noexcept(raw, __builtin_addressof(actual)) == 0 && actual.st_size == 5);
    }
    int pipe_descriptors[2]{-1, -1};
    // One complete two-FD cell is borrowed by pipe2; each success FD transfers
    // immediately to a distinct RAII owner, never retained as a raw owner.
    CHECK(::control_test::posix_abi::pipe2_noexcept(pipe_descriptors, O_CLOEXEC) == 0);
    posix_file input{pipe_descriptors[0]}, output{pipe_descriptors[1]};
    CHECK(posix_seek_nothrow(posix_io_observer{input.native_handle()}, 0, seekdir::cur).error == ESPIPE);
    CHECK(posix_close_nothrow(file));
    CHECK(posix_seek_nothrow(observer, 0, seekdir::cur).error == EBADF);
    ::fast_io::io::println("PASS independent SDK/native no-throw seek checks: ", checks);
}

#include "posix_test_abi.h"
#include <linux/memfd.h>
#include <fast_io.h>

#if !defined(__linux__) || !defined(__NR_memfd_create) || (!defined(__NR_fcntl) && !defined(__NR_fcntl64))
# error "This real-kernel F_SETFL fixture requires the target Linux syscall declarations; other providers need separate qualification"
#endif

// Run only through the sole SSH Linux keeper in the owned 64 GiB cgroup.
// The oracle uses the independent SDK-selected libc fcntl symbol, never the
// new production raw-syscall operation or its status/error conversion.
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); ::fast_io::fast_terminate(); } } while(false)

namespace
{
inline int oracle_get_flags(int descriptor) noexcept
{
    int const flags{::control_test::posix_abi::control_test_native_fcntl(descriptor, F_GETFL, 0)};
    CHECK(flags >= 0);
    return flags;
}
inline void check_unchanged_descriptor_flags(int descriptor, int expected) noexcept
{
    CHECK(::control_test::posix_abi::control_test_native_fcntl(descriptor, F_GETFD, 0) == expected);
}
inline void check_invalid_descriptor(int descriptor) noexcept
{
    errno = 0;
    int const native_result{::control_test::posix_abi::control_test_native_fcntl(descriptor, F_SETFL, O_NONBLOCK)};
    int const native_error{errno};
    CHECK(native_result == -1 && native_error == EBADF);
    // The Linux raw syscall returns its own native error and must not overwrite
    // the caller's errno, including on failure. No native owner is touched.
    errno = EDOM;
    auto const actual{::fast_io::posix_setfl_nothrow(::fast_io::posix_io_observer{descriptor}, O_NONBLOCK)};
    CHECK(!actual && actual.error == native_error);
    CHECK(errno == EDOM);
}
}

int main()
{
    using namespace ::fast_io;
    static_assert(noexcept(posix_setfl_nothrow(posix_io_observer{-1}, 0)));
    check_invalid_descriptor(-1);

    int const raw{::fast_io::system_call<__NR_memfd_create, int>("uwvm-posix-setfl", MFD_CLOEXEC)};
    CHECK(!::fast_io::linux_system_call_fails(raw));
    posix_file owner{raw}; // Successful creation transfers once into this RAII owner.
    auto const observer{posix_io_observer{owner.native_handle()}};
    int const before{oracle_get_flags(raw)};
    int const descriptor_flags{::control_test::posix_abi::control_test_native_fcntl(raw, F_GETFD, 0)};
    CHECK(descriptor_flags >= 0 && (descriptor_flags & FD_CLOEXEC) != 0);
    CHECK((before & O_ACCMODE) == O_RDWR && (before & (O_APPEND | O_NONBLOCK | O_SYNC)) == 0);
    constexpr char payload[]{"flag"};
    // [safe payload[0..4)] [safe NUL]
    //       ^ one complete four-byte initialized payload is borrowed until
    //         synchronous return; its terminator is not written or advanced.
    auto const seeded{posix_write_nothrow(observer, payload, sizeof(payload) - 1u)};
    CHECK(seeded && seeded.transferred == sizeof(payload) - 1u);
    struct ::stat native_before{};
    CHECK(::control_test::posix_abi::fstat_noexcept(raw, __builtin_addressof(native_before)) == 0 && native_before.st_size == 4);
    auto const before_position{::control_test::posix_abi::lseek_noexcept(raw, 0, SEEK_CUR)};
    CHECK(before_position == 4);

    // Deliberately supply another access mode and creation flags. The native
    // F_SETFL ABI ignores them; the operation must not synthesize reopen/truncate
    // or claim that access mode / FD_CLOEXEC was changed.
    int const enabled_request{(before & ~O_ACCMODE) | O_RDONLY | O_APPEND | O_NONBLOCK | O_CREAT | O_TRUNC};
    errno = EDOM;
    auto changed{posix_setfl_nothrow(observer, enabled_request)};
    CHECK(changed && changed.error == 0 && errno == EDOM);
    int current{oracle_get_flags(raw)};
    CHECK((current & (O_APPEND | O_NONBLOCK)) == (O_APPEND | O_NONBLOCK));
    CHECK((current & O_ACCMODE) == (before & O_ACCMODE));
    CHECK((current & ~(O_APPEND | O_NONBLOCK)) == (before & ~(O_APPEND | O_NONBLOCK)));
    struct ::stat native_after{};
    CHECK(::control_test::posix_abi::fstat_noexcept(raw, __builtin_addressof(native_after)) == 0 && native_after.st_size == native_before.st_size);
    CHECK(::control_test::posix_abi::lseek_noexcept(raw, 0, SEEK_CUR) == before_position);
    check_unchanged_descriptor_flags(raw, descriptor_flags);
    auto observed{posix_getfl_nothrow(observer)};
    CHECK(observed && observed.flags == current);

    changed = posix_setfl_nothrow(observer, current & ~O_NONBLOCK);
    CHECK(changed && changed.error == 0);
    current = oracle_get_flags(raw);
    CHECK((current & O_APPEND) != 0 && (current & O_NONBLOCK) == 0);
    CHECK((current & O_ACCMODE) == O_RDWR);
    check_unchanged_descriptor_flags(raw, descriptor_flags);

    // Independently mutate through libc, then clear through the public API.
    // This checks both directions against an actual open file description.
    CHECK(::control_test::posix_abi::control_test_native_fcntl(raw, F_SETFL, before | O_NONBLOCK) == 0);
    observed = posix_getfl_nothrow(observer);
    CHECK(observed && observed.flags == oracle_get_flags(raw));
    CHECK((observed.flags & O_NONBLOCK) != 0 && (observed.flags & O_APPEND) == 0);
    changed = posix_setfl_nothrow(observer, before);
    CHECK(changed && oracle_get_flags(raw) == before);

    // Linux documents that F_SETFL silently ignores O_SYNC/O_DSYNC. The native
    // zero return remains success in this one-call API. A caller that requires
    // exact managed bits performs its own get/verify/rollback transaction.
    int const sync_request{before | O_APPEND | O_SYNC};
    changed = posix_setfl_nothrow(observer, sync_request);
    CHECK(changed && changed.error == 0);
    auto const verified{posix_getfl_nothrow(observer)};
    CHECK(verified && verified.flags == oracle_get_flags(raw));
    CHECK((verified.flags & O_APPEND) != 0 && (verified.flags & O_SYNC) == 0);
    CHECK((verified.flags & (O_APPEND | O_SYNC)) != (sync_request & (O_APPEND | O_SYNC)));
    auto const rollback{posix_setfl_nothrow(observer, before)};
    CHECK(rollback && rollback.error == 0 && oracle_get_flags(raw) == before);
    check_unchanged_descriptor_flags(raw, descriptor_flags);

    int descriptors[2]{-1, -1};
    // [safe descriptors[0] descriptors[1]]
    //       ^ SDK pipe2 borrows exactly this complete two-FD cell until return.
    // On success both numeric descriptors immediately enter separate owners.
    CHECK(::control_test::posix_abi::pipe2_noexcept(descriptors, O_CLOEXEC) == 0);
    posix_file reader{descriptors[0]}, writer{descriptors[1]};
    auto const pipe_reader{posix_io_observer{reader.native_handle()}};
    auto const pipe_writer{posix_io_observer{writer.native_handle()}};
    int const reader_before{oracle_get_flags(reader.native_handle())};
    int const writer_before{oracle_get_flags(writer.native_handle())};
    CHECK((reader_before & O_ACCMODE) == O_RDONLY && (reader_before & O_NONBLOCK) == 0);
    CHECK((writer_before & O_ACCMODE) == O_WRONLY && (writer_before & O_NONBLOCK) == 0);
    CHECK(posix_setfl_nothrow(pipe_reader, reader_before | O_NONBLOCK));
    CHECK(posix_setfl_nothrow(pipe_writer, writer_before | O_NONBLOCK));
    CHECK(oracle_get_flags(reader.native_handle()) == (reader_before | O_NONBLOCK));
    CHECK(oracle_get_flags(writer.native_handle()) == (writer_before | O_NONBLOCK));

    char byte{};
    // [safe byte]
    //       ^ the public read borrows one complete writable byte through return;
    //         no pointer is advanced and the still-open writer prevents EOF.
    auto const empty_read{posix_read_nothrow(pipe_reader, __builtin_addressof(byte), sizeof(byte))};
    CHECK(!empty_read && (empty_read.error == EAGAIN || empty_read.error == EWOULDBLOCK));
    CHECK(posix_setfl_nothrow(pipe_reader, reader_before));
    CHECK(posix_setfl_nothrow(pipe_writer, writer_before));
    CHECK(oracle_get_flags(reader.native_handle()) == reader_before);
    CHECK(oracle_get_flags(writer.native_handle()) == writer_before);

    // No file opens occur between retirement and the invalid-number probes;
    // the closed descriptor cannot be reused by a subsequent fixture operation.
    int const retired{owner.native_handle()};
    CHECK(posix_close_nothrow(owner) && owner.native_handle() == -1);
    check_invalid_descriptor(retired);
    CHECK(posix_close_nothrow(reader));
    CHECK(posix_close_nothrow(writer));
    ::fast_io::io::println("PASS independent SDK/kernel F_SETFL checks: ", checks);
}

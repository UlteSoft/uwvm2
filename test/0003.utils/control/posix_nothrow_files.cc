#include "posix_test_abi.h"
#include <array>
#include <limits>
#include <utility>
#include <fast_io.h>
#include <uwvm2/utils/control/sealed_input.h>

// Real kernel operations exercise one fast_io implementation in EH/noEH builds.
// This unit is only executed by the SSH Linux keeper inside the owned cgroup.
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); ::fast_io::fast_terminate(); } } while(false)
int main()
{
    using namespace ::fast_io;
    CHECK(posix_status_nothrow(posix_io_observer{-1}).error == EBADF);
    CHECK(posix_truncate_nothrow(posix_io_observer{-1}, 0).error == EBADF);
    CHECK(posix_read_nothrow(posix_io_observer{-1}, nullptr, 0).error == EBADF);
    CHECK(posix_write_nothrow(posix_io_observer{-1}, nullptr, 0).error == EBADF);
    CHECK(posix_fsync_nothrow(posix_io_observer{-1}).error == EBADF);
    posix_file invalid{};
    CHECK(posix_close_nothrow(invalid).error == EBADF);
    CHECK(invalid.native_handle() == -1);
    char directory_name[]{"/dev/shm/uwvm-posix-nothrow-XXXXXX"};
    CHECK(::control_test::posix_abi::mkdtemp_noexcept(directory_name) != nullptr);
    auto directory{posix_openat_nothrow(posix_at_fdcwd(), directory_name, O_RDONLY | O_DIRECTORY | O_CLOEXEC)};
    CHECK(directory);
    auto const entry{at(directory.file)}; // Borrow only while the directory owner remains alive.
    auto missing{posix_openat_nothrow(entry, "missing", O_WRONLY | O_CLOEXEC)};
    CHECK(!missing && missing.error == ENOENT && missing.file.native_handle() == -1);
    CHECK(posix_fstatat_nothrow(entry, "missing").error == ENOENT);
    auto seed{posix_openat_nothrow(entry, "commands", O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, static_cast<perms>(0600))};
    CHECK(seed);
    constexpr char command_bytes[]{"help\n"};
    // The write borrows the five initialized payload bytes of this one static
    // array, excluding its NUL cell. No cursor or pointer is advanced.
    auto const seeded_write{posix_write_nothrow(posix_io_observer{seed.file.native_handle()}, command_bytes, sizeof(command_bytes) - 1u)};
    CHECK(seeded_write && seeded_write.transferred == 5);
    auto const seeded{posix_status_nothrow(posix_io_observer{seed.file.native_handle()})};
    CHECK(seeded && seeded.value.type == file_type::regular && seeded.value.size == 5);
    // The independent SDK-native fstat oracle returns the raw libc structure;
    // the fast_io conversion is deliberately not used to compute its fields.
    struct ::stat native_seed{};
    CHECK(::control_test::posix_abi::fstat_noexcept(seed.file.native_handle(), __builtin_addressof(native_seed)) == 0);
    CHECK(S_ISREG(native_seed.st_mode) && native_seed.st_size == 5);
    CHECK(seeded.value.dev == static_cast<::std::uint_least64_t>(native_seed.st_dev) &&
        seeded.value.ino == static_cast<::std::uint_least64_t>(native_seed.st_ino));
    // tmpfs validates real API success/error semantics only. This is not
    // a power-loss or persistent-volume durability qualification.
    CHECK(posix_fsync_nothrow(posix_io_observer{seed.file.native_handle()}));
    CHECK(posix_fsync_nothrow(posix_io_observer{directory.file.native_handle()}));
    auto const by_name{posix_fstatat_nothrow(entry, "commands")};
    CHECK(by_name && by_name.value.dev == seeded.value.dev && by_name.value.ino == seeded.value.ino);
    struct ::stat native_name{};
    CHECK(::control_test::posix_abi::fstatat_noexcept(entry.fd, "commands", __builtin_addressof(native_name), 0) == 0);
    CHECK(by_name.value.size == static_cast<::fast_io::uintfpos_t>(native_name.st_size) &&
        by_name.value.dev == static_cast<::std::uint_least64_t>(native_name.st_dev) &&
        by_name.value.ino == static_cast<::std::uint_least64_t>(native_name.st_ino));
    CHECK((::control_test::posix_abi::fcntl_noexcept(seed.file.native_handle(), F_GETFD) & FD_CLOEXEC) != 0);
    auto write_only{posix_openat_nothrow(entry, "commands", O_WRONLY | O_CLOEXEC)};
    CHECK(write_only && posix_status_nothrow(posix_io_observer{write_only.file.native_handle()}).value.size == 5);
    CHECK((::control_test::posix_abi::fcntl_noexcept(write_only.file.native_handle(), F_GETFL) & O_ACCMODE) == O_WRONLY);
    auto const zero_write{posix_write_nothrow(posix_io_observer{write_only.file.native_handle()}, nullptr, 0)};
    CHECK(zero_write && zero_write.transferred == 0);
    int const once_closed{write_only.file.native_handle()};
    CHECK(posix_close_nothrow(write_only.file));
    CHECK(write_only.file.native_handle() == -1);
    CHECK(posix_status_nothrow(posix_io_observer{once_closed}).error == EBADF);
    // A second close consumes only the invalid owner, never a reused native fd.
    CHECK(posix_close_nothrow(write_only.file).error == EBADF);
    auto read_only{posix_openat_nothrow(entry, "commands", O_RDONLY | O_CLOEXEC)};
    CHECK(read_only);
    CHECK(posix_write_nothrow(posix_io_observer{read_only.file.native_handle()}, command_bytes, 5).error == EBADF);
    CHECK(posix_write_nothrow(posix_io_observer{seed.file.native_handle()}, nullptr,
        ::std::numeric_limits<::std::size_t>::max()).error == EINVAL);
    // pipe2 borrows one complete two-descriptor cell. On success each returned
    // descriptor transfers immediately to exactly one local RAII owner.
    int pipe_descriptors[2]{-1, -1};
    CHECK(::control_test::posix_abi::pipe2_noexcept(pipe_descriptors, O_CLOEXEC | O_NONBLOCK) == 0);
    posix_file pipe_input{pipe_descriptors[0]}, pipe_output{pipe_descriptors[1]};
    CHECK(posix_fsync_nothrow(posix_io_observer{pipe_output.native_handle()}).error == EINVAL);
    int const capacity{::control_test::posix_abi::fcntl_noexcept(pipe_output.native_handle(), F_GETPIPE_SZ)};
    CHECK(capacity >= 4096 && capacity <= 1048576);
    ::std::array<char, 8192> pipe_payload{};
    ::std::size_t filled{};
    bool full{};
    // A fresh kernel pipe needs at most capacity/8192+2 one-write attempts;
    // all writes borrow the same complete bounded array, without retries.
    for(unsigned attempts{}; attempts != 258u; ++attempts)
    {
        auto const next{posix_write_nothrow(posix_io_observer{pipe_output.native_handle()}, pipe_payload.data(), pipe_payload.size())};
        if(!next)
        {
            CHECK(next.error == EAGAIN || next.error == EWOULDBLOCK);
            full = true;
            break;
        }
        CHECK(next.transferred != 0 && next.transferred <= pipe_payload.size());
        CHECK(next.transferred <= static_cast<::std::size_t>(capacity) &&
            filled <= static_cast<::std::size_t>(capacity) - next.transferred);
        filled += next.transferred;
    }
    CHECK(full && filled == static_cast<::std::size_t>(capacity));
    // Drain exactly one Linux PIPE_BUF from the full pipe. The buffer borrow
    // stays within its first 4096 bytes; an 8192-byte nonblocking write must
    // then return a real partial transfer, rather than loop or invent success.
    auto const drained{posix_read_nothrow(posix_io_observer{pipe_input.native_handle()}, pipe_payload.data(), 4096)};
    CHECK(drained && drained.transferred == 4096);
    auto const partial{posix_write_nothrow(posix_io_observer{pipe_output.native_handle()}, pipe_payload.data(), pipe_payload.size())};
    CHECK(partial && partial.transferred != 0 && partial.transferred < pipe_payload.size());
    CHECK(posix_close_nothrow(pipe_output));
    CHECK(posix_close_nothrow(pipe_input));
    ::std::array<char, 8> bytes{};
    auto const read{posix_read_nothrow(posix_io_observer{read_only.file.native_handle()}, bytes.data(), 5)};
    CHECK(read && read.transferred == 5);
    CHECK(bytes[0] == 'h' && bytes[1] == 'e' && bytes[2] == 'l' && bytes[3] == 'p' && bytes[4] == '\n');
    // The previous read wrote [bytes.data(),bytes.data()+5); bytes[5..8) remain
    // owned spare capacity. This EOF probe borrows one spare byte only.
    auto const eof{posix_read_nothrow(posix_io_observer{read_only.file.native_handle()}, bytes.data() + 5, 1)};
    CHECK(eof && eof.transferred == 0);
    CHECK(posix_read_nothrow(posix_io_observer{read_only.file.native_handle()}, nullptr,
        ::std::numeric_limits<::std::size_t>::max()).error == EINVAL);
    auto const too_large{static_cast<::fast_io::uintfpos_t>(::std::numeric_limits<::off_t>::max()) + 1u};
    CHECK(posix_truncate_nothrow(posix_io_observer{seed.file.native_handle()}, too_large).error == EINVAL);
    CHECK(posix_status_nothrow(posix_io_observer{seed.file.native_handle()}).value.size == 5);
    CHECK(::uwvm2::utils::control::seal_console_input_host_api(seed.file.native_handle()) == ::uwvm2::utils::control::sealed_input_status::ok);
    auto denied{::uwvm2::utils::control::open_guest_path_sealed_host_api(entry.fd, "commands", O_WRONLY | O_TRUNC, 0600)};
    CHECK(denied.descriptor == -1 && denied.error == EACCES);
    CHECK(posix_status_nothrow(posix_io_observer{seed.file.native_handle()}).value.size == 5);
    auto benign{::uwvm2::utils::control::open_guest_path_sealed_host_api(entry.fd, "other", O_RDWR | O_CREAT | O_TRUNC, 0600)};
    CHECK(benign.descriptor >= 0 && benign.error == 0);
    posix_file other{benign.descriptor}; // Adopt the sole published result exactly once.
    CHECK(posix_status_nothrow(posix_io_observer{other.native_handle()}).value.size == 0);
    CHECK(posix_truncate_nothrow(posix_io_observer{other.native_handle()}, 3));
    CHECK(posix_status_nothrow(posix_io_observer{other.native_handle()}).value.size == 3);
    // Exercise each direction: public truncate observed by raw fstat, then
    // raw libc truncate observed by public status. Neither oracle calls fast_io.
    struct ::stat native_other{};
    CHECK(::control_test::posix_abi::fstat_noexcept(other.native_handle(), __builtin_addressof(native_other)) == 0 && native_other.st_size == 3);
    CHECK(::control_test::posix_abi::ftruncate_noexcept(other.native_handle(), 7) == 0);
    CHECK(posix_status_nothrow(posix_io_observer{other.native_handle()}).value.size == 7);
    ::uwvm2::utils::control::unseal_console_input_after_guest_drain_host_api();
    CHECK(posix_close_nothrow(other));
    CHECK(posix_close_nothrow(read_only.file));
    CHECK(posix_close_nothrow(seed.file));
    ::fast_io::native_unlinkat(entry, "other");
    ::fast_io::native_unlinkat(entry, "commands");
    CHECK(posix_close_nothrow(directory.file));
    CHECK(::control_test::posix_abi::rmdir_noexcept(directory_name) == 0);
    ::fast_io::io::println("PASS fast_io POSIX nothrow: ", checks, " real errors, exact flags, partial write, EOF, fsync, close retirement and deferred truncation checks");
}

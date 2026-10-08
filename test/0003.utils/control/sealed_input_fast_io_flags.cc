#include "posix_test_abi.h"
#include <fast_io.h>
#include <uwvm2/utils/control/sealed_input.h>

#if !defined(__linux__) || !defined(__NR_memfd_create) || !defined(O_PATH)
# error "This sealed-input fixture requires actual Linux memfd/procfs/O_PATH providers"
#endif

// Source-only until the sole SSH Linux keeper compiles and runs this unit in
// the owned cgroup. No guest, stdin mutation, broker or debugger is started.
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); ::fast_io::fast_terminate(); } } while(false)

int main()
{
    namespace ctl = ::uwvm2::utils::control;
    using namespace ::fast_io;
    static_assert(noexcept(posix_getfl_nothrow(posix_io_observer{-1})));
    CHECK(!ctl::console_input_sealed());
    auto const invalid_flags{posix_getfl_nothrow(posix_io_observer{-1})};
    CHECK(!invalid_flags && invalid_flags.error == EBADF && invalid_flags.flags == 0);
    CHECK(ctl::seal_console_input_host_api(-1) == ctl::sealed_input_status::invalid_descriptor);
    CHECK(!ctl::console_input_sealed());

    // The literal name borrows its complete NUL-terminated static array only
    // through this actual kernel call. Its FD transfers immediately to RAII.
    int const descriptor{::fast_io::system_call<__NR_memfd_create, int>("uwvm-sealed-getfl", 1u /* MFD_CLOEXEC */)};
    CHECK(!::fast_io::linux_system_call_fails(descriptor));
    posix_file seed{descriptor};
    auto const seed_flags{::control_test::posix_abi::control_test_native_fcntl(seed.native_handle(), F_GETFL, 0)};
    CHECK(seed_flags >= 0 && (seed_flags & O_ACCMODE) == O_RDWR);
    auto const path{::fast_io::concat_std("/proc/self/fd/", ::fast_io::mnp::dec(seed.native_handle()))};
    // Seed remains owned while each trusted procfs pathname is synchronously
    // borrowed. No guest pathname, arbitrary host read or filesystem write occurs.
    auto write_only{posix_openat_nothrow(posix_at_fdcwd(), path.c_str(), O_WRONLY | O_CLOEXEC)};
    CHECK(write_only);
    int const writer_before{::control_test::posix_abi::control_test_native_fcntl(write_only.file.native_handle(), F_GETFL, 0)};
    CHECK(writer_before >= 0 && (writer_before & O_ACCMODE) == O_WRONLY);
    CHECK(ctl::seal_console_input_host_api(write_only.file.native_handle()) == ctl::sealed_input_status::invalid_descriptor);
    CHECK(!ctl::console_input_sealed());
    CHECK(::control_test::posix_abi::control_test_native_fcntl(write_only.file.native_handle(), F_GETFL, 0) == writer_before);
    CHECK(ctl::inspect_guest_file_host_api(write_only.file.native_handle()) == ctl::sealed_input_decision::allow);

    auto path_only{posix_openat_nothrow(posix_at_fdcwd(), path.c_str(), O_PATH | O_CLOEXEC)};
    CHECK(path_only);
    int const path_before{::control_test::posix_abi::control_test_native_fcntl(path_only.file.native_handle(), F_GETFL, 0)};
    CHECK(path_before >= 0 && (path_before & O_PATH) != 0);
    CHECK(ctl::seal_console_input_host_api(path_only.file.native_handle()) == ctl::sealed_input_status::invalid_descriptor);
    CHECK(!ctl::console_input_sealed());
    CHECK(::control_test::posix_abi::control_test_native_fcntl(path_only.file.native_handle(), F_GETFL, 0) == path_before);

    auto read_only{posix_openat_nothrow(posix_at_fdcwd(), path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC)};
    CHECK(read_only);
    int const reader_before{::control_test::posix_abi::control_test_native_fcntl(read_only.file.native_handle(), F_GETFL, 0)};
    CHECK(reader_before >= 0 && (reader_before & O_ACCMODE) == O_RDONLY && (reader_before & O_NONBLOCK) != 0);
    CHECK(ctl::seal_console_input_host_api(read_only.file.native_handle()) == ctl::sealed_input_status::ok);
    CHECK(ctl::console_input_sealed());
    CHECK(ctl::seal_console_input_host_api(read_only.file.native_handle()) == ctl::sealed_input_status::already_sealed);
    CHECK(ctl::inspect_guest_file_host_api(read_only.file.native_handle()) == ctl::sealed_input_decision::denied);
    CHECK(ctl::inspect_guest_file_host_api(seed.native_handle()) == ctl::sealed_input_decision::denied);
    CHECK(::control_test::posix_abi::control_test_native_fcntl(read_only.file.native_handle(), F_GETFL, 0) == reader_before);
    // No guest was admitted: host drain is already complete. Retirement
    // releases only the retained duplicate, preserving the original owner.
    ctl::unseal_console_input_after_guest_drain_host_api();
    CHECK(!ctl::console_input_sealed());
    CHECK(::control_test::posix_abi::control_test_native_fcntl(read_only.file.native_handle(), F_GETFL, 0) == reader_before);
    CHECK(ctl::inspect_guest_file_host_api(seed.native_handle()) == ctl::sealed_input_decision::allow);

    CHECK(ctl::seal_console_input_host_api(seed.native_handle()) == ctl::sealed_input_status::ok);
    CHECK(ctl::console_input_sealed());
    CHECK(::control_test::posix_abi::control_test_native_fcntl(seed.native_handle(), F_GETFL, 0) == seed_flags);
    ctl::unseal_console_input_after_guest_drain_host_api();
    CHECK(!ctl::console_input_sealed());
    int const retired{write_only.file.native_handle()};
    CHECK(posix_close_nothrow(write_only.file));
    // Nothing opens another FD between this retirement and seal attempt. The
    // duplicate syscall must report EBADF before any identity can be published.
    CHECK(ctl::seal_console_input_host_api(retired) == ctl::sealed_input_status::invalid_descriptor);
    CHECK(!ctl::console_input_sealed());
    ::fast_io::io::println("PASS real sealed-input FastIO flag/access-mode checks: ", checks);
}

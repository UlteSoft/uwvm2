#include <uwvm2/utils/control/sealed_input.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <util.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ctl = uwvm2::utils::control;
static void require(bool value, int line)
{
    if(!value) { std::fprintf(stderr, "FAIL macOS sealed input line %d\n", line); std::abort(); }
}
#define CHECK(value) require(bool(value), __LINE__)

int main()
{
    int input[2]{}, unrelated[2]{};
    CHECK(::pipe(input) == 0 && ::pipe(unrelated) == 0);
    CHECK(ctl::seal_console_input_host_api(input[0]) == ctl::sealed_input_status::ok);
    CHECK(ctl::seal_console_input_host_api(input[0]) == ctl::sealed_input_status::already_sealed);
    // Darwin gives pipe read/write ends different st_ino values. The kernel's
    // paired pipe handles must still identify both as debugger input aliases.
    CHECK(ctl::inspect_guest_file_host_api(input[0]) == ctl::sealed_input_decision::denied);
    CHECK(ctl::inspect_guest_file_host_api(input[1]) == ctl::sealed_input_decision::denied);
    CHECK(ctl::inspect_guest_output_host_api(input[1]) == ctl::sealed_input_decision::denied);
    CHECK(ctl::inspect_guest_output_host_api(unrelated[1]) == ctl::sealed_input_decision::allow);
    int duplicate{::dup(input[0])}; CHECK(duplicate >= 0);
    CHECK(ctl::inspect_guest_file_host_api(duplicate) == ctl::sealed_input_decision::denied);
    char fd_path[32]{};
    std::snprintf(fd_path, sizeof(fd_path), "/dev/fd/%d", input[0]);
    auto const reopened_pipe{ctl::open_guest_path_sealed_host_api(AT_FDCWD, fd_path, O_RDONLY, 0600)};
    CHECK(reopened_pipe.descriptor == -1);
    ::close(duplicate);
    ctl::unseal_console_input_after_guest_drain_host_api();
    ::close(input[0]); ::close(input[1]); ::close(unrelated[0]); ::close(unrelated[1]);

    char protected_path[]{"/tmp/uwvm-macos-sealed-XXXXXX"};
    int protected_file{::mkstemp(protected_path)}; CHECK(protected_file >= 0);
    char alias_path[sizeof(protected_path) + 8]{};
    std::snprintf(alias_path, sizeof(alias_path), "%s.alias", protected_path);
    CHECK(::write(protected_file, "sealed", 6) == 6);
    CHECK(::link(protected_path, alias_path) == 0);
    CHECK(ctl::seal_console_input_host_api(protected_file) == ctl::sealed_input_status::ok);
    auto rejected{ctl::open_guest_path_sealed_host_api(AT_FDCWD, alias_path, O_RDWR | O_TRUNC, 0600)};
    CHECK(rejected.descriptor == -1 && rejected.error == EACCES);
    struct stat unchanged{}; CHECK(::fstat(protected_file, &unchanged) == 0 && unchanged.st_size == 6);
    char other_path[]{"/tmp/uwvm-macos-other-XXXXXX"};
    int other{::mkstemp(other_path)}; CHECK(other >= 0);
    CHECK(::write(other, "other", 5) == 5);
    auto accepted{ctl::open_guest_path_sealed_host_api(AT_FDCWD, other_path, O_RDWR | O_TRUNC, 0600)};
    CHECK(accepted.descriptor >= 0 && accepted.error == 0);
    struct stat truncated{}; CHECK(::fstat(accepted.descriptor, &truncated) == 0 && truncated.st_size == 0);
    ::close(accepted.descriptor); ::close(other);
    ctl::unseal_console_input_after_guest_drain_host_api();
    ::close(protected_file); ::unlink(alias_path); ::unlink(protected_path); ::unlink(other_path);

    int master{}, slave{};
    char slave_path[128]{};
    CHECK(::openpty(&master, &slave, slave_path, nullptr, nullptr) == 0);
    CHECK(ctl::seal_console_input_host_api(master) != ctl::sealed_input_status::ok);
    CHECK(ctl::seal_console_input_host_api(slave) == ctl::sealed_input_status::ok);
    CHECK(ctl::inspect_guest_file_host_api(master) == ctl::sealed_input_decision::denied);
    CHECK(ctl::inspect_guest_output_host_api(master) == ctl::sealed_input_decision::denied);
    CHECK(ctl::inspect_guest_output_host_api(slave) == ctl::sealed_input_decision::allow);
    CHECK(ctl::inspect_guest_file_host_api(slave) == ctl::sealed_input_decision::denied);
    auto const reopened{ctl::open_guest_path_sealed_host_api(AT_FDCWD, slave_path, O_RDWR, 0600)};
    CHECK(reopened.descriptor == -1 && reopened.error == EACCES);
    auto const null_device{ctl::open_guest_path_sealed_host_api(AT_FDCWD, "/dev/null", O_WRONLY, 0600)};
    CHECK(null_device.descriptor >= 0 && null_device.error == 0);
    ::close(null_device.descriptor);
    ctl::unseal_console_input_after_guest_drain_host_api();
    ::close(slave); ::close(master);
    std::puts("PASS macOS sealed console pipe, regular, PTY master/slave, deferred truncate");
}

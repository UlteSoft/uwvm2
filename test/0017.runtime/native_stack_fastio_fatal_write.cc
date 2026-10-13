// Execute the actual fatal setup path with writable and read-only stderr.
// Native execution belongs to the 64 GiB test cgroup; compile both EH/no-EH.
#include <uwvm2/runtime/lib/uwvm_runtime_native_stack_guard.h>
#include <array>
#include <cerrno>
#include <cstring>

#if (defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))) && !defined(_WIN32)
#include <sys/wait.h>
#include "../posix_noexcept_abi.h"
namespace native = ::uwvm2::runtime::lib::native_stack;
namespace test_posix = ::uwvm2test::posix_abi;

static bool check_setup_failure(bool read_only)
{
    ::fast_io::posix_pipe output{};
    auto const child{test_posix::fork_noexcept()};
    if(child < 0) { return false; }
    if(child == 0)
    {
        int const source{read_only ? output.in().native_handle() : output.out().native_handle()};
        if(test_posix::dup2_noexcept(source, ::fast_io::posix_stderr_number) < 0)
        { test_posix::_exit_noexcept(98); }
        native::setup_failed();
    }
    output.out().reset();
    ::std::array<char, 256u> bytes{};
    ::std::size_t consumed{};
    bool valid{true};
    while(consumed != bytes.size())
    {
        // [safe received][safe remaining capacity] unsafe (one-past bytes)
        //                ^^ consumed < size proved before deriving this cursor.
        auto const remaining{bytes.size() - consumed};
        auto const read{::fast_io::posix_read_nothrow(output.in(), bytes.data() + consumed, remaining)};
        if(read.error == EINTR) { continue; }
        if(read.error != 0 || read.transferred > remaining) { valid = false; break; }
        if(read.transferred == 0u) { break; }
        // [safe received + transferred][safe unused] unsafe (one-past bytes)
        //                              ^^ transfer <= remaining bounds new index.
        consumed += read.transferred;
    }
    if(consumed == bytes.size()) { valid = false; }
    int status{};
    if(test_posix::waitpid_noexcept(child, &status, 0) != child ||
       !WIFEXITED(status) || WEXITSTATUS(status) != 126) { return false; }
    constexpr char expected[]{"uwvm: [fatal] cannot establish native stack fault handling.\n"};
    if(read_only) { return valid && consumed == 0u; }
    return valid && consumed == sizeof(expected) - 1u &&
           ::std::memcmp(bytes.data(), expected, sizeof(expected) - 1u) == 0;
}

int main()
{
    bool const writable{check_setup_failure(false)};
    bool const failed_write{check_setup_failure(true)};
    ::fast_io::println(::fast_io::out(), "native fast_io fatal write: writable=", writable,
                       " readonly-error=", failed_write);
    return writable && failed_write ? 0 : 1;
}
#else
int main() { return 77; }
#endif

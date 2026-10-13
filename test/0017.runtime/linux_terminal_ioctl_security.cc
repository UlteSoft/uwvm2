// Define UWVM_TEST_WRAP_IOCTL and link --wrap=ioctl for fault-injection checks.
#if !defined(__linux__)
int main() { return 0; }
#else
#include <uwvm2/utils/control/sealed_input.h>
#include <fast_io_dsal/string.h>
#include <fast_io_device.h>
#include <pty.h>
#include <cstdarg>
#include <stdexcept>
#if !defined(TIOCGDEV) || !defined(TIOCGPTN)
int main() { fast_io::io::println("SKIP Linux terminal identity ioctls unavailable"); }
#else
namespace ctl = uwvm2::utils::control;
#if defined(UWVM_TEST_WRAP_IOCTL)
static int unavailable_fd{-1};
static bool unavailable_all_devices{};
extern "C" int __real_ioctl(int, unsigned long, ...);
extern "C" int __wrap_ioctl(int fd, unsigned long request, ...)
{
    va_list arguments;
    va_start(arguments, request);
    auto* output = va_arg(arguments, void*);
    va_end(arguments);
    if((unavailable_all_devices || fd == unavailable_fd) && request == TIOCGDEV) { errno = ENOTTY; return -1; }
    return __real_ioctl(fd, request, output);
}
#endif
static void require(bool condition, char const* label)
{
    fast_io::io::println("TERMINAL_CHECK ", fast_io::mnp::os_c_str(label), " ",
        fast_io::mnp::os_c_str(condition ? "PASS" : "FAIL"));
    if(!condition) { fast_io::fast_terminate(); }
}
int main()
{
#if defined(__cpp_exceptions)
    try
#endif
    {
        int master{}, slave{};
        require(fast_io::noexcept_call(::openpty, &master, &slave, nullptr, nullptr, nullptr) == 0, "real PTY pair");
        fast_io::native_file master_owner{master}, slave_owner{slave};
        require(ctl::seal_console_input_host_api(master) == ctl::sealed_input_status::identity_unavailable,
            "master command injection rejected");
        require(!ctl::console_input_sealed(), "rejected master publishes nothing");
        unsigned device{};
        auto const query{ctl::details::query_terminal_device(slave, TIOCGDEV, device)};
        fast_io::io::println("TERMINAL_IDENTITY_QUERY ", query);
        auto const sealed{ctl::seal_console_input_host_api(slave)};
        if(query == 1)
        {
            require(sealed == ctl::sealed_input_status::ok, "supported slave input remains usable");
            require(ctl::inspect_guest_file_host_api(slave) == ctl::sealed_input_decision::denied, "slave input alias denied");
            require(ctl::inspect_guest_output_host_api(slave) == ctl::sealed_input_decision::allow, "launch slave output usable");
            require(ctl::inspect_guest_output_host_api(master) == ctl::sealed_input_decision::denied, "master output injection denied");
            fast_io::native_file duplicate{fast_io::io_dup, slave_owner};
#if defined(UWVM_TEST_WRAP_IOCTL)
            unavailable_fd = duplicate.native_handle();
            require(ctl::inspect_guest_file_host_api(unavailable_fd) == ctl::sealed_input_decision::identity_unavailable,
                "unavailable guest identity cannot grant access");
            require(ctl::inspect_guest_output_host_api(unavailable_fd) == ctl::sealed_input_decision::identity_unavailable,
                "unavailable output identity cannot grant access");
            unavailable_fd = -1;
#endif
            fast_io::native_file sink{"/dev/null", fast_io::open_mode::out};
            require(ctl::inspect_guest_output_host_api(sink.native_handle()) == ctl::sealed_input_decision::allow,
                "exact null output sink remains usable");
            ctl::unseal_console_input_after_guest_drain_host_api();
        }
        else
        {
            require(sealed == ctl::sealed_input_status::identity_unavailable, "unsupported slave identity fails closed");
            require(!ctl::console_input_sealed(), "unsupported slave publishes nothing");
        }
#if defined(UWVM_TEST_WRAP_IOCTL)
        // Exercise the same missing-query class on a real slave even natively.
        unavailable_all_devices = true;
        require(ctl::seal_console_input_host_api(slave) == ctl::sealed_input_status::identity_unavailable,
            "unsupported startup identity fails closed");
        require(!ctl::console_input_sealed(), "startup identity failure publishes nothing");
        unavailable_all_devices = false;
#endif
        fast_io::native_file null_input{"/dev/null", fast_io::open_mode::in};
        require(ctl::seal_console_input_host_api(null_input.native_handle()) == ctl::sealed_input_status::ok,
            "exact null input remains usable");
        require(ctl::inspect_guest_file_host_api(null_input.native_handle()) == ctl::sealed_input_decision::denied,
            "sealed null input alias denied");
        ctl::unseal_console_input_after_guest_drain_host_api();
        fast_io::native_pipe pipe;
        require(ctl::seal_console_input_host_api(pipe.in().native_handle()) == ctl::sealed_input_status::ok,
            "pipe command input remains usable");
        require(ctl::inspect_guest_output_host_api(pipe.out().native_handle()) == ctl::sealed_input_decision::denied,
            "pipe peer injection denied");
        ctl::unseal_console_input_after_guest_drain_host_api();
        fast_io::native_white_hole entropy;
        std::uint64_t token{};
        auto* bytes = reinterpret_cast<std::byte*>(&token);
        fast_io::operations::read_all_bytes(entropy, bytes, bytes + sizeof(token));
        auto name = fast_io::concat_fast_io("/tmp/uwvm-terminal-security-", fast_io::mnp::hex(token));
        fast_io::native_file file{name, fast_io::open_mode::in | fast_io::open_mode::out |
            fast_io::open_mode::creat | fast_io::open_mode::excl, static_cast<fast_io::perms>(0600)};
        struct cleanup
        {
            fast_io::string const& name;
            ~cleanup()
            {
#if defined(__cpp_exceptions)
                try { fast_io::native_unlinkat(fast_io::posix_at_fdcwd(), name); } catch(...) {}
#else
                fast_io::native_unlinkat(fast_io::posix_at_fdcwd(), name);
#endif
            }
        } remove{name};
        require(ctl::seal_console_input_host_api(file.native_handle()) == ctl::sealed_input_status::ok,
            "regular command input remains usable");
        require(ctl::inspect_guest_output_host_api(file.native_handle()) == ctl::sealed_input_decision::denied,
            "regular input output alias denied");
        ctl::unseal_console_input_after_guest_drain_host_api();
        fast_io::io::println("linux_terminal_ioctl_security PASS");
    }
#if defined(__cpp_exceptions)
    catch(std::exception const& error)
    {
        fast_io::io::perrln("terminal security failure ", fast_io::mnp::os_c_str(error.what()));
        return 1;
    }
#endif
}
#endif
#endif

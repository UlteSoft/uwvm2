#include <cerrno>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>

#if defined(UWVM) && (defined(__unix__) || defined(__APPLE__))
# include <sys/types.h>
# include <sys/wait.h>
# include <unistd.h>
#endif

#include <fast_io_dsal/string_view.h>

#include <uwvm2/imported/wasi/wasip1/func/base.h>

namespace
{
    using alignment_error_function = void (*)(::std::uint_least64_t, ::std::size_t, ::fast_io::u8string_view) noexcept;

    static_assert(::std::is_same_v<decltype(&::uwvm2::imported::wasi::wasip1::func::output_wasip1_guest_pointer_alignment_error),
                                   alignment_error_function>);

    consteval bool check_alignment_name_views()
    {
        ::uwvm2::imported::wasi::wasip1::func::check_wasip1_guest_pointer_alignment<4uz>(4u, u8"literal name");

        char8_t const embedded_null_name[]{u8'n', u8'a', u8'm', u8'e', u8'\0', u8'x'};
        ::uwvm2::imported::wasi::wasip1::func::check_wasip1_guest_pointer_alignment<8uz>(
            8u, ::fast_io::u8string_view{embedded_null_name, sizeof(embedded_null_name)});
        return true;
    }

    static_assert(check_alignment_name_views());

#if defined(UWVM) && (defined(__unix__) || defined(__APPLE__))
    [[nodiscard]] int fail(char const* message) noexcept
    {
        ::std::fprintf(stderr, "guest_pointer_alignment_name: %s\n", message);
        return 1;
    }

    [[nodiscard]] bool capture_alignment_diagnostic(bool enable_color, ::std::string& output)
    {
        int pipe_fds[2]{};
        if(::pipe(pipe_fds) != 0) { return false; }

        pid_t const child_pid{::fork()};
        if(child_pid < 0)
        {
            ::close(pipe_fds[0]);
            ::close(pipe_fds[1]);
            return false;
        }

        if(child_pid == 0)
        {
            ::close(pipe_fds[0]);
            if(::dup2(pipe_fds[1], STDERR_FILENO) < 0) { ::_exit(97); }
            ::close(pipe_fds[1]);

            // u8log_output owns a duplicate of the original stderr descriptor.
            // Rebind it after dup2 so this child captures the UWVM-only branch.
            ::uwvm2::uwvm::io::u8log_output.reopen(::fast_io::io_dup, ::fast_io::u8err());
            ::uwvm2::uwvm::utils::ansies::put_color = enable_color;

            // The view ends before the sentinel and therefore is not NUL-terminated
            // at data() + size(). A C-string regression would print the sentinel.
            char8_t const pointer_name_storage[]{u8'n', u8'a', u8'm', u8'e', u8'!', u8'\0'};
            ::uwvm2::imported::wasi::wasip1::func::check_wasip1_guest_pointer_alignment<4uz>(
                1u, ::fast_io::u8string_view{pointer_name_storage, 4uz});
            ::_exit(98);
        }

        ::close(pipe_fds[1]);
        char buffer[256]{};
        for(;;)
        {
            auto const read_size{::read(pipe_fds[0], buffer, sizeof(buffer))};
            if(read_size > 0)
            {
                output.append(buffer, static_cast<::std::size_t>(read_size));
                continue;
            }
            if(read_size == 0) { break; }
            if(errno == EINTR) { continue; }
            ::close(pipe_fds[0]);
            return false;
        }
        ::close(pipe_fds[0]);

        int child_status{};
        while(::waitpid(child_pid, ::std::addressof(child_status), 0) < 0)
        {
            if(errno == EINTR) { continue; }
            return false;
        }
        return WIFSIGNALED(child_status);
    }

    [[nodiscard]] int check_captured_diagnostic(bool enable_color)
    {
        ::std::string output{};
        if(!capture_alignment_diagnostic(enable_color, output)) { return fail("failed to capture terminating diagnostic"); }

        if(output.find("WASI Preview 1 guest pointer alignment trap: ") == ::std::string::npos) { return fail("missing diagnostic text"); }
        auto const expected_name_field{enable_color ? "\x1b[93mname\x1b[97m = " : "name = "};
        if(output.find(expected_name_field) == ::std::string::npos) { return fail("missing exact bounded pointer name field"); }
        if(output.find('!') != ::std::string::npos) { return fail("pointer name escaped its string_view boundary"); }
        if(output.find("required alignment = ") == ::std::string::npos || output.find(" bytes\n\n") == ::std::string::npos)
        {
            return fail("incomplete alignment diagnostic");
        }

        bool const contains_ansi{output.find("\x1b[") != ::std::string::npos};
        if(contains_ansi != enable_color) { return fail(enable_color ? "colored diagnostic contains no ANSI sequence" : "NO_COLOR diagnostic contains ANSI"); }

        if(enable_color &&
           (output.find("\x1b[0m\x1b[97muwvm: ") == ::std::string::npos ||
            output.find("\x1b[91m[fatal] ") == ::std::string::npos ||
            output.find("\x1b[93mname") == ::std::string::npos ||
            output.find("\x1b[36m4") == ::std::string::npos))
        {
            return fail("diagnostic color roles are incomplete");
        }

        return 0;
    }
#endif
}

int main()
{
#if defined(UWVM) && (defined(__unix__) || defined(__APPLE__))
    if(check_captured_diagnostic(true) != 0) { return 1; }
    if(check_captured_diagnostic(false) != 0) { return 1; }
#endif
}

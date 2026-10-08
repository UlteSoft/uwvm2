// Small Windows cross-compiler check for the real inherited-HANDLE adapter.
// It uses only test stubs for the controller and never runs a guest.
#if defined(_WIN32) && !defined(__CYGWIN__)
# define UWVM_MODULE
# define UWVM_UTILS_HAS_FAST_IO_NATIVE_THREAD
# include <array>
# include <chrono>
# include <cstddef>
# include <cstdint>
# include <cstdlib>
# include <cstring>
# include <memory>
# include <new>
# include <string>
# include <type_traits>
# include <utility>
# include <fast_io.h>
# include <windows.h>
# include <uwvm2/utils/control/win32_abi.h>
# include <tlhelp32.h>
namespace uwvm2::utils::control
{
    enum class sealed_input_status { ok };
    enum class sealed_input_decision { allow };
    inline sealed_input_status seal_debug_control_handle_host_api(void*) noexcept { return sealed_input_status::ok; }
    inline sealed_input_decision inspect_guest_file_host_api(int) noexcept { return sealed_input_decision::allow; }
    inline sealed_input_decision inspect_guest_output_host_api(int) noexcept { return sealed_input_decision::allow; }
}
namespace uwvm2::uwvm::debugger
{
    inline constexpr ::std::size_t max_command_bytes{512u};
    inline ::std::string console_help{"help\n"};
    enum class execution_status { running, exited, closed };
    struct inspection { execution_status execution{}; };
    enum class console_command_kind
    { empty, help, quit, unsupported, invalid, source_step, source_breakpoint, source_locals, source_type, source_value,
      assembly_next, assembly_step, assembly_disassemble, assembly_disassemble_range, assembly_registers,
      wasm_event, wasm_script, replacement_file, protocol };
    struct command { console_command_kind kind{}; };
    inline command parse_console_command(::std::string const&) { return {}; }
    inline ::std::string unsupported_step_message(command const&) { return {}; }
    struct controller
    {
        inspection inspect() const noexcept { return {}; }
        bool detach_resume() noexcept { return true; }
        int execute(command const&) { return 0; }
    };
    namespace details
    {
        inline ::std::string format_reply(int, command const&) { return {}; }
    }
}
# include "../../src/uwvm2/uwvm/debugger/windows_control_handle.h"
int main()
{
    auto channel{::uwvm2::uwvm::debugger::windows_control_handle::adopt(0u)};
    return channel ? 1 : 0;
}
#else
int main() { return 0; }
#endif

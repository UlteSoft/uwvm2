// Grammar only: this component never authenticates or executes a native step.
#include <uwvm2/uwvm/debugger/command.h>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <fast_io.h>

namespace dbg = ::uwvm2::uwvm::debugger;
#define CHECK(value) do { if(!(value)) { ::fast_io::io::perrln("debug_native_finish_command: FAIL line ", ::fast_io::mnp::dec(__LINE__), " ", #value); ::fast_io::fast_terminate(); } } while(false)
int main()
{
    for(auto spelling : {"finish asm", "fin asm"})
    {
        auto const command{dbg::parse_console_command(::fast_io::mnp::os_c_str(spelling))};
        CHECK(command.kind == dbg::console_command_kind::assembly_finish);
        CHECK(command.operation == ::uwvm2::utils::control::operation::step);
        CHECK(command.payload_size == 0u && command.requested_step_thread == 0u);
        CHECK(!command.wait_for_event && command.disassembly_stop_identifier == 0u);
    }
    for(auto spelling : {"finish asm 42", "fin asm 42", "finish asm 18446744073709551615"})
    {
        auto const command{dbg::parse_console_command(::fast_io::mnp::os_c_str(spelling))};
        CHECK(command.kind == dbg::console_command_kind::assembly_finish);
        CHECK(command.payload_size == 8u && command.requested_step_thread != 0u);
        CHECK(command.operation == ::uwvm2::utils::control::operation::step && !command.wait_for_event);
        // [owned payload ... exact size=8<=fixed capacity] payload_end
        // [safe                                         ] check extent before
        //  ^^ forming the complete little-endian field's one-past endpoint.
        auto const* const begin{command.payload.data()};
        auto const* const end{begin + command.payload_size};
        ::std::uint64_t decoded{};
        auto const parsed{::fast_io::parse_by_scan(begin, end, ::fast_io::mnp::le_get<64>(decoded))};
        CHECK(parsed.code == ::fast_io::parse_code::ok && parsed.iter == end);
        CHECK(decoded == command.requested_step_thread);
    }
    for(auto invalid : {"finish asm 0", "fin asm -1", "finish asm 18446744073709551616", "finish asm x", "finish asm 1 2",
                        "fin asm 42 over", "finish asm 0x10", "finish asm 42;continue", "finish asm frame 1", "finish asm 42 wait"})
    { CHECK(dbg::parse_console_command(::fast_io::mnp::os_c_str(invalid)).kind == dbg::console_command_kind::invalid); }
    for(auto execution : {"wasm-script trace wasm on; finish asm", "wasm-script status; fin asm 42"})
    {
        auto const script{dbg::parse_console_command(::fast_io::mnp::os_c_str(execution))};
        CHECK(script.kind == dbg::console_command_kind::wasm_script);
        auto const batch{dbg::parse_wasm_script(script)};
        CHECK(!batch.valid && batch.size == 0u);
    }
    CHECK(dbg::parse_console_command("step asm 42").kind == dbg::console_command_kind::assembly_step);
    CHECK(dbg::parse_console_command("step wasm 42").kind == dbg::console_command_kind::protocol);
    ::fast_io::io::println("debug_native_finish_command: PASS bounded native-finish grammar; no native execution capability proved");
}

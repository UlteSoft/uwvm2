#include <uwvm2/uwvm/debugger/command.h>
#include <fast_io.h>
namespace dbg=::uwvm2::uwvm::debugger;
namespace ws=dbg::wasip1_state;
static unsigned checks{};
static void require(bool valid) { ++checks;if(!valid) { ::fast_io::fast_terminate(); } }
int main()
{
    auto file{dbg::parse_console_command("set wasip1 file 0 410042 if-stop 9")};
    require(file.kind==dbg::console_command_kind::wasip1_state && file.disassembly_stop_identifier==9u);
    require(file.wasip1_state_request.operation==ws::action::create_file && file.wasip1_state_request.value.size()==3u && file.wasip1_state_request.value[1u]==u8'\0');
    for(auto line: ::fast_io::array<::fast_io::string_view,6u>{"set wasip1 fd-dup 0 4 0x60006e 0", "unset wasip1 fd 0 4 6291566 0", "set wasip1 checkpoint 0 7", "set wasip1 restore 0 7 bindings", "set wasip1 restore 0 7 strict", "unset wasip1 checkpoint 0 7"})
    { auto parsed{dbg::parse_console_command(line)};require(parsed.kind==dbg::console_command_kind::wasip1_state && ws::is_mutation(parsed.wasip1_state_request.operation)); }
    for(auto line: ::fast_io::array<::fast_io::string_view,7u>{"set wasip1 restore 0 7", "set wasip1 restore 0 7 all", "set wasip1 checkpoint 0 8", "set wasip1 checkpoint 0 -1", "set wasip1 file 0 0", "unset wasip1 fd 0 2147483648 0 0", "set wasip1 fd-dup 0 4 -1 0"})
    { auto parsed{dbg::parse_console_command(line)};require(parsed.kind==dbg::console_command_kind::invalid); }
    ws::request request{};require(!ws::parse("set wasip1 arg 0 0 00",request));
    require(ws::parse("set wasip1 file 0 -",request) && request.value.empty());
    ::fast_io::io::println("wasip1_checkpoint_commands PASS checks=",checks);
}

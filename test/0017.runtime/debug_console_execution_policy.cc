// HOST console scheduling grammar only; no VM stopped-state simulation.
#include <uwvm2/uwvm/debugger/console_execution.h>
#include <fast_io.h>
#include <initializer_list>
namespace policy = ::uwvm2::uwvm::debugger::console_execution;
int main()
{
    for(auto text : {::fast_io::string_view{"c"}, ::fast_io::string_view{"continue"}, ::fast_io::string_view{"  c \t\r"}})
    { auto const value{policy::classify_resume(text)}; if(!value.recognized || value.background || value.from_entry) { return 1; } }
    for(auto text : {::fast_io::string_view{"c&"}, ::fast_io::string_view{"continue&"}, ::fast_io::string_view{"  continue \t& \r"}})
    { auto const value{policy::classify_resume(text)}; if(!value.recognized || !value.background || value.from_entry) { return 2; } }
    for(auto text : {::fast_io::string_view{""}, ::fast_io::string_view{"&"}, ::fast_io::string_view{"c&&"},
        ::fast_io::string_view{"continue & extra"}, ::fast_io::string_view{"continue 1"}, ::fast_io::string_view{"step&"},
        ::fast_io::string_view{"continue;quit"}, ::fast_io::string_view{"continue\n"},
        ::fast_io::string_view{"starti\n"}, ::fast_io::string_view{"start\v"}, ::fast_io::string_view{"run\f"}})
    { if(policy::classify_resume(text).recognized) { return 3; } }
    for(auto text : {::fast_io::string_view{"run"}, ::fast_io::string_view{"r"}, ::fast_io::string_view{"  ru \t\r"}})
    { auto const value{policy::classify_resume(text)}; if(!value.recognized || value.background || !value.from_entry) { return 4; } }
    for(auto text : {::fast_io::string_view{"run&"}, ::fast_io::string_view{"r&"}, ::fast_io::string_view{"  run \t& \r"}})
    { auto const value{policy::classify_resume(text)}; if(!value.recognized || !value.background || !value.from_entry) { return 5; } }
    for(auto text : {::fast_io::string_view{"run 1"}, ::fast_io::string_view{"r app.wasm"}, ::fast_io::string_view{"run;quit"},
        ::fast_io::string_view{"r&&"}, ::fast_io::string_view{"run & extra"}, ::fast_io::string_view{"run\n"}})
    { if(policy::classify_resume(text).recognized) { return 6; } }
    for(auto text : {::fast_io::string_view{"run"}, ::fast_io::string_view{"r"}})
    {
        auto const command{::uwvm2::uwvm::debugger::parse_console_command(text)};
        if(command.kind != ::uwvm2::uwvm::debugger::console_command_kind::protocol ||
           command.operation != ::uwvm2::utils::control::operation::resume || !command.run_from_entry || command.payload_size != 0u ||
           ::uwvm2::uwvm::debugger::console_aliases::repeatable(command)) { return 7; }
    }
    for(auto text : {::fast_io::string_view{"start"}, ::fast_io::string_view{"startw"}, ::fast_io::string_view{"starti"}})
    {
        auto const p{policy::classify_resume(text)};
        auto const c{::uwvm2::uwvm::debugger::parse_console_command(text)};
        if(!p.recognized || !p.from_entry || p.background || !c.run_from_entry ||
           p.launch_action != c.launch_action || policy::launch_word(p.launch_action) != text ||
           ::uwvm2::uwvm::debugger::console_aliases::repeatable(c)) { return 8; }
    }
    for(auto text : {::fast_io::string_view{"start&"}, ::fast_io::string_view{"starti&"}, ::fast_io::string_view{"startw&"}, ::fast_io::string_view{"stw&"}, ::fast_io::string_view{"sti 1"},
                    ::fast_io::string_view{"start 1"}, ::fast_io::string_view{"kill&"},
                    ::fast_io::string_view{"kill 1"}, ::fast_io::string_view{"restart -1"},
                    ::fast_io::string_view{"restart"}, ::fast_io::string_view{"restart 0 extra"}})
    {
        if(policy::classify_resume(text).recognized ||
           ::uwvm2::uwvm::debugger::parse_console_command(text).kind !=
               ::uwvm2::uwvm::debugger::console_command_kind::invalid) { return 9; }
    }
    auto const killed{::uwvm2::uwvm::debugger::parse_console_command("kill")};
    if(!killed.run_from_entry || killed.launch_action != ::uwvm2::uwvm::debugger::console_launch_action::kill ||
       killed.payload_size != 0u || ::uwvm2::uwvm::debugger::console_aliases::repeatable(killed) ||
       ::uwvm2::uwvm::debugger::parse_console_command("restart 42").kind !=
           ::uwvm2::uwvm::debugger::console_command_kind::invalid) { return 10; }
    for(auto alias : {::fast_io::string_view{"st"}, ::fast_io::string_view{"stw"}, ::fast_io::string_view{"sti"}})
    {
        auto const p{policy::classify_resume(alias)};
        auto const c{::uwvm2::uwvm::debugger::parse_console_command(alias)};
        if(!p.recognized || !p.from_entry || p.background || p.launch_action != c.launch_action ||
           ::uwvm2::uwvm::debugger::console_aliases::parse(alias, 0u, 0u).ambiguous) { return 11; }
    }
    ::fast_io::io::println("debug_console_execution_policy: PASS");
}

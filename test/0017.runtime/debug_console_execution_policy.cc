// HOST console scheduling grammar only; no VM stopped-state simulation.
#include <uwvm2/uwvm/debugger/console_execution.h>
#include <fast_io.h>
#include <initializer_list>
namespace policy = ::uwvm2::uwvm::debugger::console_execution;
int main()
{
    for(auto text : {::fast_io::string_view{"c"}, ::fast_io::string_view{"continue"}, ::fast_io::string_view{"  c \t\r"}})
    { auto const value{policy::classify_resume(text)}; if(!value.recognized || value.background) { return 1; } }
    for(auto text : {::fast_io::string_view{"c&"}, ::fast_io::string_view{"continue&"}, ::fast_io::string_view{"  continue \t& \r"}})
    { auto const value{policy::classify_resume(text)}; if(!value.recognized || !value.background) { return 2; } }
    for(auto text : {::fast_io::string_view{""}, ::fast_io::string_view{"&"}, ::fast_io::string_view{"c&&"},
        ::fast_io::string_view{"continue & extra"}, ::fast_io::string_view{"continue 1"}, ::fast_io::string_view{"step&"},
        ::fast_io::string_view{"continue;quit"}, ::fast_io::string_view{"continue\n"}})
    { if(policy::classify_resume(text).recognized) { return 3; } }
    ::fast_io::io::println("debug_console_execution_policy: PASS");
}

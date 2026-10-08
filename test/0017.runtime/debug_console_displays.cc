#include <uwvm2/uwvm/debugger/console_displays.h>
#include <uwvm2/uwvm/debugger/console_completion.h>
#include <uwvm2/uwvm/debugger/command.h>
#include <cstdlib>
namespace displays = uwvm2::uwvm::debugger::console_displays;
using namespace uwvm2::uwvm::debugger;
static void check(bool value, char const* message)
{ if(!value) { ::fast_io::io::perrln(::fast_io::mnp::os_c_str(message)); ::std::abort(); } }
int main()
{
    displays::session owned{}; ::std::uint64_t id{};
    check(!owned.new_stop(0u) && owned.new_stop(41u) && !owned.new_stop(41u) && owned.new_stop(42u),"new nonzero stop observation only");
    for(auto text : {"enable 1","disable wasm-event 1","delete 1","displayed x","info registers"})
    { check(displays::parse(::fast_io::string_view{::fast_io::mnp::os_c_str(text)}).operation == displays::action::none,"existing commands retain their grammar"); }
    check(displays::parse("display\tparameter + 1").expression == "parameter + 1","tab separator preserves expression");
    check(displays::parse("enable\tdisplay\t2").identifier == 2u,"tab-separated policy");
    for(auto text : {"undisplay 0","disable display -1","enable display 1 2","delete display 18446744073709551616"})
    { check(displays::parse(::fast_io::string_view{::fast_io::mnp::os_c_str(text)}).operation == displays::action::invalid,"malformed display ID rejects entire input"); }
    for(auto text : {"","x\ncontinue","x\x1b[2J"})
    { check(owned.add(::fast_io::string_view{::fast_io::mnp::os_c_str(text)},id) == displays::error::invalid_expression && id == 0u,"display has no injected command/control suffix"); }
    ::fast_io::array<char,257u> too_long{}; for(auto& c : too_long) { c = 'x'; }
    check(owned.add(displays::view{too_long.data(),too_long.size()},id) == displays::error::invalid_expression,"per-expression cap");
    for(::std::size_t n{}; n != displays::maximum_displays; ++n)
    { check(owned.add("parameter + 1",id) == displays::error::none && id == n+1u,"owned bounded display entries"); }
    check(owned.add("parameter",id) == displays::error::exhausted && id == 0u,"display count cap");
    check(owned.change(displays::action::disable,0u) == displays::error::none,"disable all");
    for(auto const& row : owned.records()) { check(!row.enabled,"disabled records retained"); }
    check(owned.change(displays::action::enable,2u) == displays::error::none && owned.records()[1u].enabled,"enable exact retained record");
    check(owned.change(displays::action::remove,1u) == displays::error::none && owned.change(displays::action::enable,1u) == displays::error::unknown_identifier,"deleted ID never resurrects");
    check(owned.add("counter",id) == displays::error::none && id == 33u,"new entry never reuses a deleted ID");
    auto const completed{console_completion::context("display par",11u)};
    check(completed.type == console_completion::kind::symbol && completed.prefix == "par" && completed.selectors.empty(),"display dynamic symbol completion");
    auto const literal{console_completion::context("display 1 + par",15u)};
    check(literal.prefix == "par" && literal.selectors.empty(),"display numbers never become thread/stop selectors");
    for(auto text : {"condition 12 par","condition 12 4 + par","break 0 1 2 if par","break-source 0 file.cpp:9 ignore 2 if par"})
    {
        ::fast_io::string_view line{::fast_io::mnp::os_c_str(text)};
        auto const part{console_completion::context(line,line.size())};
        check(part.type==console_completion::kind::symbol && part.prefix=="par" && part.selectors.empty(),
              "condition breakpoint IDs and predicate numerals never become frame/thread selectors");
    }
    for(auto text : {"condition 0 par","condition -1 par","condition 18446744073709551616 par"})
    {
        ::fast_io::string_view line{::fast_io::mnp::os_c_str(text)};
        check(console_completion::context(line,line.size()).type==console_completion::kind::none,
              "invalid condition ID never selects a symbol scope");
    }
    for(auto text : {"break 0 1 2 ignore 3","break-source 0 source file.cpp:9 ignore 0","break-source 0 a ignore word.cpp:9 ignore 18446744073709551615"})
    { auto const parsed{parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)})};
      check(parsed.operation == ::uwvm2::utils::control::operation::breakpoint_set && parsed.breakpoint_initial_ignore,"initial ignore is installed as one bounded command"); }
    auto const path{parse_console_command("break-source 0 a ignore word.cpp:9")};
    check(path.kind == console_command_kind::source_breakpoint && !path.breakpoint_initial_ignore,"literal source path is not a policy suffix");
    for(auto text : {"break 0 1 2 ignore -1","break 0 1 2 ignore 18446744073709551616","break 0 1 2 ignore 1 ignore 2","status ignore 3"})
    { check(parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)}).kind == console_command_kind::invalid,"invalid initial policy never creates a breakpoint"); }
    ::fast_io::io::println("debug_console_displays: PASS (owned HOST syntax DATA; no VM authority)");
}

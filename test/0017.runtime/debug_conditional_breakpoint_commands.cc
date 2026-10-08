#include <uwvm2/uwvm/debugger/command.h>
#include <uwvm2/uwvm/debugger/source_scalar_expression.h>
#include <fast_io.h>
using namespace uwvm2::uwvm::debugger;
static void check(bool ok)
{ if(!ok) { ::fast_io::io::perrln("conditional command DATA failed"); ::fast_io::fast_terminate(); } }
int main()
{
    auto const both{parse_console_command("break-source 0 source file.cpp:9 ignore 3 if counter == 4")};
    check(both.kind == console_command_kind::source_breakpoint && both.breakpoint_initial_ignore &&
        both.breakpoint_ignore == 3u && both.breakpoint_initial_condition);
    check(::fast_io::string_view{both.breakpoint_condition.data(),both.breakpoint_condition_size} == "counter == 4");
    auto const numeric{parse_console_command("break 0 1 2 if parameter > 0")};
    check(numeric.kind == console_command_kind::protocol && numeric.breakpoint_initial_condition);
    auto const update{parse_console_command("condition 17 parameter + 1")};
    check(update.kind == console_command_kind::breakpoint_control && update.breakpoint_policy == breakpoint_action::condition &&
        update.breakpoint_id == 17u && update.breakpoint_condition_size != 0u);
    auto const clear{parse_console_command("condition 17")};
    check(clear.kind == console_command_kind::breakpoint_control && clear.breakpoint_condition_size == 0u);
    for(auto line : {"condition 0 x","condition -1 x","condition 18446744073709551616 x","break 0 1 2 if ","status if true",
                    "break 0 1 2 ignore 1 ignore 2 if true"})
    { check(parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(line)}).kind == console_command_kind::invalid); }
    source_scalar_expression::program expression{};
    check(source_scalar_expression::parse("(counter == 4 && parameter == 7) != false",expression) == source_scalar_expression::error::none);
    check(source_scalar_expression::parse("(counter = 4) != false",expression) != source_scalar_expression::error::none);
    ::fast_io::io::println("PASS bounded conditional breakpoint command DATA; actual stops have a separate test");
}

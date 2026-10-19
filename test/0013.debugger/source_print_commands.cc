// HOST-only preference/grammar checks. No guest or native stop is fabricated.
#include <uwvm2/uwvm/debugger/command.h>
#include <uwvm2/uwvm/debugger/console_aliases.h>
#include <uwvm2/uwvm/debugger/console_completion.h>
#include <uwvm2/uwvm/debugger/console_help.h>
namespace dbg = ::uwvm2::uwvm::debugger;
using view = ::fast_io::string_view;
static void require(bool okay, view why)
{ if(!okay) { ::fast_io::io::perrln("source print command: ", why); ::fast_io::fast_terminate(); } }
static bool has(dbg::console_editing::command_choices const& choices, view word)
{ for(::std::size_t i{}; i != choices.size; ++i) { if(choices.words[i].text == word) { return true; } } return false; }
static view expression(dbg::console_command const& command)
{ return view{command.source_variable_name.data(), command.source_variable_name_size}; }
int main()
{
    dbg::console_print_preferences prefs{};
    auto command{dbg::parse_console_command("set print pretty off")};
    require(command.kind == dbg::console_command_kind::source_print_options && command.source_raw && command.payload_size == 0u,
        "printing preferences carry no runtime payload");
    prefs.update(command);
    command = dbg::parse_console_command("print packet.items"); prefs.apply(command);
    require(command.source_raw && command.source_print_elements == 200u && expression(command) == "packet.items", "session default applies");
    command = dbg::parse_console_command("print /p 1 7 0 packet.items"); prefs.apply(command);
    require(!command.source_raw && command.source_print_explicit_override && command.requested_step_thread == 1u &&
        command.disassembly_stop_identifier == 7u && command.source_frame_explicit && command.source_frame_ordinal == 0u &&
        expression(command) == "packet.items", "one-query pretty override preserves selectors");
    require(!prefs.pretty, "one-query override leaves the default unchanged");
    prefs.update(dbg::parse_console_command("set print pretty on"));
    command = dbg::parse_console_command("print-frame --raw 1 7 2 packet.items"); prefs.apply(command);
    require(command.source_raw && command.source_frame_explicit && command.source_frame_ordinal == 2u &&
        expression(command) == "packet.items", "raw explicit-frame query bypasses printers");
    command = dbg::parse_console_command("print /r 1 || missing");
    require(command.kind == dbg::console_command_kind::source_value && expression(command) == "1 || missing" &&
        command.requested_step_thread == 0u, "format option preserves numeric expression grammar");
    command = dbg::parse_console_command("print -- -1 + 2"); prefs.apply(command);
    require(!command.source_raw && !command.source_print_explicit_override && expression(command) == "-1 + 2", "option terminator preserves expression bytes");
    prefs.update(dbg::parse_console_command("set\tprint elements\t0 "));
    command = dbg::parse_console_command("print packet"); prefs.apply(command);
    require(command.source_print_elements == 0u && !command.source_raw, "zero requests a summary without synthetic elements");
    prefs.update(dbg::parse_console_command("set print elements 4096"));
    require(prefs.elements == dbg::maximum_source_print_elements, "maximum bounded element budget admitted");
    for(auto line : {view{"set print elements -1"}, view{"set print elements 4097"}, view{"set print elements 18446744073709551616"},
        view{"set print elements 2 trailing"}, view{"set print pretty yes"}, view{"set print pretty on off"},
        view{"show print pretty on"}, view{"show print elements 2"}, view{"info pretty-printer extra"}, view{"print /r"},
        view{"print-frame /p 1 7 packet"}})
    { require(dbg::parse_console_command(line).kind == dbg::console_command_kind::invalid, "malformed or over-budget command refused"); }
    prefs.update(dbg::parse_console_command("show print pretty"));
    prefs.update(dbg::parse_console_command("show print elements"));
    require(prefs.pretty && prefs.elements == 4096u, "show commands never mutate preferences");
    command = dbg::console_aliases::parse("sh print pretty", 0u, 0u).command;
    require(command.kind == dbg::console_command_kind::source_print_options && command.source_print_policy == dbg::source_print_action::show_pretty,
        "show command participates in unique-prefix resolution without a guest");
    command = dbg::console_aliases::parse("i pretty-p", 0u, 0u).command;
    require(command.kind == dbg::console_command_kind::source_print_options && command.source_print_policy == dbg::source_print_action::list_printers,
        "pretty-printer registry participates in info aliases");
    command = dbg::console_aliases::parse("p /r packet", 0u, 0u).command;
    require(command.kind == dbg::console_command_kind::source_value && command.source_raw && expression(command) == "packet", "p keeps raw format option");
    view input{"print /r 1 7 0 packet.it"};
    auto query{dbg::console_completion::context(input, input.size())};
    require(query.type == dbg::console_completion::kind::symbol && query.prefix == "it" && query.root == "packet" &&
        query.selectors == "1 7 0 " && input.subview(query.begin, query.end - query.begin) == "it", "raw expression completion keeps original selectors and replacement span");
    input = "print /p packet";
    query = dbg::console_completion::context(input, input.size());
    require(query.type == dbg::console_completion::kind::symbol && query.prefix == "packet" && query.root.empty(), "pretty option allows first-name completion");
    input = "set print pretty o";
    auto choices{dbg::console_editing::command_matches(input, input.size())};
    require(has(choices, "on") && has(choices, "off"), "on/off complete from static host vocabulary");
    input = "show print "; choices = dbg::console_editing::command_matches(input, input.size());
    require(has(choices, "pretty") && has(choices, "elements"), "show preference completion");
    require(dbg::console_help::find(dbg::console_help::canonical("set print pretty")) != nullptr &&
        dbg::console_help::find(dbg::console_help::canonical("show print elements")) != nullptr &&
        dbg::console_help::find(dbg::console_help::canonical("info pretty-printer")) != nullptr, "each command resolves to help");
    require(!dbg::console_aliases::repeatable(dbg::parse_console_command("set print pretty off")), "changing a preference never repeats implicitly");
    ::fast_io::io::println("source print command checks passed");
}

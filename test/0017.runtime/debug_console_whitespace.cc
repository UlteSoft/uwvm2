// HOST command grammar only. Run in the admitted SSH Linux cgroup.
#include <uwvm2/uwvm/debugger/console_tui.h>
#include <uwvm2/uwvm/debugger/console_completion.h>
#include <uwvm2/uwvm/debugger/console_displays.h>
#include <uwvm2/uwvm/debugger/console_help.h>
#include <uwvm2/uwvm/debugger/console_messages.h>
#include <fast_io.h>
namespace dbg = uwvm2::uwvm::debugger;
using view = fast_io::string_view;
static unsigned failures{};
static void check(bool good, view label)
{ if(!good) { fast_io::io::perrln("FAIL whitespace: ", label); ++failures; } }
static fast_io::string spaced(view input, view separator)
{
    fast_io::string result{}; result.append(separator); result.append(separator);
    for(char c : input) { if(c == ' ') { result.append(separator); result.append(separator); result.append(separator); } else { result.push_back(c); } }
    result.append(separator); result.append(separator); return result;
}
int main()
{
    constexpr view commands[]{
        "help asm registers full 1", "info modules 1", "info threads", "status", "wait", "quit", "server",
        "step wasm 1 into", "step source 1 over", "step asm 1", "ni 1", "finish asm 1", "frames wasm 1 1 0 1",
        "frame 0", "up 1", "down 1", "bt 1", "locals source 1", "locals wasm 1 0 0 1", "globals 1 0 0 1",
        "operands 1 0 0 1", "saved 1 0 0 1", "table 1 0 0 0 1", "info registers 1 1", "info all-registers",
        "disassemble 1 1 8", "disassemble-range 1 1 8 0 1 1", "memory 0 0 0 4", "replace 0 1 1 /tmp/body.bin",
        "breakwasm 0 1 0", "breakwasm 0 1 0 ignore 2", "breaksource 0 source.cc:20", "bs 0 source.cc:20",
        "until 1 1 source.cc:20", "advance 1 1 20", "breakasm 128", "bi 128", "startw", "stw", "starti", "sti", "start", "st", "condition 1", "delete 1", "enable 1", "disable 1", "ignore 1 2",
        "trace wasm on memory", "trace wasm read 1 4", "catch wasm memory 0 all", "info wasm-events", "delete wasm-event 1",
        "path clear", "path create locals 1 0 0 0 0", "path extend 1 1 0", "path members 1 1 0 1",
        "members locals 1 0 0 0 0 0 1", "set wasm memory 0 0 1 0 bytes 0102", "set wasm global 0 0 1 bits i32 2a",
        "info wasip1 args 0", "info wasip1 env 0", "info wasip1 fds 0 0 4", "info wasip1 preopens 0",
        "set wasip1 arg 0 0 6869", "set wasip1 env 0 41 42", "unset wasip1 env 0 41", "set wasip1 fd-dup 0 1 0 0",
        "set wasip1 checkpoint 0 1", "set wasip1 restore 0 1 bindings", "set wasip1 restore 0 1 bindings if-stop 1", "trace wasip1 on fd_write"};
    unsigned parsed{};
    for(auto separator : {view{" "}, view{"\t"}, view{"\n"}, view{"\r"}, view{"\v"}, view{"\f"}})
    {
        for(auto command : commands)
        {
            auto const original{dbg::parse_console_command(command)};
            check(original.kind != dbg::console_command_kind::invalid, command);
            auto const input{spaced(command, separator)};
            auto const actual{dbg::parse_console_command(input.subview(0u))};
            check(actual.kind == original.kind && actual.launch_action == original.launch_action && actual.operation == original.operation && actual.payload_size == original.payload_size &&
                  actual.payload == original.payload && actual.source_module == original.source_module && actual.source_line == original.source_line &&
                  actual.native_breakpoint_offset == original.native_breakpoint_offset && actual.breakpoint_id == original.breakpoint_id && actual.breakpoint_ignore == original.breakpoint_ignore &&
                  actual.disassembly_stop_identifier == original.disassembly_stop_identifier && actual.help_topic == original.help_topic &&
                  actual.help_topic_size == original.help_topic_size && actual.replacement_path == original.replacement_path, command);
            ++parsed;
        }
        auto const alias{spaced("i reg 1 1", separator)};
        check(dbg::console_aliases::parse(alias.subview(0u), 1u, 1u).command.kind == dbg::console_command_kind::assembly_registers,
              "aliases and numeric arguments accept all repeated c_space separators");
        auto const search{spaced("native register",separator)};
        check(dbg::console_help::search(search.subview(0u)).find("help asm registers") != fast_io::containers::npos, "apropos command words use C whitespace");
        auto const invalid{spaced("set wasm memory missing",separator)};
        check(dbg::console_messages::syntax_error(invalid.subview(0u)).find("set wasm memory MODULE") != fast_io::containers::npos,
              "usage errors select the correct page across repeated separators");
        fast_io::string output{};
        dbg::console_tui::screen screen{&output, +[](void* context, view text) noexcept { static_cast<fast_io::string*>(context)->append(text); }, true};
        for(std::size_t i{}; i < dbg::console_tui::layout_names.size(); ++i)
        {
            if(i==static_cast<std::size_t>(dbg::console_tui::layout::split)) { continue; } // removed legacy layout
            auto const input{spaced(fast_io::concat_fast_io("layout ", dbg::console_tui::layout_names[i]).subview(0u), separator)};
            check(screen.command(input.subview(0u)) && screen.enabled() && static_cast<std::size_t>(screen.current_layout()) == i,
                  "every layout accepts leading, repeated and trailing C whitespace");
        }
        auto const next{spaced("layout next", separator)};
        check(screen.command(next.subview(0u)) && screen.current_layout() == dbg::console_tui::layout::src, "layout next cycles once");
        for(auto focus : {view{"next"}, view{"prev"}, view{"cmd"}, view{"code"}, view{"src"}, view{"asm"}, view{"regs"}, view{"wasm"}, view{"wasip1"}})
        { auto const input{spaced(fast_io::concat_fast_io("focus ",focus).subview(0u),separator)}; check(screen.command(input.subview(0u)), "focus word dispatch"); }
        auto const disable{spaced("tui disable", separator)}, enable{spaced("tui enable", separator)};
        check(screen.command(disable.subview(0u)) && !screen.enabled(), "spaced TUI disable");
        check(screen.command(enable.subview(0u)) && screen.enabled(), "spaced TUI enable");
        check(output.find("error:") == fast_io::containers::npos, "valid UI commands have no usage error");
        auto const display{spaced("disable display 1", separator)};
        auto const request{dbg::console_displays::parse(display.subview(0u))};
        check(request.operation == dbg::console_displays::action::disable && request.identifier == 1u, "display command arguments use C whitespace");
    }
    auto const source{dbg::parse_console_command("  bs\t 0\t /tmp/a  b.cc:20   if\t counter  + 1 > 2")};
    check(source.kind == dbg::console_command_kind::source_breakpoint && source.breakpoint_initial_condition &&
          view{source.source_path.data(),source.source_path_size} == "/tmp/a  b.cc" &&
          view{source.breakpoint_condition.data(),source.breakpoint_condition_size} == "counter  + 1 > 2", "source path and condition retain internal spaces");
    auto const expression{dbg::parse_console_command("  print\t counter  +  1")};
    check(expression.kind == dbg::console_command_kind::source_value &&
          view{expression.source_variable_name.data(),expression.source_variable_name_size} == "counter  +  1", "source expression bytes remain unchanged");
    auto const script{dbg::parse_console_command("  wasm-script\t\t status;  memory 0 0 0 4")};
    check(script.kind == dbg::console_command_kind::wasm_script && view{script.script_text.data(),script.script_size} == "status;  memory 0 0 0 4", "script body remains unchanged");
    fast_io::string output{};
    dbg::console_tui::screen screen{&output,+[](void* context,view text) noexcept { static_cast<fast_io::string*>(context)->append(text); },true};
    check(screen.command("  set\t source   /tmp/a  b\tc.cc") && screen.source_override() == "/tmp/a  b\tc.cc", "source path stays opaque");
    check(screen.command("layout  regs") && screen.current_layout() == dbg::console_tui::layout::registers, "reported layout regs case");
    check(screen.command("layout  asm  extra") && screen.current_layout() == dbg::console_tui::layout::registers, "invalid extra argument does not change layout");
    check(screen.command("tui  disable extra") && screen.enabled(), "invalid extra argument does not disable TUI");
    check(!screen.command("tuinonsense enable") && !screen.command("layoutregs"), "command word boundaries are exact");
    for(auto input : {view{"replace\t 0   1\t 1   /tmp/a  b.bin"}, view{"set\t source   /tmp/a  b.cc"},view{"bs\t 0   /tmp/a  b.cc"}})
    { auto const request{dbg::console_completion::context(input,input.size())};check(request.type == dbg::console_completion::kind::path && request.prefix.starts_with("/tmp/a  b"), "path completion skips only argument separators"); }
    check(dbg::parse_console_command("setwasm memory 0 0 1 0 bytes 0102").kind == dbg::console_command_kind::invalid &&
          dbg::parse_console_command("info wasip1x args 0").kind == dbg::console_command_kind::invalid, "near-prefix commands remain invalid");
    fast_io::io::println("debug_console_whitespace: ",failures == 0u ? view{"PASS"} : view{"FAIL"}, " parsed=", parsed);
    return failures != 0u;
}

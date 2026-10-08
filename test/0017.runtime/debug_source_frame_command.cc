// Production host grammar component only; actual source/frame capture is
// separately qualified by the genuine compiler/product CLI fixture recipe.
#include <uwvm2/uwvm/debugger/command.h>
#include <fast_io.h>
#include <initializer_list>
#include <string_view>
using namespace uwvm2::uwvm::debugger;
static void check(bool value, ::std::string_view text)
{ if(!value) { ::fast_io::io::perrln("debug_source_frame_command: ", text); ::fast_io::fast_terminate(); } }
int main()
{
    auto frame{parse_console_command("frame 7 99 2")};
    check(frame.kind == console_command_kind::source_frame && frame.frame_action == source_frame_action::select &&
          frame.requested_step_thread == 7u && frame.disassembly_stop_identifier == 99u && frame.source_frame_ordinal == 2u && frame.payload_size == 8u,
          "explicit frame ordinal carries participant/stop but no address");
    auto up{parse_console_command("up 7 99 1")};
    check(up.kind == console_command_kind::source_frame && up.frame_action == source_frame_action::up && up.source_frame_ordinal == 1u,
          "up uses positive checked scalar count");
    auto bare{parse_console_command("down")};
    check(bare.kind == console_command_kind::source_frame && bare.frame_action == source_frame_action::down && bare.source_frame_ordinal == 1u && bare.payload_size == 0u,
          "bare spelling defers genuine current thread/stop to controller");
    auto list{parse_console_command("frames 7 99")};
    check(list.kind == console_command_kind::source_frame && list.frame_action == source_frame_action::list && list.payload_size == 8u,
          "list is authenticated read-only backtrace operation");
    auto print{parse_console_command("print 7 99 2 packet.grid[1][2]")};
    check(print.kind == console_command_kind::source_value && print.source_frame_explicit && print.source_frame_ordinal == 2u &&
          print.requested_step_thread == 7u && print.disassembly_stop_identifier == 99u,
          "DAP frame/value is one command, no select/query race");
    auto locals{parse_console_command("locals source 7 99 2")};
    check(locals.kind == console_command_kind::source_locals && locals.source_frame_explicit && locals.source_frame_ordinal == 2u &&
          locals.requested_step_thread == 7u && locals.disassembly_stop_identifier == 99u, "atomic selected source locals");
    for(auto text : {::fast_io::string_view{"frame -1"}, ::fast_io::string_view{"frame 0 99 1"},
                    ::fast_io::string_view{"frame 7 0 1"}, ::fast_io::string_view{"up 18446744073709551616"},
                    ::fast_io::string_view{"locals source 7 0 1"}, ::fast_io::string_view{"frame 7 99 1 trailing"}})
    { check(parse_console_command(text).kind == console_command_kind::invalid, "malformed/overflow frame grammar rejected before control request"); }
    ::fast_io::io::println("PASS production frame grammar only; actual producer/runtime qualification separate");
}

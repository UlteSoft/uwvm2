// Copied reply presentation tests; actual guest state is covered by the PTY runner.
// Match console.h's dependency order, then check this wrapper's own push/pop.
#include <uwvm2/uwvm/debugger/controller.h>
#include <uwvm2/uwvm/debugger/console_help.h>
#include <uwvm2/uwvm/debugger/console_tui.h>
#define refresh uwvm_debugger_test_refresh_macro
#include <uwvm2/uwvm/debugger/console_messages.h>
#ifndef refresh
# error "console_messages must restore the caller's macro state"
#endif
#undef refresh
#include <uwvm2/uwvm/debugger/console.h>
#include <fast_io.h>
#include <memory>
namespace dbg = uwvm2::uwvm::debugger;
using view = ::fast_io::string_view;
static void check(bool ok, view label)
{ if(!ok) { ::fast_io::io::perrln("FAIL CLI messages: ", label); ::fast_io::fast_terminate(); } }
int main()
{
    auto owner{::std::make_unique<dbg::controller_reply>()}; auto& reply{*owner};
    auto up{dbg::parse_console_command("up")}; reply.status = uwvm2::utils::control::error::invalid_state;
    for(auto text : {view{"up"}, view{"print value"}, view{"si"}, view{"info registers"}, view{"memory 0 0 0 4"}, view{"locals wasm 1"}})
    {
        auto command{dbg::parse_console_command(text)};
        reply.execution = dbg::execution_status::running;
        check(dbg::details::format_cli_reply(reply, command).find("Press Ctrl+C") != ::fast_io::containers::npos, "running explains how to stop");
        reply.execution = dbg::execution_status::stopping;
        check(dbg::details::format_cli_reply(reply, command).find("Use \"wait\"") != ::fast_io::containers::npos, "pending stop does not claim a stop");
        reply.execution = dbg::execution_status::exited;
        check(dbg::details::format_cli_reply(reply, command).find("Use \"run\"") != ::fast_io::containers::npos, "exited suggests a fresh run");
        reply.execution = dbg::execution_status::closed;
        check(dbg::details::format_cli_reply(reply, command).find("Restart the debugger") != ::fast_io::containers::npos, "closed explains restart");
    }
    reply.execution = dbg::execution_status::stopped; reply.reason = dbg::stop_reason::initial;
    check(dbg::details::format_cli_reply(reply, up).find("has not started") != ::fast_io::containers::npos, "up before run has no stack");
    check(dbg::details::format_reply(reply, up).find("command or thread is not valid") != ::std::string::npos, "transport grammar unchanged");
    reply.reason = dbg::stop_reason::breakpoint; reply.stop_identifier = 7;
    dbg::stopped_thread thread{}; thread.identifier = 1; reply.threads.push_back(thread);
    check(dbg::details::format_cli_reply(reply, dbg::parse_console_command("frame 1 6 0")).find("Stop 6 is no longer current") != ::fast_io::containers::npos, "stale stop is identified");
    check(dbg::details::format_cli_reply(reply, dbg::parse_console_command("frame 99 7 0")).find("Thread 99") != ::fast_io::containers::npos, "wrong thread identified");
    check(dbg::details::format_cli_reply(reply, dbg::parse_console_command("info registers")).find("Use \"si\"") != ::fast_io::containers::npos, "register prerequisite explains next command");
    reply.reason = dbg::stop_reason::native_step;
    check(dbg::details::format_cli_reply(reply, up).find("native instruction stop") != ::fast_io::containers::npos, "native stop does not invent source frames");
    reply.reason = dbg::stop_reason::breakpoint; reply.status = uwvm2::utils::control::error::source_debug_info_unavailable;
    check(dbg::details::format_cli_reply(reply, up).find("Compile the Wasm program with -g") != ::fast_io::containers::npos, "missing metadata explains remedy");
    reply.status = uwvm2::utils::control::error::source_location_unmapped; reply.source_frame_out_of_range = true;
    check(dbg::details::format_cli_reply(reply, up).find("Already at the outermost frame") != ::fast_io::containers::npos, "up at boundary uses ordinary terms");
    check(dbg::details::format_cli_reply(reply, dbg::parse_console_command("down")).find("innermost frame (#0)") != ::fast_io::containers::npos, "down boundary");
    reply.source_frame_out_of_range = false; reply.status = uwvm2::utils::control::error::none;
    reply.source_frames_available = true; reply.source_stop_identifier = 7; reply.source_frame_thread = 1;
    reply.source_frame_total = 2; reply.selected_source_frame = 1;
    dbg::source_frame_view current{}, caller{}; current.name = "leaf"; caller.name = "outer\n\x1b[2J"; caller.function = 2;
    reply.source_frames = {current, caller};
    auto concise{dbg::details::format_cli_reply(reply, up)};
    check(concise.starts_with("#1 ") && concise.find("#0") == ::fast_io::containers::npos && concise.find("\x1b") == ::fast_io::containers::npos, "one selected sanitized frame");
    check(dbg::details::format_cli_reply(reply, dbg::parse_console_command("up 1 7 1")).starts_with("source-frames stop="), "explicit frame query retains detailed fields");
    check(dbg::console_messages::syntax_error("up nope").find("Usage: up [COUNT]") != ::fast_io::containers::npos, "invalid arguments name expected syntax");
    check(dbg::console_messages::syntax_error("made-up\x1b[2J").find("\x1b") == ::fast_io::containers::npos, "unknown command cannot control terminal");
    check(dbg::console_messages::syntax_error("pri").find("Usage: print EXPR") != ::fast_io::containers::npos, "missing argument after a unique abbreviation has the same usage");
    for(auto const& sample : {::std::pair<view,view>{"thr nonsense", "help thread"}, {"reg nonsense", "help register"},
         {"set wasip1 nonsense", "help set wasip1"}, {"info nonsense", "help info"}, {"trace nonsense", "help trace"}})
    { auto text{dbg::console_messages::syntax_error(sample.first)};
      check(text.find("Unknown or incomplete subcommand") != ::fast_io::containers::npos && text.find(sample.second) != ::fast_io::containers::npos, "invalid subcommands guide the nearest menu"); }
    check(dbg::console_messages::syntax_error("thr \x1b[2J").find("\x1b") == ::fast_io::containers::npos, "menu errors do not echo untrusted argument bytes");
    check(dbg::console_messages::syntax_error("set wasip1 arg nope").find("Usage: set wasip1 arg MODULE") != ::fast_io::containers::npos, "leaf usage takes precedence over a family menu");
    check(dbg::console_messages::syntax_error("thr list extra").find("Usage: status") != ::fast_io::containers::npos,
          "recognized abbreviated root and full child with bad arguments retain leaf usage");
    check(dbg::console_help::format("info threads") == dbg::console_help::format("execution status"),
          "suggested info threads has the existing status help page");
    for(auto const& sample : {::std::pair<view,view>{"thr lsit", "thread list"}, {"reg reed", "register read"},
         {"info threds", "info threads"}, {"i wasi agrs", "info wasip1 args"},
         {"set wasm memroy 0 0 1 0 bytes 0102", "set wasm memory"}, {"set wasip1 restroe 0 1", "set wasip1 restore"},
         {"set wasip1 fd-dupp 0 1 2", "set wasip1 fd-dup"}, {"thr step-ovre", "thread step-over"}, {"trace wasmm", "trace wasm"},
         {"info waspi1 args", "info wasip1"}, {"set waspi1 arg 0 0 6869", "set wasip1"},
         {"set wasip2 restore 0 1", "set wasip1"}, {"trace wasip2 on fd_write", "trace wasip1"}})
    {
        auto text{dbg::console_messages::syntax_error(sample.first)};
        check(text.find(::fast_io::concat_fast_io("Did you mean \"", sample.second, "\"").subview(0u)) != ::fast_io::containers::npos &&
              text.find(::fast_io::concat_fast_io("help ", sample.second).subview(0u)) != ::fast_io::containers::npos &&
              text.find("No command was executed") != ::fast_io::containers::npos,
              "nested typo names the scoped command and its exact help page without executing it");
    }
    check(dbg::console_messages::syntax_error("set wasm memory 0 0 1 0 bytes read").find("Did you mean") == ::fast_io::containers::npos,
          "argument text is never treated as a misspelled command word");
    check(dbg::console_messages::syntax_error("set wasip1 restroe 0 \x1b[2J").find("\x1b") == ::fast_io::containers::npos,
          "subcommand suggestions never echo hostile trailing arguments");
    check(dbg::console_messages::syntax_error("reg reedxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx").find("Did you mean") == ::fast_io::containers::npos,
          "unrelated and overlong child names receive no speculative correction");
    auto const ambiguous_start{dbg::console_messages::syntax_error("sta")};
    check(ambiguous_start.find("  status\n") != ::fast_io::containers::npos && ambiguous_start.find("  start\n") != ::fast_io::containers::npos &&
          ambiguous_start.find("  startw\n") != ::fast_io::containers::npos && ambiguous_start.find("  starti\n") != ::fast_io::containers::npos,
          "ambiguous input lists actual matching commands on separate lines");
    check(dbg::console_messages::syntax_error("info w").find("  info wasm-events\n  info wasip1\n") != ::fast_io::containers::npos, "subcommand ambiguity includes its command scope");
    for(auto input : {view{"wat"}, view{"wiat"}, view{"waitt"}})
    { check(dbg::console_messages::syntax_error(input).find("Did you mean \"wait\"") != ::fast_io::containers::npos, "single typo/transposition suggests the command without executing it"); }
    check(dbg::console_messages::syntax_error("prinnt counter").find("Did you mean \"print\"") != ::fast_io::containers::npos, "print typo suggestion");
    check(dbg::console_messages::syntax_error("made-up").find("Did you mean") == ::fast_io::containers::npos, "unrelated commands have no speculative correction");
    check(dbg::console_messages::syntax_error("wat\x1b[2J").find("\x1b") == ::fast_io::containers::npos, "typo diagnostics never echo hostile input");
    reply.script_commands = {up}; auto child{::std::make_unique<dbg::controller_reply>()};
    child->execution = dbg::execution_status::stopped; child->reason = dbg::stop_reason::initial;
    child->status = uwvm2::utils::control::error::invalid_state; reply.script_replies.push_back(::std::move(*child));
    auto script{dbg::details::format_cli_reply(reply, dbg::parse_console_command("wasm-script up"))};
    check(script.find("has not started") != ::fast_io::containers::npos && script.find("command or thread is not valid") == ::fast_io::containers::npos, "script child errors use the human presentation");
    reply.script_commands.clear(); reply.script_replies.clear();
    reply.wasm_mutation_value.status = dbg::wasm_state::status::unavailable_foreign_host_state;
    auto refused{dbg::details::format_cli_reply(reply, dbg::parse_console_command("set wasm memory 0 0 1 0 bytes 0102"))};
    check(refused.starts_with("error:") && refused.find("No change was made") != ::fast_io::containers::npos &&
          refused.find("help set wasm memory") != ::fast_io::containers::npos, "typed edit refusal explains outcome and exact command help");
    reply.wasm_mutation_value.status = dbg::wasm_state::status::available;
    reply.wasm_mutation_value.reason = dbg::wasm_mutation::refusal::type_mismatch;
    check(dbg::details::format_cli_reply(reply, dbg::parse_console_command("set wasm global 0 0 1 bits i32 2a")).find("different Wasm type") != ::fast_io::containers::npos, "mutation refusal reason is preserved even when capture was available");
    reply.wasip1_state_values.result = dbg::wasip1_state::status::bad_descriptor;
    check(dbg::details::format_cli_reply(reply, dbg::parse_console_command("info wasip1 fds 0")).find("help info wasip1 fds") != ::fast_io::containers::npos, "WASIp1 refusal has a specific help topic");
    reply.disassembly_count = 32u;
    auto denied_window{dbg::console_messages::native_view(reply, dbg::parse_console_command("disassemble 1 7 32"), "native-disassembly stop=7\n")};
    check(denied_window.find("No instruction in this window") != ::fast_io::containers::npos && denied_window.find("instruction 31") == ::fast_io::containers::npos, "all unavailable disassembly is one useful explanation");
    check(dbg::console_messages::native_view(reply, dbg::parse_console_command("disassemble 1 7 32"), "error: invalid native data\n").empty(), "presentation never replaces invalid raw metadata errors");
    auto values{::std::make_unique<dbg::controller_reply>()};
    values->execution = dbg::execution_status::stopped; values->source_stop_identifier = 7; values->stop_identifier = 7;
    values->source_object_type_available = true; values->source_object_value_available = true;
    dbg::source_dwarf::object_node scalar{};
    scalar.kind = dbg::source_dwarf::type_kind::scalar; scalar.type_name = "volatile int";
    scalar.scalar_kind = dbg::source_dwarf::numeric_kind::signed_integer; scalar.scalar_bytes = 4;
    scalar.byte_size = 4; scalar.value_available = true; scalar.bits = 2;
    values->source_object_type = {scalar};
    check(dbg::details::format_cli_reply(*values, dbg::parse_console_command("print counter")) == "counter = 2\n", "simple print is expression equals value");
    check(dbg::details::format_cli_reply(*values, dbg::parse_console_command("ptype counter")) == "type = volatile int\n", "simple ptype is readable");
    check(dbg::details::format_cli_reply(*values, dbg::parse_console_command("print 1 7 counter")).starts_with("source-value stop="), "explicit print preserves detailed provenance");
    check(dbg::details::format_reply(*values, dbg::parse_console_command("print counter")).starts_with("source-value stop="), "wire output is unchanged");
    values->source_object_type[0].kind = dbg::source_dwarf::type_kind::pointer;
    check(dbg::details::format_cli_reply(*values, dbg::parse_console_command("print ptr")).find("guest:0x") != ::fast_io::containers::npos, "guest pointers remain explicitly distinguished");
    values->source_object_type[0] = scalar; values->source_object_type[0].bits = 0xffffffffu;
    check(dbg::details::format_cli_reply(*values, dbg::parse_console_command("print counter")) == "counter = -1\n", "signed width is preserved");
    values->source_object_type[0].reason = dbg::source_dwarf::object_unavailable_reason::incomplete_value;
    check(dbg::details::format_cli_reply(*values, dbg::parse_console_command("print counter")).find("unavailable bits") != ::fast_io::containers::npos, "missing copied bits remain explicit");
    values->source_object_type[0] = scalar;
    dbg::source_dwarf::object_node member{scalar}; member.parent = 0; member.depth = 1; member.name = "field\n\x1b[2J";
    member.reason = dbg::source_dwarf::object_unavailable_reason::unknown_member_offset;
    values->source_object_type.push_back(member);
    auto layout{dbg::details::format_cli_reply(*values, dbg::parse_console_command("ptype counter"))};
    check(layout.find("offset unavailable") != ::fast_io::containers::npos && layout.find("\x1b") == ::fast_io::containers::npos, "unknown layouts and control bytes are not invented or emitted");
    values->source_object_type[1].parent = 1;
    check(dbg::details::format_cli_reply(*values, dbg::parse_console_command("print counter")).starts_with("error: invalid source object tree"), "invalid tree is rejected before pretty printing");
    values->source_object_type.assign(128u, scalar);
    for(::std::size_t i{1u}; i < values->source_object_type.size(); ++i)
    { auto& node{values->source_object_type[i]}; node.parent = 0; node.depth = 1; node.name.assign(4096u, 'x'); }
    auto bounded{dbg::details::format_cli_reply(*values, dbg::parse_console_command("print counter"))};
    check(bounded.size() < 65536u && bounded.find("Output truncated.") != ::fast_io::containers::npos, "pretty object output is bounded and marked");
    values->source_object_type.back().parent = 128;
    check(dbg::details::format_cli_reply(*values, dbg::parse_console_command("print counter")).starts_with("error: invalid source object tree"), "invalid omitted tail cannot hide behind output truncation");
    values->source_object_value_available = false; values->source_object_type_available = false; values->source_object_type.clear();
    values->source_locals_available = true;
    dbg::source_dwarf::numeric_variable variable{}; variable.name = "counter"; variable.type_name = "int";
    variable.kind = dbg::source_dwarf::numeric_kind::signed_integer; variable.byte_count = 4; variable.bits = 2;
    values->source_locals = {variable};
    auto local_command{dbg::console_aliases::parse("info locals", 1, 7).command};
    check(local_command.concise_source_locals && dbg::details::format_cli_reply(*values, local_command) == "counter = 2\n", "info locals hides transport labels");
    check(dbg::details::format_cli_reply(*values, dbg::parse_console_command("locals source 1")).starts_with("source-stop 7"), "explicit locals preserves detailed output");
    variable.parameter = true; variable.kind = dbg::source_dwarf::numeric_kind::boolean; variable.bits = 1; values->source_locals = {variable};
    check(dbg::details::format_cli_reply(*values, local_command) == "counter = true [parameter]\n", "boolean and parameter meanings remain visible");
    auto stack_command{dbg::console_aliases::parse("bt", 1, 7).command};
    check(stack_command.concise_backtrace && stack_command.requested_step_thread == 1 && stack_command.payload_size == 8, "bt shorthand carries the actual selection without changing the wire payload");
    ::uwvm2::runtime::exception::diagnostic_frame copied{}; copied.function_name = u8"leaf\n\x1b[2J"; copied.function_index = 2;
    dbg::stopped_thread captured{}; captured.identifier = 1;
    captured.trace = ::uwvm2::runtime::exception::diagnostic_trace::make({copied}, true); values->threads = {captured};
    auto stack{dbg::details::format_cli_reply(*values, stack_command)};
    check(stack.starts_with("#0 leaf") && stack.find("\x1b") == ::fast_io::containers::npos && stack.find("Partial stack") != ::fast_io::containers::npos, "bt lists copied safe frames and preserves partial-stack meaning");
    check(dbg::details::format_cli_reply(*values, dbg::parse_console_command("bt 1")).starts_with("stopped:"), "explicit bt preserves detailed output");
    ::fast_io::io::println("PASS CLI state, frame and usage messages");
}

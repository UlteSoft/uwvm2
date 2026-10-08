// Host-input grammar/editing tests only; this never fabricates a VM stop.
#include <uwvm2/uwvm/debugger/console_line_editor.h>
#include <uwvm2/uwvm/debugger/console_aliases.h>
#include <fast_io.h>
#include <string_view>
namespace editing = uwvm2::uwvm::debugger::console_editing;
namespace aliases = uwvm2::uwvm::debugger::console_aliases;
namespace debugger = uwvm2::uwvm::debugger;
static void check(bool ok, char const* name)
{ if(!ok) { ::fast_io::io::perrln("debug_console_editor: FAIL ", ::fast_io::mnp::os_c_str(name)); ::fast_io::fast_terminate(); } }
static void input(editing::editor& e, ::std::string_view text)
{ for(auto const c : text) { static_cast<void>(e.feed(static_cast<unsigned char>(c))); } }
static bool equal(editing::editor const& e, ::std::string_view text)
{ auto const got{e.line().view()}; return got.size() == text.size() && ::std::string_view{got.data(), got.size()} == text; }
int main()
{
    editing::editor e; e.begin(); input(e, "status");
    check(e.feed('\n') == editing::action::complete && equal(e, "status"), "line completion");
    e.remember(e.line().view(), true); e.begin();
    check(e.feed('\n') == editing::action::complete && equal(e, "status"), "eligible empty Enter repeats");
    e.remember("replace 0 1 1 owned.body", false); e.begin();
    check(e.feed('\n') == editing::action::complete && e.line().size == 0u, "mutation cannot implicitly repeat");
    input(e, "continue"); check(e.feed(3) == editing::action::interrupted && e.line().size == 0u, "typed Ctrl+C cancels partial command");
    input(e, "pause"); check(e.feed(-3) == editing::action::interrupted && e.line().size == 0u, "qualified OS Ctrl+C cancels copied line");
    e.remember("status", true); e.begin(); static_cast<void>(e.feed(-3));
    check(e.feed('\n') == editing::action::complete && equal(e, ""), "cancel cannot replay an old command through a leftover newline");
    e.begin(); check(e.feed(4) == editing::action::end, "empty Ctrl+D exits input");
    e.begin(); input(e, "status"); static_cast<void>(e.feed(1));
    check(e.feed(4) == editing::action::changed && equal(e, "tatus"), "nonempty Ctrl+D deletes only under cursor");
    static_cast<void>(e.feed(5)); check(e.feed(4) == editing::action::unchanged && equal(e, "tatus"), "Ctrl+D at nonempty end does not exit");
    e.begin(); input(e, "abc def"); static_cast<void>(e.feed(23)); check(equal(e, "abc "), "Ctrl+W deletes previous word");
    static_cast<void>(e.feed(21)); check(equal(e, ""), "Ctrl+U deletes prefix");
    input(e, "xyz"); static_cast<void>(e.feed(1)); static_cast<void>(e.feed(6)); static_cast<void>(e.feed(11));
    check(equal(e, "x") && e.cursor() == 1u, "Ctrl+K deletes suffix");
    e.begin(); input(e, "a\xc3\xa9z"); static_cast<void>(e.feed(2)); static_cast<void>(e.feed(127));
    check(equal(e, "az") && e.cursor() == 1u, "Backspace removes a complete UTF-8 codepoint");
    e.begin(); input(e, "draft"); static_cast<void>(e.feed(editing::key_up));
    // The repeat-cancellation case above explicitly remembered "status"
    // AFTER "replace". Preserve that actual history order; cancellation clears
    // only repetition/current line, not previously completed host commands.
    check(equal(e, "status"), "history recalls latest complete host command");
    static_cast<void>(e.feed(editing::key_up));
    check(equal(e, "replace 0 1 1 owned.body"), "history preserves earlier completed mutating command");
    static_cast<void>(e.feed(editing::key_down)); check(equal(e, "status"), "history walks forward in actual record order");
    static_cast<void>(e.feed(editing::key_down)); check(equal(e, "draft"), "history restores edited draft");
    for(unsigned i{}; i != 40u; ++i) { e.remember("status", true); }
    check(e.history_size() == editing::maximum_history, "history is bounded");
    e.begin(); input(e, "never"); e.forget_repeat(); e.begin();
    check(e.feed('\n') == editing::action::complete && equal(e, ""), "invalid command can clear repeat eligibility");
    e.begin(); for(unsigned i{}; i != editing::maximum_bytes; ++i) { static_cast<void>(e.feed('x')); }
    check(e.line().size == editing::maximum_bytes && e.feed('\n') == editing::action::complete, "exact 512-byte input is bounded and complete");
    e.begin(); for(unsigned i{}; i != editing::maximum_bytes + 10u; ++i) { static_cast<void>(e.feed('x')); }
    input(e, "continue"); check(e.feed('\n') == editing::action::oversized, "overflow cannot execute a suffix");
    e.begin(); input(e, "status\x1b"); check(e.feed(-1) == editing::action::oversized, "EOF after incomplete escape cannot execute prefix");
    e.begin(); input(e, "status\x1b[9"); check(e.feed('\n') == editing::action::oversized, "unknown escape discards complete command");
    e.begin(); input(e, "status"); check(e.feed('\r') == editing::action::complete, "CR completes one command");
    e.remember(e.line().view(), true); e.begin();
    check(e.feed('\n') == editing::action::unchanged && e.line().size == 0u, "CRLF cannot repeat command twice");
    check(e.feed('\n') == editing::action::complete && equal(e, "status"), "subsequent deliberate Enter still repeats");
    auto const step{aliases::parse("s", 42u, 7u)};
    check(!step.current_thread_required && step.command.kind == debugger::console_command_kind::source_step &&
        step.command.source_policy == debugger::source_step_policy::into && step.command.requested_step_thread == 42u, "s uses admitted source into");
    auto const next{aliases::parse(" next ", 42u, 7u)};
    check(next.command.source_policy == debugger::source_step_policy::over, "next uses source over");
    check(aliases::parse("finish", 42u, 7u).command.source_policy == debugger::source_step_policy::out, "finish uses source out");
    check(aliases::parse("si", 42u, 7u).command.kind == debugger::console_command_kind::assembly_step, "si uses actual native admission");
    check(aliases::parse("s 42", 0u, 0u).command.kind == debugger::console_command_kind::protocol, "existing explicit Wasm s THREAD is unchanged");
    check(aliases::parse("s", 0u, 7u).current_thread_required && aliases::parse("n", 42u, 0u).current_thread_required,
        "missing real stopped thread/stop cannot authorize shorthand");
    check(aliases::parse("disas", 42u, 7u).command.disassembly_stop_identifier == 7u, "disassembly carries current public lifetime label");
    check(aliases::parse("i r $pc", 0u, 0u).command.kind == debugger::console_command_kind::assembly_registers, "register alias retains controller auto selection");
    check(aliases::parse("i threads", 0u, 0u).command.operation == uwvm2::utils::control::operation::status, "info threads alias");
    check(aliases::repeatable(step.command) && !aliases::repeatable(debugger::parse_console_command("continue")) &&
        !aliases::repeatable(debugger::parse_console_command("replace 0 1 1 owned.body")) &&
        !aliases::repeatable(debugger::parse_console_command("wasm-script status")), "only admitted step/query categories repeat");
    check(aliases::repeatable(debugger::parse_console_command("frames")) &&
        aliases::repeatable(debugger::parse_console_command("frame")) &&
        !aliases::repeatable(debugger::parse_console_command("frame 1")) &&
        !aliases::repeatable(debugger::parse_console_command("up")) &&
        !aliases::repeatable(debugger::parse_console_command("down")), "frame display repeats; cursor changes do not");
    ::fast_io::io::println("debug_console_editor: PASS editing, bounds, cancellation, CRLF, history, repeat and admitted aliases");
}

// CLI presentation regression on copied reply data; no VM state is simulated.
#include <uwvm2/uwvm/debugger/console.h>
#include <fast_io.h>
#include <memory>
namespace dbg = uwvm2::uwvm::debugger;
using view = ::fast_io::string_view;
static void check(bool good, view label)
{ if(!good) { ::fast_io::io::perrln("FAIL console output: ", label); ::fast_io::fast_terminate(); } }
int main()
{
    auto owner{::std::make_unique<dbg::controller_reply>()}; auto& reply{*owner};
    auto const resume{dbg::parse_console_command("continue")};
    auto const status{dbg::parse_console_command("status")};
    reply.status = ::uwvm2::utils::control::error::none;
    for(auto state : {dbg::execution_status::running, dbg::execution_status::stopping})
    {
        reply.execution = state;
        check(dbg::details::format_cli_reply(reply, resume).empty(), "resume acknowledgement is quiet");
        check(!dbg::details::format_cli_reply(reply, status).empty(), "explicit status remains visible");
        check(!dbg::details::format_reply(reply, resume).empty(), "machine transport status grammar retained");
    }
    reply.execution = dbg::execution_status::running; reply.timed_out = true;
    check(!dbg::details::format_cli_reply(reply, resume).empty(), "timeout is never suppressed");
    reply.timed_out = false; reply.status = ::uwvm2::utils::control::error::source_debug_info_unavailable;
    check(dbg::details::format_cli_reply(reply, resume).starts_with("error:"), "failed execution is never suppressed");
    reply.status = ::uwvm2::utils::control::error::none;
    reply.execution = dbg::execution_status::exited;
    for(auto const& sample : {::std::pair<::std::int_least64_t, view>{0, "Program exited normally.\n"},
         {7, "Program exited with code 07.\n"}, {255, "Program exited with code 0377.\n"}, {-1, "Program exited with code -1.\n"}})
    {
        reply.guest_exit_code = sample.first;
        check(dbg::details::format_cli_reply(reply, status) == sample.second, "normal/nonzero exit event");
        check(dbg::details::format_reply(reply, status).starts_with("guest exited: "), "DAP/server exit grammar retained");
    }
    reply.execution = dbg::execution_status::stopped; reply.reason = dbg::stop_reason::breakpoint; reply.stop_identifier = 7u;
    auto stop{dbg::details::format_cli_reply(reply, resume)};
    check(stop.starts_with("stopped: breakpoint\n") && stop.find("stop-id 7") != ::fast_io::containers::npos, "actual stop report retained");
    check(dbg::details::format_program_start("/tmp/example.wasm") == "Starting program: /tmp/example.wasm\n", "loaded program label");
    auto label{dbg::details::format_program_start("/tmp/name\n\x1b[2J\xc2\x9b" "31m.wasm")};
    check(label.find("\x1b") == ::fast_io::containers::npos && label.find("\xc2\x9b") == ::fast_io::containers::npos &&
          label.find("\\n") != ::fast_io::containers::npos, "program label cannot control terminal output");
    ::std::size_t lines{}; for(char c : label) { if(c == '\n') { ++lines; } }
    check(lines == 1u, "one start message even for a multiline filename");
    ::fast_io::string output{};
    dbg::console_io io{::std::addressof(output), nullptr,
        +[](void* p, view text) noexcept { static_cast<::fast_io::string*>(p)->append(text); }};
    dbg::managed_cli_shutdown_result cleanup{}; cleanup.phase = dbg::managed_cli_shutdown_phase::completed;
    dbg::write_console_shutdown_result(io, cleanup); check(output.empty(), "successful cleanup is quiet");
    cleanup.phase = dbg::managed_cli_shutdown_phase::pending_guest_thread; cleanup.deadline_expired = true;
    dbg::write_console_shutdown_result(io, cleanup);
    check(output.starts_with("error:") && output.find("owners retained") != ::fast_io::containers::npos, "pending cleanup diagnostics retained");
    ::fast_io::io::println("PASS console output");
}

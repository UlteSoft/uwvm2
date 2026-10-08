#include <uwvm2/uwvm/debugger/command.h>
#include <uwvm2/uwvm/debugger/wasip1_calls.h>
#include <uwvm2/utils/container/impl.h>
namespace dbg = ::uwvm2::uwvm::debugger;
int main()
{
    ::uwvm2::utils::container::string line{"set wasip1 arg 0 1 "};
    auto sink{::uwvm2::utils::container::string_ref_uwvm{::std::addressof(line)}};
    for(::std::size_t i{}; i != 4096u; ++i) { ::fast_io::io::print(sink, "7a"); }
    auto command{dbg::parse_console_command(::fast_io::string_view{line.data(), line.size()})};
    if(command.kind != dbg::console_command_kind::wasip1_state || command.wasip1_state_request.value.size() != 4096u) { return 1; }
    ::fast_io::io::print(sink, "7a");
    if(dbg::parse_console_command(::fast_io::string_view{line.data(), line.size()}).kind != dbg::console_command_kind::invalid) { return 2; }
    line = "memory "; line.resize(600u, '1');
    if(dbg::parse_console_command(::fast_io::string_view{line.data(), line.size()}).kind != dbg::console_command_kind::invalid) { return 3; }
    auto guarded{dbg::parse_console_command("set wasip1 arg 0 1 61 if-stop 41")};
    if(guarded.kind != dbg::console_command_kind::wasip1_state || guarded.disassembly_stop_identifier != 41u) { return 8; }
    for(auto invalid : ::fast_io::array<::fast_io::string_view, 3u>{"set wasip1 arg 0 1 61 if-stop 0", "set wasip1 arg 0 1 61 if-stop -1", "info wasip1 args 0 if-stop 41"})
    { if(dbg::parse_console_command(invalid).kind != dbg::console_command_kind::invalid) { return 9; } }
    namespace calls = dbg::wasip1_calls; namespace ws = dbg::wasip1_state;
    ws::request request{}; request.operation = ws::action::trace_enable; request.name = u8"fd_write";
    (void)calls::apply(request);
    calls::record row{}; row.count = 4u; row.arguments[0] = 1u; row.arguments[1] = 32u; row.widths.fill(32u);
    if(calls::enter(u8"fd_read", row).call != 0u) { return 4; }
    auto call{calls::enter(u8"fd_write", row)}; calls::leave(call, true, 8u);
    request = {}; request.operation = ws::action::trace_read;
    auto page{calls::apply(request)};
    if(page.records.size() != 2u || page.records[0].returned || !page.records[1].returned || page.records[1].call != call.call || calls::errno_name(page.records[1].result) != "ebadf") { return 5; }
    request.operation = ws::action::trace_clear; (void)calls::apply(request); calls::leave(call, true, 0u);
    request.operation = ws::action::trace_read; if(!calls::apply(request).records.empty()) { return 6; }
    request.operation = ws::action::trace_enable; request.name.clear(); (void)calls::apply(request);
    for(::std::size_t i{}; i != 260u; ++i) { auto c{calls::enter(u8"fd_write", row)}; calls::leave(c, true, 0u); }
    request = {}; request.operation = ws::action::trace_read; request.count = 64u;
    page = calls::apply(request);
    if(page.records.size() != 64u || page.overwritten != 8u || !page.gap || page.remaining != 448u) { return 7; }
    ::fast_io::io::println("WASIp1 long command and trace regression PASS");
}

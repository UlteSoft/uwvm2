// Pure host grammar, not runtime credentials or product read qualification.
#include <uwvm2/uwvm/debugger/command.h>
#include <fast_io.h>
#include <array>
#include <string_view>
#define CHECK(x) do { if(!(x)) { ::fast_io::io::perrln("native disassembly grammar failure line=", __LINE__); ::fast_io::fast_terminate(); } } while(false)
namespace dbg = ::uwvm2::uwvm::debugger;
static auto parse(::std::string_view text)
{
    // [caller-owned bounded immutable command] end
    // [safe                                ] borrowed only synchronously.
    return dbg::parse_console_command(::fast_io::string_view{text.data(), text.size()});
}
int main()
{
    auto const good{parse("disassemble 3 17 32")};
    CHECK(good.kind == dbg::console_command_kind::assembly_disassemble);
    CHECK(good.operation == ::uwvm2::utils::control::operation::backtrace && good.payload_size == 8u);
    CHECK(good.requested_step_thread == 3u && good.disassembly_stop_identifier == 17u && good.disassembly_count == 32u);
    for(auto const value : {"disassemble 0 17 1", "disassemble 3 0 1", "disassemble 3 17 0", "disassemble 3 17 33",
                           "disassemble 3 17 -1", "disassemble 3 17 1 0x1234", "disassemble 3 17 1 -1",
                           "disassemble 3 0x17 1", "disassemble 3 18446744073709551616 1", "disassemble 3 17 1;quit"})
    { CHECK(parse(value).kind == dbg::console_command_kind::invalid); }
    CHECK(parse("disassemble 3 17 1").disassembly_count == 1u);
    ::fast_io::io::println("native disassembly grammar: PASS bounded decimal participant/stop/count, no address/offset input");
}

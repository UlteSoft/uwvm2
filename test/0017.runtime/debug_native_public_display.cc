// Formatter regression DATA only; this does not mint runtime/native authority.
#include <uwvm2/uwvm/debugger/console.h>
#include <fast_io.h>
#include <string>
namespace dbg = ::uwvm2::uwvm::debugger;
template<typename T> concept copies_native_bytes = requires(T value) { value.bytes; };
static_assert(!copies_native_bytes<dbg::native_display_identity>);
int main()
{
    dbg::controller_reply reply{};
    reply.status = ::uwvm2::utils::control::error::none;
    reply.disassembly_stop_identifier = 7u; reply.disassembly_count = 2u;
    auto& code{reply.disassembly_code};
    code.participant = 1u; code.pc = 0x1000u;
    code.function_generation = 1u; code.runtime_epoch = 1u;
    code.native_instruction_stop = true;
    // Hidden rows retain only the authenticated boundary label. No bytes,
    // decoded instruction, destination or additional boundary is returned.
    reply.disassembly[0u].pc = code.pc; reply.disassembly[1u].pc = code.pc;
    auto const command{dbg::parse_console_command(::fast_io::string_view{"disassemble 1 7 2"})};
    auto const text{dbg::details::format_reply(reply, command)};
    if(text.find("error:") != ::std::string::npos ||
       text.find("instruction 0 pc=0x") == ::std::string::npos ||
       text.find("instruction 1 pc=0x") == ::std::string::npos ||
       text.find(" unavailable\n") == ::std::string::npos ||
       text.find("bytes=") != ::std::string::npos) { ::fast_io::io::perr(text); return 1; }
    ::fast_io::io::println("PASS hidden native rows format successfully without copied code payload");
}

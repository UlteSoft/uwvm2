// Production projection/formatter DATA only, not OS or genuine JIT qualification.
#include <uwvm2/uwvm/debugger/console.h>
#include <fast_io.h>
namespace dbg = ::uwvm2::uwvm::debugger;
int main()
{
    using machine = dbg::native_registers::architecture;
    for(auto architecture : {machine::x86_64, machine::aarch64, machine::i686, machine::powerpc,
                            machine::mips64, machine::riscv64, machine::loongarch64,
                            machine::sparc64, machine::s390x, machine::arm})
    {
        unsigned const minimum{architecture == machine::i686 || architecture == machine::arm || architecture == machine::powerpc ? 32u : 64u};
        unsigned const maximum{architecture == machine::i686 || architecture == machine::arm ? 32u : 64u};
        for(unsigned bits{minimum}; bits <= maximum; bits += 32u)
        {
            dbg::native_registers::snapshot raw{}; raw.machine = architecture; raw.word_bits = bits;
            raw.values[dbg::native_registers::pc_index(architecture)] = 0x1234u;
            for(::std::size_t i{}; i != raw.size(); ++i)
            { if(i != dbg::native_registers::pc_index(architecture)) { raw.values[i] = 0xdecafbad12345678ull; } }
            dbg::native_registers::numeric_location locations[32u]{};
            for(unsigned i{}; i != 32u; ++i) { locations[i] = {i,32u}; }
            dbg::controller_reply reply{};
            reply.registers = dbg::native_registers::project(raw, locations, 32u);
            reply.registers_stop_identifier = 17u;
            reply.disassembly_code.pc = 0x1234u; reply.disassembly_code.participant = 3u;
            reply.disassembly_code.function = 2u; reply.disassembly_code.function_generation = 2u;
            reply.disassembly_code.runtime_epoch = 8u;
            auto const command{dbg::parse_console_command(::fast_io::string_view{"info registers 3 17 all"})};
            auto const text{dbg::details::format_reply(reply,command)};
            if(text.starts_with("error:")) { ::fast_io::io::perr(text); return 1; }
            ::fast_io::io::print(text);
        }
    }
}

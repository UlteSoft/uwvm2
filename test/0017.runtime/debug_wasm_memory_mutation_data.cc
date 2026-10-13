// Cold command/transport DATA only: no runtime capture or memory authority.
#include <uwvm2/uwvm/debugger/command.h>
#include <uwvm2/uwvm/debugger/console_line_editor.h>
#include <fast_io.h>
#include <bit>
#include <cstdint>
#include <string>
namespace dbg=::uwvm2::uwvm::debugger;
namespace wm=dbg::wasm_mutation;
namespace editing=dbg::console_editing;
static void require(bool good,char const* reason)
{ if(!good) { ::fast_io::io::perrln("Wasm memory mutation DATA FAIL: ",::fast_io::mnp::os_c_str(reason));::fast_io::fast_terminate(); } }
int main(int argc,char** argv)
{
    if(argc==2) { require(::fast_io::string_view{::fast_io::mnp::os_c_str(argv[1])}=="--require-big" &&
        ::std::endian::native==::std::endian::big,"actual big endian target"); }
    else { require(argc==1,"finite argument count"); }
    auto request{dbg::parse_console_command("set wasm memory 3 4 7 4294967304 bytes 00807fFF")};
    require(request.kind==dbg::console_command_kind::wasm_mutation && wm::valid(request.wasm_mutation_request),"actual bounded command");
    auto const& value{request.wasm_mutation_request};
    require(value.target==wm::destination::memory && value.source==wm::source_kind::bytes && value.module==3u && value.index==4u &&
        value.participant==7u && value.element==UINT64_C(4294967304) && value.memory_size==4u,"actual logical index+wide offset DATA");
    require(value.memory_bytes[0u]==::std::byte{} && value.memory_bytes[1u]==::std::byte{0x80u} &&
        value.memory_bytes[2u]==::std::byte{0x7fu} && value.memory_bytes[3u]==::std::byte{0xffu},"byte order is host endian independent");
    ::std::string hex(512u,'0');
    auto line{::fast_io::concat_std("set wasm memory 3 4 7 4294967304 bytes ",hex)};
    require(line.size()>dbg::max_command_bytes && line.size()<=dbg::max_input_command_bytes,"full256 HEX needs transport extension");
    auto maximum{dbg::parse_console_command(::fast_io::string_view{line.data(),line.size()})};
    require(maximum.kind==dbg::console_command_kind::wasm_mutation && maximum.wasm_mutation_request.memory_size==256u,"maximum256 parser accepted");
    editing::editor editor{};editor.begin();
    for(auto byte:line) { static_cast<void>(editor.feed(static_cast<unsigned char>(byte))); }
    require(editor.feed('\n')==editing::action::complete && editor.line().size==line.size(),"interactive editor preserves whole maximum payload");
    require(dbg::parse_console_command(editor.line().view()).kind==dbg::console_command_kind::wasm_mutation,"edited whole memory grammar");
    auto bad{value};bad.memory_size=0u;require(!wm::valid(bad),"zero-byte no-op denied");
    bad=value;bad.memory_size=257u;require(!wm::valid(bad),"native API payload bound");
    bad=value;bad.memory_bytes[4u]=::std::byte{1u};require(!wm::valid(bad),"inactive payload tail cannot hide bytes");
    bad=value;bad.target=wm::destination::global;require(!wm::valid(bad),"raw bytes cannot masquerade as numeric or reference carrier");
    bad=value;bad.source=wm::source_kind::function;require(!wm::valid(bad),"memory source is only owned raw bytes");
    bad=value;bad.original.long_path.push_back(0u);require(!wm::valid(bad),"memory request cannot hide allocated source path");
    bad=value;bad.path_handle=1u;require(!wm::valid(bad),"memory cannot inherit DATA path-label authority");
    for(auto text:{"set wasm memory 3 4 7 0 bytes 0","set wasm memory 3 4 7 0 bytes 0x",
        "set wasm memory 3 4 7 0 bytes gg","set wasm memory 3 4 0 0 bytes 00",
        "set wasm memory 3 -1 7 0 bytes 00","set wasm memory 3 4 7 -1 bytes 00",
        "set wasm memory 3 4 7 18446744073709551616 bytes 00","set wasm memory 3 4 7 0 bits i32 00",
        "set wasm memory 3 4 7 0 bytes 00 extra","set wasm native 3 4 7 0 bytes 00"})
    { require(dbg::parse_console_command(::fast_io::string_view{::fast_io::mnp::os_c_str(text)}).kind==dbg::console_command_kind::invalid,
        "malformed grammar/sign/overflow/address/width denied before runtime"); }
    hex.append(2u,'0');line=::fast_io::concat_std("set wasm memory 3 4 7 0 bytes ",hex);
    require(dbg::parse_console_command(::fast_io::string_view{line.data(),line.size()}).kind==dbg::console_command_kind::invalid,"257 bytes denied before any copy");
    line.assign(513u,'x');require(dbg::parse_console_command(::fast_io::string_view{line.data(),line.size()}).kind==dbg::console_command_kind::invalid,"legacy grammar retains512 budget");
    line.assign(dbg::max_input_command_bytes+1u,'x');
    require(dbg::parse_console_command(::fast_io::string_view{line.data(),line.size()}).kind==dbg::console_command_kind::invalid,"whole transport quota checked");
    editor.begin();for(auto byte:line) { static_cast<void>(editor.feed(static_cast<unsigned char>(byte))); }
    require(editor.feed('\n')==editing::action::oversized,"transport overflow cannot execute suffix");
    wm::result result{};result.target=wm::destination::memory;result.status=dbg::wasm_state::status::available;
    result.runtime_epoch=9u;result.module=3u;result.index=4u;result.element=UINT64_C(4294967304);result.memory_size=256u;result.address_bytes=8u;result.applied=true;
    auto text{wm::format(result)};
    require(text.find("Wasm mutation v=3")!=::std::string::npos && text.find("target=memory")!=::std::string::npos &&
        text.find("bytes=256 address-bytes=8")!=::std::string::npos && text.find("element=4294967304")!=::std::string::npos,"actual format full numeric coordinates without native pointers");
    ::fast_io::io::println("Wasm memory mutation DATA PASS endian=",::std::endian::native==::std::endian::little ? ::fast_io::string_view{"little"} : ::fast_io::string_view{"big"}," runtime-authority=false");
}

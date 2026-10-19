#include <uwvm2/uwvm/debugger/wasm_symbols.h>
#include <uwvm2/uwvm/debugger/wasm_listing.h>
#include <uwvm2/uwvm/debugger/console_debug_highlight.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace uwvm2::uwvm::debugger;
static void require(bool ok,char const* what) { if(!ok) { std::fprintf(stderr,"FAIL %s\n",what);std::abort(); } }
static std::span<std::byte const> bytes(std::string_view v) { return {reinterpret_cast<std::byte const*>(v.data()),v.size()}; }
int main()
{
    wasm_symbols::table names{};names.functions.emplace(2,"export_fallback");names.globals.emplace(2,"wrong_space");
    std::string name_map{char(1),char(2),char(4),'l','e','a','f'};
    std::string section{char(1),char(name_map.size())};section+=name_map;
    wasm_symbols::load_name_section(bytes(section),names);
    require(wasm_symbols::lookup(names,false,2)=="leaf","name section overrides export");
    std::string call{char(0x10),char(2)},get{char(0x23),char(2)};
    require(wasm_symbols::reference(names,bytes(call))=="leaf","function index space");
    require(wasm_symbols::reference(names,bytes(get))=="wrong_space","global index space");
    require(wasm_symbols::reference(names,bytes(std::string{char(0x11),char(2)})).empty(),"indirect type index is not a function");
    require(wasm_symbols::reference(names,bytes(std::string{char(0x10),char(0x80)})).empty(),"truncated LEB rejected");
    require(wasm_listing::instruction(bytes(call)).subview(0u)=="call 2","fully decoded call omits raw bytes");
    std::string load{char(0x28),char(2),char(8)};
    require(wasm_listing::instruction(bytes(load)).subview(0u)=="i32.load offset=8 align=4","memory immediate preserved");
    auto before=names.functions;section.pop_back();wasm_symbols::load_name_section(bytes(section),names);
    require(names.functions==before,"malformed optional section preserves fallbacks");
    require(wasm_symbols::display("a\x1b[2J") == "a?[2J","metadata controls cannot repaint terminal");
    using style=console_source_highlight::style;
    bool mnemonic{},number{},symbol{},reg{};
    console_debug_highlight::scan("=> 4  call 2  <leaf(int)>",false,[&](auto a,auto b,style s)
    { auto word{fast_io::string_view{"=> 4  call 2  <leaf(int)>"}.subview(a,b-a)};mnemonic|=word=="call"&&s==style::keyword;number|=word=="2"&&s==style::number;symbol|=word=="<leaf(int)>"&&s==style::string; });
    console_debug_highlight::scan("   0x1000  mov x0, #7",true,[&](auto a,auto b,style s)
    { reg|=fast_io::string_view{"   0x1000  mov x0, #7"}.subview(a,b-a)=="x0"&&s==style::type; });
    require(mnemonic&&number&&symbol&&reg,"semantic colours distinguish operands");
    std::puts("PASS Wasm symbols, malformed maps, index spaces, operands and instruction colours");
}

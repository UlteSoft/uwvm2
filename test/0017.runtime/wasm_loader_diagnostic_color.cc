// Formatting DATA only: selected ANSI bytes keep the original diagnostic
// order without creating a conditional type alternative for every fragment.
#include <uwvm2/uwvm/wasm/loader/wasm_file.h>
#include <fast_io.h>
namespace loader = ::uwvm2::uwvm::wasm::loader;
namespace ansi = ::uwvm2::uwvm::utils::ansies;

int main()
{
    auto const saved{ansi::put_color};
    for(bool enabled : {false, true})
    {
        ansi::put_color = enabled;
        auto const text{::fast_io::u8concat(
            loader::details::diagnostic_color(u8"\033[31m"), u8"error ",
            loader::details::diagnostic_color(u8"\033[37m"), u8"module ",
            loader::details::diagnostic_color(u8"\033[33m"), u8"name",
            loader::details::diagnostic_color(u8"\033[0m"), u8"\n")};
        auto const expected{enabled ? ::fast_io::u8string_view{u8"\033[31merror \033[37mmodule \033[33mname\033[0m\n"} : ::fast_io::u8string_view{u8"error module name\n"}};
        if(::fast_io::u8string_view{text.data(), text.size()} != expected)
        { ansi::put_color = saved; ::fast_io::io::perrln("FAIL loader diagnostic color bytes/order"); return 1; }
    }
    ansi::put_color = saved;
    ::fast_io::io::println("PASS loader diagnostic colors on/off: exact bytes/order, bounded fast_io text");
}

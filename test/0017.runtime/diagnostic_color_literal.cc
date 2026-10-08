// ANSI literal output is identical while formatter alternatives stay bounded.
#include <uwvm2/uwvm/utils/ansies/impl.h>
#include <uwvm2/utils/container/string_concat.h>
#include <type_traits>
#include <fast_io.h>
namespace color = uwvm2::uwvm::utils::ansies;
static_assert(std::is_same_v<decltype(color::diagnostic_color(u8"\033[31m")),fast_io::basic_io_scatter_t<char8_t>>);
int main()
{
    for(bool enabled:{false,true})
    {
        color::put_color = enabled;
        auto const output{uwvm2::utils::container::u8concat_uwvm(color::diagnostic_color(u8"\033[31m"),u8"one",
            color::diagnostic_color(u8"\033[32m"),u8"two",color::diagnostic_color(u8"\033[0m"),u8"\n")};
        auto const expected{enabled ? u8"\033[31mone\033[32mtwo\033[0m\n" : u8"onetwo\n"};
        if(output != uwvm2::utils::container::u8string_view{fast_io::mnp::os_c_str(expected)}) { fast_io::io::perrln("ANSI literal policy mismatch");return 1; }
        // The fallback still supports a nonliteral formatting manipulator.
        auto const generic{uwvm2::utils::container::u8concat_uwvm(color::diagnostic_color(fast_io::mnp::cond(true,u8"token")))};
        if(generic != uwvm2::utils::container::u8string_view{fast_io::mnp::os_c_str(enabled ? u8"token" : u8"")}) { return 2; }
    }
    fast_io::io::println("PASS ANSI literal on/off bytes and generic manipulator fallback");
}

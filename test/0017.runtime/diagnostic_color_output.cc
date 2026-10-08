// Compare actual FastIO formatting bytes, including empty/colored scatter
// records. This tests the conditional diagnostic path, not an emulated stream.
#include <type_traits>
#include <fast_io.h>
#include <fast_io_dsal/string.h>
#include <uwvm2/uwvm/utils/ansies/impl.h>
#include <uwvm2/uwvm/utils/ansies/uwvm_color_push_macro.h>

int main()
{
    namespace color = ::uwvm2::uwvm::utils::ansies;
    auto const previous{color::put_color};
    unsigned checks{};
    for(bool enabled : {false, true, false})
    {
        color::put_color = enabled;
        auto compare = [&](auto&& token)
        {
            auto const actual{::fast_io::u8concat_fast_io(u8"before", color::diagnostic_color(token), u8"after")};
            auto const expected{::fast_io::u8concat_fast_io(u8"before", ::fast_io::mnp::cond(enabled, token), u8"after")};
            if(actual != expected)
            { ::fast_io::io::perrln("diagnostic color bytes differ"); ::fast_io::fast_terminate(); }
            ++checks;
        };
        compare(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE);
        compare(UWVM_COLOR_U8_YELLOW);
        compare(UWVM_COLOR_U8_CYAN);
        compare(UWVM_COLOR_U8_LT_GREEN);
        compare(UWVM_COLOR_U8_WHITE);
        compare(UWVM_COLOR_U8_ORANGE);
        compare(UWVM_COLOR_U8_LT_RED);
        compare(UWVM_COLOR_U8_RST_ALL);
        auto const actual{::fast_io::u8concat_fast_io(
            color::diagnostic_color(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE), u8"uwvm: ",
            color::diagnostic_color(UWVM_COLOR_U8_YELLOW), u8"[warn]  ",
            color::diagnostic_color(UWVM_COLOR_U8_WHITE), u8"type[",
            color::diagnostic_color(UWVM_COLOR_U8_CYAN), 42u,
            color::diagnostic_color(UWVM_COLOR_U8_WHITE), u8"] duplicate.",
            color::diagnostic_color(UWVM_COLOR_U8_RST_ALL), u8"\n")};
        auto const expected{enabled ?
            ::fast_io::u8concat_fast_io(UWVM_COLOR_U8_RST_ALL_AND_SET_WHITE, u8"uwvm: ",
                UWVM_COLOR_U8_YELLOW, u8"[warn]  ", UWVM_COLOR_U8_WHITE, u8"type[",
                UWVM_COLOR_U8_CYAN, 42u, UWVM_COLOR_U8_WHITE, u8"] duplicate.", UWVM_COLOR_U8_RST_ALL, u8"\n") :
            ::fast_io::u8concat_fast_io(u8"uwvm: [warn]  type[42] duplicate.\n")};
        if(actual != expected)
        { ::fast_io::io::perrln("wide diagnostic bytes differ"); ::fast_io::fast_terminate(); }
        ++checks;
    }
    color::put_color = previous;
    ::fast_io::io::println("diagnostic_color_output PASS checks=", checks);
}

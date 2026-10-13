// Build with the hosted header or with UWVM_TEST_IMPORT_FAST_IO and a fresh
// fast_io PCM/provider object; the latter deliberately includes no fast_io header.
#include <memory>
#include <string>
#include <type_traits>
#if defined(UWVM_TEST_IMPORT_FAST_IO)
import fast_io;
#else
#include <fast_io.h>
#include <fast_io_unit/string.h>
#endif

template<typename Char>
[[nodiscard]] bool check_character_view()
{
    constexpr Char expected{::fast_io::char_literal_v<u8'A', Char>};
    constexpr Char replacement{::fast_io::char_literal_v<u8'B', Char>};
    Char source{expected};
    ::fast_io::manipulators::chvw_t<Char> const owned{::fast_io::mnp::chvw(source)};
    static_assert(::std::is_same_v<decltype(::fast_io::mnp::chvw(source)),
                                  ::fast_io::manipulators::chvw_t<Char>>);
    source = replacement;
    if(owned.reference != expected || source != replacement) { return false; }

    ::std::basic_string<Char> text{};
    ::fast_io::basic_ostring_ref_std<Char> output{::std::addressof(text)};
    ::fast_io::io::print(output, owned, ::fast_io::mnp::chvw(Char{}), ::fast_io::mnp::chvw(source));
    // The explicit carrier must emit one code unit even when its value is zero.
    // The bounded string indices below are used only after proving its extent.
    if(text.size() != 3u || text[0u] != expected || text[1u] != Char{} || text[2u] != replacement)
    { return false; }
    return true;
}

int main()
{
    auto const passed{check_character_view<char>() && check_character_view<char8_t>() &&
        check_character_view<char16_t>() && check_character_view<char32_t>() &&
        check_character_view<wchar_t>()};
    // Exercise concat's eager/reserve path as well as the direct string-ref print.
    auto const copied{::fast_io::concat_std(::fast_io::mnp::chvw('A'), ::fast_io::mnp::chvw('B'))};
    if(!passed || copied != "AB") { return 1; }
    ::fast_io::println(::fast_io::out(), "PASS public character-view carrier");
}

// Use the identical public predicates via the hosted header or the fresh BMI.
#if defined(UWVM_TEST_IMPORT_FAST_IO)
import fast_io;
#else
#include <fast_io.h>
#endif

template <typename Char>
[[nodiscard]] constexpr bool check_categories()
{
    namespace category = ::fast_io::char_category;
    constexpr auto upper{::fast_io::char_literal_v<u8'A', Char>};
    constexpr auto lower{::fast_io::char_literal_v<u8'a', Char>};
    constexpr auto digit{::fast_io::char_literal_v<u8'7', Char>};
    constexpr auto space{::fast_io::char_literal_v<u8' ', Char>};
    constexpr auto tab{::fast_io::char_literal_v<u8'\t', Char>};
    constexpr auto newline{::fast_io::char_literal_v<u8'\n', Char>};
    constexpr auto punctuation{::fast_io::char_literal_v<u8'!', Char>};
    constexpr auto not_hex{::fast_io::char_literal_v<u8'G', Char>};
    return category::is_c_alpha(upper) && category::is_c_alpha(lower) &&
           !category::is_c_alpha(digit) && category::is_c_alnum(upper) &&
           category::is_c_alnum(digit) && !category::is_c_alnum(punctuation) &&
           category::is_c_digit(digit) && !category::is_c_digit(upper) &&
           category::is_c_blank(space) && category::is_c_blank(tab) &&
           !category::is_c_blank(newline) && category::is_c_space(newline) &&
           category::is_c_space(space) && !category::is_c_space(upper) &&
           category::is_c_cntrl(newline) && category::is_c_cntrl(Char{}) &&
           !category::is_c_cntrl(upper) && category::is_c_graph(upper) &&
           category::is_c_graph(punctuation) && !category::is_c_graph(space) &&
           !category::is_c_graph(Char{}) && category::is_c_print(space) &&
           !category::is_c_print(newline) && category::is_c_punct(punctuation) &&
           !category::is_c_punct(upper) && category::is_c_lower(lower) &&
           !category::is_c_lower(upper) && category::is_c_upper(upper) &&
           !category::is_c_upper(lower) && category::is_c_xdigit(lower) &&
           category::is_c_xdigit(digit) && !category::is_c_xdigit(not_hex);
}

static_assert(check_categories<char>());
static_assert(check_categories<char8_t>());
static_assert(check_categories<char16_t>());
static_assert(check_categories<char32_t>());
static_assert(check_categories<wchar_t>());
static_assert(::fast_io::char_category::is_c_cntrl(char8_t{0x7f}));
static_assert(!::fast_io::char_category::is_c_alpha(char8_t{0xc3}));

int main()
{
    bool const passed{check_categories<char>() && check_categories<char8_t>() &&
                      check_categories<char16_t>() && check_categories<char32_t>() &&
                      check_categories<wchar_t>()};
    ::fast_io::println(::fast_io::out(), "public character categories: ", passed);
    return passed ? 0 : 1;
}

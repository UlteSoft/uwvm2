#include <array>
#include <cstdint>
#if defined(UWVM_TEST_IMPORT_FAST_IO)
import fast_io;
#else
#include <fast_io.h>
#endif

template<typename Char>
[[nodiscard]] bool check_decimal_result()
{
    ::std::array<Char, 2u> const input{
        ::fast_io::char_literal_v<u8'4', Char>, ::fast_io::char_literal_v<u8'2', Char>};
    ::std::uint32_t value{};
    // [owned two-character input] end
    // [safe                    ] one-past, never dereferenced;
    //  ^^ the fixed extent bounds input.data()+input.size() before scanning.
    Char const* const first{input.data()};
    Char const* const last{first + input.size()};
    ::fast_io::parse_result<Char const*> const parsed{
        ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::dec_get<true, true>(value))};
    return parsed.code == ::fast_io::parse_code::ok && parsed.iter == last && value == 42u;
}

template<::std::size_t Bits>
[[nodiscard]] bool check_little_endian_result()
{
    static_assert(Bits == 8u || Bits == 16u || Bits == 32u || Bits == 64u);
    ::std::array<char, 8u> const bytes{4, 3, 2, 1, 0, 0, 0, 0};
    ::std::uint64_t value{};
    // [owned eight-byte input] end
    // [safe                 ] bounded one-past prefix;
    //  ^^ Bits/8<=8 is proven above before forming this scanner endpoint.
    char const* const first{bytes.data()};
    char const* const last{first + Bits / 8u};
    ::fast_io::parse_result<char const*> const parsed{
        ::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<Bits>(value))};
    constexpr ::std::uint64_t expected{Bits == 8u ? 4u : Bits == 16u ? 0x0304u : 0x01020304u};
    return parsed.code == ::fast_io::parse_code::ok && parsed.iter == last && value == expected;
}

int main()
{
    auto const passed{check_decimal_result<char>() && check_decimal_result<char8_t>() &&
        check_decimal_result<char16_t>() && check_decimal_result<char32_t>() && check_decimal_result<wchar_t>() &&
        check_little_endian_result<8u>() && check_little_endian_result<16u>() &&
        check_little_endian_result<32u>() && check_little_endian_result<64u>()};
    ::fast_io::println(::fast_io::out(), "public typed parse results: ", passed);
    return passed ? 0 : 1;
}

// Header and true PCM-only import profiles; no preincluded fast_io header in
// the importer. Both products and EH/noEH run only via the shared cgroup lane.
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#if defined(UWVM_TEST_IMPORT_FAST_IO)
import fast_io;
#else
#include <fast_io.h>
#include <fast_io_unit/string.h>
#endif

#if CHAR_BIT == 8
static unsigned checks{};
#define CHECK(x) do { ++checks; if(!(x)) { ::fast_io::perrln("FAIL ", __LINE__, " ", ::fast_io::mnp::os_c_str(#x)); return 1; } } while(false)
template<typename Char>
int exercise()
{
    Char input[6]{static_cast<Char>(0x55u), static_cast<Char>(0x12u), static_cast<Char>(0x34u),
        static_cast<Char>(0x56u), static_cast<Char>(0x78u), static_cast<Char>(0xAAu)};
    // [left sentinel] [four complete initialized wire cells] [right sentinel]
    //                ^^ first                          last ^^
    // The six-cell owned extent proves both endpoints before forming them.
    auto const first{input + 1u}, last{input + 5u};
    ::std::uint32_t output{0xA5A5A5A5u};
    auto const parsed{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::be_get<32u>(output))};
    CHECK(parsed.code == ::fast_io::parse_code::ok && parsed.iter == last && output == 0x12345678u);
    CHECK(input[0] == static_cast<Char>(0x55u) && input[5] == static_cast<Char>(0xAAu));
    output = 0xA5A5A5A5u;
    auto const little{::fast_io::parse_by_scan(first, last, ::fast_io::mnp::le_get<32u>(output))};
    CHECK(little.code == ::fast_io::parse_code::ok && little.iter == last && output == 0x78563412u);
    output = 0xA5A5A5A5u;
    // Exactly three initialized cells remain; this endpoint is still within
    // the same owned array. Truncation must not publish a partial destination.
    auto const truncated{::fast_io::parse_by_scan(first, first + 3u, ::fast_io::mnp::be_get<32u>(output))};
    CHECK(truncated.code != ::fast_io::parse_code::ok && output == 0xA5A5A5A5u);
    auto const little_truncated{::fast_io::parse_by_scan(first, first + 3u, ::fast_io::mnp::le_get<32u>(output))};
    CHECK(little_truncated.code != ::fast_io::parse_code::ok && output == 0xA5A5A5A5u);
    ::std::basic_string<Char> encoded{};
    ::fast_io::basic_ostring_ref_std<Char> destination{::std::addressof(encoded)};
    ::fast_io::io::print(destination, ::fast_io::mnp::le_put<32u>(::std::uint32_t{0x12345678u}));
    CHECK(encoded.size() == 4u);
    // Exactly four owned initialized elements were emitted. Check this extent
    // before indexing or forming the scanner's one-past endpoint.
    CHECK(encoded[0u] == static_cast<Char>(0x78u) && encoded[1u] == static_cast<Char>(0x56u) &&
        encoded[2u] == static_cast<Char>(0x34u) && encoded[3u] == static_cast<Char>(0x12u));
    auto const encoded_first{encoded.data()}, encoded_last{encoded.data() + encoded.size()};
    auto const encoded_parsed{::fast_io::parse_by_scan(encoded_first, encoded_last, ::fast_io::mnp::le_get<32u>(output))};
    CHECK(encoded_parsed.code == ::fast_io::parse_code::ok && encoded_parsed.iter == encoded_last && output == 0x12345678u);
    Char negative[4]{static_cast<Char>(0xFEu), static_cast<Char>(0xDCu), static_cast<Char>(0xBAu), static_cast<Char>(0x98u)};
    ::std::int32_t signed_output{};
    // Complete four-cell owned object; its one-past endpoint is formed only
    // with this exact extent, without a native misaligned typed dereference.
    auto const signed_parsed{::fast_io::parse_by_scan(negative, negative + 4u, ::fast_io::mnp::be_get<32u>(signed_output))};
    CHECK(signed_parsed.code == ::fast_io::parse_code::ok && signed_parsed.iter == negative + 4u &&
        ::std::bit_cast<::std::uint32_t>(signed_output) == 0xFEDCBA98u);
    Char wide[8]{static_cast<Char>(0x01u), static_cast<Char>(0x23u), static_cast<Char>(0x45u), static_cast<Char>(0x67u),
        static_cast<Char>(0x89u), static_cast<Char>(0xABu), static_cast<Char>(0xCDu), static_cast<Char>(0xEFu)};
    ::std::uint64_t wide_output{};
    // The eight-cell extent is owned and complete, independently of host byte
    // order or Char's width; no typed misaligned native load is used.
    auto const wide_big{::fast_io::parse_by_scan(wide, wide + 8u, ::fast_io::mnp::be_get<64u>(wide_output))};
    CHECK(wide_big.code == ::fast_io::parse_code::ok && wide_big.iter == wide + 8u && wide_output == 0x0123456789ABCDEFuLL);
    auto const wide_little{::fast_io::parse_by_scan(wide, wide + 8u, ::fast_io::mnp::le_get<64u>(wide_output))};
    CHECK(wide_little.code == ::fast_io::parse_code::ok && wide_little.iter == wide + 8u && wide_output == 0xEFCDAB8967452301uLL);
    return 0;
}
int main()
{
    if(exercise<char>() || exercise<wchar_t>() || exercise<char8_t>() || exercise<char16_t>() || exercise<char32_t>()) { return 1; }
    ::fast_io::println("PASS public little/big-endian scanner checks ", checks);
}
#else
int main() { return 77; }
#endif

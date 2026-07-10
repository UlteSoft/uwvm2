#include <cfloat>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>

#include <array>
#include <bit>
#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#include <fast_io.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#if __has_include(<fast_float/fast_float.h>)
#define UWVM2_FAST_IO_FLOAT_EXT_HAS_FAST_FLOAT 1
#include <fast_float/fast_float.h>
#endif

#if defined(FAST_IO_FLOAT_EXT_USE_DRAGONBOX)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshift-count-overflow"
#endif
#include <dragonbox/dragonbox_to_chars.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#define UWVM2_FAST_IO_FLOAT_EXT_HAS_DRAGONBOX 1
#endif

namespace
{

using ::fast_io::parse_code;

[[noreturn]] void fail(::std::string_view context, ::std::string_view detail = {}) noexcept
{
    if(detail.empty())
    {
        ::std::fprintf(stderr, "fast_io_float_ext_fuzzer failure: %.*s\n", static_cast<int>(context.size()),
                       context.data());
    }
    else
    {
        ::std::fprintf(stderr, "fast_io_float_ext_fuzzer failure: %.*s: %.*s\n",
                       static_cast<int>(context.size()), context.data(), static_cast<int>(detail.size()),
                       detail.data());
    }
    ::std::abort();
}

template <typename Manip>
[[nodiscard]] ::std::string fast_io_text(Manip manip)
{
    using manip_type = ::std::remove_cvref_t<Manip>;
    ::std::size_t reserve_size{};
    if constexpr(requires { ::fast_io::print_reserve_size(::fast_io::io_reserve_type<char, manip_type>, manip); })
    {
        reserve_size = ::fast_io::print_reserve_size(::fast_io::io_reserve_type<char, manip_type>, manip);
    }
    else
    {
        reserve_size = ::fast_io::print_reserve_size(::fast_io::io_reserve_type<char, manip_type>);
    }

    ::std::string out(reserve_size, '\0');
    auto* const end{
        ::fast_io::print_reserve_define(::fast_io::io_reserve_type<char, manip_type>, out.data(), manip)};
    out.resize(static_cast<::std::size_t>(end - out.data()));
    return out;
}

template <typename T, typename MakeManip>
[[nodiscard]] bool parse_full(::std::string_view text, T& out, MakeManip make_manip) noexcept
{
    auto const* const first{text.data()};
    auto const* const last{text.data() + text.size()};
    auto const [next, err]{::fast_io::parse_by_scan(first, last, make_manip(out))};
    return err == parse_code::ok && next == last;
}

[[nodiscard]] ::std::uint_least64_t load_u64(::std::uint8_t const* data, ::std::size_t size,
                                             ::std::size_t offset) noexcept
{
    ::std::uint_least64_t raw{};
    for(::std::size_t i{}; i != sizeof(raw) && offset + i < size; ++i)
    {
        raw |= static_cast<::std::uint_least64_t>(data[offset + i]) << (i * 8u);
    }
    return raw;
}

[[nodiscard]] double double_from_prefix(::std::uint8_t const* data, ::std::size_t size) noexcept
{
    return ::std::bit_cast<double>(static_cast<::std::uint64_t>(load_u64(data, size, 0)));
}

[[nodiscard]] ::std::string printable_token(::std::uint8_t const* data, ::std::size_t size)
{
    ::std::string text;
    text.reserve(size);
    for(::std::size_t i{}; i != size; ++i)
    {
        if(data[i] == '\t' || data[i] == '\n' || data[i] == '\r' || (0x20u <= data[i] && data[i] < 0x7fu))
        {
            text.push_back(static_cast<char>(data[i]));
        }
    }
    return text;
}

#if defined(__SIZEOF_FLOAT80__)

struct float80_bits
{
    ::std::uint_least64_t mantissa{};
    ::std::uint_least16_t sign_exponent{};
};

[[nodiscard]] float80_bits get_float80_bits(__float80 value) noexcept
{
    ::std::array<unsigned char, sizeof(__float80)> bytes{};
    ::std::memcpy(bytes.data(), __builtin_addressof(value), sizeof(value));

    float80_bits result{};
    for(::std::size_t index{}; index != sizeof(::std::uint_least64_t); ++index)
    {
        result.mantissa |= static_cast<::std::uint_least64_t>(bytes[index]) << (index * 8u);
    }
    result.sign_exponent = static_cast<::std::uint_least16_t>(bytes[8]) |
                           static_cast<::std::uint_least16_t>(static_cast<::std::uint_least16_t>(bytes[9]) << 8u);
    return result;
}

[[nodiscard]] __float80 make_float80_bits(::std::uint_least64_t mantissa, ::std::uint_least16_t exponent,
                                          bool sign) noexcept
{
    ::std::array<unsigned char, sizeof(__float80)> bytes{};
    auto const sign_exponent{static_cast<::std::uint_least16_t>((sign ? 0x8000u : 0u) | (exponent & 0x7fffu))};
    for(::std::size_t index{}; index != sizeof(::std::uint_least64_t); ++index)
    {
        bytes[index] = static_cast<unsigned char>(mantissa >> (index * 8u));
    }
    bytes[8] = static_cast<unsigned char>(sign_exponent);
    bytes[9] = static_cast<unsigned char>(sign_exponent >> 8u);

    __float80 value{};
    ::std::memcpy(__builtin_addressof(value), bytes.data(), sizeof(value));
    return value;
}

[[nodiscard]] __float80 canonical_float80_from_bytes(::std::uint8_t const* data, ::std::size_t size) noexcept
{
    constexpr ::std::uint_least64_t integer_bit{::std::uint_least64_t{1} << 63u};
    constexpr ::std::uint_least64_t quiet_bit{::std::uint_least64_t{1} << 62u};
    auto mantissa{load_u64(data, size, 0)};
    auto exponent{static_cast<::std::uint_least16_t>(load_u64(data, size, 8) & 0x7fffu)};
    auto const sign{(load_u64(data, size, 10) & 1u) != 0u};

    if(exponent == 0u)
    {
        mantissa &= ~integer_bit;
    }
    else if(exponent == 0x7fffu)
    {
        mantissa |= integer_bit;
        if((load_u64(data, size, 11) & 1u) != 0u) { mantissa |= quiet_bit; }
    }
    else
    {
        mantissa |= integer_bit;
    }

    return make_float80_bits(mantissa, exponent, sign);
}

[[nodiscard]] bool float80_is_nan(__float80 value) noexcept
{
    auto const bits{get_float80_bits(value)};
    return (bits.sign_exponent & 0x7fffu) == 0x7fffu &&
           (bits.mantissa & ~(::std::uint_least64_t{1} << 63u)) != 0u;
}

[[nodiscard]] bool float80_is_zero(__float80 value) noexcept
{
    auto const bits{get_float80_bits(value)};
    return (bits.sign_exponent & 0x7fffu) == 0u && bits.mantissa == 0u;
}

[[nodiscard]] bool float80_sign(__float80 value) noexcept
{
    return (get_float80_bits(value).sign_exponent & 0x8000u) != 0u;
}

[[nodiscard]] bool same_float80(__float80 lhs, __float80 rhs) noexcept
{
    if(float80_is_nan(lhs) && float80_is_nan(rhs)) { return true; }
    if(float80_is_zero(lhs) && float80_is_zero(rhs)) { return float80_sign(lhs) == float80_sign(rhs); }
    auto const lhs_bits{get_float80_bits(lhs)};
    auto const rhs_bits{get_float80_bits(rhs)};
    return lhs_bits.mantissa == rhs_bits.mantissa && lhs_bits.sign_exponent == rhs_bits.sign_exponent;
}

[[nodiscard]] bool host_long_double_is_x87() noexcept
{
    return ::std::numeric_limits<long double>::digits == 64 &&
           ::std::numeric_limits<long double>::max_exponent == 16384 && sizeof(long double) >= 10u;
}

[[nodiscard]] float80_bits get_host_long_double_bits(long double value) noexcept
{
    ::std::array<unsigned char, sizeof(long double)> bytes{};
    ::std::memcpy(bytes.data(), __builtin_addressof(value), sizeof(value));
    float80_bits result{};
    for(::std::size_t index{}; index != sizeof(::std::uint_least64_t); ++index)
    {
        result.mantissa |= static_cast<::std::uint_least64_t>(bytes[index]) << (index * 8u);
    }
    result.sign_exponent = static_cast<::std::uint_least16_t>(bytes[8]) |
                           static_cast<::std::uint_least16_t>(static_cast<::std::uint_least16_t>(bytes[9]) << 8u);
    return result;
}

[[nodiscard]] bool strtold_full(::std::string_view text, long double& out)
{
    ::std::string z{text};
    char* end{};
    errno = 0;
    out = ::std::strtold(z.c_str(), &end);
    return end == z.c_str() + z.size() && end != z.c_str();
}

void expect_strtold_float80_oracle(::std::string_view text, __float80 parsed)
{
    if(!host_long_double_is_x87()) { return; }

    long double oracle{};
    if(!strtold_full(text, oracle)) { return; }

    auto const oracle_bits{get_host_long_double_bits(oracle)};
    auto const parsed_bits{get_float80_bits(parsed)};
    if((oracle_bits.sign_exponent & 0x7fffu) == 0x7fffu &&
       (oracle_bits.mantissa & ~(::std::uint_least64_t{1} << 63u)) != 0u && float80_is_nan(parsed))
    {
        return;
    }
    if(oracle_bits.mantissa != parsed_bits.mantissa || oracle_bits.sign_exponent != parsed_bits.sign_exponent)
    {
        fail("strtold float80 oracle compare", text);
    }
}

[[nodiscard]] __float80 float80_zero(bool sign = false) noexcept { return make_float80_bits(0u, 0u, sign); }
[[nodiscard]] __float80 float80_one() noexcept
{
    return make_float80_bits(::std::uint_least64_t{1} << 63u, 0x3fffu, false);
}
[[nodiscard]] __float80 float80_one_point_five() noexcept
{
    return make_float80_bits((::std::uint_least64_t{1} << 63u) | (::std::uint_least64_t{1} << 62u), 0x3fffu,
                             false);
}
[[nodiscard]] __float80 float80_min_subnormal() noexcept { return make_float80_bits(1u, 0u, false); }
[[nodiscard]] __float80 float80_min_normal() noexcept
{
    return make_float80_bits(::std::uint_least64_t{1} << 63u, 1u, false);
}
[[nodiscard]] __float80 float80_max_finite() noexcept
{
    return make_float80_bits(~::std::uint_least64_t{}, 0x7ffeu, false);
}
[[nodiscard]] __float80 float80_infinity(bool sign = false) noexcept
{
    return make_float80_bits(::std::uint_least64_t{1} << 63u, 0x7fffu, sign);
}
[[nodiscard]] __float80 float80_quiet_nan() noexcept
{
    return make_float80_bits((::std::uint_least64_t{1} << 63u) | (::std::uint_least64_t{1} << 62u), 0x7fffu,
                             false);
}

#endif

#if defined(__SIZEOF_INT128__) && (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__))

[[nodiscard]] __uint128_t load_u128(::std::uint8_t const* data, ::std::size_t size) noexcept
{
    __uint128_t raw{};
    for(::std::size_t i{}; i != sizeof(raw) && i < size; ++i)
    {
        raw |= static_cast<__uint128_t>(data[i]) << (i * 8u);
    }
    return raw;
}

[[nodiscard]] __float128 float128_from_bytes(::std::uint8_t const* data, ::std::size_t size) noexcept
{
    return ::std::bit_cast<__float128>(load_u128(data, size));
}

[[nodiscard]] __uint128_t float128_bits(__float128 value) noexcept { return ::std::bit_cast<__uint128_t>(value); }

[[nodiscard]] __float128 make_float128(::std::uint64_t high, ::std::uint64_t low) noexcept
{
    auto const raw{(static_cast<__uint128_t>(high) << 64u) | low};
    return ::std::bit_cast<__float128>(raw);
}

[[nodiscard]] bool float128_is_nan(__float128 value) noexcept
{
    auto const raw{float128_bits(value)};
    auto const exponent{static_cast<unsigned>((raw >> 112u) & 0x7fffu)};
    auto const fraction{raw & (((static_cast<__uint128_t>(1) << 112u) - 1u))};
    return exponent == 0x7fffu && fraction != 0u;
}

[[nodiscard]] bool float128_is_zero(__float128 value) noexcept
{
    return (float128_bits(value) & ((static_cast<__uint128_t>(1) << 127u) - 1u)) == 0u;
}

[[nodiscard]] bool float128_sign(__float128 value) noexcept { return (float128_bits(value) >> 127u) != 0u; }

#endif

template <typename T>
[[nodiscard]] bool semantically_same(T lhs, T rhs) noexcept
{
#if defined(__SIZEOF_FLOAT80__)
    if constexpr(::std::same_as<T, __float80>)
    {
        return same_float80(lhs, rhs);
    }
    else
#endif
#if defined(__SIZEOF_INT128__) && (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__))
        if constexpr(::std::same_as<T, __float128>)
    {
        if(float128_is_nan(lhs) && float128_is_nan(rhs)) { return true; }
        if(float128_is_zero(lhs) && float128_is_zero(rhs)) { return float128_sign(lhs) == float128_sign(rhs); }
        return lhs == rhs;
    }
    else
#endif
    {
        if(::std::isnan(lhs) && ::std::isnan(rhs)) { return true; }
        if(lhs == T{} && rhs == T{}) { return ::std::signbit(lhs) == ::std::signbit(rhs); }
        return lhs == rhs;
    }
}

template <typename T, typename MakeManip>
void expect_parse_semantic(::std::string_view context, ::std::string_view text, T expected, MakeManip make_manip)
{
    T parsed{};
    if(!parse_full(text, parsed, make_manip)) { fail(context, text); }
    if(!semantically_same(expected, parsed)) { fail(context, text); }
}

template <typename T>
void expect_hex_output_roundtrip(T value)
{
    {
        auto const text{fast_io_text(::fast_io::mnp::hexfloat(value))};
        expect_parse_semantic("hexfloat_get output roundtrip", text, value,
                              [](T& out) { return ::fast_io::mnp::hexfloat_get(out); });
    }
    {
        auto const text{fast_io_text(::fast_io::mnp::hexfloat0x(value))};
        expect_parse_semantic("hexfloat0x_get output roundtrip", text, value,
                              [](T& out) { return ::fast_io::mnp::hexfloat0x_get(out); });
    }
    {
        auto const text{fast_io_text(::fast_io::mnp::hexfloat<true>(value))};
        expect_parse_semantic("uppercase hexfloat_get output roundtrip", text, value,
                              [](T& out) { return ::fast_io::mnp::hexfloat_get(out); });
    }
    {
        auto const text{fast_io_text(::fast_io::mnp::hexfloat0x<true>(value))};
        expect_parse_semantic("uppercase hexfloat0x_get output roundtrip", text, value,
                              [](T& out) { return ::fast_io::mnp::hexfloat0x_get(out); });
    }
}

template <typename Manip>
void smoke_print(::std::string_view context, Manip manip)
{
    auto const text{fast_io_text(manip)};
    if(text.empty()) { fail(context, "empty output"); }
}

template <typename T>
void run_hex_output_template_matrix(T value)
{
    smoke_print("hexfloat", ::fast_io::mnp::hexfloat(value));
    smoke_print("hexfloat uppercase", ::fast_io::mnp::hexfloat<true>(value));
    smoke_print("hexfloat0x", ::fast_io::mnp::hexfloat0x(value));
    smoke_print("hexfloat0x uppercase", ::fast_io::mnp::hexfloat0x<true>(value));
    smoke_print("comma_hexfloat", ::fast_io::mnp::comma_hexfloat(value));
    smoke_print("comma_hexfloat uppercase", ::fast_io::mnp::comma_hexfloat<true>(value));
    smoke_print("comma_hexfloat0x", ::fast_io::mnp::comma_hexfloat0x(value));
    smoke_print("comma_hexfloat0x uppercase", ::fast_io::mnp::comma_hexfloat0x<true>(value));

    constexpr ::std::size_t precisions[]{0u, 1u, 2u, 8u, 21u, 36u};
    for(auto const precision: precisions)
    {
        smoke_print("hexfloat precision", ::fast_io::mnp::hexfloat(value, precision));
        smoke_print("hexfloat uppercase precision", ::fast_io::mnp::hexfloat<true>(value, precision));
        smoke_print("hexfloat0x precision", ::fast_io::mnp::hexfloat0x(value, precision));
        smoke_print("hexfloat0x uppercase precision", ::fast_io::mnp::hexfloat0x<true>(value, precision));
        smoke_print("comma_hexfloat precision", ::fast_io::mnp::comma_hexfloat(value, precision));
        smoke_print("comma_hexfloat uppercase precision", ::fast_io::mnp::comma_hexfloat<true>(value, precision));
        smoke_print("comma_hexfloat0x precision", ::fast_io::mnp::comma_hexfloat0x(value, precision));
        smoke_print("comma_hexfloat0x uppercase precision",
                    ::fast_io::mnp::comma_hexfloat0x<true>(value, precision));
    }

    expect_hex_output_roundtrip(value);
}

template <typename T, bool noskipws, bool prefix, bool allow_plus>
void run_hexfloat_get_template(T expected)
{
    constexpr ::std::string_view body_prefix{"0x1.8p0"};
    constexpr ::std::string_view body_no_prefix{"1.8p0"};
    constexpr ::std::string_view spaced_prefix{" \t0x1.8p0"};
    constexpr ::std::string_view spaced_no_prefix{" \t1.8p0"};
    constexpr ::std::string_view plus_prefix{"+0x1.8p0"};
    constexpr ::std::string_view plus_no_prefix{"+1.8p0"};

    auto const body{prefix ? body_prefix : body_no_prefix};
    auto const spaced{prefix ? spaced_prefix : spaced_no_prefix};
    auto const plus{prefix ? plus_prefix : plus_no_prefix};

    expect_parse_semantic("hexfloat_get template", noskipws ? body : spaced, expected,
                          [](T& out) { return ::fast_io::mnp::hexfloat_get<noskipws, prefix, allow_plus>(out); });
    if constexpr(allow_plus)
    {
        expect_parse_semantic("hexfloat_get plus template", plus, expected,
                              [](T& out) { return ::fast_io::mnp::hexfloat_get<noskipws, prefix, allow_plus>(out); });
    }
}

template <typename T, bool noskipws, bool allow_plus>
void run_hexfloat0x_get_template(T expected)
{
    constexpr ::std::string_view body{"0x1.8p0"};
    constexpr ::std::string_view spaced{" \t0x1.8p0"};
    constexpr ::std::string_view plus{"+0x1.8p0"};

    expect_parse_semantic("hexfloat0x_get template", noskipws ? body : spaced, expected,
                          [](T& out) { return ::fast_io::mnp::hexfloat0x_get<noskipws, allow_plus>(out); });
    if constexpr(allow_plus)
    {
        expect_parse_semantic("hexfloat0x_get plus template", plus, expected,
                              [](T& out) { return ::fast_io::mnp::hexfloat0x_get<noskipws, allow_plus>(out); });
    }
}

template <typename T>
void run_hex_scan_template_matrix(T expected)
{
    run_hexfloat_get_template<T, false, false, false>(expected);
    run_hexfloat_get_template<T, false, false, true>(expected);
    run_hexfloat_get_template<T, false, true, false>(expected);
    run_hexfloat_get_template<T, false, true, true>(expected);
    run_hexfloat_get_template<T, true, false, false>(expected);
    run_hexfloat_get_template<T, true, false, true>(expected);
    run_hexfloat_get_template<T, true, true, false>(expected);
    run_hexfloat_get_template<T, true, true, true>(expected);
    run_hexfloat0x_get_template<T, false, false>(expected);
    run_hexfloat0x_get_template<T, false, true>(expected);
    run_hexfloat0x_get_template<T, true, false>(expected);
    run_hexfloat0x_get_template<T, true, true>(expected);
}

template <typename T>
void exercise_parsed_token(::std::string_view text, T parsed)
{
    static_cast<void>(text);
#if defined(__SIZEOF_FLOAT80__)
    if constexpr(::std::same_as<T, __float80>) { expect_strtold_float80_oracle(text, parsed); }
#endif
    expect_hex_output_roundtrip(parsed);
}

template <typename T>
void fuzz_parse_token(::std::string_view token)
{
    T parsed{};
    if(parse_full(token, parsed, [](T& out) { return ::fast_io::mnp::hexfloat_get(out); }))
    {
        exercise_parsed_token(token, parsed);
    }

    parsed = {};
    if(parse_full(token, parsed, [](T& out) { return ::fast_io::mnp::hexfloat0x_get(out); }))
    {
        exercise_parsed_token(token, parsed);
    }
}

template <typename T, typename MakeManip>
void expect_required_token(::std::string_view context, ::std::string_view token, MakeManip make_manip)
{
    T parsed{};
    if(!parse_full(token, parsed, make_manip)) { fail(context, token); }
    exercise_parsed_token(token, parsed);
}

template <typename T>
void run_required_token_cases()
{
    constexpr ::std::string_view no_prefix_cases[]{
        "0p+0", "-0p+0", "0.p+0", "0.0p+0", "1p+0", "-1p+0", "1.8p+0", "1.fffffffffffffp+1023",
        "1p-1074", "1p+16383", "inf", "-inf", "nan", "-nan",
    };
    for(auto const text: no_prefix_cases)
    {
        expect_required_token<T>("required hexfloat_get token", text,
                                 [](T& out) { return ::fast_io::mnp::hexfloat_get(out); });
    }

    constexpr ::std::string_view prefixed_cases[]{
        "0x0p+0", "-0x0p+0", "0x0.p+0", "0x0.0p+0", "0x1p+0", "-0x1p+0", "0x1.8p+0",
        "0x1.fffffffffffffp+1023", "0x1p-1074", "0x1p+16383",
    };
    for(auto const text: prefixed_cases)
    {
        expect_required_token<T>("required hexfloat0x_get token", text,
                                 [](T& out) { return ::fast_io::mnp::hexfloat0x_get(out); });
        expect_required_token<T>("required prefixed hexfloat_get token", text,
                                 [](T& out) { return ::fast_io::mnp::hexfloat_get<false, true, false>(out); });
    }

    expect_required_token<T>("required plus hexfloat_get token", "+1.8p0",
                             [](T& out) { return ::fast_io::mnp::hexfloat_get<false, false, true>(out); });
    expect_required_token<T>("required plus hexfloat0x_get token", "+0x1.8p0",
                             [](T& out) { return ::fast_io::mnp::hexfloat0x_get<false, true>(out); });
    expect_required_token<T>("required plus prefixed hexfloat_get token", "+0x1.8p0",
                             [](T& out) { return ::fast_io::mnp::hexfloat_get<false, true, true>(out); });
}

void fuzz_double_projection(double value)
{
#if defined(UWVM2_FAST_IO_FLOAT_EXT_HAS_FAST_FLOAT)
    if(!::std::isnan(value))
    {
        auto const hex_text{fast_io_text(::fast_io::mnp::hexfloat(value))};
        double fast_float_hex{};
        auto const* const first{hex_text.data()};
        auto const* const last{hex_text.data() + hex_text.size()};
        auto const result{::fast_float::from_chars(first, last, fast_float_hex, ::fast_float::chars_format::hex)};
        if(result.ec == ::std::errc{} && result.ptr == last)
        {
            if((value == 0.0 && fast_float_hex == 0.0 && ::std::signbit(value) != ::std::signbit(fast_float_hex)) ||
               (value != 0.0 && value != fast_float_hex))
            {
                fail("fast_float double hex oracle compare", hex_text);
            }
        }
    }
#else
    (void)value;
#endif

#if defined(UWVM2_FAST_IO_FLOAT_EXT_HAS_FAST_FLOAT) && defined(UWVM2_FAST_IO_FLOAT_EXT_HAS_DRAGONBOX)
    if(!::std::isnan(value))
    {
        ::std::array<char, jkj::dragonbox::max_output_string_length<jkj::dragonbox::ieee754_binary64>> buffer{};
        auto* const end{jkj::dragonbox::to_chars(value, buffer.data())};
        ::std::string_view const text{buffer.data(), static_cast<::std::size_t>(end - buffer.data())};
        double parsed{};
        auto const result{::fast_float::from_chars(text.data(), text.data() + text.size(), parsed)};
        if(result.ec != ::std::errc{} || result.ptr != text.data() + text.size())
        {
            fail("fast_float dragonbox double parse", text);
        }
        if((value == 0.0 && parsed == 0.0 && ::std::signbit(value) != ::std::signbit(parsed)) ||
           (value != 0.0 && value != parsed))
        {
            fail("fast_float dragonbox double compare", text);
        }
    }
#endif
}

#if defined(__SIZEOF_FLOAT80__)
void run_float80_fixed_once()
{
    static bool const done{[] {
        run_hex_output_template_matrix(float80_zero(false));
        run_hex_output_template_matrix(float80_zero(true));
        run_hex_output_template_matrix(float80_one());
        run_hex_output_template_matrix(float80_one_point_five());
        run_hex_output_template_matrix(float80_min_subnormal());
        run_hex_output_template_matrix(float80_min_normal());
        run_hex_output_template_matrix(float80_max_finite());
        run_hex_output_template_matrix(float80_infinity(false));
        run_hex_output_template_matrix(float80_infinity(true));
        run_hex_output_template_matrix(float80_quiet_nan());
        run_hex_scan_template_matrix(float80_one_point_five());
        run_required_token_cases<__float80>();

        expect_required_token<__float80>("required float80 max finite token", "0x1.fffffffffffffffep+16383",
                                         [](__float80& out) { return ::fast_io::mnp::hexfloat0x_get(out); });
        expect_required_token<__float80>("required float80 min subnormal token",
                                         "0x0.0000000000000002p-16382",
                                         [](__float80& out) { return ::fast_io::mnp::hexfloat0x_get(out); });
        return true;
    }()};
    (void)done;
}
#endif

#if defined(__SIZEOF_INT128__) && (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__))
void run_float128_fixed_once()
{
    static bool const done{[] {
        run_hex_output_template_matrix(static_cast<__float128>(0.0));
        run_hex_output_template_matrix(-static_cast<__float128>(0.0));
        run_hex_output_template_matrix(static_cast<__float128>(1.0));
        run_hex_output_template_matrix(static_cast<__float128>(1.5));
        run_hex_output_template_matrix(make_float128(0x0000000000000000ull, 0x0000000000000001ull));
        run_hex_output_template_matrix(make_float128(0x7ffeffffffffffffull, 0xffffffffffffffffull));
        run_hex_output_template_matrix(make_float128(0x7fff000000000000ull, 0x0000000000000000ull));
        run_hex_output_template_matrix(make_float128(0xffff000000000000ull, 0x0000000000000000ull));
        run_hex_output_template_matrix(make_float128(0x7fff800000000000ull, 0x0000000000000000ull));
        run_hex_scan_template_matrix(static_cast<__float128>(1.5));
        run_required_token_cases<__float128>();

        expect_required_token<__float128>("required float128 max finite token",
                                          "0x1.ffffffffffffffffffffffffffffp+16383",
                                          [](__float128& out) { return ::fast_io::mnp::hexfloat0x_get(out); });
        expect_required_token<__float128>("required float128 min subnormal token",
                                          "0x0.0000000000000000000000000001p-16382",
                                          [](__float128& out) { return ::fast_io::mnp::hexfloat0x_get(out); });
        return true;
    }()};
    (void)done;
}
#endif

} // namespace

extern "C" int LLVMFuzzerTestOneInput(::std::uint8_t const* data, ::std::size_t size)
{
    auto const token{printable_token(data, size)};

#if defined(__SIZEOF_FLOAT80__)
    run_float80_fixed_once();
    fuzz_parse_token<__float80>(token);
    expect_hex_output_roundtrip(canonical_float80_from_bytes(data, size));
#endif

#if defined(__SIZEOF_INT128__) && (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__))
    run_float128_fixed_once();
    fuzz_parse_token<__float128>(token);
    expect_hex_output_roundtrip(float128_from_bytes(data, size));
#endif

    fuzz_double_projection(double_from_prefix(data, size));

    return 0;
}

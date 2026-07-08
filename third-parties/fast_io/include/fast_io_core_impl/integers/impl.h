#pragma once
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC system_header
#endif

#if defined(FAST_IO_ENABLE_FLOATING_POINT_ENVIRONMENT)
#if __has_include(<cfenv>)
#include <cfenv>
#define FAST_IO_HAS_FLOATING_POINT_ENVIRONMENT 1
#endif
#endif

namespace fast_io
{

namespace details
{

inline constexpr ::std::size_t method_ptr_size{sizeof(::std::size_t) * 2};

inline constexpr ::std::size_t method_ptr_hold_size{::fast_io::details::method_ptr_size % sizeof(::std::size_t) == 0
														? ::fast_io::details::method_ptr_size / sizeof(::std::size_t)
														: ::fast_io::details::method_ptr_size / sizeof(::std::size_t) +
															  1};

struct scalar_manip_detail_tag
{
};

template <typename scalar_type>
concept has_scalar_manip_detail_tag =
	requires(scalar_type) { typename ::std::remove_cvref_t<scalar_type>::scalar_manip_detail_tag; };

} // namespace details

namespace manipulators
{

enum class scalar_placement : char8_t
{
	none,
	left,
	middle,
	right,
	internal
};

enum class floating_format : char8_t
{
	fixed,
	general,
	scientific,
	decimal,
	hexfloat
};

enum class floating_nan_payload_scan : ::std::uint_fast8_t
{
	none,
	consume,
	preserve
};

enum class floating_precision : ::std::uint_least8_t
{
	significant,
	fractional,
	significant_preserve_trailing_zero,
	fractional_preserve_trailing_zero
};

enum class floating_rounding : ::std::uint_least8_t
{
	nearest_to_even,
	nearest_to_odd,
	nearest_toward_plus_infinity,
	nearest_toward_minus_infinity,
	nearest_toward_zero,
	nearest_away_from_zero,
	toward_plus_infinity,
	toward_minus_infinity,
	toward_zero,
	away_from_zero,
	current_environment
};

enum class lc_time_flag : ::std::uint_least8_t
{
	none,
	d_t_fmt,
	d_fmt,
	t_fmt,
	t_fmt_ampm,
	date_fmt,
	era_d_t_fmt,
	era_d_fmt,
	era_t_fmt
};

enum class percentage_flag : ::std::uint_least8_t
{
	none,
	percent,
	sexratio
};

struct ip_flags
{
	bool v6shorten{true};
	bool v6uppercase{};
	bool v6bracket{true};
	bool v6full{};
	bool showport{};
	bool ipv4_mapped_ipv6{true};
};

inline constexpr ip_flags ip_default_flags{.showport = true};
inline constexpr ip_flags ip_default_inaddr_flags{};

template <ip_flags flags, typename T>
struct ip_manip_t
{
	using value_type = T;
	using manip_tag = manip_tag_t;
#if 0
	using scalar_manip_detail_tag = ::fast_io::details::scalar_manip_detail_tag;
#endif
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
};

struct ip_scan_flags
{
	bool allowv6shorten{true};
	bool allowv6uppercase{true};
	bool allowv6bracket{true};
	bool requirev6full{false};
	bool requireport{false};
	bool ipv4_mapped_ipv6{true};
};

inline constexpr ip_scan_flags ip_scan_default_flags{.requireport = true};
inline constexpr ip_scan_flags ip_scan_no_ipv4_mapped_ipv6{.requireport = true, .ipv4_mapped_ipv6 = false};
inline constexpr ip_scan_flags ip_scan_default_inaddr_flags{};

template <ip_scan_flags flags, typename T>
struct ip_scan_manip_t
{
	using value_type = T;
	using manip_tag = manip_tag_t;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
};

struct scalar_flags
{
	// Scan and print: integer radix in the closed range [2, 36]. Floating
	// manipulators keep this at 10 as a dispatch invariant; the actual decimal
	// versus hexadecimal floating syntax is selected by `floating`.
	::std::size_t base{10};
	// Print only: use alphabetic spellings for scalar values that have such a
	// spelling, such as boolalpha-style bools and nullptr. Numeric scan paths do
	// not consult this flag.
	bool alphabet{};
	// Scan and print: print a base prefix for integer/hex-float output, or
	// require and consume that prefix during integer/hex-float input. Decimal
	// integer scan ignores the prefix request because base 10 has no prefix.
	bool showbase{};
	// Print only: emit an explicit '+' for non-negative signed numeric output
	// and for floating special values according to the NaN sign policy. Input
	// uses `allow_leading_plus` instead, matching from_chars-style defaults.
	bool showpos{};
	// Scan only: disable the usual leading whitespace skip. String-like scan
	// also uses this to switch from token parsing to exact-position parsing.
	bool noskipws{};
	// Print only: choose uppercase prefix letters for bases with alphabetic
	// prefixes, for example 0X/0B instead of 0x/0b. Scan accepts valid prefix
	// spellings independently of this output preference.
	bool uppercase_showbase{};
	// Scan and print: select the modern octal prefix grammar for base 8 where
	// the manipulator requests a prefix, such as 0o/0O instead of the legacy 0
	// spelling.
	bool modern_octal{};
	// Print only: use uppercase alphabetic digits and uppercase spellings for
	// affected scalar text, such as A-F, INF/NAN, TRUE/FALSE, and nullptr forms.
	// Scan is case-tolerant where the grammar allows it.
	bool uppercase{};
	// Print only: use an uppercase exponent marker for floating output, such as
	// E for decimal scientific notation and P for hexadecimal floating output.
	// Scan accepts both exponent-marker cases where applicable.
	bool uppercase_e{};
#if 0
	// Print only: reserved exponent-sign policy. If enabled, it would control
	// whether positive floating exponents are printed with an explicit '+'.
	bool showpos_e{true};
#endif
	// Print only: request the comma variant of scalar formatting. For floating
	// and timestamp-like output this changes the fractional separator, and for
	// percentage-style output it selects the comma-aware spelling.
	bool comma{};
	// Scan and print, with manipulator-specific meaning: integer print uses it
	// for full-width/zero-filled forms such as addresses; integer scan reuses
	// the same storage bit as the skip-leading-zero policy.
	bool full{};
	// Print only: JSON-friendly decimal floating spelling. For general/default
	// and fixed decimal output, integer-valued finite results keep a fractional
	// marker such as 0.0 or 10.0; scientific output is not forced this way.
	bool json_float{};
	// Print only: alignment metadata for scalar width formatting. Numeric scan
	// does not use placement; width wrappers consume this kind of policy while
	// composing formatted output.
	scalar_placement placement{scalar_placement::none};
	// Scan and print: select the floating grammar/format family. Print uses it
	// to choose fixed, general, scientific, shortest decimal, or hexfloat
	// output; scan uses it to constrain decimal fixed/scientific forms or to
	// select hexadecimal floating parsing.
	floating_format floating{};
	// Print only: locale time-format selector for timestamp manipulators, such
	// as d_t_fmt, d_fmt, t_fmt, and the era-aware variants. It is not part of
	// numeric scan.
	lc_time_flag time_flag{};
#if 0
	// Scan only: reserved locale-aware parse selector. It is currently disabled
	// and should not be assumed by active scan code.
	bool localeparse{};
#endif
	// Scan only: string-like input mode that stops at a line boundary instead
	// of using the ordinary whitespace-delimited token behavior.
	bool line{};
	// Print only: select percentage or sex-ratio scalar output. Ordinary
	// integer/floating scan does not interpret trailing percent notation through
	// this flag.
	percentage_flag percentage{};
	// Print only: when printing NaNs, preserve and print the NaN sign if the
	// representation carries one. Infinity sign output is governed by the
	// ordinary sign path, not by this NaN-specific switch.
	bool nan_show_sign{true};
	// Print only: append the implementation-specific NaN kind/type suffix, for
	// example forms such as signaling or indeterminate NaNs, when that
	// information is representable by the floating backend.
	bool nan_show_type{};
	// Scan only: when parsing NaN special values, decide whether a consumed
	// leading '-' is reflected into the resulting NaN sign bit. Infinity uses
	// the normal parsed sign regardless of this NaN-specific policy.
	bool nan_parse_sign{true};
	// Scan only: policy for a NaN parenthesized payload/type suffix. `none`
	// rejects or stops before payload handling, `consume` accepts and discards
	// the suffix, and `preserve` maps recognized suffix information into the
	// produced NaN where the target format supports it.
	floating_nan_payload_scan nan_payload_scan{floating_nan_payload_scan::consume};
	// Scan and print for floating values: rounding policy used by precision
	// output and by decimal-to-binary input. The current-environment variant is
	// only meaningful when floating environment support is enabled.
	floating_rounding rounding{};
	// Scan and print for precision-bearing floating manipulators: significant
	// precision means total significant digits; fractional precision means
	// digits after the radix point. Hexfloat precision supports significant
	// hexadecimal digits only.
	floating_precision precision{};
	// Scan only: accept an optional leading '+' before integer, decimal-float,
	// or hex-float input. The default is false to match from_chars-style
	// behavior; exponent signs such as e+1 or p+1 are controlled by the
	// floating grammar and remain separate.
	bool allow_leading_plus{};
};

inline constexpr scalar_flags integral_default_scalar_flags{};
inline constexpr scalar_flags floating_point_default_scalar_flags{.floating = floating_format::decimal};
inline constexpr scalar_flags address_default_scalar_flags{.base = 16, .showbase = true, .full = true};

template <scalar_flags flags, typename T>
struct scalar_manip_t
{
	using value_type = T;
	using scalar_flags_type = scalar_flags;
	using manip_tag = manip_tag_t;
	using scalar_manip_detail_tag = ::fast_io::details::scalar_manip_detail_tag;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
};

struct member_function_pointer_holder_t
{
	using manip_tag = manip_tag_t;
	::fast_io::freestanding::array<::std::size_t, ::fast_io::details::method_ptr_hold_size> reference;
};

template <typename T>
struct whole_get_t
{
	using value_type = T;
	using manip_tag = manip_tag_t;
	value_type reference;
};

} // namespace manipulators

namespace details
{
template <::std::size_t bs, bool upper, bool shbase, bool fll, bool showpos = false, bool comma = false, bool oct_c2y = false,
		  ::fast_io::manipulators::percentage_flag perflag = ::fast_io::manipulators::percentage_flag::none>
inline constexpr ::fast_io::manipulators::scalar_flags base_mani_flags_cache{
	.base = bs,
	.showbase = shbase,
	.showpos = showpos,
	.uppercase_showbase = ((bs == 2 || bs == 3 || (bs == 8 && oct_c2y) || bs == 16) ? upper : false),
	.modern_octal = (bs == 8 ? oct_c2y : false),
	.uppercase = ((bs <= 10) ? false : upper),
	.comma = comma,
	.full = fll,
	.percentage = perflag};

template <bool upper>
inline constexpr ::fast_io::manipulators::scalar_flags boolalpha_mani_flags_cache{.alphabet = true, .uppercase = upper};

template <bool uppercase, bool comma, bool showbase = false>
inline constexpr ::fast_io::manipulators::scalar_flags hexafloat_mani_flags_cache{
	.showbase = showbase,
	.uppercase_showbase = uppercase,
	.uppercase = uppercase,
	.uppercase_e = uppercase,
	.comma = comma,
	.floating = ::fast_io::manipulators::floating_format::hexfloat};

template <bool uppercase, bool comma, ::fast_io::manipulators::floating_format fm>
inline constexpr ::fast_io::manipulators::scalar_flags dcmfloat_mani_flags_cache{
	.uppercase = uppercase, .uppercase_e = uppercase, .comma = comma, .floating = fm};

inline constexpr ::fast_io::manipulators::scalar_flags
set_floating_rounding_flag(::fast_io::manipulators::scalar_flags flags,
						   ::fast_io::manipulators::floating_rounding rounding) noexcept
{
	flags.rounding = rounding;
	return flags;
}

template <::fast_io::manipulators::scalar_flags flags, ::fast_io::manipulators::floating_rounding rounding>
inline constexpr ::fast_io::manipulators::scalar_flags floating_rounding_mani_flags_cache{
	::fast_io::details::set_floating_rounding_flag(flags, rounding)};

inline constexpr ::fast_io::manipulators::scalar_flags
set_floating_precision_flag(::fast_io::manipulators::scalar_flags flags,
							::fast_io::manipulators::floating_precision precision) noexcept
{
	flags.precision = precision;
	return flags;
}

template <::fast_io::manipulators::scalar_flags flags, ::fast_io::manipulators::floating_precision precision>
inline constexpr ::fast_io::manipulators::scalar_flags floating_precision_mani_flags_cache{
	::fast_io::details::set_floating_precision_flag(flags, precision)};

template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::manipulators::floating_precision precision,
		  ::fast_io::manipulators::floating_rounding rounding>
inline constexpr ::fast_io::manipulators::scalar_flags floating_precision_rounding_mani_flags_cache{
	::fast_io::details::set_floating_rounding_flag(
		::fast_io::details::set_floating_precision_flag(flags, precision), rounding)};

template <::fast_io::manipulators::floating_precision precision>
inline constexpr bool floating_precision_is_significant{
	precision == ::fast_io::manipulators::floating_precision::significant ||
	precision == ::fast_io::manipulators::floating_precision::significant_preserve_trailing_zero};

template <::fast_io::manipulators::floating_precision precision>
inline constexpr bool floating_precision_is_fractional{
	precision == ::fast_io::manipulators::floating_precision::fractional ||
	precision == ::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero};

template <::fast_io::manipulators::floating_precision precision>
inline constexpr bool floating_precision_preserves_trailing_zero{
	precision == ::fast_io::manipulators::floating_precision::significant_preserve_trailing_zero ||
	precision == ::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero};

inline constexpr ::fast_io::manipulators::scalar_flags
set_json_float_flag(::fast_io::manipulators::scalar_flags flags, bool json_float) noexcept
{
	flags.json_float = json_float;
	return flags;
}

template <::fast_io::manipulators::scalar_flags flags, bool json_float>
inline constexpr ::fast_io::manipulators::scalar_flags json_float_mani_flags_cache{
	::fast_io::details::set_json_float_flag(flags, json_float)};

inline constexpr ::fast_io::manipulators::scalar_flags
set_allow_leading_plus_flag(::fast_io::manipulators::scalar_flags flags, bool allow_leading_plus) noexcept
{
	flags.allow_leading_plus = allow_leading_plus;
	return flags;
}

template <::fast_io::manipulators::scalar_flags flags, bool allow_leading_plus>
inline constexpr ::fast_io::manipulators::scalar_flags allow_leading_plus_mani_flags_cache{
	::fast_io::details::set_allow_leading_plus_flag(flags, allow_leading_plus)};

template <::fast_io::manipulators::floating_rounding rounding>
inline constexpr bool floating_rounding_is_nearest{
	rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_to_odd ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_plus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_minus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_zero ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_away_from_zero};

inline
#if !defined(FAST_IO_HAS_FLOATING_POINT_ENVIRONMENT)
	constexpr
#endif
	::fast_io::manipulators::floating_rounding current_floating_rounding() noexcept
{
#if defined(FAST_IO_HAS_FLOATING_POINT_ENVIRONMENT)
	switch (::std::fegetround())
	{
#if defined(FE_UPWARD)
	case FE_UPWARD:
		return ::fast_io::manipulators::floating_rounding::toward_plus_infinity;
#endif
#if defined(FE_DOWNWARD)
	case FE_DOWNWARD:
		return ::fast_io::manipulators::floating_rounding::toward_minus_infinity;
#endif
#if defined(FE_TOWARDZERO)
	case FE_TOWARDZERO:
		return ::fast_io::manipulators::floating_rounding::toward_zero;
#endif
	default:
		return ::fast_io::manipulators::floating_rounding::nearest_to_even;
	}
#else
	return ::fast_io::manipulators::floating_rounding::nearest_to_even;
#endif
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool floating_rounding_directed_round_up(bool negative) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::toward_plus_infinity)
	{
		return !negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::toward_minus_infinity)
	{
		return negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::away_from_zero)
	{
		return true;
	}
	else
	{
		return false;
	}
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool floating_rounding_nearest_tie_round_up(bool negative,
																		   ::std::uint_least64_t mantissa) noexcept
{
	auto const rounded_down{mantissa >> 1u};
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		return (rounded_down & 1u) != 0u;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_odd)
	{
		return (rounded_down & 1u) == 0u;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_plus_infinity)
	{
		return !negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_minus_infinity)
	{
		return negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_away_from_zero)
	{
		return true;
	}
	else
	{
		return false;
	}
}

template <bool uppercase, bool shbase>
inline constexpr ::fast_io::manipulators::scalar_flags cryptohash_mani_flags_cache{
	.base = 16, .showbase = shbase, .uppercase_showbase = uppercase, .uppercase = uppercase};

template <::std::size_t bs, bool noskipws, bool shbase, bool skipzero, bool oct_c2y,
		  bool allow_leading_plus = false>
inline constexpr ::fast_io::manipulators::scalar_flags base_scan_mani_flags_cache{
	.base = bs,
	.showbase = shbase,
	.noskipws = noskipws,
	.modern_octal = oct_c2y,
	.full = skipzero,
	.allow_leading_plus = allow_leading_plus};

template <bool shport>
inline constexpr ::fast_io::manipulators::ip_flags base_ip_prefix_flags_cache{.showport = shport};

template <typename inttype>
struct unsigned_integer_alias_type_traits_helper
{
	using alias_type = ::std::conditional_t<
		(sizeof(inttype) == sizeof(::std::uint_least8_t)), ::std::uint_least8_t,
		::std::conditional_t<
			(sizeof(inttype) == sizeof(::std::uint_least16_t)), ::std::uint_least16_t,
			::std::conditional_t<(sizeof(inttype) == sizeof(::std::uint_least32_t)), ::std::uint_least32_t,
								 ::std::conditional_t<(sizeof(inttype) == sizeof(::std::uint_least64_t)),
													  ::std::uint_least64_t, inttype>>>>;
};

template <typename inttype>
struct signed_integer_alias_type_traits_helper
{
	using alias_type = ::std::conditional_t<
		(sizeof(inttype) == sizeof(::std::int_least8_t)), ::std::int_least8_t,
		::std::conditional_t<
			(sizeof(inttype) == sizeof(::std::int_least16_t)), ::std::int_least16_t,
			::std::conditional_t<(sizeof(inttype) == sizeof(::std::int_least32_t)), ::std::int_least32_t,
								 ::std::conditional_t<(sizeof(inttype) == sizeof(::std::int_least64_t)),
													  ::std::int_least64_t, inttype>>>>;
};

template <typename inttype>
struct integer_alias_type_traits
{
	using alias_type = ::std::conditional_t<
		my_unsigned_integral<inttype>,
		::std::conditional_t<(sizeof(inttype) < sizeof(unsigned)),
							 typename unsigned_integer_alias_type_traits_helper<unsigned>::alias_type,
							 typename unsigned_integer_alias_type_traits_helper<inttype>::alias_type>,
		::std::conditional_t<(sizeof(inttype) < sizeof(int)),
							 typename signed_integer_alias_type_traits_helper<int>::alias_type,
							 typename signed_integer_alias_type_traits_helper<inttype>::alias_type>>;
};

template <typename inttype>
using integer_alias_type = typename integer_alias_type_traits<::std::remove_cvref_t<inttype>>::alias_type;

using uintptr_alias_type = ::fast_io::details::integer_alias_type<::std::size_t>;

template <typename inttype>
struct integer_full_alias_type_traits
{
	using alias_type = ::std::conditional_t<my_unsigned_integral<inttype>,
											typename unsigned_integer_alias_type_traits_helper<inttype>::alias_type,
											typename signed_integer_alias_type_traits_helper<inttype>::alias_type>;
};

template <typename inttype>
using integer_full_alias_type = typename integer_full_alias_type_traits<::std::remove_cvref_t<inttype>>::alias_type;

using uintptr_full_alias_type = ::fast_io::details::integer_full_alias_type<::std::size_t>;

template <typename flt>
struct float_alias_type_traits
{
	using alias_type = flt;
};

#ifdef __STDCPP_FLOAT64_T__
template <>
struct float_alias_type_traits<double>
{
	using alias_type =
		::std::conditional_t<sizeof(_Float64) == sizeof(double) && ::std::numeric_limits<double>::is_iec559, _Float64,
							 double>;
};
#endif

#ifdef __STDCPP_FLOAT32_T__
template <>
struct float_alias_type_traits<float>
{
	using alias_type =
		::std::conditional_t<sizeof(_Float32) == sizeof(float) && ::std::numeric_limits<float>::is_iec559, _Float32,
							 float>;
};
#endif

inline constexpr bool long_double_alias_is_double{
	sizeof(long double) == sizeof(double) &&
	::std::numeric_limits<long double>::digits == ::std::numeric_limits<double>::digits &&
	::std::numeric_limits<long double>::max_exponent == ::std::numeric_limits<double>::max_exponent};

#ifdef __SIZEOF_FLOAT80__
inline constexpr bool long_double_alias_is_float80{
	sizeof(long double) == sizeof(__float80) && ::std::numeric_limits<long double>::digits == 64 &&
	::std::numeric_limits<long double>::max_exponent == 16384};
#else
inline constexpr bool long_double_alias_is_float80{};
#endif

#if defined(__SIZEOF_INT128__) && defined(__STDCPP_FLOAT128_T__)
inline constexpr bool long_double_alias_is_float128{
	sizeof(long double) == sizeof(_Float128) && ::std::numeric_limits<long double>::digits == 113 &&
	::std::numeric_limits<long double>::max_exponent == 16384};
#elif defined(__SIZEOF_INT128__) && defined(__FLOAT128__)
inline constexpr bool long_double_alias_is_float128{
	sizeof(long double) == sizeof(__float128) && ::std::numeric_limits<long double>::digits == 113 &&
	::std::numeric_limits<long double>::max_exponent == 16384};
#else
inline constexpr bool long_double_alias_is_float128{};
#endif

template <bool is_double, bool is_float80, bool is_float128>
struct long_double_alias_type_traits
{
	using alias_type = typename float_alias_type_traits<double>::alias_type;
};

#ifdef __SIZEOF_FLOAT80__
template <bool is_float128>
struct long_double_alias_type_traits<false, true, is_float128>
{
	using alias_type = __float80;
};
#endif

#if defined(__SIZEOF_INT128__) && defined(__STDCPP_FLOAT128_T__)
template <>
struct long_double_alias_type_traits<false, false, true>
{
	using alias_type = _Float128;
};
#elif defined(__SIZEOF_INT128__) && defined(__FLOAT128__)
template <>
struct long_double_alias_type_traits<false, false, true>
{
	using alias_type = __float128;
};
#endif

template <>
struct float_alias_type_traits<long double>
{
	using alias_type =
		typename long_double_alias_type_traits<long_double_alias_is_double, long_double_alias_is_float80,
											   long_double_alias_is_float128>::alias_type;
};

#if (defined(__SIZEOF_FLOAT16__) || defined(__FLOAT16__)) && defined(__STDCPP_FLOAT16_T__)
template <>
struct float_alias_type_traits<__float16>
{
	using alias_type = _Float16;
};
#endif

template <typename flt>
using float_alias_type = typename float_alias_type_traits<::std::remove_cvref_t<flt>>::alias_type;

inline constexpr ::fast_io::manipulators::scalar_flags
compute_bool_scalar_flags_cache(::fast_io::manipulators::scalar_flags flags) noexcept
{
	flags.uppercase = false;
	if (!flags.showbase)
	{
		flags.base = 2;
		flags.uppercase_showbase = false;
	}
	return flags;
}

template <::fast_io::manipulators::scalar_flags cache, typename scalar_type>
inline constexpr auto scalar_flags_int_cache(scalar_type t) noexcept
{
	using scalar_type_nocvref = ::std::remove_cvref_t<scalar_type>;
	if constexpr (cache.full)
	{
		if constexpr (::std::same_as<scalar_type_nocvref, ::std::nullptr_t>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, ::std::nullptr_t>{};
		}
		else if constexpr (::std::same_as<scalar_type_nocvref, bool>)
		{
			return ::fast_io::manipulators::scalar_manip_t<compute_bool_scalar_flags_cache(cache), bool>{t};
		}
		else if constexpr (::std::same_as<scalar_type_nocvref, ::std::byte>)
		{
			using alias_type = integer_full_alias_type<char unsigned>;
			return ::fast_io::manipulators::scalar_manip_t<cache, alias_type>{static_cast<alias_type>(t)};
		}
		else if constexpr (::fast_io::details::my_integral<scalar_type_nocvref>)
		{
			using alias_type = integer_full_alias_type<scalar_type_nocvref>;
			return ::fast_io::manipulators::scalar_manip_t<cache, alias_type>{static_cast<alias_type>(t)};
		}
		else if constexpr (::std::is_pointer_v<scalar_type_nocvref>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, uintptr_full_alias_type>{
				static_cast<uintptr_full_alias_type>(::std::bit_cast<::std::size_t>(t))};
		}
		else if constexpr (::std::same_as<::fast_io::manipulators::member_function_pointer_holder_t,
										  scalar_type_nocvref>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache,
														   ::fast_io::manipulators::member_function_pointer_holder_t>{
				t};
		}
		else if constexpr (::fast_io::details::has_scalar_manip_detail_tag<scalar_type>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, typename scalar_type_nocvref::value_type>{
				t.reference};
		}
		else
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, uintptr_full_alias_type>{
				static_cast<uintptr_full_alias_type>(::std::bit_cast<::std::size_t>(::std::to_address(t)))};
		}
	}
	else
	{
		if constexpr (::std::same_as<scalar_type_nocvref, ::std::nullptr_t>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, ::std::nullptr_t>{};
		}
		else if constexpr (::std::same_as<scalar_type_nocvref, bool>)
		{
			return ::fast_io::manipulators::scalar_manip_t<compute_bool_scalar_flags_cache(cache), bool>{t};
		}
		else if constexpr (::std::same_as<scalar_type_nocvref, ::std::byte>)
		{
			using alias_type = integer_alias_type<char unsigned>;
			return ::fast_io::manipulators::scalar_manip_t<cache, alias_type>{static_cast<alias_type>(t)};
		}
		else if constexpr (::fast_io::details::my_integral<scalar_type_nocvref>)
		{
			using alias_type = integer_alias_type<scalar_type_nocvref>;
			return ::fast_io::manipulators::scalar_manip_t<cache, alias_type>{static_cast<alias_type>(t)};
		}
		else if constexpr (::std::same_as<::fast_io::manipulators::member_function_pointer_holder_t,
										  scalar_type_nocvref>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache,
														   ::fast_io::manipulators::member_function_pointer_holder_t>{
				t};
		}
		else if constexpr (::std::is_pointer_v<scalar_type_nocvref>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, uintptr_alias_type>{
				static_cast<uintptr_alias_type>(::std::bit_cast<::std::size_t>(t))};
		}
		else if constexpr (::fast_io::details::has_scalar_manip_detail_tag<scalar_type>)
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, typename scalar_type_nocvref::value_type>{
				t.reference};
		}
		else
		{
			return ::fast_io::manipulators::scalar_manip_t<cache, uintptr_alias_type>{
				static_cast<uintptr_alias_type>(::std::bit_cast<::std::size_t>(::std::to_address(t)))};
		}
	}
}

template <typename scalar_type>
concept scalar_integrals = ::fast_io::details::non_character_integral<scalar_type> ||
						   ::std::same_as<::std::nullptr_t, ::std::remove_cvref_t<scalar_type>> ||
						   ::std::same_as<::std::byte, ::std::remove_cvref_t<scalar_type>> ||
						   ::std::same_as<bool, ::std::remove_cvref_t<scalar_type>> ||
						   ::fast_io::details::has_scalar_manip_detail_tag<scalar_type>;

} // namespace details

namespace manipulators
{

template <scalar_flags flags, typename T>
struct scalar_manip_precision_t
{
	using scalar_flags_type = scalar_flags;
	using manip_tag = manip_tag_t;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
	::std::size_t precision;
};

template <floating_rounding rounding_policy, scalar_flags flags, typename T>
inline constexpr auto rounding(scalar_manip_t<flags, T> value) noexcept
{
	return scalar_manip_t<::fast_io::details::floating_rounding_mani_flags_cache<flags, rounding_policy>, T>{
		value.reference};
}

template <floating_rounding rounding_policy, scalar_flags flags, typename T>
inline constexpr auto rounding(scalar_manip_precision_t<flags, T> value) noexcept
{
	return scalar_manip_precision_t<::fast_io::details::floating_rounding_mani_flags_cache<flags, rounding_policy>, T>{
		value.reference, value.precision};
}

template <bool enabled = true, scalar_flags flags, typename T>
	requires(flags.base == 10 && flags.floating != floating_format::hexfloat)
inline constexpr auto json_float(scalar_manip_t<flags, T> value) noexcept
{
	return scalar_manip_t<::fast_io::details::json_float_mani_flags_cache<flags, enabled>, T>{value.reference};
}

template <bool enabled = true, scalar_flags flags, typename T>
	requires(flags.base == 10 && flags.floating != floating_format::hexfloat)
inline constexpr auto json_float(scalar_manip_precision_t<flags, T> value) noexcept
{
	return scalar_manip_precision_t<::fast_io::details::json_float_mani_flags_cache<flags, enabled>, T>{
		value.reference, value.precision};
}

template <floating_rounding rounding_policy, bool allow_leading_plus = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<::std::remove_cvref_t<scalar_type>>)
inline constexpr auto rounding_get(scalar_type &value) noexcept
{
	return scalar_manip_t<::fast_io::details::allow_leading_plus_mani_flags_cache<
							  ::fast_io::details::floating_rounding_mani_flags_cache<
								  ::fast_io::manipulators::floating_point_default_scalar_flags, rounding_policy>,
							  allow_leading_plus>,
						  scalar_type &>{value};
}

template <scalar_placement flags, typename T>
struct width_t
{
	using manip_tag = manip_tag_t;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
	::std::size_t width;
};

template <scalar_placement flags, typename T, ::std::integral ch_type>
struct width_ch_t
{
	using manip_tag = manip_tag_t;
	using char_type = ch_type;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
	::std::size_t width;
	char_type ch;
};

template <typename T>
struct width_runtime_t
{
	using manip_tag = manip_tag_t;
	scalar_placement placement;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
	::std::size_t width;
};

template <typename T, ::std::integral ch_type>
struct width_runtime_ch_t
{
	using manip_tag = manip_tag_t;
	using char_type = ch_type;
	scalar_placement placement;
#ifndef __INTELLISENSE__
#if __has_cpp_attribute(msvc::no_unique_address)
	[[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
	[[no_unique_address]]
#endif
#endif
	T reference;
	::std::size_t width;
	char_type ch;
};

template <::std::size_t bs, bool shbase = false, bool full = false, bool oct_c2y = false, typename scalar_type>
	requires((2 <= bs && bs <= 36) && (::fast_io::details::scalar_integrals<scalar_type>))
inline constexpr auto base(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<bs, false, shbase, full, false, false, oct_c2y>>(t);
}

template <::std::size_t bs, bool shbase = false, bool full = false, bool oct_c2y = false, typename scalar_type>
	requires((2 <= bs && bs <= 36) && (::fast_io::details::character_integral<scalar_type>))
inline constexpr auto base(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<bs, false, shbase, full, false, false, oct_c2y>>(t);
}

template <::std::size_t bs, bool shbase = false, bool full = false, bool oct_c2y = false, typename scalar_type>
	requires((2 <= bs && bs <= 36) && (::fast_io::details::scalar_integrals<scalar_type>))
inline constexpr auto baseupper(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<bs, true, shbase, full, false, false, oct_c2y>>(t);
}

template <::std::size_t bs, bool shbase = false, bool full = false, bool oct_c2y = false, typename scalar_type>
	requires((2 <= bs && bs <= 36) && (::fast_io::details::character_integral<scalar_type>))
inline constexpr auto baseupper(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<bs, true, shbase, full, false, false, oct_c2y>>(t);
}

template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto hex(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, false, shbase, full>>(t);
}

template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto hex(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, false, shbase, full>>(t);
}

template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto hexupper(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, true, shbase, full>>(t);
}

template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto hexupper(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, true, shbase, full>>(t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto hex0x(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<16, false, true, full>>(
		t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto hex0x(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<16, false, true, full>>(
		t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto hex0xupper(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<16, true, true, full>>(
		t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto hex0xupper(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<16, true, true, full>>(
		t);
}

template <bool shbase = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto uhexfull(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, false, shbase, true>>(
		static_cast<::fast_io::details::my_make_unsigned_t<::std::remove_cvref_t<scalar_type>>>(t));
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto addrvw(scalar_type t) noexcept
{
	if constexpr (::fast_io::details::my_signed_integral<scalar_type>)
	{
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(
			static_cast<::fast_io::details::my_make_unsigned_t<::std::remove_cvref_t<scalar_type>>>(t));
	}
	else
	{
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(t);
	}
}

template <bool uppercase = false, typename scalar_type>
	requires((::std::is_pointer_v<scalar_type> || ::std::contiguous_iterator<scalar_type>) &&
			 (!::std::is_function_v<::std::remove_cvref_t<scalar_type>>))
inline constexpr auto pointervw(scalar_type t) noexcept
{
	// Object pointers and contiguous iterators: print the address value, not pointee text.
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(t);
}

template <bool uppercase = false, typename scalar_type>
	requires(::std::is_function_v<scalar_type>)
inline constexpr auto funcvw(scalar_type *t) noexcept
{
	// Function pointer entry addresses are kept separate from object/iterator addresses.
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(::std::bit_cast<::std::size_t>(t));
}

template <bool uppercase = false, typename scalar_type>
	requires(::std::is_member_object_pointer_v<scalar_type>)
inline constexpr auto fieldptrvw(scalar_type t) noexcept
{
	// Pointer-to-member fields are ABI encodings, not object addresses.
	if constexpr (sizeof(t) == sizeof(::fast_io::manipulators::member_function_pointer_holder_t))
	{
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(
			::std::bit_cast<::fast_io::manipulators::member_function_pointer_holder_t>(t));
	}
	else
	{
		using equivalentsizetype = ::std::conditional_t<
			sizeof(scalar_type) == sizeof(::std::size_t), ::std::size_t,
			::std::conditional_t<sizeof(scalar_type) == sizeof(::std::uint_least64_t),
								 ::std::uint_least64_t,
								 ::std::conditional_t<sizeof(scalar_type) == sizeof(::std::uint_least32_t),
													  ::std::uint_least32_t,
													  ::std::conditional_t<sizeof(scalar_type) == sizeof(::std::uint_least16_t),
																		   ::std::uint_least16_t,
																		   ::std::uint_least8_t>>>>;
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(::std::bit_cast<equivalentsizetype>(t));
	}
}

template <bool uppercase = false, typename scalar_type>
	requires(::std::is_member_function_pointer_v<scalar_type> && (sizeof(scalar_type) % sizeof(::std::size_t) == 0))
inline constexpr auto methodvw(scalar_type t) noexcept
{
	// Pointer-to-member functions may be multi-word ABI encodings.
	if constexpr (sizeof(scalar_type) == sizeof(::std::size_t))
	{
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(
			::std::bit_cast<::std::size_t>(t));
	}
	else
	{
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(
			::std::bit_cast<::fast_io::manipulators::member_function_pointer_holder_t>(t));
	}
}

template <bool uppercase = false, typename scalar_type>
	requires((::std::is_pointer_v<scalar_type> && !::std::is_function_v<::std::remove_cvref_t<scalar_type>>) ||
			 ::std::integral<scalar_type>)
inline constexpr auto handlevw(scalar_type t) noexcept
{
	if constexpr (::std::integral<scalar_type>)
	{
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<10, false, false, false, false>>(t);
	}
	else
	{
		return ::fast_io::details::scalar_flags_int_cache<
			::fast_io::details::base_mani_flags_cache<16, uppercase, true, true, false>>(t);
	}
}

template <typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto dfvw(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, false, true, true, true>>(t);
}

template <bool shbase = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto uhexupperfull(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<16, true, shbase, true>>(
		static_cast<::fast_io::details::my_make_unsigned_t<::std::remove_cvref_t<scalar_type>>>(t));
}

// Character code units are deliberately separated from the default numeric
// print path even though they satisfy ::std::integral. Use chvw for a character
// view, and dec when the numeric value of a code unit is intended.
template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto dec(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<10, false, shbase, full>>(t);
}

template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto dec(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<10, false, shbase, full>>(t);
}

template <bool shbase = false, bool full = false, bool oct_c2y = false, bool uppercase_showbase = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto oct(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<8, uppercase_showbase, shbase, full, false, false, oct_c2y>>(t);
}

template <bool shbase = false, bool full = false, bool oct_c2y = false, bool uppercase_showbase = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto oct(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<8, uppercase_showbase, shbase, full, false, false, oct_c2y>>(t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto oct0(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<8, false, true, full>>(
		t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto oct0(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<8, false, true, full>>(
		t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto oct0o(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<8, false, true, full, false, false, true>>(t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto oct0o(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<8, false, true, full, false, false, true>>(t);
}

template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto bin(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<2, false, shbase, full>>(t);
}

template <bool shbase = false, bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto bin(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<
		::fast_io::details::base_mani_flags_cache<2, false, shbase, full>>(t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::scalar_integrals<scalar_type>)
inline constexpr auto bin0b(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<2, false, true, full>>(
		t);
}

template <bool full = false, typename scalar_type>
	requires(::fast_io::details::character_integral<scalar_type>)
inline constexpr auto bin0b(scalar_type t) noexcept
{
	return ::fast_io::details::scalar_flags_int_cache<::fast_io::details::base_mani_flags_cache<2, false, true, full>>(
		t);
}

template <scalar_flags flags, typename scalar_type>
	requires(((2 <= flags.base && flags.base <= 36 && (::fast_io::details::scalar_integrals<scalar_type>)) ||
			  (flags.base == 10 && ::fast_io::details::my_floating_point<scalar_type>)))
inline constexpr auto scalar_generic(scalar_type t) noexcept
{
	if constexpr (::fast_io::details::my_floating_point<scalar_type>)
	{
		using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
		return scalar_manip_t<floating_point_default_scalar_flags, float_alias_type>{static_cast<float_alias_type>(t)};
	}
	else
	{
		return ::fast_io::details::scalar_flags_int_cache<flags>(t);
	}
}

template <bool upper = false>
inline constexpr scalar_manip_t<::fast_io::details::boolalpha_mani_flags_cache<upper>, bool> boolalpha(bool b) noexcept
{
	return {b};
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto hexfloat(scalar_type t) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, false>, float_alias_type>{
		static_cast<float_alias_type>(t)};
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto hexfloat0x(scalar_type t) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, false, true>, float_alias_type>{
		static_cast<float_alias_type>(t)};
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto hexfloat(scalar_type t, ::std::size_t n) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_precision_t<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::hexafloat_mani_flags_cache<uppercase, false>, precision_mode, rounding_policy>,
		float_alias_type>{
		static_cast<float_alias_type>(t), n};
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto hexfloat(scalar_type t, ::std::size_t n) noexcept
{
	return hexfloat<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto hexfloat0x(scalar_type t, ::std::size_t n) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_precision_t<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::hexafloat_mani_flags_cache<uppercase, false, true>, precision_mode, rounding_policy>,
		float_alias_type>{
		static_cast<float_alias_type>(t), n};
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto hexfloat0x(scalar_type t, ::std::size_t n) noexcept
{
	return hexfloat0x<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_hexfloat(scalar_type t) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, true>, float_alias_type>{
		static_cast<float_alias_type>(t)};
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_hexfloat0x(scalar_type t) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_t<::fast_io::details::hexafloat_mani_flags_cache<uppercase, true, true>, float_alias_type>{
		static_cast<float_alias_type>(t)};
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_hexfloat(scalar_type t, ::std::size_t n) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_precision_t<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::hexafloat_mani_flags_cache<uppercase, true>, precision_mode, rounding_policy>,
		float_alias_type>{
		static_cast<float_alias_type>(t), n};
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_hexfloat(scalar_type t, ::std::size_t n) noexcept
{
	return comma_hexfloat<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_hexfloat0x(scalar_type t, ::std::size_t n) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_precision_t<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::hexafloat_mani_flags_cache<uppercase, true, true>, precision_mode, rounding_policy>,
		float_alias_type>{
		static_cast<float_alias_type>(t), n};
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_hexfloat0x(scalar_type t, ::std::size_t n) noexcept
{
	return comma_hexfloat0x<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto decimal(scalar_type t) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_t<
		::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::decimal>,
		float_alias_type>{static_cast<float_alias_type>(t)};
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_decimal(scalar_type t) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_t<
		::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::decimal>,
		float_alias_type>{static_cast<float_alias_type>(t)};
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto decimal(scalar_type t, ::std::size_t n) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_precision_t<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::decimal>,
			precision_mode, rounding_policy>,
		float_alias_type>{static_cast<float_alias_type>(t), n};
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto decimal(scalar_type t, ::std::size_t n) noexcept
{
	return decimal<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_decimal(scalar_type t, ::std::size_t n) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_precision_t<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::decimal>,
			precision_mode, rounding_policy>,
		float_alias_type>{static_cast<float_alias_type>(t), n};
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_decimal(scalar_type t, ::std::size_t n) noexcept
{
	return comma_decimal<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto general(scalar_type t) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_t<
		::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::general>,
		float_alias_type>{static_cast<float_alias_type>(t)};
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_general(scalar_type t) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_t<
		::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::general>,
		float_alias_type>{static_cast<float_alias_type>(t)};
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto general(scalar_type t, ::std::size_t n) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_precision_t<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::general>,
			precision_mode, rounding_policy>,
		float_alias_type>{static_cast<float_alias_type>(t), n};
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto general(scalar_type t, ::std::size_t n) noexcept
{
	return general<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_general(scalar_type t, ::std::size_t n) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_precision_t<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::general>,
			precision_mode, rounding_policy>,
		float_alias_type>{static_cast<float_alias_type>(t), n};
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_general(scalar_type t, ::std::size_t n) noexcept
{
	return comma_general<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto fixed(scalar_type t) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_t<
		::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::fixed>,
		float_alias_type>{static_cast<float_alias_type>(t)};
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_fixed(scalar_type t) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_t<
		::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::fixed>,
		float_alias_type>{static_cast<float_alias_type>(t)};
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto fixed(scalar_type t, ::std::size_t n) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_precision_t<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::fixed>,
			precision_mode, rounding_policy>,
		float_alias_type>{static_cast<float_alias_type>(t), n};
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto fixed(scalar_type t, ::std::size_t n) noexcept
{
	return fixed<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_fixed(scalar_type t, ::std::size_t n) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_precision_t<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::fixed>,
			precision_mode, rounding_policy>,
		float_alias_type>{static_cast<float_alias_type>(t), n};
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_fixed(scalar_type t, ::std::size_t n) noexcept
{
	return comma_fixed<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto scientific(scalar_type t) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_t<
		::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::scientific>,
		float_alias_type>{static_cast<float_alias_type>(t)};
}

template <bool uppercase = false, typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_scientific(scalar_type t) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_t<
		::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::scientific>,
		float_alias_type>{static_cast<float_alias_type>(t)};
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto scientific(scalar_type t, ::std::size_t n) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_precision_t<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, false, manipulators::floating_format::scientific>,
			precision_mode, rounding_policy>,
		float_alias_type>{static_cast<float_alias_type>(t), n};
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto scientific(scalar_type t, ::std::size_t n) noexcept
{
	return scientific<uppercase, precision_mode, rounding_policy>(t, n);
}

template <bool uppercase = false,
		  floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_scientific(scalar_type t, ::std::size_t n) noexcept
{
	using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
	return scalar_manip_precision_t<
		::fast_io::details::floating_precision_rounding_mani_flags_cache<
			::fast_io::details::dcmfloat_mani_flags_cache<uppercase, true, manipulators::floating_format::scientific>,
			precision_mode, rounding_policy>,
		float_alias_type>{static_cast<float_alias_type>(t), n};
}

template <floating_precision precision_mode,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool uppercase = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<scalar_type>)
inline constexpr auto comma_scientific(scalar_type t, ::std::size_t n) noexcept
{
	return comma_scientific<uppercase, precision_mode, rounding_policy>(t, n);
}

template <floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool allow_leading_plus = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<::std::remove_cvref_t<scalar_type>>)
inline constexpr auto decimal_get(scalar_type &value) noexcept
{
	return scalar_manip_t<::fast_io::details::allow_leading_plus_mani_flags_cache<
							  ::fast_io::details::floating_precision_rounding_mani_flags_cache<
								  ::fast_io::manipulators::floating_point_default_scalar_flags,
								  floating_precision::significant, rounding_policy>,
							  allow_leading_plus>,
						  scalar_type &>{value};
}

template <floating_precision precision_mode = floating_precision::significant,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool allow_leading_plus = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<::std::remove_cvref_t<scalar_type>>)
inline constexpr auto decimal_get(scalar_type &value, ::std::size_t n) noexcept
{
	return scalar_manip_precision_t<::fast_io::details::allow_leading_plus_mani_flags_cache<
										::fast_io::details::floating_precision_rounding_mani_flags_cache<
											::fast_io::manipulators::floating_point_default_scalar_flags,
											precision_mode, rounding_policy>,
										allow_leading_plus>,
									scalar_type &>{value, n};
}

template <floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool allow_leading_plus = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<::std::remove_cvref_t<scalar_type>>)
inline constexpr auto fixed_get(scalar_type &value) noexcept
{
	return scalar_manip_t<::fast_io::details::allow_leading_plus_mani_flags_cache<
							  ::fast_io::details::floating_precision_rounding_mani_flags_cache<
								  ::fast_io::details::dcmfloat_mani_flags_cache<false, false, manipulators::floating_format::fixed>,
								  floating_precision::fractional, rounding_policy>,
							  allow_leading_plus>,
						  scalar_type &>{value};
}

template <floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool allow_leading_plus = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<::std::remove_cvref_t<scalar_type>>)
inline constexpr auto fixed_get(scalar_type &value, ::std::size_t n) noexcept
{
	return scalar_manip_precision_t<::fast_io::details::allow_leading_plus_mani_flags_cache<
										::fast_io::details::floating_precision_rounding_mani_flags_cache<
											::fast_io::details::dcmfloat_mani_flags_cache<false, false, manipulators::floating_format::fixed>,
											precision_mode, rounding_policy>,
										allow_leading_plus>,
									scalar_type &>{value, n};
}

template <floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool allow_leading_plus = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<::std::remove_cvref_t<scalar_type>>)
inline constexpr auto scientific_get(scalar_type &value) noexcept
{
	return scalar_manip_t<::fast_io::details::allow_leading_plus_mani_flags_cache<
							  ::fast_io::details::floating_precision_rounding_mani_flags_cache<
								  ::fast_io::details::dcmfloat_mani_flags_cache<false, false, manipulators::floating_format::scientific>,
								  floating_precision::fractional, rounding_policy>,
							  allow_leading_plus>,
						  scalar_type &>{value};
}

template <floating_precision precision_mode = floating_precision::fractional,
		  floating_rounding rounding_policy = floating_rounding::nearest_to_even,
		  bool allow_leading_plus = false,
		  typename scalar_type>
	requires(::fast_io::details::my_floating_point<::std::remove_cvref_t<scalar_type>>)
inline constexpr auto scientific_get(scalar_type &value, ::std::size_t n) noexcept
{
	return scalar_manip_precision_t<::fast_io::details::allow_leading_plus_mani_flags_cache<
										::fast_io::details::floating_precision_rounding_mani_flags_cache<
											::fast_io::details::dcmfloat_mani_flags_cache<false, false, manipulators::floating_format::scientific>,
											precision_mode, rounding_policy>,
										allow_leading_plus>,
									scalar_type &>{value, n};
}

template <::std::integral inttype>
inline constexpr ::std::remove_cvref_t<inttype> bitfieldvw(inttype v) noexcept
{
	// Copy bit-fields into an ordinary integer value before formatting.
	return v;
}

} // namespace manipulators

namespace details
{

template <::std::integral char_type, ::std::size_t base>
inline constexpr auto generate_base_prefix_array() noexcept
{
	static_assert(2 <= base && base <= 36);
	if constexpr (base < 10)
	{
		// 0[9]0000
		return ::fast_io::freestanding::array<char_type, 4>{
			char_literal_v<u8'0', char_type>, char_literal_v<u8'[', char_type>,
			::fast_io::char_literal_add<char_type>(base), char_literal_v<u8']', char_type>};
	}
	else
	{
		constexpr char8_t decade{static_cast<char8_t>(static_cast<char8_t>(base) / static_cast<char8_t>(10u))},
			unit{static_cast<char8_t>(static_cast<char8_t>(base) % static_cast<char8_t>(10u))};
		return ::fast_io::freestanding::array<char_type, 5>{
			char_literal_v<u8'0', char_type>, char_literal_v<u8'[', char_type>,
			::fast_io::char_literal_add<char_type>(decade), ::fast_io::char_literal_add<char_type>(unit),
			char_literal_v<u8']', char_type>};
	}
}

template <::std::integral char_type, ::std::size_t base>
inline constexpr auto base_prefix_array{generate_base_prefix_array<char_type, base>()};

template <::std::size_t base, bool uppercase_showbase, bool oct_c2y, ::std::integral char_type>
inline constexpr char_type *print_reserve_show_base_impl(char_type *iter)
{
	static_assert(2 <= base && base <= 36);
	if constexpr (base == 2)
	{
		if constexpr (uppercase_showbase)
		{
			if constexpr (::std::same_as<char_type, char>)
			{
				iter = copy_string_literal("0B", iter);
			}
			else if constexpr (::std::same_as<char_type, wchar_t>)
			{
				iter = copy_string_literal(L"0B", iter);
			}
			else if constexpr (::std::same_as<char_type, char16_t>)
			{
				iter = copy_string_literal(u"0B", iter);
			}
			else if constexpr (::std::same_as<char_type, char32_t>)
			{
				iter = copy_string_literal(U"0B", iter);
			}
			else
			{
				iter = copy_string_literal(u8"0B", iter);
			}
		}
		else
		{
			if constexpr (::std::same_as<char_type, char>)
			{
				iter = copy_string_literal("0b", iter);
			}
			else if constexpr (::std::same_as<char_type, wchar_t>)
			{
				iter = copy_string_literal(L"0b", iter);
			}
			else if constexpr (::std::same_as<char_type, char16_t>)
			{
				iter = copy_string_literal(u"0b", iter);
			}
			else if constexpr (::std::same_as<char_type, char32_t>)
			{
				iter = copy_string_literal(U"0b", iter);
			}
			else
			{
				iter = copy_string_literal(u8"0b", iter);
			}
		}
	}
	else if constexpr (base == 3)
	{
		if constexpr (uppercase_showbase)
		{
			if constexpr (::std::same_as<char_type, char>)
			{
				iter = copy_string_literal("0T", iter);
			}
			else if constexpr (::std::same_as<char_type, wchar_t>)
			{
				iter = copy_string_literal(L"0T", iter);
			}
			else if constexpr (::std::same_as<char_type, char16_t>)
			{
				iter = copy_string_literal(u"0T", iter);
			}
			else if constexpr (::std::same_as<char_type, char32_t>)
			{
				iter = copy_string_literal(U"0T", iter);
			}
			else
			{
				iter = copy_string_literal(u8"0T", iter);
			}
		}
		else
		{
			if constexpr (::std::same_as<char_type, char>)
			{
				iter = copy_string_literal("0t", iter);
			}
			else if constexpr (::std::same_as<char_type, wchar_t>)
			{
				iter = copy_string_literal(L"0t", iter);
			}
			else if constexpr (::std::same_as<char_type, char16_t>)
			{
				iter = copy_string_literal(u"0t", iter);
			}
			else if constexpr (::std::same_as<char_type, char32_t>)
			{
				iter = copy_string_literal(U"0t", iter);
			}
			else
			{
				iter = copy_string_literal(u8"0t", iter);
			}
		}
	}
	else if constexpr (base == 8)
	{
		if constexpr (oct_c2y)
		{
			if constexpr (uppercase_showbase)
			{
				if constexpr (::std::same_as<char_type, char>)
				{
					iter = copy_string_literal("0O", iter);
				}
				else if constexpr (::std::same_as<char_type, wchar_t>)
				{
					iter = copy_string_literal(L"0O", iter);
				}
				else if constexpr (::std::same_as<char_type, char16_t>)
				{
					iter = copy_string_literal(u"0O", iter);
				}
				else if constexpr (::std::same_as<char_type, char32_t>)
				{
					iter = copy_string_literal(U"0O", iter);
				}
				else
				{
					iter = copy_string_literal(u8"0O", iter);
				}
			}
			else
			{
				if constexpr (::std::same_as<char_type, char>)
				{
					iter = copy_string_literal("0o", iter);
				}
				else if constexpr (::std::same_as<char_type, wchar_t>)
				{
					iter = copy_string_literal(L"0o", iter);
				}
				else if constexpr (::std::same_as<char_type, char16_t>)
				{
					iter = copy_string_literal(u"0o", iter);
				}
				else if constexpr (::std::same_as<char_type, char32_t>)
				{
					iter = copy_string_literal(U"0o", iter);
				}
				else
				{
					iter = copy_string_literal(u8"0o", iter);
				}
			}
		}
		else
		{
			*iter = char_literal_v<u8'0', char_type>;
			++iter;
		}
	}
	else if constexpr (base == 16)
	{
		if constexpr (uppercase_showbase)
		{
			if constexpr (::std::same_as<char_type, char>)
			{
				iter = copy_string_literal("0X", iter);
			}
			else if constexpr (::std::same_as<char_type, wchar_t>)
			{
				iter = copy_string_literal(L"0X", iter);
			}
			else if constexpr (::std::same_as<char_type, char16_t>)
			{
				iter = copy_string_literal(u"0X", iter);
			}
			else if constexpr (::std::same_as<char_type, char32_t>)
			{
				iter = copy_string_literal(U"0X", iter);
			}
			else
			{
				iter = copy_string_literal(u8"0X", iter);
			}
		}
		else
		{
			if constexpr (::std::same_as<char_type, char>)
			{
				iter = copy_string_literal("0x", iter);
			}
			else if constexpr (::std::same_as<char_type, wchar_t>)
			{
				iter = copy_string_literal(L"0x", iter);
			}
			else if constexpr (::std::same_as<char_type, char16_t>)
			{
				iter = copy_string_literal(u"0x", iter);
			}
			else if constexpr (::std::same_as<char_type, char32_t>)
			{
				iter = copy_string_literal(U"0x", iter);
			}
			else
			{
				iter = copy_string_literal(u8"0x", iter);
			}
		}
	}
	else if constexpr (base != 10)
	{
		constexpr auto arr{base_prefix_array<char_type, base>};
		constexpr ::std::size_t sz{arr.size()};
		iter = non_overlapped_copy_n(arr.data(), sz, iter);
	}
	return iter;
}

template <::std::size_t base, bool uppercase, bool ryu_mode = false, ::std::integral char_type, typename T>
inline constexpr void print_reserve_integral_main_impl(char_type *iter, T t, ::std::size_t len)
{
	if constexpr (base <= 10 && uppercase)
	{
		print_reserve_integral_main_impl<base, false, ryu_mode>(iter, t, len); // prevent duplications
	}
	else if constexpr (need_seperate_print<T>)
	{
		constexpr ::std::size_t basetdigits{::fast_io::details::cal_max_int_size<T, base>()};
		constexpr ::std::size_t sizetdigits{::fast_io::details::cal_max_int_size<optimal_print_unsigned_type, base>()};
		static_assert(basetdigits != 0 && sizetdigits != 0);
		if constexpr (base == 2 || base == 4 || base == 16)
		{
			optimal_print_unsigned_type high;
			optimal_print_unsigned_type low{
				::fast_io::details::intrinsics::unpack_generic<T, optimal_print_unsigned_type>(t, high)};
			if (len > sizetdigits)
			{
				print_reserve_integral_main_impl<base, false, false>(iter - sizetdigits, high, len - sizetdigits);
				len = sizetdigits;
			}
			print_reserve_integral_main_impl<base, false, false>(iter, low, len);
		}
		else
		{
			constexpr ::std::size_t sizetdigitsm1{sizetdigits - 1};
			constexpr ::std::size_t remain_digits{basetdigits - sizetdigitsm1 * 2};
			constexpr T maxhighdigits{compile_pow10<T, (sizetdigitsm1 * 2)>};
			if constexpr (!ryu_mode && remain_digits != 0)
			{
				static_assert(remain_digits < 3);
				if constexpr (remain_digits == 1)
				{
					if (len == basetdigits)
					{
						T high{t / maxhighdigits};
						t = t % maxhighdigits;
						*(iter - basetdigits) = ::fast_io::char_literal_add<char_type>(high);
						--len;
					}
				}
				else
				{
					constexpr ::std::size_t least_digits{basetdigits - remain_digits};
					if (len > least_digits)
					{
						T high{t / maxhighdigits};
						t = t % maxhighdigits;
						::std::uint_least8_t rem{static_cast<::std::uint_least8_t>(high)};
						if (len == basetdigits)
						{
							constexpr auto tb{::fast_io::details::digits_table<char_type, base, uppercase>};
							non_overlapped_copy_n(tb + (rem << 1), 2, iter - basetdigits);
							len -= 2u;
						}
						else
						{
							*(iter + 1 - basetdigits) = ::fast_io::char_literal_add<char_type>(high);
							--len;
						}
					}
				}
			}
			optimal_print_unsigned_type low;
			if (len > sizetdigitsm1)
			{
				constexpr T halfdigits{compile_pow10<T, sizetdigitsm1>};
				optimal_print_unsigned_type high{static_cast<optimal_print_unsigned_type>(t / halfdigits)};
				low = static_cast<optimal_print_unsigned_type>(t % halfdigits);
				print_reserve_integral_main_impl<base, false, false>(iter - sizetdigitsm1, high, len - sizetdigitsm1);
				len = sizetdigitsm1;
			}
			else
			{
				low = static_cast<optimal_print_unsigned_type>(t);
			}
			print_reserve_integral_main_impl<base, false, false>(iter, low, len);
		}
	}
	else
	{
		if constexpr (ryu_mode)
		{
			print_reserve_integral_main_impl<base, uppercase, false>(iter, t, len);
		}
		else
		{
			constexpr auto tb{::fast_io::details::digits_table<char_type, base, uppercase>};
			constexpr T pw{static_cast<T>(base * base)};
			::std::size_t const len2{len >> static_cast<::std::size_t>(1u)};
			for (::std::size_t i{}; i != len2; ++i)
			{
				auto const rem{t % pw};
				t /= pw;
				non_overlapped_copy_n(tb + (rem << 1), 2, iter -= 2);
			}
			if ((len & 1))
			{
				if constexpr (base <= 10)
				{
					*--iter = ::fast_io::char_literal_add<char_type>(t);
				}
				else
				{
					*--iter = static_cast<char_type>(tb[(t << 1) + 1]);
				}
			}
		}
	}
}

template <bool full, ::std::size_t base, bool uppercase, ::std::integral char_type, my_unsigned_integral T>
inline constexpr char_type *print_reserve_integral_withfull_main_impl(char_type *first, T u)
{
	if constexpr (base <= 10 && uppercase)
	{
		return print_reserve_integral_withfull_main_impl<full, base, false>(first, u); // prevent duplications
	}
	else
	{
		if constexpr (full)
		{
			constexpr ::std::size_t sz{::fast_io::details::cal_max_int_size<T, base>()};
			if constexpr (sizeof(u) <= sizeof(unsigned))
			{
				print_reserve_integral_main_impl<base, uppercase>(first += sz, static_cast<unsigned>(u), sz);
			}
			else
			{
				print_reserve_integral_main_impl<base, uppercase>(first += sz, u, sz);
			}
			return first;
		}
		else
		{
			if constexpr (base == 10 && (::std::numeric_limits<::std::uint_least32_t>::digits == 32u))
			{
				if constexpr (false)
				{
					return ::fast_io::details::uprsv::uprsv_main<base, uppercase>(first, u);
				}
				else
				{
					return ::fast_io::details::jeaiii::jeaiii_main(first, u);
				}
			}
			else
			{
				::std::size_t const sz{chars_len<base, false>(u)};
				if constexpr (sizeof(u) <= sizeof(unsigned))
				{
					print_reserve_integral_main_impl<base, uppercase>(first += sz, static_cast<unsigned>(u), sz);
				}
				else
				{
					print_reserve_integral_main_impl<base, uppercase>(first += sz, u, sz);
				}
				return first;
			}
		}
	}
}

template <::std::size_t base, bool uppercase, ::std::integral char_type, my_unsigned_integral T>
inline constexpr void print_reserve_integral_withfull_precise_main_impl(char_type *last, T u, ::std::size_t n)
{

	if constexpr (sizeof(u) <= sizeof(unsigned) && sizeof(unsigned) <= sizeof(::std::size_t))
	{
		print_reserve_integral_main_impl<base, uppercase>(last, static_cast<unsigned>(u), n);
	}
	else
	{
		print_reserve_integral_main_impl<base, uppercase>(last, u, n);
	}
}

template <::std::size_t base, bool showbase = false, bool uppercase_showbase = false, bool showpos = false,
		  bool uppercase = false, bool full = false, bool oct_c2y = false, typename int_type, ::std::integral char_type>
inline constexpr char_type *print_reserve_integral_define(char_type *first, int_type t)
{
	if constexpr (base <= 10 && uppercase)
	{
		return print_reserve_integral_define<base, showbase, uppercase_showbase, showpos, false, full, oct_c2y>(
			first, t); // prevent duplications
	}
	else
	{
		static_assert((2 <= base) && (base <= 36));
		if constexpr (::std::same_as<bool, ::std::remove_cvref_t<int_type>>)
		{
			if constexpr (showpos)
			{
				*first = char_literal_v<u8'+', char_type>;
				++first;
			}
			if constexpr (showbase && (base != 10))
			{
				first = print_reserve_show_base_impl<base, uppercase_showbase, oct_c2y>(first);
			}
			*first = t ? char_literal_v<u8'1', char_type> : char_literal_v<u8'0', char_type>;
			++first;
			return first;
		}
		else
		{
			using unsigned_type = ::fast_io::details::my_make_unsigned_t<int_type>;
			unsigned_type u{static_cast<unsigned_type>(t)};
			if constexpr (showpos)
			{
				if constexpr (::fast_io::details::my_unsigned_integral<int_type>)
				{
					*first = char_literal_v<u8'+', char_type>;
				}
				else
				{
					if (t < 0)
					{
						*first = char_literal_v<u8'-', char_type>;
						constexpr unsigned_type zero{};
						u = zero - u;
					}
					else
					{
						*first = char_literal_v<u8'+', char_type>;
					}
				}
				++first;
			}
			else
			{
				if constexpr (::fast_io::details::my_signed_integral<int_type>)
				{
					if (t < 0)
					{
						*first = char_literal_v<u8'-', char_type>;
						++first;
						constexpr unsigned_type zero{};
						u = zero - u;
					}
				}
			}
			if constexpr (showbase && (base != 10))
			{
				first = print_reserve_show_base_impl<base, uppercase_showbase, oct_c2y>(first);
			}
			return print_reserve_integral_withfull_main_impl<full, base, uppercase>(first, u);
		}
	}
}

template <::std::size_t base, bool showbase = false, bool uppercase_showbase = false, bool showpos = false,
		  bool uppercase = false, bool oct_c2y = false, typename int_type, ::std::integral char_type>
inline constexpr void print_reserve_integral_define_precise(char_type *start, ::std::size_t n, int_type t)
{
	if constexpr (base <= 10 && uppercase)
	{
		return print_reserve_integral_define_precise<base, showbase, uppercase_showbase, showpos, false, oct_c2y>(
			start, n, t); // prevent duplications
	}
	else
	{
		auto first{start};
		static_assert((2 <= base) && (base <= 36));
		if constexpr (::std::same_as<bool, ::std::remove_cvref_t<int_type>>)
		{
			if constexpr (showpos)
			{
				*first = char_literal_v<u8'+', char_type>;
				++first;
			}
			if constexpr (showbase && (base != 10))
			{
				first = print_reserve_show_base_impl<base, uppercase_showbase, oct_c2y>(first);
			}
			*first = t ? char_literal_v<u8'1', char_type> : char_literal_v<u8'0', char_type>;
			++first;
			return first;
		}
		else
		{
			using unsigned_type = ::fast_io::details::my_make_unsigned_t<int_type>;
			unsigned_type u{static_cast<unsigned_type>(t)};
			if constexpr (showpos)
			{
				if constexpr (::fast_io::details::my_unsigned_integral<int_type>)
				{
					*first = char_literal_v<u8'+', char_type>;
				}
				else
				{
					if (t < 0)
					{
						*first = char_literal_v<u8'-', char_type>;
						constexpr unsigned_type zero{};
						u = zero - u;
					}
					else
					{
						*first = char_literal_v<u8'+', char_type>;
					}
				}
				++first;
			}
			else
			{
				if constexpr (::fast_io::details::my_signed_integral<int_type>)
				{
					if (t < 0)
					{
						*first = char_literal_v<u8'-', char_type>;
						++first;
						constexpr unsigned_type zero{};
						u = zero - u;
					}
				}
			}
			if constexpr (showbase && (base != 10))
			{
				first = print_reserve_show_base_impl<base, uppercase_showbase, oct_c2y>(first);
			}
			if constexpr (base == 10 && (::std::numeric_limits<::std::uint_least32_t>::digits == 32u))
			{
				return ::fast_io::details::jeaiii::jeaiii_main_len(
					first, u, static_cast<::std::uint_least32_t>((start + n) - first));
			}
			else
			{
				auto ed{start + n};
				if constexpr (my_unsigned_integral<int_type> && !showbase && !showpos)
				{
					print_reserve_integral_withfull_precise_main_impl<base, uppercase>(ed, u, n);
				}
				else
				{
					print_reserve_integral_withfull_precise_main_impl<base, uppercase>(
						ed, u, static_cast<::std::size_t>(ed - first));
				}
			}
		}
	}
}

template <::std::size_t base, bool oct_c2y>
inline constexpr ::std::size_t compute_print_showbase_length() noexcept
{
	if (base == 2 || base == 3 || base == 16)
	{
		return 2; // 0b 0t 0x
	}
	else if (base == 8)
	{
		return oct_c2y ? 2 : 1; // oct_c2y: 0o, default: 0
	}
	else if (base < 10)
	{
		return 4; // 0[9]
	}
	else if (10 < base)
	{
		return 5; // 0[36]
	}
	return 0;
}

template <::std::size_t base, bool oct_c2y>
inline constexpr ::std::size_t print_showbase_length{compute_print_showbase_length<base, oct_c2y>()};

template <::std::size_t base = 10, bool showbase = false, bool showpos = false, bool oct_c2y = false, my_integral T>
inline constexpr ::std::size_t print_reserve_scalar_size_impl()
{
	::std::size_t total_sum{};
	if constexpr (showpos)
	{
		++total_sum;
	}
	if constexpr (showbase)
	{
		total_sum += ::fast_io::details::print_showbase_length<base, oct_c2y>;
	}
	if constexpr (::std::same_as<::std::remove_cvref_t<T>, bool>)
	{
		++total_sum;
	}
	else
	{
		if constexpr (!showpos)
		{
			if constexpr (my_signed_integral<T>)
			{
				++total_sum;
			}
		}
		total_sum += ::fast_io::details::cal_max_int_size<T, base>();
	}
	return total_sum;
}

template <::std::size_t base, bool showbase, bool showpos, bool oct_c2y, my_integral T>
inline constexpr ::std::size_t print_integer_reserved_size_cache{
	print_reserve_scalar_size_impl<base, showbase, showpos, oct_c2y, T>()};

template <::std::size_t base, bool showbase, bool showpos, bool full, bool oct_c2y, my_integral T>
inline constexpr ::std::size_t print_reserve_scalar_cal_precise_cache_size_impl()
{
	::std::size_t total_sum{};
	if constexpr (showpos)
	{
		++total_sum;
	}
	if constexpr (showbase)
	{
		constexpr auto curr_length{::fast_io::details::print_showbase_length<base, oct_c2y>};
		total_sum += curr_length;
		// base==10 does not have showbase
		if constexpr (full)
		{
			if constexpr (::std::same_as<::std::remove_cvref_t<T>, bool>)
			{
				++total_sum;
			}
			else
			{
				if constexpr (!showpos)
				{
					if constexpr (my_signed_integral<T>)
					{
						++total_sum;
					}
				}
				total_sum += ::fast_io::details::cal_max_int_size<T, base>();
			}
		}
	}
	return total_sum;
}

template <::std::size_t base, bool showbase, bool showpos, bool full, bool oct_c2y, my_integral T>
inline constexpr ::std::size_t print_integer_reserved_precise_size_cache{
	print_reserve_scalar_cal_precise_cache_size_impl<base, showbase, showpos, full, oct_c2y, T>()};

template <::std::size_t base, bool showbase, bool showpos, bool full, bool oct_c2y, my_integral T>
inline constexpr ::std::size_t print_integer_reserved_precise_size(T t)
{
	if constexpr (::std::same_as<::std::remove_cvref_t<T>, bool>)
	{
		return print_integer_reserved_size_cache<base, showbase, showpos, oct_c2y, T>;
	}
	else if constexpr (full)
	{
		if constexpr (!showpos && my_signed_integral<T>)
		{
			::std::size_t total_sum{print_integer_reserved_precise_size_cache<base, showbase, showpos, full, oct_c2y, T>};
			if (t < 0)
			{
				++total_sum;
			}
			return total_sum;
		}
		else
		{
			return print_integer_reserved_precise_size_cache<base, showbase, showpos, full, oct_c2y, T>;
		}
	}
	else
	{
		::std::size_t total_sum{print_integer_reserved_precise_size_cache<base, showbase, showpos, false, oct_c2y, T>};
		if constexpr (my_signed_integral<T>)
		{
			using unsigned_type = my_make_unsigned_t<T>;
			unsigned_type u{static_cast<unsigned_type>(t)};
			if (t < 0)
			{
				++total_sum;
				constexpr unsigned_type zero{};
				u = static_cast<unsigned_type>(zero - u);
			}
			return total_sum + chars_len<base, false>(u);
		}
		else
		{
			return total_sum + chars_len<base, false>(t);
		}
	}
}

template <::std::integral char_type, ::std::size_t base, bool showbase, bool uppercase_showbase, bool showpos,
		  bool uppercase, bool full, bool oct_c2y>
inline constexpr ::std::size_t nullptr_print_optimization_call_size_impl() noexcept
{
	::fast_io::freestanding::array<char_type,
								   print_integer_reserved_size_cache<base, showbase, showpos, oct_c2y, ::std::size_t>>
		arr;
	auto res{print_reserve_integral_define<base, showbase, uppercase_showbase, showpos, uppercase, full, oct_c2y>(
		arr.data(), ::std::size_t{})};
	return static_cast<::std::size_t>(res - arr.data());
}

template <::std::integral char_type, ::std::size_t base, bool showbase, bool uppercase_showbase, bool showpos,
		  bool uppercase, bool full, bool oct_c2y>
inline constexpr ::std::size_t nullptr_print_optimization_call_size_cache{
	nullptr_print_optimization_call_size_impl<char_type, base, showbase, uppercase_showbase, showpos, uppercase,
											  full, oct_c2y>()};

template <::std::integral char_type, ::std::size_t base, bool showbase, bool uppercase_showbase, bool showpos,
		  bool uppercase, bool full, bool oct_c2y>
inline constexpr auto nullptr_print_optimization_call_impl() noexcept
{
	constexpr ::std::size_t sz{nullptr_print_optimization_call_size_cache<char_type, base, showbase, uppercase_showbase,
																		  showpos, uppercase, full, oct_c2y>};
	::fast_io::freestanding::array<char_type, sz> arr{};
	[[maybe_unused]] auto res{print_reserve_integral_define<base, showbase, uppercase_showbase, showpos, uppercase, full, oct_c2y>(
		arr.data(), ::std::size_t{})};
	return arr;
}

template <::std::integral char_type, ::std::size_t base, bool showbase, bool uppercase_showbase, bool showpos,
		  bool uppercase, bool full, bool oct_c2y>
inline constexpr auto nullptr_print_optimization_call_cache{
	nullptr_print_optimization_call_impl<char_type, base, showbase, uppercase_showbase, showpos, uppercase, full, oct_c2y>()};

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *print_reserve_boolalpha_impl(char_type *iter, bool b)
{
	if (b)
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal("TRUE", iter);
			}
			else
			{
				return copy_string_literal("true", iter);
			}
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(L"TRUE", iter);
			}
			else
			{
				return copy_string_literal(L"true", iter);
			}
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(u"TRUE", iter);
			}
			else
			{
				return copy_string_literal(u"true", iter);
			}
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(U"TRUE", iter);
			}
			else
			{
				return copy_string_literal(U"true", iter);
			}
		}
		else
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(u8"TRUE", iter);
			}
			else
			{
				return copy_string_literal(u8"true", iter);
			}
		}
	}
	else
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal("FALSE", iter);
			}
			else
			{
				return copy_string_literal("false", iter);
			}
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(L"FALSE", iter);
			}
			else
			{
				return copy_string_literal(L"false", iter);
			}
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(u"FALSE", iter);
			}
			else
			{
				return copy_string_literal(u"false", iter);
			}
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(U"FALSE", iter);
			}
			else
			{
				return copy_string_literal(U"false", iter);
			}
		}
		else
		{
			if constexpr (uppercase)
			{
				return copy_string_literal(u8"FALSE", iter);
			}
			else
			{
				return copy_string_literal(u8"false", iter);
			}
		}
	}
}

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *print_reserve_nullptr_alphabet_impl(char_type *iter)
{
	if constexpr (::std::same_as<char_type, char>)
	{
		if constexpr (uppercase)
		{
			return copy_string_literal("NULLPTR", iter);
		}
		else
		{
			return copy_string_literal("nullptr", iter);
		}
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		if constexpr (uppercase)
		{
			return copy_string_literal(L"NULLPTR", iter);
		}
		else
		{
			return copy_string_literal(L"nullptr", iter);
		}
	}
	else if constexpr (::std::same_as<char_type, char16_t>)
	{
		if constexpr (uppercase)
		{
			return copy_string_literal(u"NULLPTR", iter);
		}
		else
		{
			return copy_string_literal(u"nullptr", iter);
		}
	}
	else if constexpr (::std::same_as<char_type, char32_t>)
	{
		if constexpr (uppercase)
		{
			return copy_string_literal(U"NULLPTR", iter);
		}
		else
		{
			return copy_string_literal(U"nullptr", iter);
		}
	}
	else
	{
		if constexpr (uppercase)
		{
			return copy_string_literal(u8"NULLPTR", iter);
		}
		else
		{
			return copy_string_literal(u8"nullptr", iter);
		}
	}
}

template <::std::size_t base, bool showbase = false, bool uppercase_showbase = false, bool showpos = false,
		  bool uppercase = false, bool full = false, bool oct_c2y = false, ::std::integral char_type>
inline constexpr char_type *print_reserve_method_impl(char_type *iter,
													  ::fast_io::manipulators::member_function_pointer_holder_t mfph)
{
	if constexpr (base <= 10 && uppercase)
	{
		return print_reserve_method_impl<base, showbase, uppercase_showbase, showpos, false, full, oct_c2y>(
			iter, mfph); // prevent duplications
	}
	else if constexpr (::fast_io::details::method_ptr_hold_size == 0)
	{
		return iter;
	}
	else
	{
		iter = details::print_reserve_integral_define<base, showbase, uppercase_showbase, showpos, uppercase, full, oct_c2y>(
			iter, mfph.reference.front());
		using myssizet = ::std::make_signed_t<::std::size_t>;
		if constexpr (::fast_io::details::method_ptr_hold_size == 2)
		{
			::std::size_t backptr{mfph.reference.back()};
			return details::print_reserve_integral_define<base, showbase, uppercase_showbase, true, uppercase, false, oct_c2y>(
				iter, static_cast<myssizet>(backptr));
		}
		else
		{

			::std::size_t last{::fast_io::details::method_ptr_hold_size};
			for (::std::size_t i{1}; i != last; ++i)
			{
				iter =
					details::print_reserve_integral_define<base, showbase, uppercase_showbase, true, uppercase, false, oct_c2y>(
						iter, static_cast<myssizet>(mfph.reference[i]));
			}
			return iter;
		}
	}
}

} // namespace details

template <typename scalar_type>
	requires(details::non_character_integral<scalar_type> || ::fast_io::details::my_floating_point<scalar_type> ||
			 ::std::same_as<::std::nullptr_t, ::std::remove_cvref_t<scalar_type>>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr auto print_alias_define(io_alias_t, scalar_type t) noexcept
{
	if constexpr (details::non_character_integral<scalar_type>)
	{
		using int_alias_type = ::fast_io::details::integer_alias_type<scalar_type>;
		return manipulators::scalar_manip_t<manipulators::integral_default_scalar_flags, int_alias_type>{
			static_cast<int_alias_type>(t)};
	}
	else if constexpr (details::my_floating_point<scalar_type>)
	{
		using float_alias_type = ::fast_io::details::float_alias_type<scalar_type>;
		return manipulators::scalar_manip_t<manipulators::floating_point_default_scalar_flags, float_alias_type>{
			static_cast<float_alias_type>(t)};
	}
	else
	{
		return manipulators::scalar_manip_t<manipulators::scalar_flags{.alphabet = true}, ::std::nullptr_t>{};
	}
}

template <::std::integral char_type, typename T>
	requires(details::non_character_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<char_type, T>) noexcept
{
	if constexpr (::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
	{
		return details::print_integer_reserved_size_cache<10, false, false, false, char8_t>;
	}
	else
	{
		return details::print_integer_reserved_size_cache<10, false, false, false, ::std::remove_cvref_t<T>>;
	}
}

template <::std::integral char_type, typename T>
	requires(details::non_character_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]] // always inline to reduce inline depth in GCC and LLVM clang
#endif
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, T>, char_type *iter, T t) noexcept
{
	if constexpr (::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
	{
		return details::print_reserve_integral_define<10, false, false, false, false, false>(iter,
																							 static_cast<char8_t>(t));
	}
	else
	{
		return details::print_reserve_integral_define<10, false, false, false, false, false>(iter, t);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags, typename T>
	requires(details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte> ||
			 ::std::same_as<::std::remove_cvref_t<T>, ::std::nullptr_t> ||
			 ::std::same_as<::std::remove_cv_t<T>, ::fast_io::manipulators::member_function_pointer_holder_t>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>) noexcept
{
	if constexpr (flags.alphabet)
	{
		static_assert((::std::same_as<::std::remove_cvref_t<T>, bool> ||
					   ::std::same_as<::std::remove_cvref_t<T>, ::std::nullptr_t>),
					  "only bool and ::std::nullptr_t support alphabet output");
		if constexpr (::std::same_as<::std::remove_cvref_t<T>, bool>)
		{
			return 5; // u8"false"
		}
		else
		{
			return 7; // u8"nullptr"
		}
	}
	else if constexpr (::std::same_as<::std::remove_cv_t<T>, ::fast_io::manipulators::member_function_pointer_holder_t>)
	{
		constexpr ::std::size_t method_size{
			(details::print_integer_reserved_size_cache<flags.base, flags.showbase, flags.showpos, flags.modern_octal, ::std::size_t> + 1) *
				::fast_io::details::method_ptr_hold_size -
			1};
		return method_size;
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<T>, ::std::nullptr_t>)
	{
		return details::nullptr_print_optimization_call_size_cache<char_type, flags.base, flags.showbase,
																   flags.uppercase_showbase, flags.showpos,
																   flags.uppercase, flags.full, flags.modern_octal>;
	}
	else if constexpr (::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
	{
		return details::print_integer_reserved_size_cache<flags.base, flags.showbase, flags.showpos, flags.modern_octal, char8_t>;
	}
	else
	{
		return details::print_integer_reserved_size_cache<flags.base, flags.showbase, flags.showpos, flags.modern_octal, T>;
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires(details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte> ||
			 ::std::same_as<::std::remove_cvref_t<T>, ::std::nullptr_t> ||
			 ::std::same_as<::std::remove_cv_t<T>, ::fast_io::manipulators::member_function_pointer_holder_t>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]] // always inline to reduce inline depth in GCC and LLVM clang
#endif
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T>>, char_type *iter,
					 ::fast_io::manipulators::scalar_manip_t<flags, T> t) noexcept
{
	static_assert(flags.percentage == ::fast_io::manipulators::percentage_flag::none);
	if constexpr (flags.alphabet)
	{
		static_assert((::std::same_as<::std::remove_cvref_t<T>, bool> ||
					   ::std::same_as<::std::remove_cvref_t<T>, ::std::nullptr_t>),
					  "only bool and ::std::nullptr_t support alphabet output");
		if constexpr (::std::same_as<::std::remove_cvref_t<T>, bool>)
		{
			return details::print_reserve_boolalpha_impl<flags.uppercase>(iter, t.reference);
		}
		else
		{
			return details::print_reserve_nullptr_alphabet_impl<flags.uppercase>(iter);
		}
	}
	else if constexpr (::std::same_as<::std::remove_cv_t<T>, ::fast_io::manipulators::member_function_pointer_holder_t>)
	{
		return ::fast_io::details::print_reserve_method_impl<flags.base, flags.showbase, flags.uppercase_showbase,
															 flags.showpos, flags.uppercase, flags.full, flags.modern_octal>(iter,
																															 t.reference);
	}
	else if constexpr (::std::same_as<::std::remove_cv_t<T>, ::std::nullptr_t>)
	{
		constexpr auto &cache{details::nullptr_print_optimization_call_cache<char_type, flags.base, flags.showbase,
																			 flags.uppercase_showbase, flags.showpos,
																			 flags.uppercase, flags.full, flags.modern_octal>};
		constexpr ::std::size_t n{cache.size()};
		return details::non_overlapped_copy_n(cache.element, n, iter);
	}
	else if constexpr (::std::same_as<::std::remove_cv_t<T>, ::std::byte>)
	{
		return details::print_reserve_integral_define<flags.base, flags.showbase, flags.uppercase_showbase,
													  flags.showpos, flags.uppercase, flags.full, flags.modern_octal>(
			iter, static_cast<char8_t>(t.reference));
	}
	else
	{
		return details::print_reserve_integral_define<flags.base, flags.showbase, flags.uppercase_showbase,
													  flags.showpos, flags.uppercase, flags.full, flags.modern_octal>(iter, t.reference);
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) && !flags.alphabet)
inline constexpr ::std::size_t
print_reserve_precise_size(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>,
						   manipulators::scalar_manip_t<flags, T> t) noexcept
{
	if constexpr (::std::same_as<T, ::std::byte>)
	{
		return details::print_integer_reserved_precise_size<flags.base, flags.showbase, flags.showpos, flags.full,
															flags.modern_octal>(static_cast<char8_t>(t.reference));
	}
	else
	{
		return details::print_integer_reserved_precise_size<flags.base, flags.showbase, flags.showpos, flags.full,
															flags.modern_octal>(t.reference);
	}
}

template <::std::integral char_type, ::std::random_access_iterator Iter, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) && !flags.alphabet)
inline constexpr void print_reserve_precise_define(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>,
												   Iter iter, ::std::size_t n,
												   manipulators::scalar_manip_t<flags, T> t) noexcept
{
	if constexpr (::std::same_as<T, ::std::byte>)
	{
		details::print_reserve_integral_define_precise<flags.base, flags.showbase, flags.uppercase_showbase,
													   flags.showpos, flags.uppercase, flags.modern_octal>(
			iter, n, static_cast<char8_t>(t.reference));
	}
	else
	{
		details::print_reserve_integral_define_precise<flags.base, flags.showbase, flags.uppercase_showbase,
													   flags.showpos, flags.uppercase, flags.modern_octal>(iter, n, t.reference);
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename T>
	requires((details::my_integral<T> || ::std::same_as<::std::remove_cv_t<T>, ::std::byte>) && !flags.alphabet &&
			 (flags.showpos || (details::my_signed_integral<T> && !::std::same_as<::std::remove_cv_t<T>, bool>)))
inline constexpr ::std::size_t
print_define_internal_shift(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, T>>,
							manipulators::scalar_manip_t<flags, T> t) noexcept
{
	if constexpr (flags.showpos)
	{
		return 1;
	}
	else
	{
		return t.reference < 0;
	}
}

} // namespace fast_io

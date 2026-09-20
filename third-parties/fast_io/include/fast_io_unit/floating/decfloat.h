#pragma once
#include "hexfloat.h"
#include "roundtrip.h"

namespace fast_io
{

namespace details
{

template <typename T, ::std::size_t table_size>
inline constexpr auto scan_decfloat_generate_power10_table() noexcept
{
	using value_type = ::std::remove_cv_t<T>;
	::fast_io::freestanding::array<value_type, table_size> table{};
	value_type value{static_cast<value_type>(1)};
	for (::std::size_t i{}; i != table_size; ++i)
	{
		table.index_unchecked(i) = value;
		value = static_cast<value_type>(value * static_cast<value_type>(10));
	}
	return table;
}

template <typename T, ::std::size_t table_size>
inline constexpr auto scan_decfloat_power10_table{
	::fast_io::details::scan_decfloat_generate_power10_table<T, table_size>()};

template <typename T, ::std::size_t table_size>
inline constexpr auto scan_decfloat_generate_clinger_max_mantissa_table() noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	::fast_io::freestanding::array<::std::uint_least64_t, table_size> table{};
	::std::uint_least64_t divisor{1u};
	constexpr ::std::uint_least64_t limit{::std::uint_least64_t{2u} << trait::mbits};
	for (::std::size_t i{}; i != table_size; ++i)
	{
		table.index_unchecked(i) = limit / divisor;
		divisor *= 5u;
	}
	return table;
}

template <typename T, ::std::size_t table_size>
inline constexpr auto scan_decfloat_clinger_max_mantissa_table{
	::fast_io::details::scan_decfloat_generate_clinger_max_mantissa_table<T, table_size>()};

template <typename T>
concept scan_decfloat_has_iec559_traits = requires {
	typename ::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>::mantissa_type;
};

/*
numeric_limits describes arithmetic precision, not necessarily an IEC 60559
object encoding.  PowerPC IBM double-double long double reports 106
significant bits but stores two binary64 components.  It is admitted only by
the explicit component-sum bridge in punning.h; the ordinary implicit-field
case still requires equal object/carrier sizes and therefore cannot
accidentally concatenate those components.  The remaining layouts are
binary16, bfloat16, binary32, binary64, little-endian x87 binary80, and
binary128.
*/
template <typename T, bool = ::fast_io::details::scan_decfloat_has_iec559_traits<T>>
struct scan_decfloat_layout_supported_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct scan_decfloat_layout_supported_impl<T, true>
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	using mantissa_type = typename trait::mantissa_type;
	inline static constexpr bool x87_binary80{
		::fast_io::details::fp_floating_point_is_float80<no_cvref_t> &&
		::std::endian::native == ::std::endian::little && trait::mbits == 63u &&
		trait::ebits == 15u && sizeof(mantissa_type) == sizeof(::std::uint_least64_t)};
	inline static constexpr bool implicit_integer_bit{
		sizeof(no_cvref_t) == sizeof(mantissa_type) &&
		((trait::mbits == 10u && trait::ebits == 5u) ||
		 (trait::mbits == 7u && trait::ebits == 8u) ||
		 (trait::mbits == 23u && trait::ebits == 8u) ||
		 (trait::mbits == 52u && trait::ebits == 11u) ||
		 (trait::mbits == 112u && trait::ebits == 15u))};
	inline static constexpr bool ibm_double_double{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
		::fast_io::details::fp_floating_point_is_ibm_double_double<no_cvref_t>
#else
		false
#endif
	};
	inline static constexpr bool value{x87_binary80 || implicit_integer_bit ||
		ibm_double_double};
};

template <typename T>
inline constexpr bool scan_decfloat_layout_supported{
	::fast_io::details::scan_decfloat_layout_supported_impl<T>::value};

template <typename T, bool = ::fast_io::details::scan_decfloat_has_iec559_traits<T>>
struct scan_decfloat_compute_supported_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct scan_decfloat_compute_supported_impl<T, true>
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	inline static constexpr bool value{sizeof(typename trait::mantissa_type) <= sizeof(::std::uint_least64_t) &&
									   trait::mbits + 3u <= 64u &&
									   trait::e10max <= ::fast_io::details::iec559_traits<double>::e10max};
};

template <typename T>
inline constexpr bool scan_decfloat_compute_supported{
	::fast_io::details::scan_decfloat_compute_supported_impl<T>::value};

template <typename T, bool = ::fast_io::details::scan_decfloat_has_iec559_traits<T>>
struct scan_decfloat_native_wide_supported_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct scan_decfloat_native_wide_supported_impl<T, true>
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	inline static constexpr bool value{!::fast_io::details::scan_decfloat_compute_supported<no_cvref_t> &&
									   (sizeof(typename trait::mantissa_type) > sizeof(::std::uint_least64_t) ||
										trait::e10max > ::fast_io::details::iec559_traits<double>::e10max ||
										::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)};
};

template <typename T>
inline constexpr bool scan_decfloat_native_wide_supported{
	::fast_io::details::scan_decfloat_native_wide_supported_impl<T>::value};

template <typename T>
concept scan_decfloat_supported_floating_point =
	::fast_io::details::my_floating_point<::std::remove_cvref_t<T>> &&
	::fast_io::details::scan_decfloat_layout_supported<T> &&
	(::fast_io::details::scan_decfloat_compute_supported<T> ||
	 ::fast_io::details::scan_decfloat_native_wide_supported<T>);

template <typename T, bool = ::fast_io::details::scan_decfloat_has_iec559_traits<T>>
struct scan_decfloat_binary32_like_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct scan_decfloat_binary32_like_impl<T, true>
{
	using trait = ::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>;
	inline static constexpr bool value{trait::mbits == 23u && trait::ebits == 8u &&
									   sizeof(typename trait::mantissa_type) <= sizeof(::std::uint_least32_t)};
};

template <typename T, bool = ::fast_io::details::scan_decfloat_has_iec559_traits<T>>
struct scan_decfloat_binary64_like_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct scan_decfloat_binary64_like_impl<T, true>
{
	using trait = ::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>;
	inline static constexpr bool value{trait::mbits == 52u && trait::ebits == 11u &&
									   sizeof(typename trait::mantissa_type) <= sizeof(::std::uint_least64_t)};
};

template <typename T>
inline constexpr bool scan_decfloat_binary32_like{
	::fast_io::details::scan_decfloat_binary32_like_impl<T>::value};

template <typename T>
inline constexpr bool scan_decfloat_binary64_like{
	::fast_io::details::scan_decfloat_binary64_like_impl<T>::value};

template <::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr bool scan_decfloat_decimal_digit(char_type ch, char8_t &digit) noexcept
{
	using unsigned_char_type = ::fast_io::details::my_make_unsigned_t<char_type>;
	auto uch{static_cast<unsigned_char_type>(ch)};
	if constexpr (sizeof(char_type) == sizeof(char8_t) && ::fast_io::details::is_ascii<char_type>)
	{
		constexpr auto zero{static_cast<unsigned_char_type>(::fast_io::char_literal_v<u8'0', char_type>)};
		auto const value{static_cast<unsigned_char_type>(uch - zero)};
		if (value < 10u)
		{
			digit = static_cast<char8_t>(value);
			return true;
		}
	}
	else
	{
		if (::fast_io::details::char_digit_to_literal<10, char_type>(uch))
		{
			return false;
		}
		digit = static_cast<char8_t>(uch);
		return true;
	}
	return false;
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr bool scan_decfloat_try_clinger(::std::uint_least64_t significand,
															  ::std::int_least64_t exponent,
															  bool negative, T &value) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (::fast_io::details::scan_decfloat_binary32_like<no_cvref_t>)
	{
		constexpr ::std::uint_least64_t significand_limit{::std::uint_least64_t{1} << 24u};
		if (significand <= significand_limit && -10 <= exponent && exponent <= 10)
		{
			no_cvref_t result{static_cast<no_cvref_t>(significand)};
			auto const &table{::fast_io::details::scan_decfloat_power10_table<no_cvref_t, 11u>};
			if (exponent < 0)
			{
				result /= table.index_unchecked(static_cast<::std::size_t>(-exponent));
			}
			else
			{
				result *= table.index_unchecked(static_cast<::std::size_t>(exponent));
			}
			value = negative ? -result : result;
			return true;
		}
		if (0 <= exponent && exponent <= 10)
		{
			auto const &max_table{::fast_io::details::scan_decfloat_clinger_max_mantissa_table<no_cvref_t, 11u>};
			auto const index{static_cast<::std::size_t>(exponent)};
			if (significand <= max_table.index_unchecked(index))
			{
				auto const &table{::fast_io::details::scan_decfloat_power10_table<no_cvref_t, 11u>};
				no_cvref_t result{static_cast<no_cvref_t>(significand)};
				result *= table.index_unchecked(index);
				value = negative ? -result : result;
				return true;
			}
		}
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary64_like<no_cvref_t>)
	{
		constexpr ::std::uint_least64_t significand_limit{::std::uint_least64_t{1} << 53u};
		if (significand <= significand_limit && -22 <= exponent && exponent <= 22)
		{
			no_cvref_t result{static_cast<no_cvref_t>(significand)};
			auto const &table{::fast_io::details::scan_decfloat_power10_table<no_cvref_t, 23u>};
			if (exponent < 0)
			{
				result /= table.index_unchecked(static_cast<::std::size_t>(-exponent));
			}
			else
			{
				result *= table.index_unchecked(static_cast<::std::size_t>(exponent));
			}
			value = negative ? -result : result;
			return true;
		}
		if (0 <= exponent && exponent <= 22)
		{
			auto const &max_table{::fast_io::details::scan_decfloat_clinger_max_mantissa_table<no_cvref_t, 23u>};
			auto const index{static_cast<::std::size_t>(exponent)};
			if (significand <= max_table.index_unchecked(index))
			{
				auto const &table{::fast_io::details::scan_decfloat_power10_table<no_cvref_t, 23u>};
				no_cvref_t result{static_cast<no_cvref_t>(significand)};
				result *= table.index_unchecked(index);
				value = negative ? -result : result;
				return true;
			}
		}
	}
	else if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
	{
		if (-27 <= exponent && exponent <= 27)
		{
			no_cvref_t result{static_cast<no_cvref_t>(significand)};
			auto const &table{::fast_io::details::scan_decfloat_power10_table<no_cvref_t, 28u>};
			if (exponent < 0)
			{
				result /= table.index_unchecked(static_cast<::std::size_t>(-exponent));
			}
			else
			{
				result *= table.index_unchecked(static_cast<::std::size_t>(exponent));
			}
			value = negative ? -result : result;
			return true;
		}
	}
	return false;
}

/*
The Clinger shortcut evaluates a small decimal with native floating multiply
or divide.  Its theorem assumes nearest-even hardware arithmetic.  Runtime
callers may change the C floating environment even when fast_io itself was
compiled without floating-environment integration, so the absence of a library
macro cannot prove that assumption.  The three assignment sites below admit
Clinger only during constant evaluation, whose arithmetic mode is fixed by the
translation and cannot observe a runtime fenv.  Runtime input always uses the
integer cached-product/exact pipeline and is consequently a pure function of
the explicit rounding template argument.
*/

/*
The retained prefix must be long enough to contain every terminating decimal
midpoint of every supported IEEE binary format.  This is stronger than merely
retaining enough digits to print the destination: an input can lie at the
midpoint between zero and the least subnormal, whose terminating expansion is
very long for binary80 and binary128.

After powers of two are cancelled, any midpoint is A/2^k with A odd.  Its
terminating decimal significand is A*5^k.  For the widest supported format,
binary128, k<=16495 and A has at most p+1=114 bits.  Hence its significand has
at most

  floor(log10(A)+k*log10(5))+1
	< 114*log10(2)+16495*log10(5)+1 < 11565

digits.  The round value 12000 leaves 435 guard digits.  Narrow destinations
use their smaller independently sufficient bounds below, so a binary32/64 hot
parse does not pay binary128's stack and streaming-storage cost.  If a longer
input agrees with a midpoint in every retained place, the midpoint has already
terminated, so the omitted input is either all zero (equal) or has a nonzero
digit (strictly greater).  Conversely, a value below that midpoint must differ
inside the retained prefix.  Thus the prefix plus exact_truncated_nonzero is
sufficient to order every input around every rounding boundary; no unbounded
input storage is required.

For binary64, a midpoint numerator has at most p+1=54 bits and k<=1075,
making the same strict bound smaller than
54*log10(2)+1075*log10(5)+1<768.7; since a digit count is integral, 768
positions suffice.  IBM double-double shares binary64's minimum quantum
2^-1074 but an adjacent-value midpoint can carry 107 numerator bits.  Its
strict bound is

  107*log10(2)+1075*log10(5)+1 < 785,

so 832 positions retain every IBM midpoint with 47 guard digits.  Every format
with ebits<=8 has p<=24 and k<=150, giving a bound below 114, so 128 positions
suffice for binary32, bfloat16, binary16, and narrower encodings.  The four
capacities below are therefore proved rather than empirical parser limits.
*/
template <typename T>
inline constexpr ::std::size_t scan_decfloat_exact_digit_capacity{
	::fast_io::details::fp_floating_point_is_ibm_double_double<::std::remove_cvref_t<T>> ? 832u : ::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>::ebits >= 15u ? 12000u : ::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>::ebits >= 11u ? 768u
																															   : 128u};

template <typename T>
struct scan_decfloat_significand_state
{
	::std::uint_least64_t significand{};
	::std::uint_least64_t significant_digits{};
	::std::uint_least64_t stored_digits{};
	::std::uint_least64_t fractional_digits{};
	::std::uint_least16_t exact_stored_digits{};
	bool has_digit{};
	bool has_nonzero_digit{};
	bool truncated_nonzero{};
	bool exact_truncated_nonzero{};
	char8_t exact_digits[scan_decfloat_exact_digit_capacity<T>];
};

template <::std::integral char_type>
struct scan_decfloat_fast_result
{
	char_type const *iter{};
	::fast_io::parse_code code{};
	bool handled{};
};

inline constexpr ::std::uint_least64_t scan_decfloat_uint64_significand_digit_limit{19u};

template <typename>
inline constexpr ::std::uint_least64_t scan_decfloat_significand_digit_limit{
	::fast_io::details::scan_decfloat_uint64_significand_digit_limit};
inline constexpr ::std::int_least32_t scan_decfloat_dragonbox_min_power10{-342};
inline constexpr ::std::int_least32_t scan_decfloat_dragonbox_max_power10{326};
inline constexpr ::std::uint_least64_t scan_decfloat_pow10_0_to_8_table[]{
	1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u, 10000000u, 100000000u};

template <typename T>
inline constexpr void scan_decfloat_append_exact_digit(scan_decfloat_significand_state<T> &state,
													   char8_t digit) noexcept
{
	if (state.exact_stored_digits != ::fast_io::details::scan_decfloat_exact_digit_capacity<T>)
	{
		state.exact_digits[state.exact_stored_digits] = digit;
		++state.exact_stored_digits;
	}
	else if (digit != 0)
	{
		state.exact_truncated_nonzero = true;
	}
}

template <typename T>
inline constexpr void scan_decfloat_append_exact_eight_digits(scan_decfloat_significand_state<T> &state,
															  ::std::uint_least32_t digits) noexcept
{
	if (state.exact_stored_digits == ::fast_io::details::scan_decfloat_exact_digit_capacity<T>)
	{
		if (digits != 0)
		{
			state.exact_truncated_nonzero = true;
		}
		return;
	}
	for (auto divisor{10000000u}; divisor != 0u; divisor /= 10u)
	{
		auto const digit{static_cast<char8_t>(digits / divisor)};
		digits -= static_cast<::std::uint_least32_t>(digit) * divisor;
		::fast_io::details::scan_decfloat_append_exact_digit(state, digit);
		if (divisor == 1u)
		{
			break;
		}
	}
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr void scan_decfloat_append_exact_ascii8_digits_slow(scan_decfloat_significand_state<T> &state,
																	::std::uint_least64_t val) noexcept
{
	for (::std::size_t offset{}; offset != 8u; ++offset)
	{
		auto const ch{static_cast<char8_t>((val >> (offset * 8u)) & 0xFFu)};
		auto const digit{static_cast<char8_t>(ch - u8'0')};
		::fast_io::details::scan_decfloat_append_exact_digit(state, digit);
	}
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scan_decfloat_append_exact_ascii8_digits(scan_decfloat_significand_state<T> &state,
															   ::std::uint_least64_t val) noexcept
{
	if constexpr (::std::endian::native == ::std::endian::little &&
				  ::std::numeric_limits<::std::uint_least64_t>::digits == 64u)
	{
		if (state.exact_stored_digits + 8u <= ::fast_io::details::scan_decfloat_exact_digit_capacity<T>)
		{
			auto const digits{static_cast<::std::uint_least64_t>(val - 0x3030303030303030u)};
			::fast_io::freestanding::my_memcpy(
				state.exact_digits + state.exact_stored_digits, __builtin_addressof(digits),
				sizeof(::std::uint_least64_t));
			state.exact_stored_digits += 8u;
			return;
		}
	}
	::fast_io::details::scan_decfloat_append_exact_ascii8_digits_slow(state, val);
}

struct scan_decfloat_adjusted_mantissa
{
	::std::uint_least64_t mantissa{};
	::std::int_least32_t power2{};
};

[[nodiscard]] inline constexpr bool
scan_decfloat_adjusted_mantissa_equal(scan_decfloat_adjusted_mantissa left,
									  scan_decfloat_adjusted_mantissa right) noexcept
{
	return left.mantissa == right.mantissa && left.power2 == right.power2;
}

struct scan_decfloat_uint128
{
	::std::uint_least64_t low{};
	::std::uint_least64_t high{};
};

[[nodiscard]] inline constexpr ::std::int_least32_t
scan_decfloat_binary_power(::std::int_least32_t exponent) noexcept
{
	return static_cast<::std::int_least32_t>((((152170 + 65536) * exponent) >> 16) + 63);
}

template <typename T>
[[nodiscard]] inline constexpr ::std::int_least32_t scan_decfloat_smallest_power10() noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	if constexpr (trait::mbits == 10u && trait::ebits == 5u)
	{
		return -27;
	}
	else if constexpr (trait::mbits == 7u && trait::ebits == 8u)
	{
		return -60;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary32_like<no_cvref_t>)
	{
		return -64;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary64_like<no_cvref_t>)
	{
		return -342;
	}
	else
	{
		return ::fast_io::details::scan_decfloat_dragonbox_min_power10;
	}
}

template <typename T>
[[nodiscard]] inline constexpr ::std::int_least32_t scan_decfloat_largest_power10() noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	if constexpr (trait::mbits == 10u && trait::ebits == 5u)
	{
		return 4;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary32_like<no_cvref_t>)
	{
		return 38;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary64_like<no_cvref_t>)
	{
		return 308;
	}
	else
	{
		return static_cast<::std::int_least32_t>(trait::e10max);
	}
}

template <typename T>
[[nodiscard]] inline constexpr ::std::int_least32_t scan_decfloat_minimum_exponent() noexcept
{
	using trait = ::fast_io::details::iec559_traits<::std::remove_cv_t<T>>;
	return -static_cast<::std::int_least32_t>((static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u);
}

template <typename T>
[[nodiscard]] inline constexpr ::std::int_least32_t scan_decfloat_infinite_power() noexcept
{
	using trait = ::fast_io::details::iec559_traits<::std::remove_cv_t<T>>;
	return static_cast<::std::int_least32_t>((static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u);
}

template <typename T>
[[nodiscard]] inline constexpr ::std::int_least32_t scan_decfloat_min_round_to_even_power10() noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	if constexpr (trait::mbits == 10u && trait::ebits == 5u)
	{
		return -22;
	}
	else if constexpr (trait::mbits == 7u && trait::ebits == 8u)
	{
		return -24;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary32_like<no_cvref_t>)
	{
		return -17;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary64_like<no_cvref_t>)
	{
		return -4;
	}
	else
	{
		return 0;
	}
}

template <typename T>
[[nodiscard]] inline constexpr ::std::int_least32_t scan_decfloat_max_round_to_even_power10() noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	if constexpr (trait::mbits == 10u && trait::ebits == 5u)
	{
		return 5;
	}
	else if constexpr (trait::mbits == 7u && trait::ebits == 8u)
	{
		return 3;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary32_like<no_cvref_t>)
	{
		return 10;
	}
	else if constexpr (::fast_io::details::scan_decfloat_binary64_like<no_cvref_t>)
	{
		return 23;
	}
	else
	{
		return 0;
	}
}

template <::fast_io::manipulators::floating_rounding rounding>
inline constexpr bool scan_decfloat_nearest_rounding{
	rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_to_odd ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_plus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_minus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_zero ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_away_from_zero};

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool scan_decfloat_directed_round_up(bool negative) noexcept
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
[[nodiscard]] inline constexpr bool scan_decfloat_nearest_tie_round_up(bool negative,
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

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr ::std::uint_least64_t
scan_decfloat_round_mantissa(bool negative, ::std::uint_least64_t mantissa, bool has_tail, bool is_tie) noexcept
{
	if constexpr (::fast_io::details::scan_decfloat_nearest_rounding<rounding>)
	{
		/*
		`mantissa = 2q + g`, where q is the lower candidate and g is the
		half-ulp guard bit. Hence g=0 is strictly below the midpoint even when
		later discarded bits are nonzero, while g=1 is at or above it. All six
		nearest policies therefore share this magnitude test; they differ only
		when `is_tie` proves exact equality with the midpoint. In particular,
		nearest-to-odd is not round-to-odd jamming: parity may select a result
		only at a tie.
		*/
		if ((mantissa & 1u) != 0u)
		{
			if (!is_tie || ::fast_io::details::scan_decfloat_nearest_tie_round_up<rounding>(negative, mantissa))
			{
				++mantissa;
			}
		}
	}
	else
	{
		if (::fast_io::details::scan_decfloat_directed_round_up<rounding>(negative))
		{
			if ((mantissa & 1u) != 0u)
			{
				++mantissa;
			}
			else if (has_tail)
			{
				mantissa += ::std::uint_least64_t{2u};
			}
		}
	}
	return mantissa >> 1u;
}

[[nodiscard]] inline constexpr scan_decfloat_uint128
scan_decfloat_mul_64x128_high(::std::uint_least64_t value, ::fast_io::details::uint64x2 cache) noexcept
{
	::std::uint_least64_t high{};
	auto low{::fast_io::intrinsics::umul(value, cache.hi, high)};
	auto const middle{::fast_io::intrinsics::umulh(value, cache.lo)};
	low += middle;
	if (low < middle)
	{
		++high;
	}
	return {low, high};
}

template <typename T, ::fast_io::manipulators::floating_rounding rounding =
						  ::fast_io::manipulators::floating_rounding::nearest_to_even>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr bool scan_decfloat_compute_adjusted(::std::int_least64_t exponent,
																   ::std::uint_least64_t significand,
																   bool negative,
																   scan_decfloat_adjusted_mantissa &answer) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if constexpr (!::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
	{
		return false;
	}
	else
	{
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		constexpr auto mantissa_explicit_bits{static_cast<::std::int_least32_t>(trait::mbits)};
		constexpr auto minimum_exponent{::fast_io::details::scan_decfloat_minimum_exponent<no_cvref_t>()};
		constexpr auto infinite_power{::fast_io::details::scan_decfloat_infinite_power<no_cvref_t>()};
		constexpr auto max_finite_mantissa{(::std::uint_least64_t{1u} << mantissa_explicit_bits) - 1u};
		if (significand == 0)
		{
			answer = {};
			return true;
		}
		if (exponent < ::fast_io::details::scan_decfloat_smallest_power10<no_cvref_t>())
		{
			if constexpr (!::fast_io::details::scan_decfloat_nearest_rounding<rounding>)
			{
				if (::fast_io::details::scan_decfloat_directed_round_up<rounding>(negative))
				{
					answer = {.mantissa = 1u, .power2 = 0};
					return true;
				}
			}
			answer = {};
			return true;
		}
		if (exponent > ::fast_io::details::scan_decfloat_largest_power10<no_cvref_t>())
		{
			if constexpr (!::fast_io::details::scan_decfloat_nearest_rounding<rounding>)
			{
				if (!::fast_io::details::scan_decfloat_directed_round_up<rounding>(negative))
				{
					answer = {.mantissa = max_finite_mantissa, .power2 = infinite_power - 1};
					return true;
				}
			}
			answer = {.mantissa = 0, .power2 = infinite_power};
			return true;
		}
		if (exponent < ::fast_io::details::scan_decfloat_dragonbox_min_power10 ||
			exponent > ::fast_io::details::scan_decfloat_dragonbox_max_power10)
		{
			return false;
		}
		auto const exponent32{static_cast<::std::int_least32_t>(exponent)};
		auto const leading_zeroes{static_cast<::std::int_least32_t>(::std::countl_zero(significand))};
		significand <<= static_cast<unsigned>(leading_zeroes);
		auto const cache{::fast_io::details::compute_pow10_float64_scan(exponent32)};
		auto const product{::fast_io::details::scan_decfloat_mul_64x128_high(significand, cache)};
		auto const upperbit{static_cast<::std::int_least32_t>(product.high >> 63u)};
		auto const shift{upperbit + 64 - mantissa_explicit_bits - 3};
		auto mantissa{product.high >> static_cast<unsigned>(shift)};
		auto power2{static_cast<::std::int_least32_t>(
			::fast_io::details::scan_decfloat_binary_power(exponent32) + upperbit - leading_zeroes - minimum_exponent)};
		if (power2 <= 0)
		{
			if (-power2 + 1 >= 64)
			{
				answer = {};
				return true;
			}
			auto const subnormal_shift{static_cast<unsigned>(-power2 + 1)};
			if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
			{
				mantissa >>= subnormal_shift;
				mantissa += mantissa & 1u;
				mantissa >>= 1u;
			}
			else
			{
				auto const subnormal_tail_mask{(::std::uint_least64_t{1u} << subnormal_shift) - 1u};
				bool const has_tail{(mantissa & subnormal_tail_mask) != 0u || product.low != 0u};
				mantissa >>= subnormal_shift;
				bool const is_tie{!has_tail && (mantissa & 1u) != 0u};
				mantissa =
					::fast_io::details::scan_decfloat_round_mantissa<rounding>(negative, mantissa, has_tail, is_tie);
			}
			answer.power2 = mantissa < (::std::uint_least64_t{1} << mantissa_explicit_bits) ? 0 : 1;
			answer.mantissa = mantissa;
			return true;
		}
		if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
		{
			if (product.low <= 1 &&
				::fast_io::details::scan_decfloat_min_round_to_even_power10<no_cvref_t>() <= exponent &&
				exponent <= ::fast_io::details::scan_decfloat_max_round_to_even_power10<no_cvref_t>() &&
				(mantissa & 3u) == 1u &&
				(mantissa << static_cast<unsigned>(shift)) == product.high)
			{
				mantissa &= ~::std::uint_least64_t{1};
			}
			mantissa += mantissa & 1u;
			mantissa >>= 1u;
		}
		else
		{
			auto const shifted_back{mantissa << static_cast<unsigned>(shift)};
			bool const is_tie{
				product.low <= 1 &&
				::fast_io::details::scan_decfloat_min_round_to_even_power10<no_cvref_t>() <= exponent &&
				exponent <= ::fast_io::details::scan_decfloat_max_round_to_even_power10<no_cvref_t>() &&
				(mantissa & 1u) != 0u && shifted_back == product.high};
			bool const has_tail{shifted_back != product.high || product.low != 0u};
			mantissa =
				::fast_io::details::scan_decfloat_round_mantissa<rounding>(negative, mantissa, has_tail, is_tie);
		}
		if (mantissa >= (::std::uint_least64_t{2} << mantissa_explicit_bits))
		{
			mantissa = ::std::uint_least64_t{1} << mantissa_explicit_bits;
			++power2;
		}
		mantissa &= ~(::std::uint_least64_t{1} << mantissa_explicit_bits);
		if (power2 >= infinite_power)
		{
			if constexpr (!::fast_io::details::scan_decfloat_nearest_rounding<rounding>)
			{
				if (!::fast_io::details::scan_decfloat_directed_round_up<rounding>(negative))
				{
					answer = {.mantissa = max_finite_mantissa, .power2 = infinite_power - 1};
					return true;
				}
			}
			answer = {.mantissa = 0, .power2 = infinite_power};
			return true;
		}
		answer = {.mantissa = mantissa, .power2 = power2};
		return true;
	}
}

template <typename T>
inline constexpr void scan_decfloat_to_float(bool negative, scan_decfloat_adjusted_mantissa adjusted,
											 T &value) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	using mantissa_type = typename trait::mantissa_type;
	auto word{static_cast<mantissa_type>(
		static_cast<mantissa_type>(adjusted.mantissa) |
		(static_cast<mantissa_type>(adjusted.power2) << trait::mbits))};
	if (negative)
	{
		word |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << (trait::mbits + trait::ebits));
	}
	value = ::fast_io::bit_cast<no_cvref_t>(word);
}

inline constexpr ::std::int_least64_t scan_decfloat_saturating_add(::std::int_least64_t a,
																   ::std::int_least64_t b) noexcept
{
	constexpr auto int64_max{(::std::numeric_limits<::std::int_least64_t>::max)()};
	constexpr auto int64_min{(::std::numeric_limits<::std::int_least64_t>::min)()};
	if (b > 0 && a > int64_max - b)
	{
		return int64_max;
	}
	if (b < 0 && a < int64_min - b)
	{
		return int64_min;
	}
	return a + b;
}

template <typename T>
[[nodiscard]] inline constexpr ::std::int_least64_t
scan_decfloat_adjusted_exponent(scan_decfloat_significand_state<T> const &state,
								::std::int_least64_t exponent) noexcept
{
	auto adjusted_exponent{::fast_io::details::scan_decfloat_saturating_add(
		exponent, -static_cast<::std::int_least64_t>(state.fractional_digits))};
	return ::fast_io::details::scan_decfloat_saturating_add(
		adjusted_exponent,
		static_cast<::std::int_least64_t>(state.significant_digits - state.stored_digits));
}

template <typename T, ::fast_io::manipulators::floating_rounding rounding =
						  ::fast_io::manipulators::floating_rounding::nearest_to_even>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_native_wide(T &value, bool negative, ::std::uint_least64_t significand,
								 ::std::int_least64_t adjusted_exponent) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	if (!significand)
	{
		value = negative ? -static_cast<no_cvref_t>(0.0) : static_cast<no_cvref_t>(0.0);
		return ::fast_io::parse_code::ok;
	}
	constexpr auto e10max{static_cast<::std::int_least64_t>(trait::e10max)};
	constexpr auto underflow_guard{
		static_cast<::std::int_least64_t>(trait::e10max + trait::m10digits + 8u)};
	if (adjusted_exponent > e10max)
	{
		::fast_io::details::fp_assign_infinity(value, negative);
		return ::fast_io::parse_code::overflow;
	}
	if (adjusted_exponent < -underflow_guard)
	{
		value = negative ? -static_cast<no_cvref_t>(0.0) : static_cast<no_cvref_t>(0.0);
		return ::fast_io::parse_code::overflow;
	}
	no_cvref_t result{static_cast<no_cvref_t>(significand)};
	auto const &table{::fast_io::details::scan_decfloat_power10_table<no_cvref_t, 20u>};
	constexpr ::std::int_least64_t chunk{19};
	if (adjusted_exponent < 0)
	{
		auto exponent{-adjusted_exponent};
		for (; exponent >= chunk; exponent -= chunk)
		{
			result /= table.index_unchecked(static_cast<::std::size_t>(chunk));
		}
		if (exponent)
		{
			result /= table.index_unchecked(static_cast<::std::size_t>(exponent));
		}
	}
	else
	{
		for (; adjusted_exponent >= chunk; adjusted_exponent -= chunk)
		{
			result *= table.index_unchecked(static_cast<::std::size_t>(chunk));
		}
		if (adjusted_exponent)
		{
			result *= table.index_unchecked(static_cast<::std::size_t>(adjusted_exponent));
		}
	}
	value = negative ? -result : result;
	if (result == static_cast<no_cvref_t>(0.0))
	{
		return ::fast_io::parse_code::overflow;
	}
	return ::fast_io::parse_code::ok;
}

	// The exact midpoint comparator needs arbitrary-precision multiplication but
	// not a native 128-bit scalar.  Keep its limb layer available on every target:
	// intrinsics::umul supplies the complete 64-by-64 product as two words, and
	// the explicit carry recurrence below is mathematically identical to adding
	// the incoming carry to a native-u128 product.  Only the quotient materializer,
	// which actually stores a 128-bit quotient, remains capability-gated below.
/*
12000 decimal digits occupy at most

  ceil(12000*log2(10)/64) = 623

64-bit limbs.  A 640-limb allocation therefore represents the complete
retained significand.  At a binary128 rounding boundary the alternative
power-of-five construction is smaller: 5^16495 times a 114-bit midpoint
uses at most 601 limbs.  The decimal top-exponent guards reject values
outside the destination range before any larger scaling is attempted.  A
target without native 128-bit integers can instantiate this quotient only
through binary64: 768 decimal digits and 5^1075 times a 54-bit midpoint each
need fewer than 41 limbs.  Its 288-limb bound retains generous room for the
guarded shifts.  Selecting 288 limbs for narrow types and 640 only for
binary80/binary128 prevents a cold exact fallback from imposing wide-format
stack cost on binary32/binary64 or MSVC.
*/
template <typename T>
inline constexpr ::std::size_t scan_decfloat_bigint_limb_capacity{
	::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>::ebits >= 15u ? 640u : 288u};

template <::std::size_t limb_capacity>
struct scan_decfloat_bigint
{
	::std::uint_least64_t limb[limb_capacity];
	::std::size_t size{};
};

template <typename T>
using scan_decfloat_bigint_for = ::fast_io::details::scan_decfloat_bigint<
	::fast_io::details::scan_decfloat_bigint_limb_capacity<T>>;

template <::std::size_t limb_capacity>
inline constexpr void scan_decfloat_bigint_clear(scan_decfloat_bigint<limb_capacity> &value) noexcept
{
	value.size = 0u;
}

template <::std::size_t limb_capacity>
inline constexpr void scan_decfloat_bigint_copy(scan_decfloat_bigint<limb_capacity> &out,
												scan_decfloat_bigint<limb_capacity> const &in) noexcept
{
	out.size = in.size;
	for (::std::size_t index{}; index != in.size; ++index)
	{
		out.limb[index] = in.limb[index];
	}
	}

	template <::std::size_t limb_capacity>
	inline constexpr void scan_decfloat_bigint_normalize(scan_decfloat_bigint<limb_capacity> &value) noexcept
	{
		for (; value.size && value.limb[value.size - 1u] == 0u; --value.size)
		{
		}
	}

	template <::std::size_t limb_capacity>
	inline constexpr void scan_decfloat_bigint_set_u64(scan_decfloat_bigint<limb_capacity> &value,
													   ::std::uint_least64_t word) noexcept
	{
		value.limb[0] = word;
		value.size = word == 0u ? 0u : 1u;
	}

	// Let B=2^64 and x*y=p_low+B*p_high.  For an incoming carry c<B,
	// (p_low+c) mod B is the next limb and p_high+[p_low+c>=B] is the next
	// carry.  Moreover x*y+c <= (B-1)^2+(B-1)=B^2-B, so that high-word
	// addition cannot overflow.  This proves exact equivalence to the former
	// native-u128 multiply-add recurrence.
	[[nodiscard]] inline constexpr ::std::uint_least64_t
	scan_decfloat_bigint_mul_add_carry(::std::uint_least64_t left,
									 ::std::uint_least64_t right,
									 ::std::uint_least64_t &carry) noexcept
	{
		::std::uint_least64_t product_high{};
		auto product_low{::fast_io::intrinsics::umul(left, right, product_high)};
		auto const low_before_carry{product_low};
		product_low = static_cast<::std::uint_least64_t>(product_low + carry);
		product_high = static_cast<::std::uint_least64_t>(
			product_high + static_cast<::std::uint_least64_t>(product_low < low_before_carry));
		carry = product_high;
		return product_low;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_mul_small(scan_decfloat_bigint<limb_capacity> &value,
																	   ::std::uint_least32_t multiplier) noexcept
	{
		::std::uint_least64_t carry{};
		for (::std::size_t index{}; index != value.size; ++index)
		{
			value.limb[index] = ::fast_io::details::scan_decfloat_bigint_mul_add_carry(
				value.limb[index], static_cast<::std::uint_least64_t>(multiplier), carry);
		}
		if (carry)
		{
			if (value.size == limb_capacity)
			{
				return false;
			}
			value.limb[value.size] = static_cast<::std::uint_least64_t>(carry);
			++value.size;
		}
		return true;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_mul_u64(scan_decfloat_bigint<limb_capacity> &value,
																	 ::std::uint_least64_t multiplier) noexcept
	{
		if (!multiplier || !value.size)
		{
			::fast_io::details::scan_decfloat_bigint_clear(value);
			return true;
		}
		::std::uint_least64_t carry{};
		for (::std::size_t index{}; index != value.size; ++index)
		{
			value.limb[index] = ::fast_io::details::scan_decfloat_bigint_mul_add_carry(
				value.limb[index], multiplier, carry);
		}
		if (carry)
		{
			if (value.size == limb_capacity)
			{
				return false;
			}
			value.limb[value.size] = static_cast<::std::uint_least64_t>(carry);
			++value.size;
		}
		return true;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_add_small(scan_decfloat_bigint<limb_capacity> &value,
																	   ::std::uint_least32_t addend) noexcept
	{
		if (!value.size)
		{
			::fast_io::details::scan_decfloat_bigint_set_u64(value, addend);
			return true;
		}
		auto const first_before_add{value.limb[0]};
		value.limb[0] = static_cast<::std::uint_least64_t>(first_before_add + addend);
		bool carry{value.limb[0] < first_before_add};
		for (::std::size_t index{1u}; carry && index != value.size; ++index)
		{
			++value.limb[index];
			carry = value.limb[index] == 0u;
		}
		if (carry)
		{
			if (value.size == limb_capacity)
			{
				return false;
			}
			value.limb[value.size] = static_cast<::std::uint_least64_t>(carry);
			++value.size;
		}
		return true;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_add_u64(scan_decfloat_bigint<limb_capacity> &value,
																	 ::std::uint_least64_t addend) noexcept
	{
		if (!addend)
		{
			return true;
		}
		if (!value.size)
		{
			::fast_io::details::scan_decfloat_bigint_set_u64(value, addend);
			return true;
		}
		auto const first_before_add{value.limb[0]};
		value.limb[0] = static_cast<::std::uint_least64_t>(first_before_add + addend);
		bool carry{value.limb[0] < first_before_add};
		for (::std::size_t index{1u}; carry && index != value.size; ++index)
		{
			++value.limb[index];
			carry = value.limb[index] == 0u;
		}
		if (carry)
		{
			if (value.size == limb_capacity)
			{
				return false;
			}
			value.limb[value.size] = static_cast<::std::uint_least64_t>(carry);
			++value.size;
		}
		return true;
	}

	template <::std::size_t limb_capacity, typename T>
	inline constexpr bool scan_decfloat_bigint_from_digits(scan_decfloat_bigint<limb_capacity> &value,
														   scan_decfloat_significand_state<T> const &state) noexcept
	{
		::fast_io::details::scan_decfloat_bigint_clear(value);
		constexpr ::std::uint_least64_t chunk_digits{19u};
		constexpr ::std::uint_least64_t chunk_power{10000000000000000000ull};
		::std::uint_least64_t chunk{};
		::std::uint_least64_t count{};
		for (::std::size_t index{}; index != state.exact_stored_digits; ++index)
		{
			chunk = chunk * 10u + static_cast<::std::uint_least64_t>(state.exact_digits[index]);
			++count;
			if (count == chunk_digits)
			{
				if (!::fast_io::details::scan_decfloat_bigint_mul_u64(value, chunk_power) ||
					!::fast_io::details::scan_decfloat_bigint_add_u64(value, chunk))
				{
					return false;
				}
				chunk = 0;
				count = 0;
			}
		}
		if (count)
		{
			if (!::fast_io::details::scan_decfloat_bigint_mul_u64(
					value, ::fast_io::details::print_rsv_fp_pow10_0_to_19_table[count]) ||
				!::fast_io::details::scan_decfloat_bigint_add_u64(value, chunk))
			{
				return false;
			}
		}
		return true;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr ::std::size_t
	scan_decfloat_bigint_bit_width(scan_decfloat_bigint<limb_capacity> const &value) noexcept
	{
		if (!value.size)
		{
			return 0u;
		}
		return (value.size - 1u) * 64u +
			   static_cast<::std::size_t>(::std::bit_width(value.limb[value.size - 1u]));
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_get_bit(scan_decfloat_bigint<limb_capacity> const &value,
																	 ::std::size_t bit) noexcept
	{
		auto const limb_index{bit / 64u};
		if (limb_index >= value.size)
		{
			return false;
		}
		return ((value.limb[limb_index] >> (bit % 64u)) & 1u) != 0u;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool
	scan_decfloat_bigint_any_bits_below(
		scan_decfloat_bigint<limb_capacity> const &value,
		::std::size_t bit_limit) noexcept
	{
		auto const whole_limbs{bit_limit / 64u};
		auto const checked_limbs{
			whole_limbs < value.size ? whole_limbs : value.size};
		for (::std::size_t index{}; index != checked_limbs; ++index)
		{
			if (value.limb[index] != 0u)
			{
				return true;
			}
		}
		auto const remaining_bits{bit_limit % 64u};
		if (remaining_bits && whole_limbs < value.size)
		{
			auto const mask{
				(::std::uint_least64_t{1u} << remaining_bits) - 1u};
			return (value.limb[whole_limbs] & mask) != 0u;
		}
		return false;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr int scan_decfloat_bigint_compare(scan_decfloat_bigint<limb_capacity> const &left,
																	scan_decfloat_bigint<limb_capacity> const &right) noexcept
	{
		if (left.size != right.size)
		{
			return left.size < right.size ? -1 : 1;
		}
		for (auto index{left.size}; index != 0u;)
		{
			--index;
			if (left.limb[index] != right.limb[index])
			{
				return left.limb[index] < right.limb[index] ? -1 : 1;
			}
		}
		return 0;
	}

	template <::std::size_t limb_capacity>
	inline constexpr void scan_decfloat_bigint_sub_assign(scan_decfloat_bigint<limb_capacity> &left,
														  scan_decfloat_bigint<limb_capacity> const &right) noexcept
	{
		::std::uint_least64_t borrow{};
		for (::std::size_t index{}; index != left.size; ++index)
		{
			auto const subtrahend{index < right.size ? right.limb[index] : ::std::uint_least64_t{}};
			auto const old{left.limb[index]};
			auto const with_borrow{static_cast<::std::uint_least64_t>(subtrahend + borrow)};
			left.limb[index] = static_cast<::std::uint_least64_t>(old - with_borrow);
			borrow = old < with_borrow || (borrow && with_borrow == 0u);
		}
		::fast_io::details::scan_decfloat_bigint_normalize(left);
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_shl1_add_bit(scan_decfloat_bigint<limb_capacity> &value,
																		  bool bit) noexcept
	{
		::std::uint_least64_t carry{static_cast<::std::uint_least64_t>(bit)};
		for (::std::size_t index{}; index != value.size; ++index)
		{
			auto const next_carry{value.limb[index] >> 63u};
			value.limb[index] = static_cast<::std::uint_least64_t>((value.limb[index] << 1u) | carry);
			carry = next_carry;
		}
		if (carry)
		{
			if (value.size == limb_capacity)
			{
				return false;
			}
			value.limb[value.size] = carry;
			++value.size;
		}
		else if (bit && !value.size)
		{
			value.limb[0] = 1u;
			value.size = 1u;
		}
		return true;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_shift_left(scan_decfloat_bigint<limb_capacity> &out,
																		scan_decfloat_bigint<limb_capacity> const &in,
																		::std::size_t shift) noexcept
	{
		::fast_io::details::scan_decfloat_bigint_clear(out);
		if (!in.size)
		{
			return true;
		}
		auto const limb_shift{shift / 64u};
		auto const bit_shift{shift % 64u};
		if (in.size + limb_shift + (bit_shift != 0u) >
			limb_capacity)
		{
			return false;
		}
		for (::std::size_t index{}; index != limb_shift; ++index)
		{
			out.limb[index] = 0u;
		}
		::std::uint_least64_t carry{};
		for (::std::size_t index{}; index != in.size; ++index)
		{
			auto const word{in.limb[index]};
			out.limb[index + limb_shift] = static_cast<::std::uint_least64_t>((word << bit_shift) | carry);
			carry = bit_shift == 0u ? 0u : static_cast<::std::uint_least64_t>(word >> (64u - bit_shift));
		}
		out.size = in.size + limb_shift;
		if (carry)
		{
			out.limb[out.size] = carry;
			++out.size;
		}
		::fast_io::details::scan_decfloat_bigint_normalize(out);
		return true;
	}

	template <::std::size_t limb_capacity>
	inline constexpr void
	scan_decfloat_bigint_shift_right_one_inplace(scan_decfloat_bigint<limb_capacity> &value) noexcept
	{
		::std::uint_least64_t carry{};
		for (auto index{value.size}; index != 0u;)
		{
			--index;
			auto const word{value.limb[index]};
			value.limb[index] = static_cast<::std::uint_least64_t>(
				(word >> 1u) | (carry << 63u));
			carry = word & 1u;
		}
		::fast_io::details::scan_decfloat_bigint_normalize(value);
	}

	template <::std::size_t limb_capacity>
#if __has_cpp_attribute(__gnu__::__always_inline__)
	[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
	[[msvc::forceinline]]
#endif
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_shift_left_inplace(scan_decfloat_bigint<limb_capacity> &value,
																				::std::size_t shift) noexcept
	{
		if (!value.size || !shift)
		{
			return true;
		}
		auto const limb_shift{shift / 64u};
		auto const bit_shift{shift % 64u};
		if (value.size + limb_shift + (bit_shift != 0u) >
			limb_capacity)
		{
			return false;
		}
		if (limb_shift)
		{
			for (auto index{value.size}; index != 0u;)
			{
				--index;
				value.limb[index + limb_shift] = value.limb[index];
			}
			for (::std::size_t index{}; index != limb_shift; ++index)
			{
				value.limb[index] = 0u;
			}
			value.size += limb_shift;
		}
		if (bit_shift)
		{
			::std::uint_least64_t carry{};
			for (::std::size_t index{limb_shift}; index != value.size; ++index)
			{
				auto const word{value.limb[index]};
				value.limb[index] = static_cast<::std::uint_least64_t>((word << bit_shift) | carry);
				carry = static_cast<::std::uint_least64_t>(word >> (64u - bit_shift));
			}
			if (carry)
			{
				value.limb[value.size] = carry;
				++value.size;
			}
		}
		::fast_io::details::scan_decfloat_bigint_normalize(value);
		return true;
	}

	inline constexpr auto scan_decfloat_generate_pow5_0_to_27_table() noexcept
	{
		::fast_io::freestanding::array<::std::uint_least64_t, 28u> table{};
		table.index_unchecked(0u) = 1u;
		for (::std::size_t index{1u}; index != 28u; ++index)
		{
			table.index_unchecked(index) =
				static_cast<::std::uint_least64_t>(table.index_unchecked(index - 1u) * 5u);
		}
		return table;
	}

	// 5^27 = 7,450,580,596,923,828,125 < 2^63, so every multiplication in
	// this constant-initialization recurrence is representable in uint_least64_t.
	// Generation preserves the exact chunk constants without a handwritten table.
	inline constexpr auto scan_decfloat_pow5_0_to_27_table{
		::fast_io::details::scan_decfloat_generate_pow5_0_to_27_table()};

	template <typename T>
	[[nodiscard]] inline constexpr bool
	scan_decfloat_try_exact_dyadic(T &value, bool negative,
								  ::std::uint_least64_t significand,
								  ::std::int_least64_t decimal_exponent) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		if constexpr (!::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
		{
			/*
			The bit construction below targets the at-most-64-bit IEEE field
			model used by the cached converter.  Wide native formats retain their
			existing exact-bigint path; declining this optional shortcut changes
			neither their accepted grammar nor their rounding result.
			*/
			return false;
		}
		else
		{
			using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
			constexpr auto precision_bits{
				static_cast<unsigned>(trait::mbits + 1u)};
			constexpr auto exponent_bias{
				static_cast<::std::int_least32_t>(
					(static_cast<::std::uint_least32_t>(1u)
					 << (trait::ebits - 1u)) -
					1u)};
			constexpr auto minimum_normal_exponent{
				static_cast<::std::int_least32_t>(1 - exponent_bias)};
			constexpr auto maximum_normal_exponent{exponent_bias};

			/*
			M*10^e = (M*5^e)*2^e for e>=0 and
			M*10^-k = (M/5^k)*2^-k exactly when 5^k divides M.  Since
			5^27 fits in uint64_t and 5^28 does not, an unsigned 64-bit M
			can satisfy the negative divisibility case only for k<=27.
			The positive multiplication guard proves that its coefficient is
			also represented exactly.  No floating instruction or ambient
			rounding mode participates in either reduction.
			*/
			if (decimal_exponent < -27 || 27 < decimal_exponent)
			{
				return false;
			}
			::std::uint_least64_t coefficient{significand};
			auto binary_exponent{static_cast<::std::int_least32_t>(
				decimal_exponent)};
			if (decimal_exponent < 0)
			{
				auto const power5{
					::fast_io::details::scan_decfloat_pow5_0_to_27_table[
						static_cast<::std::size_t>(-decimal_exponent)]};
				if (coefficient % power5 != 0u)
				{
					/*
					An uncancelled factor 5 remains in the denominator, so the
					rational is not dyadic and cannot be materialized by an
					exact binary shift.  The cached/exact rounding pipeline must
					decide between adjacent target values.
					*/
					return false;
				}
				coefficient /= power5;
			}
			else
			{
				auto const power5{
					::fast_io::details::scan_decfloat_pow5_0_to_27_table[
						static_cast<::std::size_t>(decimal_exponent)]};
				constexpr auto uint64_max{
					(::std::numeric_limits<::std::uint_least64_t>::max)()};
				if (coefficient > uint64_max / power5)
				{
					/*
					The exact coefficient does not fit the shortcut's carrier.
					Falling through preserves it in the existing 64x128 cached
					product rather than accepting a wrapped integer.
					*/
					return false;
				}
				coefficient *= power5;
			}

			if (coefficient == 0u)
			{
				/*
				The caller normally handles zero before conversion.  Keeping the
				helper total avoids applying count-zero operations outside their
				nonzero domain without creating a second signed-zero policy.
				*/
				return false;
			}
			auto const trailing_zeroes{
				static_cast<unsigned>(::std::countr_zero(coefficient))};
			coefficient >>= trailing_zeroes;
			binary_exponent += static_cast<::std::int_least32_t>(
				trailing_zeroes);
			auto const coefficient_bits{static_cast<unsigned>(
				::std::numeric_limits<::std::uint_least64_t>::digits -
				::std::countl_zero(coefficient))};
			if (precision_bits < coefficient_bits)
			{
				/*
				After removing every factor two, an odd coefficient wider than
				the target precision cannot be represented exactly at any binary
				exponent.  All rounding policies therefore continue through the
				normal cached converter.
				*/
				return false;
			}

			auto const leading_exponent{
				static_cast<::std::int_least32_t>(
					binary_exponent +
					static_cast<::std::int_least32_t>(coefficient_bits) - 1)};
			::fast_io::details::scan_decfloat_adjusted_mantissa adjusted;
			if (minimum_normal_exponent <= leading_exponent &&
				leading_exponent <= maximum_normal_exponent)
			{
				/*
				Shifting the odd coefficient to precision_bits places its leading
				one in the implicit-bit position and appends only zero bits.
				Removing that implicit bit and adding the IEEE bias therefore
				encodes coefficient*2^binary_exponent exactly.
				*/
				auto const normalized{
					coefficient << (precision_bits - coefficient_bits)};
				auto const implicit_bit{
					::std::uint_least64_t{1u} << trait::mbits};
				adjusted.mantissa = normalized & (implicit_bit - 1u);
				adjusted.power2 = leading_exponent + exponent_bias;
			}
			else if (leading_exponent < minimum_normal_exponent)
			{
				constexpr auto subnormal_unit_exponent{
					static_cast<::std::int_least32_t>(
						minimum_normal_exponent -
						static_cast<::std::int_least32_t>(trait::mbits))};
				auto const subnormal_shift{
					static_cast<::std::int_least32_t>(
						binary_exponent - subnormal_unit_exponent)};
				if (subnormal_shift < 0 ||
					static_cast<unsigned>(subnormal_shift) >=
						::std::numeric_limits<::std::uint_least64_t>::digits ||
					trait::mbits <
						coefficient_bits +
							static_cast<unsigned>(subnormal_shift))
				{
					/*
					A negative shift leaves a fractional subnormal quantum; an
					oversized shift leaves the finite subnormal field.  In both
					cases the exact dyadic is not a target value, so rounding
					must remain in the general path.
					*/
					return false;
				}
				adjusted.mantissa =
					coefficient << static_cast<unsigned>(subnormal_shift);
				adjusted.power2 = 0;
			}
			else
			{
				/*
				The exact dyadic lies above the largest finite binade.  Its
				infinity-versus-max-finite result depends on the requested
				rounding policy, which the general converter already implements.
				*/
				return false;
			}
			::fast_io::details::scan_decfloat_to_float(
				negative, adjusted, value);
			return true;
		}
	}

	inline constexpr ::std::uint_least64_t
		scan_decfloat_pow5_anchor_chunk_count{32u};
	inline constexpr ::std::uint_least64_t
		scan_decfloat_pow5_anchor_exponent{
			27u * scan_decfloat_pow5_anchor_chunk_count};

	template <::std::size_t limb_capacity>
	inline constexpr ::std::uint_least64_t
		scan_decfloat_pow5_anchor_maximum_exponent{
			limb_capacity >= 640u ? 5095u : 1075u};

	template <::std::size_t limb_capacity>
	struct scan_decfloat_pow5_anchor_table
	{
		inline static constexpr ::std::size_t extent{
			static_cast<::std::size_t>(
				scan_decfloat_pow5_anchor_maximum_exponent<limb_capacity> /
				scan_decfloat_pow5_anchor_exponent) +
			1u};
		scan_decfloat_bigint<limb_capacity> values[extent]{};

		inline constexpr scan_decfloat_pow5_anchor_table() noexcept
		{
			::fast_io::details::scan_decfloat_bigint_set_u64(values[0u], 1u);
			for (::std::size_t entry{1u}; entry != extent; ++entry)
			{
				::fast_io::details::scan_decfloat_bigint_copy(
					values[entry], values[entry - 1u]);
				for (::std::uint_least64_t chunk{};
					 chunk != scan_decfloat_pow5_anchor_chunk_count; ++chunk)
				{
					(void)::fast_io::details::scan_decfloat_bigint_mul_u64(
						values[entry],
						::fast_io::details::scan_decfloat_pow5_0_to_27_table[27u]);
				}
			}
		}
	};

	template <::std::size_t limb_capacity>
	inline constexpr scan_decfloat_pow5_anchor_table<limb_capacity>
		scan_decfloat_pow5_anchor_table_instance{};

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_mul_pow5(scan_decfloat_bigint<limb_capacity> &value,
																	  ::std::uint_least64_t exponent) noexcept
	{
		/*
		A short decimal coefficient occupies one limb, but binary80/binary128
		can request nearly five thousand powers of five.  Starting from that
		coefficient and multiplying by 5^27 makes every early pass revisit an
		ever-growing accumulator.  The constexpr anchors store every 864th
		power; copying the nearest lower anchor, multiplying it once by the
		coefficient, and finishing at most 31 chunks removes most of that
		quadratic growth.  Six wide anchors occupy 30 KiB, and constexpr storage
		avoids a first-call initialization spike.  Multi-limb decimal prefixes
		retain the general exact recurrence.
		*/
		if (!::std::is_constant_evaluated() && value.size <= 1u &&
			exponent >= ::fast_io::details::scan_decfloat_pow5_anchor_exponent)
		{
			using table_type =
				::fast_io::details::scan_decfloat_pow5_anchor_table<limb_capacity>;
			auto const anchor_index_wide{
				exponent /
				::fast_io::details::scan_decfloat_pow5_anchor_exponent};
			if (anchor_index_wide <
				static_cast<::std::uint_least64_t>(table_type::extent))
			{
				// Compare in the exponent's full domain before narrowing. This ordering is required on 32-bit targets:
				// truncating an adversarial quotient first could make an out-of-range exponent name a valid anchor.
				auto const anchor_index{
					static_cast<::std::size_t>(anchor_index_wide)};
				auto const coefficient{
					value.size ? value.limb[0u] : ::std::uint_least64_t{}};
				::fast_io::details::scan_decfloat_bigint_copy(
					value,
					::fast_io::details::
						scan_decfloat_pow5_anchor_table_instance<limb_capacity>
							.values[anchor_index]);
				if (!::fast_io::details::scan_decfloat_bigint_mul_u64(
						value, coefficient))
				{
					return false;
				}
				exponent %=
					::fast_io::details::scan_decfloat_pow5_anchor_exponent;
			}
		}
		constexpr ::std::uint_least64_t chunk{27u};
		for (; exponent >= chunk; exponent -= chunk)
		{
			if (!::fast_io::details::scan_decfloat_bigint_mul_u64(
					value, ::fast_io::details::scan_decfloat_pow5_0_to_27_table[chunk]))
			{
				return false;
			}
		}
		if (exponent != 0)
		{
			// The loop exit invariant is `0 < exponent < chunk`, and chunk is 27. The explicit conversion therefore
			// preserves the complete index on every supported size_t width and documents the table-bound proof.
			auto const tail_index{static_cast<::std::size_t>(exponent)};
			if (!::fast_io::details::scan_decfloat_bigint_mul_u64(
					value, ::fast_io::details::scan_decfloat_pow5_0_to_27_table[tail_index]))
			{
				return false;
			}
		}
		return true;
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_pow5(scan_decfloat_bigint<limb_capacity> &value,
																  ::std::uint_least64_t exponent) noexcept
	{
		::fast_io::details::scan_decfloat_bigint_set_u64(value, 1u);
		return ::fast_io::details::scan_decfloat_bigint_mul_pow5(value, exponent);
	}

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr ::std::int_least64_t
	scan_decfloat_bigint_floor_log2_ratio(scan_decfloat_bigint<limb_capacity> const &numerator,
										  scan_decfloat_bigint<limb_capacity> const &denominator) noexcept
	{
		auto const numerator_bits{::fast_io::details::scan_decfloat_bigint_bit_width(numerator)};
		auto const denominator_bits{::fast_io::details::scan_decfloat_bigint_bit_width(denominator)};
		auto exponent{static_cast<::std::int_least64_t>(numerator_bits) -
					  static_cast<::std::int_least64_t>(denominator_bits)};
		::fast_io::details::scan_decfloat_bigint<limb_capacity> shifted;
		if (exponent >= 0)
		{
			(void)::fast_io::details::scan_decfloat_bigint_shift_left(
				shifted, denominator, static_cast<::std::size_t>(exponent));
			if (::fast_io::details::scan_decfloat_bigint_compare(numerator, shifted) < 0)
			{
				--exponent;
			}
		}
		else
		{
			(void)::fast_io::details::scan_decfloat_bigint_shift_left(
				shifted, numerator, static_cast<::std::size_t>(-exponent));
			if (::fast_io::details::scan_decfloat_bigint_compare(shifted, denominator) < 0)
			{
				--exponent;
			}
		}
		return exponent;
	}

	/*
	The full exact materializer accumulates p+1 bits: p target significand bits
	plus the possible carry created by rounding.  Native-u128 targets use that
	carrier for binary80/binary128.  A target without native u128 admits only the
	compute-supported formats at this entry; their largest p is binary64's 53,
	so a uint_least64_t contains the complete quotient and its carry.  The exact
	division below explicitly rejects any set quotient bit outside the selected
	carrier, making this a proved width substitution rather than truncation.
	*/
#if defined(__SIZEOF_INT128__)
	using scan_decfloat_quotient_type = __uint128_t;
#else
	using scan_decfloat_quotient_type = ::std::uint_least64_t;
#endif
	inline constexpr ::std::size_t scan_decfloat_quotient_bits{
		::std::numeric_limits<scan_decfloat_quotient_type>::digits};

	struct scan_decfloat_bigint_div_result
	{
		scan_decfloat_quotient_type quotient{};
		int twice_remainder_compare{};
		bool remainder_nonzero{};
		bool quotient_overflow{};
	};

	template <::std::size_t limb_capacity>
	[[nodiscard]] inline constexpr scan_decfloat_bigint_div_result
	scan_decfloat_bigint_div_shifted_to_quotient(
		scan_decfloat_bigint<limb_capacity> const &numerator,
		scan_decfloat_bigint<limb_capacity> const &denominator,
		::std::int_least64_t binary_shift) noexcept
	{
		/*
		The quotient requested by assign_big has only p or p+1 significant
		bits, but the old restoring division visited every bit of the scaled
		dividend.  That is about 16,000 iterations for a finite binary128 value
		near either decimal exponent limit.

		Materialize the power-of-two scale on one side, align the divisor once,
		then extract only the actual quotient bits.  Each subtraction leaves the
		exact remainder, so the final midpoint comparison and every directed
		rounding policy remain unchanged.  The loop is bounded by the quotient
		width (at most 114 useful bits for binary128), independently of the
		decimal exponent.
		*/
		if (denominator.size == 1u && denominator.limb[0] == 1u)
		{
			/*
			Positive decimal exponents leave the exact denominator equal to
			one.  After scale selection their effective divisor is normally a
			large power of two.  Extracting the quotient and classifying the
			discarded low bits is exact and touches each relevant limb once;
			constructing and repeatedly shifting a 5,000-digit aligned divisor
			would be pure overhead.
			*/
			auto const numerator_bits{
				::fast_io::details::scan_decfloat_bigint_bit_width(numerator)};
			scan_decfloat_quotient_type quotient{};
			bool quotient_overflow{};
			if (binary_shift >= 0)
			{
				auto const left_shift{static_cast<::std::size_t>(binary_shift)};
				if (numerator_bits &&
					(numerator_bits > ::fast_io::details::scan_decfloat_quotient_bits ||
					 left_shift >
						 ::fast_io::details::scan_decfloat_quotient_bits - numerator_bits))
				{
					quotient_overflow = true;
				}
				else
				{
					for (::std::size_t bit{}; bit != numerator_bits; ++bit)
					{
						if (::fast_io::details::scan_decfloat_bigint_get_bit(
								numerator, bit))
						{
							quotient |=
								static_cast<scan_decfloat_quotient_type>(1u)
								<< (bit + left_shift);
						}
					}
				}
				return {.quotient = quotient,
						.twice_remainder_compare = -1,
						.remainder_nonzero = false,
						.quotient_overflow = quotient_overflow};
			}

			auto const right_shift{static_cast<::std::size_t>(-binary_shift)};
			auto const quotient_width{
				numerator_bits > right_shift ? numerator_bits - right_shift : 0u};
			if (quotient_width >
				::fast_io::details::scan_decfloat_quotient_bits)
			{
				quotient_overflow = true;
			}
			else
			{
				for (::std::size_t bit{}; bit != quotient_width; ++bit)
				{
					if (::fast_io::details::scan_decfloat_bigint_get_bit(
							numerator, bit + right_shift))
					{
						quotient |=
							static_cast<scan_decfloat_quotient_type>(1u) << bit;
					}
				}
			}
			bool const remainder_nonzero{
				::fast_io::details::scan_decfloat_bigint_any_bits_below(
					numerator, right_shift)};
			int twice_remainder_compare{-1};
			if (right_shift &&
				::fast_io::details::scan_decfloat_bigint_get_bit(
					numerator, right_shift - 1u))
			{
				twice_remainder_compare =
					::fast_io::details::scan_decfloat_bigint_any_bits_below(
						numerator, right_shift - 1u)
						? 1
						: 0;
			}
			return {.quotient = quotient,
					.twice_remainder_compare = twice_remainder_compare,
					.remainder_nonzero = remainder_nonzero,
					.quotient_overflow = quotient_overflow};
		}

		::fast_io::details::scan_decfloat_bigint<limb_capacity> remainder;
		::fast_io::details::scan_decfloat_bigint<limb_capacity> divisor;
		if (binary_shift > 0)
		{
			if (!::fast_io::details::scan_decfloat_bigint_shift_left(
					remainder, numerator, static_cast<::std::size_t>(binary_shift)))
			{
				return {.quotient_overflow = true};
			}
		}
		else
		{
			::fast_io::details::scan_decfloat_bigint_copy(remainder, numerator);
		}
		if (binary_shift < 0)
		{
			if (!::fast_io::details::scan_decfloat_bigint_shift_left(
					divisor, denominator, static_cast<::std::size_t>(-binary_shift)))
			{
				return {.quotient_overflow = true};
			}
		}
		else
		{
			::fast_io::details::scan_decfloat_bigint_copy(divisor, denominator);
		}

		scan_decfloat_quotient_type quotient{};
		bool quotient_overflow{};
		auto const remainder_bits{
			::fast_io::details::scan_decfloat_bigint_bit_width(remainder)};
		auto const divisor_bits{
			::fast_io::details::scan_decfloat_bigint_bit_width(divisor)};
		::fast_io::details::scan_decfloat_bigint<limb_capacity> aligned_divisor;
		if (remainder_bits >= divisor_bits && divisor_bits != 0u)
		{
			auto quotient_bit{remainder_bits - divisor_bits};
			if (!::fast_io::details::scan_decfloat_bigint_shift_left(
					aligned_divisor, divisor, quotient_bit))
			{
				return {.quotient_overflow = true};
			}
			bool quotient_nonzero_possible{true};
			if (::fast_io::details::scan_decfloat_bigint_compare(
					aligned_divisor, remainder) > 0)
			{
				if (quotient_bit == 0u)
				{
					quotient_nonzero_possible = false;
				}
				else
				{
					--quotient_bit;
					::fast_io::details::scan_decfloat_bigint_shift_right_one_inplace(
						aligned_divisor);
				}
			}
			for (; quotient_nonzero_possible;)
			{
				if (::fast_io::details::scan_decfloat_bigint_compare(
						remainder, aligned_divisor) >= 0)
				{
					::fast_io::details::scan_decfloat_bigint_sub_assign(
						remainder, aligned_divisor);
					if (quotient_bit < ::fast_io::details::scan_decfloat_quotient_bits)
					{
						quotient |= static_cast<scan_decfloat_quotient_type>(1u)
									<< quotient_bit;
					}
					else
					{
						quotient_overflow = true;
					}
				}
				if (quotient_bit == 0u)
				{
					break;
				}
				--quotient_bit;
				::fast_io::details::scan_decfloat_bigint_shift_right_one_inplace(
					aligned_divisor);
			}
		}
		::fast_io::details::scan_decfloat_bigint_copy(aligned_divisor, remainder);
		(void)::fast_io::details::scan_decfloat_bigint_shl1_add_bit(aligned_divisor, false);
		return {.quotient = quotient,
				.twice_remainder_compare =
					::fast_io::details::scan_decfloat_bigint_compare(aligned_divisor, divisor),
				.remainder_nonzero = remainder.size != 0u,
				.quotient_overflow = quotient_overflow};
	}

	template <::fast_io::manipulators::floating_rounding rounding>
	[[nodiscard]] inline constexpr bool scan_decfloat_big_round_up(
		bool negative, scan_decfloat_quotient_type quotient,
		int twice_remainder_compare,
		bool remainder_nonzero,
		bool tail_nonzero) noexcept
	{
		if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
		{
			/*
			For the retained lower endpoint let the scaled magnitude be q+r/d,
			0<=r<d.  Comparing 2r with d orders that endpoint against q+1/2.
			The type-dependent prefix contains the complete terminating decimal
			expansion of every binary midpoint.  Hence, if 2r<d, no omitted suffix
			can reach the midpoint: any first differing midpoint digit was already
			retained.  If 2r=d, a nonzero suffix is strictly above equality; if it
			is zero, the input is the exact tie.  Finally 2r>d is already above.
			Consequently every nearest policy returns q below the midpoint and q+1
			above it.  Only exact equality may consult the policy's tie rule.
			Treating every inexact nearest-to-odd input as "make q odd" would instead
			implement round-to-odd jamming and can select the farther neighbour.
			*/
			if (twice_remainder_compare < 0)
			{
				return false;
			}
			if (twice_remainder_compare > 0 || tail_nonzero)
			{
				return true;
			}
			if (!remainder_nonzero)
			{
				return false;
			}
			return ::fast_io::details::floating_rounding_nearest_tie_round_up<rounding>(
				negative, static_cast<::std::uint_least64_t>(quotient) << 1u);
		}
		else
		{
			return (remainder_nonzero || tail_nonzero) &&
				   ::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
		}
	}

	template <typename T>
	[[nodiscard]] inline constexpr ::std::uint_least64_t
	scan_decfloat_adjusted_significand(scan_decfloat_adjusted_mantissa adjusted) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		if (adjusted.power2 == 0)
		{
			return adjusted.mantissa;
		}
		return adjusted.mantissa | (::std::uint_least64_t{1u} << trait::mbits);
	}

	struct scan_decfloat_extended_mantissa
	{
		::std::uint_least64_t mantissa{};
		::std::int_least64_t power2{};
	};

	template <typename T>
	[[nodiscard]] inline constexpr scan_decfloat_extended_mantissa
	scan_decfloat_adjusted_to_extended(scan_decfloat_adjusted_mantissa adjusted) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		constexpr auto bias{static_cast<::std::int_least64_t>(trait::mbits) -
							static_cast<::std::int_least64_t>(
								::fast_io::details::scan_decfloat_minimum_exponent<no_cvref_t>())};
		::fast_io::details::scan_decfloat_extended_mantissa extended;
		if (adjusted.power2 == 0)
		{
			extended.mantissa = adjusted.mantissa;
			extended.power2 = 1 - bias;
		}
		else
		{
			extended.mantissa =
				::fast_io::details::scan_decfloat_adjusted_significand<no_cvref_t>(adjusted);
			extended.power2 = static_cast<::std::int_least64_t>(adjusted.power2) - bias;
		}
		return extended;
	}

	template <typename T>
	[[nodiscard]] inline constexpr scan_decfloat_extended_mantissa
	scan_decfloat_adjusted_to_extended_halfway(scan_decfloat_adjusted_mantissa adjusted) noexcept
	{
		auto extended{
			::fast_io::details::scan_decfloat_adjusted_to_extended<T>(adjusted)};
		extended.mantissa = static_cast<::std::uint_least64_t>((extended.mantissa << 1u) | 1u);
		--extended.power2;
		return extended;
	}

	template <typename T>
	[[nodiscard]] inline constexpr bool
	scan_decfloat_compare_exact_to_extended(
		scan_decfloat_significand_state<T> const &state,
		::std::int_least64_t decimal_exponent,
		scan_decfloat_extended_mantissa boundary,
		int &order) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		if constexpr (!::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
		{
			return false;
		}
		else
		{
			::fast_io::details::scan_decfloat_bigint_for<no_cvref_t> real_digits;
			if (!::fast_io::details::scan_decfloat_bigint_from_digits(real_digits, state))
			{
				return false;
			}
			::fast_io::details::scan_decfloat_bigint_for<no_cvref_t> theor_digits;
			::fast_io::details::scan_decfloat_bigint_set_u64(
				theor_digits, boundary.mantissa);
			if (decimal_exponent < 0)
			{
				if (!::fast_io::details::scan_decfloat_bigint_mul_pow5(
						theor_digits,
						static_cast<::std::uint_least64_t>(-decimal_exponent)))
				{
					return false;
				}
			}
			else if (decimal_exponent > 0)
			{
				if (!::fast_io::details::scan_decfloat_bigint_mul_pow5(
						real_digits,
						static_cast<::std::uint_least64_t>(decimal_exponent)))
				{
					return false;
				}
			}
			auto const pow2_exponent{boundary.power2 - decimal_exponent};
			if (pow2_exponent > 0)
			{
				if (!::fast_io::details::scan_decfloat_bigint_shift_left_inplace(
						theor_digits, static_cast<::std::size_t>(pow2_exponent)))
				{
					return false;
				}
			}
			else if (pow2_exponent < 0)
			{
				if (!::fast_io::details::scan_decfloat_bigint_shift_left_inplace(
						real_digits, static_cast<::std::size_t>(-pow2_exponent)))
				{
					return false;
				}
			}
			order = ::fast_io::details::scan_decfloat_bigint_compare(
				real_digits, theor_digits);
			if (order == 0 && state.exact_truncated_nonzero)
			{
				/*
				The stored decimal is the lower endpoint of the omitted suffix.
				A nonzero omitted digit makes the complete input strictly larger.
				The exact-digit capacity theorem guarantees that a terminating
				binary boundary cannot first differ beyond this prefix.
				*/
				order = 1;
			}
			return true;
		}
	}

	template <typename T, ::fast_io::manipulators::floating_rounding rounding>
	[[nodiscard]] inline constexpr bool
	scan_decfloat_try_interval_compare(T &value, bool negative,
									   scan_decfloat_significand_state<T> const &state,
									   ::std::int_least64_t decimal_exponent,
									   scan_decfloat_adjusted_mantissa lower,
									   scan_decfloat_adjusted_mantissa upper,
									   ::fast_io::parse_code &code) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		if constexpr (!::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
		{
			return false;
		}
		else
		{
			int order{};
			::fast_io::details::scan_decfloat_adjusted_mantissa selected;
			if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
			{
				auto const halfway{
					::fast_io::details::scan_decfloat_adjusted_to_extended_halfway<no_cvref_t>(
						lower)};
				if (!::fast_io::details::scan_decfloat_compare_exact_to_extended(
						state, decimal_exponent, halfway, order))
				{
					return false;
				}
				auto const lower_significand{
					::fast_io::details::scan_decfloat_adjusted_significand<no_cvref_t>(
						lower)};
				bool const select_upper{
					order > 0 ||
					(order == 0 &&
					 ::fast_io::details::floating_rounding_nearest_tie_round_up<rounding>(
						 negative,
						 static_cast<::std::uint_least64_t>(
							 (lower_significand << 1u) | 1u)))};
				selected = select_upper ? upper : lower;
			}
			else
			{
				bool const round_up{
					::fast_io::details::floating_rounding_directed_round_up<rounding>(
						negative)};
				auto const boundary{
					::fast_io::details::scan_decfloat_adjusted_to_extended<no_cvref_t>(
						round_up ? lower : upper)};
				if (!::fast_io::details::scan_decfloat_compare_exact_to_extended(
						state, decimal_exponent, boundary, order))
				{
					return false;
				}
				/*
				For magnitude-up rounding, lower is the first representable
				value not below the decimal interval's lower endpoint; it remains
				the answer through exact equality.  For magnitude-down rounding,
				upper is the last representable value not above the interval's
				upper endpoint and wins from exact equality onward.
				*/
				selected = round_up ? (order > 0 ? upper : lower)
									: (order >= 0 ? upper : lower);
			}
			::fast_io::details::scan_decfloat_to_float(negative, selected, value);
			code = ((state.significand != 0 && selected.mantissa == 0 && selected.power2 == 0) ||
					selected.power2 == ::fast_io::details::scan_decfloat_infinite_power<no_cvref_t>())
					   ? ::fast_io::parse_code::overflow
					   : ::fast_io::parse_code::ok;
			return true;
		}
	}

	template <typename T>
	inline constexpr void scan_decfloat_assign_min_subnormal(T &value, bool negative) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr auto mbits{trait::mbits};
		constexpr auto ebits{trait::ebits};
		if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
		{
			::fast_io::details::fp_assign_float80_bits(value, 1u, 0u, negative);
		}
		else if constexpr (
			::fast_io::details::fp_floating_point_is_ibm_double_double<no_cvref_t>)
		{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
			(void)::fast_io::details::fp_assign_ibm_double_double_significand(
				value, 1u, -1074, negative);
#endif
		}
		else
		{
			mantissa_type bits{1u};
			if (negative)
			{
				bits |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << (mbits + ebits));
			}
			::fast_io::details::fp_assign_bits(value, bits);
		}
	}

	template <typename T>
	inline constexpr void scan_decfloat_assign_max_finite(T &value, bool negative) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr auto mbits{trait::mbits};
		constexpr auto ebits{trait::ebits};
		if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
		{
			::fast_io::details::fp_assign_float80_bits(value, static_cast<::std::uint_least64_t>(~::std::uint_least64_t{}),
													   (static_cast<::std::uint_least32_t>(1u) << ebits) - 2u,
													   negative);
		}
		else if constexpr (
			::fast_io::details::fp_floating_point_is_ibm_double_double<no_cvref_t>)
		{
			value = negative
				? -(::std::numeric_limits<no_cvref_t>::max)()
				: (::std::numeric_limits<no_cvref_t>::max)();
		}
		else
		{
			constexpr mantissa_type exponent{(static_cast<mantissa_type>(1u) << ebits) - 2u};
			constexpr mantissa_type fraction{(static_cast<mantissa_type>(1u) << mbits) - 1u};
			mantissa_type bits{static_cast<mantissa_type>((exponent << mbits) | fraction)};
			if (negative)
			{
				bits |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << (mbits + ebits));
			}
			::fast_io::details::fp_assign_bits(value, bits);
		}
	}

	template <::fast_io::manipulators::floating_rounding rounding, typename T>
	inline constexpr ::fast_io::parse_code scan_decfloat_assign_overflow_value(T &value, bool negative) noexcept
	{
		bool assign_max_finite{};
		if constexpr (rounding == ::fast_io::manipulators::floating_rounding::toward_zero)
		{
			assign_max_finite = true;
		}
		else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::toward_plus_infinity)
		{
			assign_max_finite = negative;
		}
		else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::toward_minus_infinity)
		{
			assign_max_finite = !negative;
		}
		if (assign_max_finite)
		{
			::fast_io::details::scan_decfloat_assign_max_finite(value, negative);
		}
		else
		{
			::fast_io::details::fp_assign_infinity(value, negative);
		}
		return ::fast_io::parse_code::overflow;
	}

	/*
	Assigning an arbitrary decimal uses the exact limb numerator/denominator of
	its retained lower endpoint and the midpoint-capacity theorem proved at the
	state definition to classify any omitted suffix.  The resulting endpoint
	quotient/remainder plus sticky bit is therefore sufficient for the exact
	rounding decision.  On no-u128 targets this path is instantiated only for
	compute-supported formats, for which p+1<=54<=64; native wide targets retain
	scan_decfloat_assign_native_wide.  Consequently an arithmetic ambiguity never
	escapes as lexical `partial`, and every explicit rounding policy reaches the
	same decision on MSVC.
	*/
	template <typename T, ::fast_io::manipulators::floating_rounding rounding =
							  ::fast_io::manipulators::floating_rounding::nearest_to_even>
	[[nodiscard]] inline constexpr ::fast_io::parse_code
	scan_decfloat_assign_big(T &value, bool negative, scan_decfloat_significand_state<T> const &state,
							 ::std::int_least64_t exponent) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr ::std::size_t mbits{trait::mbits};
		constexpr ::std::size_t ebits{trait::ebits};
		constexpr ::std::size_t precision_bits{mbits + 1u};
#if !defined(__SIZEOF_INT128__)
		static_assert(
			::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>);
		static_assert(precision_bits + 1u <=
					  ::fast_io::details::scan_decfloat_quotient_bits);
#endif
		constexpr auto bias{
			static_cast<::std::int_least64_t>((static_cast<::std::uint_least32_t>(1u) << ebits) >> 1u) - 1};
		constexpr auto min_exponent{[]() constexpr noexcept
		{
			if constexpr (::fast_io::details::
				fp_floating_point_is_ibm_double_double<no_cvref_t>)
			{
				return static_cast<::std::int_least64_t>(
					::std::numeric_limits<no_cvref_t>::min_exponent - 1);
			}
			else
			{
				return static_cast<::std::int_least64_t>(1 - bias);
			}
		}()};
		constexpr auto max_exponent{[]() constexpr noexcept
		{
			if constexpr (::fast_io::details::
				fp_floating_point_is_ibm_double_double<no_cvref_t>)
			{
				return static_cast<::std::int_least64_t>(
					::std::numeric_limits<no_cvref_t>::max_exponent - 1);
			}
			else
			{
				/*
				For an IEC field with E exponent bits the bias is
				B=2^(E-1)-1 and the largest finite unbiased exponent is B.
				This is the exact inverse of the encoded exponent (2^E-2);
				it depends only on the representation already proved by
				scan_decfloat_layout_supported.

				Do not query numeric_limits here.  Compiler extension types may
				have a complete iec559_traits specialization while the selected
				C++ standard library does not specialize numeric_limits for that
				frontend.  In particular Clang with libstdc++ reports
				numeric_limits<__float128>::max_exponent==0, which would turn
				every value at exponent zero (for example 1.25) into a false
				overflow.  Substitution of B is identical for every standard
				specialization and keeps the arithmetic independent of frontend
				library declarations.
				*/
				return bias;
			}
		}()};
		if constexpr (rounding == ::fast_io::manipulators::floating_rounding::current_environment)
		{
			switch (::fast_io::details::current_floating_rounding())
			{
			case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
				return ::fast_io::details::scan_decfloat_assign_big<
					T, ::fast_io::manipulators::floating_rounding::toward_plus_infinity>(
					value, negative, state, exponent);
			case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
				return ::fast_io::details::scan_decfloat_assign_big<
					T, ::fast_io::manipulators::floating_rounding::toward_minus_infinity>(
					value, negative, state, exponent);
			case ::fast_io::manipulators::floating_rounding::toward_zero:
				return ::fast_io::details::scan_decfloat_assign_big<
					T, ::fast_io::manipulators::floating_rounding::toward_zero>(
					value, negative, state, exponent);
			default:
				return ::fast_io::details::scan_decfloat_assign_big<
					T, ::fast_io::manipulators::floating_rounding::nearest_to_even>(
					value, negative, state, exponent);
			}
		}
		else
		{
			if (!state.exact_stored_digits)
			{
				value = negative ? -static_cast<no_cvref_t>(0.0) : static_cast<no_cvref_t>(0.0);
				return ::fast_io::parse_code::ok;
			}
			auto decimal_exponent{::fast_io::details::scan_decfloat_saturating_add(
				exponent, -static_cast<::std::int_least64_t>(state.fractional_digits))};
			decimal_exponent = ::fast_io::details::scan_decfloat_saturating_add(
				decimal_exponent,
				static_cast<::std::int_least64_t>(state.significant_digits - state.exact_stored_digits));
			auto const decimal_top_exponent{::fast_io::details::scan_decfloat_saturating_add(
				decimal_exponent, static_cast<::std::int_least64_t>(state.exact_stored_digits - 1u))};
			constexpr auto high_guard{static_cast<::std::int_least64_t>(trait::e10max + 2u)};
			constexpr auto low_guard{static_cast<::std::int_least64_t>(trait::e10max + trait::m10digits + 16u)};
			if (decimal_top_exponent > high_guard)
			{
				return ::fast_io::details::scan_decfloat_assign_overflow_value<rounding>(value, negative);
			}
			if (decimal_top_exponent < -low_guard)
			{
				if constexpr (!::fast_io::details::floating_rounding_is_nearest<rounding>)
				{
					if (state.has_nonzero_digit &&
						::fast_io::details::floating_rounding_directed_round_up<rounding>(negative))
					{
						::fast_io::details::scan_decfloat_assign_min_subnormal(value, negative);
						return ::fast_io::parse_code::overflow;
					}
				}
				value = negative ? -static_cast<no_cvref_t>(0.0) : static_cast<no_cvref_t>(0.0);
				return ::fast_io::parse_code::overflow;
			}

			::fast_io::details::scan_decfloat_bigint_for<no_cvref_t> numerator;
			if (!::fast_io::details::scan_decfloat_bigint_from_digits(numerator, state))
			{
				return ::fast_io::details::scan_decfloat_assign_overflow_value<rounding>(value, negative);
			}
			::fast_io::details::scan_decfloat_bigint_for<no_cvref_t> denominator;
			::fast_io::details::scan_decfloat_bigint_set_u64(denominator, 1u);
			::std::int_least64_t binary_exponent_adjust{};
			if (decimal_exponent >= 0)
			{
				if (!::fast_io::details::scan_decfloat_bigint_mul_pow5(
						numerator, static_cast<::std::uint_least64_t>(decimal_exponent)))
				{
					return ::fast_io::details::scan_decfloat_assign_overflow_value<rounding>(value, negative);
				}
				binary_exponent_adjust = decimal_exponent;
			}
			else
			{
				auto const pow5_exponent{static_cast<::std::uint_least64_t>(-decimal_exponent)};
				if (!::fast_io::details::scan_decfloat_bigint_pow5(denominator, pow5_exponent))
				{
					value = negative ? -static_cast<no_cvref_t>(0.0) : static_cast<no_cvref_t>(0.0);
					return ::fast_io::parse_code::overflow;
				}
				binary_exponent_adjust = decimal_exponent;
			}

			::std::int_least64_t binary_exponent{};
			if (decimal_exponent >= 0)
			{
				binary_exponent = static_cast<::std::int_least64_t>(
									  ::fast_io::details::scan_decfloat_bigint_bit_width(numerator)) -
								  1 + binary_exponent_adjust;
			}
			else
			{
				binary_exponent =
					::fast_io::details::scan_decfloat_bigint_floor_log2_ratio(numerator, denominator) +
					binary_exponent_adjust;
			}

			bool subnormal{};
			::std::int_least64_t target_exponent{binary_exponent};
			::std::int_least64_t scale_exponent{};
			if (binary_exponent >= min_exponent)
			{
				scale_exponent = binary_exponent - static_cast<::std::int_least64_t>(precision_bits - 1u);
			}
			else
			{
				subnormal = true;
				target_exponent = min_exponent;
				scale_exponent = min_exponent - static_cast<::std::int_least64_t>(precision_bits - 1u);
			}

			auto const division{
				::fast_io::details::scan_decfloat_bigint_div_shifted_to_quotient(
					numerator, denominator, binary_exponent_adjust - scale_exponent)};
			if (division.quotient_overflow)
			{
				return ::fast_io::details::scan_decfloat_assign_overflow_value<rounding>(value, negative);
			}
			auto significand{division.quotient};
			if (::fast_io::details::scan_decfloat_big_round_up<rounding>(
					negative, significand, division.twice_remainder_compare,
					division.remainder_nonzero, state.exact_truncated_nonzero))
			{
				++significand;
			}
			auto const hidden_bit{
				static_cast<::fast_io::details::scan_decfloat_quotient_type>(1u)
				<< mbits};
			auto const carry_bit{hidden_bit << 1u};
			if (!subnormal)
			{
				if (significand >= carry_bit)
				{
					significand >>= 1u;
					++target_exponent;
				}
				if (target_exponent > max_exponent)
				{
					return ::fast_io::details::scan_decfloat_assign_overflow_value<rounding>(value, negative);
				}
				if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
				{
					::fast_io::details::fp_assign_float80_bits(
						value, static_cast<::std::uint_least64_t>(significand),
						static_cast<::std::uint_least32_t>(target_exponent + bias), negative);
				}
				else if constexpr (
					::fast_io::details::fp_floating_point_is_ibm_double_double<no_cvref_t>)
				{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
					/*
					Before carry, scale_exponent equals target_exponent-(p-1).
					If rounding produces 2^p, the branch above replaces it by
					2^(p-1) and increments target_exponent.  Reusing the old scale
					would therefore divide the result by two.  Deriving the dyadic
					scale from the final exponent preserves the invariant

					  value = significand * 2^(target_exponent-(p-1))

					in both the carry and no-carry cases; the component bridge then
					encodes exactly that already-rounded dyadic.
					*/
					if (!::fast_io::details::fp_assign_ibm_double_double_significand(
							value, static_cast<__uint128_t>(significand),
							static_cast<::std::int_least32_t>(target_exponent -
								static_cast<::std::int_least64_t>(mbits)), negative))
					{
						return ::fast_io::details::scan_decfloat_assign_overflow_value<rounding>(
							value, negative);
					}
#endif
				}
				else
				{
					auto fraction{static_cast<mantissa_type>(significand - hidden_bit)};
					auto bits{static_cast<mantissa_type>(
						(static_cast<mantissa_type>(static_cast<::std::uint_least64_t>(target_exponent + bias)) << mbits) |
						fraction)};
					if (negative)
					{
						bits |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << (mbits + ebits));
					}
					::fast_io::details::fp_assign_bits(value, bits);
				}
				return ::fast_io::parse_code::ok;
			}
			if (!significand)
			{
				value = negative ? -static_cast<no_cvref_t>(0.0) : static_cast<no_cvref_t>(0.0);
				return ::fast_io::parse_code::overflow;
			}
			if (significand >= hidden_bit)
			{
				if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
				{
					::fast_io::details::fp_assign_float80_bits(value, static_cast<::std::uint_least64_t>(hidden_bit),
															   1u, negative);
				}
				else if constexpr (
					::fast_io::details::fp_floating_point_is_ibm_double_double<no_cvref_t>)
				{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
					(void)::fast_io::details::fp_assign_ibm_double_double_significand(
						value, static_cast<__uint128_t>(hidden_bit),
						static_cast<::std::int_least32_t>(scale_exponent), negative);
#endif
				}
				else
				{
					mantissa_type bits{static_cast<mantissa_type>(mantissa_type{1u} << mbits)};
					if (negative)
					{
						bits |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << (mbits + ebits));
					}
					::fast_io::details::fp_assign_bits(value, bits);
				}
			}
			else if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
			{
				::fast_io::details::fp_assign_float80_bits(
					value, static_cast<::std::uint_least64_t>(significand), 0u, negative);
			}
			else if constexpr (
				::fast_io::details::fp_floating_point_is_ibm_double_double<no_cvref_t>)
			{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
				(void)::fast_io::details::fp_assign_ibm_double_double_significand(
					value, static_cast<__uint128_t>(significand),
					static_cast<::std::int_least32_t>(scale_exponent), negative);
#endif
			}
			else
			{
				auto bits{static_cast<mantissa_type>(significand)};
				if (negative)
				{
					bits |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << (mbits + ebits));
				}
				::fast_io::details::fp_assign_bits(value, bits);
			}
			return ::fast_io::parse_code::ok;
		}
	}

	template <typename T>
	inline constexpr void scan_decfloat_state_from_u64(scan_decfloat_significand_state<T> &state,
													   ::std::uint_least64_t significand) noexcept
	{
		auto const original{significand};
		char8_t buffer[20];
		::std::size_t size{};
		for (; significand; significand /= 10u)
		{
			buffer[size] = static_cast<char8_t>(significand % 10u);
			++size;
		}
		state.has_digit = true;
		state.has_nonzero_digit = size != 0u;
		state.significant_digits = size;
		state.stored_digits = size;
		state.significand = original;
		for (auto index{size}; index != 0u;)
		{
			--index;
			::fast_io::details::scan_decfloat_append_exact_digit(state, buffer[index]);
		}
	}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool
scan_decfloat_decimal_round_up(bool negative, ::std::uint_least64_t rounded_down,
							   ::std::uint_least64_t remainder, ::std::uint_least64_t divisor,
							   bool tail_nonzero) noexcept
{
	/*
	For retained decimal integer q, the discarded suffix represents a
	nonnegative fraction. An empty suffix is exact and cannot change q under
	any rounding policy; this branch also prevents a directed mode from moving
	an already representable value.
	*/
	if (!remainder && !tail_nonzero)
	{
		return false;
	}
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		/*
		The common nearest decision follows from comparing the discarded
		fraction with 1/2. A nonzero tail after an exactly-half retained
		remainder makes the full suffix strictly greater than one half. Parity
		(or sign for another tie convention) matters only in the exact-half
		branch; this is the defining distinction between nearest-to-odd and
		round-to-odd jamming.
		*/
		auto const half{divisor >> 1u};
		if (remainder < half)
		{
			return false;
		}
		if (half < remainder || tail_nonzero)
		{
			return true;
		}
		return ::fast_io::details::print_rsv_fp_decimal_tie_round_up<rounding>(negative, rounded_down);
	}
	else
	{
		return ::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
	}
}

template <typename T>
inline constexpr void scan_decfloat_append_digit(scan_decfloat_significand_state<T> &state, char8_t digit,
												 bool after_decimal) noexcept
{
	constexpr auto digit_limit{::fast_io::details::scan_decfloat_significand_digit_limit<T>};
	state.has_digit = true;
	if (after_decimal)
	{
		++state.fractional_digits;
	}
	if (!state.has_nonzero_digit)
	{
		if (digit == 0)
		{
			return;
		}
		state.has_nonzero_digit = true;
	}
	++state.significant_digits;
	::fast_io::details::scan_decfloat_append_exact_digit(state, digit);
	if (state.stored_digits != digit_limit)
	{
		state.significand = state.significand * 10u + static_cast<::std::uint_least64_t>(digit);
		++state.stored_digits;
	}
	else if (digit != 0)
	{
		state.truncated_nonzero = true;
	}
}

[[nodiscard]] inline constexpr bool
scan_decfloat_ascii8_is_digits(::std::uint_least64_t val) noexcept
{
	/*
	Interpret val as eight little-endian unsigned bytes c_i.  For an ASCII
	digit, c_i-0x30 is in [0,9] and c_i+0x46 is in [0x76,0x7f], so neither
	operation sets that lane's high bit or propagates a borrow/carry.

	If some lane is invalid, choose the least-significant invalid lane; every
	earlier lane is a digit and therefore supplies no incoming borrow/carry.
	For c_i<0x30, subtraction modulo 256 lies in [0xd0,0xff] and sets bit 7.
	For 0x3a<=c_i<=0xb9, addition lies in [0x80,0xff] and sets bit 7.  For
	c_i>=0xba, subtraction lies in [0x8a,0xcf] and sets bit 7.  Thus the masked
	word is zero iff all eight lanes are digits.  This also proves that whole-
	word carry propagation cannot create a false acceptance.
	*/
	return ((((val + 0x4646464646464646u) | (val - 0x3030303030303030u)) &
			 0x8080808080808080u) == 0);
}

[[nodiscard]] inline constexpr bool
scan_decfloat_ascii8_has_nonzero_digit(::std::uint_least64_t val) noexcept
{
	/*
	The caller first proves every byte is a digit.  XOR with eight ASCII zero
	bytes is therefore zero exactly for the string "00000000"; any nonzero
	digit changes at least one lane and hence the full integer.
	*/
	return (val ^ 0x3030303030303030u) != 0;
}

template <::std::size_t vec_size, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *
scan_decfloat_skip_after_exact_limit_simd(char_type const *first, char_type const *last,
										  bool &tail_nonzero) noexcept
{
	static_assert(sizeof(char_type) == sizeof(char8_t));
	using unsigned_char_type = ::std::make_unsigned_t<::std::remove_cvref_t<char_type>>;
	using signed_char_type = ::std::make_signed_t<unsigned_char_type>;
	constexpr unsigned N{vec_size / sizeof(char_type)};
	using simd_vector_type = ::fast_io::intrinsics::simd_vector<signed_char_type, N>;
	// std::bit_cast preserves every byte of the character array, so both branches
	// construct the same vector on every ISA.  Clang 22 and 23 were additionally
	// verified to lower the constant form to the same x86-64 instructions as
	// load(); Clang 21 and older keep the load fallback because their constexpr
	// vector lowering remains unmeasured.  Admitting later Clang releases and
	// non-Clang frontends is a conservative code-generation hypothesis based on
	// that semantic identity, not a claim that every future target was measured;
	// their emitted objects must be re-audited before relying on performance.
#if (__cpp_lib_bit_cast >= 201806L) && (!defined(__clang__) || __clang_major__ >= 22)
	constexpr simd_vector_type zeroes{
		::std::bit_cast<simd_vector_type>(::fast_io::details::characters_array_impl<u8'0', char_type, N>)};
	constexpr simd_vector_type nines{
		::std::bit_cast<simd_vector_type>(::fast_io::details::characters_array_impl<u8'9', char_type, N>)};
#else
	simd_vector_type zeroes;
	zeroes.load(::fast_io::details::characters_array_impl<u8'0', char_type, N>.data());
	simd_vector_type nines;
	nines.load(::fast_io::details::characters_array_impl<u8'9', char_type, N>.data());
#endif
	for (; N <= static_cast<::std::size_t>(last - first);)
	{
		/*
		The loop guard proves the full N-code-unit load is contained in
		[first,last); no masked or speculative overread is required.
		*/
		simd_vector_type vec;
		vec.load(first);
		/*
		ASCII '0'..'9' are positive signed-byte values.  Lane-wise signed
		comparisons therefore satisfy

		    valid_i <=> ('0' <= vec_i && vec_i <= '9')

		for every possible input byte: negative lanes fail the lower bound and
		bytes above 0x7f also appear negative and fail it.  Conjunction is thus
		the exact digit predicate, not a locale-dependent approximation.
		*/
		auto const valid{(zeroes <= vec) & (vec <= nines)};
		auto const invalid{~valid};
		if (!::fast_io::intrinsics::is_all_zeros(invalid))
		{
			/*
			Comparison masks use an all-zero lane for valid and an all-one lane
			for invalid after complementation.  Counting zero lanes from the
			front yields precisely the index of the first nondigit.  The scalar
			prefix loop visits exactly the accepted lanes and computes their
			sticky OR before returning that boundary.
			*/
			auto const valid_count{::fast_io::intrinsics::vector_mask_countr_zero(invalid)};
			auto const *const invalid_pos{first + valid_count};
			for (; first != invalid_pos; ++first)
			{
				if (!tail_nonzero && *first != ::fast_io::char_literal_v<u8'0', char_type>)
				{
					tail_nonzero = true;
				}
			}
			return first;
		}
		if (!tail_nonzero && !::fast_io::intrinsics::is_all_zeros(vec != zeroes))
		{
			/*
			All lanes are already known digits.  Therefore vec!=zeroes has a
			nonzero lane iff at least one digit is 1..9, exactly the sticky
			predicate needed after the retained exact prefix.
			*/
			tail_nonzero = true;
		}
		first += N;
	}
	return first;
}

template <::std::size_t vec_size, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *
scan_decfloat_skip_after_exact_limit_padding_simd(
	char_type const *first, char_type const *last, bool &tail_nonzero,
	::std::size_t padding) noexcept
{
	/*
	The ordinary SIMD loop owns every complete semantic block.  Only its final
	0<R<N suffix is eligible for the padded leaf, so ordinary scanner ABI and
	loop code generation are completely independent of this protocol.
	*/
	first = ::fast_io::details::scan_decfloat_skip_after_exact_limit_simd<vec_size>(
		first, last, tail_nonzero);
#if ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                  \
	!defined(__AVX512F__) &&                                            \
	(defined(__AVX2__) || defined(__SSE2__))
	static_assert(sizeof(char_type) == sizeof(char8_t));
	constexpr ::std::size_t N{vec_size / sizeof(char_type)};
	auto const remaining{static_cast<::std::size_t>(last - first)};
	if (remaining == 0u || N <= remaining || padding < N - remaining)
	{
		return first;
	}

	using unsigned_char_type =
		::std::make_unsigned_t<::std::remove_cvref_t<char_type>>;
	using signed_char_type = ::std::make_signed_t<unsigned_char_type>;
	using simd_vector_type =
		::fast_io::intrinsics::simd_vector<signed_char_type, N>;
#if defined(__GNUC__) && !defined(__clang__)
	constexpr ::std::size_t minimum_profitable_tail{4u};
#else
	constexpr ::std::size_t minimum_profitable_tail{3u};
#endif
	if (remaining < minimum_profitable_tail)
	{
		/*
		For one to three decimal bytes the scalar cleanup is cheaper than
		materializing a complete vector and extracting its mask.  Returning the
		unchanged semantic cursor leaves that cleanup to the caller.
		*/
		return first;
	}
#if (__cpp_lib_bit_cast >= 201806L) && (!defined(__clang__) || __clang_major__ >= 22)
	constexpr simd_vector_type zeroes{
		::std::bit_cast<simd_vector_type>(
			::fast_io::details::characters_array_impl<u8'0', char_type, N>)};
	constexpr simd_vector_type nines{
		::std::bit_cast<simd_vector_type>(
			::fast_io::details::characters_array_impl<u8'9', char_type, N>)};
#else
	simd_vector_type zeroes;
	zeroes.load(
		::fast_io::details::characters_array_impl<u8'0', char_type, N>.data());
	simd_vector_type nines;
	nines.load(
		::fast_io::details::characters_array_impl<u8'9', char_type, N>.data());
#endif
	simd_vector_type vec;
	vec.load(first);
	auto const invalid{~((zeroes <= vec) & (vec <= nines))};
	auto accepted{
		::fast_io::intrinsics::is_all_zeros(invalid)
			? N
			: static_cast<::std::size_t>(
				  ::fast_io::intrinsics::vector_mask_countr_zero(invalid))};
	if (remaining < accepted)
	{
		accepted = remaining;
	}
	auto const *const stop{first + accepted};
	if (!tail_nonzero)
	{
		auto probe{first};
		for (; probe != stop; ++probe)
		{
			if (*probe !=
				::fast_io::char_literal_v<u8'0', char_type>)
			{
				tail_nonzero = true;
				break;
			}
		}
	}
	first = stop;
#else
	(void)padding;
#endif
	return first;
}

template <typename T, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *scan_decfloat_skip_after_exact_limit_run(
	char_type const *first, char_type const *last, bool after_decimal,
	scan_decfloat_significand_state<T> &state) noexcept
{
	auto const *const original_first{first};
	auto tail_nonzero{state.exact_truncated_nonzero};
	FAST_IO_IF_NOT_CONSTEVAL
	{
		if constexpr (::fast_io::details::is_ascii<char_type> && sizeof(char_type) == sizeof(char8_t) &&
					  ::std::numeric_limits<::std::uint_least64_t>::digits == 64u)
		{
			constexpr auto simd_size{
				::fast_io::intrinsics::optimal_simd_vector_run_with_cpu_instruction_size_with_mask_countr};
			if constexpr (simd_size != 0)
			{
				first = ::fast_io::details::scan_decfloat_skip_after_exact_limit_simd<simd_size>(
					first, last, tail_nonzero);
			}
			for (; static_cast<::std::size_t>(last - first) >= sizeof(::std::uint_least64_t);)
			{
				::std::uint_least64_t val;
				::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));
				if constexpr (::std::endian::little != ::std::endian::native)
				{
					val = ::fast_io::little_endian(val);
				}
				if (!::fast_io::details::scan_decfloat_ascii8_is_digits(val))
				{
					break;
				}
				if (!tail_nonzero && ::fast_io::details::scan_decfloat_ascii8_has_nonzero_digit(val))
				{
					tail_nonzero = true;
				}
				first += sizeof(::std::uint_least64_t);
			}
		}
	}
	char8_t digit{};
	for (; first != last && ::fast_io::details::scan_decfloat_decimal_digit(*first, digit); ++first)
	{
		if (!tail_nonzero && digit != 0)
		{
			tail_nonzero = true;
		}
	}
	auto const skipped{static_cast<::std::uint_least64_t>(first - original_first)};
	if (skipped)
	{
		if (after_decimal)
		{
			state.fractional_digits += skipped;
		}
		state.significant_digits += skipped;
		if (tail_nonzero)
		{
			state.exact_truncated_nonzero = true;
			state.truncated_nonzero = true;
		}
	}
	return first;
}

template <typename T, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *
scan_decfloat_skip_after_exact_limit_padding_run(
	char_type const *first, char_type const *last, bool after_decimal,
	scan_decfloat_significand_state<T> &state, ::std::size_t padding) noexcept
{
	auto const *const original_first{first};
	auto tail_nonzero{state.exact_truncated_nonzero};
	FAST_IO_IF_NOT_CONSTEVAL
	{
		if constexpr (
			::fast_io::details::is_ascii<char_type> &&
			sizeof(char_type) == sizeof(char8_t) &&
			::std::numeric_limits<::std::uint_least64_t>::digits == 64u)
		{
			constexpr auto simd_size{
				::fast_io::intrinsics::
					optimal_simd_vector_run_with_cpu_instruction_size_with_mask_countr};
			if constexpr (simd_size != 0)
			{
				first =
					::fast_io::details::
						scan_decfloat_skip_after_exact_limit_padding_simd<
							simd_size>(first, last, tail_nonzero, padding);
			}
			for (; static_cast<::std::size_t>(last - first) >=
				   sizeof(::std::uint_least64_t);)
			{
				::std::uint_least64_t val;
				::fast_io::freestanding::my_memcpy(
					__builtin_addressof(val), first,
					sizeof(::std::uint_least64_t));
				if constexpr (::std::endian::little != ::std::endian::native)
				{
					val = ::fast_io::little_endian(val);
				}
				if (!::fast_io::details::scan_decfloat_ascii8_is_digits(val))
				{
					break;
				}
				if (!tail_nonzero &&
					::fast_io::details::
						scan_decfloat_ascii8_has_nonzero_digit(val))
				{
					tail_nonzero = true;
				}
				first += sizeof(::std::uint_least64_t);
			}
		}
	}
	char8_t digit{};
	for (; first != last &&
		   ::fast_io::details::scan_decfloat_decimal_digit(*first, digit);
		 ++first)
	{
		if (!tail_nonzero && digit != 0)
		{
			tail_nonzero = true;
		}
	}
	auto const skipped{
		static_cast<::std::uint_least64_t>(first - original_first)};
	if (skipped)
	{
		if (after_decimal)
		{
			state.fractional_digits += skipped;
		}
		state.significant_digits += skipped;
		if (tail_nonzero)
		{
			state.exact_truncated_nonzero = true;
			state.truncated_nonzero = true;
		}
	}
	return first;
}

[[nodiscard]] inline constexpr ::std::uint_least32_t
scan_decfloat_ascii8_parse(::std::uint_least64_t val) noexcept
{
	/*
	Let B=2^8 and let the already validated little-endian bytes be
	c_i='0'+d_i, 0<=d_i<=9.  Whole-word subtraction by eight 0x30 bytes has no
	inter-byte borrow: starting at the least-significant lane, c_i>=0x30 and
	every preceding lane generated no borrow.  Hence the resulting word is
	V=sum(d_i B^i) exactly.

	W=10V+(V/B) has byte i equal to p_i=10d_i+d_(i+1) for i<7.
	Since p_i<=99<B, neither multiplication by ten nor the addition propagates
	a carry between bytes.  The even bytes p_0,p_2,p_4,p_6 are therefore the
	four adjacent two-digit groups.  With M=0x000000ff000000ff, write

	  X=W&M              = p_0+p_4 B^4,
	  Y=(W/B^2)&M        = p_2+p_6 B^4.

	The constants below are 100+10^6 B^4 and 1+10^4 B^4.  In unsigned
	64-bit arithmetic, terms containing B^8 vanish modulo B^8, so the high
	base-B^4 limb of

	  X(100+10^6 B^4)+Y(1+10^4 B^4)

	is 10^6 p_0+10^4 p_2+100 p_4+p_6.  Its low limb is
	100p_0+p_2<=9999<B^4 and cannot carry into that result.  Substituting the
	p_i gives sum(d_i 10^(7-i)), the exact eight-digit integer; it is below
	10^8 and consequently survives the final uint_least32_t conversion.
	*/
	constexpr ::std::uint_least64_t mask{0x000000FF000000FFu};
	constexpr ::std::uint_least64_t mul1{0x000F424000000064u};
	constexpr ::std::uint_least64_t mul2{0x0000271000000001u};
	val -= 0x3030303030303030u;
	val = (val * 10u) + (val >> 8u);
	val = (((val & mask) * mul1) + (((val >> 16u) & mask) * mul2)) >> 32u;
	return static_cast<::std::uint_least32_t>(val);
}

template <typename T>
inline constexpr void scan_decfloat_append_eight_digits(scan_decfloat_significand_state<T> &state,
														bool after_decimal,
														::std::uint_least32_t digits) noexcept
{
	constexpr auto digit_limit{::fast_io::details::scan_decfloat_significand_digit_limit<T>};
	state.has_digit = true;
	if (after_decimal)
	{
		state.fractional_digits += 8u;
	}
	if (!state.has_nonzero_digit)
	{
		if (digits == 0)
		{
			return;
		}
		state.has_nonzero_digit = true;
	}
	state.significant_digits += 8u;
	::fast_io::details::scan_decfloat_append_exact_eight_digits(state, digits);
	if (state.stored_digits == digit_limit)
	{
		if (digits != 0)
		{
			state.truncated_nonzero = true;
		}
		return;
	}
	auto const available{digit_limit - state.stored_digits};
	if (8u <= available)
	{
		state.significand = state.significand * 100000000u + static_cast<::std::uint_least64_t>(digits);
		state.stored_digits += 8u;
		return;
	}
	auto const divisor{::fast_io::details::scan_decfloat_pow10_0_to_8_table[8u - available]};
	auto const stored_part{digits / divisor};
	auto const truncated_part{digits - stored_part * divisor};
	state.significand =
		state.significand * ::fast_io::details::scan_decfloat_pow10_0_to_8_table[available] +
		static_cast<::std::uint_least64_t>(stored_part);
	state.stored_digits = digit_limit;
	if (truncated_part != 0)
	{
		state.truncated_nonzero = true;
	}
}

template <typename T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scan_decfloat_append_eight_ascii_digits(scan_decfloat_significand_state<T> &state,
															  bool after_decimal,
															  ::std::uint_least64_t val) noexcept
{
	constexpr auto digit_limit{::fast_io::details::scan_decfloat_significand_digit_limit<T>};
	auto const digits{::fast_io::details::scan_decfloat_ascii8_parse(val)};
	state.has_digit = true;
	if (after_decimal)
	{
		state.fractional_digits += 8u;
	}
	if (!state.has_nonzero_digit)
	{
		if (digits == 0)
		{
			return;
		}
		state.has_nonzero_digit = true;
	}
	state.significant_digits += 8u;
	::fast_io::details::scan_decfloat_append_exact_ascii8_digits(state, val);
	if (state.stored_digits == digit_limit)
	{
		if (digits != 0)
		{
			state.truncated_nonzero = true;
		}
		return;
	}
	auto const available{digit_limit - state.stored_digits};
	if (8u <= available)
	{
		state.significand = state.significand * 100000000u + static_cast<::std::uint_least64_t>(digits);
		state.stored_digits += 8u;
		return;
	}
	auto const divisor{::fast_io::details::scan_decfloat_pow10_0_to_8_table[8u - available]};
	auto const stored_part{digits / divisor};
	auto const truncated_part{digits - stored_part * divisor};
	state.significand =
		state.significand * ::fast_io::details::scan_decfloat_pow10_0_to_8_table[available] +
		static_cast<::std::uint_least64_t>(stored_part);
	state.stored_digits = digit_limit;
	if (truncated_part != 0)
	{
		state.truncated_nonzero = true;
	}
}

template <typename T, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *
scan_decfloat_digits(char_type const *first, char_type const *last, bool after_decimal,
					 scan_decfloat_significand_state<T> &state) noexcept
{
	constexpr auto digit_limit{::fast_io::details::scan_decfloat_significand_digit_limit<T>};
	char8_t digit{};
	if constexpr (sizeof(char_type) == sizeof(char8_t) && ::fast_io::details::is_ascii<char_type> &&
				  ::std::numeric_limits<::std::uint_least64_t>::digits == 64u)
	{
		FAST_IO_IF_NOT_CONSTEVAL
		{
			for (; first != last && !state.has_nonzero_digit; ++first)
			{
				if (!::fast_io::details::scan_decfloat_decimal_digit(*first, digit))
				{
					return first;
				}
				::fast_io::details::scan_decfloat_append_digit<T>(state, digit, after_decimal);
			}
			if (static_cast<::std::size_t>(last - first) >= 2u * sizeof(::std::uint_least64_t))
			{
				for (; static_cast<::std::size_t>(last - first) >= sizeof(::std::uint_least64_t);)
				{
					if (state.has_nonzero_digit && state.stored_digits == digit_limit &&
						state.exact_stored_digits == ::fast_io::details::scan_decfloat_exact_digit_capacity<T>)
					{
						first = ::fast_io::details::scan_decfloat_skip_after_exact_limit_run(
							first, last, after_decimal, state);
						break;
					}
					::std::uint_least64_t val;
					::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));
					if constexpr (::std::endian::little != ::std::endian::native)
					{
						val = ::fast_io::little_endian(val);
					}
					if (!::fast_io::details::scan_decfloat_ascii8_is_digits(val))
					{
						break;
					}
					if (state.has_nonzero_digit &&
						state.stored_digits == digit_limit)
					{
						if (after_decimal)
						{
							state.fractional_digits += 8u;
						}
						state.significant_digits += 8u;
						auto const has_nonzero{
							::fast_io::details::scan_decfloat_ascii8_has_nonzero_digit(val)};
						if (state.exact_stored_digits !=
							::fast_io::details::scan_decfloat_exact_digit_capacity<T>)
						{
							::fast_io::details::scan_decfloat_append_exact_ascii8_digits(state, val);
						}
						else if (!state.exact_truncated_nonzero && has_nonzero)
						{
							state.exact_truncated_nonzero = true;
						}
						if (!state.truncated_nonzero && has_nonzero)
						{
							state.truncated_nonzero = true;
						}
					}
					else
					{
						::fast_io::details::scan_decfloat_append_eight_ascii_digits<T>(
							state, after_decimal, val);
					}
					first += sizeof(::std::uint_least64_t);
				}
			}
		}
	}
	for (; first != last && ::fast_io::details::scan_decfloat_decimal_digit(*first, digit); ++first)
	{
		::fast_io::details::scan_decfloat_append_digit<T>(state, digit, after_decimal);
	}
	return first;
}

template <typename T, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *
scan_decfloat_digits_padding(
	char_type const *first, char_type const *last, bool after_decimal,
	scan_decfloat_significand_state<T> &state, ::std::size_t padding) noexcept
{
	constexpr auto digit_limit{
		::fast_io::details::scan_decfloat_significand_digit_limit<T>};
	char8_t digit{};
	if constexpr (
		sizeof(char_type) == sizeof(char8_t) &&
		::fast_io::details::is_ascii<char_type> &&
		::std::numeric_limits<::std::uint_least64_t>::digits == 64u)
	{
		FAST_IO_IF_NOT_CONSTEVAL
		{
			for (; first != last && !state.has_nonzero_digit; ++first)
			{
				if (!::fast_io::details::scan_decfloat_decimal_digit(
						*first, digit))
				{
					return first;
				}
				::fast_io::details::scan_decfloat_append_digit<T>(
					state, digit, after_decimal);
			}
			if (static_cast<::std::size_t>(last - first) >=
				2u * sizeof(::std::uint_least64_t))
			{
				for (; static_cast<::std::size_t>(last - first) >=
					   sizeof(::std::uint_least64_t);)
				{
					if (state.has_nonzero_digit &&
						state.stored_digits == digit_limit &&
						state.exact_stored_digits ==
							::fast_io::details::
								scan_decfloat_exact_digit_capacity<T>)
					{
						first =
							::fast_io::details::
								scan_decfloat_skip_after_exact_limit_padding_run(
									first, last, after_decimal, state, padding);
						break;
					}
					::std::uint_least64_t val;
					::fast_io::freestanding::my_memcpy(
						__builtin_addressof(val), first,
						sizeof(::std::uint_least64_t));
					if constexpr (
						::std::endian::little != ::std::endian::native)
					{
						val = ::fast_io::little_endian(val);
					}
					if (!::fast_io::details::
							scan_decfloat_ascii8_is_digits(val))
					{
						break;
					}
					if (state.has_nonzero_digit &&
						state.stored_digits == digit_limit)
					{
						if (after_decimal)
						{
							state.fractional_digits += 8u;
						}
						state.significant_digits += 8u;
						auto const has_nonzero{
							::fast_io::details::
								scan_decfloat_ascii8_has_nonzero_digit(val)};
						if (state.exact_stored_digits !=
							::fast_io::details::
								scan_decfloat_exact_digit_capacity<T>)
						{
							::fast_io::details::
								scan_decfloat_append_exact_ascii8_digits(
									state, val);
						}
						else if (!state.exact_truncated_nonzero &&
								 has_nonzero)
						{
							state.exact_truncated_nonzero = true;
						}
						if (!state.truncated_nonzero && has_nonzero)
						{
							state.truncated_nonzero = true;
						}
					}
					else
					{
						::fast_io::details::
							scan_decfloat_append_eight_ascii_digits<T>(
								state, after_decimal, val);
					}
					first += sizeof(::std::uint_least64_t);
				}
			}
		}
	}
	for (; first != last &&
		   ::fast_io::details::scan_decfloat_decimal_digit(*first, digit);
		 ++first)
	{
		::fast_io::details::scan_decfloat_append_digit<T>(
			state, digit, after_decimal);
	}
	return first;
}

template <::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_exponent(char_type const *first, char_type const *last, ::std::int_least64_t &exponent) noexcept
{
	if constexpr (sizeof(char_type) == sizeof(char8_t) && ::fast_io::details::is_ascii<char_type>)
	{
		FAST_IO_IF_NOT_CONSTEVAL
		{
			auto const *const original_first{first};
			if (first == last)
			{
				return {first, ::fast_io::parse_code::invalid};
			}
			using unsigned_char_type = ::fast_io::details::my_make_unsigned_t<char_type>;
			constexpr auto plus{static_cast<unsigned_char_type>(u8'+')};
			constexpr auto minus{static_cast<unsigned_char_type>(u8'-')};
			constexpr auto zero{static_cast<unsigned_char_type>(u8'0')};
			bool negative{};
			auto uch{static_cast<unsigned_char_type>(*first)};
			if (uch == minus)
			{
				negative = true;
				++first;
			}
			else if (uch == plus)
			{
				++first;
			}
			if (first == last)
			{
				return {first, ::fast_io::parse_code::invalid};
			}
			uch = static_cast<unsigned_char_type>(*first);
			auto digit{static_cast<unsigned_char_type>(uch - zero)};
			if (10u <= digit)
			{
				return {first, ::fast_io::parse_code::invalid};
			}
			::std::uint_least64_t value{static_cast<::std::uint_least64_t>(digit)};
			++first;
			::std::size_t digit_count{1u};
			for (; first != last; ++first)
			{
				uch = static_cast<unsigned_char_type>(*first);
				digit = static_cast<unsigned_char_type>(uch - zero);
				if (10u <= digit)
				{
					break;
				}
				if (digit_count == 18u)
				{
					return ::fast_io::details::scan_hexfloat_exponent(original_first, last, exponent);
				}
				value = value * 10u + static_cast<::std::uint_least64_t>(digit);
				++digit_count;
			}
			exponent = negative ? -static_cast<::std::int_least64_t>(value) :
								  static_cast<::std::int_least64_t>(value);
			return {first, ::fast_io::parse_code::ok};
		}
	}
	return ::fast_io::details::scan_hexfloat_exponent(first, last, exponent);
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr bool scan_decfloat_exponent_prefix_may_extend(char_type const *first,
																			 char_type const *last) noexcept
{
	constexpr auto plus{::fast_io::char_literal_v<u8'+', char_type>};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', char_type>};
	++first;
	if (first == last)
	{
		return true;
	}
	if ((*first == plus || *first == minus) && first + 1 == last)
	{
		return true;
	}
	return false;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr bool scan_decfloat_special_start_char(char_type ch) noexcept
{
	return ::fast_io::details::scan_hexfloat_caseless_equal<u8'i', u8'I'>(ch) ||
		   ::fast_io::details::scan_hexfloat_caseless_equal<u8'n', u8'N'>(ch);
}

template <typename T>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_adjusted(T &value, bool negative, ::std::uint_least64_t significand,
							  scan_decfloat_adjusted_mantissa adjusted) noexcept
{
	::fast_io::details::scan_decfloat_to_float(negative, adjusted, value);
	if ((significand != 0 && adjusted.mantissa == 0 && adjusted.power2 == 0) ||
		adjusted.power2 == ::fast_io::details::scan_decfloat_infinite_power<T>())
	{
		return ::fast_io::parse_code::overflow;
	}
	return ::fast_io::parse_code::ok;
}

template <typename T, ::fast_io::manipulators::floating_rounding rounding =
						  ::fast_io::manipulators::floating_rounding::nearest_to_even>
[[nodiscard]] inline constexpr ::fast_io::parse_code scan_decfloat_assign(T &value, bool negative,
																		  scan_decfloat_significand_state<T> const &state,
																		  ::std::int_least64_t exponent) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	auto adjusted_exponent{::fast_io::details::scan_decfloat_adjusted_exponent(state, exponent)};
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		if (::std::is_constant_evaluated() &&
			!state.truncated_nonzero &&
			::fast_io::details::scan_decfloat_try_clinger(
				state.significand, adjusted_exponent, negative, value))
		{
			/*
			Constant evaluation has a translation-fixed nearest-even mode, and
			an untruncated state denotes the exact integer significand.  The
			Clinger bounds then prove the native multiply/divide is correctly
			rounded.  At runtime the first conjunct is false and this entire
			branch folds away, preserving fenv independence.
			*/
			return ::fast_io::parse_code::ok;
		}
	}
	if constexpr (::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
	{
		::fast_io::details::scan_decfloat_adjusted_mantissa adjusted;
		if (!state.truncated_nonzero)
		{
			if (::fast_io::details::scan_decfloat_compute_adjusted<no_cvref_t, rounding>(
					adjusted_exponent, state.significand, negative, adjusted))
			{
				return ::fast_io::details::scan_decfloat_assign_adjusted(value, negative, state.significand, adjusted);
			}
		}
		else if (state.significand != 0 &&
				 state.significand != (::std::numeric_limits<::std::uint_least64_t>::max)())
		{
			::fast_io::details::scan_decfloat_adjusted_mantissa upper_adjusted;
			if (::fast_io::details::scan_decfloat_compute_adjusted<no_cvref_t, rounding>(
					adjusted_exponent, state.significand, negative, adjusted) &&
				::fast_io::details::scan_decfloat_compute_adjusted<no_cvref_t, rounding>(
					adjusted_exponent, state.significand + 1u, negative, upper_adjusted))
			{
				if (::fast_io::details::scan_decfloat_adjusted_mantissa_equal(adjusted, upper_adjusted))
				{
					return ::fast_io::details::scan_decfloat_assign_adjusted(value, negative, state.significand, adjusted);
				}
				auto decimal_exponent{::fast_io::details::scan_decfloat_saturating_add(
					exponent, -static_cast<::std::int_least64_t>(state.fractional_digits))};
				decimal_exponent = ::fast_io::details::scan_decfloat_saturating_add(
					decimal_exponent,
					static_cast<::std::int_least64_t>(
						state.significant_digits - state.exact_stored_digits));
				::fast_io::parse_code code{};
				if (::fast_io::details::scan_decfloat_try_interval_compare<
						no_cvref_t, rounding>(
						value, negative, state, decimal_exponent,
						adjusted, upper_adjusted, code))
				{
					return code;
				}
			}
		}
	}
#ifdef __SIZEOF_INT128__
	return ::fast_io::details::scan_decfloat_assign_big<T, rounding>(value, negative, state, exponent);
#else
	if constexpr (::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
	{
		/*
		The cached interval could not prove one endpoint.  The portable exact
		quotient has p+1<=54 bits here, so it is the arithmetic continuation;
		`partial` is reserved for genuinely extensible lexical state.
		*/
		return ::fast_io::details::scan_decfloat_assign_big<T, rounding>(
			value, negative, state, exponent);
	}
	else if constexpr (::fast_io::details::scan_decfloat_native_wide_supported<no_cvref_t>)
	{
		return ::fast_io::details::scan_decfloat_assign_native_wide<T, rounding>(
			value, negative, state.significand, adjusted_exponent);
	}
	return ::fast_io::parse_code::partial;
#endif
}

template <typename T, ::fast_io::manipulators::floating_rounding rounding =
						  ::fast_io::manipulators::floating_rounding::nearest_to_even>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_significand(T &value, bool negative, ::std::uint_least64_t significand,
								 ::std::int_least64_t adjusted_exponent) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	if (!significand)
	{
		value = negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
		return ::fast_io::parse_code::ok;
	}
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		if (::std::is_constant_evaluated() &&
			::fast_io::details::scan_decfloat_try_clinger(
				significand, adjusted_exponent, negative, value))
		{
			/*
			This state contains no omitted digits by construction.  Therefore
			the constant-evaluation Clinger theorem applies directly; runtime
			again bypasses native floating arithmetic before reading the fenv.
			*/
			return ::fast_io::parse_code::ok;
		}
	}
	if constexpr (::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
	{
		::fast_io::details::scan_decfloat_adjusted_mantissa adjusted;
		if (::fast_io::details::scan_decfloat_compute_adjusted<no_cvref_t, rounding>(
				adjusted_exponent, significand, negative, adjusted))
		{
			return ::fast_io::details::scan_decfloat_assign_adjusted(value, negative, significand, adjusted);
		}
	}
#ifdef __SIZEOF_INT128__
	::fast_io::details::scan_decfloat_significand_state<T> state;
	::fast_io::details::scan_decfloat_state_from_u64(state, significand);
	return ::fast_io::details::scan_decfloat_assign_big<T, rounding>(value, negative, state, adjusted_exponent);
#else
	if constexpr (::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
	{
		::fast_io::details::scan_decfloat_significand_state<T> state;
		::fast_io::details::scan_decfloat_state_from_u64(state, significand);
		return ::fast_io::details::scan_decfloat_assign_big<T, rounding>(
			value, negative, state, adjusted_exponent);
	}
	else if constexpr (::fast_io::details::scan_decfloat_native_wide_supported<no_cvref_t>)
	{
		return ::fast_io::details::scan_decfloat_assign_native_wide<T, rounding>(
			value, negative, significand, adjusted_exponent);
	}
	return ::fast_io::parse_code::partial;
#endif
}

template <typename T, ::fast_io::manipulators::floating_precision precision_mode,
		  ::fast_io::manipulators::floating_rounding rounding =
			  ::fast_io::manipulators::floating_rounding::nearest_to_even>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_precision(T &value, bool negative, scan_decfloat_significand_state<T> const &state,
							   ::std::int_least64_t exponent, ::std::size_t precision) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return ::fast_io::details::scan_decfloat_assign_precision<
				T, precision_mode, ::fast_io::manipulators::floating_rounding::toward_plus_infinity>(
				value, negative, state, exponent, precision);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return ::fast_io::details::scan_decfloat_assign_precision<
				T, precision_mode, ::fast_io::manipulators::floating_rounding::toward_minus_infinity>(
				value, negative, state, exponent, precision);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return ::fast_io::details::scan_decfloat_assign_precision<
				T, precision_mode, ::fast_io::manipulators::floating_rounding::toward_zero>(
				value, negative, state, exponent, precision);
		default:
			return ::fast_io::details::scan_decfloat_assign_precision<
				T, precision_mode, ::fast_io::manipulators::floating_rounding::nearest_to_even>(
				value, negative, state, exponent, precision);
		}
	}
	else
	{
		auto adjusted_exponent{::fast_io::details::scan_decfloat_adjusted_exponent(state, exponent)};
		auto significand{state.significand};
		if constexpr (::fast_io::details::floating_precision_is_significant<precision_mode>)
		{
			if (!precision)
			{
				precision = 1u;
			}
			if (state.significant_digits <= precision || state.stored_digits <= precision)
			{
				return ::fast_io::details::scan_decfloat_assign<T, rounding>(value, negative, state, exponent);
			}
			auto const cut{static_cast<::std::uint_least32_t>(state.stored_digits - precision)};
			auto const divisor{::fast_io::details::print_rsv_fp_pow10_0_to_19_table[cut]};
			auto quotient{significand / divisor};
			auto const remainder{significand - quotient * divisor};
			if (::fast_io::details::scan_decfloat_decimal_round_up<rounding>(
					negative, quotient, remainder, divisor, state.truncated_nonzero))
			{
				++quotient;
			}
			significand = quotient;
			adjusted_exponent = ::fast_io::details::scan_decfloat_saturating_add(
				adjusted_exponent, static_cast<::std::int_least64_t>(cut));
			if (precision < 20u &&
				significand == ::fast_io::details::print_rsv_fp_pow10_0_to_19_table[precision])
			{
				significand /= 10u;
				adjusted_exponent = ::fast_io::details::scan_decfloat_saturating_add(adjusted_exponent, 1);
			}
		}
		else
		{
			constexpr auto int64_max{(::std::numeric_limits<::std::int_least64_t>::max)()};
			if (static_cast<::std::uintmax_t>(precision) >
				static_cast<::std::uintmax_t>(int64_max))
			{
				return ::fast_io::details::scan_decfloat_assign<T, rounding>(value, negative, state, exponent);
			}
			auto const target_exponent{-static_cast<::std::int_least64_t>(precision)};
			if (target_exponent <= adjusted_exponent)
			{
				return ::fast_io::details::scan_decfloat_assign<T, rounding>(value, negative, state, exponent);
			}
			auto const cut64{static_cast<::std::uint_least64_t>(target_exponent) -
							 static_cast<::std::uint_least64_t>(adjusted_exponent)};
			if (20u <= cut64)
			{
				if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
				{
					significand = 0u;
				}
				else
				{
					significand = static_cast<::std::uint_least64_t>(
						::fast_io::details::floating_rounding_directed_round_up<rounding>(negative));
				}
			}
			else
			{
				auto const divisor{
					::fast_io::details::print_rsv_fp_pow10_0_to_19_table[static_cast<::std::size_t>(cut64)]};
				auto quotient{significand / divisor};
				auto const remainder{significand - quotient * divisor};
				if (::fast_io::details::scan_decfloat_decimal_round_up<rounding>(
						negative, quotient, remainder, divisor, state.truncated_nonzero))
				{
					++quotient;
				}
				significand = quotient;
			}
			adjusted_exponent = target_exponent;
		}
		return ::fast_io::details::scan_decfloat_assign_significand<T, rounding>(
			value, negative, significand, adjusted_exponent);
	}
}

template <typename T, ::fast_io::manipulators::floating_rounding rounding =
						  ::fast_io::manipulators::floating_rounding::nearest_to_even>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_short(T &value, bool negative, ::std::uint_least64_t significand,
						   ::std::uint_least64_t fractional_digits,
						   ::std::int_least64_t exponent) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	auto adjusted_exponent{::fast_io::details::scan_decfloat_saturating_add(
		exponent, -static_cast<::std::int_least64_t>(fractional_digits))};
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		if (::std::is_constant_evaluated() &&
			::fast_io::details::scan_decfloat_try_clinger(
				significand, adjusted_exponent, negative, value))
		{
			/*
			The short parser's coefficient and adjusted exponent denote the
			complete decimal exactly.  Constant evaluation may use Clinger;
			runtime conversion falls through to the same integer kernel used by
			long inputs and all other explicit policies.
			*/
			return ::fast_io::parse_code::ok;
		}
	}
	if (::fast_io::details::scan_decfloat_try_exact_dyadic(
			value, negative, significand, adjusted_exponent))
	{
		/*
		The helper returns true only after constructing the exact target bit
		pattern with integer arithmetic.  Exact representability makes all ten
		explicit rounding policies, and every current-environment selection,
		observationally identical; returning here cannot bypass a policy-specific
		tie, underflow, or overflow decision.
		*/
		return ::fast_io::parse_code::ok;
	}
	if constexpr (::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
	{
		::fast_io::details::scan_decfloat_adjusted_mantissa adjusted;
		if (::fast_io::details::scan_decfloat_compute_adjusted<no_cvref_t, rounding>(
				adjusted_exponent, significand, negative, adjusted))
		{
			return ::fast_io::details::scan_decfloat_assign_adjusted(value, negative, significand, adjusted);
		}
	}
#ifdef __SIZEOF_INT128__
	::fast_io::details::scan_decfloat_significand_state<T> state;
	::fast_io::details::scan_decfloat_state_from_u64(state, significand);
	return ::fast_io::details::scan_decfloat_assign_big<T, rounding>(value, negative, state, adjusted_exponent);
#else
	if constexpr (::fast_io::details::scan_decfloat_compute_supported<no_cvref_t>)
	{
		::fast_io::details::scan_decfloat_significand_state<T> state;
		::fast_io::details::scan_decfloat_state_from_u64(state, significand);
		return ::fast_io::details::scan_decfloat_assign_big<T, rounding>(
			value, negative, state, adjusted_exponent);
	}
	else if constexpr (::fast_io::details::scan_decfloat_native_wide_supported<no_cvref_t>)
	{
		return ::fast_io::details::scan_decfloat_assign_native_wide<T, rounding>(
			value, negative, significand, adjusted_exponent);
	}
	return ::fast_io::parse_code::partial;
#endif
}

template <typename T>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_current_environment(T &value, bool negative,
										 scan_decfloat_significand_state<T> const &state,
										 ::std::int_least64_t exponent) noexcept
{
	switch (::fast_io::details::current_floating_rounding())
	{
	case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
		return ::fast_io::details::scan_decfloat_assign<
			T, ::fast_io::manipulators::floating_rounding::toward_plus_infinity>(
			value, negative, state, exponent);
	case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
		return ::fast_io::details::scan_decfloat_assign<
			T, ::fast_io::manipulators::floating_rounding::toward_minus_infinity>(
			value, negative, state, exponent);
	case ::fast_io::manipulators::floating_rounding::toward_zero:
		return ::fast_io::details::scan_decfloat_assign<
			T, ::fast_io::manipulators::floating_rounding::toward_zero>(
			value, negative, state, exponent);
	default:
		return ::fast_io::details::scan_decfloat_assign<
			T, ::fast_io::manipulators::floating_rounding::nearest_to_even>(
			value, negative, state, exponent);
	}
}

template <typename T>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_short_current_environment(T &value, bool negative, ::std::uint_least64_t significand,
											   ::std::uint_least64_t fractional_digits,
											   ::std::int_least64_t exponent) noexcept
{
	switch (::fast_io::details::current_floating_rounding())
	{
	case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
		return ::fast_io::details::scan_decfloat_assign_short<
			T, ::fast_io::manipulators::floating_rounding::toward_plus_infinity>(
			value, negative, significand, fractional_digits, exponent);
	case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
		return ::fast_io::details::scan_decfloat_assign_short<
			T, ::fast_io::manipulators::floating_rounding::toward_minus_infinity>(
			value, negative, significand, fractional_digits, exponent);
	case ::fast_io::manipulators::floating_rounding::toward_zero:
		return ::fast_io::details::scan_decfloat_assign_short<
			T, ::fast_io::manipulators::floating_rounding::toward_zero>(
			value, negative, significand, fractional_digits, exponent);
	default:
		return ::fast_io::details::scan_decfloat_assign_short<
			T, ::fast_io::manipulators::floating_rounding::nearest_to_even>(
			value, negative, significand, fractional_digits, exponent);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr scan_decfloat_fast_result<char_type>
scan_decfloat_contiguous_short_define_impl(char_type const *first,
										   char_type const *end,
										   bool negative, T &value) noexcept
{
	if constexpr (sizeof(char_type) != sizeof(char8_t) || !::fast_io::details::is_ascii<char_type>)
	{
		return {};
	}
	else
	{
		FAST_IO_IF_CONSTEVAL
		{
			return {};
		}
		else
		{
			constexpr auto digit_limit{::fast_io::details::scan_decfloat_significand_digit_limit<T>};
			::std::uint_least64_t significand{};
			::std::uint_least64_t digit_count{};
			::std::uint_least64_t fractional_digits{};
			bool has_digit{};
			char8_t digit{};
			constexpr auto zero{
				::fast_io::char_literal_v<u8'0', char_type>};
			constexpr ::std::uint_least64_t ascii_zeroes{
				0x3030303030303030ULL};
			/*
			If z leading integer zeroes precede the first nonzero digit, replacing
			the digit sequence 0^z D by D leaves both M*10^q and q unchanged:
			the integer radix position is unchanged and the removed coefficient
			terms are exactly zero.  Consequently they consume no significand
			precision.  The eight-byte equality is stronger than a digit test and
			therefore cannot skip punctuation or a nonzero digit.  Unaligned loads
			use memcpy, so the optimization adds no alignment precondition.
			*/
			for (; static_cast<::std::size_t>(end - first) >=
				   sizeof(::std::uint_least64_t);)
			{
				::std::uint_least64_t val;
				::fast_io::freestanding::my_memcpy(
					__builtin_addressof(val), first,
					sizeof(::std::uint_least64_t));
				if constexpr (
					::std::endian::little != ::std::endian::native)
				{
					val = ::fast_io::little_endian(val);
				}
				if (val != ascii_zeroes)
				{
					break;
				}
				first += sizeof(::std::uint_least64_t);
				has_digit = true;
			}
			for (; first != end && *first == zero; ++first)
			{
				has_digit = true;
			}
			for (; digit_count + 8u <= digit_limit &&
				   static_cast<::std::size_t>(end - first) >= sizeof(::std::uint_least64_t);)
			{
				::std::uint_least64_t val;
				::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));
				if constexpr (::std::endian::little != ::std::endian::native)
				{
					val = ::fast_io::little_endian(val);
				}
				if (!::fast_io::details::scan_decfloat_ascii8_is_digits(val))
				{
					break;
				}
				significand = significand * 100000000u +
							  static_cast<::std::uint_least64_t>(
								  ::fast_io::details::scan_decfloat_ascii8_parse(val));
				digit_count += 8u;
				first += sizeof(::std::uint_least64_t);
				has_digit = true;
			}
			/*
			Peel a compiler-selected short scalar prefix without changing the
			decimal recurrence.
			One successful call consumes exactly one abstract digit d and applies

				(S,n,p) -> (10*S+d,n+1,p+1).

			The `&&` sequence stops at the first end/nondigit, exactly where the
			former loop stopped; only after every peeled call succeeds can the
			residual loop run.  Testing the digit before `digit_limit` is
			significant: a nondigit after the last storable digit completes the
			short token, whereas one more digit must select the exact long
			fallback.  Thus the peeled and loop forms have identical pointer,
			coefficient, count and fallback results.

			Four calls give the best measured layout on Apple/LLVM and GCC
			11--15.  GCC 16 lowers three calls to a smaller public parser and a
			better real Raptor-Lake result; LLVM-MCA also reduces its modeled
			uops versus four calls on Alder Lake, Zen 3/4 and Broadwell.  The
			compile-time choice changes only how many applications of the proved
			recurrence are written straight-line.  It does not claim to remove
			every dynamic branch, and the residual loop is identical.
			*/
			bool digit_limit_reached{};
			auto scan_one_digit = [&]() constexpr noexcept {
				if (first == end ||
					!::fast_io::details::scan_decfloat_decimal_digit(*first, digit))
				{
					return false;
				}
				if (digit_count == digit_limit)
				{
					digit_limit_reached = true;
					return false;
				}
				significand =
					significand * 10u + static_cast<::std::uint_least64_t>(digit);
				++digit_count;
				++first;
				has_digit = true;
				return true;
			};
#if defined(__GNUC__) && !defined(__clang__) && 16 <= __GNUC__
			if (scan_one_digit() && scan_one_digit() &&
				scan_one_digit())
#else
			if (scan_one_digit() && scan_one_digit() &&
				scan_one_digit() && scan_one_digit())
#endif
			{
				while (scan_one_digit())
				{
				}
			}
			if (digit_limit_reached)
			{
				return {};
			}

			constexpr auto dot{::fast_io::char_literal_v<
				(flags.comma ? u8',' : u8'.'), char_type>};
			if (first != end && *first == dot)
			{
				++first;
				auto const *fraction_begin{first};
				/*
				Fractional zeroes may be erased from the stored coefficient only
				while no earlier nonzero digit exists.  Unlike integer leading
				zeroes, each erased fractional zero still moves the decimal radix;
				fractional_digits is therefore measured from fraction_begin after
				scanning and includes every skipped code unit.  Thus

					0.00D * 10^E = D * 10^(E-3)

				is preserved exactly.  Once significand is nonzero, an intervening
				zero changes the place value of every later digit and must enter
				the multiply-by-ten recurrence.
				*/
				if (!digit_count)
				{
					for (; static_cast<::std::size_t>(end - first) >=
						   sizeof(::std::uint_least64_t);)
					{
						::std::uint_least64_t val;
						::fast_io::freestanding::my_memcpy(
							__builtin_addressof(val), first,
							sizeof(::std::uint_least64_t));
						if constexpr (
							::std::endian::little !=
							::std::endian::native)
						{
							val = ::fast_io::little_endian(val);
						}
						if (val != ascii_zeroes)
						{
							break;
						}
						first += sizeof(::std::uint_least64_t);
						has_digit = true;
					}
					for (; first != end && *first == zero; ++first)
					{
						has_digit = true;
					}
				}
				for (; digit_count + 8u <= digit_limit &&
					   static_cast<::std::size_t>(end - first) >= sizeof(::std::uint_least64_t);)
				{
					::std::uint_least64_t val;
					::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));
					if constexpr (::std::endian::little != ::std::endian::native)
					{
						val = ::fast_io::little_endian(val);
					}
					if (!::fast_io::details::scan_decfloat_ascii8_is_digits(val))
					{
						break;
					}
					significand = significand * 100000000u +
								  static_cast<::std::uint_least64_t>(
									  ::fast_io::details::scan_decfloat_ascii8_parse(val));
					digit_count += 8u;
					first += sizeof(::std::uint_least64_t);
					has_digit = true;
				}
				/*
				This is the same compiler-selected recurrence prefix proved for
				the integer part.  `fractional_digits` is derived from
				first-fraction_begin afterward, so advancing `first` inside the
				peeled step also preserves every radix-place contribution,
				including zeroes.
				*/
				bool fractional_digit_limit_reached{};
				auto scan_one_fractional_digit = [&]() constexpr noexcept {
					if (first == end ||
						!::fast_io::details::scan_decfloat_decimal_digit(*first, digit))
					{
						return false;
					}
					if (digit_count == digit_limit)
					{
						fractional_digit_limit_reached = true;
						return false;
					}
					significand =
						significand * 10u +
						static_cast<::std::uint_least64_t>(digit);
					++digit_count;
					++first;
					has_digit = true;
					return true;
				};
#if defined(__GNUC__) && !defined(__clang__) && 16 <= __GNUC__
				if (scan_one_fractional_digit() &&
					scan_one_fractional_digit() &&
					scan_one_fractional_digit())
#else
				if (scan_one_fractional_digit() &&
					scan_one_fractional_digit() &&
					scan_one_fractional_digit() &&
					scan_one_fractional_digit())
#endif
				{
					while (scan_one_fractional_digit())
					{
					}
				}
				if (fractional_digit_limit_reached)
				{
					return {};
				}
				fractional_digits = static_cast<::std::uint_least64_t>(first - fraction_begin);
			}

			if (!has_digit)
			{
				/*
				No decimal digit means that this speculative ASCII-only path
				owns no lexical result.  Returning unhandled delegates Inf/NaN,
				an isolated radix point, and every invalid prefix to the complete
				scanner below.  That scanner was already the semantic authority
				for non-ASCII, constant-evaluated, and over-limit inputs.

				This convention also lets the caller enter the short scanner
				without first decoding the leading code unit.  A numeric token
				therefore classifies its first digit exactly once rather than
				once in the caller and once here.  The partition is exhaustive:
				a decimal within the short coefficient bound returns handled,
				an over-limit decimal returns unhandled at the exact digit
				boundary, and a token with no digit returns unhandled here.  In
				all unhandled cases the complete scanner restarts at the
				unchanged position following the already-classified optional
				sign, with the saved `negative` bit; an eventual lexical failure
				still reports `begin`.  Thus no speculative cursor, coefficient,
				value or public error position is observable.
				*/
				return {};
			}

			::std::int_least64_t exponent{};
			constexpr auto lower_e{::fast_io::char_literal_v<u8'e', char_type>};
			constexpr auto upper_e{::fast_io::char_literal_v<u8'E', char_type>};
			if constexpr (flags.floating == ::fast_io::manipulators::floating_format::fixed)
			{
			}
			else if (first != end && (*first == lower_e || *first == upper_e))
			{
				auto const *exponent_begin{first};
				auto exponent_result{::fast_io::details::scan_decfloat_exponent(first + 1, end, exponent)};
				if (exponent_result.code == ::fast_io::parse_code::ok)
				{
					first = exponent_result.iter;
				}
				else if (::fast_io::details::scan_decfloat_exponent_prefix_may_extend(exponent_begin, end))
				{
					return {end, ::fast_io::parse_code::partial, true};
				}
				else if constexpr (flags.floating == ::fast_io::manipulators::floating_format::scientific)
				{
					return {exponent_result.iter, exponent_result.code, true};
				}
				else
				{
					first = exponent_begin;
				}
			}
			else if constexpr (flags.floating == ::fast_io::manipulators::floating_format::scientific)
			{
				return {first, ::fast_io::parse_code::invalid, true};
			}

			if (significand == 0)
			{
				value = negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
				return {first, ::fast_io::parse_code::ok, true};
			}

			if constexpr (flags.rounding == ::fast_io::manipulators::floating_rounding::current_environment)
			{
				return {first,
						::fast_io::details::scan_decfloat_assign_short_current_environment(
							value, negative, significand, fractional_digits, exponent),
						true};
			}
			else
			{
				return {first,
						::fast_io::details::scan_decfloat_assign_short<T, flags.rounding>(
							value, negative, significand, fractional_digits, exponent),
						true};
			}
		}
	}
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T,
		  bool use_precision = false>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_contiguous_padding_define_impl(
	char_type const *begin, char_type const *end, T &value,
	::std::size_t precision, ::std::size_t padding) noexcept
{
	auto first{begin};
	constexpr auto plus{::fast_io::char_literal_v<u8'+', char_type>};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', char_type>};
	bool negative{};
	if (first != end && *first == minus)
	{
		negative = true;
		++first;
	}
	else if constexpr (flags.allow_leading_plus)
	{
		if (first != end && *first == plus)
		{
			++first;
		}
	}

	constexpr auto dot{::fast_io::char_literal_v<
		(flags.comma ? u8',' : u8'.'), char_type>};
	if (first != end &&
		::fast_io::details::scan_decfloat_special_start_char(*first))
	{
		auto const special_result{
			::fast_io::details::
				scan_hexfloat_special_value<flags.nan_parse_sign,
											flags.nan_payload_scan>(
					first, end, negative, value)};
		if (special_result.matched)
		{
			return {special_result.iter, special_result.code};
		}
	}

	::fast_io::details::scan_decfloat_significand_state<T>
		significand_state;
	first = ::fast_io::details::scan_decfloat_digits_padding<T, char_type>(
		first, end, false, significand_state, padding);

	if (first != end && *first == dot)
	{
		++first;
		first =
			::fast_io::details::scan_decfloat_digits_padding<T, char_type>(
				first, end, true, significand_state, padding);
	}

	if (!significand_state.has_digit)
	{
		return {begin, ::fast_io::parse_code::invalid};
	}

	::std::int_least64_t exponent{};
	constexpr auto lower_e{
		::fast_io::char_literal_v<u8'e', char_type>};
	constexpr auto upper_e{
		::fast_io::char_literal_v<u8'E', char_type>};
	if constexpr (
		flags.floating ==
		::fast_io::manipulators::floating_format::fixed)
	{
	}
	else if (first != end && (*first == lower_e || *first == upper_e))
	{
		auto const *exponent_begin{first};
		auto exponent_result{
			::fast_io::details::scan_decfloat_exponent(
				first + 1, end, exponent)};
		if (exponent_result.code == ::fast_io::parse_code::ok)
		{
			first = exponent_result.iter;
		}
		else if (
			::fast_io::details::scan_decfloat_exponent_prefix_may_extend(
				exponent_begin, end))
		{
			return {end, ::fast_io::parse_code::partial};
		}
		else if constexpr (
			flags.floating ==
			::fast_io::manipulators::floating_format::scientific)
		{
			return exponent_result;
		}
		else
		{
			first = exponent_begin;
		}
	}
	else if constexpr (
		flags.floating ==
		::fast_io::manipulators::floating_format::scientific)
	{
		return {first, ::fast_io::parse_code::invalid};
	}

	if (!significand_state.has_nonzero_digit)
	{
		value = negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
		return {first, ::fast_io::parse_code::ok};
	}
	if constexpr (use_precision)
	{
		return {
			first,
			::fast_io::details::
				scan_decfloat_assign_precision<T, flags.precision,
											 flags.rounding>(
					value, negative, significand_state, exponent, precision)};
	}
	else
	{
		if constexpr (
			flags.rounding ==
			::fast_io::manipulators::floating_rounding::
				current_environment)
		{
			return {
				first,
				::fast_io::details::
					scan_decfloat_assign_current_environment(
						value, negative, significand_state, exponent)};
		}
		else
		{
			return {
				first,
				::fast_io::details::
					scan_decfloat_assign<T, flags.rounding>(
						value, negative, significand_state, exponent)};
		}
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision = false>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_contiguous_define_impl(char_type const *begin, char_type const *end, T &value,
									 ::std::size_t precision = 0) noexcept
{
	auto first{begin};
	constexpr auto plus{::fast_io::char_literal_v<u8'+', char_type>};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', char_type>};
	bool negative{};
	if (first != end && *first == minus)
	{
		negative = true;
		++first;
	}
	else if constexpr (flags.allow_leading_plus)
	{
		if (first != end && *first == plus)
		{
			++first;
		}
	}

	if constexpr (!use_precision)
	{
		/*
		The short scanner is a transactional classifier: `handled` proves that
		its returned pointer/code/value are final, while unhandled exposes none
		of its local state.  Calling it directly removes the former duplicate
		leading-digit decode.  Its proof above shows that every rejected case is
		reparsed by the unchanged complete grammar from the preserved
		post-sign cursor and sign bit; public lexical failure still returns
		`begin`.
		*/
		auto const short_result{
			::fast_io::details::scan_decfloat_contiguous_short_define_impl<
				char_type, flags>(
				first, end, negative, value)};
		if (short_result.handled)
		{
			return {short_result.iter, short_result.code};
		}
	}

	constexpr auto dot{::fast_io::char_literal_v<
		(flags.comma ? u8',' : u8'.'), char_type>};
	if (first != end &&
		::fast_io::details::scan_decfloat_special_start_char(*first))
	{
		auto const special_result{
			::fast_io::details::scan_hexfloat_special_value<flags.nan_parse_sign, flags.nan_payload_scan>(
				first, end, negative, value)};
		if (special_result.matched)
		{
			return {special_result.iter, special_result.code};
		}
	}

	::fast_io::details::scan_decfloat_significand_state<T> significand_state;
	first = ::fast_io::details::scan_decfloat_digits<T, char_type>(first, end, false, significand_state);

	if (first != end && *first == dot)
	{
		++first;
		first = ::fast_io::details::scan_decfloat_digits<T, char_type>(first, end, true, significand_state);
	}

	if (!significand_state.has_digit)
	{
		return {begin, ::fast_io::parse_code::invalid};
	}

	::std::int_least64_t exponent{};
	constexpr auto lower_e{::fast_io::char_literal_v<u8'e', char_type>};
	constexpr auto upper_e{::fast_io::char_literal_v<u8'E', char_type>};
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::fixed)
	{
	}
	else if (first != end && (*first == lower_e || *first == upper_e))
	{
		auto const *exponent_begin{first};
		auto exponent_result{::fast_io::details::scan_decfloat_exponent(first + 1, end, exponent)};
		if (exponent_result.code == ::fast_io::parse_code::ok)
		{
			first = exponent_result.iter;
		}
		else if (::fast_io::details::scan_decfloat_exponent_prefix_may_extend(exponent_begin, end))
		{
			return {end, ::fast_io::parse_code::partial};
		}
		else if constexpr (flags.floating == ::fast_io::manipulators::floating_format::scientific)
		{
			return exponent_result;
		}
		else
		{
			first = exponent_begin;
		}
	}
	else if constexpr (flags.floating == ::fast_io::manipulators::floating_format::scientific)
	{
		return {first, ::fast_io::parse_code::invalid};
	}

	if (!significand_state.has_nonzero_digit)
	{
		value = negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
		return {first, ::fast_io::parse_code::ok};
	}
	if constexpr (use_precision)
	{
		return {first, ::fast_io::details::scan_decfloat_assign_precision<T, flags.precision, flags.rounding>(
						   value, negative, significand_state, exponent, precision)};
	}
	else
	{
		if constexpr (flags.rounding == ::fast_io::manipulators::floating_rounding::current_environment)
		{
			return {first, ::fast_io::details::scan_decfloat_assign_current_environment(
							   value, negative, significand_state, exponent)};
		}
		else
		{
			return {first, ::fast_io::details::scan_decfloat_assign<T, flags.rounding>(
							   value, negative, significand_state, exponent)};
		}
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision = false>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_contiguous_define(char_type const *begin, char_type const *end, T &value,
								::std::size_t precision = 0) noexcept
{
	if constexpr (!flags.noskipws)
	{
		if (begin == end)
		{
			return {begin, ::fast_io::parse_code::end_of_file};
		}
		bool has_space{};
		if constexpr (sizeof(char_type) == sizeof(char8_t) && ::fast_io::details::is_ascii<char_type>)
		{
			using unsigned_char_type = ::fast_io::details::my_make_unsigned_t<char_type>;
			auto const ch{static_cast<unsigned_char_type>(*begin)};
			has_space = ch <= static_cast<unsigned_char_type>(u8' ') &&
						::fast_io::char_category::is_c_space(*begin);
		}
		else
		{
			has_space = ::fast_io::char_category::is_c_space(*begin);
		}
		if (has_space)
		{
			begin = ::fast_io::details::find_space_common_impl<false, true>(begin, end);
			if (begin == end)
			{
				return {begin, ::fast_io::parse_code::end_of_file};
			}
		}
	}

	return ::fast_io::details::scan_decfloat_contiguous_define_impl<char_type, flags, T, use_precision>(
		begin, end, value, precision);
}

template <::std::integral char_type,
		  scan_decfloat_supported_floating_point T>
inline constexpr bool scan_decfloat_terminal_padding_leaf_available{
#if ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC))) &&                  \
	!defined(__AVX512F__) &&                                            \
	(defined(__AVX2__) || defined(__SSE2__))
	sizeof(char_type) == sizeof(char8_t) &&
		::fast_io::details::is_ascii<char_type> &&
#if defined(__GNUC__) && !defined(__clang__)
		/*
		GCC binary32/binary64 amortize the padded parser body on the Linux
		P-core matrix.  Its binary16/bfloat16, binary80, and binary128 bodies do
		not, so their padded CPO shares the ordinary public implementation.
		*/
		(23u <= ::fast_io::details::
					 iec559_traits<::std::remove_cvref_t<T>>::mbits &&
		 ::fast_io::details::
				 iec559_traits<::std::remove_cvref_t<T>>::mbits <= 52u)
#elif defined(__clang__)
		/*
		Clang profits for binary16/bfloat16, binary32, and binary128.  Its
		binary64/binary80 padded CPO likewise shares the ordinary public body.
		*/
		(::fast_io::details::
			 iec559_traits<::std::remove_cvref_t<T>>::mbits <= 23u ||
		 ::fast_io::details::
			 iec559_traits<::std::remove_cvref_t<T>>::mbits == 112u)
#else
		true
#endif
#else
	false
#endif
};

template <::std::integral char_type,
		  scan_decfloat_supported_floating_point T>
inline constexpr bool
	scan_decfloat_terminal_padding_dispatch_available{
		scan_decfloat_terminal_padding_leaf_available<char_type, T> &&
#if defined(__GNUC__) && !defined(__clang__)
	/*
	No measured GCC decimal type amortizes terminal dispatch against the
	unchanged public context baseline.  Callers may still invoke the padded CPO
	directly, while padded views retain ordinary public dispatch.
	*/
		false
#elif defined(__clang__)
		/*
		Clang binary16/bfloat16 amortize terminal dispatch at the public view
		boundary.  Binary32 and wider types keep the ordinary context path.
		*/
		::fast_io::details::
			iec559_traits<::std::remove_cvref_t<T>>::mbits <= 10u
#else
		false
#endif
};

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T,
		  bool use_precision = false>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_contiguous_padding_define(
	char_type const *begin, char_type const *end, T &value,
	::std::size_t precision, ::std::size_t padding) noexcept
{
	if constexpr (
		!::fast_io::details::
			scan_decfloat_terminal_padding_leaf_available<char_type, T>)
	{
		(void)padding;
		return ::fast_io::details::
			scan_decfloat_contiguous_define<char_type, flags, T,
										   use_precision>(
				begin, end, value, precision);
	}
	else
	{
		/*
		The padded parser can help only after the exact retained-prefix capacity
		has been crossed.  Keeping shorter inputs on the ordinary entry avoids a
		second large parser body in the common decimal path.
		*/
		constexpr ::std::size_t minimum_semantic_size{
			::fast_io::details::scan_decfloat_exact_digit_capacity<T> +
			1u};
		if (padding == 0u ||
			static_cast<::std::size_t>(end - begin) <
				minimum_semantic_size)
		{
			return ::fast_io::details::
				scan_decfloat_contiguous_define<char_type, flags, T,
											   use_precision>(
					begin, end, value, precision);
		}
		if constexpr (!flags.noskipws)
		{
			if (begin == end)
			{
				return {begin, ::fast_io::parse_code::end_of_file};
			}
			bool has_space{};
			if constexpr (
				sizeof(char_type) == sizeof(char8_t) &&
				::fast_io::details::is_ascii<char_type>)
			{
				using unsigned_char_type =
					::fast_io::details::my_make_unsigned_t<char_type>;
				auto const ch{
					static_cast<unsigned_char_type>(*begin)};
				has_space =
					ch <= static_cast<unsigned_char_type>(u8' ') &&
					::fast_io::char_category::is_c_space(*begin);
			}
			else
			{
				has_space =
					::fast_io::char_category::is_c_space(*begin);
			}
			if (has_space)
			{
				begin =
					::fast_io::details::find_space_common_impl<
						false, true>(begin, end);
				if (begin == end)
				{
					return {
						begin,
						::fast_io::parse_code::end_of_file};
				}
			}
		}
		return ::fast_io::details::
			scan_decfloat_contiguous_padding_define_impl<
				char_type, flags, T, use_precision>(
				begin, end, value, precision, padding);
	}
}

enum class scan_decfloat_context_phase : ::std::uint_least8_t
{
	start,
	after_sign,
	integer,
	fraction,
	exponent_start,
	exponent_after_sign,
	exponent_digits,
	special
};

template <::std::integral char_type, typename T = double>
struct scan_decfloat_context
{
	::fast_io::details::scan_decfloat_significand_state<T> significand_state;
	::fast_io::details::scan_floating_context<char_type> special_buffer;
	::std::uint_least64_t exponent{};
	::fast_io::details::scan_decfloat_context_phase phase{};
	char_type sign_char{};
	bool negative{};
	bool has_sign{};
	bool has_decimal_point{};
	bool has_exponent_marker{};
	bool exponent_negative{};
	bool exponent_has_digit{};
	bool exponent_overflow{};
	bool special_sign_prefixed{};
};

template <::std::integral char_type, typename T>
inline constexpr void scan_decfloat_context_append_exponent_digit(
	::fast_io::details::scan_decfloat_context<char_type, T> &state, char8_t digit) noexcept
{
	state.exponent_has_digit = true;
	constexpr auto exponent_limit{
		static_cast<::std::uint_least64_t>((::std::numeric_limits<::std::int_least64_t>::max)())};
	auto const value{static_cast<::std::uint_least64_t>(digit)};
	if (state.exponent > (exponent_limit - value) / 10u)
	{
		state.exponent = exponent_limit;
		state.exponent_overflow = true;
	}
	else if (!state.exponent_overflow)
	{
		state.exponent = state.exponent * 10u + value;
	}
}

template <::std::integral char_type, typename T>
[[nodiscard]] inline constexpr ::std::int_least64_t
scan_decfloat_context_exponent(::fast_io::details::scan_decfloat_context<char_type, T> const &state) noexcept
{
	return state.exponent_negative ? -static_cast<::std::int_least64_t>(state.exponent)
								   : static_cast<::std::int_least64_t>(state.exponent);
}

template <typename T, ::fast_io::manipulators::scalar_flags flags, bool use_precision,
		  ::std::integral char_type>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_context_assign(::fast_io::details::scan_decfloat_context<char_type, T> const &state, T &value,
							 ::std::size_t precision = 0) noexcept
{
	if (!state.significand_state.has_digit)
	{
		return ::fast_io::parse_code::invalid;
	}
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::scientific)
	{
		if (!state.has_exponent_marker || !state.exponent_has_digit)
		{
			return ::fast_io::parse_code::invalid;
		}
	}
	if (!state.significand_state.has_nonzero_digit)
	{
		value = state.negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
		return ::fast_io::parse_code::ok;
	}
	auto const exponent{state.exponent_has_digit
							? ::fast_io::details::scan_decfloat_context_exponent(state)
							: ::std::int_least64_t{}};
	if constexpr (use_precision)
	{
		return ::fast_io::details::scan_decfloat_assign_precision<T, flags.precision, flags.rounding>(
			value, state.negative, state.significand_state, exponent, precision);
	}
	else
	{
		if constexpr (flags.rounding == ::fast_io::manipulators::floating_rounding::current_environment)
		{
			return ::fast_io::details::scan_decfloat_assign_current_environment(
				value, state.negative, state.significand_state, exponent);
		}
		else
		{
			return ::fast_io::details::scan_decfloat_assign<T, flags.rounding>(
				value, state.negative, state.significand_state, exponent);
		}
	}
}

template <::std::integral char_type, typename T>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_context_append_special_prefix(::fast_io::details::scan_decfloat_context<char_type, T> &state,
											char_type const *chunk_begin) noexcept
{
	if (state.has_sign && !state.special_sign_prefixed)
	{
		if (!state.special_buffer.reserve(state.special_buffer.size + 1u))
		{
			return {chunk_begin, ::fast_io::parse_code::overflow};
		}
		state.special_buffer.data()[state.special_buffer.size] = state.sign_char;
		++state.special_buffer.size;
		state.special_sign_prefixed = true;
	}
	return {chunk_begin, ::fast_io::parse_code::partial};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_context_special_define(::fast_io::details::scan_decfloat_context<char_type, T> &state,
									 char_type const *begin, char_type const *end, T &value,
									 ::std::size_t precision = 0) noexcept
{
	auto prefix_result{::fast_io::details::scan_decfloat_context_append_special_prefix(state, begin)};
	if (prefix_result.code != ::fast_io::parse_code::partial)
	{
		return prefix_result;
	}
	auto const old_size{state.special_buffer.size};
	auto const append_result{::fast_io::details::scan_floating_context_append(state.special_buffer, begin, end)};
	if (append_result.code != ::fast_io::parse_code::partial)
	{
		return {append_result.iter, append_result.code};
	}
	T parsed_value{};
	auto const *buffer_begin{state.special_buffer.data()};
	auto const *buffer_end{buffer_begin + state.special_buffer.size};
	::fast_io::parse_result<char_type const *> parse_result;
	if constexpr (use_precision)
	{
		parse_result = ::fast_io::details::scan_decfloat_contiguous_define<char_type, flags, T, true>(
			buffer_begin, buffer_end, parsed_value, precision);
	}
	else
	{
		parse_result = ::fast_io::details::scan_decfloat_contiguous_define<char_type, flags>(
			buffer_begin, buffer_end, parsed_value);
	}
	if (parse_result.code == ::fast_io::parse_code::ok)
	{
		if (parse_result.iter == buffer_end)
		{
			if (::fast_io::details::scan_hexfloat_special_end_may_extend<flags.nan_payload_scan>(
					buffer_begin, buffer_end))
			{
				if (append_result.truncated)
				{
					return {append_result.iter, ::fast_io::parse_code::overflow};
				}
				return {end, ::fast_io::parse_code::partial};
			}
			value = parsed_value;
			return {::fast_io::details::scan_floating_context_map_iter(
						state.special_buffer, old_size, begin, append_result.iter, parse_result.iter),
					::fast_io::parse_code::ok};
		}
		if (::fast_io::details::scan_hexfloat_special_parse_may_extend<flags.nan_payload_scan>(
				parse_result.iter, buffer_end))
		{
			return {end, ::fast_io::parse_code::partial};
		}
		value = parsed_value;
		return {::fast_io::details::scan_floating_context_map_iter(
					state.special_buffer, old_size, begin, end, parse_result.iter),
				::fast_io::parse_code::ok};
	}
	if (parse_result.iter == buffer_end)
	{
		if (append_result.truncated)
		{
			return {append_result.iter, ::fast_io::parse_code::overflow};
		}
		return {end, ::fast_io::parse_code::partial};
	}
	if (parse_result.code == ::fast_io::parse_code::end_of_file ||
		parse_result.code == ::fast_io::parse_code::partial)
	{
		if (append_result.truncated)
		{
			return {append_result.iter, ::fast_io::parse_code::overflow};
		}
		return {end, ::fast_io::parse_code::partial};
	}
	if (parse_result.code == ::fast_io::parse_code::invalid)
	{
		if (::fast_io::details::
				scan_hexfloat_special_invalid_prefix_may_extend<flags.allow_leading_plus>(
					buffer_begin, buffer_end))
		{
			if (append_result.truncated)
			{
				return {append_result.iter, ::fast_io::parse_code::overflow};
			}
			return {end, ::fast_io::parse_code::partial};
		}
	}
	return {::fast_io::details::scan_floating_context_map_iter(
				state.special_buffer, old_size, begin, end, parse_result.iter),
			parse_result.code};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_context_numeric_define(::fast_io::details::scan_decfloat_context<char_type, T> &state,
									 char_type const *first, char_type const *end, T &value,
									 ::std::size_t precision) noexcept;

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_context_finish_or_exponent(::fast_io::details::scan_decfloat_context<char_type, T> &state,
										 char_type const *first, char_type const *end, T &value,
										 ::std::size_t precision = 0) noexcept
{
	constexpr auto lower_e{::fast_io::char_literal_v<u8'e', char_type>};
	constexpr auto upper_e{::fast_io::char_literal_v<u8'E', char_type>};
	if constexpr (flags.floating != ::fast_io::manipulators::floating_format::fixed)
	{
		if (first != end && (*first == lower_e || *first == upper_e))
		{
			state.has_exponent_marker = true;
			state.phase = ::fast_io::details::scan_decfloat_context_phase::exponent_start;
			++first;
			if (first == end)
			{
				return {first, ::fast_io::parse_code::partial};
			}
			return ::fast_io::details::scan_decfloat_context_numeric_define<char_type, flags, T, use_precision>(
				state, first, end, value, precision);
		}
	}
	auto code{::fast_io::details::scan_decfloat_context_assign<T, flags, use_precision>(state, value, precision)};
	return {first, code};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_context_numeric_define(::fast_io::details::scan_decfloat_context<char_type, T> &state,
									 char_type const *first, char_type const *end, T &value,
									 ::std::size_t precision) noexcept
{
	constexpr auto plus{::fast_io::char_literal_v<u8'+', char_type>};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', char_type>};
	constexpr auto dot{::fast_io::char_literal_v<
		(flags.comma ? u8',' : u8'.'), char_type>};
	for (;;)
	{
		switch (state.phase)
		{
		case ::fast_io::details::scan_decfloat_context_phase::start:
			if (!state.significand_state.has_digit && !state.has_sign)
			{
				first = ::fast_io::details::scan_floating_context_skip_space<char_type, flags.noskipws>(first, end);
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
			}
			if (*first == minus)
			{
				state.negative = true;
				state.has_sign = true;
				state.sign_char = *first;
				++first;
				state.phase = ::fast_io::details::scan_decfloat_context_phase::after_sign;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			if constexpr (flags.allow_leading_plus)
			{
				if (*first == plus)
				{
					state.has_sign = true;
					state.sign_char = *first;
					++first;
					state.phase = ::fast_io::details::scan_decfloat_context_phase::after_sign;
					if (first == end)
					{
						return {end, ::fast_io::parse_code::partial};
					}
					break;
				}
			}
			state.phase = ::fast_io::details::scan_decfloat_context_phase::integer;
			break;
		case ::fast_io::details::scan_decfloat_context_phase::after_sign:
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			state.phase = ::fast_io::details::scan_decfloat_context_phase::integer;
			break;
		case ::fast_io::details::scan_decfloat_context_phase::integer:
		{
			char8_t digit{};
			if (first != end && *first == dot)
			{
				state.has_decimal_point = true;
				state.phase = ::fast_io::details::scan_decfloat_context_phase::fraction;
				++first;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			auto const *before_digits{first};
			first = ::fast_io::details::scan_decfloat_digits<T, char_type>(
				first, end, false, state.significand_state);
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			if (first == before_digits && !state.significand_state.has_digit &&
				!::fast_io::details::scan_decfloat_decimal_digit(*first, digit))
			{
				if (::fast_io::details::scan_decfloat_special_start_char(*first))
				{
					state.phase = ::fast_io::details::scan_decfloat_context_phase::special;
					return ::fast_io::details::scan_decfloat_context_special_define<char_type, flags, T, use_precision>(
						state, first, end, value, precision);
				}
				return {first, ::fast_io::parse_code::invalid};
			}
			if (first != end && *first == dot)
			{
				state.has_decimal_point = true;
				state.phase = ::fast_io::details::scan_decfloat_context_phase::fraction;
				++first;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			return ::fast_io::details::scan_decfloat_context_finish_or_exponent<char_type, flags, T, use_precision>(
				state, first, end, value, precision);
		}
		case ::fast_io::details::scan_decfloat_context_phase::fraction:
			first = ::fast_io::details::scan_decfloat_digits<T, char_type>(
				first, end, true, state.significand_state);
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			if (!state.significand_state.has_digit)
			{
				return {first, ::fast_io::parse_code::invalid};
			}
			return ::fast_io::details::scan_decfloat_context_finish_or_exponent<char_type, flags, T, use_precision>(
				state, first, end, value, precision);
		case ::fast_io::details::scan_decfloat_context_phase::exponent_start:
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			if (*first == minus)
			{
				state.exponent_negative = true;
				++first;
				state.phase = ::fast_io::details::scan_decfloat_context_phase::exponent_after_sign;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			if (*first == plus)
			{
				++first;
				state.phase = ::fast_io::details::scan_decfloat_context_phase::exponent_after_sign;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			state.phase = ::fast_io::details::scan_decfloat_context_phase::exponent_digits;
			break;
		case ::fast_io::details::scan_decfloat_context_phase::exponent_after_sign:
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			state.phase = ::fast_io::details::scan_decfloat_context_phase::exponent_digits;
			break;
		case ::fast_io::details::scan_decfloat_context_phase::exponent_digits:
		{
			char8_t digit{};
			for (; first != end && ::fast_io::details::scan_decfloat_decimal_digit(*first, digit); ++first)
			{
				::fast_io::details::scan_decfloat_context_append_exponent_digit(state, digit);
			}
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			if (!state.exponent_has_digit)
			{
				if constexpr (flags.floating == ::fast_io::manipulators::floating_format::scientific)
				{
					return {first, ::fast_io::parse_code::invalid};
				}
			}
			auto code{::fast_io::details::scan_decfloat_context_assign<T, flags, use_precision>(
				state, value, precision)};
			return {first, code};
		}
		case ::fast_io::details::scan_decfloat_context_phase::special:
			return ::fast_io::details::scan_decfloat_context_special_define<char_type, flags, T, use_precision>(
				state, first, end, value, precision);
		}
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_context_eof(::fast_io::details::scan_decfloat_context<char_type, T> &state, T &value,
						  ::std::size_t precision = 0) noexcept
{
	if (state.phase == ::fast_io::details::scan_decfloat_context_phase::special)
	{
		if (!state.special_buffer.size)
		{
			return ::fast_io::parse_code::end_of_file;
		}
		T parsed_value{};
		auto const *buffer_begin{state.special_buffer.data()};
		auto const *buffer_end{buffer_begin + state.special_buffer.size};
		::fast_io::parse_result<char_type const *> parse_result;
		if constexpr (use_precision)
		{
			parse_result = ::fast_io::details::scan_decfloat_contiguous_define<char_type, flags, T, true>(
				buffer_begin, buffer_end, parsed_value, precision);
		}
		else
		{
			parse_result = ::fast_io::details::scan_decfloat_contiguous_define<char_type, flags>(
				buffer_begin, buffer_end, parsed_value);
		}
		if (parse_result.code == ::fast_io::parse_code::ok)
		{
			value = parsed_value;
		}
		return parse_result.code;
	}
	if (state.phase == ::fast_io::details::scan_decfloat_context_phase::start && !state.significand_state.has_digit &&
		!state.has_sign)
	{
		return ::fast_io::parse_code::end_of_file;
	}
	return ::fast_io::details::scan_decfloat_context_assign<T, flags, use_precision>(state, value, precision);
}

} // namespace details

template <details::scan_decfloat_supported_floating_point T>
inline constexpr ::fast_io::manipulators::scalar_manip_t<::fast_io::manipulators::floating_point_default_scalar_flags,
														 T &>
scan_alias_define(io_alias_t, T &value) noexcept
{
	return {value};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_contiguous_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
					   char_type const *begin, char_type const *end,
					   ::fast_io::manipulators::scalar_manip_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_decfloat_contiguous_define<char_type, flags>(begin, end, value.reference);
}

/*
Let B and E be the original semantic endpoints. Whitespace, sign,
significand, radix-point, and exponent handling advance only by guarded
pointer iteration over [B,E]; malformed optional exponents rewind only to a
saved cursor from that same span. The speculative short and special-value
classifiers publish an iterator only when handled or matched, so their null
miss sentinels are unobservable. Numeric conversion returns only a parse_code
and therefore cannot replace the fixed lexical cursor. Hence every public
result has the form B+k, 0 <= k <= E-B, with B's array provenance,
independently of success, partial input, EOF, invalid syntax, Inf/NaN,
overflow, underflow, or rounding policy.

This proof belongs only to the ordinary decimal scalar CPO. Precision,
padding, context-state, and hexadecimal scanners have independent protocols
and cannot inherit it from a shared floating representation.
*/
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::std::true_type scan_contiguous_result_in_range(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return {};
}

/// @brief Scans a decimal floating value with a provider-proved readable tail.
/// @details The dispatcher supplies the true semantic `[begin,end)` and P readable elements after `end`.  Decimal
///          SIMD code may use P only to complete a 16- or 32-byte unmasked load after the exact significand limit; it
///          caps the valid-lane count at `end` and reduces sticky bits over semantic lanes only.  All other grammar
///          branches remain bounded by `end`.  Therefore the returned iterator never enters padding and changing
///          padding values cannot affect the parsed value, status, or rounding.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_contiguous_padding_define(
	io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>> tag,
	char_type const *begin, char_type const *end, ::std::size_t padding,
	::fast_io::manipulators::scalar_manip_t<flags, T &> value) noexcept
{
	(void)tag;
	if constexpr (
		!::fast_io::details::
			scan_decfloat_terminal_padding_leaf_available<char_type, T>)
	{
		(void)padding;
		return scan_contiguous_define(tag, begin, end, value);
	}
	else
	{
		return ::fast_io::details::
			scan_decfloat_contiguous_padding_define<char_type, flags>(
				begin, end, value.reference, 0u, padding);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_contiguous_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_precision_t<flags, T &>>,
					   char_type const *begin, char_type const *end,
					   ::fast_io::manipulators::scalar_manip_precision_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_decfloat_contiguous_define<char_type, flags, T, true>(
		begin, end, value.reference, value.precision);
}

/// @brief Scans a precision-controlled decimal floating value with a provider-proved readable tail.
/// @details This is the precision-manipulator form of the padded decimal protocol.  Precision changes only the final
///          rounding assignment; the same SIMD proof bounds physical reads by `[begin,end+padding)`, semantic progress
///          by `[begin,end]`, and sticky-bit accumulation by characters before `end`.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_contiguous_padding_define(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_precision_t<flags, T &>> tag,
	char_type const *begin, char_type const *end, ::std::size_t padding,
	::fast_io::manipulators::scalar_manip_precision_t<flags, T &> value) noexcept
{
	(void)tag;
	if constexpr (
		!::fast_io::details::
			scan_decfloat_terminal_padding_leaf_available<char_type, T>)
	{
		(void)padding;
		return scan_contiguous_define(tag, begin, end, value);
	}
	else
	{
		return ::fast_io::details::
			scan_decfloat_contiguous_padding_define<
				char_type, flags, T, true>(
				begin, end, value.reference, value.precision, padding);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr auto
scan_context_type(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return io_type_t<::fast_io::details::scan_decfloat_context<char_type, T>>{};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_context_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
					::fast_io::details::scan_decfloat_context<char_type, T> &state, char_type const *begin,
					char_type const *end,
					::fast_io::manipulators::scalar_manip_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_decfloat_context_numeric_define<char_type, flags, T, false>(
		state, begin, end, value.reference, 0);
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_code
scan_context_eof_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
						::fast_io::details::scan_decfloat_context<char_type, T> &state,
						::fast_io::manipulators::scalar_manip_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_decfloat_context_eof<char_type, flags, T, false>(state, value.reference);
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::std::size_t
scan_context_eof_rewind_size(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
							 ::fast_io::details::scan_decfloat_context<char_type, T> &state,
							 ::fast_io::manipulators::scalar_manip_t<flags, T &>) noexcept
{
	if (state.phase == ::fast_io::details::scan_decfloat_context_phase::special)
	{
		return state.special_buffer.size;
	}
	return 0u;
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr auto
scan_context_type(io_reserve_type_t<char_type,
									::fast_io::manipulators::scalar_manip_precision_t<flags, T &>>) noexcept
{
	return io_type_t<::fast_io::details::scan_decfloat_context<char_type, T>>{};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_context_define(io_reserve_type_t<char_type,
									  ::fast_io::manipulators::scalar_manip_precision_t<flags, T &>>,
					::fast_io::details::scan_decfloat_context<char_type, T> &state, char_type const *begin,
					char_type const *end,
					::fast_io::manipulators::scalar_manip_precision_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_decfloat_context_numeric_define<char_type, flags, T, true>(
		state, begin, end, value.reference, value.precision);
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_code
scan_context_eof_define(io_reserve_type_t<char_type,
										  ::fast_io::manipulators::scalar_manip_precision_t<flags, T &>>,
						::fast_io::details::scan_decfloat_context<char_type, T> &state,
						::fast_io::manipulators::scalar_manip_precision_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_decfloat_context_eof<char_type, flags, T, true>(
		state, value.reference, value.precision);
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::std::size_t
scan_context_eof_rewind_size(io_reserve_type_t<char_type,
											   ::fast_io::manipulators::scalar_manip_precision_t<flags, T &>>,
							 ::fast_io::details::scan_decfloat_context<char_type, T> &state,
							 ::fast_io::manipulators::scalar_manip_precision_t<flags, T &>) noexcept
{
	if (state.phase == ::fast_io::details::scan_decfloat_context_phase::special)
	{
		return state.special_buffer.size;
	}
	return 0u;
}

/*
Range theorem for the ordinary decimal context transition. Let [B,E] be the
closed cursor range supplied to scan_context_define and let S be any
protocol-valid state. In the normal numeric path, whitespace and digit scans
return a cursor in [B,E], every direct increment is guarded by cursor != E,
and recursive phase transitions receive that cursor unchanged. Consequently,
normal success, invalid, overflow, and partial results return either the
current cursor or E.

The special-prefix helper returns B, but its partial result is internal and
processing continues; only allocation failure publishes B, with overflow.
scan_floating_context_append likewise returns only B or E. The decimal
contiguous sub-parser's lexical range theorem places its iterator in the
special buffer's closed range with that buffer's provenance. Consequently,
scan_floating_context_map_iter may subtract it from the buffer base and is the
sole conversion back to the caller's chunk: it clamps an iterator in the old
buffered prefix to B, an offset beyond this chunk to E, and otherwise returns B
plus an offset no larger than E-B. Thus Inf/NaN success, invalid payloads,
truncation, and allocation failure also preserve the theorem.

A partial normal transition reaches E; a partial special transition returns E
(including the empty span, where B == E). Therefore this certificate neither
hides a nonempty no-progress transition nor weakens the dispatcher's separate
partial/no-progress checks. EOF does not return an iterator. Dynamic precision
changes only final numeric assignment and shares the same lexical state
machine, so the proof applies identically to the precision CPO. Formally, for
both overloads below and every parse code, the returned iterator R has B's
array provenance and satisfies 0 <= R-B <= E-B.

This ADL proof is intentionally restricted to these decimal scalar protocols.
It establishes nothing for hexadecimal, custom, transcoding, or string
scanners, even when those scanners happen to use a related representation.
*/
template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating !=
			 ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::std::true_type scan_context_result_in_range(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return {};
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating !=
			 ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::std::true_type scan_context_result_in_range(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::
			scalar_manip_precision_t<flags, T &>>) noexcept
{
	return {};
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating !=
				 ::fast_io::manipulators::floating_format::hexfloat &&
			 ::fast_io::details::
				 scan_decfloat_terminal_padding_dispatch_available<
					 char_type, T>)
inline constexpr ::std::true_type
scan_context_terminal_padding_equivalent(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return {};
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating !=
				 ::fast_io::manipulators::floating_format::hexfloat &&
			 ::fast_io::details::
				 scan_decfloat_terminal_padding_dispatch_available<
					 char_type, T>)
inline constexpr ::std::true_type
scan_context_terminal_padding_equivalent(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::
			scalar_manip_precision_t<flags, T &>>) noexcept
{
	return {};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags, details::my_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat &&
			 !details::scan_decfloat_supported_floating_point<T>)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_contiguous_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
					   char_type const *begin, char_type const *,
					   ::fast_io::manipulators::scalar_manip_t<flags, T &>) noexcept
{
	static_assert(details::scan_decfloat_supported_floating_point<T>,
				  "fast_io decimal floating scan does not support this floating-point type");
	return {begin, ::fast_io::parse_code::invalid};
}

} // namespace fast_io

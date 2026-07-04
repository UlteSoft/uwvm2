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

template <typename T>
concept scan_decfloat_supported_floating_point =
	::fast_io::details::my_floating_point<::std::remove_cvref_t<T>> &&
	::fast_io::details::scan_decfloat_compute_supported<T>;

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
	if constexpr (sizeof(char_type) == sizeof(char8_t) && !::fast_io::details::is_ebcdic<char_type>)
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
	return false;
}

struct scan_decfloat_significand_state
{
	::std::uint_least64_t significand{};
	::std::uint_least64_t significant_digits{};
	::std::uint_least64_t stored_digits{};
	::std::uint_least64_t fractional_digits{};
	bool has_digit{};
	bool has_nonzero_digit{};
	bool truncated_nonzero{};
};

template <::std::integral char_type>
struct scan_decfloat_fast_result
{
	char_type const *iter{};
	::fast_io::parse_code code{};
	bool handled{};
};

inline constexpr ::std::uint_least64_t scan_decfloat_significand_digit_limit{19u};
inline constexpr ::std::int_least32_t scan_decfloat_dragonbox_min_power10{-342};
inline constexpr ::std::int_least32_t scan_decfloat_dragonbox_max_power10{326};
inline constexpr ::std::uint_least64_t scan_decfloat_pow10_0_to_8_table[]{
	1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u, 10000000u, 100000000u};

struct scan_decfloat_adjusted_mantissa
{
	::std::uint_least64_t mantissa{};
	::std::int_least32_t power2{};
};

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
		if (((mantissa & 1u) != 0u || has_tail) &&
			::fast_io::details::scan_decfloat_directed_round_up<rounding>(negative))
		{
			++mantissa;
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
		if (significand == 0 || exponent < ::fast_io::details::scan_decfloat_smallest_power10<no_cvref_t>())
		{
			answer = {};
			return true;
		}
		if (exponent > ::fast_io::details::scan_decfloat_largest_power10<no_cvref_t>())
		{
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

inline constexpr void scan_decfloat_append_digit(scan_decfloat_significand_state &state, char8_t digit,
												 bool after_decimal) noexcept
{
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
	if (state.stored_digits != ::fast_io::details::scan_decfloat_significand_digit_limit)
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
	return ((((val + 0x4646464646464646u) | (val - 0x3030303030303030u)) &
			 0x8080808080808080u) == 0);
}

[[nodiscard]] inline constexpr ::std::uint_least32_t
scan_decfloat_ascii8_parse(::std::uint_least64_t val) noexcept
{
	constexpr ::std::uint_least64_t mask{0x000000FF000000FFu};
	constexpr ::std::uint_least64_t mul1{0x000F424000000064u};
	constexpr ::std::uint_least64_t mul2{0x0000271000000001u};
	val -= 0x3030303030303030u;
	val = (val * 10u) + (val >> 8u);
	val = (((val & mask) * mul1) + (((val >> 16u) & mask) * mul2)) >> 32u;
	return static_cast<::std::uint_least32_t>(val);
}

inline constexpr void scan_decfloat_append_eight_digits(scan_decfloat_significand_state &state,
														bool after_decimal,
														::std::uint_least32_t digits) noexcept
{
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
	if (state.stored_digits == ::fast_io::details::scan_decfloat_significand_digit_limit)
	{
		if (digits != 0)
		{
			state.truncated_nonzero = true;
		}
		return;
	}
	auto const available{::fast_io::details::scan_decfloat_significand_digit_limit - state.stored_digits};
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
	state.stored_digits = ::fast_io::details::scan_decfloat_significand_digit_limit;
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
					 scan_decfloat_significand_state &state) noexcept
{
	char8_t digit{};
	if constexpr (sizeof(char_type) == sizeof(char8_t) && !::fast_io::details::is_ebcdic<char_type> &&
				  ::std::numeric_limits<::std::uint_least64_t>::digits == 64u &&
				  ::fast_io::details::scan_decfloat_binary64_like<T>)
	{
#ifdef __cpp_if_consteval
		if !consteval
#else
		if (!__builtin_is_constant_evaluated())
#endif
		{
			for (; first != last && !state.has_nonzero_digit; ++first)
			{
				if (!::fast_io::details::scan_decfloat_decimal_digit(*first, digit))
				{
					return first;
				}
				::fast_io::details::scan_decfloat_append_digit(state, digit, after_decimal);
			}
			if (static_cast<::std::size_t>(last - first) >= 2u * sizeof(::std::uint_least64_t))
			{
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
					::fast_io::details::scan_decfloat_append_eight_digits(
						state, after_decimal, ::fast_io::details::scan_decfloat_ascii8_parse(val));
					first += sizeof(::std::uint_least64_t);
				}
			}
		}
	}
	for (; first != last && ::fast_io::details::scan_decfloat_decimal_digit(*first, digit); ++first)
	{
		::fast_io::details::scan_decfloat_append_digit(state, digit, after_decimal);
	}
	return first;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_exponent(char_type const *first, char_type const *last, ::std::int_least64_t &exponent) noexcept
{
	return ::fast_io::details::scan_hexfloat_exponent(first, last, exponent);
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
																		  scan_decfloat_significand_state const &state,
																		  ::std::int_least64_t exponent) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	auto adjusted_exponent{::fast_io::details::scan_decfloat_saturating_add(
		exponent, -static_cast<::std::int_least64_t>(state.fractional_digits))};
	adjusted_exponent = ::fast_io::details::scan_decfloat_saturating_add(
		adjusted_exponent,
		static_cast<::std::int_least64_t>(state.significant_digits - state.stored_digits));
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		if (!state.truncated_nonzero &&
			::fast_io::details::scan_decfloat_try_clinger(state.significand, adjusted_exponent, negative, value))
		{
			return ::fast_io::parse_code::ok;
		}
	}
	::fast_io::details::scan_decfloat_adjusted_mantissa adjusted;
	if (!state.truncated_nonzero)
	{
		if (::fast_io::details::scan_decfloat_compute_adjusted<no_cvref_t, rounding>(
				adjusted_exponent, state.significand, negative, adjusted))
		{
			return ::fast_io::details::scan_decfloat_assign_adjusted(value, negative, state.significand, adjusted);
		}
	}
	else
	{
		::fast_io::details::scan_decfloat_adjusted_mantissa adjusted_next;
		if (::fast_io::details::scan_decfloat_compute_adjusted<no_cvref_t, rounding>(
				adjusted_exponent, state.significand, negative, adjusted) &&
			::fast_io::details::scan_decfloat_compute_adjusted<no_cvref_t, rounding>(
				adjusted_exponent, state.significand + 1u, negative, adjusted_next) &&
			adjusted.mantissa == adjusted_next.mantissa && adjusted.power2 == adjusted_next.power2)
		{
			return ::fast_io::details::scan_decfloat_assign_adjusted(value, negative, state.significand, adjusted);
		}
	}
	return ::fast_io::parse_code::partial;
}

template <typename T, ::fast_io::manipulators::floating_rounding rounding =
						  ::fast_io::manipulators::floating_rounding::nearest_to_even>
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
		if (::fast_io::details::scan_decfloat_try_clinger(significand, adjusted_exponent, negative, value))
		{
			return ::fast_io::parse_code::ok;
		}
	}
	::fast_io::details::scan_decfloat_adjusted_mantissa adjusted;
	if (::fast_io::details::scan_decfloat_compute_adjusted<no_cvref_t, rounding>(
			adjusted_exponent, significand, negative, adjusted))
	{
		return ::fast_io::details::scan_decfloat_assign_adjusted(value, negative, significand, adjusted);
	}
	return ::fast_io::parse_code::partial;
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr scan_decfloat_fast_result<char_type>
scan_decfloat_contiguous_short_define_impl(char_type const *begin, char_type const *first,
										   char_type const *end, bool negative, T &value) noexcept
{
	if constexpr (sizeof(char_type) != sizeof(char8_t) || ::fast_io::details::is_ebcdic<char_type>)
	{
		return {};
	}
	else
	{
#ifdef __cpp_if_consteval
		if consteval
		{
			return {};
		}
		else
#else
		if (__builtin_is_constant_evaluated())
		{
			return {};
		}
		else
#endif
		{
			::std::uint_least64_t significand{};
			::std::uint_least64_t digit_count{};
			::std::uint_least64_t fractional_digits{};
			char8_t digit{};
			for (; first != end && ::fast_io::details::scan_decfloat_decimal_digit(*first, digit); ++first)
			{
				if (digit_count == ::fast_io::details::scan_decfloat_significand_digit_limit)
				{
					return {};
				}
				significand = significand * 10u + static_cast<::std::uint_least64_t>(digit);
				++digit_count;
			}

			constexpr auto dot{::fast_io::char_literal_v<u8'.', char_type>};
			if (first != end && *first == dot)
			{
				++first;
				auto const *fraction_begin{first};
				for (; digit_count + 8u <= ::fast_io::details::scan_decfloat_significand_digit_limit &&
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
				}
				for (; first != end && ::fast_io::details::scan_decfloat_decimal_digit(*first, digit); ++first)
				{
					if (digit_count == ::fast_io::details::scan_decfloat_significand_digit_limit)
					{
						return {};
					}
					significand = significand * 10u + static_cast<::std::uint_least64_t>(digit);
					++digit_count;
				}
				fractional_digits = static_cast<::std::uint_least64_t>(first - fraction_begin);
			}

			if (digit_count == 0)
			{
				return {begin, ::fast_io::parse_code::invalid, true};
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

			return {first,
					::fast_io::details::scan_decfloat_assign_short<T, flags.rounding>(
						value, negative, significand, fractional_digits, exponent),
					true};
		}
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_contiguous_define_impl(char_type const *begin, char_type const *end, T &value) noexcept
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
	else if (first != end && *first == plus)
	{
		return {begin, ::fast_io::parse_code::invalid};
	}

	constexpr auto dot{::fast_io::char_literal_v<u8'.', char_type>};
	char8_t first_digit{};
	bool const numeric_candidate{
		first != end && (*first == dot ||
						 ::fast_io::details::scan_decfloat_decimal_digit(*first, first_digit))};
	if (numeric_candidate)
	{
		auto const short_result{
			::fast_io::details::scan_decfloat_contiguous_short_define_impl<char_type, flags>(begin, first, end,
																							 negative, value)};
		if (short_result.handled)
		{
			return {short_result.iter, short_result.code};
		}
	}

	auto const special_result{
		::fast_io::details::scan_hexfloat_special_value<flags.nan_parse_sign, flags.nan_payload_scan>(
			first, end, negative, value)};
	if (special_result.matched)
	{
		return {special_result.iter, special_result.code};
	}

	::fast_io::details::scan_decfloat_significand_state significand_state;
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
	return {first, ::fast_io::details::scan_decfloat_assign<T, flags.rounding>(
					   value, negative, significand_state, exponent)};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_contiguous_define(char_type const *begin, char_type const *end, T &value) noexcept
{
	if constexpr (!flags.noskipws)
	{
		if (begin == end)
		{
			return {begin, ::fast_io::parse_code::end_of_file};
		}
		bool has_space{};
		if constexpr (sizeof(char_type) == sizeof(char8_t) && !::fast_io::details::is_ebcdic<char_type>)
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

	return ::fast_io::details::scan_decfloat_contiguous_define_impl<char_type, flags>(begin, end, value);
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

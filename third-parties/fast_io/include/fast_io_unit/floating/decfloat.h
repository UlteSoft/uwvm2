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

inline constexpr bool scan_decfloat_clinger_environment_independent{
#if defined(FAST_IO_HAS_FLOATING_POINT_ENVIRONMENT)
	false
#else
	true
#endif
};

inline constexpr ::std::size_t scan_decfloat_exact_digit_capacity{768u};

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
	char8_t exact_digits[scan_decfloat_exact_digit_capacity]{};
};

template <::std::integral char_type>
struct scan_decfloat_fast_result
{
	char_type const *iter{};
	::fast_io::parse_code code{};
	bool handled{};
};

inline constexpr ::std::uint_least64_t scan_decfloat_uint64_significand_digit_limit{19u};

template <typename T>
inline constexpr ::std::uint_least64_t scan_decfloat_significand_digit_limit{
	[]() constexpr noexcept {
		using trait = ::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>;
		constexpr ::std::uint_least64_t digits{static_cast<::std::uint_least64_t>(trait::m10digits) + 2u};
		if constexpr (digits < ::fast_io::details::scan_decfloat_uint64_significand_digit_limit)
		{
			return digits;
		}
		else
		{
			return ::fast_io::details::scan_decfloat_uint64_significand_digit_limit;
		}
	}()};
inline constexpr ::std::int_least32_t scan_decfloat_dragonbox_min_power10{-342};
inline constexpr ::std::int_least32_t scan_decfloat_dragonbox_max_power10{326};
inline constexpr ::std::uint_least64_t scan_decfloat_pow10_0_to_8_table[]{
	1u, 10u, 100u, 1000u, 10000u, 100000u, 1000000u, 10000000u, 100000000u};

inline constexpr void scan_decfloat_append_exact_digit(scan_decfloat_significand_state &state,
													   char8_t digit) noexcept
{
	if (state.exact_stored_digits != ::fast_io::details::scan_decfloat_exact_digit_capacity)
	{
		state.exact_digits[state.exact_stored_digits] = digit;
		++state.exact_stored_digits;
	}
	else if (digit != 0)
	{
		state.exact_truncated_nonzero = true;
	}
}

inline constexpr void scan_decfloat_append_exact_eight_digits(scan_decfloat_significand_state &state,
															  ::std::uint_least32_t digits) noexcept
{
	if (state.exact_stored_digits == ::fast_io::details::scan_decfloat_exact_digit_capacity)
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

[[nodiscard]] inline constexpr ::std::int_least64_t
scan_decfloat_adjusted_exponent(scan_decfloat_significand_state const &state,
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

#ifdef __SIZEOF_INT128__
	inline constexpr ::std::size_t scan_decfloat_bigint_limb_capacity{288u};

	struct scan_decfloat_bigint
	{
		::std::uint_least64_t limb[scan_decfloat_bigint_limb_capacity]{};
		::std::size_t size{};
	};

	inline constexpr void scan_decfloat_bigint_normalize(scan_decfloat_bigint &value) noexcept
	{
		for (; value.size && value.limb[value.size - 1u] == 0u; --value.size)
		{
		}
	}

	inline constexpr void scan_decfloat_bigint_set_u64(scan_decfloat_bigint &value,
													   ::std::uint_least64_t word) noexcept
	{
		value.limb[0] = word;
		value.size = word == 0u ? 0u : 1u;
	}

	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_mul_small(scan_decfloat_bigint &value,
																	   ::std::uint_least32_t multiplier) noexcept
	{
		__uint128_t carry{};
		for (::std::size_t index{}; index != value.size; ++index)
		{
			auto const product{static_cast<__uint128_t>(value.limb[index]) * multiplier + carry};
			value.limb[index] = static_cast<::std::uint_least64_t>(product);
			carry = product >> 64u;
		}
		if (carry)
		{
			if (value.size == ::fast_io::details::scan_decfloat_bigint_limb_capacity)
			{
				return false;
			}
			value.limb[value.size] = static_cast<::std::uint_least64_t>(carry);
			++value.size;
		}
		return true;
	}

	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_add_small(scan_decfloat_bigint &value,
																	   ::std::uint_least32_t addend) noexcept
	{
		if (!value.size)
		{
			::fast_io::details::scan_decfloat_bigint_set_u64(value, addend);
			return true;
		}
		__uint128_t sum{static_cast<__uint128_t>(value.limb[0]) + addend};
		value.limb[0] = static_cast<::std::uint_least64_t>(sum);
		auto carry{sum >> 64u};
		for (::std::size_t index{1u}; carry && index != value.size; ++index)
		{
			sum = static_cast<__uint128_t>(value.limb[index]) + carry;
			value.limb[index] = static_cast<::std::uint_least64_t>(sum);
			carry = sum >> 64u;
		}
		if (carry)
		{
			if (value.size == ::fast_io::details::scan_decfloat_bigint_limb_capacity)
			{
				return false;
			}
			value.limb[value.size] = static_cast<::std::uint_least64_t>(carry);
			++value.size;
		}
		return true;
	}

	inline constexpr bool scan_decfloat_bigint_from_digits(scan_decfloat_bigint &value,
														   scan_decfloat_significand_state const &state) noexcept
	{
		value = {};
		for (::std::size_t index{}; index != state.exact_stored_digits; ++index)
		{
			if (!::fast_io::details::scan_decfloat_bigint_mul_small(value, 10u) ||
				!::fast_io::details::scan_decfloat_bigint_add_small(
					value, static_cast<::std::uint_least32_t>(state.exact_digits[index])))
			{
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] inline constexpr ::std::size_t
	scan_decfloat_bigint_bit_width(scan_decfloat_bigint const &value) noexcept
	{
		if (!value.size)
		{
			return 0u;
		}
		return (value.size - 1u) * 64u +
			   static_cast<::std::size_t>(::std::bit_width(value.limb[value.size - 1u]));
	}

	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_get_bit(scan_decfloat_bigint const &value,
																	 ::std::size_t bit) noexcept
	{
		auto const limb_index{bit / 64u};
		if (limb_index >= value.size)
		{
			return false;
		}
		return ((value.limb[limb_index] >> (bit % 64u)) & 1u) != 0u;
	}

	[[nodiscard]] inline constexpr int scan_decfloat_bigint_compare(scan_decfloat_bigint const &left,
																	scan_decfloat_bigint const &right) noexcept
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

	inline constexpr void scan_decfloat_bigint_sub_assign(scan_decfloat_bigint &left,
														  scan_decfloat_bigint const &right) noexcept
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

	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_shl1_add_bit(scan_decfloat_bigint &value,
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
			if (value.size == ::fast_io::details::scan_decfloat_bigint_limb_capacity)
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

	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_shift_left(scan_decfloat_bigint &out,
																		scan_decfloat_bigint const &in,
																		::std::size_t shift) noexcept
	{
		out = {};
		if (!in.size)
		{
			return true;
		}
		auto const limb_shift{shift / 64u};
		auto const bit_shift{shift % 64u};
		if (in.size + limb_shift + (bit_shift != 0u) >
			::fast_io::details::scan_decfloat_bigint_limb_capacity)
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

	[[nodiscard]] inline constexpr bool scan_decfloat_bigint_pow5(scan_decfloat_bigint &value,
																  ::std::uint_least64_t exponent) noexcept
	{
		::fast_io::details::scan_decfloat_bigint_set_u64(value, 1u);
		for (; exponent; --exponent)
		{
			if (!::fast_io::details::scan_decfloat_bigint_mul_small(value, 5u))
			{
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] inline constexpr ::std::int_least64_t
	scan_decfloat_bigint_floor_log2_ratio(scan_decfloat_bigint const &numerator,
										  scan_decfloat_bigint const &denominator) noexcept
	{
		auto const numerator_bits{::fast_io::details::scan_decfloat_bigint_bit_width(numerator)};
		auto const denominator_bits{::fast_io::details::scan_decfloat_bigint_bit_width(denominator)};
		auto exponent{static_cast<::std::int_least64_t>(numerator_bits) -
					  static_cast<::std::int_least64_t>(denominator_bits)};
		::fast_io::details::scan_decfloat_bigint shifted;
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

	struct scan_decfloat_bigint_div_result
	{
		__uint128_t quotient{};
		int twice_remainder_compare{};
		bool remainder_nonzero{};
		bool quotient_overflow{};
	};

	[[nodiscard]] inline constexpr scan_decfloat_bigint_div_result
	scan_decfloat_bigint_div_shifted_to_u128(scan_decfloat_bigint const &numerator,
											 scan_decfloat_bigint const &denominator,
											 ::std::int_least64_t binary_shift) noexcept
	{
		::fast_io::details::scan_decfloat_bigint divisor;
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
			divisor = denominator;
		}
		auto const dividend_bits{
			::fast_io::details::scan_decfloat_bigint_bit_width(numerator) +
			(binary_shift > 0 ? static_cast<::std::size_t>(binary_shift) : 0u)};
		::fast_io::details::scan_decfloat_bigint remainder;
		__uint128_t quotient{};
		bool quotient_overflow{};
		for (auto bit_index{dividend_bits}; bit_index != 0u;)
		{
			--bit_index;
			bool bit{};
			if (binary_shift > 0)
			{
				auto const shift{static_cast<::std::size_t>(binary_shift)};
				bit = bit_index >= shift &&
					  ::fast_io::details::scan_decfloat_bigint_get_bit(numerator, bit_index - shift);
			}
			else
			{
				bit = ::fast_io::details::scan_decfloat_bigint_get_bit(numerator, bit_index);
			}
			if (!::fast_io::details::scan_decfloat_bigint_shl1_add_bit(remainder, bit))
			{
				return {.quotient_overflow = true};
			}
			if (::fast_io::details::scan_decfloat_bigint_compare(remainder, divisor) >= 0)
			{
				::fast_io::details::scan_decfloat_bigint_sub_assign(remainder, divisor);
				if (bit_index < 128u)
				{
					quotient |= static_cast<__uint128_t>(1u) << bit_index;
				}
				else
				{
					quotient_overflow = true;
				}
			}
		}
		::fast_io::details::scan_decfloat_bigint twice_remainder{remainder};
		(void)::fast_io::details::scan_decfloat_bigint_shl1_add_bit(twice_remainder, false);
		return {.quotient = quotient,
				.twice_remainder_compare =
					::fast_io::details::scan_decfloat_bigint_compare(twice_remainder, divisor),
				.remainder_nonzero = remainder.size != 0u,
				.quotient_overflow = quotient_overflow};
	}

	template <::fast_io::manipulators::floating_rounding rounding>
	[[nodiscard]] inline constexpr bool scan_decfloat_big_round_up(bool negative, __uint128_t quotient,
																   int twice_remainder_compare,
																   bool remainder_nonzero,
																   bool tail_nonzero) noexcept
	{
		if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
		{
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

	template <typename T, ::fast_io::manipulators::floating_rounding rounding =
							  ::fast_io::manipulators::floating_rounding::nearest_to_even>
	[[nodiscard]] inline constexpr ::fast_io::parse_code
	scan_decfloat_assign_big(T &value, bool negative, scan_decfloat_significand_state const &state,
							 ::std::int_least64_t exponent) noexcept
	{
		using no_cvref_t = ::std::remove_cvref_t<T>;
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr ::std::size_t mbits{trait::mbits};
		constexpr ::std::size_t ebits{trait::ebits};
		constexpr ::std::size_t precision_bits{mbits + 1u};
		constexpr auto bias{
			static_cast<::std::int_least64_t>((static_cast<::std::uint_least32_t>(1u) << ebits) >> 1u) - 1};
		constexpr auto min_exponent{1 - bias};
		constexpr auto max_exponent{bias};
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

			::fast_io::details::scan_decfloat_bigint numerator;
			if (!::fast_io::details::scan_decfloat_bigint_from_digits(numerator, state))
			{
				return ::fast_io::details::scan_decfloat_assign_overflow_value<rounding>(value, negative);
			}
			::fast_io::details::scan_decfloat_bigint denominator;
			::fast_io::details::scan_decfloat_bigint_set_u64(denominator, 1u);
			::std::int_least64_t binary_exponent_adjust{};
			if (decimal_exponent >= 0)
			{
				for (::std::int_least64_t counter{}; counter != decimal_exponent; ++counter)
				{
					if (!::fast_io::details::scan_decfloat_bigint_mul_small(numerator, 5u))
					{
						return ::fast_io::details::scan_decfloat_assign_overflow_value<rounding>(value, negative);
					}
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

			auto const division{::fast_io::details::scan_decfloat_bigint_div_shifted_to_u128(
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
			auto const hidden_bit{static_cast<__uint128_t>(1u) << mbits};
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
#endif

	inline constexpr void scan_decfloat_state_from_u64(scan_decfloat_significand_state &state,
													   ::std::uint_least64_t significand) noexcept
	{
		auto const original{significand};
		char8_t buffer[20]{};
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
	if (!remainder && !tail_nonzero)
	{
		return false;
	}
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
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
inline constexpr void scan_decfloat_append_digit(scan_decfloat_significand_state &state, char8_t digit,
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
	return ((((val + 0x4646464646464646u) | (val - 0x3030303030303030u)) &
			 0x8080808080808080u) == 0);
}

[[nodiscard]] inline constexpr bool
scan_decfloat_ascii8_has_nonzero_digit(::std::uint_least64_t val) noexcept
{
	return (val ^ 0x3030303030303030u) != 0;
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

template <typename T>
inline constexpr void scan_decfloat_append_eight_digits(scan_decfloat_significand_state &state,
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
	constexpr auto digit_limit{::fast_io::details::scan_decfloat_significand_digit_limit<T>};
	char8_t digit{};
	if constexpr (sizeof(char_type) == sizeof(char8_t) && !::fast_io::details::is_ebcdic<char_type> &&
				  ::std::numeric_limits<::std::uint_least64_t>::digits == 64u)
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
				::fast_io::details::scan_decfloat_append_digit<T>(state, digit, after_decimal);
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
							::fast_io::details::scan_decfloat_exact_digit_capacity)
						{
							::fast_io::details::scan_decfloat_append_exact_eight_digits(
								state, ::fast_io::details::scan_decfloat_ascii8_parse(val));
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
						::fast_io::details::scan_decfloat_append_eight_digits<T>(
							state, after_decimal, ::fast_io::details::scan_decfloat_ascii8_parse(val));
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
	auto adjusted_exponent{::fast_io::details::scan_decfloat_adjusted_exponent(state, exponent)};
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even &&
				  ::fast_io::details::scan_decfloat_clinger_environment_independent)
	{
		if (!state.truncated_nonzero &&
			::fast_io::details::scan_decfloat_try_clinger(state.significand, adjusted_exponent, negative, value))
		{
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
	}
#ifdef __SIZEOF_INT128__
	return ::fast_io::details::scan_decfloat_assign_big<T, rounding>(value, negative, state, exponent);
#else
	if constexpr (::fast_io::details::scan_decfloat_native_wide_supported<no_cvref_t>)
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
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even &&
				  ::fast_io::details::scan_decfloat_clinger_environment_independent)
	{
		if (::fast_io::details::scan_decfloat_try_clinger(significand, adjusted_exponent, negative, value))
		{
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
	::fast_io::details::scan_decfloat_significand_state state;
	::fast_io::details::scan_decfloat_state_from_u64(state, significand);
	return ::fast_io::details::scan_decfloat_assign_big<T, rounding>(value, negative, state, adjusted_exponent);
#else
	if constexpr (::fast_io::details::scan_decfloat_native_wide_supported<no_cvref_t>)
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
scan_decfloat_assign_precision(T &value, bool negative, scan_decfloat_significand_state const &state,
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
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_assign_short(T &value, bool negative, ::std::uint_least64_t significand,
						   ::std::uint_least64_t fractional_digits,
						   ::std::int_least64_t exponent) noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	auto adjusted_exponent{::fast_io::details::scan_decfloat_saturating_add(
		exponent, -static_cast<::std::int_least64_t>(fractional_digits))};
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even &&
				  ::fast_io::details::scan_decfloat_clinger_environment_independent)
	{
		if (::fast_io::details::scan_decfloat_try_clinger(significand, adjusted_exponent, negative, value))
		{
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
	::fast_io::details::scan_decfloat_significand_state state;
	::fast_io::details::scan_decfloat_state_from_u64(state, significand);
	return ::fast_io::details::scan_decfloat_assign_big<T, rounding>(value, negative, state, adjusted_exponent);
#else
	if constexpr (::fast_io::details::scan_decfloat_native_wide_supported<no_cvref_t>)
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
										 scan_decfloat_significand_state const &state,
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
			constexpr auto digit_limit{::fast_io::details::scan_decfloat_significand_digit_limit<T>};
			::std::uint_least64_t significand{};
			::std::uint_least64_t digit_count{};
			::std::uint_least64_t fractional_digits{};
			char8_t digit{};
			for (; first != end && ::fast_io::details::scan_decfloat_decimal_digit(*first, digit); ++first)
			{
				if (digit_count == digit_limit)
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
				}
				for (; first != end && ::fast_io::details::scan_decfloat_decimal_digit(*first, digit); ++first)
				{
					if (digit_count == digit_limit)
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

	constexpr auto dot{::fast_io::char_literal_v<u8'.', char_type>};
	char8_t first_digit{};
	bool const numeric_candidate{
		first != end && (*first == dot ||
						 ::fast_io::details::scan_decfloat_decimal_digit(*first, first_digit))};
	if (numeric_candidate)
	{
		if constexpr (!use_precision)
		{
			auto const short_result{
				::fast_io::details::scan_decfloat_contiguous_short_define_impl<char_type, flags>(begin, first, end,
																								 negative, value)};
			if (short_result.handled)
			{
				return {short_result.iter, short_result.code};
			}
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

	return ::fast_io::details::scan_decfloat_contiguous_define_impl<char_type, flags, T, use_precision>(
		begin, end, value, precision);
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

template <::std::integral char_type>
struct scan_decfloat_context
{
	::fast_io::details::scan_decfloat_significand_state significand_state;
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

template <::std::integral char_type>
inline constexpr void scan_decfloat_context_append_exponent_digit(
	::fast_io::details::scan_decfloat_context<char_type> &state, char8_t digit) noexcept
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

template <::std::integral char_type>
[[nodiscard]] inline constexpr ::std::int_least64_t
scan_decfloat_context_exponent(::fast_io::details::scan_decfloat_context<char_type> const &state) noexcept
{
	return state.exponent_negative ? -static_cast<::std::int_least64_t>(state.exponent)
								   : static_cast<::std::int_least64_t>(state.exponent);
}

template <typename T, ::fast_io::manipulators::scalar_flags flags, bool use_precision,
		  ::std::integral char_type>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_decfloat_context_assign(::fast_io::details::scan_decfloat_context<char_type> const &state, T &value,
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

template <::std::integral char_type>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_context_append_special_prefix(::fast_io::details::scan_decfloat_context<char_type> &state,
											char_type const *chunk_begin) noexcept
{
	if (state.has_sign && !state.special_sign_prefixed)
	{
		if (state.special_buffer.size == state.special_buffer.capacity)
		{
			return {chunk_begin, ::fast_io::parse_code::overflow};
		}
		state.special_buffer.buffer.index_unchecked(state.special_buffer.size) = state.sign_char;
		++state.special_buffer.size;
		state.special_sign_prefixed = true;
	}
	return {chunk_begin, ::fast_io::parse_code::partial};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_context_special_define(::fast_io::details::scan_decfloat_context<char_type> &state,
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
		return append_result;
	}
	T parsed_value{};
	auto const *buffer_begin{state.special_buffer.buffer.data()};
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
			return {end, ::fast_io::parse_code::partial};
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
		return {end, ::fast_io::parse_code::partial};
	}
	if (parse_result.code == ::fast_io::parse_code::end_of_file ||
		parse_result.code == ::fast_io::parse_code::partial)
	{
		return {end, ::fast_io::parse_code::partial};
	}
	if (parse_result.code == ::fast_io::parse_code::invalid)
	{
		if (buffer_begin != buffer_end &&
			!::fast_io::char_category::is_c_space(*(buffer_end - 1)))
		{
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
scan_decfloat_context_numeric_define(::fast_io::details::scan_decfloat_context<char_type> &state,
									 char_type const *first, char_type const *end, T &value,
									 ::std::size_t precision) noexcept;

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  scan_decfloat_supported_floating_point T, bool use_precision>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_decfloat_context_finish_or_exponent(::fast_io::details::scan_decfloat_context<char_type> &state,
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
scan_decfloat_context_numeric_define(::fast_io::details::scan_decfloat_context<char_type> &state,
									 char_type const *first, char_type const *end, T &value,
									 ::std::size_t precision) noexcept
{
	constexpr auto plus{::fast_io::char_literal_v<u8'+', char_type>};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', char_type>};
	constexpr auto dot{::fast_io::char_literal_v<u8'.', char_type>};
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
				state.phase = ::fast_io::details::scan_decfloat_context_phase::special;
				return ::fast_io::details::scan_decfloat_context_special_define<char_type, flags, T, use_precision>(
					state, first, end, value, precision);
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
scan_decfloat_context_eof(::fast_io::details::scan_decfloat_context<char_type> &state, T &value,
						  ::std::size_t precision = 0) noexcept
{
	if (state.phase == ::fast_io::details::scan_decfloat_context_phase::special)
	{
		if (!state.special_buffer.size)
		{
			return ::fast_io::parse_code::end_of_file;
		}
		T parsed_value{};
		auto const *buffer_begin{state.special_buffer.buffer.data()};
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

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr auto
scan_context_type(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return io_type_t<::fast_io::details::scan_decfloat_context<char_type>>{};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_context_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
					::fast_io::details::scan_decfloat_context<char_type> &state, char_type const *begin,
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
						::fast_io::details::scan_decfloat_context<char_type> &state,
						::fast_io::manipulators::scalar_manip_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_decfloat_context_eof<char_type, flags, T, false>(state, value.reference);
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr auto
scan_context_type(io_reserve_type_t<char_type,
									::fast_io::manipulators::scalar_manip_precision_t<flags, T &>>) noexcept
{
	return io_type_t<::fast_io::details::scan_decfloat_context<char_type>>{};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::scan_decfloat_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_context_define(io_reserve_type_t<char_type,
									  ::fast_io::manipulators::scalar_manip_precision_t<flags, T &>>,
					::fast_io::details::scan_decfloat_context<char_type> &state, char_type const *begin,
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
						::fast_io::details::scan_decfloat_context<char_type> &state,
						::fast_io::manipulators::scalar_manip_precision_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_decfloat_context_eof<char_type, flags, T, true>(
		state, value.reference, value.precision);
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

#pragma once
#include "punning.h"
/*
Algorithm: fast_io
Author: cqwrteur
Reference: no reference
Credit: Jk jeon
*/
namespace fast_io
{

namespace details
{

template <::std::uint_least32_t e2hexdigits, ::std::random_access_iterator Iter>
inline constexpr Iter with_sign_prt_rsv_exponent_hex_impl(Iter iter, ::std::int_least32_t e2)
{
	using char_type = ::std::iter_value_t<Iter>;
	::std::uint_least32_t ue2{static_cast<::std::uint_least32_t>(e2)};
	if (e2 < 0)
	{
		ue2 = 0u - ue2;
		*iter = char_literal_v<u8'-', char_type>;
	}
	else
	{
		*iter = char_literal_v<u8'+', char_type>;
	}
	++iter;
	return prt_rsv_exponent_impl<e2hexdigits, false>(iter, ue2);
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr bool scan_hexfloat_digit(char_type ch, ::std::uint_least8_t &digit) noexcept
{
	constexpr auto zero{::fast_io::char_literal_v<u8'0', char_type>};
	constexpr auto nine{::fast_io::char_literal_v<u8'9', char_type>};
	constexpr auto lower_a{::fast_io::char_literal_v<u8'a', char_type>};
	constexpr auto lower_f{::fast_io::char_literal_v<u8'f', char_type>};
	constexpr auto upper_a{::fast_io::char_literal_v<u8'A', char_type>};
	constexpr auto upper_f{::fast_io::char_literal_v<u8'F', char_type>};
	if (zero <= ch && ch <= nine)
	{
		digit = static_cast<::std::uint_least8_t>(ch - zero);
		return true;
	}
	if (lower_a <= ch && ch <= lower_f)
	{
		digit = static_cast<::std::uint_least8_t>(ch - lower_a + 10u);
		return true;
	}
	if (upper_a <= ch && ch <= upper_f)
	{
		digit = static_cast<::std::uint_least8_t>(ch - upper_a + 10u);
		return true;
	}
	return false;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_hexfloat_exponent(char_type const *first, char_type const *last, ::std::int_least64_t &exponent) noexcept
{
	if (first == last)
	{
		return {first, ::fast_io::parse_code::invalid};
	}
	constexpr auto plus{::fast_io::char_literal_v<u8'+', char_type>};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', char_type>};
	constexpr auto zero{::fast_io::char_literal_v<u8'0', char_type>};
	constexpr auto nine{::fast_io::char_literal_v<u8'9', char_type>};
	bool negative{};
	if (*first == minus)
	{
		negative = true;
		++first;
	}
	else if (*first == plus)
	{
		++first;
	}
	if (first == last || *first < zero || nine < *first)
	{
		return {first, ::fast_io::parse_code::invalid};
	}
	constexpr auto exponent_limit{static_cast<::std::uint_least64_t>((::std::numeric_limits<::std::int_least64_t>::max)())};
	::std::uint_least64_t value{};
	bool overflow{};
	while (first != last && zero <= *first && *first <= nine)
	{
		auto const digit{static_cast<::std::uint_least64_t>(*first - zero)};
		if (value > (exponent_limit - digit) / 10u)
		{
			value = exponent_limit;
			overflow = true;
		}
		else if (!overflow)
		{
			value = value * 10u + digit;
		}
		++first;
	}
	exponent = negative ? -static_cast<::std::int_least64_t>(value) : static_cast<::std::int_least64_t>(value);
	return {first, ::fast_io::parse_code::ok};
}

[[nodiscard]] inline constexpr ::std::uint_least64_t scan_hexfloat_round_shift(::std::uint_least64_t stored,
																			   ::std::int_least64_t remaining_bits,
																			   bool truncated_nonzero,
																			   ::std::int_least64_t shift) noexcept
{
	auto const stored_bits{static_cast<::std::int_least64_t>(::std::bit_width(stored))};
	if (shift <= 0)
	{
		auto const left_shift{remaining_bits - shift};
		return left_shift >= 64 ? 0u : stored << static_cast<unsigned>(left_shift);
	}

	auto const shift_in_stored{shift - remaining_bits};
	if (shift_in_stored <= 0)
	{
		return 0u;
	}
	if (shift_in_stored > stored_bits)
	{
		return 0u;
	}

	::std::uint_least64_t quotient{};
	bool half{};
	bool below_half{truncated_nonzero};
	if (shift_in_stored == stored_bits)
	{
		half = true;
		auto const below_mask{(::std::uint_least64_t{1} << static_cast<unsigned>(stored_bits - 1)) - 1u};
		below_half = below_half || (stored & below_mask) != 0u;
	}
	else
	{
		quotient = stored >> static_cast<unsigned>(shift_in_stored);
		half = ((stored >> static_cast<unsigned>(shift_in_stored - 1)) & 1u) != 0u;
		if (shift_in_stored > 1)
		{
			auto const below_mask{(::std::uint_least64_t{1} << static_cast<unsigned>(shift_in_stored - 1)) - 1u};
			below_half = below_half || (stored & below_mask) != 0u;
		}
	}
	if (half && (below_half || (quotient & 1u) != 0u))
	{
		++quotient;
	}
	return quotient;
}

template <typename T>
inline constexpr ::fast_io::parse_code scan_hexfloat_assign_ieee_result(T &value, bool negative,
																		::std::uint_least64_t stored,
																		::std::size_t stored_hex_digits,
																		::std::int_least64_t significant_hex_digits,
																		bool truncated_nonzero,
																		::std::int_least64_t binary_exponent) noexcept
{
	using trait = ::fast_io::details::iec559_traits<T>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr ::std::size_t mbits{trait::mbits};
	constexpr ::std::size_t ebits{trait::ebits};
	constexpr ::std::size_t precision_bits{mbits + 1u};
	static_assert(precision_bits <= 60u);
	constexpr auto bias{static_cast<::std::int_least64_t>((static_cast<::std::uint_least32_t>(1u) << ebits) >> 1u) - 1};
	constexpr auto min_exponent{1 - bias};
	constexpr auto max_exponent{bias};
	constexpr auto int64_max{(::std::numeric_limits<::std::int_least64_t>::max)()};
	constexpr auto int64_min{(::std::numeric_limits<::std::int_least64_t>::min)()};

	auto const stored_bits{static_cast<::std::int_least64_t>(::std::bit_width(stored))};
	auto const remaining_bits{(significant_hex_digits - static_cast<::std::int_least64_t>(stored_hex_digits)) * 4};
	auto const total_bits{stored_bits + remaining_bits};
	auto const exponent_offset{total_bits - 1};
	auto const exponent2{binary_exponent > int64_max - exponent_offset
							 ? int64_max
							 : (binary_exponent < int64_min + exponent_offset ? int64_min : binary_exponent + exponent_offset)};

	if (exponent2 > max_exponent)
	{
		return ::fast_io::parse_code::overflow;
	}

	mantissa_type result_bits{};
	if (negative)
	{
		result_bits = static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << (mbits + ebits));
	}

	if (exponent2 >= min_exponent)
	{
		auto significand{::fast_io::details::scan_hexfloat_round_shift(
			stored, remaining_bits, truncated_nonzero, total_bits - static_cast<::std::int_least64_t>(precision_bits))};
		if (significand == (::std::uint_least64_t{1} << precision_bits))
		{
			significand >>= 1u;
			if (exponent2 == max_exponent)
			{
				return ::fast_io::parse_code::overflow;
			}
			result_bits |= static_cast<mantissa_type>((exponent2 + 1 + bias) << mbits);
		}
		else
		{
			result_bits |= static_cast<mantissa_type>((exponent2 + bias) << mbits);
		}
		result_bits |= static_cast<mantissa_type>(significand - (::std::uint_least64_t{1} << mbits));
		value = ::fast_io::bit_cast<T>(result_bits);
		return ::fast_io::parse_code::ok;
	}

	auto const subnormal_shift{(min_exponent - static_cast<::std::int_least64_t>(mbits)) - binary_exponent};
	auto const fraction{::fast_io::details::scan_hexfloat_round_shift(stored, remaining_bits, truncated_nonzero, subnormal_shift)};
	if (fraction == 0u)
	{
		return ::fast_io::parse_code::overflow;
	}
	if (fraction >= (::std::uint_least64_t{1} << mbits))
	{
		result_bits |= static_cast<mantissa_type>(mantissa_type{1} << mbits);
	}
	else
	{
		result_bits |= static_cast<mantissa_type>(fraction);
	}
	value = ::fast_io::bit_cast<T>(result_bits);
	return ::fast_io::parse_code::ok;
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point T>
inline constexpr ::fast_io::parse_result<char_type const *>
scan_hexfloat_contiguous_define_impl(char_type const *begin, char_type const *end, T &value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<T>;
	constexpr ::std::size_t precision_bits{trait::mbits + 1u};
	constexpr ::std::size_t stored_hex_digits_limit{(precision_bits + 7u) / 4u};
	auto first{begin};
	constexpr auto plus{::fast_io::char_literal_v<u8'+', char_type>};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', char_type>};
	bool negative{};
	if (first != end && (*first == minus || *first == plus))
	{
		negative = *first == minus;
		++first;
	}

	if constexpr (flags.showbase)
	{
		constexpr auto zero{::fast_io::char_literal_v<u8'0', char_type>};
		constexpr auto lower_x{::fast_io::char_literal_v<u8'x', char_type>};
		constexpr auto upper_x{::fast_io::char_literal_v<u8'X', char_type>};
		if (end - first < 2 || first[0] != zero || (first[1] != lower_x && first[1] != upper_x))
		{
			return {begin, ::fast_io::parse_code::invalid};
		}
		first += 2;
	}

	bool has_digit{};
	bool has_nonzero_digit{};
	::std::uint_least64_t stored{};
	::std::size_t stored_hex_digits{};
	::std::int_least64_t significant_hex_digits{};
	::std::int_least64_t fractional_hex_digits{};
	bool truncated_nonzero{};
	auto const append_digit{[&]([[maybe_unused]] bool after_decimal, ::std::uint_least8_t digit) constexpr noexcept
	{
		has_digit = true;
		if (after_decimal)
		{
			++fractional_hex_digits;
		}
		if (!has_nonzero_digit)
		{
			if (digit == 0u)
			{
				return;
			}
			has_nonzero_digit = true;
		}
		++significant_hex_digits;
		if (stored_hex_digits != stored_hex_digits_limit)
		{
			stored = (stored << 4u) | digit;
			++stored_hex_digits;
		}
		else if (digit != 0u)
		{
			truncated_nonzero = true;
		}
	}};

	while (first != end)
	{
		::std::uint_least8_t digit{};
		if (!::fast_io::details::scan_hexfloat_digit(*first, digit))
		{
			break;
		}
		append_digit(false, digit);
		++first;
	}

	constexpr auto dot{::fast_io::char_literal_v<u8'.', char_type>};
	if (first != end && *first == dot)
	{
		++first;
		while (first != end)
		{
			::std::uint_least8_t digit{};
			if (!::fast_io::details::scan_hexfloat_digit(*first, digit))
			{
				break;
			}
			append_digit(true, digit);
			++first;
		}
	}

	constexpr auto lower_p{::fast_io::char_literal_v<u8'p', char_type>};
	constexpr auto upper_p{::fast_io::char_literal_v<u8'P', char_type>};
	if (!has_digit || first == end || (*first != lower_p && *first != upper_p))
	{
		return {first, ::fast_io::parse_code::invalid};
	}
	++first;
	::std::int_least64_t exponent{};
	auto const exponent_result{::fast_io::details::scan_hexfloat_exponent(first, end, exponent)};
	if (exponent_result.code != ::fast_io::parse_code::ok)
	{
		return exponent_result;
	}

	if (!has_nonzero_digit)
	{
		value = negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
		return {exponent_result.iter, ::fast_io::parse_code::ok};
	}

	constexpr auto int64_max{(::std::numeric_limits<::std::int_least64_t>::max)()};
	constexpr auto int64_min{(::std::numeric_limits<::std::int_least64_t>::min)()};
	auto const fractional_exponent{fractional_hex_digits > int64_max / 4 ? int64_max : fractional_hex_digits * 4};
	auto const binary_exponent{exponent < int64_min + fractional_exponent ? int64_min : exponent - fractional_exponent};
	return {exponent_result.iter,
			::fast_io::details::scan_hexfloat_assign_ieee_result(value, negative, stored, stored_hex_digits,
																 significant_hex_digits, truncated_nonzero, binary_exponent)};
}

template <bool showbase, bool showbase_uppercase, bool showpos, bool uppercase, bool uppercase_e, bool comma,
		  typename flt, ::std::integral char_type>
inline constexpr char_type *print_rsvhexfloat_define_impl(char_type *iter, flt f) noexcept
{
	using trait = iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr ::std::size_t mbits{trait::mbits};
	constexpr ::std::size_t ebits{trait::ebits};
	constexpr ::std::uint_least32_t bias{(static_cast<::std::uint_least32_t>(1 << ebits) >> 1) - 1};
	constexpr mantissa_type exponent_mask{(static_cast<mantissa_type>(1) << ebits) - 1};
	auto [mantissa, exponent, sign] = get_punned_result(f);
	constexpr ::std::uint_least32_t exponent_mask_u32{static_cast<::std::uint_least32_t>(exponent_mask)};
	constexpr ::std::int_least32_t minus_bias{-static_cast<::std::int_least32_t>(bias)};
	constexpr ::std::uint_least32_t makeup_bits{((mbits / 4 + 1) * 4 - mbits) % 4}; // Thanks jk-jeon for the formula
	iter = print_rsv_fp_sign_impl<showpos>(iter, sign);
	if (exponent == exponent_mask_u32)
	{
		return prsv_fp_nan_impl<uppercase>(iter, mantissa);
	}
	if constexpr (showbase)
	{
		iter = print_reserve_show_base_impl<16, showbase_uppercase, false>(iter);
	}
	if (!mantissa && !exponent)
	{
		return prsv_fp_hex0p0<uppercase>(iter);
	}
	::std::int_least32_t e2{static_cast<::std::int_least32_t>(exponent) + minus_bias};
	if (mantissa)
	{
		::std::uint_least32_t trailing_zeros_digits{
			static_cast<::std::uint_least32_t>(my_countr_zero_unchecked(mantissa)) + makeup_bits};
		constexpr ::std::uint_least32_t trailing_zeros_mdigits_d4{
			static_cast<::std::uint_least32_t>((mbits + makeup_bits) >> 2)};
		::std::uint_least32_t trailing_zeros_digits_d4{trailing_zeros_digits >> 2};
		::std::uint_least32_t trailing_zeros_digits_d4m4{trailing_zeros_digits_d4 << 2};

		mantissa <<= makeup_bits;
		mantissa >>= trailing_zeros_digits_d4m4;
		::std::uint_least32_t mantissa_len{trailing_zeros_mdigits_d4 - trailing_zeros_digits_d4};
		if (exponent == 0)
		{
			++e2;
			iter = prsv_fp_hex0d<comma>(iter);
		}
		else
		{
			iter = prsv_fp_hex1d<comma>(iter);
		}
		print_reserve_integral_main_impl<16, uppercase>(iter += mantissa_len, mantissa, mantissa_len);
	}
	else
	{
		*iter = char_literal_v<u8'1', char_type>;
		++iter;
	}
	*iter = char_literal_v < uppercase_e ? u8'P' : u8'p', char_type > ;
	++iter;
	return with_sign_prt_rsv_exponent_hex_impl<trait::e2hexdigits>(iter, e2);
}

template <bool showbase, typename mantissa_type>
inline constexpr ::std::size_t print_rsvhexfloat_size_cache{
	print_integer_reserved_size_cache<16, showbase, true, false, mantissa_type> + 6 +
	print_integer_reserved_size_cache<10, true, true, false, ::std::int_least32_t>};

} // namespace details

namespace manipulators
{

template <bool noskipws = false, bool prefix = false, ::fast_io::details::my_floating_point scalar_type>
inline constexpr scalar_manip_t<::fast_io::manipulators::scalar_flags{.showbase = prefix,
																	  .noskipws = noskipws,
																	  .floating = ::fast_io::manipulators::floating_format::hexfloat},
								scalar_type &>
hexfloat_get(scalar_type &t) noexcept
{
	return {t};
}

} // namespace manipulators

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point T>
	requires(flags.floating == ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_contiguous_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
					   char_type const *begin, char_type const *end,
					   ::fast_io::manipulators::scalar_manip_t<flags, T &> value) noexcept
{
	if constexpr (!flags.noskipws)
	{
		begin = ::fast_io::details::find_space_common_impl<false, true>(begin, end);
		if (begin == end)
		{
			return {begin, ::fast_io::parse_code::end_of_file};
		}
	}
	return ::fast_io::details::scan_hexfloat_contiguous_define_impl<char_type, flags>(begin, end, value.reference);
}

} // namespace fast_io

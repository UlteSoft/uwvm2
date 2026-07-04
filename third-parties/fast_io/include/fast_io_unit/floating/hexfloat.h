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
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr bool scan_hexfloat_digit(char_type ch, char8_t &digit) noexcept
{
	using unsigned_char_type = ::fast_io::details::my_make_unsigned_t<char_type>;
	auto uch{static_cast<unsigned_char_type>(ch)};
	if (::fast_io::details::char_digit_to_literal<16, char_type>(uch))
	{
		return false;
	}
	digit = static_cast<char8_t>(uch);
	return true;
}

template <::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr bool scan_hexfloat_decimal_digit(char_type ch, char8_t &digit) noexcept
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

template <char8_t lower, char8_t upper, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr bool scan_hexfloat_caseless_equal(char_type ch) noexcept
{
	return ch == ::fast_io::char_literal_v<lower, char_type> || ch == ::fast_io::char_literal_v<upper, char_type>;
}

template <::std::integral char_type>
struct scan_hexfloat_special_result
{
	char_type const *iter{};
	::fast_io::parse_code code{};
	bool matched{};
};

template <::std::integral char_type>
[[nodiscard]] inline constexpr bool scan_hexfloat_nan_sequence_ind(char_type const *first,
																   char_type const *last) noexcept
{
	return last - first == 3 &&
		   ::fast_io::details::scan_hexfloat_caseless_equal<u8'i', u8'I'>(first[0]) &&
		   ::fast_io::details::scan_hexfloat_caseless_equal<u8'n', u8'N'>(first[1]) &&
		   ::fast_io::details::scan_hexfloat_caseless_equal<u8'd', u8'D'>(first[2]);
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr bool scan_hexfloat_nan_sequence_snan(char_type const *first,
																	char_type const *last) noexcept
{
	return last - first == 4 &&
		   ::fast_io::details::scan_hexfloat_caseless_equal<u8's', u8'S'>(first[0]) &&
		   ::fast_io::details::scan_hexfloat_caseless_equal<u8'n', u8'N'>(first[1]) &&
		   ::fast_io::details::scan_hexfloat_caseless_equal<u8'a', u8'A'>(first[2]) &&
		   ::fast_io::details::scan_hexfloat_caseless_equal<u8'n', u8'N'>(first[3]);
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr bool scan_hexfloat_nan_sequence_char(char_type ch) noexcept
{
	using family = ::fast_io::char_category::char_category_family;
	return ch == ::fast_io::char_literal_v<u8'_', char_type> ||
		   ::fast_io::char_category::char_category_traits<family::c_alnum, false>::char_is(ch);
}

template <bool nan_parse_sign, ::fast_io::manipulators::floating_nan_payload_scan nan_payload_scan,
		  ::std::integral char_type, ::fast_io::details::my_floating_point T>
[[nodiscard]] inline constexpr scan_hexfloat_special_result<char_type>
scan_hexfloat_special_value(char_type const *first, char_type const *end, bool negative, T &value) noexcept
{
	if (first == end)
	{
		return {};
	}
	if (::fast_io::details::scan_hexfloat_caseless_equal<u8'i', u8'I'>(*first))
	{
		if (end - first < 3 ||
			!::fast_io::details::scan_hexfloat_caseless_equal<u8'n', u8'N'>(first[1]) ||
			!::fast_io::details::scan_hexfloat_caseless_equal<u8'f', u8'F'>(first[2]))
		{
			return {first, ::fast_io::parse_code::invalid, true};
		}
		auto iter{first + 3};
		if (end - iter >= 5 &&
			::fast_io::details::scan_hexfloat_caseless_equal<u8'i', u8'I'>(iter[0]) &&
			::fast_io::details::scan_hexfloat_caseless_equal<u8'n', u8'N'>(iter[1]) &&
			::fast_io::details::scan_hexfloat_caseless_equal<u8'i', u8'I'>(iter[2]) &&
			::fast_io::details::scan_hexfloat_caseless_equal<u8't', u8'T'>(iter[3]) &&
			::fast_io::details::scan_hexfloat_caseless_equal<u8'y', u8'Y'>(iter[4]))
		{
			iter += 5;
		}
		::fast_io::details::fp_assign_infinity(value, negative);
		return {iter, ::fast_io::parse_code::ok, true};
	}
	if (::fast_io::details::scan_hexfloat_caseless_equal<u8'n', u8'N'>(*first))
	{
		if (end - first < 3 ||
			!::fast_io::details::scan_hexfloat_caseless_equal<u8'a', u8'A'>(first[1]) ||
			!::fast_io::details::scan_hexfloat_caseless_equal<u8'n', u8'N'>(first[2]))
		{
			return {first, ::fast_io::parse_code::invalid, true};
		}
		auto iter{first + 3};
		bool indeterminate{};
		bool signaling{};
		if constexpr (nan_payload_scan != ::fast_io::manipulators::floating_nan_payload_scan::none)
		{
			if (iter != end && *iter == ::fast_io::char_literal_v<u8'(', char_type>)
			{
				auto const *seq_begin{iter + 1};
				for (auto scan{seq_begin}; scan != end; ++scan)
				{
					if (*scan == ::fast_io::char_literal_v<u8')', char_type>)
					{
						if constexpr (nan_payload_scan ==
									  ::fast_io::manipulators::floating_nan_payload_scan::preserve)
						{
							indeterminate =
								::fast_io::details::scan_hexfloat_nan_sequence_ind(seq_begin, scan);
							signaling = ::fast_io::details::scan_hexfloat_nan_sequence_snan(seq_begin, scan);
						}
						iter = scan + 1;
						break;
					}
					if (!::fast_io::details::scan_hexfloat_nan_sequence_char(*scan))
					{
						break;
					}
				}
			}
		}
		if (indeterminate)
		{
			::fast_io::details::fp_assign_nan<T, false, true>(value, true);
		}
		else if (signaling)
		{
			::fast_io::details::fp_assign_nan<T, true, false>(value, nan_parse_sign && negative);
		}
		else
		{
			::fast_io::details::fp_assign_nan<T, false, false>(value, nan_parse_sign && negative);
		}
		return {iter, ::fast_io::parse_code::ok, true};
	}
	return {};
}

struct scan_hexfloat_significand_state
{
	bool has_digit{};
	bool has_nonzero_digit{};
	::std::uint_least64_t stored{};
	::std::size_t stored_hex_digits{};
	::std::int_least64_t significant_hex_digits{};
	::std::int_least64_t fractional_hex_digits{};
	bool truncated_nonzero{};
};

template <::std::size_t stored_hex_digits_limit>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scan_hexfloat_append_digit(scan_hexfloat_significand_state &state, bool after_decimal,
												 char8_t digit) noexcept
{
	state.has_digit = true;
	if (after_decimal)
	{
		++state.fractional_hex_digits;
	}
	if (!state.has_nonzero_digit)
	{
		if (digit == 0u)
		{
			return;
		}
		state.has_nonzero_digit = true;
	}
	++state.significant_hex_digits;
	if (state.stored_hex_digits != stored_hex_digits_limit)
	{
		state.stored = (state.stored << 4u) | digit;
		++state.stored_hex_digits;
	}
	else if (digit != 0u)
	{
		state.truncated_nonzero = true;
	}
}

#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scan_hexfloat_ascii8_pack(::std::uint_least64_t val, ::std::uint_least64_t &nibbles,
												::std::uint_least64_t &packed) noexcept
{
	val -= 0x3030303030303030;
	nibbles = (val & 0x0f0f0f0f0f0f0f0f) + ((val & 0x1010101010101010) >> 4) * 9u;
	constexpr ::std::uint_least64_t mask{0x000000FF000000FF};
	constexpr ::std::uint_least64_t pow_base_2{
		::fast_io::details::compile_pow_n<::std::uint_least64_t, 16, 2>};
	constexpr ::std::uint_least64_t pow_base_4{
		::fast_io::details::compile_pow_n<::std::uint_least64_t, 16, 4>};
	constexpr ::std::uint_least64_t pow_base_6{
		::fast_io::details::compile_pow_n<::std::uint_least64_t, 16, 6>};
	constexpr ::std::uint_least64_t mul1{pow_base_2 + (pow_base_6 << 32u)};
	constexpr ::std::uint_least64_t mul2{1u + (pow_base_4 << 32u)};
	auto folded{(nibbles * 16u) + (nibbles >> 8u)};
	packed = (((folded & mask) * mul1) + (((folded >> 16u) & mask) * mul2)) >> 32u;
}

#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr bool scan_hexfloat_ascii8_nibbles(::std::uint_least64_t val,
																 ::std::uint_least64_t &nibbles,
																 ::std::uint_least64_t &packed) noexcept
{
	constexpr ::std::uint_least64_t first_bound1{0x3939393939393939};
	constexpr ::std::uint_least64_t first_bound2{0x1919191919191919};
	auto const invalid{((((val + 0x4646464646464646) | (val - 0x3030303030303030)) &
						 ((val + first_bound1) | (val - 0x4040404040404040)) &
						 ((val + first_bound2) | (val - 0x6060606060606060))) |
						~(((val + 0x3f3f3f3f3f3f3f3f) | (val - 0x4040404040404040)) &
						  ((val + 0x1f1f1f1f1f1f1f1f) | (val - 0x6060606060606060)))) &
					   0x8080808080808080};
	if (invalid)
	{
		return false;
	}
	::fast_io::details::scan_hexfloat_ascii8_pack(val, nibbles, packed);
	return true;
}

template <::std::size_t stored_hex_digits_limit>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scan_hexfloat_append_ascii8(scan_hexfloat_significand_state &state, bool after_decimal,
												  ::std::uint_least64_t nibbles,
												  ::std::uint_least64_t packed) noexcept
{
	state.has_digit = true;
	if (after_decimal)
	{
		state.fractional_hex_digits += 8u;
	}
	::std::uint_least64_t significant_digits{8u};
	if (!state.has_nonzero_digit)
	{
		auto const nonzero_high{(nibbles + 0x7f7f7f7f7f7f7f7f) & 0x8080808080808080};
		if (nonzero_high == 0)
		{
			return;
		}
		significant_digits -= ::std::countr_zero(nonzero_high) >> 3u;
		state.has_nonzero_digit = true;
	}
	state.significant_hex_digits += static_cast<::std::int_least64_t>(significant_digits);
	if (state.stored_hex_digits == stored_hex_digits_limit)
	{
		if (packed != 0)
		{
			state.truncated_nonzero = true;
		}
		return;
	}
	auto available{stored_hex_digits_limit - state.stored_hex_digits};
	if (significant_digits <= available)
	{
		state.stored = (state.stored << static_cast<unsigned>(significant_digits * 4u)) | packed;
		state.stored_hex_digits += static_cast<::std::size_t>(significant_digits);
		return;
	}
	auto const truncated_digits{significant_digits - available};
	auto const truncated_bits{static_cast<unsigned>(truncated_digits * 4u)};
	state.stored = (state.stored << static_cast<unsigned>(available * 4u)) | (packed >> truncated_bits);
	state.stored_hex_digits = stored_hex_digits_limit;
	if ((packed & ((::std::uint_least64_t{1} << truncated_bits) - 1u)) != 0)
	{
		state.truncated_nonzero = true;
	}
}

template <::std::size_t stored_hex_digits_limit, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *scan_hexfloat_significand_run(char_type const *first, char_type const *last,
																bool after_decimal,
																scan_hexfloat_significand_state &state) noexcept
{
#ifdef __cpp_if_consteval
	if !consteval
#else
	if (!__builtin_is_constant_evaluated())
#endif
	{
		if constexpr (!::fast_io::details::is_ebcdic<char_type> && sizeof(char_type) == sizeof(char8_t) &&
					  ::std::numeric_limits<::std::uint_least64_t>::digits == 64u)
		{
			for (; static_cast<::std::size_t>(last - first) >= sizeof(::std::uint_least64_t);)
			{
				::std::uint_least64_t val;
				::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));
				if constexpr (::std::endian::little != ::std::endian::native)
				{
					val = ::fast_io::little_endian(val);
				}
				::std::uint_least64_t nibbles{};
				::std::uint_least64_t packed{};
				if (!::fast_io::details::scan_hexfloat_ascii8_nibbles(val, nibbles, packed))
				{
					break;
				}
				::fast_io::details::scan_hexfloat_append_ascii8<stored_hex_digits_limit>(state, after_decimal, nibbles,
																						 packed);
				first += sizeof(::std::uint_least64_t);
			}
		}
	}
	using unsigned_char_type = ::fast_io::details::my_make_unsigned_t<char_type>;
	for (; first != last; ++first)
	{
		auto uch{static_cast<unsigned_char_type>(*first)};
		if (::fast_io::details::char_digit_to_literal<16, char_type>(uch))
		{
			break;
		}
		::fast_io::details::scan_hexfloat_append_digit<stored_hex_digits_limit>(state, after_decimal,
																				static_cast<char8_t>(uch));
	}
	return first;
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
	char8_t first_digit{};
	if (first == last || !::fast_io::details::scan_hexfloat_decimal_digit(*first, first_digit))
	{
		return {first, ::fast_io::parse_code::invalid};
	}
	constexpr auto exponent_limit{static_cast<::std::uint_least64_t>((::std::numeric_limits<::std::int_least64_t>::max)())};
	::std::uint_least64_t value{};
	bool overflow{};
	do
	{
		auto const digit{static_cast<::std::uint_least64_t>(first_digit)};
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
	} while (first != last && ::fast_io::details::scan_hexfloat_decimal_digit(*first, first_digit));
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
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::parse_result<char_type const *>
scan_hexfloat_contiguous_scalar_define_impl(char_type const *begin, char_type const *end, T &value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<T>;
	constexpr ::std::size_t precision_bits{trait::mbits + 1u};
	constexpr ::std::size_t stored_hex_digits_limit{(precision_bits + 7u) / 4u};
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

	auto const special_result{
		::fast_io::details::scan_hexfloat_special_value<flags.nan_parse_sign, flags.nan_payload_scan>(
			first, end, negative, value)};
	if (special_result.matched)
	{
		return {special_result.iter, special_result.code};
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

	::fast_io::details::scan_hexfloat_significand_state significand_state;
	first = ::fast_io::details::scan_hexfloat_significand_run<stored_hex_digits_limit>(first, end, false,
																					   significand_state);

	constexpr auto dot{::fast_io::char_literal_v<u8'.', char_type>};
	if (first != end && *first == dot)
	{
		++first;
		first = ::fast_io::details::scan_hexfloat_significand_run<stored_hex_digits_limit>(first, end, true,
																						   significand_state);
	}

	constexpr auto lower_p{::fast_io::char_literal_v<u8'p', char_type>};
	constexpr auto upper_p{::fast_io::char_literal_v<u8'P', char_type>};
	if (!significand_state.has_digit || first == end || (*first != lower_p && *first != upper_p))
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

	if (!significand_state.has_nonzero_digit)
	{
		value = negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
		return {exponent_result.iter, ::fast_io::parse_code::ok};
	}

	constexpr auto int64_max{(::std::numeric_limits<::std::int_least64_t>::max)()};
	constexpr auto int64_min{(::std::numeric_limits<::std::int_least64_t>::min)()};
	auto const fractional_exponent{significand_state.fractional_hex_digits > int64_max / 4
									   ? int64_max
									   : significand_state.fractional_hex_digits * 4};
	auto const binary_exponent{exponent < int64_min + fractional_exponent ? int64_min : exponent - fractional_exponent};
	return {exponent_result.iter,
			::fast_io::details::scan_hexfloat_assign_ieee_result(
				value, negative, significand_state.stored, significand_state.stored_hex_digits,
				significand_state.significant_hex_digits, significand_state.truncated_nonzero, binary_exponent)};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::parse_result<char_type const *>
scan_hexfloat_contiguous_define_impl(char_type const *begin, char_type const *end, T &value) noexcept
{
	if constexpr (sizeof(char_type) != sizeof(char8_t))
	{
		return ::fast_io::details::scan_hexfloat_contiguous_scalar_define_impl<char_type, flags>(begin, end, value);
	}
	using trait = ::fast_io::details::iec559_traits<T>;
	constexpr ::std::size_t precision_bits{trait::mbits + 1u};
	constexpr ::std::size_t stored_hex_digits_limit{(precision_bits + 7u) / 4u};
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

	auto const special_result{
		::fast_io::details::scan_hexfloat_special_value<flags.nan_parse_sign, flags.nan_payload_scan>(
			first, end, negative, value)};
	if (special_result.matched)
	{
		return {special_result.iter, special_result.code};
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

	::fast_io::details::scan_hexfloat_significand_state significand_state;
	first = ::fast_io::details::scan_hexfloat_significand_run<stored_hex_digits_limit>(first, end, false,
																					   significand_state);

	constexpr auto dot{::fast_io::char_literal_v<u8'.', char_type>};
	if (first != end && *first == dot)
	{
		++first;
		first = ::fast_io::details::scan_hexfloat_significand_run<stored_hex_digits_limit>(first, end, true,
																						   significand_state);
	}

	constexpr auto lower_p{::fast_io::char_literal_v<u8'p', char_type>};
	constexpr auto upper_p{::fast_io::char_literal_v<u8'P', char_type>};
	if (!significand_state.has_digit || first == end || (*first != lower_p && *first != upper_p))
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

	if (!significand_state.has_nonzero_digit)
	{
		value = negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
		return {exponent_result.iter, ::fast_io::parse_code::ok};
	}

	constexpr auto int64_max{(::std::numeric_limits<::std::int_least64_t>::max)()};
	constexpr auto int64_min{(::std::numeric_limits<::std::int_least64_t>::min)()};
	auto const fractional_exponent{significand_state.fractional_hex_digits > int64_max / 4
									   ? int64_max
									   : significand_state.fractional_hex_digits * 4};
	auto const binary_exponent{exponent < int64_min + fractional_exponent ? int64_min
																		  : exponent - fractional_exponent};
	return {exponent_result.iter,
			::fast_io::details::scan_hexfloat_assign_ieee_result(
				value, negative, significand_state.stored, significand_state.stored_hex_digits,
				significand_state.significant_hex_digits, significand_state.truncated_nonzero, binary_exponent)};
}

template <bool showbase, bool showbase_uppercase, bool showpos, bool uppercase, bool uppercase_e, bool comma,
		  bool nan_show_sign = true, bool nan_show_type = false, typename flt, ::std::integral char_type>
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
	constexpr ::std::uint_least32_t makeup_bits{((mbits / 4u + 1u) * 4u - mbits) % 4u}; // Thanks jk-jeon for the formula
	if (exponent == exponent_mask_u32)
	{
		return prsv_fp_nan_impl<showpos, uppercase, nan_show_sign, nan_show_type, mbits>(iter, mantissa, sign);
	}
	iter = print_rsv_fp_sign_impl<showpos>(iter, sign);
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

template <bool uppercase, ::std::integral char_type>
inline constexpr char_type *print_rsvhexfloat_digit_impl(char_type *iter, ::std::uint_least32_t digit) noexcept
{
	if (digit < 10u)
	{
		*iter = ::fast_io::char_literal_add<char_type>(digit);
	}
	else
	{
		*iter = static_cast<char_type>(
			char_literal_v<(uppercase ? u8'A' : u8'a'), char_type> + static_cast<char_type>(digit - 10u));
	}
	++iter;
	return iter;
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool print_rsvhexfloat_round_up(bool negative, ::std::uint_least32_t rounded_down,
															   ::std::uint_least32_t next_digit,
															   bool tail_nonzero) noexcept
{
	if (!next_digit && !tail_nonzero)
	{
		return false;
	}
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		if (next_digit < 8u)
		{
			return false;
		}
		if (8u < next_digit || tail_nonzero)
		{
			return true;
		}
		return ::fast_io::details::floating_rounding_nearest_tie_round_up<rounding>(
			negative, static_cast<::std::uint_least64_t>(rounded_down) << 1u);
	}
	else
	{
		return ::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
	}
}

template <bool showbase, bool showbase_uppercase, bool showpos, bool uppercase, bool uppercase_e, bool comma,
		  ::fast_io::manipulators::floating_rounding rounding =
			  ::fast_io::manipulators::floating_rounding::nearest_to_even,
		  bool nan_show_sign = true, bool nan_show_type = false, typename flt, ::std::integral char_type>
inline constexpr char_type *print_rsvhexfloat_precision_define_impl(char_type *iter, flt f,
																	::std::size_t precision) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return print_rsvhexfloat_precision_define_impl<
				showbase, showbase_uppercase, showpos, uppercase, uppercase_e, comma,
				::fast_io::manipulators::floating_rounding::toward_plus_infinity, nan_show_sign, nan_show_type>(
				iter, f, precision);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return print_rsvhexfloat_precision_define_impl<
				showbase, showbase_uppercase, showpos, uppercase, uppercase_e, comma,
				::fast_io::manipulators::floating_rounding::toward_minus_infinity, nan_show_sign, nan_show_type>(
				iter, f, precision);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return print_rsvhexfloat_precision_define_impl<
				showbase, showbase_uppercase, showpos, uppercase, uppercase_e, comma,
				::fast_io::manipulators::floating_rounding::toward_zero, nan_show_sign, nan_show_type>(
				iter, f, precision);
		default:
			return print_rsvhexfloat_precision_define_impl<
				showbase, showbase_uppercase, showpos, uppercase, uppercase_e, comma,
				::fast_io::manipulators::floating_rounding::nearest_to_even, nan_show_sign, nan_show_type>(
				iter, f, precision);
		}
	}
	else
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
		constexpr ::std::uint_least32_t makeup_bits{((mbits / 4u + 1u) * 4u - mbits) % 4u};
		constexpr ::std::size_t fractional_hex_digits{(mbits + makeup_bits) >> 2u};
		constexpr ::std::size_t total_hex_digits{fractional_hex_digits + 1u};
		if (exponent == exponent_mask_u32)
		{
			return prsv_fp_nan_impl<showpos, uppercase, nan_show_sign, nan_show_type, mbits>(iter, mantissa, sign);
		}
		if (!precision)
		{
			precision = 1u;
		}
		iter = print_rsv_fp_sign_impl<showpos>(iter, sign);
		if constexpr (showbase)
		{
			iter = print_reserve_show_base_impl<16, showbase_uppercase, false>(iter);
		}
		if (!mantissa && !exponent)
		{
			iter = print_rsvhexfloat_digit_impl<uppercase>(iter, 0u);
			if (1u < precision)
			{
				*iter = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
				++iter;
				iter = ::fast_io::details::my_fill_n(iter, precision - 1u, char_literal_v<u8'0', char_type>);
			}
			*iter = char_literal_v < uppercase_e ? u8'P' : u8'p', char_type > ;
			++iter;
			return with_sign_prt_rsv_exponent_hex_impl<trait::e2hexdigits>(iter, 0);
		}
		::std::int_least32_t e2{static_cast<::std::int_least32_t>(exponent) + minus_bias};
		if (exponent == 0)
		{
			++e2;
		}
		auto aligned_mantissa{static_cast<mantissa_type>(mantissa << makeup_bits)};
		auto hex_digit_at = [aligned_mantissa, exponent](::std::size_t index) constexpr noexcept -> ::std::uint_least32_t {
			if (!index)
			{
				return exponent ? 1u : 0u;
			}
			if (fractional_hex_digits < index)
			{
				return 0u;
			}
			auto const shift{static_cast<::std::uint_least32_t>((fractional_hex_digits - index) << 2u)};
			return static_cast<::std::uint_least32_t>(
				(aligned_mantissa >> shift) & static_cast<mantissa_type>(0xFu));
		};
		char8_t digits[total_hex_digits + 1u]{};
		auto const retained_digits{precision < total_hex_digits ? precision : total_hex_digits};
		for (::std::size_t index{}; index != retained_digits; ++index)
		{
			digits[index] = static_cast<char8_t>(hex_digit_at(index));
		}
		if (precision < total_hex_digits)
		{
			auto const next_digit{hex_digit_at(precision)};
			bool tail_nonzero{};
			for (auto index{precision + 1u}; index != total_hex_digits; ++index)
			{
				if (hex_digit_at(index))
				{
					tail_nonzero = true;
					break;
				}
			}
			if (::fast_io::details::print_rsvhexfloat_round_up<rounding>(
					sign, digits[precision - 1u], next_digit, tail_nonzero))
			{
				for (auto index{precision}; index; --index)
				{
					auto &digit{digits[index - 1u]};
					if (digit != 0xFu)
					{
						++digit;
						break;
					}
					digit = 0u;
				}
				if (exponent != 0 && digits[0] == 2u)
				{
					digits[0] = 1u;
					for (::std::size_t index{1u}; index != retained_digits; ++index)
					{
						digits[index] = 0u;
					}
					++e2;
				}
			}
		}
		iter = print_rsvhexfloat_digit_impl<uppercase>(iter, digits[0]);
		if (1u < precision)
		{
			*iter = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
			++iter;
			for (::std::size_t index{1u}; index != retained_digits; ++index)
			{
				iter = print_rsvhexfloat_digit_impl<uppercase>(iter, digits[index]);
			}
			if (retained_digits < precision)
			{
				iter = ::fast_io::details::my_fill_n(iter, precision - retained_digits,
													 char_literal_v<u8'0', char_type>);
			}
		}
		*iter = char_literal_v < uppercase_e ? u8'P' : u8'p', char_type > ;
		++iter;
		return with_sign_prt_rsv_exponent_hex_impl<trait::e2hexdigits>(iter, e2);
	}
}

template <bool showbase, typename mantissa_type>
inline constexpr ::std::size_t print_rsvhexfloat_size_cache{
	print_integer_reserved_size_cache<16, showbase, true, false, mantissa_type> + 6 +
	print_integer_reserved_size_cache<10, true, true, false, ::std::int_least32_t>};

} // namespace details

namespace manipulators
{

template <bool noskipws = false, bool prefix = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_floating_point scalar_type>
inline constexpr scalar_manip_t<::fast_io::manipulators::scalar_flags{.showbase = prefix,
																	  .noskipws = noskipws,
																	  .floating = ::fast_io::manipulators::floating_format::hexfloat,
																	  .allow_leading_plus = allow_leading_plus},
								scalar_type &>
	hexfloat_get(scalar_type &t) noexcept
{
	return {t};
}

template <bool noskipws = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_floating_point scalar_type>
inline constexpr scalar_manip_t<::fast_io::manipulators::scalar_flags{.showbase = true,
																	  .noskipws = noskipws,
																	  .floating = ::fast_io::manipulators::floating_format::hexfloat,
																	  .allow_leading_plus = allow_leading_plus},
								scalar_type &>
	hexfloat0x_get(scalar_type &t) noexcept
{
	return {t};
}

} // namespace manipulators

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point T>
	requires(flags.floating == ::fast_io::manipulators::floating_format::hexfloat)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
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

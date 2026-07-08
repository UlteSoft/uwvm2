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
[[nodiscard]] inline constexpr bool scan_hexfloat_caseless_equal_ascii(char_type ch, char8_t lower) noexcept
{
	auto const lower_ch{::fast_io::char_literal_add<char_type, u8'\0'>(lower)};
	auto const upper_ch{::fast_io::char_literal_add<char_type, u8'\0'>(
		static_cast<char8_t>(lower - static_cast<char8_t>(u8'a' - u8'A')))};
	return ch == lower_ch || ch == upper_ch;
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

template <::fast_io::manipulators::floating_nan_payload_scan nan_payload_scan, ::std::integral char_type>
[[nodiscard]] inline constexpr bool
scan_hexfloat_special_parse_may_extend(char_type const *first, char_type const *last) noexcept
{
	if (first == last)
	{
		return true;
	}
	if (::fast_io::details::scan_hexfloat_caseless_equal<u8'i', u8'I'>(*first))
	{
		constexpr char8_t infinity_suffix[]{u8"inity"};
		auto scan{first};
		for (::std::size_t index{}; scan != last && index != 5u; ++scan, ++index)
		{
			if (!::fast_io::details::scan_hexfloat_caseless_equal_ascii(*scan, infinity_suffix[index]))
			{
				return false;
			}
		}
		return scan == last;
	}
	if constexpr (nan_payload_scan != ::fast_io::manipulators::floating_nan_payload_scan::none)
	{
		if (*first == ::fast_io::char_literal_v<u8'(', char_type>)
		{
			for (auto scan{first + 1}; scan != last; ++scan)
			{
				if (*scan == ::fast_io::char_literal_v<u8')', char_type>)
				{
					return false;
				}
				if (!::fast_io::details::scan_hexfloat_nan_sequence_char(*scan))
				{
					return false;
				}
			}
			return true;
		}
	}
	return false;
}

template <typename storage_type_t>
struct scan_hexfloat_significand_state
{
	using storage_type = storage_type_t;
	bool has_digit{};
	bool has_nonzero_digit{};
	storage_type stored{};
	::std::size_t stored_hex_digits{};
	::std::int_least64_t significant_hex_digits{};
	::std::int_least64_t fractional_hex_digits{};
	bool truncated_nonzero{};
};

template <::std::size_t precision_bits>
struct scan_hexfloat_storage_type
{
	inline static constexpr ::std::size_t stored_hex_digits_limit{(precision_bits + 7u) / 4u};
#ifdef __SIZEOF_INT128__
	using type = ::std::conditional_t<
		(stored_hex_digits_limit * 4u <= ::std::numeric_limits<::std::uint_least64_t>::digits),
		::std::uint_least64_t, __uint128_t>;
#else
	static_assert(stored_hex_digits_limit * 4u <= ::std::numeric_limits<::std::uint_least64_t>::digits,
				  "fast_io hexfloat scan needs 128-bit integer support for this floating-point type");
	using type = ::std::uint_least64_t;
#endif
};

template <::std::size_t precision_bits>
using scan_hexfloat_storage_t = typename ::fast_io::details::scan_hexfloat_storage_type<precision_bits>::type;

template <::std::size_t stored_hex_digits_limit, typename state_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scan_hexfloat_append_digit(state_type &state, bool after_decimal,
												 char8_t digit) noexcept
{
	using storage_type = typename state_type::storage_type;
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
		state.stored = static_cast<storage_type>((state.stored << 4u) | static_cast<storage_type>(digit));
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
[[nodiscard]] inline constexpr bool scan_hexfloat_ascii8_is_xdigits(::std::uint_least64_t val) noexcept
{
	constexpr ::std::uint_least64_t first_bound1{0x3939393939393939};
	constexpr ::std::uint_least64_t first_bound2{0x1919191919191919};
	auto const invalid{((((val + 0x4646464646464646) | (val - 0x3030303030303030)) &
						 ((val + first_bound1) | (val - 0x4040404040404040)) &
						 ((val + first_bound2) | (val - 0x6060606060606060))) |
						~(((val + 0x3f3f3f3f3f3f3f3f) | (val - 0x4040404040404040)) &
						  ((val + 0x1f1f1f1f1f1f1f1f) | (val - 0x6060606060606060)))) &
					   0x8080808080808080};
	return !invalid;
}

[[nodiscard]] inline constexpr bool scan_hexfloat_ascii8_has_nonzero_digit(::std::uint_least64_t val) noexcept
{
	return (val ^ 0x3030303030303030) != 0;
}

template <::std::size_t vec_size, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *
scan_hexfloat_skip_after_storage_limit_simd(char_type const *first, char_type const *last,
											bool &truncated_nonzero) noexcept
{
	static_assert(sizeof(char_type) == sizeof(char8_t));
	using unsigned_char_type = ::std::make_unsigned_t<::std::remove_cvref_t<char_type>>;
	using signed_char_type = ::std::make_signed_t<unsigned_char_type>;
	constexpr unsigned N{vec_size / sizeof(char_type)};
	using simd_vector_type = ::fast_io::intrinsics::simd_vector<signed_char_type, N>;
#if (__cpp_lib_bit_cast >= 201806L) && !defined(__clang__)
	constexpr simd_vector_type zeroes{
		::std::bit_cast<simd_vector_type>(::fast_io::details::characters_array_impl<u8'0', char_type, N>)};
	constexpr simd_vector_type nines{
		::std::bit_cast<simd_vector_type>(::fast_io::details::characters_array_impl<u8'9', char_type, N>)};
	constexpr simd_vector_type lower_as{
		::std::bit_cast<simd_vector_type>(::fast_io::details::characters_array_impl<u8'a', char_type, N>)};
	constexpr simd_vector_type lower_fs{
		::std::bit_cast<simd_vector_type>(::fast_io::details::characters_array_impl<u8'f', char_type, N>)};
	constexpr simd_vector_type case_bits{
		::std::bit_cast<simd_vector_type>(::fast_io::details::characters_array_impl<u8' ', char_type, N>)};
#else
	simd_vector_type zeroes;
	zeroes.load(::fast_io::details::characters_array_impl<u8'0', char_type, N>.data());
	simd_vector_type nines;
	nines.load(::fast_io::details::characters_array_impl<u8'9', char_type, N>.data());
	simd_vector_type lower_as;
	lower_as.load(::fast_io::details::characters_array_impl<u8'a', char_type, N>.data());
	simd_vector_type lower_fs;
	lower_fs.load(::fast_io::details::characters_array_impl<u8'f', char_type, N>.data());
	simd_vector_type case_bits;
	case_bits.load(::fast_io::details::characters_array_impl<u8' ', char_type, N>.data());
#endif
	for (; N <= static_cast<::std::size_t>(last - first);)
	{
		simd_vector_type vec;
		vec.load(first);
		auto const lower{vec | case_bits};
		auto const valid{((zeroes <= vec) & (vec <= nines)) |
						 ((lower_as <= lower) & (lower <= lower_fs))};
		auto const invalid{~valid};
		if (!::fast_io::intrinsics::is_all_zeros(invalid))
		{
			auto const valid_count{::fast_io::intrinsics::vector_mask_countr_zero(invalid)};
			auto const *const invalid_pos{first + valid_count};
			using char_unsigned_type = ::fast_io::details::my_make_unsigned_t<char_type>;
			for (; first != invalid_pos; ++first)
			{
				auto digit{static_cast<char_unsigned_type>(*first)};
				(void)::fast_io::details::char_digit_to_literal<16, char_type>(digit);
				if (!truncated_nonzero && digit != 0)
				{
					truncated_nonzero = true;
				}
			}
			return first;
		}
		if (!truncated_nonzero && !::fast_io::intrinsics::is_all_zeros(vec != zeroes))
		{
			truncated_nonzero = true;
		}
		first += N;
	}
	return first;
}

[[nodiscard]] inline constexpr bool scan_hexfloat_ascii8_nibbles_only(::std::uint_least64_t val,
																	  ::std::uint_least64_t &nibbles) noexcept
{
	if (!::fast_io::details::scan_hexfloat_ascii8_is_xdigits(val))
	{
		return false;
	}
	val -= 0x3030303030303030;
	nibbles = (val & 0x0f0f0f0f0f0f0f0f) + ((val & 0x1010101010101010) >> 4) * 9u;
	return true;
}

[[nodiscard]] inline constexpr bool scan_hexfloat_ascii8_nibbles(::std::uint_least64_t val,
																 ::std::uint_least64_t &nibbles,
																 ::std::uint_least64_t &packed) noexcept
{
	if (!::fast_io::details::scan_hexfloat_ascii8_nibbles_only(val, nibbles))
	{
		return false;
	}
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
	return true;
}

template <::std::integral char_type, typename state_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *scan_hexfloat_skip_after_storage_limit_run(char_type const *first,
																			 char_type const *last,
																			 bool after_decimal,
																			 state_type &state) noexcept
{
	auto const *const original_first{first};
	auto truncated_nonzero{state.truncated_nonzero};
#ifdef __cpp_if_consteval
	if !consteval
#else
	if (!__builtin_is_constant_evaluated())
#endif
	{
		if constexpr (!::fast_io::details::is_ebcdic<char_type> && sizeof(char_type) == sizeof(char8_t) &&
					  ::std::numeric_limits<::std::uint_least64_t>::digits == 64u)
		{
			constexpr auto simd_size{
				::fast_io::intrinsics::optimal_simd_vector_run_with_cpu_instruction_size_with_mask_countr};
			if constexpr (simd_size != 0)
			{
				first = ::fast_io::details::scan_hexfloat_skip_after_storage_limit_simd<simd_size>(
					first, last, truncated_nonzero);
			}
			for (; static_cast<::std::size_t>(last - first) >= sizeof(::std::uint_least64_t);)
			{
				::std::uint_least64_t val;
				::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));
				if constexpr (::std::endian::little != ::std::endian::native)
				{
					val = ::fast_io::little_endian(val);
				}
				if (!::fast_io::details::scan_hexfloat_ascii8_is_xdigits(val))
				{
					break;
				}
				if (!truncated_nonzero &&
					::fast_io::details::scan_hexfloat_ascii8_has_nonzero_digit(val))
				{
					truncated_nonzero = true;
				}
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
		if (!truncated_nonzero && uch != 0)
		{
			truncated_nonzero = true;
		}
	}
	auto const skipped{static_cast<::std::int_least64_t>(first - original_first)};
	if (skipped)
	{
		state.has_digit = true;
		if (after_decimal)
		{
			state.fractional_hex_digits += skipped;
		}
		state.significant_hex_digits += skipped;
		state.truncated_nonzero = truncated_nonzero;
	}
	return first;
}

template <::std::size_t stored_hex_digits_limit, typename state_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void scan_hexfloat_append_ascii8(state_type &state, bool after_decimal,
												  ::std::uint_least64_t nibbles,
												  ::std::uint_least64_t packed) noexcept
{
	using storage_type = typename state_type::storage_type;
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
		state.stored = static_cast<storage_type>(
			(state.stored << static_cast<unsigned>(significant_digits * 4u)) |
			static_cast<storage_type>(packed));
		state.stored_hex_digits += static_cast<::std::size_t>(significant_digits);
		return;
	}
	auto const truncated_digits{significant_digits - available};
	auto const truncated_bits{static_cast<unsigned>(truncated_digits * 4u)};
	state.stored = static_cast<storage_type>(
		(state.stored << static_cast<unsigned>(available * 4u)) |
		static_cast<storage_type>(packed >> truncated_bits));
	state.stored_hex_digits = stored_hex_digits_limit;
	if ((packed & ((::std::uint_least64_t{1} << truncated_bits) - 1u)) != 0)
	{
		state.truncated_nonzero = true;
	}
}

template <::std::size_t stored_hex_digits_limit, ::std::integral char_type, typename state_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr char_type const *scan_hexfloat_significand_run(char_type const *first, char_type const *last,
																bool after_decimal,
																state_type &state) noexcept
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
				if (state.has_nonzero_digit && state.stored_hex_digits == stored_hex_digits_limit)
				{
					return ::fast_io::details::scan_hexfloat_skip_after_storage_limit_run(first, last, after_decimal, state);
				}
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
	if (state.has_nonzero_digit && state.stored_hex_digits == stored_hex_digits_limit)
	{
		return ::fast_io::details::scan_hexfloat_skip_after_storage_limit_run(first, last, after_decimal, state);
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
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
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

template <::fast_io::details::my_unsigned_integral storage_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr ::std::int_least64_t scan_hexfloat_bit_width(storage_type stored) noexcept
{
	if constexpr (sizeof(storage_type) <= sizeof(::std::uint_least64_t))
	{
		return static_cast<::std::int_least64_t>(
			::std::bit_width(static_cast<::std::uint_least64_t>(stored)));
	}
#ifdef __SIZEOF_INT128__
	else
	{
		auto const high{static_cast<::std::uint_least64_t>(stored >> 64u)};
		if (high != 0)
		{
			return static_cast<::std::int_least64_t>(64u + ::std::bit_width(high));
		}
		return static_cast<::std::int_least64_t>(
			::std::bit_width(static_cast<::std::uint_least64_t>(stored)));
	}
#endif
}

template <::fast_io::details::my_unsigned_integral storage_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr storage_type scan_hexfloat_low_mask(::std::int_least64_t bits) noexcept
{
	constexpr auto storage_bits{static_cast<::std::int_least64_t>(sizeof(storage_type) *
																 ::std::numeric_limits<unsigned char>::digits)};
	if (bits <= 0)
	{
		return {};
	}
	if (bits >= storage_bits)
	{
		return static_cast<storage_type>(~storage_type{});
	}
	return static_cast<storage_type>((storage_type{1} << static_cast<unsigned>(bits)) - storage_type{1});
}

template <::fast_io::manipulators::floating_rounding rounding,
		  ::fast_io::details::my_unsigned_integral storage_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[nodiscard]] inline constexpr storage_type scan_hexfloat_round_shift(storage_type stored,
																	  ::std::int_least64_t remaining_bits,
																	  bool truncated_nonzero,
																	  bool negative,
																	  ::std::int_least64_t shift) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return ::fast_io::details::scan_hexfloat_round_shift<
				::fast_io::manipulators::floating_rounding::toward_plus_infinity>(
				stored, remaining_bits, truncated_nonzero, negative, shift);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return ::fast_io::details::scan_hexfloat_round_shift<
				::fast_io::manipulators::floating_rounding::toward_minus_infinity>(
				stored, remaining_bits, truncated_nonzero, negative, shift);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return ::fast_io::details::scan_hexfloat_round_shift<
				::fast_io::manipulators::floating_rounding::toward_zero>(
				stored, remaining_bits, truncated_nonzero, negative, shift);
		default:
			return ::fast_io::details::scan_hexfloat_round_shift<
				::fast_io::manipulators::floating_rounding::nearest_to_even>(
				stored, remaining_bits, truncated_nonzero, negative, shift);
		}
	}
	constexpr auto storage_bits{static_cast<::std::int_least64_t>(sizeof(storage_type) *
																 ::std::numeric_limits<unsigned char>::digits)};
	auto const stored_bits{::fast_io::details::scan_hexfloat_bit_width(stored)};
	if (shift <= 0)
	{
		auto const left_shift{remaining_bits - shift};
		return left_shift >= storage_bits
				   ? storage_type{}
				   : static_cast<storage_type>(stored << static_cast<unsigned>(left_shift));
	}

	auto const shift_in_stored{shift - remaining_bits};
	if (shift_in_stored <= 0)
	{
		return 0u;
	}
	if (shift_in_stored > stored_bits)
	{
		if constexpr (!::fast_io::details::floating_rounding_is_nearest<rounding>)
		{
			if ((stored != 0u || truncated_nonzero) &&
				::fast_io::details::floating_rounding_directed_round_up<rounding>(negative))
			{
				return 1u;
			}
		}
		return 0u;
	}

	storage_type quotient{};
	bool half{};
	bool below_half{truncated_nonzero};
	if (shift_in_stored == stored_bits)
	{
		half = true;
		auto const below_mask{::fast_io::details::scan_hexfloat_low_mask<storage_type>(stored_bits - 1)};
		below_half = below_half || (stored & below_mask) != 0u;
	}
	else
	{
		quotient = stored >> static_cast<unsigned>(shift_in_stored);
		half = ((stored >> static_cast<unsigned>(shift_in_stored - 1)) & 1u) != 0u;
		if (shift_in_stored > 1)
		{
			auto const below_mask{::fast_io::details::scan_hexfloat_low_mask<storage_type>(shift_in_stored - 1)};
			below_half = below_half || (stored & below_mask) != 0u;
		}
	}
	bool const discarded_nonzero{half || below_half};
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		if (half &&
			(below_half ||
			 ::fast_io::details::floating_rounding_nearest_tie_round_up<rounding>(
				 negative, static_cast<::std::uint_least64_t>(quotient) << 1u)))
		{
			++quotient;
		}
	}
	else
	{
		if (discarded_nonzero && ::fast_io::details::floating_rounding_directed_round_up<rounding>(negative))
		{
			++quotient;
		}
	}
	return quotient;
}

template <typename T, ::fast_io::details::my_unsigned_integral storage_type,
		  ::fast_io::manipulators::floating_rounding rounding =
			  ::fast_io::manipulators::floating_rounding::nearest_to_even>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::parse_code scan_hexfloat_assign_ieee_result(T &value, bool negative,
																		storage_type stored,
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
	static_assert(precision_bits <= sizeof(storage_type) * ::std::numeric_limits<unsigned char>::digits);
	static_assert(sizeof(mantissa_type) <= sizeof(storage_type));
	constexpr auto bias{static_cast<::std::int_least64_t>((static_cast<::std::uint_least32_t>(1u) << ebits) >> 1u) - 1};
	constexpr auto min_exponent{1 - bias};
	constexpr auto max_exponent{bias};
	constexpr auto int64_max{(::std::numeric_limits<::std::int_least64_t>::max)()};
	constexpr auto int64_min{(::std::numeric_limits<::std::int_least64_t>::min)()};

	auto const stored_bits{::fast_io::details::scan_hexfloat_bit_width(stored)};
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

	if constexpr (::fast_io::details::fp_floating_point_is_float80<T>)
	{
		if (exponent2 >= min_exponent)
		{
			auto significand{::fast_io::details::scan_hexfloat_round_shift<rounding>(
				stored, remaining_bits, truncated_nonzero,
				negative,
				total_bits - static_cast<::std::int_least64_t>(precision_bits))};
			auto exponent_field{static_cast<::std::uint_least32_t>(exponent2 + bias)};
			if (significand == static_cast<storage_type>(storage_type{1} << precision_bits))
			{
				significand >>= 1u;
				if (exponent2 == max_exponent)
				{
					return ::fast_io::parse_code::overflow;
				}
				++exponent_field;
			}
			::fast_io::details::fp_assign_float80_bits(value, static_cast<::std::uint_least64_t>(significand),
													   exponent_field, negative);
			return ::fast_io::parse_code::ok;
		}

		auto const subnormal_shift{(min_exponent - static_cast<::std::int_least64_t>(mbits)) - binary_exponent};
		auto const fraction{
			::fast_io::details::scan_hexfloat_round_shift<rounding>(stored, remaining_bits, truncated_nonzero,
																	negative, subnormal_shift)};
		if (fraction == 0u)
		{
			return ::fast_io::parse_code::overflow;
		}
		if (fraction >= static_cast<storage_type>(storage_type{1} << mbits))
		{
			::fast_io::details::fp_assign_float80_bits(value, ::std::uint_least64_t{1} << mbits, 1u, negative);
		}
		else
		{
			::fast_io::details::fp_assign_float80_bits(value, static_cast<::std::uint_least64_t>(fraction), 0u,
													   negative);
		}
		return ::fast_io::parse_code::ok;
	}
	else
	{
		mantissa_type result_bits{};
		if (negative)
		{
			result_bits = static_cast<mantissa_type>(mantissa_type{1} << (mbits + ebits));
		}

		if (exponent2 >= min_exponent)
		{
			auto significand{::fast_io::details::scan_hexfloat_round_shift<rounding>(
				stored, remaining_bits, truncated_nonzero,
				negative,
				total_bits - static_cast<::std::int_least64_t>(precision_bits))};
			if (significand == static_cast<storage_type>(storage_type{1} << precision_bits))
			{
				significand >>= 1u;
				if (exponent2 == max_exponent)
				{
					return ::fast_io::parse_code::overflow;
				}
				result_bits |= static_cast<mantissa_type>(
					static_cast<mantissa_type>(static_cast<::std::uint_least64_t>(exponent2 + 1 + bias)) << mbits);
			}
			else
			{
				result_bits |= static_cast<mantissa_type>(
					static_cast<mantissa_type>(static_cast<::std::uint_least64_t>(exponent2 + bias)) << mbits);
			}
			result_bits |=
				static_cast<mantissa_type>(significand - static_cast<storage_type>(storage_type{1} << mbits));
			::fast_io::details::fp_assign_bits(value, result_bits);
			return ::fast_io::parse_code::ok;
		}

		auto const subnormal_shift{(min_exponent - static_cast<::std::int_least64_t>(mbits)) - binary_exponent};
		auto const fraction{
			::fast_io::details::scan_hexfloat_round_shift<rounding>(stored, remaining_bits, truncated_nonzero,
																	negative, subnormal_shift)};
		if (fraction == 0u)
		{
			return ::fast_io::parse_code::overflow;
		}
		if (fraction >= static_cast<storage_type>(storage_type{1} << mbits))
		{
			result_bits |= static_cast<mantissa_type>(mantissa_type{1} << mbits);
		}
		else
		{
			result_bits |= static_cast<mantissa_type>(fraction);
		}
		::fast_io::details::fp_assign_bits(value, result_bits);
		return ::fast_io::parse_code::ok;
	}
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
	using storage_type = ::fast_io::details::scan_hexfloat_storage_t<precision_bits>;
	static_assert(stored_hex_digits_limit * 4u <= sizeof(storage_type) * ::std::numeric_limits<unsigned char>::digits);
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

	::fast_io::details::scan_hexfloat_significand_state<storage_type> significand_state;
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
			::fast_io::details::scan_hexfloat_assign_ieee_result<T, storage_type, flags.rounding>(
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
	using storage_type = ::fast_io::details::scan_hexfloat_storage_t<precision_bits>;
	static_assert(stored_hex_digits_limit * 4u <= sizeof(storage_type) * ::std::numeric_limits<unsigned char>::digits);
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

	::fast_io::details::scan_hexfloat_significand_state<storage_type> significand_state;
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
			::fast_io::details::scan_hexfloat_assign_ieee_result<T, storage_type, flags.rounding>(
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
		  ::fast_io::manipulators::floating_precision precision_mode =
			  ::fast_io::manipulators::floating_precision::significant,
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
				::fast_io::manipulators::floating_rounding::toward_plus_infinity, precision_mode, nan_show_sign,
				nan_show_type>(iter, f, precision);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return print_rsvhexfloat_precision_define_impl<
				showbase, showbase_uppercase, showpos, uppercase, uppercase_e, comma,
				::fast_io::manipulators::floating_rounding::toward_minus_infinity, precision_mode, nan_show_sign,
				nan_show_type>(iter, f, precision);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return print_rsvhexfloat_precision_define_impl<
				showbase, showbase_uppercase, showpos, uppercase, uppercase_e, comma,
				::fast_io::manipulators::floating_rounding::toward_zero, precision_mode, nan_show_sign,
				nan_show_type>(iter, f, precision);
		default:
			return print_rsvhexfloat_precision_define_impl<
				showbase, showbase_uppercase, showpos, uppercase, uppercase_e, comma,
				::fast_io::manipulators::floating_rounding::nearest_to_even, precision_mode, nan_show_sign,
				nan_show_type>(iter, f, precision);
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
		constexpr bool fractional_precision{
			::fast_io::details::floating_precision_is_fractional<precision_mode>};
		constexpr bool preserve_trailing_zero{
			::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>};
		::std::size_t total_precision{precision};
		if constexpr (fractional_precision)
		{
			++total_precision;
		}
		else if (!total_precision)
		{
			total_precision = 1u;
		}
		iter = print_rsv_fp_sign_impl<showpos>(iter, sign);
		if constexpr (showbase)
		{
			iter = print_reserve_show_base_impl<16, showbase_uppercase, false>(iter);
		}
		if (!mantissa && !exponent)
		{
			iter = print_rsvhexfloat_digit_impl<uppercase>(iter, 0u);
			auto const digits_to_print{preserve_trailing_zero ? total_precision : 1u};
			if (1u < digits_to_print)
			{
				*iter = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
				++iter;
				iter = ::fast_io::details::my_fill_n(iter, digits_to_print - 1u, char_literal_v<u8'0', char_type>);
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
		auto const retained_digits{total_precision < total_hex_digits ? total_precision : total_hex_digits};
		for (::std::size_t index{}; index != retained_digits; ++index)
		{
			digits[index] = static_cast<char8_t>(hex_digit_at(index));
		}
		if (total_precision < total_hex_digits)
		{
			auto const next_digit{hex_digit_at(total_precision)};
			bool tail_nonzero{};
			for (auto index{total_precision + 1u}; index != total_hex_digits; ++index)
			{
				if (hex_digit_at(index))
				{
					tail_nonzero = true;
					break;
				}
			}
			if (::fast_io::details::print_rsvhexfloat_round_up<rounding>(
					sign, digits[total_precision - 1u], next_digit, tail_nonzero))
			{
				for (auto index{total_precision}; index; --index)
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
		auto digits_to_print{retained_digits};
		if constexpr (preserve_trailing_zero)
		{
			digits_to_print = total_precision;
		}
		else
		{
			for (; 1u < digits_to_print && !digits[digits_to_print - 1u]; --digits_to_print)
			{
			}
		}
		iter = print_rsvhexfloat_digit_impl<uppercase>(iter, digits[0]);
		if (1u < digits_to_print)
		{
			*iter = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
			++iter;
			auto const digit_limit{digits_to_print < retained_digits ? digits_to_print : retained_digits};
			for (::std::size_t index{1u}; index != digit_limit; ++index)
			{
				iter = print_rsvhexfloat_digit_impl<uppercase>(iter, digits[index]);
			}
			if (digit_limit < digits_to_print)
			{
				iter = ::fast_io::details::my_fill_n(iter, digits_to_print - digit_limit,
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

template <::std::integral char_type>
struct scan_floating_context
{
	static inline constexpr ::std::size_t capacity{8192u};
	::fast_io::freestanding::array<char_type, capacity> buffer;
	::std::size_t size{};
};

template <::std::integral char_type, bool noskipws>
inline constexpr char_type const *scan_floating_context_skip_space(char_type const *first,
																   char_type const *last) noexcept
{
	if constexpr (noskipws)
	{
		return first;
	}
	else
	{
		return ::fast_io::details::find_space_common_impl<false, true>(first, last);
	}
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr char_type const *
scan_floating_context_map_iter(scan_floating_context<char_type> const &state, ::std::size_t old_size,
							   char_type const *chunk_begin, char_type const *chunk_end,
							   char_type const *parsed_iter) noexcept
{
	auto const offset{static_cast<::std::size_t>(parsed_iter - state.buffer.data())};
	if (offset <= old_size)
	{
		return chunk_begin;
	}
	auto const consumed{offset - old_size};
	auto const chunk_size{static_cast<::std::size_t>(chunk_end - chunk_begin)};
	if (chunk_size < consumed)
	{
		return chunk_end;
	}
	return chunk_begin + consumed;
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_floating_context_append(scan_floating_context<char_type> &state, char_type const *chunk_begin,
							 char_type const *chunk_end) noexcept
{
	auto const chunk_size{static_cast<::std::size_t>(chunk_end - chunk_begin)};
	if (!chunk_size)
	{
		return {chunk_end, ::fast_io::parse_code::partial};
	}
	if (scan_floating_context<char_type>::capacity - state.size < chunk_size)
	{
		return {chunk_begin, ::fast_io::parse_code::overflow};
	}
	::fast_io::freestanding::non_overlapped_copy_n(chunk_begin, chunk_size, state.buffer.data() + state.size);
	state.size += chunk_size;
	return {chunk_end, ::fast_io::parse_code::partial};
}

enum class scan_hexfloat_context_phase : ::std::uint_least8_t
{
	start,
	after_sign,
	prefix_zero,
	prefix_x,
	integer,
	fraction,
	exponent_start,
	exponent_after_sign,
	exponent_digits,
	special
};

template <::std::integral char_type, typename storage_type>
struct scan_hexfloat_context
{
	::fast_io::details::scan_hexfloat_significand_state<storage_type> significand_state;
	::fast_io::details::scan_floating_context<char_type> special_buffer;
	::std::uint_least64_t exponent{};
	::fast_io::details::scan_hexfloat_context_phase phase{};
	char_type sign_char{};
	bool negative{};
	bool has_sign{};
	bool special_sign_prefixed{};
	bool has_exponent_marker{};
	bool exponent_negative{};
	bool exponent_has_digit{};
	bool exponent_overflow{};
};

template <::std::integral char_type, typename storage_type>
inline constexpr void scan_hexfloat_context_append_exponent_digit(
	::fast_io::details::scan_hexfloat_context<char_type, storage_type> &state, char8_t digit) noexcept
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

template <::std::integral char_type, typename storage_type>
[[nodiscard]] inline constexpr ::std::int_least64_t
scan_hexfloat_context_exponent(
	::fast_io::details::scan_hexfloat_context<char_type, storage_type> const &state) noexcept
{
	return state.exponent_negative ? -static_cast<::std::int_least64_t>(state.exponent)
								   : static_cast<::std::int_least64_t>(state.exponent);
}

template <typename T, ::fast_io::manipulators::scalar_flags flags, ::std::integral char_type,
		  typename storage_type>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_hexfloat_context_assign(
	::fast_io::details::scan_hexfloat_context<char_type, storage_type> const &state, T &value) noexcept
{
	if (!state.significand_state.has_digit || !state.has_exponent_marker || !state.exponent_has_digit)
	{
		return ::fast_io::parse_code::invalid;
	}
	if (!state.significand_state.has_nonzero_digit)
	{
		value = state.negative ? -static_cast<T>(0.0) : static_cast<T>(0.0);
		return ::fast_io::parse_code::ok;
	}
	constexpr auto int64_max{(::std::numeric_limits<::std::int_least64_t>::max)()};
	constexpr auto int64_min{(::std::numeric_limits<::std::int_least64_t>::min)()};
	auto const fractional_exponent{state.significand_state.fractional_hex_digits > int64_max / 4
									   ? int64_max
									   : state.significand_state.fractional_hex_digits * 4};
	auto const exponent{::fast_io::details::scan_hexfloat_context_exponent(state)};
	auto const binary_exponent{exponent < int64_min + fractional_exponent ? int64_min
																		   : exponent - fractional_exponent};
	return ::fast_io::details::scan_hexfloat_assign_ieee_result<T, storage_type, flags.rounding>(
		value, state.negative, state.significand_state.stored, state.significand_state.stored_hex_digits,
		state.significand_state.significant_hex_digits, state.significand_state.truncated_nonzero,
		binary_exponent);
}

template <::std::integral char_type, typename storage_type>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_hexfloat_context_append_special_prefix(
	::fast_io::details::scan_hexfloat_context<char_type, storage_type> &state,
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
		  ::fast_io::details::my_floating_point T, typename storage_type>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_hexfloat_context_special_define(
	::fast_io::details::scan_hexfloat_context<char_type, storage_type> &state,
	char_type const *begin, char_type const *end, T &value) noexcept
{
	auto prefix_result{::fast_io::details::scan_hexfloat_context_append_special_prefix(state, begin)};
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
	auto parse_result{::fast_io::details::scan_hexfloat_contiguous_define_impl<char_type, flags>(
		buffer_begin, buffer_end, parsed_value)};
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
		  ::fast_io::details::my_floating_point T, typename storage_type>
[[nodiscard]] inline constexpr ::fast_io::parse_result<char_type const *>
scan_hexfloat_context_numeric_define(
	::fast_io::details::scan_hexfloat_context<char_type, storage_type> &state,
	char_type const *first, char_type const *end, T &value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<T>;
	constexpr ::std::size_t precision_bits{trait::mbits + 1u};
	constexpr ::std::size_t stored_hex_digits_limit{(precision_bits + 7u) / 4u};
	constexpr auto plus{::fast_io::char_literal_v<u8'+', char_type>};
	constexpr auto minus{::fast_io::char_literal_v<u8'-', char_type>};
	constexpr auto zero{::fast_io::char_literal_v<u8'0', char_type>};
	constexpr auto lower_x{::fast_io::char_literal_v<u8'x', char_type>};
	constexpr auto upper_x{::fast_io::char_literal_v<u8'X', char_type>};
	constexpr auto dot{::fast_io::char_literal_v<u8'.', char_type>};
	constexpr auto lower_p{::fast_io::char_literal_v<u8'p', char_type>};
	constexpr auto upper_p{::fast_io::char_literal_v<u8'P', char_type>};
	for (;;)
	{
		switch (state.phase)
		{
		case ::fast_io::details::scan_hexfloat_context_phase::start:
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
				state.phase = ::fast_io::details::scan_hexfloat_context_phase::after_sign;
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
					state.phase = ::fast_io::details::scan_hexfloat_context_phase::after_sign;
					if (first == end)
					{
						return {end, ::fast_io::parse_code::partial};
					}
					break;
				}
			}
			state.phase = ::fast_io::details::scan_hexfloat_context_phase::after_sign;
			break;
		case ::fast_io::details::scan_hexfloat_context_phase::after_sign:
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			if constexpr (flags.showbase)
			{
				if (*first == zero)
				{
					++first;
					state.phase = ::fast_io::details::scan_hexfloat_context_phase::prefix_x;
					if (first == end)
					{
						return {end, ::fast_io::parse_code::partial};
					}
					break;
				}
				state.phase = ::fast_io::details::scan_hexfloat_context_phase::special;
				return ::fast_io::details::scan_hexfloat_context_special_define<char_type, flags, T>(
					state, first, end, value);
			}
			else
			{
				state.phase = ::fast_io::details::scan_hexfloat_context_phase::integer;
				break;
			}
		case ::fast_io::details::scan_hexfloat_context_phase::prefix_zero:
			state.phase = ::fast_io::details::scan_hexfloat_context_phase::prefix_x;
			break;
		case ::fast_io::details::scan_hexfloat_context_phase::prefix_x:
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			if (*first != lower_x && *first != upper_x)
			{
				return {first, ::fast_io::parse_code::invalid};
			}
			++first;
			state.phase = ::fast_io::details::scan_hexfloat_context_phase::integer;
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			break;
		case ::fast_io::details::scan_hexfloat_context_phase::integer:
		{
			if (first != end && *first == dot)
			{
				++first;
				state.phase = ::fast_io::details::scan_hexfloat_context_phase::fraction;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			auto const *before_digits{first};
			first = ::fast_io::details::scan_hexfloat_significand_run<stored_hex_digits_limit>(
				first, end, false, state.significand_state);
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			if (first == before_digits && !state.significand_state.has_digit)
			{
				state.phase = ::fast_io::details::scan_hexfloat_context_phase::special;
				return ::fast_io::details::scan_hexfloat_context_special_define<char_type, flags, T>(
					state, first, end, value);
			}
			if (first != end && *first == dot)
			{
				++first;
				state.phase = ::fast_io::details::scan_hexfloat_context_phase::fraction;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			if (first != end && (*first == lower_p || *first == upper_p))
			{
				state.has_exponent_marker = true;
				state.phase = ::fast_io::details::scan_hexfloat_context_phase::exponent_start;
				++first;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			return {first, ::fast_io::parse_code::invalid};
		}
		case ::fast_io::details::scan_hexfloat_context_phase::fraction:
			first = ::fast_io::details::scan_hexfloat_significand_run<stored_hex_digits_limit>(
				first, end, true, state.significand_state);
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			if (!state.significand_state.has_digit)
			{
				return {first, ::fast_io::parse_code::invalid};
			}
			if (first != end && (*first == lower_p || *first == upper_p))
			{
				state.has_exponent_marker = true;
				state.phase = ::fast_io::details::scan_hexfloat_context_phase::exponent_start;
				++first;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			return {first, ::fast_io::parse_code::invalid};
		case ::fast_io::details::scan_hexfloat_context_phase::exponent_start:
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			if (*first == minus)
			{
				state.exponent_negative = true;
				++first;
				state.phase = ::fast_io::details::scan_hexfloat_context_phase::exponent_after_sign;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			if (*first == plus)
			{
				++first;
				state.phase = ::fast_io::details::scan_hexfloat_context_phase::exponent_after_sign;
				if (first == end)
				{
					return {end, ::fast_io::parse_code::partial};
				}
				break;
			}
			state.phase = ::fast_io::details::scan_hexfloat_context_phase::exponent_digits;
			break;
		case ::fast_io::details::scan_hexfloat_context_phase::exponent_after_sign:
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			state.phase = ::fast_io::details::scan_hexfloat_context_phase::exponent_digits;
			break;
		case ::fast_io::details::scan_hexfloat_context_phase::exponent_digits:
		{
			char8_t digit{};
			for (; first != end && ::fast_io::details::scan_hexfloat_decimal_digit(*first, digit); ++first)
			{
				::fast_io::details::scan_hexfloat_context_append_exponent_digit(state, digit);
			}
			if (first == end)
			{
				return {end, ::fast_io::parse_code::partial};
			}
			auto code{::fast_io::details::scan_hexfloat_context_assign<T, flags>(state, value)};
			return {first, code};
		}
		case ::fast_io::details::scan_hexfloat_context_phase::special:
			return ::fast_io::details::scan_hexfloat_context_special_define<char_type, flags, T>(
				state, first, end, value);
		}
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point T, typename storage_type>
[[nodiscard]] inline constexpr ::fast_io::parse_code
scan_hexfloat_context_eof(
	::fast_io::details::scan_hexfloat_context<char_type, storage_type> &state, T &value) noexcept
{
	if (state.phase == ::fast_io::details::scan_hexfloat_context_phase::special)
	{
		if (!state.special_buffer.size)
		{
			return ::fast_io::parse_code::end_of_file;
		}
		T parsed_value{};
		auto const *buffer_begin{state.special_buffer.buffer.data()};
		auto const *buffer_end{buffer_begin + state.special_buffer.size};
		auto parse_result{::fast_io::details::scan_hexfloat_contiguous_define_impl<char_type, flags>(
			buffer_begin, buffer_end, parsed_value)};
		if (parse_result.code == ::fast_io::parse_code::ok)
		{
			value = parsed_value;
		}
		return parse_result.code;
	}
	if (state.phase == ::fast_io::details::scan_hexfloat_context_phase::start && !state.significand_state.has_digit &&
		!state.has_sign)
	{
		return ::fast_io::parse_code::end_of_file;
	}
	return ::fast_io::details::scan_hexfloat_context_assign<T, flags>(state, value);
}

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

template <::fast_io::manipulators::floating_rounding rounding_policy, bool noskipws = false, bool prefix = false,
		  bool allow_leading_plus = false, ::fast_io::details::my_floating_point scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::floating_precision_rounding_mani_flags_cache<
									::fast_io::manipulators::scalar_flags{
										.showbase = prefix,
										.noskipws = noskipws,
										.floating = ::fast_io::manipulators::floating_format::hexfloat,
										.allow_leading_plus = allow_leading_plus},
									::fast_io::manipulators::floating_precision::significant, rounding_policy>,
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

template <::fast_io::manipulators::floating_rounding rounding_policy, bool noskipws = false,
		  bool allow_leading_plus = false, ::fast_io::details::my_floating_point scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::floating_precision_rounding_mani_flags_cache<
									::fast_io::manipulators::scalar_flags{
										.showbase = true,
										.noskipws = noskipws,
										.floating = ::fast_io::manipulators::floating_format::hexfloat,
										.allow_leading_plus = allow_leading_plus},
									::fast_io::manipulators::floating_precision::significant, rounding_policy>,
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

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point T>
	requires(flags.floating == ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr auto
scan_context_type(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	using trait = ::fast_io::details::iec559_traits<T>;
	constexpr ::std::size_t precision_bits{trait::mbits + 1u};
	using storage_type = ::fast_io::details::scan_hexfloat_storage_t<precision_bits>;
	return io_type_t<::fast_io::details::scan_hexfloat_context<char_type, storage_type>>{};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point T>
	requires(flags.floating == ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_context_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
					typename ::std::remove_cvref_t<decltype(scan_context_type(
						io_reserve_type<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>))>::type &state,
					char_type const *begin,
					char_type const *end,
					::fast_io::manipulators::scalar_manip_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_hexfloat_context_numeric_define<char_type, flags, T>(
		state, begin, end, value.reference);
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point T>
	requires(flags.floating == ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_code
scan_context_eof_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
						typename ::std::remove_cvref_t<decltype(scan_context_type(
							io_reserve_type<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>))>::type &state,
						::fast_io::manipulators::scalar_manip_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_hexfloat_context_eof<char_type, flags, T>(state, value.reference);
}

} // namespace fast_io

#pragma once
#include "sto_generate_base_tb.h"

namespace fast_io
{

namespace details
{

inline constexpr auto generate_sto_ascii_digit_table() noexcept
{
	::fast_io::freestanding::array<char8_t, 256> table;
	for (auto &e : table)
	{
		e = static_cast<char8_t>(0xFFu);
	}
	for (::std::size_t i{}; i != 10u; ++i)
	{
		table.index_unchecked(static_cast<::std::size_t>(u8'0') + i) = static_cast<char8_t>(i);
	}
	for (::std::size_t i{}; i != 26u; ++i)
	{
		auto const digit{static_cast<char8_t>(10u + i)};
		table.index_unchecked(static_cast<::std::size_t>(u8'A') + i) = digit;
		table.index_unchecked(static_cast<::std::size_t>(u8'a') + i) = digit;
	}
	return table;
}

inline constexpr auto sto_ascii_digit_table{::fast_io::details::generate_sto_ascii_digit_table()};

template <::std::integral char_type>
	requires(::fast_io::details::is_ascii<char_type>)
inline constexpr char8_t sto_ascii_digit_table_lookup(my_make_unsigned_t<char_type> ch) noexcept
{
	if constexpr (sizeof(char_type) != sizeof(char8_t))
	{
		if (static_cast<::std::size_t>(ch) >=
			::fast_io::details::sto_ascii_digit_table.size()) [[unlikely]]
		{
			return static_cast<char8_t>(0xFFu);
		}
	}
	return ::fast_io::details::sto_ascii_digit_table.index_unchecked(static_cast<::std::size_t>(ch));
}

template <char8_t base, ::std::integral char_type>
	requires(2 <= base && base <= 36 &&
			 !::fast_io::details::is_ascii<char_type> &&
			 !::fast_io::details::is_classic_ebcdic<char_type>)
inline constexpr char8_t sto_other_execution_digit_lookup(
	my_make_unsigned_t<char_type> ch) noexcept
{
	using unsigned_char_type = my_make_unsigned_t<char_type>;
	for (char8_t digit{}; digit != base; ++digit)
	{
		char8_t const lower_literal{digit < 10u
								 ? static_cast<char8_t>(u8'0' + digit)
								 : static_cast<char8_t>(u8'a' + (digit - 10u))};
		if (ch == static_cast<unsigned_char_type>(
				  ::fast_io::arithmetic_char_literal<char_type>(lower_literal)))
		{
			return digit;
		}
		if (10u <= digit)
		{
			char8_t const upper_literal{
				static_cast<char8_t>(u8'A' + (digit - 10u))};
			if (ch == static_cast<unsigned_char_type>(
						  ::fast_io::arithmetic_char_literal<char_type>(upper_literal)))
			{
				return digit;
			}
		}
	}
	return static_cast<char8_t>(0xffu);
}

template <char8_t base, ::std::integral char_type>
	requires(2 <= base && base <= 36)
inline constexpr bool char_digit_to_literal(my_make_unsigned_t<char_type> &ch) noexcept
{
	using unsigned_char_type = my_make_unsigned_t<char_type>;
	constexpr bool ebcdic{::fast_io::details::is_ebcdic<char_type>};
	constexpr bool classic_ebcdic{::fast_io::details::is_classic_ebcdic<char_type>};
	constexpr bool ascii{::fast_io::details::is_ascii<char_type>};
	if constexpr (::std::same_as<char_type, wchar_t> &&
				  ::fast_io::details::wide_is_none_execution_endian)
	{
		ch = ::fast_io::byte_swap(static_cast<unsigned_char_type>(ch));
	}
	if constexpr (base <= 10)
	{
		constexpr unsigned_char_type base_char_type(base);
		if constexpr (ebcdic)
		{
			ch -= static_cast<unsigned_char_type>(240);
		}
		else if constexpr (ascii)
		{
			ch -= static_cast<unsigned_char_type>(u8'0');
		}
		else
		{
			auto const digit{
				::fast_io::details::sto_other_execution_digit_lookup<base, char_type>(ch)};
			ch = static_cast<unsigned_char_type>(digit);
			return base <= digit;
		}
		return base_char_type <= ch;
	}
	else
	{
		if constexpr (classic_ebcdic)
		{
			if constexpr (base <= 19)
			{
				constexpr unsigned_char_type mns{base - 10};
				unsigned_char_type ch2(ch);
				ch2 -= 0xC1;
				unsigned_char_type ch3(ch);
				ch3 -= 0x81;
				ch -= 0xF0;
				if (ch2 < mns)
				{
					ch = ch2 + static_cast<unsigned_char_type>(10);
				}
				else if (ch3 < mns)
				{
					ch = ch3 + static_cast<unsigned_char_type>(10);
				}
				else if (10 <= ch)
				{
					return true;
				}
				return false;
			}
			else if constexpr (base <= 28)
			{
				constexpr unsigned_char_type mns{base - 19};
				unsigned_char_type ch2(ch);
				ch2 -= 0xC1;
				unsigned_char_type ch3(ch);
				ch3 -= 0x81;
				unsigned_char_type ch4(ch);
				ch4 -= 0xD1;
				unsigned_char_type ch5(ch);
				ch5 -= 0x91;
				ch -= 0xF0;
				if (ch4 < mns)
				{
					ch = ch4 + static_cast<unsigned_char_type>(19);
				}
				else if (ch5 < mns)
				{
					ch = ch5 + static_cast<unsigned_char_type>(19);
				}
				else if (ch2 < 9)
				{
					ch = ch2 + static_cast<unsigned_char_type>(10);
				}
				else if (ch3 < 9)
				{
					ch = ch3 + static_cast<unsigned_char_type>(10);
				}
				else if (10 <= ch)
				{
					return true;
				}
				return false;
			}
			else
			{
				constexpr unsigned_char_type mns{base - 28};
				unsigned_char_type ch2(ch);
				ch2 -= 0xC1;
				unsigned_char_type ch3(ch);
				ch3 -= 0x81;
				unsigned_char_type ch4(ch);
				ch4 -= 0xD1;
				unsigned_char_type ch5(ch);
				ch5 -= 0x91;
				unsigned_char_type ch6(ch);
				ch6 -= 0xE2;
				unsigned_char_type ch7(ch);
				ch7 -= 0xA2;
				ch -= 0xF0;
				if (ch6 < mns)
				{
					ch = ch6 + static_cast<unsigned_char_type>(28);
				}
				else if (ch7 < mns)
				{
					ch = ch7 + static_cast<unsigned_char_type>(28);
				}
				else if (ch4 < 9)
				{
					ch = ch4 + static_cast<unsigned_char_type>(19);
				}
				else if (ch5 < 9)
				{
					ch = ch5 + static_cast<unsigned_char_type>(19);
				}
				else if (ch2 < 9)
				{
					ch = ch2 + static_cast<unsigned_char_type>(10);
				}
				else if (ch3 < 9)
				{
					ch = ch3 + static_cast<unsigned_char_type>(10);
				}
				else if (10 <= ch)
				{
					return true;
				}
				return false;
			}
		}
		else if constexpr (ascii)
		{
			auto const digit{::fast_io::details::sto_ascii_digit_table_lookup<char_type>(ch)};
			ch = static_cast<unsigned_char_type>(digit);
			return base <= digit;
		}
		else
		{
			auto const digit{
				::fast_io::details::sto_other_execution_digit_lookup<base, char_type>(ch)};
			ch = static_cast<unsigned_char_type>(digit);
			return base <= digit;
		}
	}
}

template <char8_t base, ::std::integral char_type>
	requires(2 <= base && base <= 36)
inline constexpr bool char_is_digit(my_make_unsigned_t<char_type> ch) noexcept
{
	using unsigned_char_type = my_make_unsigned_t<char_type>;
	constexpr bool ebcdic{::fast_io::details::is_ebcdic<char_type>};
	constexpr bool classic_ebcdic{::fast_io::details::is_classic_ebcdic<char_type>};
	constexpr bool ascii{::fast_io::details::is_ascii<char_type>};
	constexpr unsigned_char_type base_char_type(base);
	if constexpr (::std::same_as<char_type, wchar_t> &&
				  ::fast_io::details::wide_is_none_execution_endian)
	{
		ch = ::fast_io::byte_swap(static_cast<unsigned_char_type>(ch));
	}
	if constexpr (base <= 10)
	{
		if constexpr (ebcdic)
		{
			ch -= static_cast<unsigned_char_type>(240);
		}
		else if constexpr (ascii)
		{
			ch -= static_cast<unsigned_char_type>(u8'0');
		}
		else
		{
			return ::fast_io::details::sto_other_execution_digit_lookup<base, char_type>(ch) < base;
		}
		return ch < base_char_type;
	}
	else
	{
		if constexpr (classic_ebcdic)
		{
			if constexpr (base <= 19)
			{
				constexpr unsigned_char_type mns{base - 10};
				unsigned_char_type ch2(ch);
				ch2 -= 0xC1;
				unsigned_char_type ch3(ch);
				ch3 -= 0x81;
				ch -= 0xF0;
				return (ch2 < mns) | (ch3 < mns) | (ch < 10u);
			}
			else if constexpr (base <= 28)
			{
				constexpr unsigned_char_type mns{base - 19};
				unsigned_char_type ch2(ch);
				ch2 -= 0xC1;
				unsigned_char_type ch3(ch);
				ch3 -= 0x81;
				unsigned_char_type ch4(ch);
				ch4 -= 0xD1;
				unsigned_char_type ch5(ch);
				ch5 -= 0x91;
				ch -= 0xF0;
				return (ch4 < mns) | (ch5 < mns) | (ch2 < 9u) | (ch3 < 9u) | (ch < 10u);
			}
			else
			{
				constexpr unsigned_char_type mns{base - 28};
				unsigned_char_type ch2(ch);
				ch2 -= 0xC1;
				unsigned_char_type ch3(ch);
				ch3 -= 0x81;
				unsigned_char_type ch4(ch);
				ch4 -= 0xD1;
				unsigned_char_type ch5(ch);
				ch5 -= 0x91;
				unsigned_char_type ch6(ch);
				ch6 -= 0xE2;
				unsigned_char_type ch7(ch);
				ch7 -= 0xA2;
				ch -= 0xF0;
				return (ch6 < mns) | (ch7 < mns) | (ch4 < 9u) | (ch5 < 9u) | (ch2 < 9u) | (ch3 < 9u) | (ch < 10u);
			}
		}
		else if constexpr (ascii)
		{
			return ::fast_io::details::sto_ascii_digit_table_lookup<char_type>(ch) < base;
		}
		else
		{
			return ::fast_io::details::sto_other_execution_digit_lookup<base, char_type>(ch) < base;
		}
	}
}

template <::std::integral char_type>
inline constexpr char_type const *find_none_zero_simd_impl(char_type const *first, char_type const *last) noexcept;

struct simd_parse_result
{
	::std::size_t digits;
	fast_io::parse_code code;
};

inline constexpr char unsigned simd16_shift_table[32]{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
													  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 1, 2, 3, 4, 5,
													  6, 7, 8, 9, 10, 11, 12, 13, 14, 15};

/*
The decimal SIMD scanner is compiled only for native x86-64 with SSE4.1.
Its movemask proves a valid digit prefix; multiply-add stages then compute
10*d0+d1 and 100*p0+p1.  Because each proved digit is in [0,9], the packed
stages cannot saturate.  ARM64EC is outside the native x86 builtin contract.
GNU-family configurations other than those defining the legacy
__INTEL_COMPILER macro use raw __builtin_ia32_* vector operations.  The
fallback branch uses the equivalent x86-intrinsic spelling; both implement the
same lane algebra.
Native x86 timing and Haswell-through-Zen4 llvm-mca support the retained
family, with llvm-mca understood only as static scheduling evidence.  The
retained native reports use an i9-14900HX and whole 665-point matrices rather
than an isolated 32-byte decimal A/B, so they do not establish a universal
threshold.  Other SSE4.1 cores and frontends inherit only the lane proof.  The
32-byte entry remains a conservative specialized-kernel policy pending an
isolated native revalidation; shorter, constexpr, non-SSE, and non-native-x86
inputs use scan_int_contiguous_none_simd_space_part_define_impl instead.
*/
#if defined(__SSE4_1__) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)))

template <bool char_execharset>
inline ::std::uint_least32_t detect_length(char unsigned const *buffer) noexcept
{
	constexpr char8_t zero_constant{char_execharset ? static_cast<char8_t>('0') : u8'0'};
	constexpr char8_t v176_constant{static_cast<char8_t>((zero_constant + static_cast<char8_t>(128)) & 255u)};
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	using namespace fast_io::intrinsics;
	x86_64_v16qu chunk;
	__builtin_memcpy(__builtin_addressof(chunk), buffer, 16);
	x86_64_v16qu const v176{v176_constant, v176_constant, v176_constant, v176_constant, v176_constant, v176_constant,
							v176_constant, v176_constant, v176_constant, v176_constant, v176_constant, v176_constant,
							v176_constant, v176_constant, v176_constant, v176_constant};
	x86_64_v16qu const t0{chunk - v176};
	x86_64_v16qs const minus118{-118, -118, -118, -118, -118, -118, -118, -118,
								-118, -118, -118, -118, -118, -118, -118, -118};
	x86_64_v16qs const mask{(x86_64_v16qs)t0 < minus118};
	::std::uint_least16_t v{static_cast<::std::uint_least16_t>(__builtin_ia32_pmovmskb128((x86_64_v16qi)mask))};
#else
	__m128i chunk = _mm_loadu_si128(reinterpret_cast<__m128i const *>(buffer));
	__m128i const t0 = _mm_sub_epi8(chunk, _mm_set1_epi8(v176_constant));
	__m128i const mask = _mm_cmplt_epi8(t0, _mm_set1_epi8(-118));
	::std::uint_least16_t v{static_cast<::std::uint_least16_t>(_mm_movemask_epi8(mask))};
#endif
	return static_cast<::std::uint_least32_t>(::std::countr_one(v));
}

template <bool char_execharset>
	// Overflow digit skipping is an error-only continuation.  Marking it cold
	// changes section/outlining policy, never the digits consumed.  No retained
	// isolated A/B selects the attribute; implementations without it use ordinary
	// placement as the semantic fallback, and no unmeasured-frontend gain is claimed.
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline ::std::size_t sse_skip_overflow_digits(char unsigned const *buffer,
											  char unsigned const *buffer_end) noexcept
{
	auto it{buffer};
	for (; 16 <= buffer_end - it; it += 16)
	{
		auto new_length{detect_length<char_execharset>(it)};
		if (new_length != 16)
		{
			return static_cast<::std::size_t>(it - buffer + new_length);
		}
	};
	constexpr char8_t zero_constant{char_execharset ? static_cast<char8_t>('0') : u8'0'};
	for (; it != buffer_end && static_cast<char unsigned>(*it - zero_constant) < 10u; ++it)
	{
	}
	return static_cast<::std::size_t>(it - buffer);
}

template <bool char_execharset, bool less_than_64_bits>
	// This is the normal full-block decimal entry, so the hot attribute is the
	// complement of the overflow-only helper above.  It changes placement only;
	// the unsupported-attribute fallback is the same ordinary inline function.
	// The policy remains conservative pending an isolated native layout A/B.
#if __has_cpp_attribute(__gnu__::__hot__)
[[__gnu__::__hot__]]
#endif
inline simd_parse_result sse_parse(char unsigned const *buffer, char unsigned const *buffer_end,
								   ::std::uint_least64_t &res) noexcept
{
	constexpr char8_t zero_constant{char_execharset ? static_cast<char8_t>('0') : u8'0'};
	constexpr char8_t v176_constant{static_cast<char8_t>((zero_constant + static_cast<char8_t>(128)) & 255u)};
	using fast_io::parse_code;
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	using namespace fast_io::intrinsics;
	x86_64_v16qu chunk;
	__builtin_memcpy(__builtin_addressof(chunk), buffer, 16);
	x86_64_v16qu const v176{v176_constant, v176_constant, v176_constant, v176_constant, v176_constant, v176_constant,
							v176_constant, v176_constant, v176_constant, v176_constant, v176_constant, v176_constant,
							v176_constant, v176_constant, v176_constant, v176_constant};
	x86_64_v16qu const t0{chunk - v176};
	x86_64_v16qs const minus118{-118, -118, -118, -118, -118, -118, -118, -118,
								-118, -118, -118, -118, -118, -118, -118, -118};
	x86_64_v16qs const mask{(x86_64_v16qs)t0 < minus118};
	::std::uint_least16_t v{static_cast<::std::uint_least16_t>(__builtin_ia32_pmovmskb128((x86_64_v16qi)mask))};
	::std::uint_least32_t digits{static_cast<::std::uint_least32_t>(::std::countr_one(v))};
	if (digits == 0)
	{
		return {0, parse_code::invalid};
	}
	x86_64_v16qu const zeros{zero_constant, zero_constant, zero_constant, zero_constant, zero_constant, zero_constant,
							 zero_constant, zero_constant, zero_constant, zero_constant, zero_constant, zero_constant,
							 zero_constant, zero_constant, zero_constant, zero_constant};
	chunk -= zeros;
	// A full 16-digit chunk already has the required byte order. Avoid the
	// shuffle-table load and PSHUFB on the long-decimal hot path.
	if (digits != 16u) [[unlikely]]
	{
		x86_64_v16qi shuffle_mask;
		__builtin_memcpy(__builtin_addressof(shuffle_mask), simd16_shift_table + digits, sizeof(x86_64_v16qi));
		chunk = (x86_64_v16qu)__builtin_ia32_pshufb128((x86_64_v16qi)chunk, shuffle_mask);
	}
	chunk = (x86_64_v16qu)__builtin_ia32_pmaddubsw128(
		(x86_64_v16qi)chunk, x86_64_v16qi{10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1});
	chunk = (x86_64_v16qu)__builtin_ia32_pmaddwd128((x86_64_v8hi)chunk, x86_64_v8hi{100, 1, 100, 1, 100, 1, 100, 1});
	chunk = (x86_64_v16qu)__builtin_ia32_packusdw128((x86_64_v4si)chunk, (x86_64_v4si)chunk);
	chunk = (x86_64_v16qu)__builtin_ia32_pmaddwd128((x86_64_v8hi)chunk, x86_64_v8hi{10000, 1, 10000, 1, 0, 0, 0, 0});
	::std::uint_least64_t chunk0;
	__builtin_memcpy(__builtin_addressof(chunk0), __builtin_addressof(chunk), sizeof(chunk0));
#else
	__m128i chunk = _mm_loadu_si128(reinterpret_cast<__m128i const *>(buffer));
	__m128i const t0 = _mm_sub_epi8(chunk, _mm_set1_epi8(v176_constant));
	__m128i const mask = _mm_cmplt_epi8(t0, _mm_set1_epi8(-118));
	::std::uint_least16_t v{static_cast<::std::uint_least16_t>(_mm_movemask_epi8(mask))};
	::std::uint_least32_t digits{static_cast<::std::uint_least32_t>(::std::countr_one(v))};
	if (digits == 0)
	{
		return {0, parse_code::invalid};
	}
	chunk = _mm_sub_epi8(chunk, _mm_set1_epi8(zero_constant));
	// A full 16-digit chunk already has the required byte order. Avoid the
	// shuffle-table load and PSHUFB on the long-decimal hot path.
	if (digits != 16u) [[unlikely]]
	{
		chunk = _mm_shuffle_epi8(chunk, _mm_loadu_si128(reinterpret_cast<__m128i const *>(simd16_shift_table + digits)));
	}
	chunk = _mm_maddubs_epi16(chunk, _mm_set_epi8(1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10, 1, 10));
	chunk = _mm_madd_epi16(chunk, _mm_set_epi16(1, 100, 1, 100, 1, 100, 1, 100));
	chunk = _mm_packus_epi32(chunk, chunk);
	chunk = _mm_madd_epi16(chunk, _mm_set_epi16(0, 0, 0, 0, 1, 10000, 1, 10000));
	::std::uint_least64_t chunk0;
	::std::memcpy(__builtin_addressof(chunk0), __builtin_addressof(chunk), sizeof(chunk0));
#endif
	::std::uint_least64_t result{
		static_cast<::std::uint_least64_t>(((chunk0 & 0xffffffff) * static_cast<::std::uint_least64_t>(100000000)) + (chunk0 >> 32))};
	if (digits == 16) [[unlikely]]
	{
		if constexpr (less_than_64_bits)
		{
			//::std::uint_least32_t can never have 16 digits
			return {sse_skip_overflow_digits<char_execharset>(buffer + 16, buffer_end) + 16,
					parse_code::overflow};
		}
		else
		{
			::std::size_t digits1;
			if (16 <= buffer_end - (buffer + 16))
			{
				digits1 = detect_length<char_execharset>(buffer + 16);
			}
			else
			{
				digits1 = sse_skip_overflow_digits<char_execharset>(buffer + 16, buffer_end);
			}
			// 18446744073709551615 20 digits
			switch (digits1)
			{
			case 3:
			{
				res = result * static_cast<::std::uint_least16_t>(1000) +
					  ((buffer[16] - zero_constant) * static_cast<::std::uint_least16_t>(100) + (buffer[17] - zero_constant) * static_cast<::std::uint_least16_t>(10) +
					   (buffer[18] - zero_constant));
				return {19, parse_code::ok};
			}
			case 2:
			{
				res = result * static_cast<::std::uint_least16_t>(100) + ((buffer[16] - zero_constant) * static_cast<::std::uint_least16_t>(10) +
																		  static_cast<::std::uint_least64_t>(buffer[17] - zero_constant));
				return {18, parse_code::ok};
			}
			case 1:
			{
				res = result * static_cast<::std::uint_least16_t>(10) + (buffer[16] - zero_constant);
				return {17, parse_code::ok};
			}
			case 0:
			{
				res = result;
				return {16, parse_code::ok};
			}
			case 4:
			{
				constexpr ::std::uint_least64_t risky_value{UINT_LEAST64_MAX / static_cast<::std::uint_least64_t>(10000)};
				constexpr ::std::uint_fast16_t risky_mod{UINT_LEAST64_MAX % static_cast<::std::uint_least64_t>(10000)};
				if (result > risky_value)
				{
					return {20, parse_code::overflow};
				}
				::std::uint_fast16_t partial{static_cast<::std::uint_fast16_t>(
					static_cast<::std::uint_fast16_t>(buffer[16] - zero_constant) * static_cast<::std::uint_least16_t>(1000) +
					static_cast<::std::uint_fast8_t>(buffer[17] - zero_constant) * static_cast<::std::uint_least16_t>(100) +
					static_cast<::std::uint_fast8_t>(buffer[18] - zero_constant) * static_cast<::std::uint_least16_t>(10) +
					static_cast<::std::uint_fast8_t>(buffer[19] - zero_constant))};
				if (result == risky_value && risky_mod < partial)
				{
					return {20, parse_code::overflow};
				}
				res = result * static_cast<::std::uint_least16_t>(10000) + partial;
				return {20, parse_code::ok};
			}
			case 16:
			{
				digits1 = sse_skip_overflow_digits<char_execharset>(buffer + 16, buffer_end);
				[[fallthrough]];
			}
			default:
			{
				return {digits1 + 16, parse_code::overflow};
			}
			}
		}
	}
	res = result;
	return {digits, parse_code::ok};
}

/*
Parse one terminal decimal suffix shorter than a full SSE block.  The caller
proves that 16 bytes are physically readable, while semantic_size remains the
only lexical boundary.  Capping the movemask prefix at semantic_size makes the
result independent of every padding byte.
*/
inline simd_parse_result
sse_parse_terminal_padding_tail(char unsigned const *buffer,
								::std::size_t semantic_size,
								::std::uint_least64_t &res) noexcept
{
	auto digits{
		static_cast<::std::size_t>(detect_length<false>(buffer))};
	if (semantic_size < digits)
	{
		digits = semantic_size;
	}
	if (digits == 0u)
	{
		return {0u, parse_code::invalid};
	}
#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	using namespace fast_io::intrinsics;
	x86_64_v16qu chunk;
	__builtin_memcpy(__builtin_addressof(chunk), buffer, 16);
	x86_64_v16qu const zeros{
		u8'0', u8'0', u8'0', u8'0', u8'0', u8'0', u8'0', u8'0',
		u8'0', u8'0', u8'0', u8'0', u8'0', u8'0', u8'0', u8'0'};
	chunk -= zeros;
	x86_64_v16qi shuffle_mask;
	__builtin_memcpy(
		__builtin_addressof(shuffle_mask), simd16_shift_table + digits,
		sizeof(x86_64_v16qi));
	chunk = (x86_64_v16qu)__builtin_ia32_pshufb128(
		(x86_64_v16qi)chunk, shuffle_mask);
	chunk = (x86_64_v16qu)__builtin_ia32_pmaddubsw128(
		(x86_64_v16qi)chunk,
		x86_64_v16qi{10, 1, 10, 1, 10, 1, 10, 1,
					 10, 1, 10, 1, 10, 1, 10, 1});
	chunk = (x86_64_v16qu)__builtin_ia32_pmaddwd128(
		(x86_64_v8hi)chunk,
		x86_64_v8hi{100, 1, 100, 1, 100, 1, 100, 1});
	chunk = (x86_64_v16qu)__builtin_ia32_packusdw128(
		(x86_64_v4si)chunk, (x86_64_v4si)chunk);
	chunk = (x86_64_v16qu)__builtin_ia32_pmaddwd128(
		(x86_64_v8hi)chunk,
		x86_64_v8hi{10000, 1, 10000, 1, 0, 0, 0, 0});
	::std::uint_least64_t chunk0;
	__builtin_memcpy(
		__builtin_addressof(chunk0), __builtin_addressof(chunk),
		sizeof(chunk0));
#else
	__m128i chunk =
		_mm_loadu_si128(reinterpret_cast<__m128i const *>(buffer));
	chunk = _mm_sub_epi8(chunk, _mm_set1_epi8('0'));
	chunk = _mm_shuffle_epi8(
		chunk, _mm_loadu_si128(reinterpret_cast<__m128i const *>(
				   simd16_shift_table + digits)));
	chunk = _mm_maddubs_epi16(
		chunk, _mm_set_epi8(
				   1, 10, 1, 10, 1, 10, 1, 10,
				   1, 10, 1, 10, 1, 10, 1, 10));
	chunk = _mm_madd_epi16(
		chunk, _mm_set_epi16(1, 100, 1, 100, 1, 100, 1, 100));
	chunk = _mm_packus_epi32(chunk, chunk);
	chunk = _mm_madd_epi16(
		chunk, _mm_set_epi16(0, 0, 0, 0, 1, 10000, 1, 10000));
	::std::uint_least64_t chunk0;
	::std::memcpy(
		__builtin_addressof(chunk0), __builtin_addressof(chunk),
		sizeof(chunk0));
#endif
	res = static_cast<::std::uint_least64_t>(
		((chunk0 & 0xffffffffu) *
		 static_cast<::std::uint_least64_t>(100000000u)) +
		(chunk0 >> 32u));
	return {digits, parse_code::ok};
}

#endif

template <char8_t base, ::std::integral char_type>
	// skip_digits is reached after overflow or a caller-proved continuation.  The
	// cold request affects layout only and an unsupported frontend executes the
	// identical loop with ordinary placement.  No isolated native result is
	// retained, so this legacy hint makes no performance claim.
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr char_type const *skip_digits(char_type const *first, char_type const *last) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	for (; first != last && char_is_digit<base, char_type>(static_cast<unsigned_char_type>(*first)); ++first)
		;
	return first;
}

template <char8_t base, ::std::integral char_type, my_unsigned_integral T>
	// The overflow checker is a thin continuation shared by scalar/SWAR entries.
	// Force-inlining changes only its call boundary; the ordinary-inline fallback
	// performs the same recurrence and pointer scan.  This is a conservative
	// legacy layout policy pending native compiler revalidation, with no numeric
	// claim for an unmeasured frontend.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr parse_result<char_type const *>
scan_int_contiguous_none_simd_space_part_check_overflow_impl(char_type const *first, char_type const *last, T &res) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	constexpr unsigned_char_type base_char_type{base};
	constexpr unsigned_type risky_uint_max{static_cast<unsigned_type>(-1)};
	constexpr unsigned_type risky_value{risky_uint_max / base};
	constexpr unsigned_char_type risky_digit(risky_uint_max % base);
	constexpr bool isspecialbase{base == 2 || base == 4 || base == 16};

	bool overflow{};
	if (first != last) [[likely]]
	{
		unsigned_char_type ch{static_cast<unsigned_char_type>(*first)};
		if constexpr (isspecialbase)
		{
			if (char_is_digit<base, char_type>(ch))
			{
				++first;
				first = skip_digits<base>(first, last);
				overflow = true;
			}
		}
		else
		{
			if (!char_digit_to_literal<base, char_type>(ch)) [[unlikely]]
			{
				overflow = res > risky_value || (risky_value == res && ch > risky_digit);
				if (!overflow)
				{
					res *= base_char_type;
					res += ch;
				}
				++first;
				if (first != last && char_is_digit<base, char_type>(static_cast<unsigned_char_type>(*first)))
				{
					++first;
					first = skip_digits<base>(first, last);
					overflow = true;
				}
			}
		}
	}
	return {first, (overflow ? (parse_code::overflow) : (parse_code::ok))};
}

template <char8_t base, my_unsigned_integral T, ::std::size_t n>
inline constexpr ::fast_io::freestanding::array<T, n> generate_pow_table() noexcept
{
	::fast_io::freestanding::array<T, n> tmp;
	T b{1};
	for (auto &e : tmp)
	{
		e = b;
		b *= base;
	}
	return tmp;
}

template <char8_t base, my_unsigned_integral T, ::std::size_t n>
inline constexpr ::fast_io::freestanding::array<T, n> pow_table_n{::fast_io::details::generate_pow_table<base, T, n>()};

template <::std::integral char_type>
	requires(::fast_io::details::is_ascii<char_type> && sizeof(char_type) == sizeof(char8_t))
inline constexpr char8_t ascii_hex_digit_value(my_make_unsigned_t<char_type> ch) noexcept
{
	// Scalar tails contain an unpredictable mix of decimal and alphabetic
	// digits. The shared table avoids a data-dependent branch for that mix.
	return ::fast_io::details::sto_ascii_digit_table_lookup<char_type>(ch);
}

inline constexpr ::std::uint_least64_t ascii_hex_word_invalid_mask(::std::uint_least64_t val) noexcept
{
	return (((((val + static_cast<::std::uint_least64_t>(0x4646464646464646)) | (val - static_cast<::std::uint_least64_t>(0x3030303030303030))) &
			  ((val + static_cast<::std::uint_least64_t>(0x3939393939393939)) | (val - static_cast<::std::uint_least64_t>(0x4040404040404040))) &
			  ((val + static_cast<::std::uint_least64_t>(0x1919191919191919)) | (val - static_cast<::std::uint_least64_t>(0x6060606060606060)))) |
			 ~(((val + static_cast<::std::uint_least64_t>(0x3f3f3f3f3f3f3f3f)) | (val - static_cast<::std::uint_least64_t>(0x4040404040404040))) &
			   ((val + static_cast<::std::uint_least64_t>(0x1f1f1f1f1f1f1f1f)) | (val - static_cast<::std::uint_least64_t>(0x6060606060606060))))) &
			static_cast<::std::uint_least64_t>(0x8080808080808080));
}

inline constexpr ::std::uint_least32_t ascii_hex_word_to_u32(::std::uint_least64_t val) noexcept
{
	constexpr ::std::uint_least64_t mask{static_cast<::std::uint_least64_t>(0x000000FF000000FF)};
	constexpr ::std::uint_least64_t mul1{static_cast<::std::uint_least64_t>(0x0100000000000100)};
	constexpr ::std::uint_least64_t mul2{static_cast<::std::uint_least64_t>(0x0001000000000001)};
	val -= static_cast<::std::uint_least64_t>(0x3030303030303030);
	val = (val & static_cast<::std::uint_least64_t>(0x0f0f0f0f0f0f0f0f)) + ((val & static_cast<::std::uint_least64_t>(0x1010101010101010)) >> 4u) * 9u;
	val = (val * 16u) + (val >> 8u);
	return static_cast<::std::uint_least32_t>((((val & mask) * mul1) + (((val >> 16u) & mask) * mul2)) >> 32u);
}

template <::std::integral char_type, my_unsigned_integral T>
	requires(::fast_io::details::is_ascii<char_type> && sizeof(char_type) == sizeof(char8_t))
inline constexpr char_type const *scan_ascii_hex_digits_scalar(char_type const *first, char_type const *last,
															   T &res) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	for (; first != last; ++first)
	{
		auto const digit{
			::fast_io::details::ascii_hex_digit_value<char_type>(static_cast<unsigned_char_type>(*first))};
		if (15u < digit) [[unlikely]]
		{
			break;
		}
		res = static_cast<T>((static_cast<unsigned_type>(res) << 4u) | static_cast<unsigned_type>(digit));
	}
	return first;
}

template <::std::integral char_type, my_unsigned_integral T>
	// This wrapper selects the same bounded hexadecimal core with or without a
	// force-inline attribute.  Attribute-disabled compilers retain ordinary inline
	// semantics; no retained isolated wrapper A/B exists, so this is a conservative
	// call-boundary policy rather than a cross-compiler throughput claim.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr parse_result<char_type const *>
scan_int_contiguous_ascii_hex_space_part_define_impl(char_type const *first, char_type const *last, T &out) noexcept
{
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	constexpr ::std::size_t max_size{::fast_io::details::max_int_size_result<unsigned_type, 16>};
	::std::size_t const diff{static_cast<::std::size_t>(last - first)};
	::std::size_t mn_val{max_size};
	if (diff < mn_val)
	{
		mn_val = diff;
	}
	auto first_phase_last{first + mn_val};
	T res{out};
	if constexpr (::std::numeric_limits<::std::uint_least64_t>::digits == 64u && 8u <= max_size)
	{
		while (static_cast<::std::size_t>(first_phase_last - first) >= sizeof(::std::uint_least64_t)) [[likely]]
		{
			::std::uint_least64_t val;
			::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));
			// Canonical little-endian lane order places the first input byte in the
			// low byte.  This is required both by countr_zero's first-invalid-byte
			// result and by the subsequent hexadecimal polynomial reduction.
			if constexpr (::std::endian::little != ::std::endian::native)
			{
				val = ::fast_io::little_endian(val);
			}
			if (::std::uint_least64_t const invalid_mask{::fast_io::details::ascii_hex_word_invalid_mask(val)};
				invalid_mask != 0) [[unlikely]]
			{
				auto const valid_bytes{
					static_cast<::std::size_t>(static_cast<unsigned>(::std::countr_zero(invalid_mask)) >> 3u)};
				first = ::fast_io::details::scan_ascii_hex_digits_scalar(first, first + valid_bytes, res);
				first_phase_last = first;
				break;
			}
			auto const chunk{::fast_io::details::ascii_hex_word_to_u32(val)};
			if constexpr (sizeof(unsigned_type) <= sizeof(::std::uint_least32_t))
			{
				res = static_cast<T>(chunk);
			}
			else
			{
				res = static_cast<T>((static_cast<unsigned_type>(res) << 32u) |
									 static_cast<unsigned_type>(chunk));
			}
			first += sizeof(::std::uint_least64_t);
		}
	}
	first = ::fast_io::details::scan_ascii_hex_digits_scalar(first, first_phase_last, res);

	if (first == last)
	{
		out = res;
		return {first, parse_code::ok};
	}
	auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<16, char_type, T>(first, last, res)};
	out = res;
	return ret;
}

/*
Parse exactly sixteen decimal bytes with AArch64 AdvSIMD.  Subtracting ASCII
zero and taking a horizontal maximum proves every lane is in [0,9].  Widening
pair, quad, and octet reductions compute base-10, base-100, and base-10000
groups without lane overflow; octets[0]*10^8+octets[1] is therefore the input.
Clang and supported non-Clang GNU-compatible frontends expose different raw
builtin names, so the inner compiler guards select syntax only and avoid
arm_neon.h.  The outer guard requires both a supported compiler and __ARM_NEON,
the feature macro that Apple Clang removes under -mgeneral-regs-only (its
compatibility macro __ARM_NEON__ remains set).
The one-byte, non-EBCDIC constraint is an internal ASCII precondition, not
merely a property of the current caller.
Native M4 timings favor this kernel; Cortex/Neoverse llvm-mca results are
static-only evidence.  Other AArch64 cores and GNU-compatible frontend
versions admitted by the feature gate inherit the proved arithmetic without a
native throughput claim; unsupported compilers and targets cannot form this
helper and stay on the scalar parser.
*/
#if (defined(__aarch64__) || defined(__arm64__)) &&                     \
	(defined(__clang__) || (defined(__GNUC__) && !defined(__clang__))) && \
	defined(__ARM_NEON)
template <::std::integral char_type>
	requires(sizeof(char_type) == sizeof(char8_t) &&
			 ::fast_io::details::is_ascii<char_type>)
[[gnu::always_inline]] inline bool
aarch64_builtin_parse_16_decimal_digits(char_type const *first,
										::std::uint_least64_t &value) noexcept
{
	using u8x8 [[gnu::vector_size(8)]] = unsigned char;
	using u8x16 [[gnu::vector_size(16)]] = unsigned char;
	using u16x4 [[gnu::vector_size(8)]] = unsigned short;
	using u32x2 [[gnu::vector_size(8)]] = unsigned int;
#if defined(__clang__)
	using i8x8 [[gnu::vector_size(8)]] = signed char;
	using i8x16 [[gnu::vector_size(16)]] = signed char;
	using u16x8 [[gnu::vector_size(16)]] = unsigned short;
	using u32x4 [[gnu::vector_size(16)]] = unsigned int;
	using u64x2 [[gnu::vector_size(16)]] = unsigned long long;
#endif

	u8x16 raw;
	// Both compiler arms perform the same unaligned sixteen-byte load.
#if defined(__clang__)
	raw = __builtin_bit_cast(
		u8x16, __builtin_neon_vld1q_v(reinterpret_cast<unsigned char const *>(first), 48));
#else
	raw = __builtin_aarch64_ld1v16qi_us(
		reinterpret_cast<__builtin_aarch64_simd_qi const *>(first));
#endif
	auto const digits{raw - u8x16{48u, 48u, 48u, 48u, 48u, 48u, 48u, 48u,
								  48u, 48u, 48u, 48u, 48u, 48u, 48u, 48u}};
	// Select only the compiler spelling of the same horizontal maximum.
#if defined(__clang__)
	auto const maximum_digit{static_cast<unsigned char>(__builtin_neon_vmaxvq_u8(digits))};
#else
	auto const maximum_digit{
		static_cast<unsigned char>(__builtin_aarch64_reduc_umax_scal_v16qi_uu(digits))};
#endif
	if (maximum_digit >= 10u) [[unlikely]]
	{
		return false;
	}

	u8x8 const low_digits{__builtin_shufflevector(digits, digits, 0, 1, 2, 3, 4, 5, 6, 7)};
	u8x8 const high_digits{__builtin_shufflevector(digits, digits, 8, 9, 10, 11, 12, 13, 14, 15)};
	u8x8 const pair_weights{10u, 1u, 10u, 1u, 10u, 1u, 10u, 1u};
	// Both arms compute the same adjacent 10*d0+d1 widening reduction.
#if defined(__clang__)
	auto const pair_products_low{__builtin_bit_cast(
		u16x8, __builtin_neon_vmull_v(__builtin_bit_cast(i8x8, low_digits),
									  __builtin_bit_cast(i8x8, pair_weights), 49))};
	auto const pair_products_high{__builtin_bit_cast(
		u16x8, __builtin_neon_vmull_v(__builtin_bit_cast(i8x8, high_digits),
									  __builtin_bit_cast(i8x8, pair_weights), 49))};
	auto const pairs{__builtin_bit_cast(
		u16x8, __builtin_neon_vpaddq_v(__builtin_bit_cast(i8x16, pair_products_low),
									   __builtin_bit_cast(i8x16, pair_products_high), 49))};
#else
	auto const pair_products_low{
		__builtin_aarch64_intrinsic_vec_umult_lo_v8qi_uuu(low_digits, pair_weights)};
	auto const pair_products_high{
		__builtin_aarch64_intrinsic_vec_umult_lo_v8qi_uuu(high_digits, pair_weights)};
	auto const pairs{
		__builtin_aarch64_addpv8hi_uuu(pair_products_low, pair_products_high)};
#endif

	u16x4 const low_pairs{__builtin_shufflevector(pairs, pairs, 0, 1, 2, 3)};
	u16x4 const high_pairs{__builtin_shufflevector(pairs, pairs, 4, 5, 6, 7)};
	u16x4 const quad_weights{100u, 1u, 100u, 1u};
	// Repeat the exact reduction in base 100.
#if defined(__clang__)
	auto const quad_products_low{__builtin_bit_cast(
		u32x4, __builtin_neon_vmull_v(__builtin_bit_cast(i8x8, low_pairs),
									  __builtin_bit_cast(i8x8, quad_weights), 50))};
	auto const quad_products_high{__builtin_bit_cast(
		u32x4, __builtin_neon_vmull_v(__builtin_bit_cast(i8x8, high_pairs),
									  __builtin_bit_cast(i8x8, quad_weights), 50))};
	auto const quads{__builtin_bit_cast(
		u32x4, __builtin_neon_vpaddq_v(__builtin_bit_cast(i8x16, quad_products_low),
									   __builtin_bit_cast(i8x16, quad_products_high), 50))};
#else
	auto const quad_products_low{
		__builtin_aarch64_intrinsic_vec_umult_lo_v4hi_uuu(low_pairs, quad_weights)};
	auto const quad_products_high{
		__builtin_aarch64_intrinsic_vec_umult_lo_v4hi_uuu(high_pairs, quad_weights)};
	auto const quads{
		__builtin_aarch64_addpv4si_uuu(quad_products_low, quad_products_high)};
#endif

	u32x2 const low_quads{__builtin_shufflevector(quads, quads, 0, 1)};
	u32x2 const high_quads{__builtin_shufflevector(quads, quads, 2, 3)};
	u32x2 const octet_weights{10000u, 1u};
	// The final compiler-spelling split forms two independent eight-digit values.
#if defined(__clang__)
	auto const octet_products_low{__builtin_bit_cast(
		u64x2, __builtin_neon_vmull_v(__builtin_bit_cast(i8x8, low_quads),
									  __builtin_bit_cast(i8x8, octet_weights), 51))};
	auto const octet_products_high{__builtin_bit_cast(
		u64x2, __builtin_neon_vmull_v(__builtin_bit_cast(i8x8, high_quads),
									  __builtin_bit_cast(i8x8, octet_weights), 51))};
	auto const octets{__builtin_bit_cast(
		u64x2, __builtin_neon_vpaddq_v(__builtin_bit_cast(i8x16, octet_products_low),
									   __builtin_bit_cast(i8x16, octet_products_high), 51))};
#else
	auto const octet_products_low{
		__builtin_aarch64_intrinsic_vec_umult_lo_v2si_uuu(low_quads, octet_weights)};
	auto const octet_products_high{
		__builtin_aarch64_intrinsic_vec_umult_lo_v2si_uuu(high_quads, octet_weights)};
	auto const octets{
		__builtin_aarch64_addpv2di_uuu(octet_products_low, octet_products_high)};
#endif
	value = octets[0] * 100000000u + octets[1];
	return true;
}
#endif

template <char8_t base, ::std::integral char_type, my_unsigned_integral T>
inline parse_result<char_type const *>
runtime_scan_int_contiguous_none_simd_space_part_define_impl(char_type const *first, char_type const *last, T &out) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	constexpr char8_t base_char_type{base};
	constexpr bool isspecialbase{base == 2 || base == 4 || base == 16};
	constexpr ::std::size_t max_size{::fast_io::details::max_int_size_result<unsigned_type, base> - (!isspecialbase)};
	constexpr auto shifter{2 + ::std::bit_width(sizeof(char_type))};
	::std::size_t const diff{static_cast<::std::size_t>(last - first)};
	::std::size_t mn_val{max_size};

	if (diff < mn_val)
	{
		mn_val = diff;
	}

	auto first_phase_last{first + mn_val};
	T res{out};

	/*
	All multi-byte scalar SWAR blocks below canonicalize the loaded word to
	little-endian lane order.  Consequently the first code unit occupies the low
	lane, countr_zero locates the earliest invalid input unit, and the documented
	multiply constants reduce digits in source order on either host endianness.
	EBCDIC and wide-code-unit predicates select separate constants or the scalar
	fallback; byte-order conversion never assumes an execution character set.
	*/
	constexpr bool isebcdic{::fast_io::details::is_ebcdic<char_type>};
	if constexpr (::fast_io::details::is_ascii<char_type> &&
				  (::std::numeric_limits<::std::uint_least64_t>::digits == 64u))
	{
		if constexpr (sizeof(::std::uint_least32_t) < sizeof(::std::size_t))
		{
			if constexpr (base_char_type <= 10)
			{
				if constexpr (sizeof(char_type) == sizeof(char8_t))
				{
					if constexpr (max_size >= sizeof(::std::uint_least64_t))
					{
						constexpr ::std::uint_least64_t pow_base_sizeof_base_2{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, 2>};
						constexpr ::std::uint_least64_t pow_base_sizeof_base_4{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, 4>};
						constexpr ::std::uint_least64_t pow_base_sizeof_base_6{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, 6>};
						constexpr ::std::uint_least64_t pow_base_sizeof_u64{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, sizeof(::std::uint_least64_t)>};


						constexpr ::std::uint_least64_t baseval{0x0101010101010101};
						constexpr ::std::uint_least64_t zero_lower_bound{isebcdic ? baseval * 0xF0 : baseval * 0x30};
						constexpr ::std::uint_least64_t first_bound{0x4646464646464646 + baseval * (10 - base_char_type)};
						constexpr ::std::uint_least64_t mul1{pow_base_sizeof_base_2 + (pow_base_sizeof_base_6 << 32)};
						constexpr ::std::uint_least64_t mul2{1 + (pow_base_sizeof_base_4 << 32)};
						constexpr ::std::uint_least64_t mask{0x000000FF000000FF};
						constexpr ::std::uint_least64_t fullmask{baseval * 0x80};

						while (static_cast<::std::size_t>(first_phase_last - first) >= sizeof(::std::uint_least64_t)) [[likely]]
						{
							::std::uint_least64_t val;
							::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));

							if constexpr (::std::endian::little != ::std::endian::native)
							{
								val = ::fast_io::little_endian(val);
							}

							if (::std::uint_least64_t const cval{((val + first_bound) | (val - zero_lower_bound)) & fullmask}; cval) [[likely]]
							{
								unsigned ctrz_cval{static_cast<unsigned>(::std::countr_zero(cval))};
								auto const valid_bits{ctrz_cval & static_cast<unsigned>(-8)};

								if (valid_bits) [[likely]]
								{
									val <<= 64 - valid_bits;

									::std::uint_least64_t all_zero{zero_lower_bound};

									all_zero >>= valid_bits;

									val |= all_zero;
									val -= zero_lower_bound;

									val = (val * base_char_type) + (val >> 8);
									val = (((val & mask) * mul1) + (((val >> 16) & mask) * mul2)) >> 32;
									ctrz_cval >>= shifter;
									res = static_cast<T>(res * ::fast_io::details::pow_table_n<base_char_type, ::std::uint_least64_t, 8>.index_unchecked(ctrz_cval) + val);

									first += ctrz_cval;
								}
								// The non-Clang MSVC-compatible spelling returns directly through the
								// same overflow checker instead of the shared forward join.  Other
								// compilers share nextlabel to avoid cloning that checker.  Both routes
								// preserve the same pointer and accumulator state.
#if defined(_MSC_VER) && !defined(__clang__)
								auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<base, char_type, T>(first, last, res)};
								out = res;
								return ret;
#else
								goto nextlabel;
#endif
							}

							/*
							This point is reached only after the packed range test has proved that
							all eight bytes are valid base-2 digits.  The earlier endian conversion
							also makes the first input character byte zero of val.  Thus each byte
							can be reduced to d_i in {0, 1}: subtracting the packed zero character
							does so without a cross-byte borrow, while ASCII '0'/'1' masking has the
							same result on the selected Apple path.

							For D = sum(d_i * 2^(8*i)) and
							P = 0x8040201008040201, byte seven of D*P is

							    sum(d_i * 2^(7-i)),  0 <= i < 8,

							which is exactly the value of the eight input bits in source order.
							There is no carry into that byte: for every lower convolution byte k,
							the maximum coefficient is 1 + 2 + ... + 2^k = 2^(k+1)-1,
							which is at most 255.  Shifting the previous unsigned accumulator by
							eight and ORing this byte therefore appends the block in base two;
							unsigned wraparound, where relevant to the surrounding overflow logic,
							is defined.

							The Apple AArch64 mask is intentionally isolated.  Paired M4 hardware
							measurements favored it by about one percent even though the M4
							llvm-mca model did not predict that difference.  Cortex-A76 and
							Neoverse-N2 static models favor the generic subtraction because it
							avoids the extra AND.  In the inspected Apple-Clang-21 Cortex-A76 and
							Neoverse-N2 cross-target lowerings, (val - zero) * P becomes MADD with
							a precomputed -zero * P addend followed by EXTR; the Apple mask
							form emits AND, MUL, and EXTR.  Traditional AArch64 keeps the former,
							while x86-64 keeps the algebraically equivalent generic subtraction
							with an ISA-specific lowering.  These are respectively measured and
							modeled claims, not a claim for every core or compiler lowering.

							The enclosing one-byte, non-EBCDIC condition keeps the ASCII reduction
							out of wide and EBCDIC paths.  Invalid or partial blocks branch above
							this code and retain the existing scalar reduction.
							*/
							if constexpr (base_char_type == 2u)
							{
#if defined(__APPLE__) && (defined(__aarch64__) || defined(_M_ARM64))
								val &= baseval;
#else
								val -= zero_lower_bound;
#endif
								val *= UINT64_C(0x8040201008040201);
								res = static_cast<T>((static_cast<unsigned_type>(res) << 8u) |
													 static_cast<unsigned_type>(val >> 56u));
							}
							else
							{
								val -= zero_lower_bound;
								val = (val * base_char_type) + (val >> 8);
								val = (((val & mask) * mul1) + (((val >> 16) & mask) * mul2)) >> 32;
								res = static_cast<T>(res * pow_base_sizeof_u64 + val);
							}
							first += sizeof(::std::uint_least64_t);
						}

#if defined(__aarch64__) || defined(_M_ARM64)
						if constexpr (base_char_type == 10u && my_unsigned_integral<T> &&
									  sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
						{
							// Once two packed blocks have produced the first sixteen digits, finish a possible
							// maximum-width decimal in place.  This shares the validation already paid by the
							// ordinary large-range scanner instead of speculatively reparsing from byte zero.
							if (static_cast<::std::size_t>(last - first) >= 4u &&
								static_cast<::std::size_t>(first - (first_phase_last - mn_val)) == 16u)
							{
								::std::uint_least32_t word;
								::fast_io::freestanding::my_memcpy(
									__builtin_addressof(word), first, sizeof(word));
								word = ::fast_io::little_endian(word);
								if ((((word + 0x46464646u) | (word - 0x30303030u)) &
									 0x80808080u) == 0u) [[likely]]
								{
									word -= 0x30303030u;
									word = word * 10u + (word >> 8u);
									auto const tail{static_cast<::std::uint_least64_t>(
										((word & 0x000000FFu) * 100u) +
										((word >> 16u) & 0x000000FFu))};
									auto const next{first + 4u};
									if (next != last && char_is_digit<10u, char_type>(
													   static_cast<unsigned_char_type>(*next))) [[unlikely]]
									{
										return {skip_digits<10u>(next + 1u, last), parse_code::overflow};
									}
									constexpr auto risky_value{
										static_cast<::std::uint_least64_t>(-1) / 10000u};
									constexpr auto risky_digit{
										static_cast<::std::uint_least64_t>(-1) % 10000u};
									if (risky_value < static_cast<unsigned_type>(res) ||
										(static_cast<unsigned_type>(res) == risky_value &&
										 risky_digit < tail)) [[unlikely]]
									{
										return {next, parse_code::overflow};
									}
									out = static_cast<T>(
										static_cast<unsigned_type>(res) * 10000u + tail);
									return {next, parse_code::ok};
								}
							}
						}
#endif
					}

					if constexpr (max_size >= sizeof(::std::uint_least32_t))
					{
						constexpr ::std::uint_least32_t pow_base_sizeof_u32{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, sizeof(::std::uint_least32_t)>};
						constexpr ::std::uint_least32_t first_bound{0x46464646 + 0x01010101 * (10 - base_char_type)};
						constexpr ::std::uint_least32_t pow_base_sizeof_base_2{::fast_io::details::compile_pow_n<::std::uint_least32_t, base_char_type, 2>};
						constexpr ::std::uint_least32_t mask{0x000000FF};

						if (static_cast<::std::size_t>(first_phase_last - first) >= sizeof(::std::uint_least32_t))
						{
							::std::uint_least32_t val;
							::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least32_t));

							if constexpr (::std::endian::little != ::std::endian::native)
							{
								val = ::fast_io::little_endian(val);
							}

							if (::std::uint_least32_t const cval{((val + first_bound) | (val - 0x30303030)) & 0x80808080}; cval) [[likely]]
							{
								unsigned ctrz_cval{static_cast<unsigned>(::std::countr_zero(cval))};
								auto const valid_bits{ctrz_cval & static_cast<unsigned>(-8)};

								if (valid_bits) [[likely]]
								{
									val <<= 32 - valid_bits;

									::std::uint_least32_t all_zero{0x30303030};

									all_zero >>= valid_bits;

									val |= all_zero;

									val -= 0x30303030;
									val = (val * base_char_type) + (val >> 8);
									val = (((val & mask) * pow_base_sizeof_base_2) + ((val >> 16) & mask));

									ctrz_cval >>= shifter;
									res = static_cast<T>(res * ::fast_io::details::pow_table_n<base_char_type, ::std::uint_least32_t, 4>.index_unchecked(ctrz_cval) + val);

									first += ctrz_cval;
								}
								// Repeat the direct-return compatibility spelling for this 32-bit
								// partial SWAR exit; its state matches the shared join.
#if defined(_MSC_VER) && !defined(__clang__)
								auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<base, char_type, T>(first, last, res)};
								out = res;
								return ret;
#else
								goto nextlabel;
#endif
							}
							else
							{
								val -= 0x30303030;
								val = (val * base_char_type) + (val >> 8);
								val = (((val & mask) * pow_base_sizeof_base_2) + ((val >> 16) & mask));
								res = static_cast<T>(res * pow_base_sizeof_u32 + val);
								first += sizeof(::std::uint_least32_t);
							}
						}
					}
				}
				else if constexpr (sizeof(char_type) == sizeof(char16_t) &&
							   ::std::endian::native == ::std::endian::little)
				{
					/*
					The packed char16_t reduction is deliberately little-endian-only.
					Its constants place the first source code unit in bits 0..15 and
					use countr_zero in units of sixteen bits.  A whole-word byte swap,
					unlike the one-byte path above, would also reverse the two bytes
					inside every UTF-16 code unit: big-endian u'1' (0x0031) would become
					0x3100 and fail the 0x0030 lane predicate.  Reversing just four
					halfword lanes is possible, but adds target-specific code to an
					unmeasured path.  The scalar recurrence below reads char16_t objects
					as code units and is endian-agnostic, so selecting it on big or
					mixed/PDP byte order preserves the identical digit, pointer, and
					overflow contract.  Native little-endian x86-64 and AArch64 retain
					the existing packed implementation unchanged.
					*/
					constexpr ::std::size_t u64_size_of_c16{sizeof(::std::uint_least64_t) / sizeof(char16_t)};
					constexpr ::std::uint_least64_t pow_base_sizeof_u64{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, u64_size_of_c16>};
					constexpr ::std::uint_least64_t pow_base_sizeof_base_2{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, 2>};
					constexpr ::std::uint_least64_t mask{0x000000000000FFFF};
					constexpr ::std::uint_least64_t first_bound{0x7fc67fc67fc67fc6 + 0x0001000100010001 * (10 - base)};
					if constexpr (max_size >= u64_size_of_c16)
					{
						while (static_cast<::std::size_t>(first_phase_last - first) >= u64_size_of_c16) [[likely]]
						{
							::std::uint_least64_t val;
							::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));

							if constexpr (::std::endian::little != ::std::endian::native)
							{
								val = ::fast_io::little_endian(val);
							}

							if (::std::uint_least64_t const cval{((val + first_bound) | (val - 0x0030003000300030)) & 0x8000800080008000}; cval) [[likely]]
							{
								unsigned ctrz_cval{static_cast<unsigned>(::std::countr_zero(cval))};
								auto const valid_bits{ctrz_cval & static_cast<unsigned>(-16)};

								if (valid_bits) [[likely]]
								{
									val <<= 64 - valid_bits;

									::std::uint_least64_t all_zero{0x0030003000300030};

									all_zero >>= valid_bits;

									val |= all_zero;

									val -= 0x0030003000300030;
									val = (val * base_char_type) + (val >> 16);
									val = (((val & mask) * pow_base_sizeof_base_2) + ((val >> 32) & mask));

									ctrz_cval >>= shifter;
									res = static_cast<T>(res * ::fast_io::details::pow_table_n<base_char_type, ::std::uint_least64_t, 4>.index_unchecked(ctrz_cval) + val);

									first += ctrz_cval;
								}
								// Repeat the direct-return compatibility spelling for this partial
								// char16_t block; its state matches the shared join.
#if defined(_MSC_VER) && !defined(__clang__)
								auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<base, char_type, T>(first, last, res)};
								out = res;
								return ret;
#else
								goto nextlabel;
#endif
							}
							val -= 0x0030003000300030;
							val = (val * base_char_type) + (val >> 16);
							val = (((val & mask) * pow_base_sizeof_base_2) + ((val >> 32) & mask));
							res = static_cast<T>(res * pow_base_sizeof_u64 + val);
							first += u64_size_of_c16;
						}
					}
				}
			}
			else if constexpr (base_char_type <= 16)
			{
				if constexpr (sizeof(char_type) == sizeof(char8_t))
				{
					if constexpr (max_size >= sizeof(::std::uint_least64_t))
					{
						constexpr ::std::uint_least64_t pow_base_sizeof_base_2{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, 2>};
						constexpr ::std::uint_least64_t pow_base_sizeof_base_4{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, 4>};
						constexpr ::std::uint_least64_t pow_base_sizeof_base_6{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, 6>};
						constexpr ::std::uint_least64_t pow_base_sizeof_u64{::fast_io::details::compile_pow_n<::std::uint_least64_t, base_char_type, sizeof(::std::uint_least64_t)>};
						constexpr ::std::uint_least64_t first_bound1{0x3939393939393939 + 0x0101010101010101 * (16 - base_char_type)};
						constexpr ::std::uint_least64_t first_bound2{0x1919191919191919 + 0x0101010101010101 * (16 - base_char_type)};

						constexpr ::std::uint_least64_t mask{0x000000FF000000FF};
						constexpr ::std::uint_least64_t mul1{pow_base_sizeof_base_2 + (pow_base_sizeof_base_6 << 32)};
						constexpr ::std::uint_least64_t mul2{1 + (pow_base_sizeof_base_4 << 32)};
						while (static_cast<::std::size_t>(first_phase_last - first) >= sizeof(::std::uint_least64_t)) [[likely]]
						{
							::std::uint_least64_t val;
							::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least64_t));

							if constexpr (::std::endian::little != ::std::endian::native)
							{
								val = ::fast_io::little_endian(val);
							}

							if (::std::uint_least64_t const cval{((((val + 0x4646464646464646) | (val - 0x3030303030303030)) &
																   ((val + first_bound1) | (val - 0x4040404040404040)) &
																   ((val + first_bound2) | (val - 0x6060606060606060))) |
																  ~(((val + 0x3f3f3f3f3f3f3f3f) | (val - 0x4040404040404040)) &
																	((val + 0x1f1f1f1f1f1f1f1f) | (val - 0x6060606060606060)))) &
																 0x8080808080808080};
								cval) [[likely]]
							{
								unsigned ctrz_cval{static_cast<unsigned>(::std::countr_zero(cval))};
								auto const valid_bits{ctrz_cval & static_cast<unsigned>(-8)};

								if (valid_bits) [[likely]]
								{
									val <<= 64 - valid_bits;

									::std::uint_least64_t all_zero{0x3030303030303030};

									all_zero >>= valid_bits;

									val |= all_zero;

									val -= 0x3030303030303030;
									val = (val & 0x0f0f0f0f0f0f0f0f) + ((val & 0x1010101010101010) >> 4) * 9;
									val = (val * base_char_type) + (val >> 8);
									val = (((val & mask) * mul1) + (((val >> 16) & mask) * mul2)) >> 32;

									ctrz_cval >>= shifter;

									res = static_cast<T>(res * ::fast_io::details::pow_table_n<base_char_type, ::std::uint_least64_t, 8>.index_unchecked(ctrz_cval) + val);
									first += ctrz_cval;
								}

								// Repeat the direct-return compatibility spelling for this partial
								// alphanumeric block; its state matches the shared join.
#if defined(_MSC_VER) && !defined(__clang__)
								auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<base, char_type, T>(first, last, res)};
								out = res;
								return ret;
#else
								goto nextlabel;
#endif
							}

							val -= 0x3030303030303030;
							val = (val & 0x0f0f0f0f0f0f0f0f) + ((val & 0x1010101010101010) >> 4) * 9;
							val = (val * base_char_type) + (val >> 8);
							val = (((val & mask) * mul1) + (((val >> 16) & mask) * mul2)) >> 32;
							res = static_cast<T>(res * pow_base_sizeof_u64 + val);
							first += sizeof(::std::uint_least64_t);
						}
					}
				}
			}
		}
		else if constexpr (sizeof(::std::uint_least16_t) < sizeof(::std::size_t))
		{
			if constexpr (base_char_type <= 10)
			{
				if constexpr (sizeof(char_type) == sizeof(char8_t))
				{
					if constexpr (max_size >= sizeof(::std::uint_least32_t))
					{
						constexpr ::std::uint_least32_t pow_base_sizeof_u32{::fast_io::details::compile_pow_n<::std::uint_least32_t, base_char_type, sizeof(::std::uint_least32_t)>};
						constexpr ::std::uint_least32_t first_bound{0x46464646 + 0x01010101 * (10 - base_char_type)};

						constexpr ::std::uint_least32_t pow_base_sizeof_base_2{::fast_io::details::compile_pow_n<::std::uint_least32_t, base_char_type, 2>};
						constexpr ::std::uint_least32_t mask{0x000000FF};
						while (static_cast<::std::size_t>(first_phase_last - first) >= sizeof(::std::uint_least32_t)) [[likely]]
						{
							::std::uint_least32_t val;
							::fast_io::freestanding::my_memcpy(__builtin_addressof(val), first, sizeof(::std::uint_least32_t));

							val = ::fast_io::little_endian(val);

							if (::std::uint_least32_t const cval{((val + first_bound) | (val - 0x30303030)) & 0x80808080}; cval) [[likely]]
							{
								unsigned ctrz_cval{static_cast<unsigned>(::std::countr_zero(cval))};
								auto const valid_bits{ctrz_cval & static_cast<unsigned>(-8)};

								if (valid_bits) [[likely]]
								{
									val <<= 32 - valid_bits;

									::std::uint_least32_t all_zero{0x30303030};

									all_zero >>= valid_bits;

									val |= all_zero;

									val -= 0x30303030;
									val = (val * base_char_type) + (val >> 8);
									val = (((val & mask) * pow_base_sizeof_base_2) + ((val >> 16) & mask));
									ctrz_cval >>= shifter;

									res = static_cast<T>(res * ::fast_io::details::pow_table_n<base_char_type, ::std::uint_least32_t, 4>.index_unchecked(ctrz_cval) + val);

									first += ctrz_cval;
								}
								// Repeat the direct-return compatibility spelling for the small-word
								// SWAR exit; its state matches the shared join.
#if defined(_MSC_VER) && !defined(__clang__)
								auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<base, char_type, T>(first, last, res)};
								out = res;
								return ret;
#else
								goto nextlabel;
#endif
							}

							val -= 0x30303030;
							val = (val * base_char_type) + (val >> 8);
							val = (((val & mask) * pow_base_sizeof_base_2) + ((val >> 16) & mask));
							res = static_cast<T>(res * pow_base_sizeof_u32 + val);
							first += sizeof(::std::uint_least32_t);
						}
					}
				}
			}
		}
	}
	// The observed Clang AVX2 lowering otherwise expands this serial accumulator to
	// eight copies under -march=native.  The wider body adds front-end work
	// without breaking the multiply/add dependency chain.
	// The pragmas affect layout only.  An isolated current-version native A/B
	// is required before assigning a numeric gain to either unroll factor.
#if defined(__clang__) && defined(__AVX2__) && \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	if constexpr (11u <= base)
	{
#pragma clang loop unroll_count(4)
		for (; first != first_phase_last; ++first) [[likely]]
		{
			unsigned_char_type ch{static_cast<unsigned_char_type>(*first)};
			if (char_digit_to_literal<base, char_type>(ch)) [[unlikely]]
			{
				break;
			}
			res *= base_char_type;
			res += ch;
		}
	}
	else
#endif
	{
#if defined(__clang__) && defined(__AVX2__) && \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
#pragma clang loop unroll_count(2)
#endif
		for (; first != first_phase_last; ++first) [[likely]]
		{
			unsigned_char_type ch{static_cast<unsigned_char_type>(*first)};
			if (char_digit_to_literal<base, char_type>(ch)) [[unlikely]]
			{
				break;
			}
			res *= base_char_type;
			res += ch;
		}
	}

	// Only the compiler arms that use the shared goto require this join label.
#if !defined(_MSC_VER) || defined(__clang__)
[[maybe_unused]] nextlabel:;
#endif

	if (first == last)
	{
		out = res;
		return {first, parse_code::ok};
	}

	auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<base, char_type, T>(first, last, res)};
	out = res;
	return ret;
}

template <char8_t base, ::std::integral char_type, my_unsigned_integral T>
inline constexpr parse_result<char_type const *>
compile_time_scan_int_contiguous_none_simd_space_part_define_impl(char_type const *first, char_type const *last, T &out) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	constexpr char8_t base_char_type{base};
	constexpr bool isspecialbase{base == 2 || base == 4 || base == 16};
	constexpr ::std::size_t max_size{::fast_io::details::max_int_size_result<unsigned_type, base> - (!isspecialbase)};
	::std::size_t const diff{static_cast<::std::size_t>(last - first)};
	::std::size_t mn_val{max_size};

	if (diff < mn_val)
	{
		mn_val = diff;
	}

	auto first_phase_last{first + mn_val};
	T res{out};

	for (; first != first_phase_last; ++first)
	{
		unsigned_char_type ch{static_cast<unsigned_char_type>(*first)};
		if (char_digit_to_literal<base, char_type>(ch)) [[unlikely]]
		{
			break;
		}
		res *= base_char_type;
		res += ch;
	}

	if (first == last)
	{
		out = res;
		return {first, parse_code::ok};
	}

	auto ret{scan_int_contiguous_none_simd_space_part_check_overflow_impl<base, char_type, T>(first, last, res)};
	out = res;
	return ret;
}

template <char8_t base, ::std::integral char_type, my_unsigned_integral T>
inline constexpr parse_result<char_type const *>
scan_int_contiguous_none_simd_space_part_define_impl(char_type const *first, char_type const *last, T &res) noexcept
{
	// SIMD and target builtins are excluded from constant evaluation.  The
	// constexpr arm is a scalar implementation with the same parse contract.
	FAST_IO_IF_NOT_CONSTEVAL
	{
		using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
		if constexpr (base == 16 && sizeof(char_type) == sizeof(char8_t) &&
					  ::fast_io::details::is_ascii<char_type> &&
					  sizeof(unsigned_type) <= sizeof(::std::uint_least64_t))
		{
			return ::fast_io::details::scan_int_contiguous_ascii_hex_space_part_define_impl<char_type, T>(
				first, last, res);
		}
		return runtime_scan_int_contiguous_none_simd_space_part_define_impl<base, char_type, T>(first, last, res);
	}
	else
	{
		return compile_time_scan_int_contiguous_none_simd_space_part_define_impl<base, char_type, T>(first, last, res);
	}
}

/*
The native x86-64 four-digit helper performs one bounded 32-bit load.  After
subtracting ASCII zero, the two masks prove 0 <= d_i < B <= 10.  Hence each
pair d_0*B+d_1 is at most 99 and fits one selected byte, while the four-digit
value is at most 9999 and fits the selected low 16 bits.  The masks retain
exactly those byte and halfword fields, so a carry from an adjacent packed
field cannot affect the result.  Call sites use it only where the destination
cannot overflow.  GNU/Clang builtins supply the unaligned load;
ARM64EC and other targets keep the scalar scanner.  Native x86 matrices and
assembly support the helper, independently of this arithmetic proof.
*/
#if (defined(__GNUC__) || defined(__clang__)) &&                     \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
template <::std::size_t base, ::std::integral char_type>
	requires(2u <= base && base <= 10u && sizeof(char_type) == sizeof(char8_t) &&
			 ::fast_io::details::is_ascii<char_type>)
[[gnu::always_inline]] inline bool
scan_int_contiguous_x86_parse_four_digits(char_type const *first,
										  ::std::uint_least64_t &value) noexcept
{
	::std::uint_least32_t chunk;
	__builtin_memcpy(__builtin_addressof(chunk), first, sizeof(chunk));
	chunk -= 0x30303030u;
	constexpr auto limit_bias{static_cast<::std::uint_least32_t>(16u - base) *
							  0x01010101u};
	if ((chunk & 0xf0f0f0f0u) != 0u ||
		((chunk + limit_bias) & 0x10101010u) != 0u) [[unlikely]]
	{
		return false;
	}
	auto const pairs{(chunk * base + (chunk >> 8u)) & 0x00ff00ffu};
	value = (pairs * (base * base) + (pairs >> 16u)) & 0xffffu;
	return true;
}

/*
The decimal-only eight-digit helper is the 64-bit extension of the four-digit
proof above. Subtracting ASCII zero and applying the high-nibble/carry masks
proves every lane is in [0, 9]. Pair and quad reductions retain disjoint packed
fields; their maxima are 99 and 9999 respectively, and the final value is at
most 99,999,999. Therefore none of the unsigned packed operations can carry
across a retained field. The caller proves an eight-byte readable range.
*/
template <::std::integral char_type>
	requires(sizeof(char_type) == sizeof(char8_t) &&
			 ::fast_io::details::is_ascii<char_type>)
[[gnu::always_inline]] inline bool
scan_int_contiguous_x86_parse_eight_decimal_digits(
	char_type const *first, ::std::uint_least64_t &value) noexcept
{
	::std::uint_least64_t chunk;
	__builtin_memcpy(__builtin_addressof(chunk), first, sizeof(chunk));
	chunk -= UINT64_C(0x3030303030303030);
	/* If a lane already has a high nibble, it is outside [0, 15]. Otherwise
	   adding six cannot carry between bytes and raises that lane's high nibble
	   exactly for values [10, 15]. The combined mask is therefore equivalent to
	   the two-predicate decimal proof while requiring one fewer live constant. */
	if (((chunk | (chunk + UINT64_C(0x0606060606060606))) &
		  UINT64_C(0xf0f0f0f0f0f0f0f0)) != 0u) [[unlikely]]
	{
		return false;
	}
	auto const pairs{(chunk * 10u + (chunk >> 8u)) &
					 UINT64_C(0x00ff00ff00ff00ff)};
	auto const quads{(pairs * 100u + (pairs >> 16u)) &
					 UINT64_C(0x0000ffff0000ffff)};
	value = (quads * 10000u + (quads >> 32u)) & UINT64_C(0xffffffff);
	return true;
}
#endif

/*
The eight-byte SSE4.1 helper extends the same proof to bases 5--36.  Parallel
digit/letter masks validate all eight bytes before pmaddubsw and pmaddwd form
base-B pairs and quads; the final scalar combination yields the original
eight-digit polynomial.  For B <= 36, a pair is at most 1295 (< 2^15), a
four-digit group is at most 1,679,615 (< 2^31), and the final polynomial is
below 36^8 < 2^64.  Therefore pmaddubsw cannot saturate and pmaddwd cannot
overflow a signed 32-bit lane.  The helper performs no over-read.  Raw
__builtin_ia32_* names avoid an intrinsic header, and the ISA/ARM64EC guard is
compile-target policy.
The call-site GCC exception below is based on whole-matrix front-end evidence,
not on a claim that these isolated multiply-add instructions are slow.
*/
#if (defined(__GNUC__) || defined(__clang__)) && defined(__SSE4_1__) && \
	((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) &&   \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC)))
template <::std::size_t base, ::std::integral char_type>
	requires(5u <= base && base <= 36u && sizeof(char_type) == sizeof(char8_t) &&
			 ::fast_io::details::is_ascii<char_type>)
[[gnu::always_inline]] inline bool
scan_int_contiguous_x86_sse_parse_eight(char_type const *first,
										::std::uint_least64_t &value) noexcept
{
	using namespace ::fast_io::intrinsics;
	x86_64_v16qu chunk{};
	__builtin_memcpy(__builtin_addressof(chunk), first, sizeof(::std::uint_least64_t));
	x86_64_v16qu const lower{
		chunk | x86_64_v16qu{0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
							 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20}};
	x86_64_v16qs const schunk{(x86_64_v16qs)chunk};
	constexpr char digit_upper{static_cast<char>(base <= 10u ? 0x30u + base : 0x3au)};
	x86_64_v16qs const digit_mask{
		(schunk > x86_64_v16qs{0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,
							   0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f}) &
		(x86_64_v16qs{digit_upper, digit_upper, digit_upper, digit_upper,
					  digit_upper, digit_upper, digit_upper, digit_upper,
					  digit_upper, digit_upper, digit_upper, digit_upper,
					  digit_upper, digit_upper, digit_upper, digit_upper} > schunk)};
	x86_64_v16qs valid_vector{digit_mask};
	x86_64_v16qu values{
		chunk - x86_64_v16qu{0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
							 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30}};
	if constexpr (10u < base)
	{
		x86_64_v16qs const slower{(x86_64_v16qs)lower};
		constexpr char alpha_last{static_cast<char>(0x61u + (base - 10u))};
		x86_64_v16qs const alpha_mask{
			(slower > x86_64_v16qs{0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
								   0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60}) &
			(x86_64_v16qs{alpha_last, alpha_last, alpha_last, alpha_last,
						  alpha_last, alpha_last, alpha_last, alpha_last,
						  alpha_last, alpha_last, alpha_last, alpha_last,
						  alpha_last, alpha_last, alpha_last, alpha_last} > slower)};
		valid_vector |= alpha_mask;
		x86_64_v16qu const alpha_values{
			lower - x86_64_v16qu{0x57, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57,
								 0x57, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57}};
		values = (values & (x86_64_v16qu)digit_mask) |
				 (alpha_values & ~(x86_64_v16qu)digit_mask);
	}
	auto const valid_mask{static_cast<::std::uint_least16_t>(
		__builtin_ia32_pmovmskb128((x86_64_v16qi)valid_vector))};
	if (static_cast<::std::uint_least8_t>(valid_mask) != 0xffu) [[unlikely]]
	{
		return false;
	}
	values = (x86_64_v16qu)__builtin_ia32_pmaddubsw128(
		(x86_64_v16qi)values,
		x86_64_v16qi{base, 1, base, 1, base, 1, base, 1,
					 base, 1, base, 1, base, 1, base, 1});
	constexpr auto base_squared{static_cast<::std::uint_least16_t>(base * base)};
	values = (x86_64_v16qu)__builtin_ia32_pmaddwd128(
		(x86_64_v8hi)values,
		x86_64_v8hi{base_squared, 1, base_squared, 1,
					base_squared, 1, base_squared, 1});
	::std::uint_least64_t quads;
	__builtin_memcpy(__builtin_addressof(quads), __builtin_addressof(values),
					 sizeof(quads));
	constexpr auto base_fourth{static_cast<::std::uint_least64_t>(base_squared) *
							   static_cast<::std::uint_least64_t>(base_squared)};
	value = static_cast<::std::uint_least32_t>(quads) * base_fourth +
			(quads >> 32u);
	return true;
}
#endif

#if (defined(__GNUC__) || defined(__clang__)) &&                     \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
/*
The selector expresses a code-generation policy, not a semantic distinction.
Both callees implement the proved eight-decimal-digit polynomial above. Native
GCC 14--16 schedules the integer SWAR form with a substantially shorter
dependency chain, while Clang 18--22 benefits from pmaddubsw/pmaddwd when the
target advertises SSE4.1. Later frontend versions deliberately inherit the
highest measured policy; a baseline Clang target uses the portable x86 SWAR
form rather than requiring an unavailable ISA.
*/
template <::std::integral char_type>
	requires(sizeof(char_type) == sizeof(char8_t) &&
			 ::fast_io::details::is_ascii<char_type>)
[[gnu::always_inline]] inline bool
scan_int_contiguous_x86_parse_eight_decimal_selected(
	char_type const *first, ::std::uint_least64_t &value) noexcept
{
#if defined(__clang__) && defined(__SSE4_1__)
	return scan_int_contiguous_x86_sse_parse_eight<10u>(first, value);
#else
	return scan_int_contiguous_x86_parse_eight_decimal_digits(first, value);
#endif
}
#endif

/*
The specialized SSE hexadecimal and octal helpers share a caller-proved
load-width contract, prefix-validity movemask, and exact lane reduction.
Eight or sixteen hexadecimal digits fit the corresponding unsigned block;
signed limits and a digit beyond a full sixteen-digit hexadecimal block are
checked explicitly.  The octal helper separately enforces its 22-digit
leading-digit bound and the signed limit.  Every result pointer is advanced
to the first non-digit, including after overflow skipping.  GNU-family
configurations other than those defining the legacy __INTEL_COMPILER macro use
raw __builtin_ia32_* operations; the fallback branch uses the equivalent
x86-intrinsic spelling.  ASCII-only constants and call-site EBCDIC guards
keep execution-character encodings on the scalar path.  SSE4.1 is the minimum
compile target and there is no run-time ISA probe.

On the retained i9-14900HX GCC 13--16 follow-up, removing the specialized
short-hexadecimal and sixteen-byte octal/hexadecimal kernels improved aggregate
layout slightly but regressed their target bases by as much as approximately
11%, so those kernels were kept.  That is native whole-matrix evidence on one
host, not a pointwise or cross-core guarantee.  Other SSE4.1 compilers and
processors inherit only the arithmetic proof and no numeric throughput claim.
The family remains a conservative capability-gated policy pending broader
native revalidation; unsupported ISA, constant evaluation, wide/EBCDIC input,
and ineligible lengths retain the scalar/shared scanners.
*/
#if defined(__SSE4_1__) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)))
template <::std::integral char_type, my_integral T>
	// All three helpers below consume the family-level placement and evidence
	// policy above.  Force-inlining changes the call boundary only; an unavailable
	// attribute leaves the same helper ordinarily inline.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline parse_result<char_type const *>
scan_int_contiguous_x86_sse_hex8_space_part_define_impl(
	char_type const *first, T &t,
	[[maybe_unused]] bool sign) noexcept
{
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	auto finish_ok = [&](char_type const *it, unsigned_type res) constexpr noexcept -> parse_result<char_type const *> {
		if constexpr (my_signed_integral<T>)
		{
			constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
			constexpr unsigned_type imax{umax >> 1};
			if (res > (static_cast<unsigned_type>(imax) + sign)) [[unlikely]]
			{
				return {it, parse_code::overflow};
			}
			if (sign)
			{
				t = static_cast<T>(static_cast<unsigned_type>(0) - res);
			}
			else
			{
				t = static_cast<T>(res);
			}
		}
		else
		{
			t = static_cast<T>(res);
		}
		return {it, parse_code::ok};
	};

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	using namespace fast_io::intrinsics;
	x86_64_v16qu chunk{};
	__builtin_memcpy(__builtin_addressof(chunk), first, sizeof(::std::uint_least64_t));
	x86_64_v16qu const lower{chunk | x86_64_v16qu{0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
												  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20}};
	x86_64_v16qs const schunk{(x86_64_v16qs)chunk};
	x86_64_v16qs const slower{(x86_64_v16qs)lower};
	x86_64_v16qs const digit_mask{
		(schunk > x86_64_v16qs{0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,
							   0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f}) &
		(x86_64_v16qs{0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a,
					  0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a} > schunk)};
	x86_64_v16qs const alpha_mask{
		(slower > x86_64_v16qs{0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
							   0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60}) &
		(x86_64_v16qs{0x67, 0x67, 0x67, 0x67, 0x67, 0x67, 0x67, 0x67,
					  0x67, 0x67, 0x67, 0x67, 0x67, 0x67, 0x67, 0x67} > slower)};
	::std::uint_least16_t const valid_mask{static_cast<::std::uint_least16_t>(
		__builtin_ia32_pmovmskb128((x86_64_v16qi)(digit_mask | alpha_mask)))};
#else
	__m128i const chunk{_mm_loadl_epi64(reinterpret_cast<__m128i const *>(first))};
	__m128i const lower{_mm_or_si128(chunk, _mm_set1_epi8(0x20))};
	__m128i const digit_mask{
		_mm_and_si128(_mm_cmpgt_epi8(chunk, _mm_set1_epi8(static_cast<char>(0x2f))),
					  _mm_cmpgt_epi8(_mm_set1_epi8(static_cast<char>(0x3a)), chunk))};
	__m128i const alpha_mask{
		_mm_and_si128(_mm_cmpgt_epi8(lower, _mm_set1_epi8(static_cast<char>(0x60))),
					  _mm_cmpgt_epi8(_mm_set1_epi8(static_cast<char>(0x67)), lower))};
	::std::uint_least16_t const valid_mask{static_cast<::std::uint_least16_t>(
		_mm_movemask_epi8(_mm_or_si128(digit_mask, alpha_mask)))};
#endif
	auto const digits{static_cast<::std::uint_least32_t>(::std::countr_one(valid_mask))};
	if (digits == 0) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	x86_64_v16qu const digit_values{
		chunk - x86_64_v16qu{0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
							 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30}};
	x86_64_v16qu const alpha_values{
		lower - x86_64_v16qu{0x57, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57,
							 0x57, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57}};
	x86_64_v16qu values{(digit_values & (x86_64_v16qu)digit_mask) |
						(alpha_values & ~(x86_64_v16qu)digit_mask)};
	x86_64_v16qs const prefix_mask{
		x86_64_v16qs{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15} <
		x86_64_v16qs{static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits)}};
	values &= (x86_64_v16qu)prefix_mask;
	values = (x86_64_v16qu)__builtin_ia32_pmaddubsw128(
		(x86_64_v16qi)values, x86_64_v16qi{16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1});
	values = (x86_64_v16qu)__builtin_ia32_packuswb128((x86_64_v8hi)values, (x86_64_v8hi)values);
	::std::uint_least32_t res;
	__builtin_memcpy(__builtin_addressof(res), __builtin_addressof(values), sizeof(res));
#else
	__m128i const digit_values{_mm_sub_epi8(chunk, _mm_set1_epi8(static_cast<char>(0x30)))};
	__m128i const alpha_values{_mm_sub_epi8(lower, _mm_set1_epi8(static_cast<char>(0x57)))};
	__m128i values{_mm_blendv_epi8(alpha_values, digit_values, digit_mask)};
	values = _mm_and_si128(
		values, _mm_cmplt_epi8(_mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0),
							   _mm_set1_epi8(static_cast<char>(digits))));
	values = _mm_maddubs_epi16(values, _mm_set_epi8(1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16));
	values = _mm_packus_epi16(values, values);
	::std::uint_least32_t res;
	::fast_io::freestanding::my_memcpy(__builtin_addressof(res), __builtin_addressof(values), sizeof(res));
#endif
	res = ::fast_io::byte_swap(res);
	if (digits != 8u)
	{
		res >>= static_cast<unsigned>((8u - digits) << 2u);
	}
	return finish_ok(first + digits, static_cast<unsigned_type>(res));
}

template <::std::integral char_type, my_integral T>
	// Same force-inline capability policy, ordinary-inline fallback, and
	// unmeasured-frontend caveat as the hexadecimal/octal family comment above.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline parse_result<char_type const *>
scan_int_contiguous_x86_sse_hex16_space_part_define_impl(
	char_type const *first, char_type const *last, T &t,
	[[maybe_unused]] bool sign) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	auto finish_ok = [&](char_type const *it, unsigned_type res) constexpr noexcept -> parse_result<char_type const *> {
		if constexpr (my_signed_integral<T>)
		{
			constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
			constexpr unsigned_type imax{umax >> 1};
			if (res > (static_cast<unsigned_type>(imax) + sign)) [[unlikely]]
			{
				return {it, parse_code::overflow};
			}
			if (sign)
			{
				t = static_cast<T>(static_cast<unsigned_type>(0) - res);
			}
			else
			{
				t = static_cast<T>(res);
			}
		}
		else
		{
			t = static_cast<T>(res);
		}
		return {it, parse_code::ok};
	};

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	using namespace fast_io::intrinsics;
	x86_64_v16qu chunk;
	__builtin_memcpy(__builtin_addressof(chunk), first, sizeof(chunk));
	x86_64_v16qu const lower{chunk | x86_64_v16qu{0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
												  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20}};
	x86_64_v16qs const schunk{(x86_64_v16qs)chunk};
	x86_64_v16qs const slower{(x86_64_v16qs)lower};
	x86_64_v16qs const digit_mask{
		(schunk > x86_64_v16qs{0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,
							   0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f}) &
		(x86_64_v16qs{0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a,
					  0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a} > schunk)};
	x86_64_v16qs const alpha_mask{
		(slower > x86_64_v16qs{0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60,
							   0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x60}) &
		(x86_64_v16qs{0x67, 0x67, 0x67, 0x67, 0x67, 0x67, 0x67, 0x67,
					  0x67, 0x67, 0x67, 0x67, 0x67, 0x67, 0x67, 0x67} > slower)};
	::std::uint_least16_t const valid_mask{static_cast<::std::uint_least16_t>(
		__builtin_ia32_pmovmskb128((x86_64_v16qi)(digit_mask | alpha_mask)))};
#else
	__m128i const chunk{_mm_loadu_si128(reinterpret_cast<__m128i const *>(first))};
	__m128i const lower{_mm_or_si128(chunk, _mm_set1_epi8(0x20))};
	__m128i const digit_mask{
		_mm_and_si128(_mm_cmpgt_epi8(chunk, _mm_set1_epi8(static_cast<char>(0x2f))),
					  _mm_cmpgt_epi8(_mm_set1_epi8(static_cast<char>(0x3a)), chunk))};
	__m128i const alpha_mask{
		_mm_and_si128(_mm_cmpgt_epi8(lower, _mm_set1_epi8(static_cast<char>(0x60))),
					  _mm_cmpgt_epi8(_mm_set1_epi8(static_cast<char>(0x67)), lower))};
	::std::uint_least16_t const valid_mask{static_cast<::std::uint_least16_t>(
		_mm_movemask_epi8(_mm_or_si128(digit_mask, alpha_mask)))};
#endif
	auto const digits{static_cast<::std::uint_least32_t>(::std::countr_one(valid_mask))};
	if (digits == 0) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	x86_64_v16qu const digit_values{
		chunk - x86_64_v16qu{0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
							 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30}};
	x86_64_v16qu const alpha_values{
		lower - x86_64_v16qu{0x57, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57,
							 0x57, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57, 0x57}};
	x86_64_v16qu values{(digit_values & (x86_64_v16qu)digit_mask) |
						(alpha_values & ~(x86_64_v16qu)digit_mask)};
	x86_64_v16qs const prefix_mask{
		x86_64_v16qs{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15} <
		x86_64_v16qs{static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits)}};
	values &= (x86_64_v16qu)prefix_mask;
	values = (x86_64_v16qu)__builtin_ia32_pmaddubsw128(
		(x86_64_v16qi)values, x86_64_v16qi{16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1});
	values = (x86_64_v16qu)__builtin_ia32_packuswb128((x86_64_v8hi)values, (x86_64_v8hi)values);
	::std::uint_least64_t res;
	__builtin_memcpy(__builtin_addressof(res), __builtin_addressof(values), sizeof(res));
#else
	__m128i const digit_values{_mm_sub_epi8(chunk, _mm_set1_epi8(static_cast<char>(0x30)))};
	__m128i const alpha_values{_mm_sub_epi8(lower, _mm_set1_epi8(static_cast<char>(0x57)))};
	__m128i values{_mm_blendv_epi8(alpha_values, digit_values, digit_mask)};
	values = _mm_and_si128(
		values, _mm_cmplt_epi8(_mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0),
							   _mm_set1_epi8(static_cast<char>(digits))));
	values = _mm_maddubs_epi16(values, _mm_set_epi8(1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16, 1, 16));
	values = _mm_packus_epi16(values, values);
	::std::uint_least64_t res;
	::fast_io::freestanding::my_memcpy(__builtin_addressof(res), __builtin_addressof(values), sizeof(res));
#endif
	res = ::fast_io::byte_swap(res);
	if (digits != 16u)
	{
		res >>= static_cast<unsigned>((16u - digits) << 2u);
	}
	else if (last != first + 16u &&
			 char_is_digit<16u, char_type>(static_cast<unsigned_char_type>(first[16u]))) [[unlikely]]
	{
		return {skip_digits<16u>(first + 17u, last), parse_code::overflow};
	}
	return finish_ok(first + digits, static_cast<unsigned_type>(res));
}

template <::std::integral char_type, my_integral T>
	// Same force-inline capability policy, ordinary-inline fallback, and
	// unmeasured-frontend caveat as the hexadecimal/octal family comment above.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline parse_result<char_type const *>
scan_int_contiguous_x86_sse_oct16_space_part_define_impl(
	char_type const *first, char_type const *last, T &t,
	[[maybe_unused]] bool sign) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	auto finish_ok = [&](char_type const *it, unsigned_type res, ::std::size_t digits) constexpr noexcept -> parse_result<char_type const *> {
		if (22u < digits || (digits == 22u && 1u < static_cast<unsigned_char_type>(*first - char_literal_v<u8'0', char_type>))) [[unlikely]]
		{
			return {it, parse_code::overflow};
		}
		if constexpr (my_signed_integral<T>)
		{
			constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
			constexpr unsigned_type imax{umax >> 1};
			if (res > (static_cast<unsigned_type>(imax) + sign)) [[unlikely]]
			{
				return {it, parse_code::overflow};
			}
			if (sign)
			{
				t = static_cast<T>(static_cast<unsigned_type>(0) - res);
			}
			else
			{
				t = static_cast<T>(res);
			}
		}
		else
		{
			t = static_cast<T>(res);
		}
		return {it, parse_code::ok};
	};

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	using namespace fast_io::intrinsics;
	x86_64_v16qu chunk;
	__builtin_memcpy(__builtin_addressof(chunk), first, sizeof(chunk));
	x86_64_v16qs const schunk{(x86_64_v16qs)chunk};
	x86_64_v16qs const valid_vector{
		(schunk > x86_64_v16qs{0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f,
							   0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f, 0x2f}) &
		(x86_64_v16qs{0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38,
					  0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38, 0x38} > schunk)};
	::std::uint_least16_t const valid_mask{
		static_cast<::std::uint_least16_t>(__builtin_ia32_pmovmskb128((x86_64_v16qi)valid_vector))};
#else
	__m128i const chunk{_mm_loadu_si128(reinterpret_cast<__m128i const *>(first))};
	__m128i const valid_vector{
		_mm_and_si128(_mm_cmpgt_epi8(chunk, _mm_set1_epi8(static_cast<char>(0x2f))),
					  _mm_cmpgt_epi8(_mm_set1_epi8(static_cast<char>(0x38)), chunk))};
	::std::uint_least16_t const valid_mask{
		static_cast<::std::uint_least16_t>(_mm_movemask_epi8(valid_vector))};
#endif
	auto const digits{static_cast<::std::uint_least32_t>(::std::countr_one(valid_mask))};
	if (digits == 0) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__INTEL_COMPILER)
	x86_64_v16qu values{
		chunk - x86_64_v16qu{0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
							 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30}};
	x86_64_v16qs const prefix_mask{
		x86_64_v16qs{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15} <
		x86_64_v16qs{static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits),
					 static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits), static_cast<char>(digits)}};
	values &= (x86_64_v16qu)prefix_mask;
	auto pairs{(x86_64_v16qu)__builtin_ia32_pmaddubsw128(
		(x86_64_v16qi)values, x86_64_v16qi{8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1})};
	auto quads{(x86_64_v16qu)__builtin_ia32_pmaddwd128((x86_64_v8hi)pairs, x86_64_v8hi{64, 1, 64, 1, 64, 1, 64, 1})};
	::std::uint_least32_t quad_values[4];
	__builtin_memcpy(quad_values, __builtin_addressof(quads), sizeof(quad_values));
#else
	__m128i values{_mm_sub_epi8(chunk, _mm_set1_epi8(static_cast<char>(0x30)))};
	values = _mm_and_si128(
		values, _mm_cmplt_epi8(_mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0),
							   _mm_set1_epi8(static_cast<char>(digits))));
	auto pairs{_mm_maddubs_epi16(values, _mm_set_epi8(1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8, 1, 8))};
	auto quads{_mm_madd_epi16(pairs, _mm_set_epi16(1, 64, 1, 64, 1, 64, 1, 64))};
	::std::uint_least32_t quad_values[4];
	::fast_io::freestanding::my_memcpy(quad_values, __builtin_addressof(quads), sizeof(quad_values));
#endif
	::std::uint_least64_t res{
		(static_cast<::std::uint_least64_t>(quad_values[0]) << 36u) |
		(static_cast<::std::uint_least64_t>(quad_values[1]) << 24u) |
		(static_cast<::std::uint_least64_t>(quad_values[2]) << 12u) |
		static_cast<::std::uint_least64_t>(quad_values[3])};
	if (digits != 16u)
	{
		res >>= static_cast<unsigned>((16u - digits) * 3u);
		return finish_ok(first + digits, static_cast<unsigned_type>(res), digits);
	}

	auto it{first + 16u};
	::std::size_t total_digits{16u};
	for (; it != last && total_digits != 22u; ++it)
	{
		unsigned_char_type digit{static_cast<unsigned_char_type>(*it)};
		if (char_digit_to_literal<8u, char_type>(digit)) [[unlikely]]
		{
			return finish_ok(it, static_cast<unsigned_type>(res), total_digits);
		}
		res = (res << 3u) | digit;
		++total_digits;
	}
	if (it != last && char_is_digit<8u, char_type>(static_cast<unsigned_char_type>(*it))) [[unlikely]]
	{
		return {skip_digits<8u>(it + 1, last), parse_code::overflow};
	}
	return finish_ok(it, static_cast<unsigned_type>(res), total_digits);
}
#endif

inline constexpr parse_code ongoing_parse_code{static_cast<parse_code>(::std::numeric_limits<char unsigned>::max())};

template <char8_t base, bool oct_c2y, ::std::integral char_type>
inline constexpr parse_result<char_type const *> scan_shbase_impl(char_type const *first,
																  char_type const *last) noexcept
{
	if (first == last || *first != char_literal_v<u8'0', char_type>) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}
	if ((++first) == last) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}
	if constexpr (base == 2 || base == 3 || (base == 8 && oct_c2y) || base == 16)
	{
		auto ch{*first};
		if ((ch != char_literal_v<(base == 2 ? u8'B' : (base == 3 ? u8'T' : (base == 8 ? u8'O' : u8'X'))), char_type>)&(
				ch != char_literal_v<(base == 2 ? u8'b' : (base == 3 ? u8't' : (base == 8 ? u8'o' : u8'x'))), char_type>)) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		++first;
	}
	else
	{
		if (*first != char_literal_v<u8'[', char_type>) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		++first;
		if (first == last) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		constexpr auto digit0{char_literal_v<u8'0' + (base < 10 ? base : base / 10), char_type>};
		if (*first != digit0) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		if ((++first) == last) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		if constexpr (10 < base)
		{
			constexpr auto digit1{char_literal_v<u8'0' + (base % 10), char_type>};
			if (*first != digit1) [[unlikely]]
			{
				return {first, parse_code::invalid};
			}
			if ((++first) == last) [[unlikely]]
			{
				return {first, parse_code::invalid};
			}
		}
		if (*first != char_literal_v<u8']', char_type>) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		++first;
	}
	return {first, ongoing_parse_code};
}

template <::std::integral char_type>
inline constexpr char_type const *skip_hexdigits(char_type const *first, char_type const *last) noexcept;

template <char8_t base, bool shbase = false, bool skipzero = false, bool oct_c2y = false,
		  bool allow_leading_plus = false, bool zero_terminated_ok = false,
		  ::std::integral char_type, my_integral T>
/*
GCC 15 with AVX enabled was the measured compiler boundary where automatic
loop vectorization enlarged the complete instantiated scanner and its front-end
footprint.  Paired native all-type x86 measurements selected disabling loop
vectorization for GCC 15; applying the same setting to GCC 13 or GCC 16
regressed their aggregate latency.  SLP and explicit SSE builtins remain
enabled.  The attribute changes optimization policy only, not accepted digits
or arithmetic, and it is a compiler-version exception rather than
microarchitecture dispatch.
*/
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && \
	!defined(__CUDACC__) && __GNUC__ == 15 && defined(__AVX__) &&             \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC)) && ('A' == 0x41)
[[gnu::optimize("no-tree-loop-vectorize")]]
#endif
inline constexpr parse_result<char_type const *>
scan_int_contiguous_none_space_part_define_impl(char_type const *first, char_type const *last, T &t) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	[[maybe_unused]] bool sign{};
	if constexpr (my_signed_integral<T>)
	{
		if (first == last) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		constexpr auto minus_sign{char_literal_v<u8'-', char_type>};
		if ((sign = (minus_sign == *first)))
		{
			++first;
		}
		else if constexpr (allow_leading_plus)
		{
			if (*first == char_literal_v<u8'+', char_type>)
			{
				++first;
			}
		}
		if (first == last) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		if constexpr (shbase && base != 10)
		{
			if constexpr (base == 8 && !oct_c2y)
			{
				if (first == last || *first != char_literal_v<u8'0', char_type>) [[unlikely]]
				{
					return {first, parse_code::invalid};
				}
				++first;
			}
			else
			{
				auto phase_ret = scan_shbase_impl<base, oct_c2y>(first, last);
				if (phase_ret.code != ongoing_parse_code) [[unlikely]]
				{
					return phase_ret;
				}
				first = phase_ret.iter;
			}
		}
	}
	constexpr auto zero{char_literal_v<u8'0', char_type>};
	if (first == last) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}
	auto first_ch{*first};
	unsigned_char_type first_digit{static_cast<unsigned_char_type>(first_ch)};
	if (char_digit_to_literal<base, char_type>(first_digit)) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}
	else if (first_ch == zero) [[unlikely]]
	{
		if constexpr (skipzero)
		{
			++first;
			if constexpr (zero_terminated_ok)
			{
				while (first != last && *first == zero)
				{
					++first;
				}
			}
			else
			{
				first = ::fast_io::details::find_none_zero_simd_impl(first, last);
			}
			if (first == last) [[likely]]
			{
				t = 0;
				return {first, parse_code::ok};
			}
			first_digit = static_cast<unsigned_char_type>(*first);
			if (char_digit_to_literal<base, char_type>(first_digit)) [[unlikely]]
			{
				if constexpr (zero_terminated_ok)
				{
					t = 0;
					return {first, parse_code::ok};
				}
				return {first, parse_code::invalid};
			}
		}
		else
		{
			++first;
			if ((first == last) || (!char_is_digit<base, char_type>(static_cast<unsigned_char_type>(*first)))) [[likely]]
			{
				t = {};
				return {first, parse_code::ok};
			}
			return {first, parse_code::invalid};
		}
	}
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	/*
	This preprocessor guard begins the complete GNU-family native-x86
	specialization region, which ends at the matching directives below.  ARM64EC
	and non-x86 targets skip that region and resume at the shared scanner.  Its
	first branch performs the already-validated decimal digit's one-character
	termination check: if the second code unit is absent or non-decimal, assigning
	first_digit and returning next is exactly the general result.  Keeping it
	below the public wrapper makes internal input and from_chars share the path;
	other inputs continue through the remaining x86 specializations.

	The high-base 8-, 16-, and 32-bit bounded kernels were measured on the native
	i9-14900HX with Clang 23.  Across bases 17--36 their complete char-matrix gains
	were 1.64x--1.68x for 8-bit, 1.20x--1.30x for 16-bit, and about 1.21x for
	32-bit destinations; the reports also record smaller but positive complete-
	matrix gains.  Those numbers do not establish the one-digit decimal arm or an
	unmeasured compiler/core.  Such configurations inherit only the range and
	overflow proofs, not a numeric performance claim.  The decimal shortcut is a
	conservative x86 code-generation policy pending isolated native revalidation;
	all nonmatching inputs resume at the shared scanner after this region.
	*/
#if (defined(__GNUC__) || defined(__clang__)) &&                      \
	((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC)))
	if constexpr (base == 10u && sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type>)
	{
		auto const next{first + 1u};
		if (next == last ||
			!::fast_io::details::char_is_digit<base, char_type>(
				static_cast<unsigned_char_type>(*next))) [[likely]]
		{
			if constexpr (my_signed_integral<T>)
			{
				if (sign)
				{
					t = static_cast<T>(static_cast<unsigned_type>(0) -
									   static_cast<unsigned_type>(first_digit));
				}
				else
				{
					t = static_cast<T>(first_digit);
				}
			}
			else
			{
				t = static_cast<T>(first_digit);
			}
			return {next, parse_code::ok};
		}
	}
	/*
	A long unsigned decimal has no applicable high-base or bounded-short
	specialization below.  Enter the shared SWAR scanner before testing those
	strategies.  The same maximum-width proof used by the generic fallback seeds
	the already validated first digit only when unsigned overflow is impossible.
	Clang 20+ and GCC 14 or older use the measured 8+1/8+2 uint32_t
	kernel here.  Clang 17--19 use the shared SWAR path, while GCC 15+
	keeps its measured nine-digit kernel below.
	*/
	if constexpr (base == 10u && my_unsigned_integral<T> &&
				  (sizeof(unsigned_type) == sizeof(::std::uint_least64_t) ||
				   (sizeof(unsigned_type) == sizeof(::std::uint_least32_t)
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && !defined(__CUDACC__)
					&& __GNUC__ <= 14
#endif
				   )) &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type>)
	{
		auto const decimal_remaining{static_cast<::std::size_t>(last - first)};
		if (8u < decimal_remaining)
		{
#if (defined(__clang__) && __clang_major__ >= 20) ||                                      \
	(defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) &&            \
	 !defined(__CUDACC__) && __GNUC__ <= 14)
			if constexpr (sizeof(unsigned_type) == sizeof(::std::uint_least32_t))
			{
				FAST_IO_IF_NOT_CONSTEVAL
				{
					::std::uint_least64_t high;
					::std::uint_least64_t low;
					if (scan_int_contiguous_x86_parse_four_digits<10u>(first, high) &&
						scan_int_contiguous_x86_parse_four_digits<10u>(first + 4u, low)) [[likely]]
					{
						auto ninth{static_cast<unsigned_char_type>(first[8u])};
						if (!char_digit_to_literal<10u, char_type>(ninth)) [[likely]]
						{
							auto const prefix{high * 10000u + low};
							auto iter{first + 9u};
							if (iter == last)
							{
								t = static_cast<T>(prefix * 10u + ninth);
								return {iter, parse_code::ok};
							}
							auto tenth{static_cast<unsigned_char_type>(*iter)};
							if (char_digit_to_literal<10u, char_type>(tenth)) [[unlikely]]
							{
								t = static_cast<T>(prefix * 10u + ninth);
								return {iter, parse_code::ok};
							}
							++iter;
							if (iter != last &&
								char_is_digit<10u, char_type>(
									static_cast<unsigned_char_type>(*iter))) [[unlikely]]
							{
								return {skip_digits<10u>(iter + 1u, last), parse_code::overflow};
							}
							auto const value{prefix * 100u + ninth * 10u + tenth};
							constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
							if (static_cast<::std::uint_least64_t>(umax) < value) [[unlikely]]
							{
								return {iter, parse_code::overflow};
							}
							t = static_cast<T>(value);
							return {iter, parse_code::ok};
						}
					}
				}
			}
#endif
			unsigned_type decimal_value{};
			auto decimal_first{first};
			if constexpr (sizeof(unsigned_type) == sizeof(::std::uint_least32_t))
			{
				constexpr ::std::size_t max_digits{
					::fast_io::details::max_int_size_result<unsigned_type, base>};
				if (decimal_remaining < max_digits ||
					(decimal_remaining == max_digits &&
					 !char_is_digit<base, char_type>(
						 static_cast<unsigned_char_type>(last[-1])))) [[likely]]
				{
					decimal_value = static_cast<unsigned_type>(first_digit);
					decimal_first = first + 1u;
				}
			}
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && !defined(__CUDACC__) && __GNUC__ == 11
			else if constexpr (sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
			{
				constexpr ::std::size_t max_digits{
					::fast_io::details::max_int_size_result<unsigned_type, base>};
				if (decimal_remaining < max_digits ||
					(decimal_remaining == max_digits &&
					 !char_is_digit<base, char_type>(
						 static_cast<unsigned_char_type>(last[-1])))) [[likely]]
				{
					decimal_value = static_cast<unsigned_type>(first_digit);
					decimal_first = first + 1u;
				}
			}
#endif
			auto const ret{
				scan_int_contiguous_none_simd_space_part_define_impl<base>(
					decimal_first, last, decimal_value)};
			if (ret.code == parse_code::ok) [[likely]]
			{
				t = static_cast<T>(decimal_value);
			}
			return ret;
		}
	}
	if constexpr (17u <= base && sizeof(T) == 1u &&
				  ::fast_io::details::is_ascii<char_type>)
	{
		auto const second{first + 1u};
		if (second == last) [[unlikely]]
		{
			if constexpr (my_signed_integral<T>)
			{
				t = sign
						? static_cast<T>(static_cast<unsigned_type>(0) -
										 static_cast<unsigned_type>(first_digit))
						: static_cast<T>(first_digit);
			}
			else
			{
				t = static_cast<T>(first_digit);
			}
			return {second, parse_code::ok};
		}
		auto second_digit{static_cast<unsigned_char_type>(*second)};
		if (char_digit_to_literal<base, char_type>(second_digit)) [[unlikely]]
		{
			if constexpr (my_signed_integral<T>)
			{
				t = sign
						? static_cast<T>(static_cast<unsigned_type>(0) -
										 static_cast<unsigned_type>(first_digit))
						: static_cast<T>(first_digit);
			}
			else
			{
				t = static_cast<T>(first_digit);
			}
			return {second, parse_code::ok};
		}
		auto const iter{second + 1u};
		if (iter != last &&
			char_is_digit<base, char_type>(static_cast<unsigned_char_type>(*iter))) [[unlikely]]
		{
			return {skip_digits<base>(iter, last), parse_code::overflow};
		}
		auto const value{static_cast<::std::uint_least16_t>(first_digit) * base +
						 static_cast<::std::uint_least16_t>(second_digit)};
		constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
		if constexpr (my_signed_integral<T>)
		{
			constexpr unsigned_type imax{umax >> 1u};
			if (value > static_cast<::std::uint_least16_t>(imax + sign)) [[unlikely]]
			{
				return {iter, parse_code::overflow};
			}
			t = sign
					? static_cast<T>(static_cast<unsigned_type>(0) -
									 static_cast<unsigned_type>(value))
					: static_cast<T>(static_cast<unsigned_type>(value));
		}
		else
		{
			if (value > static_cast<::std::uint_least16_t>(umax)) [[unlikely]]
			{
				return {iter, parse_code::overflow};
			}
			t = static_cast<T>(value);
		}
		return {iter, parse_code::ok};
	}
	if constexpr (17u <= base && sizeof(T) == 2u &&
				  ::fast_io::details::is_ascii<char_type>)
	{
		::std::uint_least32_t value{static_cast<::std::uint_least32_t>(first_digit)};
		auto iter{first + 1u};
		/*
		Remembering an observed non-digit avoids a duplicate lookup before the
		overflow decision.  Measured GCC 13--16 unsigned 16-bit exact-range
		specializations regressed when carrying that state, while the signed layout
		retained it.  Other non-Clang GNU-compatible frontends inherit this split and
		require revalidation; other compilers retain the state unconditionally.  The
		false arm is semantically equivalent: it repeats char_is_digit at the same
		input position.
		*/
#if defined(__GNUC__) && !defined(__clang__)
		constexpr bool remember_nondigit{my_signed_integral<T>};
#else
		constexpr bool remember_nondigit{true};
#endif
		bool stopped_at_nondigit{};
		for (unsigned digits{1u}; digits != 4u && iter != last; ++digits)
		{
			auto digit{static_cast<unsigned_char_type>(*iter)};
			if (char_digit_to_literal<base, char_type>(digit)) [[unlikely]]
			{
				if constexpr (remember_nondigit)
				{
					stopped_at_nondigit = true;
				}
				break;
			}
			value = value * base + static_cast<::std::uint_least32_t>(digit);
			++iter;
		}
		if ((!remember_nondigit || !stopped_at_nondigit) && iter != last &&
			char_is_digit<base, char_type>(static_cast<unsigned_char_type>(*iter))) [[unlikely]]
		{
			return {skip_digits<base>(iter, last), parse_code::overflow};
		}
		constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
		if constexpr (my_signed_integral<T>)
		{
			constexpr unsigned_type imax{umax >> 1u};
			if (value > static_cast<::std::uint_least32_t>(imax + sign)) [[unlikely]]
			{
				return {iter, parse_code::overflow};
			}
			t = sign
					? static_cast<T>(static_cast<unsigned_type>(0) -
									 static_cast<unsigned_type>(value))
					: static_cast<T>(static_cast<unsigned_type>(value));
		}
		else
		{
			if (value > static_cast<::std::uint_least32_t>(umax)) [[unlikely]]
			{
				return {iter, parse_code::overflow};
			}
			t = static_cast<T>(value);
		}
		return {iter, parse_code::ok};
	}
	/*
	For GCC 15 and newer, unsigned uint32_t in bases 12--15 may contain at most
	nine non-overflowing digits.  The bounded path validates the remaining eight
	digits, then evaluates the same radix polynomial with a balanced pair/quad
	tree and performs the existing terminal/overflow checks.  GCC 13/14, Clang,
	signed, EBCDIC, and nonmatching types use the prior scanner.  Native GCC 15/16
	timing selected this compiler boundary; later GCC releases inherit the policy
	and require revalidation.  llvm-mca describes only the dependency chain.
	*/
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && !defined(__CUDACC__) && \
	__GNUC__ >= 15
	constexpr bool use_bounded_u32_midbase{my_unsigned_integral<T> && 12u <= base && base <= 15u};
#else
	constexpr bool use_bounded_u32_midbase{false};
#endif
	if constexpr (use_bounded_u32_midbase && sizeof(T) == 4u &&
				  ::fast_io::details::is_ascii<char_type>)
	{
		if (9u <= static_cast<::std::size_t>(last - first) &&
			char_is_digit<base, char_type>(static_cast<unsigned_char_type>(first[8u])))
		{
			auto const digit1{static_cast<::std::uint_least64_t>(
				::fast_io::details::sto_ascii_digit_table_lookup<char_type>(
					static_cast<unsigned_char_type>(first[1u])))};
			auto const digit2{static_cast<::std::uint_least64_t>(
				::fast_io::details::sto_ascii_digit_table_lookup<char_type>(
					static_cast<unsigned_char_type>(first[2u])))};
			auto const digit3{static_cast<::std::uint_least64_t>(
				::fast_io::details::sto_ascii_digit_table_lookup<char_type>(
					static_cast<unsigned_char_type>(first[3u])))};
			auto const digit4{static_cast<::std::uint_least64_t>(
				::fast_io::details::sto_ascii_digit_table_lookup<char_type>(
					static_cast<unsigned_char_type>(first[4u])))};
			auto const digit5{static_cast<::std::uint_least64_t>(
				::fast_io::details::sto_ascii_digit_table_lookup<char_type>(
					static_cast<unsigned_char_type>(first[5u])))};
			auto const digit6{static_cast<::std::uint_least64_t>(
				::fast_io::details::sto_ascii_digit_table_lookup<char_type>(
					static_cast<unsigned_char_type>(first[6u])))};
			auto const digit7{static_cast<::std::uint_least64_t>(
				::fast_io::details::sto_ascii_digit_table_lookup<char_type>(
					static_cast<unsigned_char_type>(first[7u])))};
			auto const digit8{static_cast<::std::uint_least64_t>(
				::fast_io::details::sto_ascii_digit_table_lookup<char_type>(
					static_cast<unsigned_char_type>(first[8u])))};
			if ((base <= digit1) | (base <= digit2) | (base <= digit3) | (base <= digit4) |
				(base <= digit5) | (base <= digit6) | (base <= digit7) | (base <= digit8)) [[unlikely]]
			{
				::std::uint_least64_t value{static_cast<::std::uint_least64_t>(first_digit)};
				auto iter{first + 1u};
				for (; iter != first + 9u; ++iter)
				{
					auto digit{static_cast<unsigned_char_type>(*iter)};
					if (char_digit_to_literal<base, char_type>(digit))
					{
						break;
					}
					value = value * base + static_cast<::std::uint_least64_t>(digit);
				}
				t = static_cast<T>(value);
				return {iter, parse_code::ok};
			}
			auto const pair0{static_cast<::std::uint_least64_t>(first_digit) * base + digit1};
			auto const pair1{digit2 * base + digit3};
			auto const pair2{digit4 * base + digit5};
			auto const pair3{digit6 * base + digit7};
			constexpr ::std::uint_least64_t base_squared{base * base};
			constexpr ::std::uint_least64_t base_fourth{base_squared * base_squared};
			auto const quad0{pair0 * base_squared + pair1};
			auto const quad1{pair2 * base_squared + pair3};
			auto const value{(quad0 * base_fourth + quad1) * base + digit8};
			auto const iter{first + 9u};
			if (iter != last &&
				char_is_digit<base, char_type>(static_cast<unsigned_char_type>(*iter))) [[unlikely]]
			{
				return {skip_digits<base>(iter, last), parse_code::overflow};
			}
			constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
			if (value > static_cast<::std::uint_least64_t>(umax)) [[unlikely]]
			{
				return {iter, parse_code::overflow};
			}
			t = static_cast<T>(value);
			return {iter, parse_code::ok};
		}
	}
	if constexpr (17u <= base && sizeof(T) == 4u &&
				  ::fast_io::details::is_ascii<char_type>)
	{
		// This is the 32-bit member of the bounded high-base family documented at
		// the enclosing native-x86 guard; it inherits that measurement scope,
		// unmeasured-target caveat, and shared-scanner fallback.
		if (!use_bounded_u32_midbase ||
			(9u <= static_cast<::std::size_t>(last - first) &&
			 char_is_digit<base, char_type>(static_cast<unsigned_char_type>(first[8u]))))
		{
			::std::uint_least64_t value{static_cast<::std::uint_least64_t>(first_digit)};
			auto iter{first + 1u};
			bool stopped_at_nondigit{};
			constexpr unsigned digit_limit{use_bounded_u32_midbase ? 9u : 8u};
			for (unsigned digits{1u}; digits != digit_limit && iter != last; ++digits)
			{
				auto digit{static_cast<unsigned_char_type>(*iter)};
				if (char_digit_to_literal<base, char_type>(digit)) [[unlikely]]
				{
					stopped_at_nondigit = true;
					break;
				}
				value = value * base + static_cast<::std::uint_least64_t>(digit);
				++iter;
			}
			if (!stopped_at_nondigit && iter != last &&
				char_is_digit<base, char_type>(static_cast<unsigned_char_type>(*iter))) [[unlikely]]
			{
				return {skip_digits<base>(iter, last), parse_code::overflow};
			}
			constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
			if constexpr (my_signed_integral<T>)
			{
				constexpr unsigned_type imax{umax >> 1u};
				if (value > static_cast<::std::uint_least64_t>(imax + sign)) [[unlikely]]
				{
					return {iter, parse_code::overflow};
				}
				t = sign
						? static_cast<T>(static_cast<unsigned_type>(0) -
										 static_cast<unsigned_type>(value))
						: static_cast<T>(static_cast<unsigned_type>(value));
			}
			else
			{
				if (value > static_cast<::std::uint_least64_t>(umax)) [[unlikely]]
				{
					return {iter, parse_code::overflow};
				}
				t = static_cast<T>(value);
			}
			return {iter, parse_code::ok};
		}
	}
	if constexpr (my_unsigned_integral<T> &&
				  sizeof(unsigned_type) == sizeof(::std::uint_least64_t) &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type> &&
				  (base == 3u || base == 4u || (11u <= base && base <= 16u)))
	{
		constexpr ::std::size_t short_limit{8u};
		auto const remaining{static_cast<::std::size_t>(last - first)};
		if (remaining <= short_limit ||
			(remaining == short_limit + 1u &&
			 !::fast_io::details::char_is_digit<base, char_type>(
				 static_cast<unsigned_char_type>(last[-1])))) [[likely]]
		{
			unsigned_type accumulator{};
			auto iter{first};
			::std::size_t digits{};
			// Keep the two short low-base cases compact.  Other bases in this
			// block benefit from Clang's existing expansion and stay unchanged.
			// The pragma changes unrolling only; current-version isolated native A/B
			// evidence is required before assigning a numeric gain to this factor.
#if defined(__clang__) && defined(__AVX2__) && \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
			if constexpr (base == 3u || base == 4u)
			{
#pragma clang loop unroll_count(2)
				for (; iter != last && digits != short_limit; ++iter, ++digits)
				{
					auto digit{static_cast<unsigned_char_type>(*iter)};
					if (::fast_io::details::char_digit_to_literal<base, char_type>(digit)) [[unlikely]]
					{
						break;
					}
					if constexpr (base == 4u)
					{
						accumulator = static_cast<unsigned_type>((accumulator << 2u) | digit);
					}
					else
					{
						accumulator = static_cast<unsigned_type>(accumulator * base + digit);
					}
				}
			}
			else
#endif
			{
				for (; iter != last && digits != short_limit; ++iter, ++digits)
				{
					auto digit{static_cast<unsigned_char_type>(*iter)};
					if (::fast_io::details::char_digit_to_literal<base, char_type>(digit)) [[unlikely]]
					{
						break;
					}
					if constexpr (base == 4u)
					{
						accumulator = static_cast<unsigned_type>((accumulator << 2u) | digit);
					}
					else
					{
						accumulator = static_cast<unsigned_type>(accumulator * base + digit);
					}
				}
			}
			if (iter == last ||
				!::fast_io::details::char_is_digit<base, char_type>(
					static_cast<unsigned_char_type>(*iter))) [[likely]]
			{
				t = static_cast<T>(accumulator);
				return {iter, parse_code::ok};
			}
		}
	}
	if constexpr (base <= 10u && my_unsigned_integral<T> &&
				  sizeof(unsigned_type) == sizeof(::std::uint_least64_t) &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type>)
	{
		auto const swar_remaining{static_cast<::std::size_t>(last - first)};
		if (!__builtin_is_constant_evaluated() && 4u <= swar_remaining &&
			swar_remaining <= 7u) [[unlikely]]
		{
			::std::uint_least64_t accumulator;
			if (::fast_io::details::scan_int_contiguous_x86_parse_four_digits<base>(
					first, accumulator)) [[likely]]
			{
				auto iter{first + 4u};
				// This path accepts at most seven characters, so no more than three
				// remain after the four-digit SWAR conversion.
				// The two-way Clang AVX2 unroll is layout policy only; the loop bounds
				// and digit/overflow semantics remain unchanged.
#if defined(__clang__) && defined(__AVX2__) && \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
#pragma clang loop unroll_count(2)
#endif
				for (; iter != last; ++iter)
				{
					auto digit{static_cast<unsigned_char_type>(*iter)};
					digit -= static_cast<unsigned_char_type>(u8'0');
					if (base <= digit) [[unlikely]]
					{
						break;
					}
					accumulator = accumulator * base + digit;
				}
				t = static_cast<T>(accumulator);
				return {iter, parse_code::ok};
			}
		}
	}
	if constexpr (base == 8u && my_unsigned_integral<T> &&
				  sizeof(unsigned_type) == sizeof(::std::uint_least64_t) &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type>)
	{
		constexpr auto max_digits{
			::fast_io::details::max_int_size_result<unsigned_type, base>};
		auto const swar_eight_remaining{static_cast<::std::size_t>(last - first)};
		if (!__builtin_is_constant_evaluated() && 8u <= swar_eight_remaining &&
			(swar_eight_remaining != 8u ||
			 ::fast_io::details::char_is_digit<base, char_type>(
				 static_cast<unsigned_char_type>(last[-1]))) &&
			(swar_eight_remaining < max_digits ||
			 (swar_eight_remaining == max_digits &&
			  !::fast_io::details::char_is_digit<base, char_type>(
				  static_cast<unsigned_char_type>(last[-1]))))) [[likely]]
		{
			::std::uint_least64_t high;
			::std::uint_least64_t low;
			if (::fast_io::details::scan_int_contiguous_x86_parse_four_digits<base>(first, high) &&
				::fast_io::details::scan_int_contiguous_x86_parse_four_digits<base>(first + 4u, low)) [[likely]]
			{
				constexpr auto base_squared{static_cast<::std::uint_least64_t>(base * base)};
				constexpr auto base_fourth{base_squared * base_squared};
				::std::uint_least64_t accumulator{high * base_fourth + low};
				auto iter{first + 8u};
				for (; iter != last; ++iter)
				{
					auto digit{static_cast<unsigned_char_type>(*iter)};
					digit -= static_cast<unsigned_char_type>(u8'0');
					if (base <= digit) [[unlikely]]
					{
						break;
					}
					accumulator = accumulator * base + digit;
				}
				t = static_cast<T>(accumulator);
				return {iter, parse_code::ok};
			}
		}
	}
	// GCC duplicates this base-dependent SIMD graph aggressively; its smaller
	// scalar graph is faster once the complete base 2--36 scanner is instantiated.
	// Native full matrices for GCC 13--16 and 17.8--22.0% linked-text reductions
	// select this compiler split.  Earlier GCC and other admitted non-Clang
	// GNU-compatible frontends inherit it conservatively without native evidence;
	// later versions also require revalidation.  llvm-mca showed the isolated SIMD
	// arithmetic remained competitive, so it is not evidence for the whole-call
	// decision.
#if defined(__SSE4_1__) && \
	!(defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && !defined(__CUDACC__))
	if constexpr ((base == 5u || base == 6u || base == 7u || base == 9u ||
				   base == 14u || 16u <= base) &&
				  my_unsigned_integral<T> &&
				  sizeof(unsigned_type) == sizeof(::std::uint_least64_t) &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type>)
	{
		constexpr auto max_digits{
			::fast_io::details::max_int_size_result<unsigned_type, base>};
		auto const remaining{static_cast<::std::size_t>(last - first)};
		if (!__builtin_is_constant_evaluated() && 8u <= remaining &&
			(remaining != 8u ||
			 ::fast_io::details::char_is_digit<base, char_type>(
				 static_cast<unsigned_char_type>(last[-1]))) &&
			(remaining < max_digits ||
			 (remaining == max_digits &&
			  !::fast_io::details::char_is_digit<base, char_type>(
				  static_cast<unsigned_char_type>(last[-1]))))) [[likely]]
		{
			::std::uint_least64_t accumulator;
			if (::fast_io::details::scan_int_contiguous_x86_sse_parse_eight<base>(
					first, accumulator)) [[likely]]
			{
				auto iter{first + 8u};
				// This Clang AVX2 factor controls only the scalar tail after an
				// already validated SIMD prefix.  No isolated native A/B measurement
				// currently supports a speedup claim for this factor; it remains a
				// code-layout policy pending revalidation.
#if defined(__clang__) && defined(__AVX2__) && \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
				if constexpr (16u <= base)
				{
#pragma clang loop unroll_count(4)
					for (; iter != last; ++iter)
					{
						auto const digit{
							::fast_io::details::sto_ascii_digit_table_lookup<char_type>(
								static_cast<unsigned_char_type>(*iter))};
						if (base <= digit) [[unlikely]]
						{
							break;
						}
						accumulator = accumulator * base + digit;
					}
				}
				else
#endif
				{
					for (; iter != last; ++iter)
					{
						auto const digit{
							::fast_io::details::sto_ascii_digit_table_lookup<char_type>(
								static_cast<unsigned_char_type>(*iter))};
						if (base <= digit) [[unlikely]]
						{
							break;
						}
						accumulator = accumulator * base + digit;
					}
				}
				t = static_cast<T>(accumulator);
				return {iter, parse_code::ok};
			}
		}
	}
#endif
#endif
	if constexpr (base <= 16 && sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type> &&
				  sizeof(unsigned_type) <= sizeof(::std::uint_least64_t)
#if defined(__aarch64__) || defined(_M_ARM64)
				  && !(base == 10u && my_signed_integral<T>)
#endif
	)
	{
		constexpr bool inline_nonoverflowing_alnum{
			10u < base && base < 16u &&
			sizeof(unsigned_type) == sizeof(::std::uint_least64_t)};
		constexpr ::std::size_t default_inline_limit{inline_nonoverflowing_alnum
														 ? ::fast_io::details::max_int_size_result<unsigned_type, base> - 1u
														 : 8u};
	/*
	Clang AArch64 extends selected bounded short scans to at most nine input
	digits.  For the selected bases B <= 10, so the temporary is below 10^9 and
	cannot overflow uint64_t; the destination range is still checked after the
	loop.  The limit changes only where the same validation and radix recurrence
	are performed.  Native M4 and cross-target assembly retained the policy;
	traditional-core conclusions remain static rather than native.
	*/
#if (defined(__aarch64__) || defined(_M_ARM64)) && defined(__clang__)
		constexpr ::std::size_t inline_limit{
			base == 2u || (5u <= base && base <= 10u) ? 9u : default_inline_limit};
#else
		constexpr ::std::size_t inline_limit{default_inline_limit};
#endif
	/*
	Targets admitted by this x86-64 macro guard have bounded base-8/base-16 short
	forms after first_digit has already been validated.  Every unrolled step
	validates its next digit before committing the accumulator; unsigned shifts
	or multiplies therefore reproduce the scalar radix recurrence.  The one- to
	four-digit early returns fit even signed 64-bit T because 16^4-1 < 2^63;
	later bounded paths retain the shared signed range check.  Other architectures
	use the equivalent generic short loop.

	No retained artifact isolates a positive native A/B for this expanded base-8/
	base-16 layout.  It is a conservative legacy code-generation policy pending
	native revalidation, and admitted but unmeasured targets inherit no numeric
	performance claim.  The native-x86 guard excludes ARM64EC even though these
	scalar operations require no x86 ISA instruction; the generic bounded short
	loop below is the exact fallback.
	*/
#if (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
		if constexpr ((base == 8u || base == 16u) && sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
		{
			auto const x86_remaining{static_cast<::std::size_t>(last - first)};
			if (x86_remaining == 1u ||
				(1u < x86_remaining &&
				 !char_is_digit<base, char_type>(static_cast<unsigned_char_type>(first[1u]))))
			{
				if constexpr (my_signed_integral<T>)
				{
					if (sign)
					{
						t = static_cast<T>(static_cast<unsigned_type>(0) - static_cast<unsigned_type>(first_digit));
					}
					else
					{
						t = static_cast<T>(first_digit);
					}
				}
				else
				{
					t = static_cast<T>(first_digit);
				}
				return {first + 1u, parse_code::ok};
			}
			if constexpr (base == 8u)
			{
				if (2u < x86_remaining &&
					!char_is_digit<8u, char_type>(static_cast<unsigned_char_type>(first[2u])))
				{
					auto digit{static_cast<unsigned_char_type>(first[1u])};
					digit -= static_cast<unsigned_char_type>(u8'0');
					auto short_value{static_cast<::std::uint_least64_t>(first_digit) * 8u + digit};
					if constexpr (my_signed_integral<T>)
					{
						if (sign)
						{
							t = static_cast<T>(static_cast<unsigned_type>(0) - static_cast<unsigned_type>(short_value));
						}
						else
						{
							t = static_cast<T>(short_value);
						}
					}
					else
					{
						t = static_cast<T>(short_value);
					}
					return {first + 2u, parse_code::ok};
				}
			}
			if (4u < x86_remaining &&
				!char_is_digit<base, char_type>(static_cast<unsigned_char_type>(first[4u])))
			{
				::std::uint_least64_t short_value{static_cast<::std::uint_least64_t>(first_digit)};
				auto short_iter{first + 1u};
				do
				{
					unsigned_char_type digit{static_cast<unsigned_char_type>(*short_iter)};
					if constexpr (base == 8u)
					{
						digit -= static_cast<unsigned_char_type>(u8'0');
						auto const next_value{short_value * 8u + digit};
						if (7u < digit)
						{
							break;
						}
						short_value = next_value;
					}
					else
					{
						if (char_digit_to_literal<base, char_type>(digit))
						{
							break;
						}
						short_value = (short_value << 4u) | digit;
					}
					++short_iter;
					digit = static_cast<unsigned_char_type>(*short_iter);
					if constexpr (base == 8u)
					{
						digit -= static_cast<unsigned_char_type>(u8'0');
						auto const next_value{short_value * 8u + digit};
						if (7u < digit)
						{
							break;
						}
						short_value = next_value;
					}
					else
					{
						if (char_digit_to_literal<base, char_type>(digit))
						{
							break;
						}
						short_value = (short_value << 4u) | digit;
					}
					++short_iter;
					digit = static_cast<unsigned_char_type>(*short_iter);
					if constexpr (base == 8u)
					{
						digit -= static_cast<unsigned_char_type>(u8'0');
						auto const next_value{short_value * 8u + digit};
						if (7u < digit)
						{
							break;
						}
						short_value = next_value;
					}
					else
					{
						if (char_digit_to_literal<base, char_type>(digit))
						{
							break;
						}
						short_value = (short_value << 4u) | digit;
					}
					++short_iter;
				} while (false);
				if constexpr (my_signed_integral<T>)
				{
					if (sign)
					{
						t = static_cast<T>(static_cast<unsigned_type>(0) - static_cast<unsigned_type>(short_value));
					}
					else
					{
						t = static_cast<T>(short_value);
					}
				}
				else
				{
					t = static_cast<T>(short_value);
				}
				return {short_iter, parse_code::ok};
			}
			if constexpr (base == 8u)
			{
				if (6u < x86_remaining &&
					!char_is_digit<8u, char_type>(static_cast<unsigned_char_type>(first[6u])))
				{
					last = first + 6u;
				}
				else if (7u < x86_remaining &&
						 !char_is_digit<8u, char_type>(static_cast<unsigned_char_type>(first[7u])))
				{
					last = first + 7u;
				}
			}
		}
#endif
	/*
	On targets admitted by this x86-64 macro guard, inspecting the code unit at
	inline_limit can prove that a longer range actually terminates there.
	Truncating last is then exact, because that code unit is known not to be a
	digit.  SSE4.1 may reduce an eight-hex-digit unsigned prefix.  The admitted
	non-Clang GNU-compatible branch deliberately keeps signed hexadecimal on the
	scalar graph: native GCC 15 affected points and assembly showed the former SSE
	layout regressed, while the unsigned specialization remains eligible.  Other
	GCC versions and compatible frontends inherit that policy without a native
	performance claim and require revalidation.
	*/
#if (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
		{
			if (inline_limit < static_cast<::std::size_t>(last - first) &&
				!char_is_digit<base, char_type>(static_cast<unsigned_char_type>(first[inline_limit])))
			{
#if defined(__SSE4_1__) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && !defined(__CUDACC__)
				constexpr bool use_sse_hex8{my_unsigned_integral<T>};
#else
				constexpr bool use_sse_hex8{true};
#endif
				if constexpr (use_sse_hex8 && base == 16u &&
							  sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
				{
					return ::fast_io::details::scan_int_contiguous_x86_sse_hex8_space_part_define_impl(
						first, t, sign);
				}
#endif
				last = first + inline_limit;
			}
		}
#endif
		if (static_cast<::std::size_t>(last - first) <= inline_limit) [[likely]]
		{
			::std::uint_least64_t short_value{static_cast<::std::uint_least64_t>(first_digit)};
			auto short_iter{first + 1};
			/*
			x86 spells the bounded octal recurrence as explicit groups.  Each digit is
			range-checked before committing, so it is equivalent to the generic loop
			and cannot read past last.  No retained report isolates a positive native
			A/B for this second expansion; it is a conservative legacy layout policy
			pending revalidation, and no unmeasured compiler/core receives a speed
			claim.  The ordinary loop in the non-x86 arm is the exact fallback.  The
			native-x86 guard excludes ARM64EC, which therefore uses that fallback.
			*/
#if (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
			if constexpr (base == 8u)
			{
				do
				{
					if (short_iter == last)
					{
						break;
					}
					unsigned_char_type digit{static_cast<unsigned_char_type>(*short_iter)};
					digit -= static_cast<unsigned_char_type>(u8'0');
					auto next_value{short_value * 8u + digit};
					if (7u < digit)
					{
						break;
					}
					short_value = next_value;
					++short_iter;
					if (short_iter == last)
					{
						break;
					}
					digit = static_cast<unsigned_char_type>(*short_iter);
					digit -= static_cast<unsigned_char_type>(u8'0');
					next_value = short_value * 8u + digit;
					if (7u < digit)
					{
						break;
					}
					short_value = next_value;
					++short_iter;
					if (short_iter == last)
					{
						break;
					}
					digit = static_cast<unsigned_char_type>(*short_iter);
					digit -= static_cast<unsigned_char_type>(u8'0');
					next_value = short_value * 8u + digit;
					if (7u < digit)
					{
						break;
					}
					short_value = next_value;
					++short_iter;
					if (short_iter == last)
					{
						break;
					}
					digit = static_cast<unsigned_char_type>(*short_iter);
					digit -= static_cast<unsigned_char_type>(u8'0');
					next_value = short_value * 8u + digit;
					if (7u < digit)
					{
						break;
					}
					short_value = next_value;
					++short_iter;
					if (short_iter == last)
					{
						break;
					}
					digit = static_cast<unsigned_char_type>(*short_iter);
					digit -= static_cast<unsigned_char_type>(u8'0');
					next_value = short_value * 8u + digit;
					if (7u < digit)
					{
						break;
					}
					short_value = next_value;
					++short_iter;
					if (short_iter == last)
					{
						break;
					}
					digit = static_cast<unsigned_char_type>(*short_iter);
					digit -= static_cast<unsigned_char_type>(u8'0');
					next_value = short_value * 8u + digit;
					if (7u < digit)
					{
						break;
					}
					short_value = next_value;
					++short_iter;
					if (short_iter == last)
					{
						break;
					}
					digit = static_cast<unsigned_char_type>(*short_iter);
					digit -= static_cast<unsigned_char_type>(u8'0');
					next_value = short_value * 8u + digit;
					if (7u < digit)
					{
						break;
					}
					short_value = next_value;
					++short_iter;
				} while (false);
			}
			else
#endif
#if (defined(__aarch64__) || defined(_M_ARM64)) && defined(__clang__)
				if constexpr (base == 2u || (5u <= base && base <= 10u))
			{
				/*
				One digit has already been consumed and inline_limit is nine, so this
				loop performs at most eight validated transitions.  Its end-of-range
				and invalid-digit exits nevertheless make the exact trip count data
				dependent; an unroll(full) contract is therefore not valid.  Clang
				AArch64 emits the same code shape when the rejected hint is absent.
				Keep this loop unforced: unroll_count is not an equivalent contract and
				can inflate the scanner enough to inhibit inlining.
				*/
				for (::std::size_t short_index{1}; short_index != inline_limit; ++short_index)
				{
					if (short_iter == last)
					{
						break;
					}
					unsigned_char_type digit{static_cast<unsigned_char_type>(*short_iter)};
					if (char_digit_to_literal<base, char_type>(digit)) [[unlikely]]
					{
						break;
					}
					if constexpr (base == 2u)
					{
						short_value = (short_value << 1u) | digit;
					}
					else if constexpr (base == 4u)
					{
						short_value = (short_value << 2u) | digit;
					}
					else if constexpr (base == 8u)
					{
						short_value = (short_value << 3u) | digit;
					}
					else if constexpr (base == 16u)
					{
						short_value = (short_value << 4u) | digit;
					}
					else
					{
						short_value = short_value * base + digit;
					}
					++short_iter;
				}
			}
			else
#endif
			{
				// The Clang AVX2 two-way factor changes only loop layout.  Do not
				// infer a current-version speedup without the isolated A/B retest.
#if defined(__clang__) && defined(__AVX2__) && \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
#pragma clang loop unroll_count(2)
#endif
				for (; short_iter != last; ++short_iter)
				{
					unsigned_char_type digit{static_cast<unsigned_char_type>(*short_iter)};
					if (char_digit_to_literal<base, char_type>(digit)) [[unlikely]]
					{
						break;
					}
					if constexpr (base == 2u)
					{
						short_value = (short_value << 1u) | digit;
					}
					else if constexpr (base == 4u)
					{
						short_value = (short_value << 2u) | digit;
					}
					else if constexpr (base == 8u)
					{
						short_value = (short_value << 3u) | digit;
					}
					else if constexpr (base == 16u)
					{
						short_value = (short_value << 4u) | digit;
					}
					else
					{
						short_value = short_value * base + digit;
					}
				}
			}
			constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
			if constexpr (my_signed_integral<T>)
			{
				constexpr unsigned_type imax{umax >> 1};
				if (short_value > static_cast<::std::uint_least64_t>(imax) + sign) [[unlikely]]
				{
					return {short_iter, parse_code::overflow};
				}
				if (sign)
				{
					t = static_cast<T>(static_cast<unsigned_type>(0) - static_cast<unsigned_type>(short_value));
				}
				else
				{
					t = static_cast<T>(short_value);
				}
			}
			else
			{
				if (short_value > static_cast<::std::uint_least64_t>(umax)) [[unlikely]]
				{
					return {short_iter, parse_code::overflow};
				}
				t = static_cast<T>(short_value);
			}
			return {short_iter, parse_code::ok};
		}
	/*
	For at least sixteen remaining octal or hexadecimal digits, SSE4.1 validates
	and reduces one safe full block, then preserves the existing overflow and
	end-pointer checks.  The threshold proves the vector load is in range.  The
	scalar x86 hexadecimal arm below remains a semantically identical fallback
	for shorter or non-SSE configurations.  This call site consumes the scoped
	i9-14900HX GCC 13--16 retention result, single-host limitation, unmeasured-
	target caveat, and shared-scalar fallback documented at the helper family;
	it adds no wider performance claim.
	*/
#if defined(__SSE4_1__) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)))
		if constexpr (base == 8u && sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
		{
			if (16u <= static_cast<::std::size_t>(last - first))
			{
				return ::fast_io::details::scan_int_contiguous_x86_sse_oct16_space_part_define_impl(
					first, last, t, sign);
			}
		}
		if constexpr (base == 16u && sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
		{
			if (16u <= static_cast<::std::size_t>(last - first))
			{
				return ::fast_io::details::scan_int_contiguous_x86_sse_hex16_space_part_define_impl(
					first, last, t, sign);
			}
		}
#endif
	/*
	Targets admitted by this x86-64 macro guard retain an explicitly bounded
	scalar hexadecimal fallback.  Its shift-by-four recurrence is exact after
	char_digit_to_literal validates each nibble; other targets reach the common
	scanner with the same contract.  On the retained i9-14900HX GCC 15 signed-
	u64 hexadecimal study, withholding the former SSE graph reduced the fixed-
	base core from 2916 to 2372 bytes and improved affected points by about
	2.9x--3.3x; the unsigned specialization remained unchanged.  This is one
	compiler/host/range result, not an ISA-wide guarantee.  Other admitted
	frontends and cores inherit only the semantic fallback policy and no numeric
	performance claim. ARM64EC is excluded by the native-x86 guard and uses the
	shared fallback.
	*/
#if (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
		if constexpr (base == 16u && sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
		{
			auto const diff{static_cast<::std::size_t>(last - first)};
			char_type const *last16{};
			if (diff <= 16u)
			{
				last16 = last;
			}
			else if (!char_is_digit<16u, char_type>(static_cast<unsigned_char_type>(first[16u])))
			{
				last16 = first + 16u;
			}
			if (last16 != nullptr)
			{
				::std::uint_least64_t short_value{static_cast<::std::uint_least64_t>(first_digit)};
				auto short_iter{first + 1};
				for (; short_iter != last16; ++short_iter)
				{
					unsigned_char_type digit{static_cast<unsigned_char_type>(*short_iter)};
					if (char_digit_to_literal<16u, char_type>(digit)) [[unlikely]]
					{
						break;
					}
					short_value = (short_value << 4u) | digit;
				}
				if constexpr (my_signed_integral<T>)
				{
					constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
					constexpr unsigned_type imax{umax >> 1};
					if (short_value > static_cast<::std::uint_least64_t>(imax) + sign) [[unlikely]]
					{
						return {short_iter, parse_code::overflow};
					}
					if (sign)
					{
						t = static_cast<T>(static_cast<unsigned_type>(0) - static_cast<unsigned_type>(short_value));
					}
					else
					{
						t = static_cast<T>(short_value);
					}
				}
				else
				{
					t = static_cast<T>(short_value);
				}
				return {short_iter, parse_code::ok};
			}
		}
#endif
	}
	/*
	GCC 15+ on targets admitted by this x86-64 macro guard uses a nine-digit
	unsigned-decimal SWAR kernel.  The first eight bytes are validated in parallel
	and reduced as pairs, quads, and one eight-digit value; the independently
	validated ninth digit completes high*10+last.  Nine decimal digits fit
	uint32_t, while a tenth digit and all other lengths stay on the overflow-aware
	path.  Native GCC 15/16 timing and assembly select this compiler boundary;
	later GCC releases inherit the policy and require revalidation.  llvm-mca
	covers only the arithmetic region, not whole-parser latency.
	*/
#if defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && !defined(__CUDACC__) && \
	__GNUC__ >= 15 &&                                                                                 \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	// GCC's generic overflow graph is expensive for nine-digit uint32_t
	// decimal input.  Keep this after the existing short-input returns so the
	// extra SWAR graph does not sit on their control-flow path.
	if constexpr (base == 10u && my_unsigned_integral<T> && sizeof(T) == 4u &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type>)
	{
		auto const decimal_remaining{static_cast<::std::size_t>(last - first)};
		if (!__builtin_is_constant_evaluated() && 9u <= decimal_remaining &&
			char_is_digit<base, char_type>(static_cast<unsigned_char_type>(first[8u])) &&
			(decimal_remaining == 9u ||
			 !char_is_digit<base, char_type>(static_cast<unsigned_char_type>(first[9u]))))
		{
			::std::uint_least64_t chunk;
			__builtin_memcpy(__builtin_addressof(chunk), first, sizeof(chunk));
			chunk -= UINT64_C(0x3030303030303030);
			if ((chunk & UINT64_C(0xf0f0f0f0f0f0f0f0)) == 0u &&
				((chunk + UINT64_C(0x0606060606060606)) &
				 UINT64_C(0x1010101010101010)) == 0u) [[likely]]
			{
				auto const pairs{(chunk * 10u + (chunk >> 8u)) &
								 UINT64_C(0x00ff00ff00ff00ff)};
				auto const quads{(pairs * 100u + (pairs >> 16u)) &
								 UINT64_C(0x0000ffff0000ffff)};
				auto const high{static_cast<::std::uint_least32_t>(
					(quads * 10000u + (quads >> 32u)) & UINT64_C(0xffffffff))};
				auto const last_digit{static_cast<unsigned_char_type>(first[8u]) -
									  static_cast<unsigned_char_type>(0x30u)};
				t = static_cast<T>(high * 10u + last_digit);
				return {first + 9u, parse_code::ok};
			}
		}
	}
#endif
	/*
	AArch64 routes 16--19 decimal digits to the proved AdvSIMD helper above, then
	validates any scalar suffix.  Exact 20-digit input stays on the dedicated
	overflow-aware 8+8+4 path, and the terminated 15-digit shape avoids a failed
	vector attempt.  Every other shape falls through unchanged.  Native M4 and
	cross-model llvm-mca evidence are reported separately.  Other AArch64 cores
	and admitted frontend versions inherit the semantically proved route without
	a native performance claim.
	*/
#if (defined(__aarch64__) || defined(__arm64__)) &&                     \
	(defined(__clang__) || (defined(__GNUC__) && !defined(__clang__))) && \
	defined(__ARM_NEON)
	if constexpr (base == 10u && my_unsigned_integral<T> &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type> &&
				  sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
	{
		auto const remaining{static_cast<::std::size_t>(last - first)};
		if (remaining - 16u <= 4u)
		{
			auto const last_is_digit{char_is_digit<10u, char_type>(
				static_cast<unsigned_char_type>(last[-1]))};
			// A terminated 15-digit range and the exact 20-digit SWAR case
			// stay on their existing faster paths.
			if ((remaining != 16u || last_is_digit) &&
				!(remaining == 20u && last_is_digit)) [[unlikely]]
			{
				::std::uint_least64_t value;
				if (::fast_io::details::aarch64_builtin_parse_16_decimal_digits(
						first, value)) [[likely]]
				{
					auto next{first + 16};
					for (; next != last; ++next)
					{
						auto digit{static_cast<unsigned_char_type>(*next)};
						if (char_digit_to_literal<10u, char_type>(digit)) [[unlikely]]
						{
							break;
						}
						value = value * 10u + digit;
					}
					t = static_cast<T>(value);
					return {next, parse_code::ok};
				}
			}
		}
	}
#endif
	unsigned_type res{};
	auto parse_first{first};
	/*
	Either bound proves that at most max_digits - 1 radix digits can be consumed.
	Such a value fits unsigned_type, so the scanner's unsigned accumulator cannot
	wrap; a signed destination is still range-checked after parsing.  Because
	first_digit is already validated and mapped, seeding the accumulator with it
	removes one equivalent scanner iteration.  Other ranges retain the zero-seeded
	overflow path.  This applies to targets admitted by the x86-64 macro guard.

	The retained reports do not isolate this seed from the surrounding x86 parser,
	so the selection is a conservative legacy code-generation policy pending
	native revalidation.  Unmeasured x86 frontends/cores inherit only the no-wrap
	proof, not a numeric speed claim. ARM64EC is excluded by the native-x86 guard;
	the zero-seeded shared scanner is its exact fallback.
	*/
#if (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	if constexpr (sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type> &&
				  sizeof(unsigned_type) <= sizeof(::std::uint_least64_t) &&
				  (sizeof(T) >= sizeof(::std::uint_least32_t) ||
				   (::fast_io::details::my_signed_integral<T> &&
					sizeof(T) >= sizeof(::std::uint_least16_t))))
	{
		constexpr ::std::size_t max_digits{
			::fast_io::details::max_int_size_result<unsigned_type, base>};
		auto const remaining{static_cast<::std::size_t>(last - first)};
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
#endif
		if (remaining < max_digits ||
			(remaining == max_digits &&
			 !char_is_digit<base, char_type>(
				 static_cast<unsigned_char_type>(last[-1])))) [[likely]]
		{
			res = static_cast<unsigned_type>(first_digit);
			parse_first = first + 1u;
		}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
	}
#endif
	/*
	The selected AArch64 unsigned bases use the same proved first-digit seed when
	the range is strictly shorter than the maximum.  That bound proves overflow is
	impossible and the general loop computes the identical radix recurrence.  The
	selection is shared AArch64 because Apple-Clang cross-target assembly for the
	inspected traditional cores also retained the removed iteration.  Other
	AArch64 compiler lowerings inherit the semantically equivalent seed; this is
	not a native traditional-core or unmeasured-compiler timing claim.
	*/
#if defined(__aarch64__) || defined(_M_ARM64)
	if constexpr (((((5u <= base && base <= 9u) || 16u < base) &&
					my_unsigned_integral<T>) ||
				   (base == 10u && my_signed_integral<T>)) &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type> &&
				  sizeof(unsigned_type) == sizeof(::std::uint_least64_t))
	{
		constexpr ::std::size_t max_digits{
			::fast_io::details::max_int_size_result<unsigned_type, base>};
		if (static_cast<::std::size_t>(last - first) < max_digits) [[likely]]
		{
			// The first digit is already validated and mapped above. Starting the
			// AArch64 accumulator with it removes one table load and one loop trip.
			// The unsigned-width bound also covers a signed destination's accumulator;
			// its narrower positive/negative limit remains checked after parsing.
			res = static_cast<unsigned_type>(first_digit);
			parse_first = first + 1;
		}
	}
#endif
	char_type const *it;
	/*
	For at least 32 one-byte decimal code units, native x86 SSE4.1 amortizes its
	16-byte validation/reduction blocks.  Constant evaluation cannot execute the
	target builtins and therefore uses the scalar scanner.  The SIMD result is
	range-checked before narrowing, preserving overflow and returned-pointer
	semantics for smaller destination types.  The threshold also proves every
	vector load used by the entry block is in range.  The family-level comment
	above records the i9-14900HX whole-matrix scope, Haswell-through-Zen4 static
	llvm-mca scope, lack of an isolated threshold A/B, and the no-claim rule for
	unmeasured targets.  Shorter, constexpr, and non-SSE inputs call
	scan_int_contiguous_none_simd_space_part_define_impl as the exact fallback.
	*/
#if defined(__SSE4_1__) && ((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC)))
	if constexpr (base == 10 && sizeof(char_type) == 1 &&
				  (::fast_io::details::is_ascii<char_type> ||
				   ::fast_io::details::is_ebcdic<char_type>) &&
				  sizeof(unsigned_type) <= sizeof(::std::uint_least64_t))
	{
		if (
#if __cpp_lib_is_constant_evaluated >= 201811L
			!__builtin_is_constant_evaluated() &&
#endif
			last - first >= 32) [[likely]]
		{
			constexpr bool smaller_than_uint64{sizeof(unsigned_type) < sizeof(::std::uint_least64_t)};
			::std::uint_least64_t temp{};
			auto [digits, ec] = sse_parse<is_ebcdic<char_type>, smaller_than_uint64>(
				reinterpret_cast<char unsigned const *>(first), reinterpret_cast<char unsigned const *>(last), temp);
			it = first + digits;
			if (ec != parse_code::ok) [[unlikely]]
			{
				return {it, ec};
			}
			if constexpr (smaller_than_uint64)
			{
				constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
				if (temp > umax) [[unlikely]]
				{
					return {it, parse_code::overflow};
				}
				res = static_cast<unsigned_type>(temp);
			}
			else
			{
				res = temp;
			}
		}
		else [[unlikely]]
		{
			auto [it2, ec] = scan_int_contiguous_none_simd_space_part_define_impl<base>(parse_first, last, res);
			if (ec != parse_code::ok) [[unlikely]]
			{
				return {it2, ec};
			}
			it = it2;
		}
	}
	else
#endif
	{
		auto [it2, ec] = scan_int_contiguous_none_simd_space_part_define_impl<base>(parse_first, last, res);
		if (ec != parse_code::ok) [[unlikely]]
		{
			return {it2, ec};
		}
		it = it2;
	}
	if constexpr (my_signed_integral<T>)
	{
		constexpr unsigned_type umax{static_cast<unsigned_type>(-1)};
		constexpr unsigned_type imax{umax >> 1};
		if (res > (static_cast<my_make_unsigned_t<T>>(imax) + sign)) [[unlikely]]
		{
			return {it, parse_code::overflow};
		}
		if (sign)
		{
			t = static_cast<T>(static_cast<unsigned_type>(0) - res);
		}
		else
		{
			t = static_cast<T>(res);
		}
	}
	else
	{
		t = res;
	}
	return {it, parse_code::ok};
}

/// @brief Places the shared run-time decimal core behind the GCC long-token boundary.
/// @details The caller has already proved a nonzero leading decimal digit. This helper forwards the complete original
///          range to the ordinary overflow-aware core and publishes its value only on `ok`, exactly matching the
///          inlined continuation. GCC otherwise absorbs that complete core after the short-path SIMD branch is removed,
///          recreating a large stack-protected caller; one shared leaf keeps the bounded recurrence hot. Clang retains
///          its own profitable placement because the no-inline policy is restricted to measured GCC x86 releases.
template <::std::integral char_type, my_unsigned_integral T>
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 14 && \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
[[gnu::noinline]]
#endif
inline constexpr parse_result<char_type const *>
scan_int_contiguous_unsigned_decimal_runtime_dispatch(
	char_type const *first, char_type const *last, T &target) noexcept
{
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	unsigned_type value{};
	auto const result{
		scan_int_contiguous_none_simd_space_part_define_impl<10u>(
			first, last, value)};
	if (result.code == parse_code::ok) [[likely]]
	{
		target = static_cast<T>(value);
	}
	return result;
}

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 16 &&  \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
/// @brief Parses the exact sixteen- through nineteen-digit decimal domain for GCC 16 and later.
/// @details The caller proves a nonzero ASCII uint64 token and a readable extent in [16, 19]. Two independently
///          validated groups form a value below 10^16; at most three checked suffix digits keep every intermediate
///          below 10^19 and therefore below UINT64_MAX. Isolating the second group prevents its constants and register
///          pressure from entering the 8--11 digit leaf. A failed proof replays the complete original range through the
///          overflow-aware scanner, retaining cursor, status, and value-publication semantics.
template <::std::integral char_type, my_unsigned_integral T>
[[gnu::noinline]] inline constexpr parse_result<char_type const *>
scan_int_contiguous_unsigned_decimal_sixteen_dispatch(
	char_type const *first, char_type const *last, T &target) noexcept
{
	FAST_IO_IF_NOT_CONSTEVAL
	{
		::std::uint_least64_t high{};
		::std::uint_least64_t low{};
		if (scan_int_contiguous_x86_parse_eight_decimal_digits(first, high) &&
			scan_int_contiguous_x86_parse_eight_decimal_digits(first + 8u, low)) [[likely]]
		{
			auto value{high * UINT64_C(100000000) + low};
			auto const remaining{static_cast<::std::size_t>(last - first - 16u)};
			auto const current{first + 16u};
			if (remaining != 0u)
			{
				auto digit{static_cast<::std::make_unsigned_t<char_type>>(*current)};
				if (char_digit_to_literal<10u, char_type>(digit)) [[unlikely]]
				{
					return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
						first, last, target);
				}
				value = value * 10u + digit;
			}
			if (2u <= remaining)
			{
				auto digit{static_cast<::std::make_unsigned_t<char_type>>(current[1u])};
				if (char_digit_to_literal<10u, char_type>(digit)) [[unlikely]]
				{
					return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
						first, last, target);
				}
				value = value * 10u + digit;
			}
			if (3u <= remaining)
			{
				auto digit{static_cast<::std::make_unsigned_t<char_type>>(current[2u])};
				if (char_digit_to_literal<10u, char_type>(digit)) [[unlikely]]
				{
					return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
						first, last, target);
				}
				value = value * 10u + digit;
			}
			target = static_cast<T>(value);
			return {last, parse_code::ok};
		}
	}
	return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
		first, last, target);
}
#endif

#if (defined(__GNUC__) || defined(__clang__)) &&                     \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
/// @brief Parses an exact 4--19 digit unsigned decimal range in bounded groups.
/// @details The caller has already proved that the first code unit is a nonzero decimal digit. Every complete eight-
///          or four-byte load is range-bounded by the local remaining count and validates all of its lanes before its
///          polynomial is observed. At most nineteen digits are accumulated, so the uint64 recurrence cannot overflow.
///          A failed group or suffix proof delegates the complete original range to the shared scanner, preserving the
///          first-delimiter cursor and partial-prefix publication contract. Outlining keeps this graph out of one- to
///          three-digit GCC callers and one- to eight-digit Clang callers. GCC 14 and later route exact 12--15 digit
///          ranges through a fixed 8+4 leaf. GCC 16 and later additionally isolate [16, 19]; GCC 14--15 retain the
///          unified bounded graph because splitting it caused repeatable caller-dependent front-end regressions.
template <::std::integral char_type, my_unsigned_integral T>
[[gnu::noinline]]
inline constexpr parse_result<char_type const *>
scan_int_contiguous_unsigned_decimal_bounded_dispatch(
	char_type const *first, char_type const *last, T &target) noexcept
{
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	if constexpr (sizeof(unsigned_type) == sizeof(::std::uint_least64_t) &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type>)
	{
		FAST_IO_IF_NOT_CONSTEVAL
		{
			auto remaining{static_cast<::std::size_t>(last - first)};
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 16
			/* The sole call site proves an exact extent inside [4, 19] before
			   entering this leaf. Rechecking that contract here added redundant
			   range arithmetic and a conditional branch to every GCC token; all
			   failed byte proofs still replay the original range through the
			   overflow-aware scanner. */
			{
				if (16u <= remaining)
				{
					return scan_int_contiguous_unsigned_decimal_sixteen_dispatch(
						first, last, target);
				}
#else
			if (4u <= remaining && remaining <= 19u) [[likely]]
			{
#endif
				auto current{first};
				::std::uint_least64_t value{};
				::std::uint_least64_t group{};
				if (8u <= remaining)
				{
					if (!scan_int_contiguous_x86_parse_eight_decimal_selected(
							current, group)) [[unlikely]]
					{
						return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
							first, last, target);
					}
					value = group;
					current += 8u;
					remaining -= 8u;
				}
#if !(defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 16)
				if (8u <= remaining)
				{
					if (!scan_int_contiguous_x86_parse_eight_decimal_selected(
							current, group)) [[unlikely]]
					{
						return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
							first, last, target);
					}
					value = value * UINT64_C(100000000) + group;
					current += 8u;
					remaining -= 8u;
				}
#endif
				if (4u <= remaining)
				{
					if (!scan_int_contiguous_x86_parse_four_digits<10u>(
							current, group)) [[unlikely]]
					{
						return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
							first, last, target);
					}
					value = value * UINT64_C(10000) + group;
					current += 4u;
					remaining -= 4u;
				}
				if (remaining != 0u)
				{
					auto digit{static_cast<::std::make_unsigned_t<char_type>>(
						*current)};
					if (char_digit_to_literal<10u, char_type>(digit)) [[unlikely]]
					{
						return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
							first, last, target);
					}
					value = value * 10u + digit;
				}
				if (2u <= remaining)
				{
					auto digit{static_cast<::std::make_unsigned_t<char_type>>(
						current[1u])};
					if (char_digit_to_literal<10u, char_type>(digit)) [[unlikely]]
					{
						return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
							first, last, target);
					}
					value = value * 10u + digit;
				}
				if (3u <= remaining)
				{
					auto digit{static_cast<::std::make_unsigned_t<char_type>>(
						current[2u])};
					if (char_digit_to_literal<10u, char_type>(digit)) [[unlikely]]
					{
						return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
							first, last, target);
					}
					value = value * 10u + digit;
				}
				target = static_cast<T>(value);
				return {last, parse_code::ok};
			}
		}
	}
	return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
		first, last, target);
}
#endif

#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 14 &&  \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
/// @brief Gives GCC an exact 8+4 prefix for twelve- through fifteen-digit decimal ranges.
/// @details The caller proves an ASCII uint64 target, a nonzero leading digit, and a readable extent in [12, 15]. The
///          validated eight- and four-byte groups form at most a twelve-digit value; at most three individually checked
///          suffix digits extend it to less than 10^15, so every intermediate is representable. Native GCC 14--16 keeps
///          the generic bounded leaf's two optional group branches on this path; this fixed-shape leaf removes them.
///          Any validation failure replays the complete range through the shared scanner, retaining delimiter cursors,
///          overflow codes, and value-publication semantics.
template <::std::integral char_type, my_unsigned_integral T>
[[gnu::noinline]] inline constexpr parse_result<char_type const *>
scan_int_contiguous_unsigned_decimal_twelve_dispatch(
	char_type const *first, char_type const *last, T &target) noexcept
{
	FAST_IO_IF_NOT_CONSTEVAL
	{
		::std::uint_least64_t high{};
		::std::uint_least64_t low{};
		if (scan_int_contiguous_x86_parse_eight_decimal_digits(first, high) &&
			scan_int_contiguous_x86_parse_four_digits<10u>(first + 8u, low)) [[likely]]
		{
			auto value{high * UINT64_C(10000) + low};
			auto const remaining{static_cast<::std::size_t>(last - first - 12u)};
			auto const current{first + 12u};
			if (remaining != 0u)
			{
				auto digit{static_cast<::std::make_unsigned_t<char_type>>(*current)};
				if (char_digit_to_literal<10u, char_type>(digit)) [[unlikely]]
				{
					return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
						first, last, target);
				}
				value = value * 10u + digit;
			}
			if (2u <= remaining)
			{
				auto digit{static_cast<::std::make_unsigned_t<char_type>>(current[1u])};
				if (char_digit_to_literal<10u, char_type>(digit)) [[unlikely]]
				{
					return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
						first, last, target);
				}
				value = value * 10u + digit;
			}
			if (3u <= remaining)
			{
				auto digit{static_cast<::std::make_unsigned_t<char_type>>(current[2u])};
				if (char_digit_to_literal<10u, char_type>(digit)) [[unlikely]]
				{
					return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
						first, last, target);
				}
				value = value * 10u + digit;
			}
			target = static_cast<T>(value);
			return {last, parse_code::ok};
		}
	}
	return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
		first, last, target);
}
#endif

/// @brief Parses the exact twenty-digit unsigned decimal domain in an isolated x86 leaf.
/// @details Keeping this fixed-width kernel separate prevents its three checked reductions and boundary arithmetic from
///          entering ordinary 4--19 digit callers. Constant evaluation and any failed block proof delegate to the same
///          run-time-dispatch abstraction, so invalid-character cursors and all non-native executions retain the shared
///          scanner semantics.
template <::std::integral char_type, my_unsigned_integral T>
#if (defined(__GNUC__) || defined(__clang__)) &&                     \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
[[gnu::noinline]]
#endif
inline constexpr parse_result<char_type const *>
scan_int_contiguous_unsigned_decimal_twenty_dispatch(
	char_type const *first, char_type const *last, T &target) noexcept
{
#if (defined(__GNUC__) || defined(__clang__)) &&                     \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	if constexpr (sizeof(unsigned_type) == sizeof(::std::uint_least64_t) &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type>)
	{
		FAST_IO_IF_NOT_CONSTEVAL
		{
			::std::uint_least64_t groups[3u];
			bool const valid{
				scan_int_contiguous_x86_parse_eight_decimal_selected(
					first, groups[0u]) &&
				scan_int_contiguous_x86_parse_eight_decimal_selected(
					first + 8u, groups[1u]) &&
				scan_int_contiguous_x86_parse_four_digits<10u>(
					first + 16u, groups[2u])};
			if (valid) [[likely]]
			{
				/*
				A twenty-digit value is H*10^12+L for the first eight and last
				twelve digits. UINT64_MAX is exactly
				18446744*10^12+73709551615, so lexicographic comparison of the
				pair is equivalent to the scalar overflow predicate. The two
				bounded eight-byte reductions and final four-byte SWAR reduction
				prove every code unit decimal before arithmetic. GCC uses the integer
				SWAR form because its SSE lowering serializes more of this fixed graph;
				Clang uses the smaller SSE form when available. L is below 10^12;
				H*10^12+L is evaluated only after representability is proved.
				*/
				auto const low{groups[1u] * UINT64_C(10000) + groups[2u]};
				constexpr ::std::uint_least64_t maximum_high{UINT64_C(18446744)};
				constexpr ::std::uint_least64_t maximum_low{
					UINT64_C(73709551615)};
				if (maximum_high < groups[0u] ||
					(groups[0u] == maximum_high && maximum_low < low)) [[unlikely]]
				{
					return {last, parse_code::overflow};
				}
				target = static_cast<T>(
					groups[0u] * UINT64_C(1000000000000) + low);
				return {last, parse_code::ok};
			}
		}
	}
#endif
	return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
		first, last, target);
}

/*
The default unsigned 64-bit decimal CPO has a much smaller strategy space than
the general radix scanner.  Keeping it separate prevents the high-base,
signed, prefix, and redundant-zero strategies from affecting its inlining
decision. Long or non-exact ranges still use the shared overflow-aware core;
exact native x86 ranges use bounded grouped leaves, while only compiler-specific
widths for which scalar recurrence is cheaper stay directly in this wrapper.
*/
template <bool noskipws, ::std::integral char_type, my_unsigned_integral T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr parse_result<char_type const *>
scan_int_contiguous_unsigned_decimal_define_impl(
	char_type const *first, char_type const *last, T &t) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
	if constexpr (!noskipws)
	{
		first = ::fast_io::details::find_space_common_impl<false, true>(first, last);
		if (first == last)
		{
			return {first, parse_code::end_of_file};
		}
	}
	else if (first == last) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}

	unsigned_char_type first_digit{static_cast<unsigned_char_type>(*first)};
	if (char_digit_to_literal<10u, char_type>(first_digit)) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}
	auto const second{first + 1u};
	if (*first == char_literal_v<u8'0', char_type>) [[unlikely]]
	{
		if (second == last ||
			!char_is_digit<10u, char_type>(
				static_cast<unsigned_char_type>(*second))) [[likely]]
		{
			t = {};
			return {second, parse_code::ok};
		}
		return {second, parse_code::invalid};
	}
#if ((defined(__GNUC__) && !defined(__clang__) && !defined(__INTEL_COMPILER) && \
	  !defined(__CUDACC__) && __GNUC__ == 15) ||                                \
	 (defined(__clang__) && __clang_major__ == 23)) &&                          \
	!defined(__SSE4_1__) &&                                                     \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) &&            \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	if constexpr (sizeof(unsigned_type) == sizeof(::std::uint_least64_t) &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type>)
	{
		FAST_IO_IF_NOT_CONSTEVAL
		{
			/*
			The measured GCC and Clang baseline x86 targets cannot form the SSE4.1
			reduction above and outline the general radix/SWAR graph for every
			terminal decimal token.
			The first nineteen uint64 digits cannot overflow, so accumulate that
			prefix without a per-digit range branch, then validate the twentieth
			digit against the exact unsigned maximum.  This remains local to the
			default unsigned-decimal CPO; other bases and grammar flags retain the
			shared scanner. Constant evaluation continues through the portable path.
			*/
			constexpr ::std::uint_least64_t zeroes{UINT64_C(0x3030303030303030)};
			constexpr ::std::uint_least64_t high_bits{UINT64_C(0x8080808080808080)};
			constexpr ::std::uint_least64_t upper_bias{UINT64_C(0x4646464646464646)};
			constexpr ::std::uint_least64_t pair_mask{UINT64_C(0x000000FF000000FF)};
			constexpr ::std::uint_least64_t pair_multiplier{
				UINT64_C(100) + (UINT64_C(1000000) << 32u)};
			constexpr ::std::uint_least64_t quad_multiplier{
				UINT64_C(1) + (UINT64_C(10000) << 32u)};

			unsigned_type value{};
			auto iter{first};
			auto const distance{static_cast<::std::size_t>(last - first)};
			auto const safe_digits{distance < 19u ? distance : 19u};
			auto const safe_last{first + safe_digits};
			while (static_cast<::std::size_t>(safe_last - iter) >= 8u) [[likely]]
			{
				::std::uint_least64_t word;
				__builtin_memcpy(__builtin_addressof(word), iter, sizeof(word));
				auto const invalid{((word + upper_bias) | (word - zeroes)) & high_bits};
				if (invalid != 0u) [[likely]]
				{
					auto const valid_bits{
						static_cast<unsigned>(::std::countr_zero(invalid)) &
						static_cast<unsigned>(-8)};
					if (valid_bits != 0u) [[likely]]
					{
						word <<= 64u - valid_bits;
						word |= zeroes >> valid_bits;
						word -= zeroes;
						word = word * 10u + (word >> 8u);
						word = (((word & pair_mask) * pair_multiplier) +
								(((word >> 16u) & pair_mask) * quad_multiplier)) >>
							   32u;
						auto const digits{valid_bits >> 3u};
						value = static_cast<unsigned_type>(
							value * ::fast_io::details::pow_table_n<
										10u, ::std::uint_least64_t, 8u>
										.index_unchecked(digits) +
							word);
						iter += digits;
					}
					t = static_cast<T>(value);
					return {iter, parse_code::ok};
				}
				word -= zeroes;
				word = word * 10u + (word >> 8u);
				word = (((word & pair_mask) * pair_multiplier) +
						(((word >> 16u) & pair_mask) * quad_multiplier)) >>
					   32u;
				value = static_cast<unsigned_type>(value * UINT64_C(100000000) + word);
				iter += 8u;
			}
			for (; iter != safe_last; ++iter)
			{
				auto digit{static_cast<unsigned_char_type>(*iter)};
				if (char_digit_to_literal<10u, char_type>(digit)) [[unlikely]]
				{
					t = static_cast<T>(value);
					return {iter, parse_code::ok};
				}
				value = static_cast<unsigned_type>(value * 10u + digit);
			}
			if (iter == last)
			{
				t = static_cast<T>(value);
				return {iter, parse_code::ok};
			}

			auto twentieth{static_cast<unsigned_char_type>(*iter)};
			if (char_digit_to_literal<10u, char_type>(twentieth)) [[likely]]
			{
				t = static_cast<T>(value);
				return {iter, parse_code::ok};
			}
			constexpr unsigned_type maximum{static_cast<unsigned_type>(-1)};
			constexpr unsigned_type maximum_div10{maximum / 10u};
			constexpr unsigned_char_type maximum_mod10{
				static_cast<unsigned_char_type>(maximum % 10u)};
			bool const overflow{
				maximum_div10 < value ||
				(maximum_div10 == value && maximum_mod10 < twentieth)};
			if (!overflow)
			{
				value = static_cast<unsigned_type>(value * 10u + twentieth);
			}
			++iter;
			if (iter != last &&
				char_is_digit<10u, char_type>(
					static_cast<unsigned_char_type>(*iter))) [[unlikely]]
			{
				return {skip_digits<10u>(iter + 1u, last), parse_code::overflow};
			}
			if (overflow) [[unlikely]]
			{
				return {iter, parse_code::overflow};
			}
			t = static_cast<T>(value);
			return {iter, parse_code::ok};
		}
	}
#endif
	if (second == last ||
		!char_is_digit<10u, char_type>(
			static_cast<unsigned_char_type>(*second))) [[likely]]
	{
		t = static_cast<T>(first_digit);
		return {second, parse_code::ok};
	}

	/*
	GCC 14--16 prefer the grouped SWAR leaf from four digits onward, while Clang
	18--23 retains a smaller scalar graph through eight digits. Inspecting the
	first code unit beyond that profitable prefix keeps
	a short token on the recurrence path even when its containing range has a
	long suffix. If that code unit is a digit, an exact 4--19 element range uses
	the bounded leaf and every other shape uses the shared run-time core. Both
	fallbacks see the complete original range and therefore retain delimiter,
	overflow, and returned-cursor semantics. Future versions intentionally inherit
	the highest measured thresholds until their lowering proves otherwise.
	*/
#if defined(__GNUC__) && !defined(__clang__)
	constexpr ::std::size_t scalar_recurrence_limit{3u};
#else
	constexpr ::std::size_t scalar_recurrence_limit{8u};
#endif
	auto const remaining{static_cast<::std::size_t>(last - first)};
	if (remaining == 20u)
	{
		return scan_int_contiguous_unsigned_decimal_twenty_dispatch(
			first, last, t);
	}
#if (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	if (scalar_recurrence_limit < remaining && remaining <= 19u)
	{
		/* The grouped leaf validates its complete first block. Performing the
		   scalar threshold probe here would duplicate one load on every valid
		   exact range; a failed packed proof still replays the original range in
		   the shared scanner and therefore preserves short-token semantics. */
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 14
		if (12u <= remaining && remaining <= 15u)
		{
			return scan_int_contiguous_unsigned_decimal_twelve_dispatch(
				first, last, t);
		}
#endif
		return scan_int_contiguous_unsigned_decimal_bounded_dispatch(first, last, t);
	}
#endif
	if (scalar_recurrence_limit < remaining &&
		char_is_digit<10u, char_type>(
			static_cast<unsigned_char_type>(first[scalar_recurrence_limit])))
	{
		return scan_int_contiguous_unsigned_decimal_runtime_dispatch(
			first, last, t);
	}

	auto const short_last{remaining <= scalar_recurrence_limit
						  ? last
						  : first + scalar_recurrence_limit};
	unsigned_type value{static_cast<unsigned_type>(first_digit)};
	auto iter{second};
	for (; iter != short_last; ++iter)
	{
		auto digit{static_cast<unsigned_char_type>(*iter)};
		if (char_digit_to_literal<10u, char_type>(digit)) [[unlikely]]
		{
			break;
		}
		value = static_cast<unsigned_type>(value * 10u + digit);
	}
	t = static_cast<T>(value);
	return {iter, parse_code::ok};
}

template <char8_t base, bool noskipws, bool shbase, bool skipzero, bool oct_c2y,
		  bool allow_leading_plus = false,
		  ::std::integral char_type, details::my_integral T>
inline constexpr parse_result<char_type const *> scan_int_contiguous_define_impl(char_type const *first,
																	 char_type const *last, T &t) noexcept
{
	if constexpr (!noskipws)
	{
		first = ::fast_io::details::find_space_common_impl<false, true>(first, last);
		if (first == last)
		{
			return {first, parse_code::end_of_file};
		}
	}
	if constexpr (my_unsigned_integral<T>)
	{
		if constexpr (allow_leading_plus)
		{
			if (first == last) [[unlikely]]
			{
				return {first, parse_code::invalid};
			}
			if (*first == char_literal_v<u8'+', char_type>)
			{
				++first;
				if (first == last) [[unlikely]]
				{
					return {first, parse_code::invalid};
				}
			}
		}
		if constexpr (shbase && base != 10)
		{
			if constexpr (base == 8 && !oct_c2y)
			{
				if (first == last || *first != char_literal_v<u8'0', char_type>) [[unlikely]]
				{
					return {first, parse_code::invalid};
				}
				++first;
			}
			else
			{
				auto phase_ret = scan_shbase_impl<base, oct_c2y>(first, last);
				if (phase_ret.code != ongoing_parse_code) [[unlikely]]
				{
					return phase_ret;
				}
				first = phase_ret.iter;
			}
		}
	}
	return scan_int_contiguous_none_space_part_define_impl<base, ((shbase && base != 10) && my_signed_integral<T>),
														   skipzero, oct_c2y, allow_leading_plus>(first, last, t);
}

/*
The default signed 64-bit decimal shape shares the isolated unsigned decimal
parser after consuming the optional minus sign.  This keeps its SIMD/SWAR and
short-token choices identical to the unsigned peer without exposing those
choices to the general radix graph.  Plus-sign policy, prefixes, wide
characters, and smaller destinations retain the shared scanner below.
*/
template <bool noskipws, ::std::integral char_type, my_signed_integral T>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr parse_result<char_type const *>
scan_int_contiguous_signed_decimal_define_impl(
	char_type const *first, char_type const *last, T &t) noexcept
{
	auto const original_first{first};
	if constexpr (sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type> &&
				  sizeof(T) == sizeof(::std::int_least64_t))
	{
		if constexpr (!noskipws)
		{
			first = ::fast_io::details::find_space_common_impl<false, true>(first, last);
			if (first == last)
			{
				return {first, parse_code::end_of_file};
			}
		}
		else if (first == last) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		bool negative{};
		if (*first == char_literal_v<u8'-', char_type>)
		{
			negative = true;
			++first;
			if (first == last) [[unlikely]]
			{
				// Preserve the general integer grammar: a sign without a digit is
				// invalid and must not publish a value. This also keeps the optimized
				// GCC >= 14 / Clang >= 18 overload identical to older compilers.
				return {first, parse_code::invalid};
			}
		}
		::std::uint_least64_t value{};
		auto const ret{
			scan_int_contiguous_unsigned_decimal_define_impl<true>(
				first, last, value)};
		if (ret.code != parse_code::ok) [[unlikely]]
		{
			return ret;
		}
		using unsigned_type = my_make_unsigned_t<::std::remove_cvref_t<T>>;
		constexpr unsigned_type maximum{
			static_cast<unsigned_type>(-1) >> 1u};
		if (value > static_cast<::std::uint_least64_t>(
					maximum + static_cast<unsigned_type>(negative))) [[unlikely]]
		{
			return {ret.iter, parse_code::overflow};
		}
		auto const narrowed{static_cast<unsigned_type>(value)};
		t = negative
				? static_cast<T>(static_cast<unsigned_type>(0) - narrowed)
				: static_cast<T>(narrowed);
		return ret;
	}
	return scan_int_contiguous_define_impl<
		10u, noskipws, false, false, true, false>(original_first, last, t);
}

template <char8_t base, bool noskipws, bool shbase, bool skipzero,
		  bool oct_c2y, bool allow_leading_plus = false,
		  ::std::integral char_type, details::my_integral T>
inline constexpr parse_result<char_type const *>
scan_int_contiguous_padding_define_impl(
	char_type const *first, char_type const *last, ::std::size_t padding,
	T &t) noexcept
{
#if defined(__SSE4_1__) &&                                              \
	((defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC)))
	using unsigned_type =
		details::my_make_unsigned_t<::std::remove_cvref_t<T>>;
	if constexpr (
		base == 10 && !shbase && !skipzero &&
		sizeof(char_type) == sizeof(char8_t) &&
		::fast_io::details::is_ascii<char_type> &&
		sizeof(unsigned_type) <= sizeof(::std::uint_least64_t) &&
		sizeof(unsigned_type) >= sizeof(::std::uint_least32_t))
	{
		FAST_IO_IF_NOT_CONSTEVAL
		{
			auto parse_first{first};
			if constexpr (!noskipws)
			{
				parse_first =
					::fast_io::details::find_space_common_impl<false, true>(
						parse_first, last);
			}
			bool negative{};
			if constexpr (my_signed_integral<T>)
			{
				if (parse_first != last &&
					*parse_first ==
						char_literal_v<u8'-', char_type>)
				{
					negative = true;
					++parse_first;
				}
				else if constexpr (allow_leading_plus)
				{
					if (parse_first != last &&
						*parse_first ==
							char_literal_v<u8'+', char_type>)
					{
						++parse_first;
					}
				}
			}
			else if constexpr (allow_leading_plus)
			{
				if (parse_first != last &&
					*parse_first == char_literal_v<u8'+', char_type>)
				{
					++parse_first;
				}
			}

			if (parse_first != last)
			{
				auto const remaining{
					static_cast<::std::size_t>(last - parse_first)};
				using unsigned_char_type =
					::std::make_unsigned_t<char_type>;
				auto const first_digit{
					static_cast<unsigned_char_type>(*parse_first)};
				constexpr auto one{
					static_cast<unsigned_char_type>(
						char_literal_v<u8'1', char_type>)};
				if (10u <= remaining && remaining < 16u &&
					padding >= 16u - remaining &&
					static_cast<unsigned_char_type>(
						first_digit - one) < 9u)
				{
					::std::uint_least64_t parsed{};
					auto const tail_result{
						::fast_io::details::
							sse_parse_terminal_padding_tail(
								reinterpret_cast<char unsigned const *>(
									parse_first),
								remaining, parsed)};
					auto const *const iter{
						parse_first + tail_result.digits};
					if (tail_result.code != parse_code::ok)
					{
						return {iter, tail_result.code};
					}
					constexpr unsigned_type umax{
						static_cast<unsigned_type>(-1)};
					if constexpr (my_signed_integral<T>)
					{
						constexpr unsigned_type imax{umax >> 1u};
						if (parsed >
							static_cast<::std::uint_least64_t>(
								imax + negative))
						{
							return {iter, parse_code::overflow};
						}
						auto const narrowed{
							static_cast<unsigned_type>(parsed)};
						t = negative
								? static_cast<T>(
									  static_cast<unsigned_type>(0) -
									  narrowed)
								: static_cast<T>(narrowed);
					}
					else
					{
						if (parsed >
							static_cast<::std::uint_least64_t>(umax))
						{
							return {iter, parse_code::overflow};
						}
						t = static_cast<T>(
							static_cast<unsigned_type>(parsed));
					}
					return {iter, parse_code::ok};
				}
			}
		}
	}
#else
	(void)padding;
#endif
	return ::fast_io::details::scan_int_contiguous_define_impl<
		base, noskipws, shbase, skipzero, oct_c2y,
		allow_leading_plus>(first, last, t);
}
} // namespace details

enum class scan_integral_context_phase : ::std::uint_least8_t
{
	space,
	sign,
	prefix,
	zero,
	zero_skip,
	zero_invalid,
	digit,
	overflow
};

namespace details
{
template <char8_t base, ::std::integral char_type, ::fast_io::details::my_integral T>
inline constexpr auto scan_context_type_impl_int() noexcept
{
	using unsigned_type = details::my_make_unsigned_t<::std::remove_cvref_t<T>>;
	constexpr ::std::size_t max_size{
		(::fast_io::details::print_integer_reserved_size_cache<base, false, ::fast_io::details::my_signed_integral<T>,
															   false, unsigned_type>)+2};
	struct scan_integer_context
	{
		::fast_io::freestanding::array<char_type, max_size> buffer;
		/*
		size counts semantic payload retained in buffer: an optional minus sign
		followed by value digits.  prefix_progress instead counts characters already
		consumed from a required radix prefix while the machine is in prefix.  These
		states cannot share one integer: the value one would mean either "minus sign
		stored" or "first prefix character consumed", and a resumed parser could not
		reconstruct which transition occurred.  Keeping them independent also means
		the prefix helper never observes sign storage and the digit helper receives
		the original sign offset unchanged.
		*/
		::std::uint_least8_t size{};
		::std::uint_least8_t prefix_progress{};
		scan_integral_context_phase integer_phase{};
		inline constexpr void reset() noexcept
		{
			size = 0;
			prefix_progress = 0;
			integer_phase = scan_integral_context_phase::space;
		}
	};
	return io_type_t<scan_integer_context>{};
}
} // namespace details

namespace details
{

template <::std::integral char_type>
inline constexpr parse_result<char_type const *> sc_int_ctx_space_phase(char_type const *first,
																		char_type const *last) noexcept
{
	first = ::fast_io::details::find_space_common_impl<false, true>(first, last);
	if (first == last)
	{
		return {first, parse_code::partial};
	}
	return {first, ongoing_parse_code};
}

template <bool allow_negative, bool allow_positive, typename State, ::std::integral char_type>
inline constexpr parse_result<char_type const *> sc_int_ctx_sign_phase(State &st, char_type const *first,
																	   char_type const *last) noexcept
{
	if (first == last)
	{
		st.integer_phase = scan_integral_context_phase::sign;
		return {first, parse_code::partial};
	}
	if constexpr (allow_negative)
	{
		if constexpr (allow_positive)
		{
			auto ch{*first};
			if (ch == char_literal_v<u8'-', char_type>)
			{
				*st.buffer.data() = ch;
				st.size = 1;
				++first;
			}
			else if (ch == char_literal_v<u8'+', char_type>)
			{
				++first;
			}
		}
		else
		{
			if (*first == char_literal_v<u8'-', char_type>)
			{
				*st.buffer.data() = char_literal_v<u8'-', char_type>;
				st.size = 1;
				++first;
			}
		}
	}
	else
	{
		if constexpr (allow_positive)
		{
			auto ch{*first};
			if (ch == char_literal_v<u8'+', char_type>)
			{
				++first;
			}
		}
	}
	return {first, ongoing_parse_code};
}

template <char8_t base, bool oct_c2y, ::std::integral char_type>
	requires(base != 10)
inline constexpr parse_result<char_type const *>
sc_int_ctx_prefix_phase(::std::uint_least8_t &sz, char_type const *first, char_type const *last) noexcept
{
	if (first == last)
	{
		return {first, parse_code::partial};
	}
	if constexpr (base == 8 && !oct_c2y)
	{
		if (sz != 0)
		{
			sz = 0;
			return {first, ongoing_parse_code};
		}
		if (*first != char_literal_v<u8'0', char_type>) [[unlikely]]
		{
			return {first, parse_code::invalid};
		}
		++first;
		if (first == last)
		{
			sz = 1;
			return {first, parse_code::partial};
		}
	}
	else
	{
		::std::uint_least8_t size_cache{sz};
		if (size_cache == 0)
		{
			if (*first != char_literal_v<u8'0', char_type>) [[unlikely]]
			{
				return {first, parse_code::invalid};
			}
			if ((++first) == last)
			{
				sz = 1;
				return {first, parse_code::partial};
			}
			if constexpr (base != 2 && base != 3 && base != 16)
			{
				size_cache = 1;
			}
		}
		if constexpr (base == 2 || base == 3 || (base == 8 && oct_c2y) || base == 16)
		{
			auto ch{*first};
			if ((ch == char_literal_v<(base == 2 ? u8'B' : (base == 3 ? u8't' : (base == 8 ? u8'O' : u8'X'))), char_type>) |
				(ch == char_literal_v<(base == 2 ? u8'b' : (base == 3 ? u8't' : (base == 8 ? u8'o' : u8'x'))), char_type>)) [[likely]]
			{
				sz = 0;
				++first;
				return {first, ongoing_parse_code};
			}
			else [[unlikely]]
			{
				return {first, parse_code::invalid};
			}
		}
		else
		{
			/*
			size_cache denotes how many characters of the generic 0[base]
			prefix have already been consumed.  A chunk-ending transition stores
			the next stage in sz; a same-chunk transition must advance this local
			copy instead.  This keeps both paths on the identical state sequence
			0 -> 1 -> 2 -> 3 [-> 4] -> complete.  Every dereference below is
			guarded by the initial non-empty test or a post-increment end test.
			*/
			if (size_cache == 1)
			{
				if (*first != char_literal_v<u8'[', char_type>) [[unlikely]]
				{
					return {first, parse_code::invalid};
				}
				if ((++first) == last)
				{
					sz = 2;
					return {first, parse_code::partial};
				}
				size_cache = 2;
			}
			constexpr auto digit0{char_literal_v<u8'0' + (base < 10 ? base : base / 10), char_type>};
			if (size_cache == 2)
			{
				if (*first != digit0) [[unlikely]]
				{
					return {first, parse_code::invalid};
				}
				if ((++first) == last)
				{
					sz = 3;
					return {first, parse_code::partial};
				}
				size_cache = 3;
			}
			if constexpr (10 < base)
			{
				constexpr auto digit1{char_literal_v<u8'0' + (base % 10), char_type>};
				if (size_cache == 3)
				{
					if (*first != digit1) [[unlikely]]
					{
						return {first, parse_code::invalid};
					}
					if ((++first) == last)
					{
						sz = 4;
						return {first, parse_code::partial};
					}
					size_cache = 4;
				}
			}
			constexpr ::std::uint_least8_t last_index{base < 10 ? 3 : 4};
			if (size_cache == last_index)
			{
				if (*first != char_literal_v<u8']', char_type>) [[unlikely]]
				{
					return {first, parse_code::invalid};
				}
				sz = 0;
				++first;
			}
		}
	}
	return {first, ongoing_parse_code};
}

template <char8_t base, bool skipzero, ::std::integral char_type>
inline constexpr parse_result<char_type const *> sc_int_ctx_zero_phase(scan_integral_context_phase &integer_phase,
																	   char_type const *first,
																	   char_type const *last) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	if (first == last)
	{
		integer_phase = scan_integral_context_phase::zero;
		return {first, parse_code::partial};
	}
	constexpr auto zero{char_literal_v<u8'0', char_type>};
	auto first_ch{*first};
	if (!char_is_digit<base, char_type>(static_cast<unsigned_char_type>(first_ch))) [[unlikely]]
	{
		return {first, parse_code::invalid};
	}
	else if (first_ch == zero) [[unlikely]]
	{
		++first;
		if constexpr (skipzero)
		{
			first = find_none_zero_simd_impl(first, last);
		}
		if (first == last)
		{
			if constexpr (skipzero)
			{
				integer_phase = scan_integral_context_phase::zero_skip;
			}
			else
			{
				integer_phase = scan_integral_context_phase::zero_invalid;
			}
			return {first, parse_code::partial};
		}
		if (!char_is_digit<base, char_type>(static_cast<unsigned_char_type>(*first))) [[likely]]
		{
			return {first, parse_code::ok};
		}
		if constexpr (!skipzero)
		{
			return {first, parse_code::invalid};
		}
	}
	return {first, ongoing_parse_code};
}

template <char8_t base, bool oct_c2y, ::std::integral char_type, typename State, my_integral T>
	// This context-phase wrapper copies a bounded digit suffix and then calls the
	// same contiguous parser.  Force-inlining affects only that phase boundary;
	// unsupported compilers retain ordinary inline semantics.  No retained native
	// A/B isolates the attribute, so it is a conservative legacy layout policy
	// pending revalidation and carries no cross-frontend speed claim.
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr parse_result<char_type const *> sc_int_ctx_digit_phase(State &st, char_type const *first,
																		char_type const *last, T &t) noexcept
{
	auto it{skip_digits<base>(first, last)};
	::std::size_t const diff{st.buffer.size() - static_cast<::std::size_t>(st.size)};
	::std::size_t const first_it_diff{static_cast<::std::size_t>(it - first)};
	if (first_it_diff < diff)
	{
		auto start{st.buffer.data() + st.size};
		auto e{non_overlapped_copy_n(first, first_it_diff, start)};
		st.size += static_cast<::std::uint_least8_t>(first_it_diff);
		if (it == last)
		{
			st.integer_phase = scan_integral_context_phase::digit;
			return {it, parse_code::partial};
		}
		if (st.size == 0) [[likely]]
		{
			t = {};
			return {it, parse_code::ok};
		}
		auto [p, ec] = scan_int_contiguous_none_space_part_define_impl<base, false, false, oct_c2y>(st.buffer.data(), e, t);
		return {p - start + first, ec};
	}
	else
	{
		if (it == last)
		{
			st.integer_phase = scan_integral_context_phase::overflow;
			return {it, parse_code::partial};
		}
		else [[unlikely]]
		{
			return {it, parse_code::overflow};
		}
	}
}

template <char8_t base, ::std::integral char_type>
inline constexpr parse_result<char_type const *> sc_int_ctx_zero_invalid_phase(char_type const *first,
																			   char_type const *last) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	if (first == last)
	{
		return {first, parse_code::partial};
	}
	/*
	The preceding chunk already consumed the zero that selected zero_invalid, so
	first denotes the first unconsumed continuation character.  A delimiter ends
	the value without being consumed; another digit makes the spelling invalid at
	that same iterator.  The non-empty test above is therefore both the complete
	dereference precondition and the reason no pointer increment belongs here.
	*/
	if (!char_is_digit<base, char_type>(static_cast<unsigned_char_type>(*first))) [[likely]]
	{
		return {first, parse_code::ok};
	}
	return {first, parse_code::invalid};
}

template <char8_t base, ::std::integral char_type>
inline constexpr parse_result<char_type const *> sc_int_ctx_skip_digits_phase(char_type const *first,
																			  char_type const *last) noexcept
{
	first = skip_digits<base>(first, last);
	return {first, (first == last) ? parse_code::partial : parse_code::invalid};
}

template <char8_t base, bool noskipws, bool shbase, bool skipzero, bool oct_c2y,
		  bool allow_leading_plus = false,
		  typename State,
		  ::std::integral char_type, my_integral T>
inline constexpr parse_result<char_type const *> scan_context_define_parse_impl(State &st, char_type const *first,
																				char_type const *last, T &t) noexcept
{
	auto phase{st.integer_phase};
	/*
	State-transition proof for the optimizer assumptions below:

	* sign is written only by sc_int_ctx_sign_phase when its input ends.  That
	  helper is called only when T is signed or leading plus is enabled; therefore
	  an unsigned, no-plus instantiation cannot resume in sign.
	* prefix is written only inside the shbase && base != 10 arm in this switch;
	  every other instantiation therefore cannot resume in prefix.
	* sc_int_ctx_zero_phase writes zero_skip exactly when skipzero is true and
	  writes zero_invalid exactly when it is false.  The opposite state is
	  unreachable for each specialization.

	The space state is intentionally not excluded for a noskipws specialization.
	Value-initialization and reset() both select space, while noskipws suppresses
	only the whitespace-consumption operation.  The space arm must consequently
	remain reachable on the first call and falls through without consuming input.
	An assumption to the contrary would be false for every freshly initialized
	noskipws context.  When the attribute is unavailable, the full switch is the
	semantically exact fallback.
	*/
	if constexpr (my_unsigned_integral<T> && !allow_leading_plus)
	{
		FAST_IO_ASSUME(phase != scan_integral_context_phase::sign);
	}
	if constexpr (!shbase || base == 10)
	{
		FAST_IO_ASSUME(phase != scan_integral_context_phase::prefix);
	}
	if constexpr (skipzero)
	{
		FAST_IO_ASSUME(phase != scan_integral_context_phase::zero_invalid);
	}
	else
	{
		FAST_IO_ASSUME(phase != scan_integral_context_phase::zero_skip);
	}
	switch (phase)
	{
	case scan_integral_context_phase::space:
	{
		if constexpr (!noskipws)
		{
			auto phase_ret = sc_int_ctx_space_phase(first, last);
			if (phase_ret.code != ongoing_parse_code) [[unlikely]]
			{
				return phase_ret;
			}
			first = phase_ret.iter;
		}
		[[fallthrough]];
	}
	case scan_integral_context_phase::sign:
	{
		if constexpr (my_signed_integral<T> || allow_leading_plus)
		{
			auto phase_ret = sc_int_ctx_sign_phase<my_signed_integral<T>, allow_leading_plus>(st, first, last);
			if (phase_ret.code != ongoing_parse_code) [[unlikely]]
			{
				return phase_ret;
			}
			first = phase_ret.iter;
		}
		[[fallthrough]];
	}
	case scan_integral_context_phase::prefix:
	{
		if constexpr (shbase && base != 10)
		{
			st.integer_phase = scan_integral_context_phase::prefix;
			/*
			The helper advances only prefix_progress.  st.size remains the sign
			offset (zero or one) until digit copying begins, so both same-buffer and
			cross-buffer prefix completion preserve a stored minus sign exactly.
			Unsigned and positive inputs have a zero sign offset; no-showbase and
			base-10 specializations discard this arm at compile time.
			*/
			auto phase_ret = sc_int_ctx_prefix_phase<base, oct_c2y>(st.prefix_progress, first, last);
			if (phase_ret.code != ongoing_parse_code) [[unlikely]]
			{
				return phase_ret;
			}
			first = phase_ret.iter;
		}
		[[fallthrough]];
	}
	case scan_integral_context_phase::zero:
	case scan_integral_context_phase::zero_skip:
	{
		if constexpr (!(shbase && base != 10))
		{
			auto phase_ret = sc_int_ctx_zero_phase<base, skipzero>(st.integer_phase, first, last);
			if (phase_ret.code != ongoing_parse_code)
			{
				if constexpr (skipzero)
				{
					if (phase_ret.code == parse_code::ok)
					{
						t = {};
					}
					else if (phase_ret.code == parse_code::invalid && phase == scan_integral_context_phase::zero_skip)
					{
						t = {};
						phase_ret.code = parse_code::ok;
					}
				}
				else
				{
					if (phase_ret.code == parse_code::ok)
					{
						t = {};
					}
				}
				return phase_ret;
			}
			first = phase_ret.iter;
		}
		[[fallthrough]];
	}
	case scan_integral_context_phase::digit:
	{
		return sc_int_ctx_digit_phase<base, oct_c2y>(st, first, last, t);
	}
	case scan_integral_context_phase::zero_invalid:
	{
		if constexpr (skipzero)
		{
			return {first, parse_code::invalid};
		}
		else
		{
			auto phase_ret = sc_int_ctx_zero_invalid_phase<base>(first, last);
			if (phase_ret.code == parse_code::ok)
			{
				t = {};
			}
			return phase_ret;
		}
	}
	case scan_integral_context_phase::overflow:
	{
		first = skip_digits<base>(first, last);
		return {first, (first == last) ? parse_code::partial : parse_code::overflow};
	}
	default:
	{
		return sc_int_ctx_skip_digits_phase<base>(first, last);
	}
	}
}

template <char8_t base, bool noskipws, bool shbase, bool skipzero, bool oct_c2y, typename State, my_integral T>
	// EOF handling is a terminal/error-only phase.  The cold attribute changes
	// section placement only; an unsupported frontend executes the identical
	// switch with ordinary placement.  No isolated native A/B is retained, so the
	// hint is a conservative legacy policy without an unmeasured-frontend claim.
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr parse_code scan_context_eof_define_parse_impl(State &st, T &t) noexcept
{
	auto phase{st.integer_phase};
	/*
	zero_skip is assigned only by sc_int_ctx_zero_phase<..., true>; an
	instantiation with skipzero == false can assign zero_invalid but never
	zero_skip, and neither value initialization nor reset introduces zero_skip.
	The EOF assumption is therefore implied by all state writes.  Without
	[[assume]], the unchanged switch remains the exact semantic fallback.
	*/
	if constexpr (!skipzero)
	{
		FAST_IO_ASSUME(phase != scan_integral_context_phase::zero_skip);
	}
	switch (phase)
	{
	case scan_integral_context_phase::space:
	{
		if constexpr (noskipws)
		{
			return parse_code::invalid;
		}
		else
		{
			return parse_code::end_of_file;
		}
	}
	case scan_integral_context_phase::digit:
		return scan_int_contiguous_none_space_part_define_impl<base, false, false, oct_c2y>(st.buffer.data(), st.buffer.data() + st.size, t).code;
	case scan_integral_context_phase::overflow:
		return parse_code::overflow;
	case scan_integral_context_phase::zero_skip:
	case scan_integral_context_phase::zero_invalid:
	{
		t = {};
		return parse_code::ok;
	}
	default:
		return parse_code::invalid;
	}
}

} // namespace details

namespace manipulators
{

/// @brief Hidden scan carrier that reads one input code unit into a destination.
/// @details `reference` is borrowed and must outlive the scan. No numeric conversion or encoding transcoding occurs.
template <typename char_type>
struct ch_get_t
{
	using manip_tag = manip_tag_t;
	char_type &reference;
};

/// @brief Scans one code unit into an integral destination.
/// @details Leading C whitespace is skipped, then exactly the first non-whitespace code unit is consumed and assigned
///          without digit interpretation or encoding transcoding. It therefore cannot be used to read a whitespace
///          code unit verbatim.
template <::fast_io::details::my_integral T>
inline constexpr ch_get_t<T &> ch_get(T &reference) noexcept
{
	return {reference};
}

/// @brief Scans an integer in compile-time radix `bs` using an explicitly configurable grammar.
/// @details `noskipws` disables leading-space skipping; `skipzero` admits and discards redundant leading zeroes;
///          `prefix` requires `0b`, `0t`, legacy `0`/modern `0o`, or `0x` for bases 2, 3, 8, and 16. Other non-decimal
///          bases use `0[bs]` with the base written in decimal (for example `0[12]`); base 10 ignores `prefix`.
///          `oct_c2y` selects `0o`, and a leading `+` is opt-in.
template <::std::size_t bs, bool noskipws = false, bool skipzero = false, bool prefix = false, bool oct_c2y = false,
		  bool allow_leading_plus = false, ::fast_io::details::my_integral scalar_type>
	requires(2 <= bs && bs <= 36)
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									bs, noskipws, (bs == 10 ? false : prefix), skipzero, oct_c2y,
									allow_leading_plus>,
								scalar_type &>
base_get(scalar_type &t) noexcept
{
	return {t};
}

/// @brief Scans a binary integer with an optional required `0b` prefix.
/// @details Whitespace, redundant-leading-zero, and leading-plus policies are selected independently by the template
///          arguments. Only binary digits participate in the token.
template <bool noskipws = false, bool skipzero = false, bool prefix = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									2, noskipws, prefix, skipzero, false, allow_leading_plus>,
								scalar_type &>
bin_get(scalar_type &t) noexcept
{
	return {t};
}

/// @brief Scans a binary integer and requires the `0b` prefix.
/// @details `noskipws`, `skipzero`, and leading-plus behavior match `bin_get`.
template <bool noskipws = false, bool skipzero = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									2, noskipws, true, skipzero, false, allow_leading_plus>,
								scalar_type &>
bin0b_get(scalar_type &t) noexcept
{
	return {t};
}

/// @brief Scans an octal integer with configurable prefix grammar.
/// @details `prefix` requires a prefix; `oct_c2y` chooses modern `0o` rather than legacy leading zero. Whitespace,
///          redundant zeroes, and leading `+` remain independently selectable.
template <bool noskipws = false, bool skipzero = false, bool prefix = false, bool oct_c2y = false,
		  bool allow_leading_plus = false, ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									8, noskipws, prefix, skipzero, oct_c2y, allow_leading_plus>,
								scalar_type &>
oct_get(scalar_type &t) noexcept
{
	return {t};
}

/// @brief Scans an octal integer and requires the legacy leading-zero prefix.
/// @details Modern `0o` is not selected by this overload; whitespace, redundant zeroes, and leading `+` are configurable.
template <bool noskipws = false, bool skipzero = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									8, noskipws, true, skipzero, false, allow_leading_plus>,
								scalar_type &>
oct0_get(scalar_type &t) noexcept
{
	return {t};
}

/// @brief Scans an octal integer and requires the modern `0o` prefix.
/// @details The prefix grammar is distinct from `oct0_get`; both lowercase and grammar-supported case variants are
///          handled by the scanner rather than by an output-case flag.
template <bool noskipws = false, bool skipzero = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									8, noskipws, true, skipzero, true, allow_leading_plus>,
								scalar_type &>
oct0o_get(scalar_type &t) noexcept
{
	return {t};
}

/// @brief Scans a decimal integer without a radix prefix.
/// @details `skipzero` controls whether redundant leading zeroes are accepted, `noskipws` controls initial whitespace,
///          and leading `+` is rejected unless explicitly enabled.
template <bool noskipws = false, bool skipzero = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									10, noskipws, false, skipzero, false, allow_leading_plus>,
								scalar_type &>
dec_get(scalar_type &t) noexcept
{
	return {t};
}

/// @brief Scans a hexadecimal integer with an optionally required `0x` prefix.
/// @details Hex digit case is accepted according to the scanner grammar. Whitespace, redundant zeroes, and leading-plus
///          handling are compile-time options.
template <bool noskipws = false, bool skipzero = false, bool prefix = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									16, noskipws, prefix, skipzero, false, allow_leading_plus>,
								scalar_type &>
hex_get(scalar_type &t) noexcept
{
	return {t};
}

/// @brief Scans a hexadecimal integer and requires the `0x` prefix.
/// @details It otherwise shares `hex_get` whitespace, redundant-zero, and leading-plus policies.
template <bool noskipws = false, bool skipzero = false, bool allow_leading_plus = false,
		  ::fast_io::details::my_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									16, noskipws, true, skipzero, false, allow_leading_plus>,
								scalar_type &>
hex0x_get(scalar_type &t) noexcept
{
	return {t};
}

/// @brief Scans a prefixed hexadecimal address representation into an unsigned integer.
/// @details The `0x` prefix is required and redundant leading zeroes are accepted, matching full-width address output;
///          no pointer validity or address-space conversion is performed.
template <bool noskipws = false, bool allow_leading_plus = false, ::fast_io::details::my_unsigned_integral scalar_type>
inline constexpr scalar_manip_t<::fast_io::details::base_scan_mani_flags_cache<
									16, noskipws, true, true, false, allow_leading_plus>,
								scalar_type &>
addrvw_get(scalar_type &t) noexcept
{
	return {t};
}

} // namespace manipulators

template <details::my_integral T>
inline constexpr ::fast_io::manipulators::scalar_manip_t<
	::fast_io::details::base_scan_mani_flags_cache<10, false, false, false, false>, T &>
scan_alias_define(io_alias_t, T &t) noexcept
{
	return {t};
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_integral T>
inline constexpr auto
scan_context_type(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return details::scan_context_type_impl_int<flags.base, char_type, T>();
}

#if (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC)) &&                    \
	((defined(__clang__) && __clang_major__ >= 18) ||                    \
	 (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 14))
/*
Select the compact default-decimal graph at the customization-point boundary.
This overload is more constrained than the general scalar CPO below, so an
unrelated radix, signed type, wide/non-ASCII character type, prefix policy, or
leading-zero policy never inherits its code-generation choice.
*/
template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_unsigned_integral T>
	requires(flags.base == 10u && !flags.showbase && !flags.full &&
			 !flags.allow_leading_plus &&
			 sizeof(char_type) == sizeof(char8_t) &&
			 ::fast_io::details::is_ascii<char_type> &&
			 sizeof(T) == sizeof(::std::uint_least64_t))
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr parse_result<char_type const *>
scan_contiguous_define(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>,
	char_type const *begin, char_type const *end,
	::fast_io::manipulators::scalar_manip_t<flags, T &> t) noexcept
{
	return details::scan_int_contiguous_unsigned_decimal_define_impl<
		flags.noskipws>(begin, end, t.reference);
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_signed_integral T>
	requires(flags.base == 10u && !flags.showbase && !flags.full &&
			 !flags.allow_leading_plus &&
			 sizeof(char_type) == sizeof(char8_t) &&
			 ::fast_io::details::is_ascii<char_type> &&
			 sizeof(T) == sizeof(::std::int_least64_t))
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr parse_result<char_type const *>
scan_contiguous_define(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>,
	char_type const *begin, char_type const *end,
	::fast_io::manipulators::scalar_manip_t<flags, T &> t) noexcept
{
	return details::scan_int_contiguous_signed_decimal_define_impl<
		flags.noskipws>(begin, end, t.reference);
}
#endif

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_integral T>
inline constexpr parse_result<char_type const *>
scan_contiguous_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
						   char_type const *begin, char_type const *end,
						   ::fast_io::manipulators::scalar_manip_t<flags, T &> t) noexcept
{
	return details::scan_int_contiguous_define_impl<flags.base, flags.noskipws, flags.showbase, flags.full,
												flags.modern_octal, flags.allow_leading_plus>(
		begin, end, t.reference);
}

/// The integer leaf advances only by bounded pointer iteration over `[begin,end]` and every early/error result returns
/// one of those cursors.  Advertising that proof lets buffered terminal dispatch commit directly while open third-party
/// scanner CPOs retain generic address and alignment validation.
template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_integral T>
inline constexpr ::std::true_type scan_contiguous_result_in_range(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return {};
}

/// Every integer context transition advances by bounded iteration over the exact supplied chunk. This is independent
/// of whether the token completes, remains partial, or reports an error, so trusted context dispatch can publish its
/// cursor without repeating the generic address/alignment proof on every refill phase.
template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_integral T>
inline constexpr ::std::true_type scan_context_result_in_range(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return {};
}

/// Integer formatting emits only digits and optional lexical punctuation (sign, base prefix, digit separator). None of
/// those spellings is a C whitespace character in the selected execution-character domain, so a target which separately
/// accepts a complete whitespace-free fragment may consume the full formatted range without a token-search pass.
template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_integral T>
inline constexpr ::std::true_type print_fragment_c_space_free(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T>>) noexcept
{
	return {};
}

/// Integer contiguous and context leaves implement the same complete-input grammar, overflow policy, target assignment,
/// and consumed cursor.  On a terminal input the context EOF transition therefore has no observable behavior absent
/// from the contiguous result.  Refillable chunks do not consume this proof and retain the context state machine.
template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_integral T>
inline constexpr ::std::true_type scan_context_terminal_contiguous_equivalent(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return {};
}

/// Integer conversion normally benefits from replacing several context transitions with one scan over a small terminal
/// stack staging range. Legality remains independently guarded by `scan_context_terminal_contiguous_equivalent`.
template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_integral T>
inline constexpr ::std::true_type to_terminal_contiguous_staging_preferred(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return {};
}

template <::std::integral char_type, manipulators::scalar_flags flags,
			  details::my_integral T>
inline constexpr ::std::size_t scan_context_current_chunk_minimum_size(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	using unsigned_type =
		::fast_io::details::my_make_unsigned_t<::std::remove_cvref_t<T>>;
	return (::fast_io::details::print_integer_reserved_size_cache<
			flags.base, false, ::fast_io::details::my_signed_integral<T>,
			false, unsigned_type>)+3u;
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_integral T>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
inline constexpr parse_result<char_type const *>
scan_context_current_chunk_integer_fallback(
	char_type const *begin, char_type const *end,
	::fast_io::manipulators::scalar_manip_t<flags, T &> target) noexcept
{
	// Keep the generic transactional temporary out of the bounded-decimal hot frame. This fallback still owns the
	// target publication rule for long tokens, unusual formatting policies, and chunk-boundary misses.
	T value{};
	auto result{details::scan_int_contiguous_define_impl<
		flags.base, flags.noskipws, flags.showbase, flags.full,
		flags.modern_octal, flags.allow_leading_plus>(begin, end, value)};
	if (result.iter == end)
	{
		return {begin, parse_code::partial};
	}
	if (result.code == parse_code::ok)
	{
		target.reference = value;
	}
	return result;
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_integral T>
inline constexpr parse_result<char_type const *>
scan_context_current_chunk_define(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>,
	char_type const *begin, char_type const *end,
	::fast_io::manipulators::scalar_manip_t<flags, T &> target) noexcept
{
#if (defined(__GNUC__) || defined(__clang__)) &&                      \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	// A buffered input's `end` is the physical chunk end, not the token end. Detect the overwhelmingly common bounded
	// unsigned decimal before that large span selects the long-range parser; seeing the delimiter proves both completion
	// and the returned iterator range, while nine leading digits fall through without publishing target state.
	if constexpr (flags.base == 10u && !flags.noskipws && !flags.showbase &&
				  !flags.full && !flags.allow_leading_plus &&
				  ::fast_io::details::my_unsigned_integral<T> &&
				  sizeof(T) == sizeof(::std::uint_least64_t) &&
				  sizeof(char_type) == sizeof(char8_t) &&
				  ::fast_io::details::is_ascii<char_type>)
	{
		auto first{::fast_io::details::find_space_common_impl<false, true>(begin, end)};
		if (first != end)
		{
			using unsigned_char_type = ::std::make_unsigned_t<char_type>;
			auto digit{static_cast<unsigned_char_type>(*first)};
			if (!::fast_io::details::char_digit_to_literal<10u, char_type>(digit))
			{
				if (digit == 0u)
				{
					auto const iter{first + 1u};
					if (iter != end)
					{
						auto next_digit{static_cast<unsigned_char_type>(*iter)};
						if (::fast_io::details::char_digit_to_literal<10u, char_type>(next_digit)) [[likely]]
						{
							target.reference = 0u;
							return {iter, parse_code::ok};
						}
					}
					return ::fast_io::scan_context_current_chunk_integer_fallback(begin, end, target);
				}
				if (static_cast<::std::size_t>(end - first) >= 4u)
				{
					::std::uint_least64_t four_digit_value;
					if (::fast_io::details::scan_int_contiguous_x86_parse_four_digits<10u>(
							first, four_digit_value)) [[likely]]
					{
						auto value{static_cast<T>(four_digit_value)};
						auto iter{first + 4u};
						for (unsigned digits{4u}; digits != 9u && iter != end; ++digits, ++iter)
						{
							digit = static_cast<unsigned_char_type>(*iter);
							if (::fast_io::details::char_digit_to_literal<10u, char_type>(digit)) [[likely]]
							{
								target.reference = value;
								return {iter, parse_code::ok};
							}
							value = static_cast<T>(value * 10u + digit);
						}
					}
				}
				auto value{static_cast<T>(digit)};
				auto iter{first + 1u};
				for (unsigned digits{1u}; digits != 9u && iter != end; ++digits, ++iter)
				{
					digit = static_cast<unsigned_char_type>(*iter);
					if (::fast_io::details::char_digit_to_literal<10u, char_type>(digit)) [[likely]]
					{
						target.reference = value;
						return {iter, parse_code::ok};
					}
					value = static_cast<T>(value * 10u + digit);
				}
			}
		}
	}
#endif
	return ::fast_io::scan_context_current_chunk_integer_fallback(begin, end, target);
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_integral T>
inline constexpr ::std::true_type scan_context_current_chunk_result_in_range(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return {};
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_integral T>
inline constexpr parse_result<char_type const *>
scan_contiguous_padding_define(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>,
	char_type const *begin, char_type const *end, ::std::size_t padding,
	::fast_io::manipulators::scalar_manip_t<flags, T &> t) noexcept
{
	return details::scan_int_contiguous_padding_define_impl<
		flags.base, flags.noskipws, flags.showbase, flags.full,
		flags.modern_octal, flags.allow_leading_plus>(
		begin, end, padding, t.reference);
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename State, details::my_integral T>
inline constexpr parse_result<char_type const *>
scan_context_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>, State &state,
					char_type const *begin, char_type const *end,
					::fast_io::manipulators::scalar_manip_t<flags, T &> t) noexcept
{
	return details::scan_context_define_parse_impl<flags.base, flags.noskipws, flags.showbase, flags.full,
												   flags.modern_octal, flags.allow_leading_plus>(
		state, begin, end, t.reference);
}

template <::std::integral char_type, manipulators::scalar_flags flags, typename State, details::my_integral T>
inline constexpr parse_code
scan_context_eof_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>, State &state,
						::fast_io::manipulators::scalar_manip_t<flags, T &> t) noexcept
{
	return details::scan_context_eof_define_parse_impl<flags.base, flags.noskipws, flags.showbase, flags.full, flags.modern_octal>(
		state, t.reference);
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_integral T>
inline constexpr ::std::true_type
scan_context_terminal_padding_equivalent(
	io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_t<flags, T &>>) noexcept
{
	return {};
}

namespace details
{
template <::std::integral char_type>
inline constexpr parse_result<char_type const *> ch_get_context_impl(char_type const *first, char_type const *last,
																	 char_type &t) noexcept
{
	first = ::fast_io::details::find_space_common_impl<false, true>(first, last);
	if (first == last)
	{
		return {first, parse_code::partial};
	}
	t = *first;
	++first;
	return {first, parse_code::ok};
}
} // namespace details

template <::std::integral char_type>
inline constexpr io_type_t<details::empty>
scan_context_type(io_reserve_type_t<char_type, manipulators::ch_get_t<char_type &>>) noexcept
{
	return {};
}

template <::std::integral char_type>
inline constexpr parse_result<char_type const *>
scan_context_define(io_reserve_type_t<char_type, manipulators::ch_get_t<char_type &>>, details::empty,
					char_type const *begin, char_type const *end, manipulators::ch_get_t<char_type &> t) noexcept
{
	return details::ch_get_context_impl(begin, end, t.reference);
}

template <::std::integral char_type>
inline constexpr parse_code scan_context_eof_define(io_reserve_type_t<char_type, manipulators::ch_get_t<char_type &>>,
													details::empty, manipulators::ch_get_t<char_type &>) noexcept
{
	return parse_code::end_of_file;
}

} // namespace fast_io

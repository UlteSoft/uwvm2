#pragma once

namespace fast_io
{

namespace details
{

template <::fast_io::details::my_integral T>
struct from_chars_runtime_safe_digits_table
{
	::fast_io::freestanding::array<::std::uint_least8_t, 35u> positive;
	::fast_io::freestanding::array<::std::uint_least8_t, 35u> negative;
};

template <::fast_io::details::my_integral T>
inline consteval auto generate_from_chars_runtime_safe_digits() noexcept
{
	using unsigned_type = ::fast_io::details::my_make_unsigned_t<T>;
	constexpr unsigned_type unsigned_max{static_cast<unsigned_type>(-1)};
	constexpr unsigned_type positive_limit{
		::fast_io::details::my_signed_integral<T>
			? static_cast<unsigned_type>(unsigned_max >> 1u)
			: unsigned_max};
	constexpr unsigned_type negative_limit{
		::fast_io::details::my_signed_integral<T>
			? static_cast<unsigned_type>(positive_limit + 1u)
			: unsigned_max};
	::fast_io::details::from_chars_runtime_safe_digits_table<T> table{};
	for (unsigned base{2u}; base != 37u; ++base)
	{
		auto count_safe_digits = [base](unsigned_type limit) constexpr {
			::std::uint_least8_t digits{};
			unsigned_type maximum_value{};
			auto const maximum_digit{static_cast<unsigned_type>(base - 1u)};
			while (maximum_value <=
				   static_cast<unsigned_type>((limit - maximum_digit) / base))
			{
				maximum_value = static_cast<unsigned_type>(
					maximum_value * static_cast<unsigned_type>(base) + maximum_digit);
				++digits;
			}
			return digits;
		};
		table.positive.index_unchecked(base - 2u) =
			count_safe_digits(positive_limit);
		table.negative.index_unchecked(base - 2u) =
			count_safe_digits(negative_limit);
	}
	return table;
}

/*
For a selected radix, entry N is the largest digit count for which every
N-digit magnitude fits the destination limit.  The generator evaluates the
recurrence M[n+1]=M[n]*base+(base-1) only after proving it remains within the
limit, so table construction itself cannot overflow.  The compact run-time
parser uses this bound to keep its common prefix free of division and overflow
comparisons; only the single boundary digit of a maximum-width value needs the
quotient/remainder test.
*/
template <::fast_io::details::my_integral T>
inline constexpr auto from_chars_runtime_safe_digits{
	::fast_io::details::generate_from_chars_runtime_safe_digits<T>()};

template <bool signed_integer, ::fast_io::details::character char_type>
inline constexpr ::fast_io::basic_from_chars_result<char_type>
from_chars_integral_map_result(::fast_io::parse_result<char_type const *> result,
							   char_type const *original_first) noexcept
{
	if (result.code == ::fast_io::parse_code::ok) [[likely]]
	{
		return {result.iter, {}};
	}
	if (result.code == ::fast_io::parse_code::overflow)
	{
		return {result.iter, ::fast_io::charconv_errc::result_out_of_range};
	}
	if constexpr (signed_integer)
	{
		return {original_first, ::fast_io::charconv_errc::invalid_argument};
	}
	else
	{
		return {result.iter, ::fast_io::charconv_errc::invalid_argument};
	}
}

template <::std::size_t base, ::fast_io::details::my_integral T,
		  ::fast_io::details::character char_type>
	requires(2u <= base && base <= 36u &&
			 !::std::same_as<::std::remove_cv_t<T>, bool>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_from_chars_result<char_type>
from_chars_integral_fixed_base(char_type const *first, char_type const *last, T &value) noexcept
{
	/*
	These bounded public-entry shortcuts and the shared scanner have identical
	parse, overflow, and end-pointer contracts.  GNU-family native x86-64 omits
	the duplicate wrapper graph because its shared scanner already owns the
	accepted x86 short-input kernels.  The exclusion is a code-generation choice,
	not an ISA correctness requirement.  ARM64EC is not classified as native
	x86-64, and other compilers retain the conservative public shortcut.

	The explicit digit tests prove that every accepted value fits: ten octal
	digits use only 30 value bits, and at most eight digits in bases 3, 4, and
	11--16 are below UINT64_MAX.  Every other case falls through to the same
	general scanner.  Native GCC/Clang x86 and M4 assembly support the current
	split; MSVC-native performance remains a separately identified retest item.
	*/
#if !((defined(__GNUC__) || defined(__clang__)) &&                     \
	  (defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	  !(defined(__arm64ec__) || defined(_M_ARM64EC)))
	if constexpr (::std::same_as<char_type, char> &&
				  ::fast_io::details::my_unsigned_integral<T> && sizeof(T) == sizeof(::std::uint_least64_t) &&
				  base == 8u)
	{
		auto const remaining{static_cast<::std::size_t>(last - first)};
		if ((remaining == 10u &&
			 ::fast_io::details::char_is_digit<8u, char>(
				 static_cast<unsigned char>(first[9u]))) ||
			(remaining == 11u &&
			 !::fast_io::details::char_is_digit<8u, char>(
				 static_cast<unsigned char>(first[10u])))) [[unlikely]]
		{
			auto const digit0{static_cast<unsigned char>(first[0u] - '0')};
			auto const digit1{static_cast<unsigned char>(first[1u] - '0')};
			auto const digit2{static_cast<unsigned char>(first[2u] - '0')};
			auto const digit3{static_cast<unsigned char>(first[3u] - '0')};
			auto const digit4{static_cast<unsigned char>(first[4u] - '0')};
			auto const digit5{static_cast<unsigned char>(first[5u] - '0')};
			auto const digit6{static_cast<unsigned char>(first[6u] - '0')};
			auto const digit7{static_cast<unsigned char>(first[7u] - '0')};
			auto const digit8{static_cast<unsigned char>(first[8u] - '0')};
			auto const digit9{static_cast<unsigned char>(first[9u] - '0')};
			if ((digit0 | digit1 | digit2 | digit3 | digit4 | digit5 | digit6 |
				 digit7 | digit8 | digit9) <= 7u) [[likely]]
			{
				auto const parsed{
					(static_cast<::std::uint_least64_t>(digit0) << 27u) |
					(static_cast<::std::uint_least64_t>(digit1) << 24u) |
					(static_cast<::std::uint_least64_t>(digit2) << 21u) |
					(static_cast<::std::uint_least64_t>(digit3) << 18u) |
					(static_cast<::std::uint_least64_t>(digit4) << 15u) |
					(static_cast<::std::uint_least64_t>(digit5) << 12u) |
					(static_cast<::std::uint_least64_t>(digit6) << 9u) |
					(static_cast<::std::uint_least64_t>(digit7) << 6u) |
					(static_cast<::std::uint_least64_t>(digit8) << 3u) |
					static_cast<::std::uint_least64_t>(digit9)};
				value = static_cast<T>(parsed);
				return {first + 10u, {}};
			}
		}
	}
	if constexpr (::std::same_as<char_type, char> &&
				  ::fast_io::details::my_unsigned_integral<T> && sizeof(T) == sizeof(::std::uint_least64_t) &&
				  (base == 3u || base == 4u || (11u <= base && base <= 16u)))
	{
		constexpr ::std::size_t short_limit{8u};
		auto const remaining{static_cast<::std::size_t>(last - first)};
		if (remaining <= short_limit ||
			(remaining == short_limit + 1u &&
			 !::fast_io::details::char_is_digit<static_cast<char8_t>(base), char>(
				 static_cast<unsigned char>(last[-1])))) [[likely]]
		{
			using unsigned_type = ::fast_io::details::my_make_unsigned_t<T>;
			unsigned_type accumulator{};
			auto iter{first};
			::std::size_t digits{};
			for (; iter != last && digits != short_limit; ++iter, ++digits)
			{
				auto digit{static_cast<unsigned char>(*iter)};
				if (::fast_io::details::char_digit_to_literal<static_cast<char8_t>(base), char>(
						digit)) [[unlikely]]
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
			if (iter == last ||
				!::fast_io::details::char_is_digit<static_cast<char8_t>(base), char>(
					static_cast<unsigned char>(*iter))) [[likely]]
			{
				if (digits == 0u) [[unlikely]]
				{
					return {first, ::fast_io::charconv_errc::invalid_argument};
				}
				value = static_cast<T>(accumulator);
				return {iter, {}};
			}
		}
	}
#endif
	auto const original_first{first};
	auto const result =
		::fast_io::details::scan_int_contiguous_none_space_part_define_impl<
			static_cast<char8_t>(base), false, true, false, false, true>(
			first, last, value);
	return ::fast_io::details::from_chars_integral_map_result<::fast_io::details::my_signed_integral<T>>(
		result, original_first);
}

/*
The compact loop is deliberately kept out of the specialized dispatcher.
Folding it into that dispatcher makes its register pressure impose a large
prologue on fixed leaves even though they never execute the loop.  Clang's size
heuristic naturally outlines it; GCC x86-64 requires an explicit `noinline` to
obtain the same isolation and keep the generic fallback frame out of the
specialized dispatcher.  The one call/return is amortized whenever the compact
radix loop is reached.  Apple M4 assembly and its complete radix/length matrix
independently verify Clang's natural layout.
*/
template <::fast_io::details::my_integral T, ::fast_io::details::character char_type>
	requires(!::std::same_as<::std::remove_cv_t<T>, bool>)
#if defined(__GNUC__) && !defined(__clang__) &&                      \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
[[gnu::noinline]]
#endif
inline constexpr ::fast_io::basic_from_chars_result<char_type>
from_chars_integral_runtime_base_compact(char_type const *first,
										 char_type const *last, T &value,
										 unsigned base) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = ::fast_io::details::my_make_unsigned_t<T>;
	auto const original_first{first};
	bool negative{};
	if constexpr (::fast_io::details::my_signed_integral<T>)
	{
		if (first != last && *first == ::fast_io::char_literal_v<u8'-', char_type>)
		{
			negative = true;
			++first;
		}
	}

	/*
	The radix is a run-time value, but the character alphabet is not.  Normalizing
	through the base-36 digit decoder therefore accepts exactly the union of all
	standard integer digits; the subsequent `digit < base` comparison selects the
	requested alphabet prefix.  Reusing the established decoder also preserves
	ASCII, wide-character, and EBCDIC execution encodings without a second table.

	Let L be the largest permitted magnitude (`max(T)` or `-min(T)` for a signed
	negative result), q=floor(L/base), and r=L mod base.  Before appending digit d,
	`accumulator > q || (accumulator == q && d > r)` is equivalent to
	`accumulator*base+d > L`; multiplication is performed only after that proof,
	so the arithmetic itself cannot overflow.  Once overflow is known the loop
	continues decoding digits solely to produce the standard end pointer.  The
	destination is assigned only on success, preserving the required unchanged
	value for both invalid input and out-of-range input.
	*/
	constexpr unsigned_type unsigned_max{static_cast<unsigned_type>(-1)};
	unsigned_type limit{unsigned_max};
	if constexpr (::fast_io::details::my_signed_integral<T>)
	{
		limit = static_cast<unsigned_type>((unsigned_max >> 1u) +
										   static_cast<unsigned_type>(negative));
	}
	auto const &safe_digits_table{
		::fast_io::details::from_chars_runtime_safe_digits<T>};
	auto const safe_digits{static_cast<::std::size_t>(
		(negative ? safe_digits_table.negative : safe_digits_table.positive)
			.index_unchecked(base - 2u))};
	unsigned_type accumulator{};
	unsigned_type cutoff{};
	unsigned_type cutlim{};
	::std::size_t digits{};
	bool any_digit{};
	bool overflow{};
	bool limit_ready{};
	auto decode_digit = [base](char_type ch, unsigned_char_type &digit) constexpr {
		digit = static_cast<unsigned_char_type>(ch);
		return ::fast_io::details::char_digit_to_literal<36u, char_type>(digit) ||
			   base <= digit;
	};

	/*
	Eight- and four-digit trees shorten the dependency chain of the safe prefix.
	For a successful eight-digit block, p_i=d[2i]*B+d[2i+1],
	q_i=p[2i]*B^2+p[2i+1], and block=q[0]*B^4+q[1]; substitution gives exactly
	the original radix polynomial.  Admission requires `digits + N <= safe_digits`,
	which proves both the block and `accumulator*B^N+block` fit.  If any decoded
	code unit is not a digit, no state is committed and the scalar loop resumes at
	the same pointer, preserving first-invalid-character semantics.
	*/
	auto const radix{static_cast<unsigned_type>(base)};
	auto const radix_squared{static_cast<unsigned_type>(radix * radix)};
	auto const radix_fourth{
		static_cast<unsigned_type>(radix_squared * radix_squared)};
	/*
	`unsigned_type` may be `uint16_t`, which is promoted to signed `int` when
	`int` represents its complete range.  At base 15, `radix_fourth` is 50625
	and squaring it then overflows that promoted `int` before the cast below.
	The calculation is eager even when an eight-digit block is inadmissible.

	Converting one operand to `uint_least32_t` makes the common arithmetic type
	wide enough to represent every value below 2^32.  On an ABI with a still wider
	signed `int`, integer promotion may select that signed type; the product remains
	representable rather than relying on it being unsigned.  For a narrow
	accumulator `radix_fourth` is at most 65535, so its square is strictly below
	2^32 and exact.  For wider accumulators the usual arithmetic conversions retain
	the wider unsigned type.  Conversion back to `unsigned_type` therefore has the
	intended well-defined modulo semantics.
	*/
	auto const radix_eighth{
		static_cast<unsigned_type>(
			static_cast<::std::uint_least32_t>(radix_fourth) * radix_fourth)};
	while (8u <= safe_digits - digits &&
		   8u <= static_cast<::std::size_t>(last - first))
	{
		unsigned_char_type digit0;
		unsigned_char_type digit1;
		unsigned_char_type digit2;
		unsigned_char_type digit3;
		unsigned_char_type digit4;
		unsigned_char_type digit5;
		unsigned_char_type digit6;
		unsigned_char_type digit7;
		bool invalid{decode_digit(first[0u], digit0)};
		invalid |= decode_digit(first[1u], digit1);
		invalid |= decode_digit(first[2u], digit2);
		invalid |= decode_digit(first[3u], digit3);
		invalid |= decode_digit(first[4u], digit4);
		invalid |= decode_digit(first[5u], digit5);
		invalid |= decode_digit(first[6u], digit6);
		invalid |= decode_digit(first[7u], digit7);
		if (invalid) [[unlikely]]
		{
			break;
		}
		auto const pair0{static_cast<unsigned_type>(digit0 * radix + digit1)};
		auto const pair1{static_cast<unsigned_type>(digit2 * radix + digit3)};
		auto const pair2{static_cast<unsigned_type>(digit4 * radix + digit5)};
		auto const pair3{static_cast<unsigned_type>(digit6 * radix + digit7)};
		auto const quad0{
			static_cast<unsigned_type>(pair0 * radix_squared + pair1)};
		auto const quad1{
			static_cast<unsigned_type>(pair2 * radix_squared + pair3)};
		auto const block{
			static_cast<unsigned_type>(quad0 * radix_fourth + quad1)};
		accumulator = static_cast<unsigned_type>(
			accumulator * radix_eighth + block);
		first += 8u;
		digits += 8u;
		any_digit = true;
	}
	while (4u <= safe_digits - digits &&
		   4u <= static_cast<::std::size_t>(last - first))
	{
		unsigned_char_type digit0;
		unsigned_char_type digit1;
		unsigned_char_type digit2;
		unsigned_char_type digit3;
		bool invalid{decode_digit(first[0u], digit0)};
		invalid |= decode_digit(first[1u], digit1);
		invalid |= decode_digit(first[2u], digit2);
		invalid |= decode_digit(first[3u], digit3);
		if (invalid) [[unlikely]]
		{
			break;
		}
		auto const pair0{static_cast<unsigned_type>(digit0 * radix + digit1)};
		auto const pair1{static_cast<unsigned_type>(digit2 * radix + digit3)};
		auto const block{
			static_cast<unsigned_type>(pair0 * radix_squared + pair1)};
		accumulator = static_cast<unsigned_type>(
			accumulator * radix_fourth + block);
		first += 4u;
		digits += 4u;
		any_digit = true;
	}
	for (; first != last; ++first)
	{
		unsigned_char_type digit;
		if (decode_digit(*first, digit))
		{
			break;
		}
		any_digit = true;
		if (!overflow)
		{
			if (digits < safe_digits)
			{
				accumulator = static_cast<unsigned_type>(
					accumulator * static_cast<unsigned_type>(base) +
					static_cast<unsigned_type>(digit));
			}
			else
			{
				if (!limit_ready)
				{
					cutoff = static_cast<unsigned_type>(limit / base);
					cutlim = static_cast<unsigned_type>(limit % base);
					limit_ready = true;
				}
				overflow = cutoff < accumulator ||
						   (accumulator == cutoff && cutlim < digit);
				if (!overflow)
				{
					accumulator = static_cast<unsigned_type>(
						accumulator * static_cast<unsigned_type>(base) +
						static_cast<unsigned_type>(digit));
				}
			}
		}
		++digits;
	}
	if (!any_digit) [[unlikely]]
	{
		return {original_first, ::fast_io::charconv_errc::invalid_argument};
	}
	if (overflow) [[unlikely]]
	{
		return {first, ::fast_io::charconv_errc::result_out_of_range};
	}
	if constexpr (::fast_io::details::my_signed_integral<T>)
	{
		value = negative
					? static_cast<T>(static_cast<unsigned_type>(unsigned_type{} - accumulator))
					: static_cast<T>(accumulator);
	}
	else
	{
		value = static_cast<T>(accumulator);
	}
	return {first, {}};
}

#if defined(__GNUC__) && !defined(__clang__) &&                      \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
/*
GCC emits out-of-line scanner-template graphs referenced from dead
`__builtin_constant_p` branches.  Calling the full fixed-base scanner from a
literal-only branch would therefore make the run-time-base object larger even
when the caller's base is not constant.  This self-contained leaf is forced
inline: a run-time caller removes every dead literal branch before code
emission, while a literal caller receives a constant-multiply loop with no
dispatch switch or out-of-line scanner edge.

For limit L, radix R, accumulator A, and next digit d, the test
`A > L/R || (A == L/R && d > L%R)` is equivalent to `A*R+d > L`.  The multiply
is evaluated only after the test proves it fits.  Overflow scanning continues
to the first non-digit solely to provide the required end pointer, and the
destination is assigned only on success.
*/
template <unsigned base, ::fast_io::details::my_integral T,
		  ::fast_io::details::character char_type>
	requires(11u <= base && base <= 15u &&
			 !::std::same_as<::std::remove_cv_t<T>, bool>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_from_chars_result<char_type>
from_chars_integral_gcc_literal_mid_base_compact(char_type const *first,
												 char_type const *last,
												 T &value) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = ::fast_io::details::my_make_unsigned_t<T>;
	auto const original_first{first};
	bool negative{};
	if constexpr (::fast_io::details::my_signed_integral<T>)
	{
		if (first != last && *first == ::fast_io::char_literal_v<u8'-', char_type>)
		{
			negative = true;
			++first;
		}
	}
	constexpr unsigned_type unsigned_max{static_cast<unsigned_type>(-1)};
	unsigned_type limit{unsigned_max};
	if constexpr (::fast_io::details::my_signed_integral<T>)
	{
		limit = static_cast<unsigned_type>((unsigned_max >> 1u) +
										   static_cast<unsigned_type>(negative));
	}
	auto const &safe_table{::fast_io::details::from_chars_runtime_safe_digits<T>};
	auto const safe_digits{static_cast<::std::size_t>(
		(negative ? safe_table.negative : safe_table.positive)
			.index_unchecked(base - 2u))};
	unsigned_type accumulator{};
	unsigned_type cutoff{};
	unsigned_type cutlim{};
	::std::size_t digits{};
	bool overflow{};
	bool limit_ready{};
	if constexpr (base == 12u || base == 14u)
	{
		/*
		Across the GCC 13 and 15 matrices, bases 12 and 14 are the scalar
		constant-multiply outliers.  The same transformation increases code size
		without a compensating speedup for the other mid-range bases.  Restricting
		the balanced block to those two measured mid-range outliers keeps the
		literal path close to the compact scalar leaf's size elsewhere.

		For four valid digits, p0=d0*R+d1 and p1=d2*R+d3 imply
		`p0*R^2+p1 = d0*R^3+d1*R^2+d2*R+d3`.  Merging that block with
		`accumulator*R^4` is therefore exactly four scalar recurrences, but with a
		shorter dependency chain.  The safe-prefix admission proves the operations
		fit; state is committed only after every code unit is validated.
		*/
		constexpr unsigned_type radix{static_cast<unsigned_type>(base)};
		constexpr unsigned_type radix_squared{
			static_cast<unsigned_type>(radix * radix)};
		constexpr unsigned_type radix_fourth{
			static_cast<unsigned_type>(radix_squared * radix_squared)};
		while (4u <= safe_digits - digits &&
			   4u <= static_cast<::std::size_t>(last - first))
		{
			unsigned_char_type digit0{static_cast<unsigned_char_type>(first[0u])};
			unsigned_char_type digit1{static_cast<unsigned_char_type>(first[1u])};
			unsigned_char_type digit2{static_cast<unsigned_char_type>(first[2u])};
			unsigned_char_type digit3{static_cast<unsigned_char_type>(first[3u])};
			bool invalid{::fast_io::details::char_digit_to_literal<
				static_cast<char8_t>(base), char_type>(digit0)};
			invalid |= ::fast_io::details::char_digit_to_literal<
				static_cast<char8_t>(base), char_type>(digit1);
			invalid |= ::fast_io::details::char_digit_to_literal<
				static_cast<char8_t>(base), char_type>(digit2);
			invalid |= ::fast_io::details::char_digit_to_literal<
				static_cast<char8_t>(base), char_type>(digit3);
			if (invalid) [[unlikely]]
			{
				break;
			}
			auto const pair0{static_cast<unsigned_type>(digit0 * radix + digit1)};
			auto const pair1{static_cast<unsigned_type>(digit2 * radix + digit3)};
			auto const block{
				static_cast<unsigned_type>(pair0 * radix_squared + pair1)};
			accumulator = static_cast<unsigned_type>(accumulator * radix_fourth + block);
			first += 4u;
			digits += 4u;
		}
	}
	for (; first != last; ++first)
	{
		unsigned_char_type digit{static_cast<unsigned_char_type>(*first)};
		if (::fast_io::details::char_digit_to_literal<
				static_cast<char8_t>(base), char_type>(digit))
		{
			break;
		}
		if (!overflow)
		{
			if (digits < safe_digits)
			{
				accumulator = static_cast<unsigned_type>(
					accumulator * static_cast<unsigned_type>(base) +
					static_cast<unsigned_type>(digit));
			}
			else
			{
				if (!limit_ready)
				{
					cutoff = static_cast<unsigned_type>(limit / base);
					cutlim = static_cast<unsigned_type>(limit % base);
					limit_ready = true;
				}
				overflow = cutoff < accumulator ||
						   (accumulator == cutoff && cutlim < digit);
				if (!overflow)
				{
					accumulator = static_cast<unsigned_type>(
						accumulator * static_cast<unsigned_type>(base) +
						static_cast<unsigned_type>(digit));
				}
			}
		}
		++digits;
	}
	if (digits == 0u) [[unlikely]]
	{
		return {original_first, ::fast_io::charconv_errc::invalid_argument};
	}
	if (overflow) [[unlikely]]
	{
		return {first, ::fast_io::charconv_errc::result_out_of_range};
	}
	if constexpr (::fast_io::details::my_signed_integral<T>)
	{
		value = negative
					? static_cast<T>(static_cast<unsigned_type>(unsigned_type{} - accumulator))
					: static_cast<T>(accumulator);
	}
	else
	{
		value = static_cast<T>(accumulator);
	}
	return {first, {}};
}

/*
GCC x86-64 lowers a run-time multiply in bases 11--15 substantially worse than
the corresponding constant multiply, but five complete scanner-policy graphs
cost several kilobytes.  This helper emits sign selection, overflow policy, and
result mapping once, then specializes only the digit recurrence.  The outlined
boundary is deliberate: forcing this helper into the dispatcher regressed the
complete GCC 13/15 matrix by roughly three to seven percent.  For a fixed radix
R and limit L, the safe-prefix proof is identical to the general compact parser.
Beyond that prefix, A > L/R || (A == L/R && d > L%R) is equivalent to A*R+d > L,
so no overflowing arithmetic is evaluated.  The input continues to be decoded
after overflow solely to preserve the standard end pointer.
*/
template <::fast_io::details::my_integral T,
		  ::fast_io::details::character char_type>
	requires(!::std::same_as<::std::remove_cv_t<T>, bool>)
[[gnu::noinline]] inline constexpr ::fast_io::basic_from_chars_result<char_type>
from_chars_integral_gcc_mid_base_compact(char_type const *first,
										 char_type const *last, T &value,
										 unsigned base) noexcept
{
	using unsigned_char_type = ::std::make_unsigned_t<char_type>;
	using unsigned_type = ::fast_io::details::my_make_unsigned_t<T>;
	auto const original_first{first};
	bool negative{};
	if constexpr (::fast_io::details::my_signed_integral<T>)
	{
		if (first != last && *first == ::fast_io::char_literal_v<u8'-', char_type>)
		{
			negative = true;
			++first;
		}
	}
	constexpr unsigned_type unsigned_max{static_cast<unsigned_type>(-1)};
	unsigned_type limit{unsigned_max};
	if constexpr (::fast_io::details::my_signed_integral<T>)
	{
		limit = static_cast<unsigned_type>((unsigned_max >> 1u) +
										   static_cast<unsigned_type>(negative));
	}
	auto const &safe_table{::fast_io::details::from_chars_runtime_safe_digits<T>};
	auto const safe_digits{static_cast<::std::size_t>(
		(negative ? safe_table.negative : safe_table.positive)
			.index_unchecked(base - 2u))};
	unsigned_type magnitude{};
	auto parse_fixed = [&]<unsigned radix>() constexpr {
		unsigned_type accumulator{};
		unsigned_type cutoff{};
		unsigned_type cutlim{};
		::std::size_t digits{};
		bool overflow{};
		bool limit_ready{};
		auto iter{first};
		for (; iter != last; ++iter)
		{
			unsigned_char_type digit{static_cast<unsigned_char_type>(*iter)};
			if (::fast_io::details::char_digit_to_literal<
					static_cast<char8_t>(radix), char_type>(digit))
			{
				break;
			}
			if (!overflow)
			{
				if (digits < safe_digits)
				{
					accumulator = static_cast<unsigned_type>(
						accumulator * static_cast<unsigned_type>(radix) +
						static_cast<unsigned_type>(digit));
				}
				else
				{
					if (!limit_ready)
					{
						cutoff = static_cast<unsigned_type>(limit / radix);
						cutlim = static_cast<unsigned_type>(limit % radix);
						limit_ready = true;
					}
					overflow = cutoff < accumulator ||
							   (accumulator == cutoff && cutlim < digit);
					if (!overflow)
					{
						accumulator = static_cast<unsigned_type>(
							accumulator * static_cast<unsigned_type>(radix) +
							static_cast<unsigned_type>(digit));
					}
				}
			}
			++digits;
		}
		magnitude = accumulator;
		return ::fast_io::parse_result<char_type const *>{
			iter, digits == 0u
					  ? ::fast_io::parse_code::invalid
				  : overflow ? ::fast_io::parse_code::overflow
							 : ::fast_io::parse_code::ok};
	};
	::fast_io::parse_result<char_type const *> result;
	switch (base)
	{
	case 11:
		result = parse_fixed.template operator()<11u>();
		break;
	case 12:
		result = parse_fixed.template operator()<12u>();
		break;
	case 13:
		result = parse_fixed.template operator()<13u>();
		break;
	case 14:
		result = parse_fixed.template operator()<14u>();
		break;
	case 15:
		result = parse_fixed.template operator()<15u>();
		break;
	default:
		::fast_io::fast_terminate();
	}
	if (result.code == ::fast_io::parse_code::invalid) [[unlikely]]
	{
		return {original_first, ::fast_io::charconv_errc::invalid_argument};
	}
	if (result.code == ::fast_io::parse_code::overflow) [[unlikely]]
	{
		return {result.iter, ::fast_io::charconv_errc::result_out_of_range};
	}
	if constexpr (::fast_io::details::my_signed_integral<T>)
	{
		value = negative
					? static_cast<T>(static_cast<unsigned_type>(unsigned_type{} - magnitude))
					: static_cast<T>(magnitude);
	}
	else
	{
		value = static_cast<T>(magnitude);
	}
	return {result.iter, {}};
}
#endif

/*
This explicitly written switch is the conservative fallback for frontends on
which the compact split is not profitable.  It also remains an internal
semantic reference for tests.  Clang callers use direct constant
branches in the public wrapper, which preserve the specialized decimal,
hexadecimal, SIMD, and bounded integer kernels without macro-generated control
flow.
*/
template <::fast_io::details::my_integral T, ::fast_io::details::character char_type>
	requires(!::std::same_as<::std::remove_cv_t<T>, bool>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_from_chars_result<char_type>
from_chars_integral_literal_base(char_type const *first, char_type const *last,
								 T &value, int base) noexcept
{
	switch (base)
	{
	case 2:
		return ::fast_io::details::from_chars_integral_fixed_base<2u>(first, last, value);
	case 3:
		return ::fast_io::details::from_chars_integral_fixed_base<3u>(first, last, value);
	case 4:
		return ::fast_io::details::from_chars_integral_fixed_base<4u>(first, last, value);
	case 5:
		return ::fast_io::details::from_chars_integral_fixed_base<5u>(first, last, value);
	case 6:
		return ::fast_io::details::from_chars_integral_fixed_base<6u>(first, last, value);
	case 7:
		return ::fast_io::details::from_chars_integral_fixed_base<7u>(first, last, value);
	case 8:
		return ::fast_io::details::from_chars_integral_fixed_base<8u>(first, last, value);
	case 9:
		return ::fast_io::details::from_chars_integral_fixed_base<9u>(first, last, value);
	[[likely]] case 10:
		return ::fast_io::details::from_chars_integral_fixed_base<10u>(first, last, value);
	case 11:
		return ::fast_io::details::from_chars_integral_fixed_base<11u>(first, last, value);
	case 12:
		return ::fast_io::details::from_chars_integral_fixed_base<12u>(first, last, value);
	case 13:
		return ::fast_io::details::from_chars_integral_fixed_base<13u>(first, last, value);
	case 14:
		return ::fast_io::details::from_chars_integral_fixed_base<14u>(first, last, value);
	case 15:
		return ::fast_io::details::from_chars_integral_fixed_base<15u>(first, last, value);
	case 16:
		return ::fast_io::details::from_chars_integral_fixed_base<16u>(first, last, value);
	case 17:
		return ::fast_io::details::from_chars_integral_fixed_base<17u>(first, last, value);
	case 18:
		return ::fast_io::details::from_chars_integral_fixed_base<18u>(first, last, value);
	case 19:
		return ::fast_io::details::from_chars_integral_fixed_base<19u>(first, last, value);
	case 20:
		return ::fast_io::details::from_chars_integral_fixed_base<20u>(first, last, value);
	case 21:
		return ::fast_io::details::from_chars_integral_fixed_base<21u>(first, last, value);
	case 22:
		return ::fast_io::details::from_chars_integral_fixed_base<22u>(first, last, value);
	case 23:
		return ::fast_io::details::from_chars_integral_fixed_base<23u>(first, last, value);
	case 24:
		return ::fast_io::details::from_chars_integral_fixed_base<24u>(first, last, value);
	case 25:
		return ::fast_io::details::from_chars_integral_fixed_base<25u>(first, last, value);
	case 26:
		return ::fast_io::details::from_chars_integral_fixed_base<26u>(first, last, value);
	case 27:
		return ::fast_io::details::from_chars_integral_fixed_base<27u>(first, last, value);
	case 28:
		return ::fast_io::details::from_chars_integral_fixed_base<28u>(first, last, value);
	case 29:
		return ::fast_io::details::from_chars_integral_fixed_base<29u>(first, last, value);
	case 30:
		return ::fast_io::details::from_chars_integral_fixed_base<30u>(first, last, value);
	case 31:
		return ::fast_io::details::from_chars_integral_fixed_base<31u>(first, last, value);
	case 32:
		return ::fast_io::details::from_chars_integral_fixed_base<32u>(first, last, value);
	case 33:
		return ::fast_io::details::from_chars_integral_fixed_base<33u>(first, last, value);
	case 34:
		return ::fast_io::details::from_chars_integral_fixed_base<34u>(first, last, value);
	case 35:
		return ::fast_io::details::from_chars_integral_fixed_base<35u>(first, last, value);
	case 36:
		return ::fast_io::details::from_chars_integral_fixed_base<36u>(first, last, value);
	[[unlikely]] default:
		::fast_io::fast_terminate();
	}
}

} // namespace details

template <::fast_io::details::my_integral T, ::fast_io::details::character char_type>
	requires(!::std::same_as<::std::remove_cv_t<T>, bool>)
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr ::fast_io::basic_from_chars_result<char_type>
from_chars(char_type const *first, char_type const *last, T &value, int base = 10) noexcept
{
	// The standard interface requires base in [2, 36].  This optional attribute
	// exposes that existing caller precondition to the optimizer and changes no
	// valid-base result.
	FAST_IO_ASSUME(2 <= base && base <= 36);
	/*
	Clang's `__builtin_constant_p` is evaluated after this always-inline public
	wrapper sees its caller.  Literal radices therefore retain fixed-base
	assembly, while a run-time radix has no edge to the large switch.  GCC x86-64
	also constant-folds the fixed leaves, but bases 11--15 use the self-contained
	literal leaf below because GCC emits out-of-line scanner graphs referenced by
	dead constant branches.  A dynamic GCC x86-64 radix instead uses the measured
	hybrid dispatcher.  GCC on other ISAs and native MSVC retain the conservative
	complete switch; this is a code-generation split, not a semantic distinction.
	*/
#if defined(__clang__)
	if (__builtin_constant_p(base) && base == 2)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<2u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 3)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<3u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 4)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<4u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 5)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<5u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 6)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<6u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 7)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<7u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 8)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<8u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 9)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<9u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 10)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<10u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 11)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<11u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 12)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<12u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 13)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<13u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 14)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<14u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 15)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<15u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 16)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<16u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 17)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<17u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 18)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<18u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 19)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<19u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 20)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<20u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 21)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<21u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 22)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<22u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 23)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<23u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 24)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<24u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 25)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<25u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 26)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<26u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 27)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<27u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 28)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<28u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 29)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<29u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 30)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<30u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 31)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<31u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 32)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<32u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 33)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<33u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 34)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<34u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 35)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<35u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 36)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<36u>(first, last, value);
	}
	/*
	Radices 2--9 have the longest legal uint64_t inputs (64 through 21 digits),
	so recurrence latency is amplified there and the established block/SIMD
	kernels provide a material throughput advantage.  Apple M4 measurements show
	that retaining these eight leaves recovers almost all of the full 35-leaf
	switch's weighted throughput.  Decimal remains specialized because it is the
	dominant public radix; hexadecimal remains specialized because its dedicated
	kernel recovered about ten percent in the per-base M4 matrix for a small code-
	size increment.  The remaining radices use the compact shared recurrence,
	which removes the majority of the run-time-base call graph.
	*/
	switch (base)
	{
	case 2:
		return ::fast_io::details::from_chars_integral_fixed_base<2u>(first, last, value);
	case 3:
		return ::fast_io::details::from_chars_integral_fixed_base<3u>(first, last, value);
	case 4:
		return ::fast_io::details::from_chars_integral_fixed_base<4u>(first, last, value);
	case 5:
		return ::fast_io::details::from_chars_integral_fixed_base<5u>(first, last, value);
	case 6:
		return ::fast_io::details::from_chars_integral_fixed_base<6u>(first, last, value);
	case 7:
		return ::fast_io::details::from_chars_integral_fixed_base<7u>(first, last, value);
	case 8:
		return ::fast_io::details::from_chars_integral_fixed_base<8u>(first, last, value);
	case 9:
		return ::fast_io::details::from_chars_integral_fixed_base<9u>(first, last, value);
	[[likely]] case 10:
		return ::fast_io::details::from_chars_integral_fixed_base<10u>(first, last, value);
	case 16:
		return ::fast_io::details::from_chars_integral_fixed_base<16u>(first, last, value);
	default:
		break;
	}
	return ::fast_io::details::from_chars_integral_runtime_base_compact(
		first, last, value, static_cast<unsigned>(base));
#elif defined(__GNUC__) && !defined(__clang__) &&                    \
	(defined(__x86_64__) || defined(_M_AMD64) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	/*
	Only GCC x86-64 uses this measured hybrid.  GCC AArch64 and other targets
	retain the complete fixed-base dispatcher below: neither x86 instruction
	selection evidence nor x86 code-size results prove that this split is
	profitable on those ISAs.
	*/
	if (__builtin_constant_p(base) && base == 11)
	{
		return ::fast_io::details::from_chars_integral_gcc_literal_mid_base_compact<11u>(
			first, last, value);
	}
	if (__builtin_constant_p(base) && base == 12)
	{
		return ::fast_io::details::from_chars_integral_gcc_literal_mid_base_compact<12u>(
			first, last, value);
	}
	if (__builtin_constant_p(base) && base == 13)
	{
		return ::fast_io::details::from_chars_integral_gcc_literal_mid_base_compact<13u>(
			first, last, value);
	}
	if (__builtin_constant_p(base) && base == 14)
	{
		return ::fast_io::details::from_chars_integral_gcc_literal_mid_base_compact<14u>(
			first, last, value);
	}
	if (__builtin_constant_p(base) && base == 15)
	{
		return ::fast_io::details::from_chars_integral_gcc_literal_mid_base_compact<15u>(
			first, last, value);
	}
#if __GNUC__ == 15 && defined(__SSE4_1__) && !defined(__AVX__)
	/*
	The dynamic GCC 15 SSE4.1 policy below intentionally shares bases 17--36,
	but a literal radix should retain its fixed scanner.  GCC 15 eliminates these
	edges far more effectively than the bases 11--15 graphs: retaining all twenty
	literal leaves adds only 1,166 bytes to the dynamic object, while the lower
	graphs require the self-contained leaf above.  Keeping the explicit predicates
	here gives literal callers byte-for-byte-equivalent fixed-base bodies with no
	run-time switch or shared-parser call; the independent-function matrix retained
	100.1% of baseline throughput.
	*/
	if (__builtin_constant_p(base) && base == 17)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<17u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 18)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<18u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 19)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<19u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 20)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<20u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 21)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<21u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 22)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<22u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 23)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<23u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 24)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<24u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 25)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<25u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 26)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<26u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 27)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<27u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 28)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<28u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 29)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<29u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 30)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<30u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 31)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<31u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 32)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<32u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 33)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<33u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 34)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<34u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 35)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<35u>(first, last, value);
	}
	if (__builtin_constant_p(base) && base == 36)
	{
		return ::fast_io::details::from_chars_integral_fixed_base<36u>(first, last, value);
	}
#endif
	switch (base)
	{
	case 2:
		return ::fast_io::details::from_chars_integral_fixed_base<2u>(first, last, value);
	case 3:
		return ::fast_io::details::from_chars_integral_fixed_base<3u>(first, last, value);
	case 4:
		return ::fast_io::details::from_chars_integral_fixed_base<4u>(first, last, value);
	case 5:
		return ::fast_io::details::from_chars_integral_fixed_base<5u>(first, last, value);
	case 6:
		return ::fast_io::details::from_chars_integral_fixed_base<6u>(first, last, value);
	case 7:
		return ::fast_io::details::from_chars_integral_fixed_base<7u>(first, last, value);
	case 8:
		return ::fast_io::details::from_chars_integral_fixed_base<8u>(first, last, value);
	case 9:
		return ::fast_io::details::from_chars_integral_fixed_base<9u>(first, last, value);
	[[likely]] case 10:
		return ::fast_io::details::from_chars_integral_fixed_base<10u>(first, last, value);
		/*
		The 665-point `(base, digits)` matrix isolates bases 11--15 as the interval
		where GCC's run-time multiply loses materially, while cloning the complete
		scanner policy costs more than the recurrence it accelerates.  These cases
		therefore share the policy once and select only five constant-multiply
		recurrences in the outlined helper.  Against the complete switch, this hybrid
		reduced the measured `uint64_t` object by 12.5% on GCC 13 and 14.5% on GCC 15;
		aggregate throughput remained within 2.3% on both compilers.
		*/
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
		return ::fast_io::details::from_chars_integral_gcc_mid_base_compact(
			first, last, value, static_cast<unsigned>(base));
	case 16:
		return ::fast_io::details::from_chars_integral_fixed_base<16u>(first, last, value);
		/*
		GCC 15's SSE4.1-without-AVX backend expands the fixed scanners for bases
		17--36 especially aggressively: the measured complete-switch object was
	57,058 bytes.  Routing only that exact compiler/ISA combination through the
	shared recurrence reduced it to 28,220 bytes (50.5%) while retaining 96.9%
		of aggregate throughput over all 665 `(base, digits)` points.  GCC 13 lost
		13.6% under the same split, and GCC 15 AVX2 did not exhibit the code-size
		anomaly, so both deliberately retain the fixed leaves.  No microarchitecture
		is selected: the condition describes compiler code generation and the
		translation unit's minimum ISA only.
		*/
#if __GNUC__ != 15 || !defined(__SSE4_1__) || defined(__AVX__)
	case 17:
		return ::fast_io::details::from_chars_integral_fixed_base<17u>(first, last, value);
	case 18:
		return ::fast_io::details::from_chars_integral_fixed_base<18u>(first, last, value);
	case 19:
		return ::fast_io::details::from_chars_integral_fixed_base<19u>(first, last, value);
	case 20:
		return ::fast_io::details::from_chars_integral_fixed_base<20u>(first, last, value);
	case 21:
		return ::fast_io::details::from_chars_integral_fixed_base<21u>(first, last, value);
	case 22:
		return ::fast_io::details::from_chars_integral_fixed_base<22u>(first, last, value);
	case 23:
		return ::fast_io::details::from_chars_integral_fixed_base<23u>(first, last, value);
	case 24:
		return ::fast_io::details::from_chars_integral_fixed_base<24u>(first, last, value);
	case 25:
		return ::fast_io::details::from_chars_integral_fixed_base<25u>(first, last, value);
	case 26:
		return ::fast_io::details::from_chars_integral_fixed_base<26u>(first, last, value);
	case 27:
		return ::fast_io::details::from_chars_integral_fixed_base<27u>(first, last, value);
	case 28:
		return ::fast_io::details::from_chars_integral_fixed_base<28u>(first, last, value);
	case 29:
		return ::fast_io::details::from_chars_integral_fixed_base<29u>(first, last, value);
	case 30:
		return ::fast_io::details::from_chars_integral_fixed_base<30u>(first, last, value);
	case 31:
		return ::fast_io::details::from_chars_integral_fixed_base<31u>(first, last, value);
	case 32:
		return ::fast_io::details::from_chars_integral_fixed_base<32u>(first, last, value);
	case 33:
		return ::fast_io::details::from_chars_integral_fixed_base<33u>(first, last, value);
	case 34:
		return ::fast_io::details::from_chars_integral_fixed_base<34u>(first, last, value);
	case 35:
		return ::fast_io::details::from_chars_integral_fixed_base<35u>(first, last, value);
	case 36:
		return ::fast_io::details::from_chars_integral_fixed_base<36u>(first, last, value);
#endif
	default:
		break;
	}
	return ::fast_io::details::from_chars_integral_runtime_base_compact(
		first, last, value, static_cast<unsigned>(base));
#else
	return ::fast_io::details::from_chars_integral_literal_base(first, last, value,
																base);
#endif
}

template <::fast_io::details::character char_type>
inline ::fast_io::basic_from_chars_result<char_type>
from_chars(char_type const *, char_type const *, bool &, int = 10) = delete;

} // namespace fast_io

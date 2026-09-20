#pragma once


namespace fast_io
{

namespace details
{

template <::fast_io::details::character char_type>
inline consteval auto generate_to_chars_runtime_digits() noexcept
{
	::fast_io::freestanding::array<char_type, 36u> table;
	for (char8_t digit{}; digit != 36u; ++digit)
	{
		table[digit] = ::fast_io::details::charliteralofnumber<char_type, false>(digit);
	}
	return table;
}

template <::fast_io::details::character char_type>
inline constexpr auto to_chars_runtime_digits{generate_to_chars_runtime_digits<char_type>()};

template <::fast_io::details::character char_type, unsigned base, unsigned digits>
	requires((base == 2u && digits == 4u) || ((base == 8u || base == 16u) && digits == 2u))
inline consteval auto generate_to_chars_runtime_power_digits() noexcept
{
	constexpr ::std::size_t values{base == 2u ? 16u : base * base};
	::fast_io::freestanding::array<char_type, values * digits> table;
	for (::std::size_t value{}; value != values; ++value)
	{
		::std::size_t temporary{value};
		for (::std::size_t position{digits}; position != 0u; --position)
		{
			table[value * digits + position - 1u] =
				::fast_io::details::charliteralofnumber<char_type, false>(
					static_cast<char8_t>(temporary % base));
			temporary /= base;
		}
	}
	return table;
}

template <::fast_io::details::character char_type, unsigned base, unsigned digits>
inline constexpr auto to_chars_runtime_power_digits{
	::fast_io::details::generate_to_chars_runtime_power_digits<char_type, base, digits>()};

/*
For a non-power-of-two divisor d, let k=floor(log2(d)) and

    m = ceil(2^64 * (2^(k+1)-d) / d).

The branch-free invariant used below is

    floor(n/d) = ((((n-umulh(n,m)) >> 1) + umulh(n,m)) >> k)

for every uint64_t n.  The three tables instantiate m for d=base, base^2, and
base^4.  Their zero entries correspond exactly to power-of-two bases; the
runtime power-of-two switch returns before any such entry can be read.
*/
inline constexpr ::fast_io::freestanding::array<::std::uint_least64_t, 35u>
	to_chars_runtime_division_magic{
		0x0000000000000000ULL, 0x5555555555555556ULL, 0x0000000000000000ULL,
		0x999999999999999aULL, 0x5555555555555556ULL, 0x2492492492492493ULL,
		0x0000000000000000ULL, 0xc71c71c71c71c71dULL, 0x999999999999999aULL,
		0x745d1745d1745d18ULL, 0x5555555555555556ULL, 0x3b13b13b13b13b14ULL,
		0x2492492492492493ULL, 0x1111111111111112ULL, 0x0000000000000000ULL,
		0xe1e1e1e1e1e1e1e2ULL, 0xc71c71c71c71c71dULL, 0xaf286bca1af286bdULL,
		0x999999999999999aULL, 0x8618618618618619ULL, 0x745d1745d1745d18ULL,
		0x642c8590b21642c9ULL, 0x5555555555555556ULL, 0x47ae147ae147ae15ULL,
		0x3b13b13b13b13b14ULL, 0x2f684bda12f684beULL, 0x2492492492492493ULL,
		0x1a7b9611a7b9611bULL, 0x1111111111111112ULL, 0x0842108421084211ULL,
		0x0000000000000000ULL, 0xf07c1f07c1f07c20ULL, 0xe1e1e1e1e1e1e1e2ULL,
		0xd41d41d41d41d41eULL, 0xc71c71c71c71c71dULL};

inline constexpr ::fast_io::freestanding::array<::std::uint_least64_t, 35u>
	to_chars_runtime_division_base4_magic{
		0x0000000000000000ULL, 0x948b0fcd6e9e0653ULL, 0x0000000000000000ULL,
		0xa36e2eb1c432ca58ULL, 0x948b0fcd6e9e0653ULL, 0xb4b985cf97efcb1eULL,
		0x0000000000000000ULL, 0x3fa39ab547994db0ULL, 0xa36e2eb1c432ca58ULL,
		0x1e7a02e70c778749ULL, 0x948b0fcd6e9e0653ULL, 0x25b55f2e54c64dadULL,
		0xb4b985cf97efcb1eULL, 0x4b66dc33f6acdf16ULL, 0x0000000000000000ULL,
		0x91bf9a3091ccf10eULL, 0x3fa39ab547994db0ULL, 0x0179a9f4ca8f1cebULL,
		0xa36e2eb1c432ca58ULL, 0x5911016e4bcd44d5ULL, 0x1e7a02e70c778749ULL,
		0xdf9f13166086565bULL, 0x948b0fcd6e9e0653ULL, 0x5798ee2308c39dfaULL,
		0x25b55f2e54c64dadULL, 0xf91bd1b62b9cec8bULL, 0xb4b985cf97efcb1eULL,
		0x7b8813d37e4522d0ULL, 0x4b66dc33f6acdf16ULL, 0x22aa4d5fac2a7594ULL,
		0x0000000000000000ULL, 0xc4b42a833cc986c5ULL, 0x91bf9a3091ccf10eULL,
		0x65c3ceb16ef32218ULL, 0x3fa39ab547994db0ULL};

inline constexpr ::fast_io::freestanding::array<::std::uint_least64_t, 35u>
	to_chars_runtime_division_base2_magic{
		0x0000000000000000ULL, 0xc71c71c71c71c71dULL, 0x0000000000000000ULL,
		0x47ae147ae147ae15ULL, 0xc71c71c71c71c71dULL, 0x4e5e0a72f0539783ULL,
		0x0000000000000000ULL, 0x948b0fcd6e9e0653ULL, 0x47ae147ae147ae15ULL,
		0x0ecf56be69c8fde3ULL, 0xc71c71c71c71c71dULL, 0x83c977ab2bedd28fULL,
		0x4e5e0a72f0539783ULL, 0x23456789abcdf013ULL, 0x0000000000000000ULL,
		0xc5894d10d4985c20ULL, 0x948b0fcd6e9e0653ULL, 0x6b1490aa31a3cfc8ULL,
		0x47ae147ae147ae15ULL, 0x293725bb804a4dcaULL, 0x0ecf56be69c8fde3ULL,
		0xef8bdb389ebacc39ULL, 0xc71c71c71c71c71dULL, 0xa36e2eb1c432ca58ULL,
		0x83c977ab2bedd28fULL, 0x67980e0bf08c7766ULL, 0x4e5e0a72f0539783ULL,
		0x37b48248727447d7ULL, 0x23456789abcdf013ULL, 0x10c8531d0952d8d8ULL,
		0x0000000000000000ULL, 0xe1709a3611655193ULL, 0xc5894d10d4985c20ULL,
		0xabfd7e03c2fa5b89ULL, 0x948b0fcd6e9e0653ULL};

inline constexpr ::std::uint_least64_t
to_chars_runtime_divide_u64(::std::uint_least64_t value, ::std::uint_least64_t magic,
							unsigned shift) noexcept
{
	auto const high{::fast_io::intrinsics::umulh(value, magic)};
	auto const adjusted{((value - high) >> 1u) + high};
	return adjusted >> shift;
}

template <::fast_io::details::my_unsigned_integral U>
inline constexpr ::std::size_t to_chars_runtime_bit_width(U value) noexcept
{
	constexpr ::std::size_t bits{::std::numeric_limits<U>::digits};
	// __SIZEOF_INT128__ is a type-availability test, not a performance gate.
	// Splitting into uint64_t halves avoids requiring a library countl_zero
	// overload for a non-standard integer type.
#if defined(__SIZEOF_INT128__)
	if constexpr (sizeof(U) == sizeof(__uint128_t))
	{
		auto const high{static_cast<::std::uint_least64_t>(value >> 64u)};
		if (high != 0u)
		{
			return bits - static_cast<::std::size_t>(::std::countl_zero(high));
		}
		auto const low{static_cast<::std::uint_least64_t>(value) |
					   static_cast<::std::uint_least64_t>(1u)};
		return 64u - static_cast<::std::size_t>(::std::countl_zero(low));
	}
	else
#endif
	{
		return bits - static_cast<::std::size_t>(
						  ::std::countl_zero(static_cast<U>(value | static_cast<U>(1u))));
	}
}

/*
The public decimal result must end immediately after a one-digit value.  The
JEAIII reserve primitive is also used by internal pointer-returning formatters
and may stage a two-character table entry for such a value, so result-returning
entry points need an exact one-character case.  This switch selects where that
case is placed; it never removes the check.

When this constant is true, the capacity-checked caller enters
to_chars_integral_decimal_unchecked, which performs the one-digit test and then
instantiates a JEAIII specialization whose single-digit precondition is known.
Otherwise the full-result JEAIII specialization owns the test.  Encoding the
state as a template argument keeps the invariant visible to the optimizer and
gives the two layouts distinct specializations and mangled identities.
In particular, single_digit_checked is part of the jeaiii_main specialization,
so the proved and conservative callees have distinct mangled identities.

The selection is deliberately code-generation-specific.  Paired hardware
measurements favored the caller-side layout by roughly five percent for M4
mixed/long decimal workloads.  Cross-target Cortex-A76 and Neoverse-N2
assembly removes two instructions from the one-digit entry, and llvm-mca
reports the corresponding modeled pipeline cost.  On x86-64, GCC 15 and Clang
21 favored that layout, while GCC 13, 14, and 16 did not.  Unknown x86-64
compiler layouts and other architectures therefore keep the conservative
result-core placement.  Static assembly and llvm-mca evidence is not a claim
about unmeasured hardware.  The AArch64 choice is ISA-wide rather than
microarchitecture dispatch: M4 supplies the native measurement, whereas the
Cortex-A76 and Neoverse-N2 results are cross-target static evidence.  Other
AArch64 compiler lowerings inherit the semantically equivalent layout without
a native performance claim.
*/
inline constexpr bool to_chars_use_decimal_unchecked_helper{
#if defined(__aarch64__) || defined(_M_ARM64)
	true
#elif ((defined(__x86_64__) || defined(_M_X64)) && !defined(__arm64ec__) && \
	   !defined(_M_ARM64EC)) &&                                                   \
	((defined(__clang__) && __clang_major__ == 21) ||                            \
	 (defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 15))
	true
#else
	false
#endif
};

template <::std::size_t base, ::fast_io::details::my_unsigned_integral U,
		  ::fast_io::details::character char_type>
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_integral_checked(char_type *first, char_type *last, U value, bool negative) noexcept
{
	::std::size_t const digits{::fast_io::details::chars_len<base, false>(value)};
	::std::size_t const length{digits + static_cast<::std::size_t>(negative)};
	if (static_cast<::std::size_t>(last - first) < length) [[unlikely]]
	{
		return {last, ::fast_io::charconv_errc::value_too_large};
	}
	if (negative)
	{
		*first++ = ::fast_io::char_literal_v<u8'-', char_type>;
	}
	if constexpr (base == 10u)
	{
		if (value < 10u)
		{
			*first = ::fast_io::char_literal_add<char_type>(value);
			return {first + 1u, {}};
		}
	}
	/*
	chars_len has already proved that value has exactly the number of radix-base
	digits recorded in digits
	(including one digit for zero), and the preceding capacity test proves that
	[first, first + digits) is writable after an optional sign.  The precise
	writer consumes that known end pointer and digit count and fills exactly that
	range backwards.  Consequently it returns the same logical end without
	repeating the length classification performed by the ordinary reserve entry.

	Decimal and power-of-two bases retain their dedicated kernels; this reuse is
	for the generic bases whose normal entry would otherwise recompute the same
	length.  Paired M4 measurements motivated the routing, and independent linked
	code-size probes showed smaller public literal-base roots.  A direct
	`digits == 1` branch inside the shared precise writer was measured separately
	and rejected: although it accelerated that microcase, uniformly distributed
	multi-digit input slowed by 0.8--9.5% across GCC 13--16 and Clang 18--21, and
	representative code grew by as much as 85%.  Keeping the branch out of the
	shared writer is therefore an aggregate and code-size decision; it does not
	imply that every individual value or target is faster.
	*/
	if constexpr (base != 10u && base != 2u && base != 4u && base != 8u && base != 16u &&
				  base != 32u)
	{
		auto *const result{first + digits};
		::fast_io::details::print_reserve_integral_withfull_precise_main_impl<base, false>(
			result, value, digits);
		return {result, {}};
	}
	return {::fast_io::details::print_reserve_integral_withfull_main_impl<false, base, false>(first, value), {}};
}

/*
"Unchecked" refers only to buffer capacity: every caller has already proved
space for the maximum decimal representation (and has emitted the sign, if
any).  This helper still handles values below ten exactly.  After that branch,
value >= 10 is the precondition represented by single_digit_checked=true in
jeaiii_main.  Passing the state as a template argument removes a duplicate
comparison without weakening the public output-range invariant.

char_literal_add preserves the execution character set for char, all supported
wide character types, and EBCDIC.  The AVX-512 implementation is restricted to
one-byte non-EBCDIC output from an unsigned type whose storage width matches
uint_least64_t.  The AVX-512 path is excluded from constant evaluation; all
remaining multi-digit types and constexpr calls use the scalar JEAIII
specialization.
*/
template <::fast_io::details::my_unsigned_integral U, ::fast_io::details::character char_type>
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_integral_decimal_unchecked(char_type *first, U value) noexcept
{
	if (value < 10u)
	{
		*first = ::fast_io::char_literal_add<char_type>(value);
		return {first + 1u, {}};
	}
	/*
	Every AVX-512 guard in this file names the complete compile-time ISA contract
	of the Champagne--Lemire decimal kernel: IFMA supplies its multiply-add, VBMI
	its byte permutation, and BW+VL the used byte/word vector forms.  Nested type
	and encoding constraints prove a one-byte ASCII-compatible representation of
	an unsigned type whose storage width matches uint_least64_t; constant
	evaluation and all other cases use JEAIII.  This is target dispatch, not
	run-time CPUID.  SDE correctness and assembly inspection exist, but no native
	throughput claim is made for the opaque virtualized host.  llvm-mca did not
	select this path; compiler and microarchitecture combinations not named by
	the recorded checks inherit only the semantic ISA contract, not a throughput
	claim.  They retain the scalar JEAIII fallback whenever any feature, type,
	encoding, or constant-evaluation condition is false.
	*/
#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)
	if constexpr (sizeof(char_type) == 1u && ::fast_io::details::is_ascii<char_type> &&
				  sizeof(U) == sizeof(::std::uint_least64_t))
	{
		if (!::std::is_constant_evaluated())
		{
			return {::fast_io::details::jeaiii::champagne_lemire_main_for_char_type(
						first, static_cast<::std::uint_least64_t>(value)),
					{}};
		}
	}
#endif
	return ::fast_io::details::jeaiii::jeaiii_main<
		false, false, char_type, ::fast_io::basic_to_chars_result<char_type>, true>(
		first, value);
}

// Define this capacity-fast wrapper only when its AVX-512 consumer can be
// formed.  Other builds call the fixed decimal/JEAIII path directly.  The
// force-inline request changes call placement only: the capacity proof and the
// selected decimal writer are identical when an implementation does not expose
// the attribute and therefore uses ordinary inline semantics.  No retained
// isolated wrapper A/B result selects this attribute; it is a conservative
// wrapper-collapse policy pending native compiler revalidation and adds no
// throughput claim to the AVX-512 evidence recorded above.
#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)
template <::fast_io::details::my_unsigned_integral U, ::fast_io::details::character char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_integral_decimal(char_type *first, char_type *last, U value, bool negative) noexcept
{
	constexpr ::std::size_t maximum_digits{::fast_io::details::cal_max_int_size<U, 10u>()};
	if (static_cast<::std::size_t>(last - first) <
		maximum_digits + static_cast<::std::size_t>(negative)) [[unlikely]]
	{
		return ::fast_io::details::to_chars_integral_checked<10u>(first, last, value, negative);
	}
	if (negative)
	{
		*first++ = ::fast_io::char_literal_v<u8'-', char_type>;
	}
	// The semantic proof, measured compiler set, static AArch64 evidence,
	// unmeasured-target caveat, and conservative arm are documented at
	// to_chars_use_decimal_unchecked_helper.
	if constexpr (::fast_io::details::to_chars_use_decimal_unchecked_helper)
	{
		return ::fast_io::details::to_chars_integral_decimal_unchecked(first, value);
	}
	else
	{
		if (value < 10u)
		{
			*first = ::fast_io::char_literal_add<char_type>(value);
			return {first + 1u, {}};
		}
		if constexpr (sizeof(char_type) == 1u && ::fast_io::details::is_ascii<char_type> &&
					  sizeof(U) == sizeof(::std::uint_least64_t))
		{
			if (!::std::is_constant_evaluated())
			{
				return {::fast_io::details::jeaiii::champagne_lemire_main_for_char_type(
							first, static_cast<::std::uint_least64_t>(value)),
						{}};
			}
		}
		return ::fast_io::details::jeaiii::jeaiii_main<
			false, false, char_type, ::fast_io::basic_to_chars_result<char_type>>(first, value);
	}
}
#endif

template <::std::size_t base, ::fast_io::details::my_unsigned_integral U,
		  ::fast_io::details::character char_type>
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_integral_fixed_base(char_type *first, char_type *last, U value, bool negative) noexcept
{
	constexpr ::std::size_t maximum_digits{::fast_io::details::cal_max_int_size<U, base>()};
	if (static_cast<::std::size_t>(last - first) <
		maximum_digits + static_cast<::std::size_t>(negative)) [[unlikely]]
	{
		return ::fast_io::details::to_chars_integral_checked<base>(first, last, value, negative);
	}
	if (negative)
	{
		*first++ = ::fast_io::char_literal_v<u8'-', char_type>;
	}
	if constexpr (base == 10u && (::std::numeric_limits<::std::uint_least32_t>::digits == 32u))
	{
		// Consume the documented decimal-layout policy; targets outside its
		// measured set keep the conservative result-core placement.
		if constexpr (::fast_io::details::to_chars_use_decimal_unchecked_helper)
		{
			return ::fast_io::details::to_chars_integral_decimal_unchecked(first, value);
		}
		else
		{
			// Same ISA, encoding, type, and constant-evaluation proof as the primary
			// AVX-512 dispatch above.
#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)
			if constexpr (sizeof(char_type) == 1u && ::fast_io::details::is_ascii<char_type> &&
						  sizeof(U) == sizeof(::std::uint_least64_t))
			{
				if (value < 10u)
				{
					*first = ::fast_io::char_literal_add<char_type>(value);
					return {first + 1u, {}};
				}
				if (!::std::is_constant_evaluated())
				{
					return {::fast_io::details::jeaiii::champagne_lemire_main_for_char_type(
								first, static_cast<::std::uint_least64_t>(value)),
							{}};
				}
			}
#endif
			return ::fast_io::details::jeaiii::jeaiii_main<
				false, false, char_type, ::fast_io::basic_to_chars_result<char_type>>(first, value);
		}
	}
	else if constexpr (base == 2u || base == 4u || base == 8u || base == 16u || base == 32u)
	{
		if constexpr (::fast_io::details::need_seperate_print<U>)
		{
			return {::fast_io::details::print_reserve_integral_withfull_main_impl<
						false, base, false>(first, value),
					{}};
		}
		else
		{
			return ::fast_io::details::print_reserve_power_of_two_main<
				base, false, char_type, ::fast_io::basic_to_chars_result<char_type>>(first, value);
		}
	}
	return {::fast_io::details::print_reserve_integral_withfull_main_impl<false, base, false>(first, value), {}};
}

template <::fast_io::details::my_unsigned_integral U, ::fast_io::details::character char_type>
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars_integral_runtime_base_compact(char_type *first, char_type *last, U value,
									   bool negative, unsigned base) noexcept
{
	constexpr auto const *digit_table{::fast_io::details::to_chars_runtime_digits<char_type>.data()};
	::std::size_t const sign_size{static_cast<::std::size_t>(negative)};
	::std::size_t const available{static_cast<::std::size_t>(last - first)};
	if (value < static_cast<U>(base))
	{
		if (available < sign_size + 1u) [[unlikely]]
		{
			return {last, ::fast_io::charconv_errc::value_too_large};
		}
		if (negative)
		{
			*first++ = ::fast_io::char_literal_v<u8'-', char_type>;
		}
		*first = digit_table[static_cast<::std::size_t>(value)];
		return {first + 1u, {}};
	}

	unsigned shift{};
	switch (base)
	{
	case 2u:
		shift = 1u;
		break;
	case 4u:
		shift = 2u;
		break;
	case 8u:
		shift = 3u;
		break;
	case 16u:
		shift = 4u;
		break;
	case 32u:
		shift = 5u;
		break;
	default:
		break;
	}
	if (shift != 0u)
	{
		::std::size_t const bit_width{::fast_io::details::to_chars_runtime_bit_width(value)};
		::std::size_t const digits{(bit_width - 1u) / shift + 1u};
		::std::size_t const length{digits + sign_size};
		if (available < length) [[unlikely]]
		{
			return {last, ::fast_io::charconv_errc::value_too_large};
		}
		char_type *const result{first + length};
		char_type *iter{result};
		U const mask{static_cast<U>(base - 1u)};
		if (base == 2u)
		{
			constexpr auto const *table{
				::fast_io::details::to_chars_runtime_power_digits<char_type, 2u, 4u>.data()};
			while (value >= static_cast<U>(16u))
			{
				::std::size_t const index{static_cast<::std::size_t>(value & static_cast<U>(15u)) * 4u};
				iter -= 4u;
				::fast_io::details::non_overlapped_copy_n(table + index, 4u, iter);
				value >>= 4u;
			}
		}
		else if (base == 8u)
		{
			constexpr auto const *table{
				::fast_io::details::to_chars_runtime_power_digits<char_type, 8u, 2u>.data()};
			while (value >= static_cast<U>(64u))
			{
				::std::size_t const index{static_cast<::std::size_t>(value & static_cast<U>(63u)) * 2u};
				iter -= 2u;
				::fast_io::details::non_overlapped_copy_n(table + index, 2u, iter);
				value >>= 6u;
			}
		}
		else if (base == 16u)
		{
			/*
			The compact runtime dispatcher is unusually sensitive to added code and
			register pressure.  Real paired full-result-ABI measurements showed that
			inlining the 16-digit SSSE3 kernel materially improves GCC 16's base-16
			case while preserving its mixed-base aggregate.  Enabling the same branch
			for GCC 13-15 or Clang regressed at least one mixed/power-base group, so the
			gate is intentionally narrower than the fixed-base SSSE3 facility.  GCC 16
			is the measured boundary; the >= 16 spelling lets later GCC releases inherit
			the path but is not evidence about versions that have not been benchmarked.

			The remaining conditions are semantic as well as performance guards:
			digits == 16 excludes leading-zero output; one-byte non-EBCDIC output is
			required by the ASCII lookup; constant evaluation and SSE2 use the scalar
			path; and ARM64EC is not treated as an x86-64 SSSE3 target.  GCC 16 inlines
			the ordinary-inline helper here, avoiding a call-frame cost.  Static
			llvm-mca results for the SIMD block were used only as pipeline support;
			the compiler gate is based on whole-call hardware measurements.
			*/
#if (defined(__x86_64__) || defined(_M_X64)) && !defined(__arm64ec__) && \
	!defined(_M_ARM64EC) && defined(__SSSE3__) &&                        \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_pshufb128) &&                     \
	FAST_IO_HAS_BUILTIN(__builtin_ia32_psrlwi128) && defined(__GNUC__) && \
	!defined(__clang__) && __GNUC__ >= 16
			if constexpr (sizeof(U) == sizeof(::std::uint_least64_t) &&
						  sizeof(char_type) == 1u && ::fast_io::details::is_ascii<char_type>)
			{
				if (!::std::is_constant_evaluated() && digits == 16u)
				{
					auto *output_first{first};
					if (negative)
					{
						*output_first++ = ::fast_io::char_literal_v<u8'-', char_type>;
					}
					::fast_io::details::print_reserve_hexadecimal_16_ssse3<false>(
						output_first, value);
					return {result, {}};
				}
			}
#endif
			constexpr auto const *table{
				::fast_io::details::to_chars_runtime_power_digits<char_type, 16u, 2u>.data()};
			if constexpr (::std::numeric_limits<U>::digits > 8u)
			{
				while (value >= 256u)
				{
					::std::size_t const index{
						static_cast<::std::size_t>(value & static_cast<U>(255u)) * 2u};
					iter -= 2u;
					::fast_io::details::non_overlapped_copy_n(table + index, 2u, iter);
					value >>= 8u;
				}
			}
		}
		do
		{
			*--iter = digit_table[static_cast<::std::size_t>(value & mask)];
			value >>= shift;
		} while (value != 0u);
		if (negative)
		{
			*first = ::fast_io::char_literal_v<u8'-', char_type>;
		}
		return {result, {}};
	}

	using working_type = ::std::conditional_t<(sizeof(U) < sizeof(unsigned)), unsigned, U>;
	working_type const divisor{static_cast<working_type>(base)};
	working_type const divisor2{divisor * divisor};
	working_type const divisor3{divisor2 * divisor};
	working_type const divisor4{divisor2 * divisor2};
	working_type temporary{static_cast<working_type>(value)};
	::std::size_t digits{};
	for (;;)
	{
		if (temporary < divisor)
		{
			digits += 1u;
			break;
		}
		if (temporary < divisor2)
		{
			digits += 2u;
			break;
		}
		if (temporary < divisor3)
		{
			digits += 3u;
			break;
		}
		if (temporary < divisor4)
		{
			digits += 4u;
			break;
		}
		/*
		For native x86-64, the per-base table and the branch-free reciprocal
		invariant documented above compute exactly floor(n/base^4) over the complete
		uint64_t domain; the surrounding comparisons have already excluded the
		terminal cases.  The `/ divisor4` arm computes the same quotient for
		other targets and wider types.  The architecture guard is conservative code
		generation, and ARM64EC is not treated as native x86-64.  M4 native screening
		rejected widening this reciprocal graph; traditional AArch64 evidence is
		static only and therefore does not justify widening the gate.

		The retained x86 compact-dispatch report measured the complete dynamic entry
		on an i9-14900HX with GCC 15 and Clang 21 and inspected the reciprocal loop
		with Raptor-Lake and Zen-4 llvm-mca models.  Those whole-entry results do not
		isolate reciprocal division from the rest of dispatch, and llvm-mca is static
		scheduling evidence.  The x86 selection is therefore a conservative
		code-generation policy, not a universal throughput claim: other x86 cores and
		frontends inherit only the complete-domain quotient proof.  The direct
		`temporary /= divisor4` arm remains the semantically equivalent fallback.
		*/
#if (defined(__x86_64__) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
		if constexpr (sizeof(working_type) <= sizeof(::std::uint_least64_t))
		{
			auto const magic{
				::fast_io::details::to_chars_runtime_division_base4_magic[base - 2u]};
			auto const wide_divisor{static_cast<::std::uint_least64_t>(divisor4)};
			unsigned const divider_shift{::std::numeric_limits<::std::uint_least64_t>::digits - 1u -
										 static_cast<unsigned>(::std::countl_zero(wide_divisor))};
			temporary = static_cast<working_type>(::fast_io::details::to_chars_runtime_divide_u64(
				static_cast<::std::uint_least64_t>(temporary), magic, divider_shift));
		}
		else
#endif
		{
			temporary /= divisor4;
		}
		digits += 4u;
	}
	::std::size_t const length{digits + sign_size};
	if (available < length) [[unlikely]]
	{
		return {last, ::fast_io::charconv_errc::value_too_large};
	}
	char_type *const result{first + length};
	char_type *iter{result};
	/*
	The output loop applies the documented complete-domain reciprocal identity
	first to base^2 and then to base, reconstructing each remainder as
	n-quotient*divisor.  Each digit index is therefore in [0, base).  The fallback
	direct-division loop has the same text and end-pointer semantics.  This second
	x86 guard consumes the evidence boundary and unmeasured-target caveat stated
	at the base^4 length loop above; it adds no independent performance claim.
	*/
#if (defined(__x86_64__) || defined(_M_X64)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	if constexpr (sizeof(working_type) <= sizeof(::std::uint_least64_t))
	{
		auto const magic{::fast_io::details::to_chars_runtime_division_magic[base - 2u]};
		auto const pair_magic{
			::fast_io::details::to_chars_runtime_division_base2_magic[base - 2u]};
		unsigned const divider_shift{
			::std::numeric_limits<unsigned>::digits - 1u - static_cast<unsigned>(::std::countl_zero(base))};
		unsigned const pair_divider_shift{
			::std::numeric_limits<unsigned>::digits - 1u -
			static_cast<unsigned>(::std::countl_zero(static_cast<unsigned>(divisor2)))};
		::std::uint_least64_t output_value{static_cast<::std::uint_least64_t>(value)};
		auto const wide_base{static_cast<::std::uint_least64_t>(base)};
		auto const wide_divisor2{static_cast<::std::uint_least64_t>(divisor2)};
		while (output_value >= wide_divisor2)
		{
			auto const pair_quotient{::fast_io::details::to_chars_runtime_divide_u64(
				output_value, pair_magic, pair_divider_shift)};
			auto const pair_remainder{output_value - pair_quotient * wide_divisor2};
			auto const high_digit{::fast_io::details::to_chars_runtime_divide_u64(
				pair_remainder, magic, divider_shift)};
			auto const low_digit{pair_remainder - high_digit * wide_base};
			iter -= 2u;
			iter[0] = digit_table[static_cast<::std::size_t>(high_digit)];
			iter[1] = digit_table[static_cast<::std::size_t>(low_digit)];
			output_value = pair_quotient;
		}
		if (output_value >= wide_base)
		{
			auto const high_digit{::fast_io::details::to_chars_runtime_divide_u64(
				output_value, magic, divider_shift)};
			auto const low_digit{output_value - high_digit * wide_base};
			iter -= 2u;
			iter[0] = digit_table[static_cast<::std::size_t>(high_digit)];
			iter[1] = digit_table[static_cast<::std::size_t>(low_digit)];
		}
		else
		{
			*--iter = digit_table[static_cast<::std::size_t>(output_value)];
		}
	}
	else
#endif
	{
		working_type output_value{static_cast<working_type>(value)};
		do
		{
			working_type const quotient{output_value / divisor};
			auto const remainder{static_cast<::std::size_t>(output_value - quotient * divisor)};
			*--iter = digit_table[remainder];
			output_value = quotient;
		} while (output_value != 0u);
	}
	if (negative)
	{
		*first = ::fast_io::char_literal_v<u8'-', char_type>;
	}
	return {result, {}};
}

} // namespace details

template <::fast_io::details::my_integral T, ::fast_io::details::character char_type>
	requires(!::std::same_as<::std::remove_cv_t<T>, bool>)
// The non-Clang GNU-compatible branch needs call-site inlining for
// __builtin_constant_p(base) below to observe a literal.  GCC 13--16 linked
// code-size probes verified that literal bases remove the runtime graph; other
// GNU-compatible frontends inherit this layout and require revalidation.  Clang
// performs the propagation without this attribute.
#if defined(__GNUC__) && !defined(__clang__)
[[gnu::always_inline]]
#endif
inline constexpr ::fast_io::basic_to_chars_result<char_type>
to_chars(char_type *first, char_type *last, T value, int base = 10) noexcept
{
	// Valid calls already satisfy the standard [2, 36] radix precondition.  The
	// optional attribute is optimizer information, not algorithm dispatch.
	FAST_IO_ASSUME(2 <= base && base <= 36);
	using unsigned_type = ::fast_io::details::my_make_unsigned_t<T>;
	bool negative{};
	unsigned_type magnitude{static_cast<unsigned_type>(value)};
	if constexpr (::fast_io::details::my_signed_integral<T>)
	{
		if (value < 0)
		{
			negative = true;
			magnitude = static_cast<unsigned_type>(unsigned_type{} - magnitude);
		}
	}

	/*
	GCC and Clang expose __builtin_constant_p, so a literal passed through this
	runtime-base API can call the corresponding fixed-base template directly.
	Substituting the known literal proves equivalence.  If base is not known, the
	tests do not select an arm and execution reaches the single compact dynamic
	implementation.  Linked assembly and code-size probes verified elimination
	of the dynamic graph for literal bases 2--36; this is not an llvm-mca claim.
	*/
#if defined(__GNUC__) || defined(__clang__)
	if (__builtin_constant_p(base) && base == 2)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<2u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 3)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<3u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 4)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<4u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 5)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<5u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 6)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<6u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 7)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<7u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 8)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<8u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 9)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<9u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 10)
	{
		// A literal decimal radix may use the capacity-fast AVX-512 wrapper;
		// all other compile targets use the ordinary fixed-base specialization.
#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)
		if constexpr (::std::numeric_limits<::std::uint_least32_t>::digits == 32u)
		{
			return ::fast_io::details::to_chars_integral_decimal(first, last, magnitude, negative);
		}
		else
		{
			return ::fast_io::details::to_chars_integral_fixed_base<10u>(first, last, magnitude, negative);
		}
#else
		return ::fast_io::details::to_chars_integral_fixed_base<10u>(first, last, magnitude, negative);
#endif
	}
	if (__builtin_constant_p(base) && base == 11)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<11u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 12)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<12u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 13)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<13u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 14)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<14u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 15)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<15u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 16)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<16u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 17)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<17u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 18)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<18u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 19)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<19u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 20)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<20u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 21)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<21u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 22)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<22u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 23)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<23u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 24)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<24u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 25)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<25u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 26)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<26u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 27)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<27u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 28)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<28u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 29)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<29u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 30)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<30u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 31)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<31u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 32)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<32u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 33)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<33u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 34)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<34u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 35)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<35u>(first, last, magnitude, negative);
	}
	if (__builtin_constant_p(base) && base == 36)
	{
		return ::fast_io::details::to_chars_integral_fixed_base<36u>(first, last, magnitude, negative);
	}
#endif

	if (base == 10) [[likely]]
	{
		// Dynamic decimal input uses the same AVX-512 contract; its capacity
		// check and scalar fallback preserve identical API behavior.
#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)
		if constexpr (::std::numeric_limits<::std::uint_least32_t>::digits == 32u)
		{
			return ::fast_io::details::to_chars_integral_decimal(first, last, magnitude, negative);
		}
		else
		{
			return ::fast_io::details::to_chars_integral_fixed_base<10u>(first, last, magnitude, negative);
		}
#else
		constexpr ::std::size_t maximum_decimal_digits{
			::fast_io::details::cal_max_int_size<unsigned_type, 10u>()};
		if (static_cast<::std::size_t>(last - first) <
			maximum_decimal_digits + static_cast<::std::size_t>(negative)) [[unlikely]]
		{
			return ::fast_io::details::to_chars_integral_checked<10u>(first, last, magnitude, negative);
		}
		if (negative)
		{
			*first++ = ::fast_io::char_literal_v<u8'-', char_type>;
		}
		// Consume the documented decimal-layout policy; this call site adds no
		// performance claim beyond the native/static scope recorded there.
		if constexpr (::fast_io::details::to_chars_use_decimal_unchecked_helper)
		{
			return ::fast_io::details::to_chars_integral_decimal_unchecked(first, magnitude);
		}
		else
		{
			return ::fast_io::details::jeaiii::jeaiii_main<
				false, false, char_type, ::fast_io::basic_to_chars_result<char_type>>(first, magnitude);
		}
#endif
	}

	return ::fast_io::details::to_chars_integral_runtime_base_compact(
		first, last, magnitude, negative, static_cast<unsigned>(base));
}

template <::fast_io::details::character char_type>
inline ::fast_io::basic_to_chars_result<char_type>
to_chars(char_type *, char_type *, bool, int = 10) = delete;

} // namespace fast_io

#pragma once

/*
This implementation requires the complete compile-target contract used by its
instruction sequence: IFMA for 52-bit integer fused multiply-add, VBMI for the
byte permutation, and BW+VL for the byte/word masked forms.  The guard prevents
the declarations and raw builtins from entering any weaker target; it is not a
run-time CPUID dispatch.  SDE correctness and generated assembly were checked,
but the archived virtualized host does not support a native throughput claim.
*/
#if defined(__AVX512IFMA__) && defined(__AVX512VBMI__) && defined(__AVX512BW__) && defined(__AVX512VL__)

namespace fast_io::details::jeaiii
{

using champagne_lemire_i64x8 [[__gnu__::__vector_size__(64)]] = long long;
using champagne_lemire_i8x64 [[__gnu__::__vector_size__(64)]] = char;
using champagne_lemire_i8x16 [[__gnu__::__vector_size__(16)]] = char;

#if defined(__GNUC__) && !defined(__clang__)
// Keep the non-Clang GNU-compatible branch's constant broadcasts as
// memory-source instructions.  In measured GCC 13--16 assembly, materializing
// them through GPRs spills a dead ZMM value and adds shuffle-port pressure.
// Other frontends admitted by this macro inherit the layout without a measured
// performance claim and require revalidation.
static constexpr ::std::uint_least64_t champagne_lemire_ten{10u};
static constexpr ::std::uint_least64_t champagne_lemire_zero{static_cast<::std::uint_least64_t>(u8'0')};
#endif

/*
The scalar/SIMD crossover depends on compiler scheduling and register
allocation, but it must remain uniform within one compiler and ISA contract.
For nine digits, GCC 13--16 static regions favor the scalar path on every
tested Zen 4/5 model.  Sapphire Rapids and Granite Rapids favor scalar for
GCC 13/14 but SIMD for GCC 15/16; the combined compiler/model matrix still
favors scalar.  Starting the GNU-compatible vector path at ten digits is
therefore the composite policy.  Only GCC 13--16 were measured; earlier,
later, and non-GCC GNU-compatible frontends inherit that threshold and require
revalidation.  In particular, no __tune_* macro selects a different algorithm
within the same AVX-512 ISA.  Clang retains the pre-existing five-digit
threshold; the GCC matrix does not provide evidence for changing a different
compiler's lowering.  Unrecognized frontends retain the legacy eleven-digit
cutoff because no lowering evidence justifies entering the vector path earlier.
*/
#if defined(__clang__)
inline constexpr ::std::uint_least64_t champagne_lemire_threshold{
	static_cast<::std::uint_least64_t>(10000u)}; // 5 digits
#elif defined(__GNUC__)
inline constexpr ::std::uint_least64_t champagne_lemire_threshold{
	static_cast<::std::uint_least64_t>(1000000000u)}; // 10 digits
#else
inline constexpr ::std::uint_least64_t champagne_lemire_threshold{
	static_cast<::std::uint_least64_t>(10000000000ull)}; // 11 digits
#endif

inline constexpr ::std::uint_least64_t champagne_lemire_power10_table[]{
	static_cast<::std::uint_least64_t>(1u),
	static_cast<::std::uint_least64_t>(10u),
	static_cast<::std::uint_least64_t>(100u),
	static_cast<::std::uint_least64_t>(1000u),
	static_cast<::std::uint_least64_t>(10000u),
	static_cast<::std::uint_least64_t>(100000u),
	static_cast<::std::uint_least64_t>(1000000u),
	static_cast<::std::uint_least64_t>(10000000u),
	static_cast<::std::uint_least64_t>(100000000u),
	static_cast<::std::uint_least64_t>(1000000000u),
	static_cast<::std::uint_least64_t>(10000000000ull),
	static_cast<::std::uint_least64_t>(100000000000ull),
	static_cast<::std::uint_least64_t>(1000000000000ull),
	static_cast<::std::uint_least64_t>(10000000000000ull),
	static_cast<::std::uint_least64_t>(100000000000000ull),
	static_cast<::std::uint_least64_t>(1000000000000000ull),
	static_cast<::std::uint_least64_t>(10000000000000000ull),
	static_cast<::std::uint_least64_t>(100000000000000000ull),
	static_cast<::std::uint_least64_t>(1000000000000000000ull),
	static_cast<::std::uint_least64_t>(10000000000000000000ull)};

inline constexpr ::std::size_t champagne_lemire_digits(::std::uint_least64_t value) noexcept
{
	if (value == 0u)
	{
		return 1u;
	}
	unsigned const bit_width{64u -
							 static_cast<unsigned>(__builtin_clzll(static_cast<unsigned long long>(value)))};
	/*
	For every bit_width b in [1,64], (b*1233)>>12 equals floor(b*log10(2)).
	Indeed, 0 < log10(2)-1233/4096 < 4.606e-6, so the accumulated error is below
	2.95e-4; direct enumeration of this finite 64-value domain gives a minimum
	fractional part of about 0.0102999 (at b=10), well above that error.  A value
	with that bit width can therefore have only estimate or estimate+1 decimal
	digits.  Comparing against 10^estimate selects the latter exactly; estimate
	is at most 19, within the table.
	*/
	unsigned const estimate{(bit_width * 1233u) >> 12u};
	return estimate + static_cast<unsigned>(value >= champagne_lemire_power10_table[estimate]);
}

inline champagne_lemire_i8x16 champagne_lemire_16_digits_from_groups(::std::uint_least64_t high,
																	 ::std::uint_least64_t low) noexcept
{
	FAST_IO_ASSUME(high < static_cast<::std::uint_least64_t>(100000000u));
	FAST_IO_ASSUME(low < static_cast<::std::uint_least64_t>(100000000u));
	constexpr ::std::uint_least64_t two52{static_cast<::std::uint_least64_t>(1u) << 52u};
	champagne_lemire_i64x8 const multipliers{
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(100000000u)),
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(10000000u)),
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(1000000u)),
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(100000u)),
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(10000u)),
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(1000u)),
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(100u)),
		static_cast<long long>(two52 / static_cast<::std::uint_least64_t>(10u))};
	champagne_lemire_i64x8 const high_values{static_cast<long long>(high), static_cast<long long>(high),
											 static_cast<long long>(high), static_cast<long long>(high), static_cast<long long>(high),
											 static_cast<long long>(high), static_cast<long long>(high), static_cast<long long>(high)};
	champagne_lemire_i64x8 const low_values{static_cast<long long>(low), static_cast<long long>(low),
											static_cast<long long>(low), static_cast<long long>(low), static_cast<long long>(low),
											static_cast<long long>(low), static_cast<long long>(low), static_cast<long long>(low)};
	// Clang and the supported non-Clang branch expose equivalent IFMA operations
	// with different builtin signatures; the latter carries an explicit
	// all-lanes mask.
#if defined(__clang__)
	champagne_lemire_i64x8 const high_remainders{
		__builtin_ia32_vpmadd52luq512(multipliers, high_values, multipliers)};
	champagne_lemire_i64x8 const low_remainders{
		__builtin_ia32_vpmadd52luq512(multipliers, low_values, multipliers)};
#else
	champagne_lemire_i64x8 const high_remainders{
		__builtin_ia32_vpmadd52luq512_mask(multipliers, high_values, multipliers, static_cast<unsigned char>(-1))};
	champagne_lemire_i64x8 const low_remainders{
		__builtin_ia32_vpmadd52luq512_mask(multipliers, low_values, multipliers, static_cast<unsigned char>(-1))};
#endif
	// Clang accepts vector constants directly.  The supported non-Clang branch
	// uses memory-source broadcasts to implement the same all-lane values while
	// avoiding the dead-ZMM spill observed with GCC; this is compiler lowering,
	// not a different numeric algorithm.
#if defined(__clang__)
	champagne_lemire_i64x8 const tens{10, 10, 10, 10, 10, 10, 10, 10};
	champagne_lemire_i64x8 const zeroes{u8'0', u8'0', u8'0', u8'0', u8'0', u8'0', u8'0', u8'0'};
	champagne_lemire_i64x8 const high_digits{
		__builtin_ia32_vpmadd52huq512(zeroes, tens, high_remainders)};
	champagne_lemire_i64x8 const low_digits{__builtin_ia32_vpmadd52huq512(zeroes, tens, low_remainders)};
#else
	champagne_lemire_i64x8 tens_for_gcc;
	champagne_lemire_i64x8 zeroes_for_gcc;
	__asm__("vpbroadcastq {%1, %0|%0, %1}" : "=v"(tens_for_gcc) : "m"(champagne_lemire_ten));
	__asm__("vpbroadcastq {%1, %0|%0, %1}" : "=v"(zeroes_for_gcc) : "m"(champagne_lemire_zero));
	champagne_lemire_i64x8 const high_digits{
		__builtin_ia32_vpmadd52huq512_mask(zeroes_for_gcc, tens_for_gcc, high_remainders,
										   static_cast<unsigned char>(-1))};
	champagne_lemire_i64x8 const low_digits{
		__builtin_ia32_vpmadd52huq512_mask(zeroes_for_gcc, tens_for_gcc, low_remainders,
										   static_cast<unsigned char>(-1))};
#endif
	champagne_lemire_i8x64 const indices{0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38,
										 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78};
	champagne_lemire_i8x64 const high_bytes{__builtin_bit_cast(champagne_lemire_i8x64, high_digits)};
	champagne_lemire_i8x64 const low_bytes{__builtin_bit_cast(champagne_lemire_i8x64, low_digits)};
	// The two compiler builtin spellings apply the same index vector and select
	// the same sixteen output bytes; the supported non-Clang branch supplies an
	// explicit all-lanes mask.
#if defined(__clang__)
	champagne_lemire_i8x64 const digits{__builtin_ia32_vpermi2varqi512(high_bytes, indices, low_bytes)};
#else
	champagne_lemire_i8x64 const digits{__builtin_ia32_vpermt2varqi512_mask(
		indices, high_bytes, low_bytes, static_cast<unsigned long long>(-1))};
#endif
	return __builtin_shufflevector(digits, digits, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
}

#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline char8_t *champagne_lemire_write_digits(char8_t *iter, champagne_lemire_i8x16 digits,
											  ::std::size_t length) noexcept
{
	if (length == 16u)
	{
		__builtin_memcpy(iter, &digits, sizeof(digits));
		return iter + 16u;
	}
	unsigned short const mask{static_cast<unsigned short>(0xffffu << (16u - length))};
	::std::uintptr_t const address{reinterpret_cast<::std::uintptr_t>(iter) + length - 16u};
	// Clang and the supported non-Clang branch give the masked-store builtin
	// different pointer-parameter types.  The computed address, sixteen-byte
	// value, and high-length-bit mask are otherwise identical.
#if defined(__clang__)
	__builtin_ia32_storedquqi128_mask(reinterpret_cast<champagne_lemire_i8x16 *>(address), digits, mask);
#else
	__builtin_ia32_storedquqi128_mask(reinterpret_cast<char *>(address), digits, mask);
#endif
	return iter + length;
}

#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline char8_t *champagne_lemire_main(char8_t *iter, ::std::uint_least64_t value) noexcept
{
	constexpr ::std::uint_least64_t divisor{static_cast<::std::uint_least64_t>(100000000u)};
	if (value >= static_cast<::std::uint_least64_t>(100000000000000u))
	{
		::std::uint_least64_t const quotient{value / divisor};
		::std::uint_least64_t const low{value - quotient * divisor};
		if (value < static_cast<::std::uint_least64_t>(10000000000000000u))
		{
			::std::size_t const length{15u +
									   (value >= static_cast<::std::uint_least64_t>(1000000000000000u))};
			champagne_lemire_i8x16 const digits{champagne_lemire_16_digits_from_groups(quotient, low)};
			return champagne_lemire_write_digits(iter, digits, length);
		}
		constexpr ::std::uint_least64_t wide_divisor{divisor * divisor};
		::std::uint_least64_t const high{value / wide_divisor};
		::std::uint_least64_t const middle{quotient - high * divisor};
		champagne_lemire_i8x16 const digits{champagne_lemire_16_digits_from_groups(middle, low)};
		char8_t *const low_iter{high < 100u ? jeaiii_first_two(iter, static_cast<::std::uint_least32_t>(high))
											: jeaiii_range4(iter, static_cast<::std::uint_least32_t>(high))};
		return champagne_lemire_write_digits(low_iter, digits, 16u);
	}
	if (value < champagne_lemire_threshold)
	{
		return jeaiii_main(iter, value);
	}
	::std::size_t const length{champagne_lemire_digits(value)};
	::std::uint_least64_t const quotient{value / divisor};
	::std::uint_least64_t const low{value - quotient * divisor};
	champagne_lemire_i8x16 const digits{champagne_lemire_16_digits_from_groups(quotient, low)};
	return champagne_lemire_write_digits(iter, digits, length);
}

template <::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline char_type *champagne_lemire_main_for_char_type(char_type *iter, ::std::uint_least64_t value) noexcept
{
	if constexpr (::std::same_as<char_type, char8_t>)
	{
		return champagne_lemire_main(iter, value);
	}
	else
	{
		using char8_t_may_alias_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
			[[__gnu__::__may_alias__]]
#endif
			= char8_t *;
		char8_t_may_alias_ptr const u8iter{reinterpret_cast<char8_t_may_alias_ptr>(iter)};
		return reinterpret_cast<char_type *>(champagne_lemire_main(u8iter, value));
	}
}

} // namespace fast_io::details::jeaiii

#endif

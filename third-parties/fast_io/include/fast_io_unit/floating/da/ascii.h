#pragma once

namespace fast_io::details::da
{

// ASCII-only emission backend for DA carriers.  Callers select this file only
// when the execution character set represents '0', '.', ',', 'e', 'E', '+' and
// '-' with their ASCII byte values.  EBCDIC and non-char destinations remain on
// the character-generic floating writers, so none of the byte packing below is
// allowed to leak into those paths.
//
// Several routines intentionally store a complete 8- or 16-byte scratch block
// and return an earlier logical end.  Their destination is therefore the full
// reserve-print buffer, not a buffer sized to the returned character count.
// This physical-store contract is stated again at each public local writer.

// Up to sixteen ASCII digits in destination byte order.  low contains the first
// eight bytes, high the next eight, and span is the logical digit count after
// suppressing leading zeroes.  Bytes outside that span are initialized scratch
// bytes and may still be written by a fixed-width store.
struct ascii_digit_block
{
	::std::uint_least64_t low;
	::std::uint_least64_t high;
	::std::uint_least32_t span;
};

inline constexpr ::std::uint_least64_t ascii_zeroes{static_cast<::std::uint_least64_t>(0x3030303030303030)};
inline constexpr ::std::uint_least64_t ascii_div10000_multiplier{static_cast<::std::uint_least64_t>(109951163)};
inline constexpr ::std::uint_least64_t ascii_div100_multiplier{static_cast<::std::uint_least64_t>(5243)};
inline constexpr ::std::uint_least64_t ascii_div10_multiplier{static_cast<::std::uint_least64_t>(103)};

// Splitting a binary64 DA carrier at 10^8 uses
// C = 0xabcc77118461cefd = ceil(2^90 / 10^8).  Write
// C * 10^8 = 2^90 + 875776.  For every reachable 0 <= x < 10^16,
// x * 875776 < 2^90; the reciprocal perturbation is therefore smaller than one
// 10^-8 quotient step.  Hence floor(x * C / 2^90) = floor(x / 10^8).  This is
// the exact domain used by both SIMD backends below, not an approximation.

// GCC 13--15 Linux System V x86-64 assembly audits select one aligned SIMD
// divisor object; GCC 16 and later GNU frontends inherit the newer
// immediate/rematerialized spelling, which does not enlarge the surrounding
// live range.  This is a code-generation transition, not arithmetic: both
// forms execute the same reciprocal division identities.  x32, MinGW, the
// Microsoft ABI, non-Linux x86-64 and other compilers also use rematerialized
// constants.  Moving the transition requires whole-caller constant-load, spill,
// call, dependency-chain and linked-text-size evidence.
inline constexpr bool ascii_x86_cached_bcd_constants_default{
	// The unselected configuration does not instantiate the opaque cached-address
	// dependency.  It computes the same quotients and emits the same bytes.
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__ && __GNUC__ < 16 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	true
#else
	false
#endif
};

// Convert 0 <= value < 10^8 to eight unpacked numeric bytes.  The reciprocal
// stages form exact quotient/remainder pairs for 10^4, 10^2 and 10 without a
// division instruction.  The returned word contains bytes in [0, 9], not
// character codes; adding ascii_zeroes converts all eight lanes to ASCII.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::uint_least64_t
ascii_bcd8(::std::uint_least64_t value) noexcept
{
	auto const four_digit_pairs{value + static_cast<::std::uint_least64_t>(4294957296) *
											((value * ascii_div10000_multiplier) >> 40u)};
	auto const two_digit_pairs{four_digit_pairs + static_cast<::std::uint_least64_t>(65436) *
													  (((four_digit_pairs * ascii_div100_multiplier) >> 19u) & static_cast<::std::uint_least64_t>(0x7f0000007f))};
	auto const digits{two_digit_pairs + static_cast<::std::uint_least64_t>(246) *
											(((two_digit_pairs * ascii_div10_multiplier) >> 10u) & static_cast<::std::uint_least64_t>(0xf000f000f000f))};
	return ::fast_io::byte_swap(digits);
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::uint_least32_t
ascii_bcd8_span(::std::uint_least64_t bcd) noexcept
{
	if (!bcd)
	{
		return 0u;
	}
	return static_cast<::std::uint_least32_t>(
		8u - static_cast<::std::uint_least32_t>(::std::countl_zero(bcd) >> 3u));
}

// The AArch64 backend uses vector arithmetic because four independent
// quotient/remainder lanes map directly to sqdmulh and narrowing/shuffle
// instructions.  Compiler builtins are used instead of the vendor intrinsics
// header, so this freestanding header acquires no hosted-header dependency.
//
// The byte-layout proof for this backend is deliberately little-endian: the
// explicit shuffle indices, the vector-to-u64 bit_cast, and packed[0]/packed[1]
// all use the AArch64 little-endian correspondence between increasing vector
// byte indices and increasing destination addresses.  Lane arithmetic itself
// is endian-neutral, but those representation steps are not.  Therefore an
// AArch64 big-endian translation unit must use the scalar writer until it has a
// separately proved shuffle/store mapping.  Only Clang and GCC are admitted:
// the implementation below names frontend-specific builtin spellings exercised
// by the current compile matrix, so a third-party frontend that merely defines
// an AArch64 target macro must not be assumed to provide either spelling.  These
// builtins are an internal source dependency, not a stable cross-frontend ABI
// contract.  The scalar implementation is the semantic fallback on every other
// ISA or unsupported compiler ABI.
#if (defined(__aarch64__) || defined(__arm64__)) && !defined(__AARCH64EB__) && \
	(defined(__clang__) || defined(__GNUC__))
using ascii_i8x16 [[gnu::vector_size(16)]] = signed char;
using ascii_u8x16 [[gnu::vector_size(16)]] = unsigned char;
using ascii_u8x8 [[gnu::vector_size(8)]] = unsigned char;
using ascii_i16x8 [[gnu::vector_size(16)]] = short;
using ascii_u16x8 [[gnu::vector_size(16)]] = unsigned short;
using ascii_u16x4 [[gnu::vector_size(8)]] = unsigned short;
using ascii_i32x2 [[gnu::vector_size(8)]] = int;
using ascii_i32x4 [[gnu::vector_size(16)]] = int;
using ascii_u64x2 [[gnu::vector_size(16)]] = unsigned long long;

struct ascii_aarch64_binary32_digit_data
{
	ascii_digit_block digits;
	ascii_u8x16 unshuffled;
};

// Clang and GCC expose different builtin spellings for the same signed,
// saturating, doubling high multiply.  Saturation is unreachable for the BCD
// input bounds below; consequently both branches return the identical lane-wise
// high product.  The preprocessor split expresses builtin availability only.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_i32x4
ascii_qdmulh(ascii_i32x4 value, int multiplier) noexcept
{
	// Semantic equivalence: for the nonnegative bounded BCD lanes used here,
	// saturation cannot occur, so both builtin spellings compute the same four
	// signed doubling-high products.  The split expresses frontend builtin
	// availability; it is not a target-performance claim.
#if defined(__clang__)
	return __builtin_bit_cast(ascii_i32x4,
							  __builtin_neon_vqdmulhq_v(__builtin_bit_cast(ascii_i8x16, value),
														__builtin_bit_cast(ascii_i8x16, ascii_i32x4{multiplier, multiplier, multiplier, multiplier}), 34));
#else
	return __builtin_aarch64_sqdmulh_nv4si(value, multiplier);
#endif
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_i32x2
ascii_qdmulh(ascii_i32x2 value, int multiplier) noexcept
{
	// Semantic equivalence is the same as for the four-lane overload: the proved
	// BCD domain excludes the sole signed saturation case.  Clang requires its
	// raw NEON builtin spelling while GCC exposes the AArch64 vector builtin;
	// choosing the spelling does not change the two lane results.
#if defined(__clang__)
	using i8x8 [[gnu::vector_size(8)]] = signed char;
	return __builtin_bit_cast(ascii_i32x2,
							  __builtin_neon_vqdmulh_v(__builtin_bit_cast(i8x8, value),
													   __builtin_bit_cast(i8x8, ascii_i32x2{multiplier, multiplier}), 2));
#else
	return __builtin_aarch64_sqdmulh_nv2si(value, multiplier);
#endif
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_i16x8
ascii_qdmulh(ascii_i16x8 value, short multiplier) noexcept
{
	// Semantic equivalence: every input lane and multiplier is inside the
	// nonsaturating BCD range, hence both builtins return the identical eight
	// signed doubling-high products.  This conditional selects a compiler API,
	// not a different arithmetic algorithm.
#if defined(__clang__)
	return __builtin_bit_cast(ascii_i16x8,
							  __builtin_neon_vqdmulhq_v(__builtin_bit_cast(ascii_i8x16, value),
														__builtin_bit_cast(ascii_i8x16,
																		   ascii_i16x8{multiplier, multiplier, multiplier, multiplier,
																					   multiplier, multiplier, multiplier, multiplier}),
														33));
#else
	return __builtin_aarch64_sqdmulh_nv8hi(value, multiplier);
#endif
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_u8x16
ascii_bcd4x4(ascii_i32x4 value) noexcept
{
	// Semantic identity: an empty assembly statement with one read/write vector
	// operand cannot alter any lane.  Apple Clang 23/M4 disassembly is the retained
	// code-generation evidence: the opaque use preserves one vector value across
	// the sqdmulh stages instead of rematerializing lanes.  Other GNU-compatible
	// AArch64 frontends selected here inherit that placement as an unmeasured
	// compiler-family hypothesis and require their own assembly audit.
	// GNU extended assembly is admitted only in GNU-driver modes.  `_MSC_VER`
	// excludes clang-cl, whose source and ABI contracts are not covered by the
	// retained assembly artifact.  This empty read/write barrier changes register
	// placement only; omitting it leaves every vector lane and emitted byte
	// unchanged.
#if !defined(_MSC_VER) && \
	(defined(__clang__) || (defined(__GNUC__) && !defined(__clang__)))
	__asm__("" : "+w"(value));
#endif
	auto const hundreds{::fast_io::details::da::ascii_qdmulh(value, 21475328)};
	auto const pairs{__builtin_bit_cast(ascii_i16x8,
										value + hundreds * ascii_i32x4{65436, 65436, 65436, 65436})};
	auto const tens{::fast_io::details::da::ascii_qdmulh(pairs, static_cast<short>(3296))};
	return __builtin_bit_cast(ascii_u8x16,
							  pairs + tens * ascii_i16x8{246, 246, 246, 246, 246, 246, 246, 246});
}

// Preserve both representations produced by the same BCD computation.  The
// destination-order word serves fixed notation, while the natural four-lane
// byte order serves the AArch64 TBL scientific writer below.  Keeping the
// vector here is not a second digit conversion: byte_swap is a bijection, so
// `digits.low-ascii_zeroes` and `unshuffled` contain exactly the same eight
// decimal digits in reverse orders.  Returning both lets notation selection
// consume whichever order avoids a vector-to-GPR-to-vector round trip.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_aarch64_binary32_digit_data
make_ascii_binary32_digit_data_simd(::std::uint_least64_t value) noexcept
{
	auto const pairs{value + static_cast<::std::uint_least64_t>(4294957296) *
								 ((value * ascii_div10000_multiplier) >> 40u)};
	auto const unshuffled{::fast_io::details::da::ascii_bcd4x4(ascii_i32x4{
		static_cast<int>(static_cast<::std::uint_least32_t>(pairs)),
		static_cast<int>(static_cast<::std::uint_least32_t>(pairs >> 32u)), 0, 0})};
	auto const raw{__builtin_bit_cast(ascii_u64x2, unshuffled)[0]};
	auto const bcd{::fast_io::byte_swap(raw)};
	/*
	Each numeric byte is in [0,9].  Consequently a byte is nonzero exactly when
	its corresponding decimal digit is nonzero.  The least significant nonzero
	byte of `raw` becomes the most significant nonzero byte of `bcd`; dividing
	countr_zero(raw) by eight therefore counts leading zero digits.  The explicit
	zero branch is required because countr_zero(0) has the full word width and the
	coefficient carrier is permitted to contain an all-zero scratch block.
	*/
	auto const span{raw ? static_cast<::std::uint_least32_t>(
							  8u - (static_cast<::std::uint_least32_t>(
										::std::countr_zero(raw)) >>
									3u))
						: 0u};
	return {{bcd + ascii_zeroes, 0u, span}, unshuffled};
}

// Each input lane is in [0, 9999].  The result contains four unpacked numeric
// digits per lane; the later byte shuffle establishes destination order.
template <typename flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_digit_block
make_ascii_digit_block_simd(::std::uint_least64_t value) noexcept
{
	if constexpr (sizeof(flt) <= sizeof(float))
	{
		return ::fast_io::details::da::make_ascii_binary32_digit_data_simd(value).digits;
	}
	else
	{
		// Mathematical equivalence: on 0 <= value < 10^16 the reciprocal
		// identity proved above gives exactly floor(value / 10^8).  Native u128
		// merely spells the multiply-and-shift directly; the fallback division
		// computes the same quotient.  This is a capability gate, not empirical
		// compiler scheduling policy.
#if defined(__SIZEOF_INT128__)
		auto const high_value{static_cast<::std::uint_least64_t>(
			(static_cast<__uint128_t>(value) * static_cast<::std::uint_least64_t>(0xabcc77118461cefd)) >> 90u)};
#else
		// Semantic fallback for targets without a native 128-bit integer type.
		auto const high_value{value / static_cast<::std::uint_least64_t>(100000000)};
#endif
		auto const low_value{value - high_value * static_cast<::std::uint_least64_t>(100000000)};
		auto const combined{ascii_i32x2{
			static_cast<int>(static_cast<::std::uint_least32_t>(high_value)),
			static_cast<int>(static_cast<::std::uint_least32_t>(low_value))}};
		auto const high_limbs{::fast_io::details::da::ascii_qdmulh(
								  combined, static_cast<int>(ascii_div10000_multiplier)) >>
							  9u};
		auto const packed_limbs{combined + high_limbs * ascii_i32x2{55536, 55536}};
		auto const limbs{__builtin_convertvector(
			__builtin_bit_cast(ascii_u16x4, packed_limbs), ascii_i32x4)};
		auto const unshuffled{::fast_io::details::da::ascii_bcd4x4(limbs)};
		auto const digits{__builtin_shufflevector(
			unshuffled, unshuffled, 7, 6, 5, 4, 3, 2, 1, 0,
			15, 14, 13, 12, 11, 10, 9, 8)};
		auto const nonzero_bytes{__builtin_bit_cast(ascii_i8x16, digits) > 0};
		auto const nonzero_words{__builtin_bit_cast(ascii_u16x8, nonzero_bytes) >> 4u};
		auto const nonzero_mask{__builtin_bit_cast(::std::uint_least64_t,
												   __builtin_convertvector(nonzero_words, ascii_u8x8))};
		auto const span{static_cast<::std::uint_least32_t>(
			16u - (static_cast<::std::uint_least32_t>(::std::countl_zero(nonzero_mask)) >> 2u))};
		auto const ascii_digits{digits + ascii_u8x16{
											 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48}};
		auto const packed{__builtin_bit_cast(ascii_u64x2, ascii_digits)};
		return {packed[0], packed[1], span};
	}
}

#endif

// pshufb requires SSSE3, but the adjacent four-lane BCD conversion also needs
// 32-bit lane multiplication.  Without SSE4.1, Clang 23 and GCC 13/15 synthesize
// it from several SSE2 pmuludq operations.  A 2026-07 i9-14900HX CPU8 paired
// audit regressed f32 by 10--15% and GCC 15 f64 by 15%, and enlarged the probe
// object by 2.7--5.3%.  SSE4.1 is therefore the measured backend-selection
// floor, not a formatting-correctness requirement.  Revalidate latency, table
// footprint, frames and spills before changing this predicate.  Other x86
// targets use the byte-identical scalar backend.
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
using ascii_x86_i8x16 [[gnu::vector_size(16)]] = signed char;
using ascii_x86_c8x16 [[gnu::vector_size(16)]] = char;
using ascii_x86_u8x16 [[gnu::vector_size(16)]] = unsigned char;
using ascii_x86_i16x8 [[gnu::vector_size(16)]] = short;
using ascii_x86_u16x8 [[gnu::vector_size(16)]] = unsigned short;
using ascii_x86_u32x4 [[gnu::vector_size(16)]] = unsigned int;
using ascii_x86_i32x4 [[gnu::vector_size(16)]] = int;
using ascii_x86_u64x2 [[gnu::vector_size(16)]] = unsigned long long;

struct alignas(16) ascii_x86_bcd_constant_cache
{
	ascii_x86_u16x8 div100;
	ascii_x86_u32x4 neg100;
	ascii_x86_u16x8 div10;
	ascii_x86_u16x8 neg10;
};

// Hidden visibility prevents an inline cache from becoming an interposable
// load on object formats that implement the GNU attribute.  The mode is a
// narrow string literal; GCC advertises the attribute under IBM1047 but rejects
// the execution-set translation of "hidden".  Therefore the ASCII conjunct is
// required for attribute syntax, while the surrounding ISA/backend guard
// independently determines whether these constants are consumed.
#if __has_cpp_attribute(__gnu__::__visibility__) && 'A' == 0x41
[[__gnu__::__visibility__("hidden")]]
#endif
inline constexpr ascii_x86_bcd_constant_cache ascii_x86_bcd_constants{
	{5243, 0, 5243, 0, 5243, 0, 5243, 0},
	{65436u, 65436u, 65436u, 65436u},
	{6554, 6554, 6554, 6554, 6554, 6554, 6554, 6554},
	{246, 246, 246, 246, 246, 246, 246, 246}};

struct ascii_x86_digit_data
{
	ascii_digit_block digits;
	ascii_x86_u8x16 unshuffled;
};

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_x86_u16x8
ascii_x86_mul_high_u16(ascii_x86_u16x8 left, ascii_x86_u16x8 right) noexcept
{
	return __builtin_bit_cast(ascii_x86_u16x8,
							  __builtin_ia32_pmulhuw128(__builtin_bit_cast(ascii_x86_i16x8, left),
														__builtin_bit_cast(ascii_x86_i16x8, right)));
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_x86_u64x2
ascii_x86_mul_low_u32_to_u64(ascii_x86_u32x4 left, ascii_x86_u32x4 right) noexcept
{
	return __builtin_bit_cast(ascii_x86_u64x2,
							  __builtin_ia32_pmuludq128(__builtin_bit_cast(ascii_x86_i32x4, left),
														__builtin_bit_cast(ascii_x86_i32x4, right)));
}

template <bool use_cached_constants = ascii_x86_cached_bcd_constants_default>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_x86_u8x16
ascii_x86_bcd4x4(ascii_x86_u32x4 value) noexcept
{
	if constexpr (use_cached_constants)
	{
		auto constants{__builtin_addressof(ascii_x86_bcd_constants)};
		// Code-generation barrier only: keep the four divisor vectors behind
		// one opaque base address.  This function is not constexpr, so every
		// invocation is a runtime invocation; an is_constant_evaluated guard is
		// both redundant and diagnosed as tautological by GCC 13-15 under
		// -Werror.  Arithmetic is unchanged.  Revalidate loads, spills and calls
		// before removing the barrier itself.
#if !defined(_MSC_VER) && \
	(defined(__clang__) || (defined(__GNUC__) && !defined(__clang__)))
		// GNU extended assembly is unavailable in native MSVC and is not part of
		// the clang-cl source contract audited here.  Omitting this empty barrier
		// can only rematerialize the address; it cannot change a divisor or digit.
		__asm__("" : "+r"(constants));
#endif
		auto const hundreds_words{::fast_io::details::da::ascii_x86_mul_high_u16(
			__builtin_bit_cast(ascii_x86_u16x8, value),
			constants->div100)};
		auto const hundreds{__builtin_bit_cast(ascii_x86_u32x4, hundreds_words) >> 3u};
		auto const pairs{value + hundreds * constants->neg100};
		auto const tens{::fast_io::details::da::ascii_x86_mul_high_u16(
			__builtin_bit_cast(ascii_x86_u16x8, pairs),
			constants->div10)};
		return __builtin_bit_cast(ascii_x86_u8x16,
			__builtin_bit_cast(ascii_x86_u16x8, pairs) +
				tens * constants->neg10);
	}
	else
	{
		auto const hundreds_words{::fast_io::details::da::ascii_x86_mul_high_u16(
			__builtin_bit_cast(ascii_x86_u16x8, value),
			ascii_x86_u16x8{5243, 0, 5243, 0, 5243, 0, 5243, 0})};
		auto const hundreds{__builtin_bit_cast(ascii_x86_u32x4, hundreds_words) >> 3u};
		auto const pairs{value + hundreds * ascii_x86_u32x4{65436u, 65436u, 65436u, 65436u}};
		auto const tens{::fast_io::details::da::ascii_x86_mul_high_u16(
			__builtin_bit_cast(ascii_x86_u16x8, pairs),
			ascii_x86_u16x8{6554, 6554, 6554, 6554, 6554, 6554, 6554, 6554})};
		return __builtin_bit_cast(ascii_x86_u8x16,
								  __builtin_bit_cast(ascii_x86_u16x8, pairs) +
									  tens * ascii_x86_u16x8{246, 246, 246, 246, 246, 246, 246, 246});
	}
}

template <typename flt, bool use_cached_constants = ascii_x86_cached_bcd_constants_default>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_x86_digit_data
make_ascii_digit_data_x86(::std::uint_least64_t value) noexcept
{
	if constexpr (sizeof(flt) <= sizeof(float))
	{
		auto const pairs{value + static_cast<::std::uint_least64_t>(4294957296) *
									 ((value * ascii_div10000_multiplier) >> 40u)};
		/*
		On little-endian x86, bit-casting {pairs,0} places the low and high
		32-bit halves of pairs in BCD lanes zero and one and clears lanes two and
		three.  This is exactly the previous four-element vector initializer, but
		exposes the complete low 64-bit lane to the backend as one vmovq.
		*/
		auto const pair_lanes{__builtin_bit_cast(
			ascii_x86_u32x4, ascii_x86_u64x2{pairs, 0u})};
		auto const unshuffled{::fast_io::details::da::ascii_x86_bcd4x4<use_cached_constants>(
			pair_lanes)};
		auto const raw{__builtin_bit_cast(ascii_x86_u64x2, unshuffled)[0]};
		auto const span{raw ? static_cast<::std::uint_least32_t>(
								  8u - (static_cast<::std::uint_least32_t>(::std::countr_zero(raw)) >> 3u))
							: 0u};
		return {{::fast_io::byte_swap(raw) + ascii_zeroes, 0u, span}, unshuffled};
	}
	else
	{
		// Mathematical equivalence is identical to the AArch64 split: the proved
		// reciprocal returns floor(value / 10^8) throughout the carrier domain,
		// and exact division is the semantic fallback.  __SIZEOF_INT128__ selects
		// only the available source spelling and carries no ISA performance claim.
#if defined(__SIZEOF_INT128__)
		auto const high_value{static_cast<::std::uint_least64_t>(
			(static_cast<__uint128_t>(value) * static_cast<::std::uint_least64_t>(0xabcc77118461cefd)) >> 90u)};
#else
		// Exact, ISA-independent fallback when the compiler has no u128 type.
		auto const high_value{value / static_cast<::std::uint_least64_t>(100000000)};
#endif
		auto const low_value{value - high_value * static_cast<::std::uint_least64_t>(100000000)};
		auto const limbs{ascii_x86_u64x2{low_value, high_value}};
		auto const quotients{::fast_io::details::da::ascii_x86_mul_low_u32_to_u64(
								 __builtin_bit_cast(ascii_x86_u32x4, limbs),
								 ascii_x86_u32x4{static_cast<unsigned int>(ascii_div10000_multiplier), 0u,
												 static_cast<unsigned int>(ascii_div10000_multiplier), 0u}) >>
							 40u};
		auto const pairs{limbs + ::fast_io::details::da::ascii_x86_mul_low_u32_to_u64(
									 __builtin_bit_cast(ascii_x86_u32x4, quotients),
									 ascii_x86_u32x4{4294957296u, 0u, 4294957296u, 0u})};
		auto const unshuffled{::fast_io::details::da::ascii_x86_bcd4x4<use_cached_constants>(
			__builtin_bit_cast(ascii_x86_u32x4, pairs))};
		auto const nonzero_mask{static_cast<::std::uint_least32_t>(
			__builtin_ia32_pmovmskb128(__builtin_bit_cast(ascii_x86_c8x16,
														  __builtin_bit_cast(ascii_x86_i8x16, unshuffled) > 0)))};
		auto const span{nonzero_mask ? static_cast<::std::uint_least32_t>(
										   16u - static_cast<::std::uint_least32_t>(::std::countr_zero(nonzero_mask)))
									 : 0u};
		auto const shuffled{__builtin_bit_cast(ascii_x86_u8x16,
											   __builtin_ia32_pshufb128(__builtin_bit_cast(ascii_x86_c8x16, unshuffled),
																		ascii_x86_c8x16{15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0}))};
		auto const ascii_digits{shuffled + ascii_x86_u8x16{
											   48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48, 48}};
		auto const packed{__builtin_bit_cast(ascii_x86_u64x2, ascii_digits)};
		return {{packed[0], packed[1], span}, unshuffled};
	}
}

template <typename flt, bool use_cached_constants = ascii_x86_cached_bcd_constants_default>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_digit_block
make_ascii_digit_block_x86(::std::uint_least64_t value) noexcept
{
	return ::fast_io::details::da::make_ascii_digit_data_x86<flt, use_cached_constants>(value).digits;
}
#endif

template <typename flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ascii_digit_block
make_ascii_digit_block(::std::uint_least64_t value) noexcept
{
	if constexpr (sizeof(flt) <= sizeof(float))
	{
		auto const low_bcd{::fast_io::details::da::ascii_bcd8(value)};
		return {low_bcd + ascii_zeroes, 0u,
				::fast_io::details::da::ascii_bcd8_span(low_bcd)};
	}
	else
	{
		auto const high_value{value / static_cast<::std::uint_least64_t>(100000000)};
		auto const low_value{value - high_value * static_cast<::std::uint_least64_t>(100000000)};
		auto const high_bcd{::fast_io::details::da::ascii_bcd8(high_value)};
		auto const low_bcd{::fast_io::details::da::ascii_bcd8(low_value)};
		auto const span{low_bcd ? 8u + ::fast_io::details::da::ascii_bcd8_span(low_bcd)
								: ::fast_io::details::da::ascii_bcd8_span(high_bcd)};
		return {high_bcd + ascii_zeroes, low_bcd + ascii_zeroes, span};
	}
}

// Compile-time generated encodings for every finite binary64 scientific
// exponent in [-324, 308].  Bytes 0..4 contain e[+-]dd[d] in destination order
// and byte 7 contains the logical length.  The cache is ASCII-only; the writer
// below loads and stores all eight bytes even when only four or five are logical.
struct ascii_exponent_cache
{
	inline static constexpr ::std::int_least32_t minimum{-324};
	inline static constexpr ::std::int_least32_t maximum{308};
	::std::uint_least64_t data[static_cast<::std::size_t>(maximum - minimum + 1)]{};

	consteval ascii_exponent_cache() noexcept
	{
		for (auto exponent{minimum}; exponent <= maximum; ++exponent)
		{
			auto magnitude{static_cast<::std::uint_least32_t>(exponent < 0 ? -exponent : exponent)};
			::std::uint_least64_t packed{static_cast<::std::uint_least64_t>(u8'e') |
										 (static_cast<::std::uint_least64_t>(exponent < 0 ? u8'-' : u8'+') << 8u)};
			::std::uint_least32_t length{4u};
			if (100u <= magnitude)
			{
				packed |= static_cast<::std::uint_least64_t>(u8'0' + magnitude / 100u) << 16u;
				magnitude %= 100u;
				packed |= static_cast<::std::uint_least64_t>(u8'0' + magnitude / 10u) << 24u;
				packed |= static_cast<::std::uint_least64_t>(u8'0' + magnitude % 10u) << 32u;
				length = 5u;
			}
			else
			{
				packed |= static_cast<::std::uint_least64_t>(u8'0' + magnitude / 10u) << 16u;
				packed |= static_cast<::std::uint_least64_t>(u8'0' + magnitude % 10u) << 24u;
			}
			data[static_cast<::std::size_t>(exponent - minimum)] =
				packed | (static_cast<::std::uint_least64_t>(length) << 56u);
		}
	}
};

// As above, hidden visibility protects direct table addressing when the
// execution set can spell the GNU attribute's narrow "hidden" argument.  The
// writer, not this optional linkage decoration, selects character encoding.
#if __has_cpp_attribute(__gnu__::__visibility__) && 'A' == 0x41
[[__gnu__::__visibility__("hidden")]]
#endif
inline constexpr ascii_exponent_cache ascii_exponents{};

// Generated fixed-layout metadata indexed by the scientific exponent.  Each
// entry records the leading-zero start, decimal-point slot, digit shift, logical
// end for coefficient lengths 1..17, and decimal's fixed/scientific decision.
// decimal_fixed_mask bit (length - 1) is one exactly when fixed is no longer
// than scientific; equality deliberately selects fixed.  The x86-only members
// additionally describe one-pshufb binary64 layouts.
struct ascii_fixed_layout_cache
{
	inline static constexpr ::std::int_least32_t minimum{-4};
	inline static constexpr ::std::int_least32_t compact_maximum{6};
	inline static constexpr ::std::int_least32_t binary64_shuffle_maximum{15};
	inline static constexpr ::std::int_least32_t maximum{22};
	// One SIMD entry occupies a cache-line-sized 64-byte slot so both shuffle
	// masks and scalar metadata share one indexed base.  The portable entry needs
	// only 32-byte alignment.  This changes physical layout, never formatting;
	// changes require checking address generation and cache footprint in assembly.
	inline static constexpr ::std::size_t entry_alignment{
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
		64u
#else
		32u
#endif
	};

	struct alignas(entry_alignment) entry
	{
		// These fields exist only when the x86 pshufb writer below is available.
		// Their absence on other targets halves each metadata entry and avoids an
		// unused architecture-specific table payload.
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
		::std::uint_least8_t binary64_shuffle[2][16]{};
		::std::uint_least8_t binary64_last_digit_position[2]{};
#endif
		::std::uint_least8_t start_position{};
		::std::uint_least8_t point_position{};
		::std::uint_least8_t shift_position{};
		::std::uint_least8_t end_position[17]{};
		::std::uint_least32_t decimal_fixed_mask{};
	};

	entry data[static_cast<::std::size_t>(maximum - minimum + 1)]{};

	consteval ascii_fixed_layout_cache() noexcept
	{
		for (auto exponent{minimum}; exponent <= maximum; ++exponent)
		{
			auto &layout{data[static_cast<::std::size_t>(exponent - minimum)]};
			// Generate exactly the fields consumed by the guarded binary64 pshufb
			// writer.  Compiling this loop elsewhere would create dead table data;
			// the scalar metadata below is generated for every target.
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
			for (::std::uint_least32_t extra{}; extra != 2u; ++extra)
			{
				auto source{static_cast<::std::uint_least8_t>(!extra)};
				auto const point_slot{0 <= exponent && exponent <= 14 ? exponent + 1 : 128};
				for (::std::uint_least32_t position{}; position != 16u; ++position)
				{
					layout.binary64_shuffle[extra][position] =
						static_cast<::std::uint_least8_t>(static_cast<::std::int_least32_t>(position) == point_slot
															  ? 0x80u
															  : source++);
				}
				auto const length{15u + extra};
				layout.binary64_last_digit_position[extra] = static_cast<::std::uint_least8_t>(
					length + static_cast<::std::uint_least32_t>(0 <= exponent && exponent < static_cast<::std::int_least32_t>(length)));
			}
#endif
			layout.start_position = static_cast<::std::uint_least8_t>(exponent < 0 ? 1 - exponent : 0);
			layout.point_position = static_cast<::std::uint_least8_t>(exponent < 0 ? 1 : exponent + 1);
			layout.shift_position = static_cast<::std::uint_least8_t>(
				layout.point_position + static_cast<::std::uint_least8_t>(0 <= exponent));
			for (::std::uint_least32_t length{1u}; length <= 17u; ++length)
			{
				auto end_position{static_cast<::std::int_least32_t>(length)};
				if (0 <= exponent)
				{
					end_position = static_cast<::std::int_least32_t>(length) > exponent + 1
									   ? static_cast<::std::int_least32_t>(length) + 1
									   : exponent + 1;
				}
				layout.end_position[length - 1u] = static_cast<::std::uint_least8_t>(end_position);
				::std::uint_least32_t fixed_length{};
				if (static_cast<::std::int_least32_t>(length) <= exponent)
				{
					fixed_length = static_cast<::std::uint_least32_t>(exponent + 1);
				}
				else if (0 <= exponent)
				{
					fixed_length = length + 2u - static_cast<::std::uint_least32_t>(static_cast<::std::int_least32_t>(length) == exponent + 1);
				}
				else
				{
					fixed_length = static_cast<::std::uint_least32_t>(-exponent) + length + 1u;
				}
				/*
				Within this cache X is in [-5,26], so the exponent suffix is
				exactly four characters (`e`, sign, two digits).  The coefficient
				is one character for L=1 and L+1 otherwise.
				*/
				auto const scientific_length{
					length == 1u ? length + 4u : length + 5u};
				layout.decimal_fixed_mask |= static_cast<::std::uint_least32_t>(
												 fixed_length <= scientific_length)
											 << (length - 1u);
			}
		}
	}
};

// Hidden visibility has the same direct-addressing purpose as for
// ascii_exponents.  The ASCII test is required because the GNU attribute mode
// itself is a narrow string literal, not because table semantics depend on the
// execution character set.
#if __has_cpp_attribute(__gnu__::__visibility__) && 'A' == 0x41
[[__gnu__::__visibility__("hidden")]]
#endif
inline constexpr ascii_fixed_layout_cache ascii_fixed_layouts{};

// Little-endian AArch64 binary32 and Clang 23 Linux System V x86-64 read a
// dense projection of decimal_fixed_mask before selecting a writer.  This
// deliberately duplicates 108 bytes so the decision neither scales the index
// by a 32- or 64-byte entry nor materializes the full-layout address on the
// scientific branch.
// Baseline, SSE4.1 and native Clang assembly all preserve a branch-free mask
// calculation before the unavoidable writer branch.  Replacing it with the
// closed-form length predicate introduces an additional exponent-dependent
// branch, while reading the canonical entry adds index/address work before
// notation is known.  The consteval copy proves bit identity with
// ascii_fixed_layouts. Compiler Explorer forced/base whole-consumer audits cover
// Clang 16--22 and trunk; none emits an instruction-identical replacement. A
// physical-core x86 Clang 22 ABBA/BAAB run was statistically neutral (overall ratio
// 0.999684) while the projection added 144 linked text bytes, so Clang 22 is a
// tested rejection rather than an omitted release. Current trunk Clang 24 also
// emits a different consumer sequence and adds the 108-byte projection without
// reducing its 553-instruction whole-consumer count. Only x86 Clang 23 retains
// that measured x86 tradeoff.  On AArch64 the projection also separates the
// mask load from the 32-byte fixed-layout address; this is valuable because the
// TBL scientific path otherwise carries a useless full-layout dependency.
// x32, MinGW, the Microsoft ABI, non-Linux x86-64 and scalar targets reuse the
// canonical layout. Change either transition only after whole-caller latency,
// text/data size, loads and spills are re-audited.
#if (((defined(__aarch64__) || defined(__arm64__)) && !defined(__AARCH64EB__) && \
	  (defined(__clang__) || defined(__GNUC__))) || \
	 (defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	  defined(__clang__) && __clang_major__ == 23 && \
	  !(defined(__arm64ec__) || defined(_M_ARM64EC))))
struct ascii_decimal_fixed_mask_cache
{
	::std::uint_least32_t data[static_cast<::std::size_t>(
		ascii_fixed_layout_cache::maximum - ascii_fixed_layout_cache::minimum + 1)]{};

	consteval ascii_decimal_fixed_mask_cache() noexcept
	{
		for (::std::size_t index{}; index != sizeof(data) / sizeof(*data); ++index)
		{
			data[index] = ascii_fixed_layouts.data[index].decimal_fixed_mask;
		}
	}
};

// This projection follows the same non-interposable linkage policy as its
// source table.  It owns distinct compact data by design, not a second layout;
// consteval assignment proves every projected word equals its source field.
// The ASCII conjunct only keeps the GNU narrow attribute argument valid.
#if __has_cpp_attribute(__gnu__::__visibility__) && 'A' == 0x41
[[__gnu__::__visibility__("hidden")]]
#endif
inline constexpr ascii_decimal_fixed_mask_cache ascii_decimal_fixed_masks{};
#endif

// Compile-time generated complete binary32 scientific shuffles for the
// little-endian AArch64 TBL and x86 pshufb backends.  The index is
// (digit_span - 1) * 4 + has_last_digit * 2 + has_extra_digit.  Source lanes
// 0..7 contain digits, 8..11 the packed exponent, 12 the optional last digit,
// and 13 the decimal point.  shuffle[15] is metadata holding the logical length
// and is outside every logical output.  Both instructions define an
// out-of-range source index as a zero byte, so the same generated permutation
// is a byte-for-byte proof object for both ISAs.  The table exists only when at
// least one proved consumer is compiled, preventing a 512-byte payload on
// scalar-only targets.
#if (((defined(__aarch64__) || defined(__arm64__)) && !defined(__AARCH64EB__) && \
	  (defined(__clang__) || defined(__GNUC__))) || \
	 ((defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	  (defined(__GNUC__) || defined(__clang__)) && \
	  !(defined(__arm64ec__) || defined(_M_ARM64EC))))
struct ascii_binary32_scientific_cache
{
	struct alignas(16) entry
	{
		::std::uint_least8_t shuffle[16]{};
	};

	entry data[32]{};

	consteval ascii_binary32_scientific_cache() noexcept
	{
		for (::std::uint_least32_t index{}; index != 32u; ++index)
		{
			auto &layout{data[index]};
			for (auto &position : layout.shuffle)
			{
				position = 0x80u;
			}
			auto const digit_span{index / 4u + 1u};
			auto const has_last_digit{static_cast<bool>((index >> 1u) & 1u)};
			auto const has_extra_digit{static_cast<bool>(index & 1u)};
			auto const leading_position{static_cast<::std::uint_least8_t>(has_extra_digit ? 7u : 6u)};
			::std::uint_least32_t length{};
			if (has_last_digit)
			{
				layout.shuffle[length++] = leading_position;
				layout.shuffle[length++] = 13u;
				for (auto position{static_cast<::std::int_least32_t>(leading_position) - 1};
					 0 <= position; --position)
				{
					layout.shuffle[length++] = static_cast<::std::uint_least8_t>(position);
				}
				layout.shuffle[length++] = 12u;
			}
			else
			{
				length = digit_span + static_cast<::std::uint_least32_t>(has_extra_digit);
				length -= static_cast<::std::uint_least32_t>(length == 2u);
				layout.shuffle[0] = leading_position;
				layout.shuffle[1] = 13u;
				for (::std::uint_least32_t position{2u}; position < length; ++position)
				{
					layout.shuffle[position] = static_cast<::std::uint_least8_t>(
						leading_position + 1u - position);
				}
			}
			for (::std::uint_least8_t exponent_position{8u}; exponent_position != 12u; ++exponent_position)
			{
				layout.shuffle[length++] = exponent_position;
			}
			layout.shuffle[15] = static_cast<::std::uint_least8_t>(length);
		}
	}
};

// Hidden visibility gives either shuffle consumer a direct table address.  The
// surrounding backend/ISA guard determines whether these ASCII lanes are
// consumed; the ASCII conjunct exists because the GNU visibility mode is a
// narrow string literal and is rejected by GCC after IBM1047 translation.
#if __has_cpp_attribute(__gnu__::__visibility__) && 'A' == 0x41
[[__gnu__::__visibility__("hidden")]]
#endif
inline constexpr ascii_binary32_scientific_cache ascii_binary32_scientific_layouts{};
#endif

// Write eight bytes for a binary32 carrier and sixteen for a binary64 carrier.
// digits.span and drop_leading_zero affect the logical contents/end only; they
// never reduce the physical store width.  The caller must supply the complete
// reserve-print buffer extent.
template <typename flt>
FAST_IO_GNU_ALWAYS_INLINE inline void store_ascii_digits(
	char *destination, ascii_digit_block digits, bool drop_leading_zero) noexcept
{
	if constexpr (sizeof(flt) <= sizeof(float))
	{
		auto const shifted{drop_leading_zero ? digits.low >> 8u : digits.low};
		::fast_io::freestanding::my_memcpy(
			destination, __builtin_addressof(shifted), sizeof(shifted));
	}
	else
	{
		auto low{digits.low};
		auto high{digits.high};
		if (drop_leading_zero)
		{
			low = (low >> 8u) | (high << 56u);
			high >>= 8u;
		}
		::fast_io::freestanding::my_memcpy(
			destination, __builtin_addressof(low), sizeof(low));
		::fast_io::freestanding::my_memcpy(
			destination + sizeof(low), __builtin_addressof(high), sizeof(high));
	}
}

// exponent must be in [-324, 308].  The cached word stores eight bytes although
// the returned logical length is four or five; all eight destination bytes must
// be writable.  Uppercase changes only the cached e byte.
template <bool uppercase_e>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *print_ascii_exponent(
	char *destination, ::std::int_least32_t exponent) noexcept
{
	auto packed{ascii_exponents.data[static_cast<::std::size_t>(exponent - ascii_exponent_cache::minimum)]};
	auto const length{static_cast<::std::uint_least32_t>(packed >> 56u)};
	if constexpr (uppercase_e)
	{
		packed ^= static_cast<::std::uint_least64_t>(u8'e' ^ u8'E');
	}
	::fast_io::freestanding::my_memcpy(
		destination, __builtin_addressof(packed), sizeof(packed));
	return destination + length;
}

// Requires digit_count in [1, 17] and exponent in
// [ascii_fixed_layout_cache::minimum, ascii_fixed_layout_cache::maximum].  The
// routine initializes scratch bytes past the returned logical end.  On SIMD x86
// a binary64 coefficient is assembled with one full 16-byte pshufb store; the
// scalar path uses the same generated point/end metadata.
template <typename flt, bool comma, bool json_float>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *print_ascii_fixed(
	char *destination, ascii_digit_block digits, ::std::uint_least32_t digit_count,
	::std::int_least32_t exponent, bool has_extra_digit,
	::std::uint_least32_t last_digit, bool has_last_digit) noexcept
{
	::fast_io::freestanding::my_memcpy(
		destination, __builtin_addressof(ascii_zeroes), sizeof(ascii_zeroes));
	auto const &layout{ascii_fixed_layouts.data[static_cast<::std::size_t>(exponent - ascii_fixed_layout_cache::minimum)]};
	auto buffer{destination + layout.start_position};
// The SIMD branch exists only with the table fields generated above.  It emits
// the same logical spelling as the scalar memmove path but protects the audited
// one-shuffle binary64 assembly shape; binary32 continues through the scalar
// layout because its shorter block does not use this shuffle metadata.
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	if constexpr (sizeof(flt) > sizeof(float))
	{
		auto const extra{static_cast<::std::uint_least32_t>(has_extra_digit)};
		auto const packed{ascii_x86_u64x2{digits.low, digits.high}};
		ascii_x86_c8x16 shuffle;
		::fast_io::freestanding::my_memcpy(
			__builtin_addressof(shuffle), layout.binary64_shuffle[extra], sizeof(shuffle));
		auto const assembled{__builtin_bit_cast(
			ascii_x86_u8x16,
			__builtin_ia32_pshufb128(__builtin_bit_cast(ascii_x86_c8x16, packed), shuffle))};
		auto const trailing_digit{__builtin_bit_cast(ascii_x86_u8x16, packed)[15]};
		::fast_io::freestanding::my_memcpy(
			buffer, __builtin_addressof(assembled), sizeof(assembled));
		buffer[16u] = static_cast<char>(trailing_digit);
		destination[layout.point_position] = static_cast<char>(comma ? u8',' : u8'.');
		buffer[layout.binary64_last_digit_position[extra]] =
			static_cast<char>(u8'0' + (has_last_digit ? last_digit : 0u));
		auto end{buffer + layout.end_position[digit_count - 1u]};
		if constexpr (json_float)
		{
			if (0 <= exponent &&
				digit_count <= static_cast<::std::uint_least32_t>(exponent + 1))
			{
				*end++ = static_cast<char>(comma ? u8',' : u8'.');
				*end++ = '0';
			}
		}
		return end;
	}
#endif
	::fast_io::details::da::store_ascii_digits<flt>(buffer, digits, !has_extra_digit);
	constexpr ::std::uint_least32_t block_size{sizeof(flt) <= sizeof(float) ? 8u : 16u};
	buffer[block_size + static_cast<::std::uint_least32_t>(has_extra_digit) - 1u] =
		static_cast<char>(u8'0' + (has_last_digit ? last_digit : 0u));
	::fast_io::freestanding::my_memmove(destination + layout.shift_position,
										destination + layout.point_position, block_size);
	destination[layout.point_position] = static_cast<char>(comma ? u8',' : u8'.');
	auto end{buffer + layout.end_position[digit_count - 1u]};
	if constexpr (json_float)
	{
		if (0 <= exponent &&
			digit_count <= static_cast<::std::uint_least32_t>(exponent + 1))
		{
			*end++ = static_cast<char>(comma ? u8',' : u8'.');
			*end++ = '0';
		}
	}
	return end;
}

// Extended fixed notation handles positive exponents beyond the compact layout
// writer.  Its callers constrain exponent and digit_count so at most six zeroes
// are appended and the decimal point lies within the reserved coefficient
// scratch area.  Like store_ascii_digits, it may write beyond the logical end.
template <typename flt, bool comma, bool json_float>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *print_ascii_fixed_extended(
	char *destination, ascii_digit_block digits, ::std::uint_least32_t digit_count,
	::std::int_least32_t exponent, bool has_extra_digit,
	::std::uint_least32_t last_digit, bool has_last_digit) noexcept
{
	::fast_io::details::da::store_ascii_digits<flt>(destination, digits, !has_extra_digit);
	constexpr ::std::uint_least32_t block_size{sizeof(flt) <= sizeof(float) ? 8u : 16u};
	destination[block_size + static_cast<::std::uint_least32_t>(has_extra_digit) - 1u] =
		static_cast<char>(u8'0' + (has_last_digit ? last_digit : 0u));
	auto const point_position{static_cast<::std::uint_least32_t>(exponent + 1)};
	if (digit_count <= point_position)
	{
		// General notation admits at most six appended zeroes; decimal's length decision admits at most five.
		switch (point_position - digit_count)
		{
		case 6u:
			destination[digit_count + 5u] = static_cast<char>(u8'0');
			[[fallthrough]];
		case 5u:
			destination[digit_count + 4u] = static_cast<char>(u8'0');
			[[fallthrough]];
		case 4u:
			destination[digit_count + 3u] = static_cast<char>(u8'0');
			[[fallthrough]];
		case 3u:
			destination[digit_count + 2u] = static_cast<char>(u8'0');
			[[fallthrough]];
		case 2u:
			destination[digit_count + 1u] = static_cast<char>(u8'0');
			[[fallthrough]];
		case 1u:
			destination[digit_count] = static_cast<char>(u8'0');
			[[fallthrough]];
		case 0u:
			break;
		}
		auto end{destination + point_position};
		if constexpr (json_float)
		{
			*end++ = static_cast<char>(comma ? u8',' : u8'.');
			*end++ = static_cast<char>(u8'0');
		}
		return end;
	}
	auto constexpr decimal_point{static_cast<char>(comma ? u8',' : u8'.')};
	if constexpr (sizeof(flt) <= sizeof(float))
	{
		destination[point_position + 1u] = destination[point_position];
		destination[point_position] = decimal_point;
	}
	else
	{
		// Extended fixed notation starts at byte eight, so the low eight digits never move.
		auto trailing_digit{destination[16u]};
		if (point_position == 16u)
		{
			destination[17u] = trailing_digit;
			destination[16u] = decimal_point;
		}
		else
		{
			::std::uint_least64_t high_digits;
			::fast_io::freestanding::my_memcpy(
				__builtin_addressof(high_digits), destination + 8u, sizeof(high_digits));
			auto const last_high_digit{static_cast<char>(high_digits >> 56u)};
			auto const shift{static_cast<::std::uint_least32_t>((point_position - 8u) * 8u)};
			auto const lower_mask{(static_cast<::std::uint_least64_t>(1u) << shift) - 1u};
			high_digits = (high_digits & lower_mask) |
						  ((high_digits & ~lower_mask) << 8u) |
						  (static_cast<::std::uint_least64_t>(static_cast<unsigned char>(decimal_point)) << shift);
			::fast_io::freestanding::my_memcpy(
				destination + 8u, __builtin_addressof(high_digits), sizeof(high_digits));
			if (digit_count == 17u)
			{
				destination[17u] = trailing_digit;
			}
			if (16u <= digit_count)
			{
				destination[16u] = last_high_digit;
			}
		}
	}
	return destination + digit_count + 1u;
}

// AArch64 binary32 scientific emission keeps the natural-order numeric digit
// vector produced by ascii_bcd4x4 and joins it with punctuation and the packed
// exponent before performing one TBL permutation and one 16-byte store.  For
// every output lane j the generated index selects exactly the byte used by the
// scalar construction:
//
//   source[0..7]   = the eight coefficient digits,
//   source[8..11]  = "e[+-]dd",
//   source[12]     = the optional interval digit,
//   source[13]     = the decimal point.
//
// The binary32 scientific exponent is in [-45,38], hence it always has two
// decimal exponent digits and lanes 8..11 are complete.  The cache construction
// enumerates digit_span in [1,8] and both Boolean fields, proving index in
// [0,31].  TBL maps an index with its high bit set to zero, exactly matching the
// unused scratch bytes in the scalar spelling.  Byte 15 stores logical-length
// metadata in the cache rather than output data, so the physical 16-byte store
// may pass the returned pointer but never the caller's reserve extent.
#if (defined(__aarch64__) || defined(__arm64__)) && !defined(__AARCH64EB__) && \
	(defined(__clang__) || defined(__GNUC__))
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline ascii_u8x16
ascii_aarch64_table_lookup(ascii_u8x16 source, ascii_u8x16 indices) noexcept
{
	/*
	Clang and GCC expose different frontend names for the same architectural TBL
	instruction.  Both operate on sixteen unsigned byte lanes, and the immediate
	48 in Clang's type-polymorphic builtin denotes uint8x16_t.  This branch is
	therefore an API spelling choice; it cannot change a selected byte.
	*/
#if defined(__clang__)
	return __builtin_bit_cast(
		ascii_u8x16,
		__builtin_neon_vqtbl1q_v(
			__builtin_bit_cast(ascii_i8x16, source),
			__builtin_bit_cast(ascii_i8x16, indices), 48));
#else
	return __builtin_aarch64_qtbl1v16qi_uuu(source, indices);
#endif
}

template <bool comma, bool uppercase_e>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *
print_ascii_scientific_aarch64_binary32(
	char *destination, ascii_u8x16 unshuffled,
	::std::uint_least32_t digit_span,
	::std::int_least32_t exponent, bool has_extra_digit,
	::std::uint_least32_t last_digit, bool has_last_digit) noexcept
{
	auto exponent_data{
		ascii_exponents.data[static_cast<::std::size_t>(
			exponent - ascii_exponent_cache::minimum)]};
	if constexpr (uppercase_e)
	{
		/*
		The packed exponent contains lowercase 'e' only in byte zero.  XOR with
		'e'^'E' changes exactly that byte and leaves sign and decimal digits
		unchanged, proving equivalence with the scalar uppercase branch.
		*/
		exponent_data ^=
			static_cast<::std::uint_least64_t>(u8'e' ^ u8'E');
	}
	auto source{__builtin_bit_cast(
		ascii_u64x2,
		unshuffled + ascii_u8x16{
			48, 48, 48, 48, 48, 48, 48, 48,
			48, 48, 48, 48, 48, 48, 48, 48})};
	/*
	The high 64-bit lane is scratch after digit conversion.  Overwriting it with
	the four exponent bytes followed by optional digit and point constructs the
	exact source alphabet documented above.  The cache never reads its remaining
	two bytes.
	*/
	source[1] =
		(exponent_data &
		 static_cast<::std::uint_least64_t>(0xffffffffu)) |
		(static_cast<::std::uint_least64_t>(u8'0' + last_digit)
		 << 32u) |
		(static_cast<::std::uint_least64_t>(
			 comma ? u8',' : u8'.')
		 << 40u);
	auto const index{
		(digit_span - 1u) * 4u +
		static_cast<::std::uint_least32_t>(has_last_digit) * 2u +
		static_cast<::std::uint_least32_t>(has_extra_digit)};
	auto const &layout{
		ascii_binary32_scientific_layouts.data[index]};
	ascii_u8x16 shuffle;
	::fast_io::freestanding::my_memcpy(
		__builtin_addressof(shuffle), layout.shuffle,
		sizeof(shuffle));
	auto const assembled{
		::fast_io::details::da::ascii_aarch64_table_lookup(
			__builtin_bit_cast(ascii_u8x16, source), shuffle)};
	::fast_io::freestanding::my_memcpy(
		destination, __builtin_addressof(assembled),
		sizeof(assembled));
	return destination + layout.shuffle[15];
}
#endif

// x86 binary32 scientific emission consumes the unshuffled digit vector before
// it becomes scalar words.  It emits one complete 16-byte pshufb result;
// layout.shuffle[15] contains the shorter logical length and byte 15 is outside
// every result.  The destination must nevertheless permit all sixteen bytes.
// The ISA guard matches the producer and the generated shuffle table exactly.
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
template <bool comma, bool uppercase_e>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *print_ascii_scientific_x86_binary32(
	char *destination, ascii_x86_u8x16 unshuffled, ::std::uint_least32_t digit_span,
	::std::int_least32_t exponent, bool has_extra_digit,
	::std::uint_least32_t last_digit, bool has_last_digit) noexcept
{
	auto exponent_data{
		ascii_exponents.data[static_cast<::std::size_t>(exponent - ascii_exponent_cache::minimum)]};
	if constexpr (uppercase_e)
	{
		exponent_data ^= static_cast<::std::uint_least64_t>(u8'e' ^ u8'E');
	}
	auto source{__builtin_bit_cast(ascii_x86_u64x2,
								   unshuffled + ascii_x86_u8x16{48, 48, 48, 48, 48, 48, 48, 48,
																48, 48, 48, 48, 48, 48, 48, 48})};
	source[1] = (exponent_data & static_cast<::std::uint_least64_t>(0xffffffffu)) |
				(static_cast<::std::uint_least64_t>(u8'0' + last_digit) << 32u) |
				(static_cast<::std::uint_least64_t>(comma ? u8',' : u8'.') << 40u);
	auto const index{(digit_span - 1u) * 4u +
					 static_cast<::std::uint_least32_t>(has_last_digit) * 2u +
					 static_cast<::std::uint_least32_t>(has_extra_digit)};
	auto const &layout{ascii_binary32_scientific_layouts.data[index]};
	ascii_x86_c8x16 shuffle;
	::fast_io::freestanding::my_memcpy(
		__builtin_addressof(shuffle), layout.shuffle, sizeof(shuffle));
	auto const assembled{__builtin_bit_cast(
		ascii_x86_u8x16,
		__builtin_ia32_pshufb128(__builtin_bit_cast(ascii_x86_c8x16, source), shuffle))};
	::fast_io::freestanding::my_memcpy(
		destination, __builtin_addressof(assembled), sizeof(assembled));
	return destination + layout.shuffle[15];
}
#endif

// Generic scientific assembly for a finite binary32/binary64 carrier.  exponent
// must lie in [-324, 308], and the coefficient arguments describe at least one
// significant digit.  store_ascii_digits and print_ascii_exponent perform their
// documented fixed-width scratch stores; the returned pointer is the logical
// end after exponent punctuation.
template <typename flt, bool comma, bool uppercase_e>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *print_ascii_scientific(
	char *destination, ascii_digit_block digits,
	::std::int_least32_t exponent, bool has_extra_digit,
	::std::uint_least32_t last_digit, bool has_last_digit) noexcept
{
	constexpr ::std::uint_least32_t block_size{sizeof(flt) <= sizeof(float) ? 8u : 16u};
	auto buffer{destination + static_cast<::std::uint_least32_t>(has_extra_digit)};
	::fast_io::details::da::store_ascii_digits<flt>(buffer, digits, false);
	buffer[block_size] = static_cast<char>(u8'0' + last_digit);
	buffer += has_last_digit ? block_size + 1u : digits.span;
	destination[0] = destination[1];
	destination[1] = static_cast<char>(comma ? u8',' : u8'.');
	buffer -= static_cast<::std::uint_least32_t>(buffer == destination + 2u);
	return ::fast_io::details::da::print_ascii_exponent<uppercase_e>(buffer, exponent);
}

// Render one DA shortest carrier using the ASCII layout tables.  The fields have
// conversion_result's meaning from compute.h.  General chooses fixed for decimal
// exponents [-4, 6].  Decimal chooses the shorter complete spelling, with ties
// in favor of fixed.  A fixed request outside the covered layout range returns
// nullptr without promising a complete logical result; its caller then invokes
// the character-generic writer.
//
// ISA branches below change only digit materialization and punctuation layout:
// every branch consumes the same carrier and must emit byte-identical results.
// The AArch64 and x86 forms preserve their audited SIMD instruction shapes; the
// scalar form is the correctness reference for unsupported ISA feature sets.
template <typename flt, ::fast_io::manipulators::scalar_flags flags, bool staged_emission = false,
	bool use_cached_constants = ascii_x86_cached_bcd_constants_default>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *print_ascii_shortest_fields(
	char *destination, ::std::uint_least64_t significand,
	::std::int_least32_t converted_exponent, ::std::uint_least32_t last_digit,
	bool has_last_digit) noexcept
{
	constexpr bool binary32{sizeof(flt) <= sizeof(float)};
	// SIMD x86 can insert a binary64 decimal point with one 16-byte shuffle
	// through exponent 15.  Other implementations use the compact metadata
	// through exponent 6 and delegate larger fixed fields to the extended writer.
	constexpr ::std::int_least32_t fast_fixed_maximum{
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
		binary32 ? ascii_fixed_layout_cache::compact_maximum : ascii_fixed_layout_cache::binary64_shuffle_maximum
#else
		ascii_fixed_layout_cache::compact_maximum
#endif
	};
	constexpr ::std::uint_least64_t extra_digit_threshold{
		binary32 ? static_cast<::std::uint_least64_t>(10000000) : static_cast<::std::uint_least64_t>(1000000000000000)};
	constexpr ::std::uint_least32_t block_size{binary32 ? 8u : 16u};
	if constexpr (binary32)
	{
		if (significand < static_cast<::std::uint_least64_t>(1000000)) [[unlikely]]
		{
			// A regular binary32 carrier normally contains seven or eight digits.
			// At the six-digit boundary, consume the optional interval digit now and
			// decrement the decimal exponent.  Multiplying the coefficient by ten
			// while subtracting one from its exponent preserves the exact value and
			// keeps every SIMD digit/layout index in its proved range.
			significand = significand * 10u + (has_last_digit ? last_digit : 0u);
			has_last_digit = false;
			--converted_exponent;
		}
	}
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::fixed)
	{
		/*
		A fixed request never changes notation, and direct-layout availability
		depends only on the carrier's scientific exponent and the backend's proved
		layout interval.  Digit materialization and digit_count affect placement
		inside an accepted layout but cannot turn a rejected exponent into one.
		Rejecting here therefore preserves the null-fallback contract while avoiding
		the complete SIMD/scalar digit block on every miss.  General and decimal stay
		below the materializer because their notation decisions do depend on the
		materialized digit count or its generated mask.
		*/
		auto const fixed_has_extra_digit{significand >= extra_digit_threshold};
		auto const fixed_exponent{static_cast<::std::int_least32_t>(
			converted_exponent + (binary32 ? 7 : 15) +
			static_cast<::std::int_least32_t>(fixed_has_extra_digit))};
		if (fixed_exponent < ascii_fixed_layout_cache::minimum ||
			fast_fixed_maximum < fixed_exponent)
		{
			return nullptr;
		}
	}
	// Little-endian AArch64 uses the sqdmulh vector lanes proved above; AArch64
	// big-endian deliberately follows the scalar branch because reversing scalar
	// words cannot repair the SIMD path's vector-index-to-address assumptions.
	// x86 uses the guarded pshufb pipeline, and all remaining targets use the
	// scalar reciprocal implementation.  Keep this guard identical to the SIMD
	// definition guard so an unavailable or unproved backend cannot be named.
	// The choice is confined to type/ISA customization and does not alter format
	// policy or the emitted character sequence.  The final compiler predicate
	// deliberately matches the declaration guard: it names only the Clang/GCC
	// builtin spellings exercised by the compile matrix, without claiming a
	// stable cross-frontend ABI contract.
#if (defined(__aarch64__) || defined(__arm64__)) && !defined(__AARCH64EB__) && \
	(defined(__clang__) || defined(__GNUC__))
	auto const digit_data{[&]() noexcept
	{
		if constexpr (binary32)
		{
			return ::fast_io::details::da::
				make_ascii_binary32_digit_data_simd(significand);
		}
		else
		{
			/*
			Binary64 scientific notation consumes destination-order digits, so its
			unshuffled member is dead.  Supplying a zero vector gives this branch
			the same structural carrier as binary32 without adding an instruction
			after constant propagation; the mathematical digit block is exactly
			the pre-existing binary64 SIMD result.
			*/
			return ascii_aarch64_binary32_digit_data{
				::fast_io::details::da::
					make_ascii_digit_block_simd<flt>(significand),
				{}};
		}
	}()};
	auto const digits{digit_data.digits};
#elif (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	auto const digit_data{::fast_io::details::da::make_ascii_digit_data_x86<flt, use_cached_constants>(
		significand)};
	auto const digits{digit_data.digits};
#else
	auto const digits{::fast_io::details::da::make_ascii_digit_block<flt>(significand)};
#endif
	auto const has_extra_digit{significand >= extra_digit_threshold};
	auto const digit_count{has_last_digit
								? block_size + static_cast<::std::uint_least32_t>(has_extra_digit)
								: digits.span - 1u + static_cast<::std::uint_least32_t>(has_extra_digit)};
	auto const exponent{static_cast<::std::int_least32_t>(
		converted_exponent + (binary32 ? 7 : 15) +
		static_cast<::std::int_least32_t>(has_extra_digit))};
	bool use_fixed{};
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::fixed)
	{
		use_fixed = true;
	}
	else if constexpr (flags.floating == ::fast_io::manipulators::floating_format::general)
	{
		/*
		At this stage `exponent` is already the exponent of the leading decimal
		digit, i.e. X in d.ddd*10^X.  `digit_count` affects the carrier's trailing
		exponent but not X.  The general presentation theorem therefore tests
		-4<=X<6 directly; reconstructing e10 and testing it would make notation
		depend on how many insignificant zeroes Dragonbox removed.
		*/
		use_fixed = -4 <= exponent && exponent < 6;
	}
	else if constexpr (flags.floating == ::fast_io::manipulators::floating_format::decimal)
	{
		if (ascii_fixed_layout_cache::minimum <= exponent &&
			exponent <= ascii_fixed_layout_cache::maximum)
		{
			// The AArch64 and audited Clang-23 Linux x86 artifacts use the compact
			// projection documented above, avoiding a full-layout address on the
			// scientific branch.  The projection is generated from the authoritative
			// field, so this backend choice cannot alter the notation bit or bytes.
#if (defined(__aarch64__) || defined(__arm64__)) && !defined(__AARCH64EB__) && \
	(defined(__clang__) || defined(__GNUC__))
			/*
			The binary32 TBL path benefits from breaking the mask dependency away
			from the fixed-layout base.  Binary64 has a longer digit state and the
			same projection was neutral-to-negative on M4, so it retains the
			canonical entry and lets the fixed branch reuse that address.
			*/
			auto const fixed_mask{[&]() noexcept
			{
				if constexpr (binary32)
				{
					return ascii_decimal_fixed_masks.data[
						static_cast<::std::size_t>(
							exponent -
							ascii_fixed_layout_cache::minimum)];
				}
				else
				{
					return ascii_fixed_layouts.data[
						static_cast<::std::size_t>(
							exponent -
							ascii_fixed_layout_cache::minimum)]
						.decimal_fixed_mask;
				}
			}()};
#elif defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	  defined(__clang__) && __clang_major__ == 23 && \
	  !(defined(__arm64ec__) || defined(_M_ARM64EC))
			auto const fixed_mask{ascii_decimal_fixed_masks.data[static_cast<::std::size_t>(
				exponent - ascii_fixed_layout_cache::minimum)]};
#else
			auto const &layout{ascii_fixed_layouts.data[static_cast<::std::size_t>(
				exponent - ascii_fixed_layout_cache::minimum)]};
			auto const fixed_mask{layout.decimal_fixed_mask};
#endif
			use_fixed = static_cast<bool>(
				(fixed_mask >> (digit_count - 1u)) & 1u);
		}
	}
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::general)
	{
		if (use_fixed)
		{
			if (exponent <= fast_fixed_maximum)
			{
				return ::fast_io::details::da::print_ascii_fixed<flt, flags.comma, flags.json_float>(
					destination, digits, digit_count, exponent, has_extra_digit,
					last_digit, has_last_digit);
			}
			return ::fast_io::details::da::print_ascii_fixed_extended<flt, flags.comma, flags.json_float>(
				destination, digits, digit_count, exponent, has_extra_digit,
				last_digit, has_last_digit);
		}
	}
	else if (use_fixed && ascii_fixed_layout_cache::minimum <= exponent &&
			 exponent <= fast_fixed_maximum)
	{
		return ::fast_io::details::da::print_ascii_fixed<flt, flags.comma, flags.json_float>(
			destination, digits, digit_count, exponent, has_extra_digit,
			last_digit, has_last_digit);
	}
	else if constexpr (flags.floating == ::fast_io::manipulators::floating_format::decimal)
	{
		if (use_fixed)
		{
			return ::fast_io::details::da::print_ascii_fixed_extended<flt, flags.comma, flags.json_float>(
				destination, digits, digit_count, exponent, has_extra_digit,
				last_digit, has_last_digit);
		}
	}
	if (use_fixed)
	{
		return nullptr;
	}
	// AArch64 TBL consumes the natural digit vector for every placement: keeping
	// it through the notation branch removes a scalar round trip in both scalar
	// and staged callers.  Complete binary32 pshufb assembly has the opposite
	// profitable placement in the audited x86 compiler pipelines: Clang uses it
	// while conversion/emission are in one scalar call, whereas GCC uses it after
	// staged preparation shortens the surrounding live range.  Every branch
	// selects the same bytes from the same carrier.  Revalidate shuffle count,
	// frame, spills and calls when staged preparation or compiler support changes;
	// placement is not part of formatting semantics.
#if (defined(__aarch64__) || defined(__arm64__)) && !defined(__AARCH64EB__) && \
	(defined(__clang__) || defined(__GNUC__))
	if constexpr (binary32)
	{
		return ::fast_io::details::da::
			print_ascii_scientific_aarch64_binary32<
				flags.comma, flags.uppercase_e>(
					destination, digit_data.unshuffled,
					digits.span, exponent, has_extra_digit,
					last_digit, has_last_digit);
	}
#elif (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__SSSE3__) && \
	(defined(__GNUC__) || defined(__clang__)) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	if constexpr (binary32)
	{
		// Both placements feed the same carrier fields to the same pshufb writer,
		// so the returned pointer and bytes are semantically identical.  Paired
		// x86 measurements and assembly inspection established the non-staged
		// placement for Clang 23 and the staged placement for GCC 16.0.1.  Other
		// compiler majors selected by these open family branches inherit the
		// polarity as an unmeasured code-generation hypothesis.
#if defined(__clang__)
		if constexpr (!staged_emission)
#else
		if constexpr (staged_emission)
#endif
		{
			return ::fast_io::details::da::print_ascii_scientific_x86_binary32<flags.comma, flags.uppercase_e>(
				destination, digit_data.unshuffled, digits.span, exponent, has_extra_digit,
				last_digit, has_last_digit);
		}
	}
#endif
	return ::fast_io::details::da::print_ascii_scientific<flt, flags.comma, flags.uppercase_e>(
		destination, digits, exponent, has_extra_digit,
		last_digit, has_last_digit);
}

template <typename flt, ::fast_io::manipulators::scalar_flags flags, bool staged_emission = false,
	bool use_cached_constants = ascii_x86_cached_bcd_constants_default>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *print_ascii_shortest(
	char *destination, conversion_result converted) noexcept
{
	return ::fast_io::details::da::print_ascii_shortest_fields<
		flt, flags, staged_emission, use_cached_constants>(destination, converted.significand,
			converted.exponent, converted.last_digit, converted.has_last_digit);
}

// GCC 13--15 Linux System V x86-64 binary64 fixed-direct entry.  Keeping the
// selected fixed band out of the general layout decision shortens the live range
// between DA conversion and pshufb emission.  It is semantically identical to
// print_ascii_shortest.  GCC 13 uses this entry only for the regular-normal hot
// leaf; its subnormal and irregular cases remain in the generic fallback because
// outlining those cases with the GCC 14-16 policy increased measured subnormal
// latency.  GCC 16 introduced the unified layout schedule used by later GNU
// frontends, so the upper transition selects a newer path rather than abandoning
// future compilers.  Other ABIs retain the compact generic entry.  Move either
// transition only with frame, spill, call, constant-load, branch and linked-text
// evidence.
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__SSE4_1__) && defined(__SSSE3__) && defined(__GNUC__) && \
	!defined(__clang__) && 13 <= __GNUC__ && __GNUC__ < 16 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
template <typename flt, ::fast_io::manipulators::scalar_flags flags>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline char *print_ascii_shortest_fixed_direct(
	char *destination, conversion_result converted) noexcept
{
	static_assert(sizeof(flt) > sizeof(float));
	static_assert(flags.floating == ::fast_io::manipulators::floating_format::decimal);
	auto const digits{::fast_io::details::da::make_ascii_digit_block_x86<flt>(
		converted.significand)};
	auto const has_extra_digit{
		converted.significand >= static_cast<::std::uint_least64_t>(1000000000000000)};
	auto const digit_count{converted.has_last_digit
		? 16u + static_cast<::std::uint_least32_t>(has_extra_digit)
		: digits.span - 1u + static_cast<::std::uint_least32_t>(has_extra_digit)};
	auto const exponent{static_cast<::std::int_least32_t>(
		converted.exponent + 15 +
		static_cast<::std::int_least32_t>(has_extra_digit))};
	return ::fast_io::details::da::print_ascii_fixed<flt, flags.comma, flags.json_float>(
		destination, digits, digit_count, exponent, has_extra_digit,
		converted.last_digit, converted.has_last_digit);
}
#endif

// Semantic equivalence: the outlined binary32 entry forwards the same scalar
// carrier fields to print_ascii_shortest_fields, so outlining cannot change the
// selected notation, emitted bytes, or returned pointer.  GCC 15.2 and GCC
// 16.0.1 measurements on CPU 8 (physical core 4 of the i9-14900HX) show that
// this boundary prevents DA conversion state and SIMD layout state from sharing
// an oversized live range.  After the integer sign-carrier fix, paired GCC 15
// AB/BA runs improved regular, finite and boundary binary32 by 2.9--4.9%; the
// wrapper plus outlined callee also removed 75 text bytes.  GCC 16 retains its
// previously audited split.  No binary64 caller reaches this template.
//
// GCC 14 has a narrower profitable domain: physical-core AB/BA measurements
// improve decimal binary32 by 11.6--13.4%, while enabling the same split for all
// presentations regresses general by 14.4--15.2%, scientific by 2.5--4.9%, and
// fixed by 3.3--4.0%.  The caller therefore instantiates this leaf only for GCC
// 14 decimal. GCC 15 is the measured all-format lower bound, and later GNU
// frontends inherit that policy rather than falling back solely because their
// version is newer. Other ABIs use the inline field writer; both paths consume
// the same carrier and emit identical bytes. Narrowing the family policy
// requires call, frame, spill, pshufb placement, complete-call timing and
// linked-text evidence.
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__SSE4_1__) && defined(__SSSE3__) && defined(__GNUC__) && \
	!defined(__clang__) && 14 <= __GNUC__ && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
template <typename flt, ::fast_io::manipulators::scalar_flags flags>
[[nodiscard]]
// noinline is the live-range boundary described above, not an ISA semantic.
// Use the GNU spelling only where the compiler advertises it.
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline char *print_ascii_shortest_split(
	char *destination, ::std::uint_least64_t significand,
	::std::int_least32_t exponent, ::std::uint_least32_t last_digit,
	bool has_last_digit) noexcept
{
	static_assert(sizeof(flt) <= sizeof(float));
	return ::fast_io::details::da::print_ascii_shortest_fields<flt, flags>(
		destination, significand, exponent, last_digit, has_last_digit);
}

#endif

} // namespace fast_io::details::da

#pragma once

namespace fast_io::details::da
{

// A two-word unsigned integer in mathematical order:
// value = hi * 2^64 + lo.  umul64x64 returns the complete 128-bit product.
// umul64x64_add_high returns floor((x * y + addend) / 2^64), where addend is
// applied to the low product word.  umul64x128_high returns bits [64, 191] of
// the 192-bit product x * y, i.e. floor(x * y / 2^64) for a 128-bit y.
struct uint64x2
{
	::std::uint_least64_t hi;
	::std::uint_least64_t lo;
};

// Small decimal scales are shared by DA precision and decimal rounding.
// Generate power[n] = 10 * power[n - 1], hence power[n] = 10^n.  10^19 is the
// largest power of ten representable by an unsigned 64-bit integer; the loop
// stops before the overflowing 10^20 product, so every constexpr
// multiplication is defined and no handwritten twenty-entry table is needed.
struct uint64_power10_table_type
{
	::std::uint_least64_t values[20u]{};

	inline constexpr uint64_power10_table_type() noexcept
	{
		values[0] = 1u;
		for (::std::size_t index{1u}; index != 20u; ++index)
		{
			values[index] = values[index - 1u] * 10u;
		}
	}
};

// Preserve the former table's 16-byte alignment.  Besides permitting paired
// constant loads, this keeps the address and following constant-section layout
// stable on audited Mach-O/ELF Clang builds; the reference facade below adds no
// storage or alignment of its own.
alignas(16) inline constexpr uint64_power10_table_type uint64_power10_table_storage{};

// Expose the generated storage as a raw-array reference so existing indexed
// consumers retain array-subscript syntax.  This alias owns no storage: taking
// its address denotes uint64_power10_table_storage.values.
inline constexpr auto const &uint64_power10_table{
	uint64_power10_table_storage.values};

static_assert(uint64_power10_table[19u] == static_cast<::std::uint_least64_t>(10000000000000000000ULL));

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr uint64x2 umul64x64(::std::uint_least64_t x,
																			::std::uint_least64_t y) noexcept
{
	::std::uint_least64_t hi;
	auto const lo{::fast_io::intrinsics::umul(x, y, hi)};
	return {hi, lo};
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::uint_least64_t umul64x64_add_high(
	::std::uint_least64_t x, ::std::uint_least64_t y, ::std::uint_least64_t addend) noexcept
{
	// Mathematical equivalence: both branches compute the full product, add the
	// low-word addend and propagate exactly one carry before returning bits
	// [64, 127].  Native u128 is only a compiler-capability spelling; the intrinsic
	// fallback reconstructs the identical word and makes no performance promise.
#if defined(__SIZEOF_INT128__)
	auto const product{static_cast<__uint128_t>(x) * y + addend};
	return static_cast<::std::uint_least64_t>(product >> 64u);
#else
	auto const product{::fast_io::details::da::umul64x64(x, y)};
	auto const lo{static_cast<::std::uint_least64_t>(product.lo + addend)};
	return static_cast<::std::uint_least64_t>(product.hi + (lo < product.lo));
#endif
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr uint64x2 umul64x128_high(::std::uint_least64_t x,
																			  uint64x2 y) noexcept
{
	// Mathematical equivalence: both branches return bits [64, 191] of x * y.
	// Native u128 changes only how the upper product and carry are expressed; the
	// fallback reconstructs the same two words.  This capability test does not
	// encode an ISA or compiler-performance preference.
#if defined(__SIZEOF_INT128__)
	auto const upper{static_cast<__uint128_t>(x) * y.hi};
	auto const upper_low{static_cast<::std::uint_least64_t>(upper)};
	auto const lo{static_cast<::std::uint_least64_t>(upper_low + ::fast_io::intrinsics::umulh(x, y.lo))};
	return {static_cast<::std::uint_least64_t>((upper >> 64u) + (lo < upper_low)), lo};
#else
	auto const upper{::fast_io::details::da::umul64x64(x, y.hi)};
	auto const lower_high{::fast_io::intrinsics::umulh(x, y.lo)};
	auto const lo{static_cast<::std::uint_least64_t>(upper.lo + lower_high)};
	return {static_cast<::std::uint_least64_t>(upper.hi + (lo < upper.lo)), lo};
#endif
}

// The binary64 DA cache stores one normalized 128-bit lower endpoint for every
// required decimal power.  These three arrays are compressed proof seeds, not
// the runtime cache: the constexpr cache constructor expands 28 minor factors,
// 23 major factors and 20 correction words into all 618 endpoints.
//
// minor[j] is the normalized 64-bit factor for 10^j, 0 <= j < 28.
// major[j] is the normalized 128-bit lower factor for
// 10^(-303 + 28*j), 0 <= j < 23.  For cache index i, q=i-293 and
//
//   (-303 + 28*floor((i+10)/28)) + ((i+10) mod 28) = q.
//
// power10_cache::compute forms the high 128 product bits, normalizes them, and
// applies correction bit i from power10_fixups so the result is the normalized
// lower endpoint for 10^q.  Generating the signed-power major factors from
// their exact integer floors requires arithmetic wider than the freestanding
// built-in types; retaining these small seeds avoids embedding a constexpr
// arbitrary-precision generator or a handwritten 618-entry cache.
//
// The downward correction is part of the correctness contract: every returned
// cache entry remains at or below the exact normalized power.  Precision
// rounding relies on this one-sided error.  Replacing the entries by nearest
// approximations, or changing the factorization without regenerating and
// reproving the correction bitmap, is not semantics-preserving.
inline constexpr ::std::uint_least64_t power10_minor[]{
	static_cast<::std::uint_least64_t>(0x8000000000000000), static_cast<::std::uint_least64_t>(0xa000000000000000), static_cast<::std::uint_least64_t>(0xc800000000000000),
	static_cast<::std::uint_least64_t>(0xfa00000000000000), static_cast<::std::uint_least64_t>(0x9c40000000000000), static_cast<::std::uint_least64_t>(0xc350000000000000),
	static_cast<::std::uint_least64_t>(0xf424000000000000), static_cast<::std::uint_least64_t>(0x9896800000000000), static_cast<::std::uint_least64_t>(0xbebc200000000000),
	static_cast<::std::uint_least64_t>(0xee6b280000000000), static_cast<::std::uint_least64_t>(0x9502f90000000000), static_cast<::std::uint_least64_t>(0xba43b74000000000),
	static_cast<::std::uint_least64_t>(0xe8d4a51000000000), static_cast<::std::uint_least64_t>(0x9184e72a00000000), static_cast<::std::uint_least64_t>(0xb5e620f480000000),
	static_cast<::std::uint_least64_t>(0xe35fa931a0000000), static_cast<::std::uint_least64_t>(0x8e1bc9bf04000000), static_cast<::std::uint_least64_t>(0xb1a2bc2ec5000000),
	static_cast<::std::uint_least64_t>(0xde0b6b3a76400000), static_cast<::std::uint_least64_t>(0x8ac7230489e80000), static_cast<::std::uint_least64_t>(0xad78ebc5ac620000),
	static_cast<::std::uint_least64_t>(0xd8d726b7177a8000), static_cast<::std::uint_least64_t>(0x878678326eac9000), static_cast<::std::uint_least64_t>(0xa968163f0a57b400),
	static_cast<::std::uint_least64_t>(0xd3c21bcecceda100), static_cast<::std::uint_least64_t>(0x84595161401484a0), static_cast<::std::uint_least64_t>(0xa56fa5b99019a5c8),
	static_cast<::std::uint_least64_t>(0xcecb8f27f4200f3a)};

inline constexpr uint64x2 power10_major[]{
	{static_cast<::std::uint_least64_t>(0xaf8e5410288e1b6f), static_cast<::std::uint_least64_t>(0x07ecf0ae5ee44dda)},
	{static_cast<::std::uint_least64_t>(0xb1442798f49ffb4a), static_cast<::std::uint_least64_t>(0x99cd11cfdf41779d)},
	{static_cast<::std::uint_least64_t>(0xb2fe3f0b8599ef07), static_cast<::std::uint_least64_t>(0x861fa7e6dcb4aa15)},
	{static_cast<::std::uint_least64_t>(0xb4bca50b065abe63), static_cast<::std::uint_least64_t>(0x0fed077a756b53aa)},
	{static_cast<::std::uint_least64_t>(0xb67f6455292cbf08), static_cast<::std::uint_least64_t>(0x1a3bc84c17b1d543)},
	{static_cast<::std::uint_least64_t>(0xb84687c269ef3bfb), static_cast<::std::uint_least64_t>(0x3d5d514f40eea742)},
	{static_cast<::std::uint_least64_t>(0xba121a4650e4ddeb), static_cast<::std::uint_least64_t>(0x92f34d62616ce413)},
	{static_cast<::std::uint_least64_t>(0xbbe226efb628afea), static_cast<::std::uint_least64_t>(0x890489f70a55368c)},
	{static_cast<::std::uint_least64_t>(0xbdb6b8e905cb600f), static_cast<::std::uint_least64_t>(0x5400e987bbc1c921)},
	{static_cast<::std::uint_least64_t>(0xbf8fdb78849a5f96), static_cast<::std::uint_least64_t>(0xde98520472bdd034)},
	{static_cast<::std::uint_least64_t>(0xc16d9a0095928a27), static_cast<::std::uint_least64_t>(0x75b7053c0f178294)},
	{static_cast<::std::uint_least64_t>(0xc350000000000000), static_cast<::std::uint_least64_t>(0x0000000000000000)},
	{static_cast<::std::uint_least64_t>(0xc5371912364ce305), static_cast<::std::uint_least64_t>(0x6c28000000000000)},
	{static_cast<::std::uint_least64_t>(0xc722f0ef9d80aad6), static_cast<::std::uint_least64_t>(0x424d3ad2b7b97ef6)},
	{static_cast<::std::uint_least64_t>(0xc913936dd571c84c), static_cast<::std::uint_least64_t>(0x03bc3a19cd1e38ea)},
	{static_cast<::std::uint_least64_t>(0xcb090c8001ab551c), static_cast<::std::uint_least64_t>(0x5cadf5bfd3072cc6)},
	{static_cast<::std::uint_least64_t>(0xcd036837130890a1), static_cast<::std::uint_least64_t>(0x36dba887c37a8c10)},
	{static_cast<::std::uint_least64_t>(0xcf02b2c21207ef2e), static_cast<::std::uint_least64_t>(0x94f967e45e03f4bc)},
	{static_cast<::std::uint_least64_t>(0xd106f86e69d785c7), static_cast<::std::uint_least64_t>(0xe13336d701beba52)},
	{static_cast<::std::uint_least64_t>(0xd31045a8341ca07c), static_cast<::std::uint_least64_t>(0x1ede48111209a051)},
	{static_cast<::std::uint_least64_t>(0xd51ea6fa85785631), static_cast<::std::uint_least64_t>(0x552a74227f3ea566)},
	{static_cast<::std::uint_least64_t>(0xd732290fbacaf133), static_cast<::std::uint_least64_t>(0xa97c177947ad4096)},
	{static_cast<::std::uint_least64_t>(0xd94ad8b1c7380874), static_cast<::std::uint_least64_t>(0x18375281ae7822bc)}};

inline constexpr ::std::uint_least32_t power10_fixups[]{
	static_cast<::std::uint_least32_t>(0x0a4e363f), static_cast<::std::uint_least32_t>(0x00001840), static_cast<::std::uint_least32_t>(0x00006400), static_cast<::std::uint_least32_t>(0x24200040),
	static_cast<::std::uint_least32_t>(0x00000000), static_cast<::std::uint_least32_t>(0x0c000000), static_cast<::std::uint_least32_t>(0x82c81380), static_cast<::std::uint_least32_t>(0x5e4ce01f),
	static_cast<::std::uint_least32_t>(0xd730f60f), static_cast<::std::uint_least32_t>(0x0000001b), static_cast<::std::uint_least32_t>(0x00000000), static_cast<::std::uint_least32_t>(0xcdf7fffc),
	static_cast<::std::uint_least32_t>(0x6e8201d8), static_cast<::std::uint_least32_t>(0x40cd3fd1), static_cast<::std::uint_least32_t>(0xdb642501), static_cast<::std::uint_least32_t>(0x00000d0d),
	static_cast<::std::uint_least32_t>(0x14042400), static_cast<::std::uint_least32_t>(0x53713840), static_cast<::std::uint_least32_t>(0x11781db4), static_cast<::std::uint_least32_t>(0x00000000)};

inline constexpr ::std::size_t power10_minor_size{sizeof(power10_minor) / sizeof(*power10_minor)};
inline constexpr ::std::size_t power10_major_size{sizeof(power10_major) / sizeof(*power10_major)};
inline constexpr ::std::size_t power10_fixup_size{sizeof(power10_fixups) / sizeof(*power10_fixups)};

static_assert(power10_minor_size == 28u);
static_assert(power10_major_size == 23u);
static_assert(power10_fixup_size == 20u);

struct power10_cache
{
	inline static constexpr ::std::size_t size{618u};
	inline static constexpr ::std::int_least32_t minimum_exponent{-293};
	alignas(64)::std::uint_least64_t data[size * 2u]{};

	[[nodiscard]] inline static constexpr uint64x2 compute(::std::size_t i) noexcept
	{
		constexpr ::std::size_t stride{sizeof(power10_minor) / sizeof(*power10_minor)};
		auto const minor{power10_minor[(i + 10u) % stride]};
		auto const major{power10_major[(i + 10u) / stride]};
		auto const h1{::fast_io::intrinsics::umulh(major.lo, minor)};
		auto const c0{static_cast<::std::uint_least64_t>(major.lo * minor)};
		auto const major_product{::fast_io::details::da::umul64x64(major.hi, minor)};
		auto const c1{static_cast<::std::uint_least64_t>(h1 + major_product.lo)};
		auto const c2{static_cast<::std::uint_least64_t>(major_product.hi + (c1 < h1))};
		uint64x2 result;
		if (c2 >> 63u)
		{
			result = {c2, c1};
		}
		else
		{
			result = {static_cast<::std::uint_least64_t>((c2 << 1u) | (c1 >> 63u)),
					  static_cast<::std::uint_least64_t>((c1 << 1u) | (c0 >> 63u))};
		}
		result.lo -= (power10_fixups[i >> 5u] >> (i & 31u)) & 1u;
		return result;
	}

	inline constexpr power10_cache() noexcept
	{
		for (::std::size_t i{}; i != size; ++i)
		{
			auto const value{compute(i)};
			// Semantic equivalence: storing value.hi at size - i - 1 and value.lo
			// one half later is a bijective transpose of the portable interleaved
			// {hi, lo} array.  The guarded operator[] below applies the inverse map,
			// so every exponent reconstructs exactly compute(i).
			// The paired lookup guard documents the Apple-Clang-23/M4 address-generation
			// evidence and the explicit unmeasured-AArch64-family hypothesis for choosing
			// this semantically equivalent transpose.
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
			data[size - i - 1u] = value.hi;
			data[size * 2u - i - 1u] = value.lo;
#else
			data[i * 2u] = value.hi;
			data[i * 2u + 1u] = value.lo;
#endif
		}
	}

	[[nodiscard]] inline constexpr uint64x2 operator[](::std::int_least32_t exponent) const noexcept
	{
		// Semantic equivalence: for i = exponent - minimum_exponent,
		// data + size + minimum_exponent indexed by ~exponent addresses
		// data[size - i - 1]; adding size addresses the corresponding low word.
		// Thus this branch exactly inverts the constructor layout for the required
		// exponent domain [-293, 324] and returns the portable {hi, lo} endpoint.
		//
		// Apple Clang 23/M4 disassembly is the retained address-generation evidence:
		// it uses one prepared base and the complemented exponent for both loads.
		// That is not a cross-AArch64 timing proof.  Other AArch64 compilers and
		// cores admitted by the family guard use this layout as an unmeasured
		// code-generation hypothesis and must be checked for extra arithmetic,
		// spills, calls, load-use latency and constant-section cost.
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
		auto const base{data + size + minimum_exponent};
		auto const index{static_cast<::std::ptrdiff_t>(~exponent)};
		return {base[index], base[index + static_cast<::std::ptrdiff_t>(size)]};
#else
		auto const index{static_cast<::std::size_t>(exponent - minimum_exponent) * 2u};
		return {data[index], data[index + 1u]};
#endif
	}
};

// Check every invariant expressible with the production fixed-width arithmetic.
// The exact rational lower-endpoint proof still belongs to the seed definition
// above; these checks prevent index drift, denormalized factors and an unnoticed
// borrow out of the low word when a correction bit is applied.
[[nodiscard]] inline constexpr bool verify_power10_seed_invariants() noexcept
{
	// These table extents are type-level invariants. Expressing the guard as a
	// discarded compile-time branch avoids MSVC 19.29's C4127 diagnostic under
	// /W4 /WX without weakening the failure result or adding a runtime branch.
	if constexpr ((power10_cache::size - 1u + 10u) / power10_minor_size >= power10_major_size ||
				  (power10_cache::size - 1u) / 32u >= power10_fixup_size)
	{
		return false;
	}
	for (auto const minor : power10_minor)
	{
		if (!(minor >> 63u))
		{
			return false;
		}
	}
	for (auto const major : power10_major)
	{
		if (!(major.hi >> 63u))
		{
			return false;
		}
	}
	constexpr auto uint64_max{(::std::numeric_limits<::std::uint_least64_t>::max)()};
	for (::std::size_t index{}; index != power10_cache::size; ++index)
	{
		auto const result{power10_cache::compute(index)};
		if (!(result.hi >> 63u))
		{
			return false;
		}
		auto const corrected{
			static_cast<bool>((power10_fixups[index >> 5u] >> (index & 31u)) & 1u)};
		if (corrected && result.lo == uint64_max)
		{
			return false;
		}
	}
	return true;
}

static_assert(verify_power10_seed_invariants());

[[nodiscard]] inline constexpr ::std::int_least32_t compute_decimal_exponent(
	::std::int_least32_t binary_exponent, bool regular = true) noexcept
{
	// Over the binary32/binary64 conversion domain, the regular expression is
	// floor(log10(2^binary_exponent)).  The irregular expression is
	// floor(log10((3/4) * 2^binary_exponent)); it supplies the closer lower
	// boundary of an exact power-of-two significand.
	return (binary_exponent * 315653 - static_cast<::std::int_least32_t>(!regular) * 131072) >> 20;
}

// Reduced reciprocal for the regular case.  It is valid on the finite
// conversion domain [-1074, 971]; verify_reduced_decimal_exponents exhaustively
// proves equality with compute_decimal_exponent over that complete interval.
[[nodiscard]] inline constexpr ::std::int_least32_t compute_decimal_exponent_reduced(
	::std::int_least32_t binary_exponent) noexcept
{
	return (binary_exponent * 78913) >> 18;
}

[[nodiscard]] inline constexpr ::std::uint_least8_t compute_exponent_shift(
	::std::int_least32_t binary_exponent, ::std::int_least32_t decimal_exponent) noexcept
{
	// Align the binary significand with the normalized decimal cache entry.  On
	// the supported domain this is e + floor(-q * log2(10)) + 1, where e is
	// binary_exponent and q is decimal_exponent.
	auto const power10_binary_exponent{(-decimal_exponent * 217707) >> 16};
	return static_cast<::std::uint_least8_t>(binary_exponent + power10_binary_exponent + 1);
}

inline constexpr bool verify_reduced_decimal_exponents() noexcept
{
	for (::std::int_least32_t exponent{-1074}; exponent <= 971; ++exponent)
	{
		if (compute_decimal_exponent_reduced(exponent) != compute_decimal_exponent(exponent))
		{
			return false;
		}
	}
	return true;
}

static_assert(verify_reduced_decimal_exponents());

struct exponent_shift_cache
{
	// One generated alignment byte per raw binary64 exponent.  Raw exponent zero
	// uses the subnormal effective exponent, hence the one-step constructor
	// adjustment.  Precision carriers admit only [1, 2046]; the verifier below
	// proves their stored shifts are in [0, extra_shift] before a left shift is
	// formed at runtime.
	inline static constexpr ::std::size_t size{2048u};
	inline static constexpr ::std::int_least32_t binary64_exponent_offset{1075};
	inline static constexpr ::std::uint_least8_t extra_shift{6u};
	::std::uint_least8_t data[size]{};

	inline constexpr exponent_shift_cache() noexcept
	{
		for (::std::size_t raw_exponent{}; raw_exponent != size; ++raw_exponent)
		{
			auto binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - binary64_exponent_offset};
			if (raw_exponent == 0u)
			{
				++binary_exponent;
			}
			auto const decimal_exponent{compute_decimal_exponent(binary_exponent)};
			data[raw_exponent] = static_cast<::std::uint_least8_t>(
				compute_exponent_shift(binary_exponent, decimal_exponent + 1) + extra_shift);
		}
	}
};

// The precision carrier multiplies a normalized 53-bit significand after a
// left shift.  Its error proof assumes that this shift never exceeds
// extra_shift, because those are exactly the guard bits retained below the
// provisional decimal integer.  Check the complete finite-normal raw-exponent
// domain at compile time; raw exponent zero is subnormal and 2047 is non-finite,
// and neither is admitted by the precision carrier.
[[nodiscard]] inline constexpr bool verify_binary64_precision_exponent_shifts() noexcept
{
	exponent_shift_cache shifts{};
	for (::std::size_t raw_exponent{1u}; raw_exponent != 2047u; ++raw_exponent)
	{
		if (exponent_shift_cache::extra_shift < shifts.data[raw_exponent])
		{
			return false;
		}
	}
	return true;
}

static_assert(verify_binary64_precision_exponent_shifts());

struct cache
{
	exponent_shift_cache exponent_shifts;
	alignas(64) power10_cache powers;
};

// Shared mathematical data for shortest and precision DA conversion.
// Format-specific ASCII, fixed, decimal, and scientific writers consume this
// object instead of owning copies of the decimal-power cache.  Where supported,
// hidden visibility makes references non-interposable so hot paths can address
// the inline object directly.  Although visibility itself is independent of
// character encoding, the GNU attribute accepts its mode as the narrow string
// literal "hidden".  GCC converts that literal to the execution character set
// before validating it: with -fexec-charset=IBM1047, the attribute is advertised
// by __has_cpp_attribute but GCC rejects the translated argument.  Require the
// ASCII execution-set spelling before applying the optional decoration; cache
// contents and lookup semantics remain identical when it is omitted.
#if __has_cpp_attribute(__gnu__::__visibility__) && 'A' == 0x41
[[__gnu__::__visibility__("hidden")]]
#endif
alignas(64) inline constexpr cache cached_data{};

} // namespace fast_io::details::da

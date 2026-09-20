#pragma once

#include "../da/impl.h"
#include "../ryu/float64fixed_full_table.h"
#include "../ryu/float64fixed_full_table2.h"
/*
Algorithm: Dragonbox
Author: Jk Jeon
Reference: Abolz
*/
namespace fast_io::details
{

struct uint32x2
{
	::std::uint_least32_t hi;
	::std::uint_least32_t lo;
};

struct uint64x2
{
	::std::uint_least64_t hi;
	::std::uint_least64_t lo;
};

inline constexpr ::std::size_t pow10_float32_table_size{78u};
inline constexpr ::std::size_t pow10_float64_table_size{619u};
inline constexpr ::std::int_least32_t pow10_float32_minimum_exponent{-31};
inline constexpr ::std::int_least32_t pow10_float64_minimum_exponent{-292};
inline constexpr ::std::int_least32_t pow10_float64_maximum_exponent{326};

// Dragonbox and DA normalize the same powers of ten to 128 bits, but their
// endpoint conventions differ.  If L(q) is DA's lower endpoint, exhaustive
// integer comparison over the complete binary64 Dragonbox domain proves
//
//   D(q) = L(q) + 1, q in [-292, -1] or q = 326,
//   D(q) = L(q),     q in [0, 325].
//
// power10_cache::compute accepts the two additional proof indices 618 and 619
// without enlarging the runtime DA cache.  These assertions prove that the
// existing compressed seeds address every factor and correction bit used by
// q = 325 and q = 326.  Generation is constant evaluation only; cached_data
// remains 618 entries with its established alignment and addressing.
//
// This reuse boundary is deliberately mathematical, not physical: Dragonbox
// retains separate contiguous runtime caches.  On the i9-14900 P-core audit,
// deriving both Dragonbox tables from DA storage regressed Clang 23 binary32
// shortest by 17.40% on the common corpus and 19.00% on the broad corpus.
// Sharing only DA's high words still regressed binary64/common by 3.33%, while
// the accompanying binary32 layout shift cost 4.98%.  Consequently the
// constexpr generator reuses DA's proven seeds, but the emitted DB32/DB64
// arrays preserve their former bytes, contiguity, alignment and addressing.
inline constexpr ::std::size_t pow10_float64_last_da_index{
	static_cast<::std::size_t>(pow10_float64_maximum_exponent -
		::fast_io::details::da::power10_cache::minimum_exponent)};
static_assert((pow10_float64_last_da_index + 10u) /
	::fast_io::details::da::power10_minor_size <
	::fast_io::details::da::power10_major_size);
static_assert(pow10_float64_last_da_index / 32u <
	::fast_io::details::da::power10_fixup_size);

[[nodiscard]] inline constexpr ::fast_io::details::da::uint64x2
compute_dragonbox_da_lower_endpoint(::std::int_least32_t exponent) noexcept
{
	auto const index{static_cast<::std::size_t>(
		exponent - ::fast_io::details::da::power10_cache::minimum_exponent)};
	return ::fast_io::details::da::power10_cache::compute(index);
}

struct pow10_float32_table_type
{
	::std::uint_least64_t values[pow10_float32_table_size]{};

	inline constexpr pow10_float32_table_type() noexcept
	{
		for (::std::size_t index{}; index != pow10_float32_table_size; ++index)
		{
			auto const exponent{pow10_float32_minimum_exponent +
				static_cast<::std::int_least32_t>(index)};
			auto const lower{compute_dragonbox_da_lower_endpoint(exponent)};
			// The 64-bit binary32 cache is the high word of the binary64
			// normalized endpoint, rounded upward exactly for negative q.
			values[index] = lower.hi +
				static_cast<::std::uint_least64_t>(exponent < 0);
		}
	}
};

// Replacing a namespace-scope raw array with a generated wrapper changes an
// otherwise non-semantic backend heuristic.  In verified release objects,
// x86-64 GCC 13/15 promote the old arrays to 32 bytes, x86-64 Clang 23 uses
// 16 bytes, and AArch64 Apple Clang retains the element alignment.  State that
// observed layout explicitly so generation cannot perturb neighboring hot
// data.  This compiler/target distinction selects no arithmetic or ISA path;
// it only preserves the emitted object's old layout.
// The open predicates deliberately extend those observations as layout-family
// hypotheses: unmeasured GNU x86-64 majors inherit 32 bytes, other x86-64
// frontends inherit 16 bytes, and non-x86 targets retain element alignment
// rather than inventing an unmeasured over-alignment.  These branches do not
// claim that every admitted compiler historically chose that exact alignment.
// Before changing them, compare symbol/section alignment, COMDAT/linkonce
// coalescing, relocations and addends, constant-load instructions, neighboring
// hot-data placement, and linked text/data size for the affected ABI.
#if defined(__GNUC__) && !defined(__clang__) && defined(__x86_64__) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
template <typename value_type>
inline constexpr ::std::size_t dragonbox_generated_table_alignment{32u};
#elif (defined(__x86_64__) || defined(_M_X64)) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
template <typename value_type>
inline constexpr ::std::size_t dragonbox_generated_table_alignment{16u};
#else
template <typename value_type>
inline constexpr ::std::size_t dragonbox_generated_table_alignment{alignof(value_type)};
#endif

// Preserve the established power-cache spelling for other generated tables
// whose element type is the 64-bit carrier.
inline constexpr ::std::size_t dragonbox_power10_table_alignment{
	dragonbox_generated_table_alignment<::std::uint_least64_t>};

// Keep the historical storage identifiers as well as their layout.  The
// generated aggregate has its array as the first and only member, so the raw
// array references below have the same address and addend as the old arrays;
// retaining the identifiers also avoids growth in unstripped symbol tables.
alignas(dragonbox_power10_table_alignment) inline constexpr pow10_float32_table_type pow10_float32_tb{};
inline constexpr auto const &pow10_float32_table{pow10_float32_tb.values};

// Let U be an N-bit unsigned type, M = 2^N - 1, and require N to be a
// multiple of four, as it is for the binary32 and binary64 carriers.  Then
// 2^N = 1 (mod 5), so q = M / 5 is exact and
//
//   inv5 = M - q + 1 = 2^N - q,
//   5 * inv5 = 4 * 2^N + 1 = 1 (mod 2^N).
//
// Starting with inverse = 1 and multiplying it by inv5 therefore produces
// 5^-i modulo 2^N.  The recurrence multiplies in uintmax_t and then converts
// to U.  If uintmax_t has W value bits, unsigned multiplication first reduces
// modulo 2^W and the conversion reduces modulo 2^N.  Because N <= W and 2^N
// divides 2^W, their composition is exactly reduction modulo 2^N, including
// on targets where U undergoes integral promotion.  Starting limit at M and
// dividing it by five produces
// floor(M / 5^i), because floor(floor(x) / 5) = floor(x / 5).  Thus entry i
// is exactly {5^-i mod 2^N, floor((2^N - 1) / 5^i)}.
// For d = 5^i this pair proves the divisibility test used below.  If x = d*q,
// then x*d^-1 mod 2^N = q <= floor(M/d).  Conversely, if the modular product
// is y <= floor(M/d), then d*y <= M and cannot wrap; x and d*y are congruent
// in [0, M], hence equal, so d divides x.
//
// The call-site bounds also prove the array extents.  Binary32 divisibility
// tests reach this table only for e2 in [7, 39], where the monotone, exact
// fixed-point log10(2) map below gives e5 = floor(e2*log10(2)) - 1 in [1, 10].
// Binary64 reaches it only for e2 in [10, 86], giving
// e5 = floor(e2*log10(2)) - 2 in [1, 23].  Consequently 11 and 24 entries
// cover every reachable index, including the harmless zeroth identity entry.
template <typename uint_type, typename value_type, ::std::size_t table_size>
struct dragonbox_mod5_table_type
{
	value_type values[table_size]{};

	inline constexpr dragonbox_mod5_table_type() noexcept
	{
		constexpr auto digits{::std::numeric_limits<uint_type>::digits};
		static_assert(digits % 4 == 0);
		constexpr uint_type maximum{(::std::numeric_limits<uint_type>::max)()};
		constexpr uint_type inverse_of_five{
			static_cast<uint_type>(maximum - maximum / 5u + 1u)};
		uint_type inverse{1u};
		uint_type limit{maximum};
		for (::std::size_t index{}; index != table_size; ++index)
		{
			values[index] = {inverse, limit};
			inverse = static_cast<uint_type>(static_cast<::std::uintmax_t>(inverse) *
				static_cast<::std::uintmax_t>(inverse_of_five));
			limit = static_cast<uint_type>(limit / 5u);
		}
	}
};

// The generated aggregate has its array as the first and only data member, so
// the facade retains the old extent, element stride and zero address addend.
// Keeping the historical identifier also preserves the storage symbol on the
// audited Itanium-family ABIs; ABIs which encode a variable's type in its
// decorated name do not expose details-namespace objects as a stable contract.
// The generated-table alignment policy preserves the old raw arrays' 32-byte
// GCC x86-64 and 16-byte Clang x86-64 layout, while other targets retain each
// element type's natural alignment.  In particular, binary32 must not inherit
// binary64's alignment.
// A raw-array reference facade also preserves the optimizer's former indexing
// model; making operator[] a wrapper member measurably changed GCC codegen.
alignas(dragonbox_generated_table_alignment<uint32x2>)
	inline constexpr dragonbox_mod5_table_type<::std::uint_least32_t, uint32x2, 11u> float_mod5_tb{};
inline constexpr auto const &float_mod5_table{float_mod5_tb.values};

inline constexpr auto compute_pow10_float32{pow10_float32_table + 31};

struct pow10_float64_table_type
{
	uint64x2 values[pow10_float64_table_size]{};

	inline constexpr pow10_float64_table_type() noexcept
	{
		for (::std::size_t index{}; index != pow10_float64_table_size; ++index)
		{
			auto const exponent{pow10_float64_minimum_exponent +
				static_cast<::std::int_least32_t>(index)};
			auto const lower{compute_dragonbox_da_lower_endpoint(exponent)};
			auto const adjustment{static_cast<::std::uint_least64_t>(
				(exponent < 0) | (exponent == pow10_float64_maximum_exponent))};
			auto const lo{static_cast<::std::uint_least64_t>(lower.lo + adjustment)};
			values[index] = {
				static_cast<::std::uint_least64_t>(lower.hi + (lo < lower.lo)), lo};
		}
	}
};

alignas(dragonbox_power10_table_alignment) inline constexpr pow10_float64_table_type pow10_float64_tb{};
inline constexpr auto const &pow10_float64_table{pow10_float64_tb.values};

// These fingerprints cover every generated word and are independent of target
// byte order.  They pin the proven tables during future seed or factorization
// changes; object-level tests additionally compare the emitted byte streams.
[[nodiscard]] inline consteval bool verify_generated_dragonbox_power10_tables() noexcept
{
	::std::uint_least64_t float32_hash{static_cast<::std::uint_least64_t>(1469598103934665603ULL)};
	for (auto const value : pow10_float32_table)
	{
		float32_hash = (float32_hash ^ value) * static_cast<::std::uint_least64_t>(1099511628211ULL);
	}
	::std::uint_least64_t float64_hash{static_cast<::std::uint_least64_t>(1469598103934665603ULL)};
	for (auto const value : pow10_float64_table)
	{
		float64_hash = (float64_hash ^ value.hi) * static_cast<::std::uint_least64_t>(1099511628211ULL);
		float64_hash = (float64_hash ^ value.lo) * static_cast<::std::uint_least64_t>(1099511628211ULL);
	}
	return float32_hash == static_cast<::std::uint_least64_t>(0xb554955490599626ULL) &&
		float64_hash == static_cast<::std::uint_least64_t>(0x5133d9e69fcf8818ULL);
}

static_assert(verify_generated_dragonbox_power10_tables());

alignas(dragonbox_generated_table_alignment<uint64x2>)
	inline constexpr dragonbox_mod5_table_type<::std::uint_least64_t, uint64x2, 24u> float64_mod5_tb{};
inline constexpr auto const &float64_mod5_table{float64_mod5_tb.values};

// Word-wise fingerprints are independent of target byte order and pin every
// generated entry to the previously audited tables.  Object-level validation
// below additionally compares emitted bytes, symbols and relocations.
[[nodiscard]] inline consteval bool verify_generated_dragonbox_mod5_tables() noexcept
{
	::std::uint_least64_t float32_hash{static_cast<::std::uint_least64_t>(1469598103934665603ULL)};
	for (::std::size_t index{}; index != 11u; ++index)
	{
		auto const value{float_mod5_table[index]};
		float32_hash = (float32_hash ^ value.hi) * static_cast<::std::uint_least64_t>(1099511628211ULL);
		float32_hash = (float32_hash ^ value.lo) * static_cast<::std::uint_least64_t>(1099511628211ULL);
	}
	::std::uint_least64_t float64_hash{static_cast<::std::uint_least64_t>(1469598103934665603ULL)};
	for (::std::size_t index{}; index != 24u; ++index)
	{
		auto const value{float64_mod5_table[index]};
		float64_hash = (float64_hash ^ value.hi) * static_cast<::std::uint_least64_t>(1099511628211ULL);
		float64_hash = (float64_hash ^ value.lo) * static_cast<::std::uint_least64_t>(1099511628211ULL);
	}
	return float32_hash == static_cast<::std::uint_least64_t>(0x01ca5dd06f009800ULL) &&
		float64_hash == static_cast<::std::uint_least64_t>(0x6f8a5d3fed1adca2ULL);
}

static_assert(verify_generated_dragonbox_mod5_tables());

inline constexpr auto compute_pow10_float64{pow10_float64_table + 292};

template <typename flt>
inline constexpr bool dragonbox_uses_binary32_core{
	iec559_traits<flt>::mbits <= iec559_traits<float>::mbits &&
	iec559_traits<flt>::ebits <= iec559_traits<float>::ebits};

/*
The binary64 precision accelerators consume only the already-punned fraction
and exponent fields; none performs arithmetic in `flt`.  Consequently their
mathematical input is determined by the IEC layout, not by whether the frontend
spells that layout `double`, `_Float64`, or an ABI-equivalent type.

For an admitted type below, mbits=52 and ebits=11 give the exact finite-value
identity

	magnitude = M * 2^(E - 1023 - 52),

with the usual E=0 subnormal adjustment.  A 64-value-bit unsigned carrier and
an equal object/carrier size prove that the punned sign, exponent, and fraction
occupy exactly the same 64-bit domain used by every binary64 DA bound.  Thus
substituting an ABI-equivalent spelling leaves every integer operand, interval
comparison, tie rejection, and emitted coefficient unchanged.  Conversely,
any binary16/32/80/128 format fails at least one field-width or layout
condition, so no non-binary64 representation can enter these specializations.
*/
template <typename flt>
inline constexpr bool dragonbox_uses_binary64_core{[]() constexpr noexcept
{
	using clean_type = ::std::remove_cvref_t<flt>;
	using trait = ::fast_io::details::iec559_traits<clean_type>;
	using mantissa_type = typename trait::mantissa_type;
	return trait::mbits == 52u && trait::ebits == 11u &&
		::std::numeric_limits<mantissa_type>::digits == 64 &&
		sizeof(clean_type) == sizeof(mantissa_type);
}()};

static_assert(::fast_io::details::dragonbox_uses_binary64_core<double>);
#if defined(FAST_IO_HAS_FLOAT64_TYPE)
static_assert(::fast_io::details::dragonbox_uses_binary64_core<_Float64>);
#endif

template <typename flt>
inline constexpr ::std::int_least32_t dragonbox_kappa{
	::fast_io::details::dragonbox_uses_binary32_core<flt> ? 1 : 2};

/*
The decimal carrier is sized for decimal digits, not merely for the source
format's stored binary fraction.  Binary80 has a 64-bit significand but its
shortest round-trip coefficient can contain 21 decimal digits, which cannot
fit in uint64_t.  Native uint128 therefore owns every admitted coefficient
wider than 19 digits.  The binary32/64 Dragonbox kernels retain their original
uint32/uint64 carriers and generated-table ABI.
*/
template <typename flt>
using dragonbox_decimal_mantissa_type =
	::std::conditional_t<::fast_io::details::dragonbox_uses_binary32_core<flt>,
		::std::uint_least32_t,
#if defined(__SIZEOF_INT128__)
		::std::conditional_t<
			(19u < ::fast_io::details::iec559_traits<flt>::m10digits),
			__uint128_t, typename iec559_traits<flt>::mantissa_type>
#else
		typename iec559_traits<flt>::mantissa_type
#endif
	>;

// Let X_i be the historical 128-bit normalized cache word for
// 10^(-342+i).  Multiplication by ten moves to the next decimal exponent.
// Its normalized successor is
//
//   X_(i+1) = floor(10*X_i / 2^s_i) + c_i,
//   s_i = 4 when 10*X_i >= 2^131, and 3 otherwise.
//
// The shift follows directly from restoring bit 127 after multiplication.
// The one-bit c_i sequence records which truncated transitions require the
// directed endpoint to be incremented; all 49 transitions are pinned by the
// correction bitmap below.  This is a separate proof domain from the DA
// cache: its major seeds start at q=-303 and its runtime cache starts at
// q=-293, whereas this scan cache starts at q=-342.  Therefore the q=-342
// seed and its 49-position correction bitmap are necessary and must not be
// described as reuse of the DA seed proof.
//
// Constant evaluation deliberately uses two 64-bit words.  For a word x,
// x*10 = (x<<3)+(x<<1); the two discarded shift fragments plus the addition
// carry are exactly floor(10*x/2^64).  Applying that identity to both limbs
// produces the complete 132-bit product without native 128-bit integers.  A
// word product has an upper part at most nine; after the low-limb carry is
// folded into the high product, the combined top part is still at most nine,
// so neither upper-part addition can overflow.  Its top nibble decides the
// 2^131 threshold.  With s=3 the threshold proves the quotient is below
// 2^128; with s=4, 10*X_i < 10*2^128 < 16*2^128 proves the same.  The
// cross-word shift therefore retains the complete quotient, and the final
// addition explicitly propagates the only possible correction carry.  Thus
// native-u128 and no-u128 targets instantiate identical bytes.
struct pow10_float64_scan_low_table_type
{
	uint64x2 values[50u]{};

	struct word_times_ten
	{
		::std::uint_least64_t upper;
		::std::uint_least64_t lower;
	};

	[[nodiscard]] inline static constexpr word_times_ten
	multiply_word_by_ten(::std::uint_least64_t value) noexcept
	{
		auto const times_eight{static_cast<::std::uint_least64_t>(value << 3u)};
		auto const times_two{static_cast<::std::uint_least64_t>(value << 1u)};
		auto const lower{static_cast<::std::uint_least64_t>(times_eight + times_two)};
		return {static_cast<::std::uint_least64_t>((value >> 61u) + (value >> 63u) +
			(lower < times_eight)),
			lower};
	}

	inline constexpr pow10_float64_scan_low_table_type() noexcept
	{
		values[0] = {static_cast<::std::uint_least64_t>(0xeef453d6923bd65aULL), static_cast<::std::uint_least64_t>(0x113faa2906a13b40ULL)};
		// Bits 47 and 48 are zero; retaining 49 addressable positions states the
		// complete transition domain rather than only the bitmap's numeric width.
		constexpr ::std::uint_least64_t correction_bits{static_cast<::std::uint_least64_t>(0x4d8c9ca29a54ULL)};
		for (::std::size_t index{}; index != 49u; ++index)
		{
			auto const low_product{multiply_word_by_ten(values[index].lo)};
			auto const high_product{multiply_word_by_ten(values[index].hi)};
			auto const middle{static_cast<::std::uint_least64_t>(
				high_product.lower + low_product.upper)};
			auto const top{static_cast<::std::uint_least64_t>(
				high_product.upper + (middle < high_product.lower))};
			auto const shift{static_cast<unsigned>(top >= 8u ? 4u : 3u)};
			uint64x2 next{
				static_cast<::std::uint_least64_t>((top << (64u - shift)) | (middle >> shift)),
				static_cast<::std::uint_least64_t>(
					(middle << (64u - shift)) | (low_product.lower >> shift))};
			auto const uncorrected_low{next.lo};
			next.lo = static_cast<::std::uint_least64_t>(
				next.lo + ((correction_bits >> index) & 1u));
			next.hi = static_cast<::std::uint_least64_t>(
				next.hi + (next.lo < uncorrected_low));
			values[index + 1u] = next;
		}
	}
};

// Preserve the historical storage identifier.  The generated wrapper contains
// only the raw array, so its size, alignment, address and relocation addends
// are the same as the former namespace-scope array.  The raw-array reference
// facade preserves the former subscript expression in runtime consumers; it
// owns no storage and therefore cannot add a symbol or relocation.
alignas(dragonbox_power10_table_alignment) inline constexpr pow10_float64_scan_low_table_type
	pow10_float64_scan_low_tb{};
inline constexpr auto const &pow10_float64_scan_low_table{
	pow10_float64_scan_low_tb.values};

[[nodiscard]] inline consteval bool verify_generated_pow10_float64_scan_low_table() noexcept
{
	::std::uint_least64_t hash{static_cast<::std::uint_least64_t>(1469598103934665603ULL)};
	for (auto const value : pow10_float64_scan_low_tb.values)
	{
		// Every historical endpoint is normalized.  Besides pinning that
		// invariant, this rejects a correction carry escaping bit 127: such a
		// carry can only wrap the two-word result to zero.
		if ((value.hi >> 63u) == 0u)
		{
			return false;
		}
		hash = (hash ^ value.hi) * static_cast<::std::uint_least64_t>(1099511628211ULL);
		hash = (hash ^ value.lo) * static_cast<::std::uint_least64_t>(1099511628211ULL);
	}
	return hash == static_cast<::std::uint_least64_t>(0x186321208d0ed723ULL);
}

static_assert(sizeof(pow10_float64_scan_low_table_type) == sizeof(uint64x2) * 50u);
static_assert(alignof(pow10_float64_scan_low_table_type) == alignof(uint64x2));
static_assert(verify_generated_pow10_float64_scan_low_table());

[[nodiscard]] inline constexpr uint64x2 compute_pow10_float64_scan(::std::int_least32_t exponent) noexcept
{
	if (exponent < -292)
	{
		return pow10_float64_scan_low_table[static_cast<::std::size_t>(exponent + 342)];
	}
	return compute_pow10_float64[exponent];
}

template <typename mantissa_type>
struct m10_result
{
	mantissa_type m10;
	::std::int_least32_t e10;
};

inline constexpr ::std::int_least32_t mul_ln2_div_ln10_floor(::std::int_least32_t e) noexcept
{
	return (e * 1262611) >> 22;
}

inline constexpr ::std::int_least32_t mul_ln10_div_ln2_floor(::std::int_least32_t e) noexcept
{
	return (e * 1741647) >> 19;
}

inline constexpr bool mul_parity_float64(::std::uint_least64_t two_f, ::std::uint_least64_t pow10_low,
										 ::std::uint_least64_t pow10_high, ::std::int_least32_t beta_minus_1) noexcept
{
	::std::uint_least64_t const p01{two_f * pow10_high};
	::std::uint_least64_t const p10{::fast_io::intrinsics::umulh(two_f, pow10_low)};
	::std::uint_least64_t const mid{p01 + p10};
	constexpr ::std::uint_least64_t one{1};
	return (mid & (one << (64 - beta_minus_1)));
}

inline constexpr bool mul_parity_float32(::std::uint_least64_t two_f, ::std::uint_least64_t pow10,
										 ::std::int_least32_t beta_minus_1) noexcept
{
	::std::uint_least64_t const p01{two_f * pow10};
	constexpr ::std::uint_least64_t one{1};
	return (p01 & (one << (64 - beta_minus_1)));
}

template <my_unsigned_integral value_type>
inline constexpr bool multiple_of_pow2_unchecked(value_type value, ::std::uint_least32_t e2) noexcept
{
	constexpr value_type one{1};
	return !(value & ((one << e2) - 1));
}

template <my_unsigned_integral value_type>
inline constexpr bool multiple_of_pow2(value_type value, ::std::int_least32_t e2) noexcept
{
	constexpr ::std::int_least32_t e2max_bits{static_cast<::std::int_least32_t>(sizeof(value_type) * 8)};
	return e2 < e2max_bits && multiple_of_pow2_unchecked(value, static_cast<::std::uint_least32_t>(e2));
}

inline constexpr bool multiple_of_pow5(::std::uint_least64_t value, ::std::uint_least32_t e5) noexcept
{
	auto m5{float64_mod5_table[e5]};
	return value * m5.hi <= m5.lo;
}

/*
After the exponent-bias adjustment, the source is m*2^e2.  The normal
interval uses doubled integers

	L=(2m-1)*2^(e2-1), C=(2m)*2^(e2-1),
	R=(2m+1)*2^(e2-1).

Writing k=minus_k, a boundary is an integer in the decimal grid precisely
when

	F*2^(e2-1) / 10^k
		= F*2^(e2-k-1) / 5^k

is integral (for k<0 the same identity moves 2^-k*5^-k into the
numerator).  L and R have odd F, so they cannot supply a missing factor of
two; C may, which is why the midpoint helper has the extra multiple-of-two
case.  Substituting the finite binary32/binary64 e2 domains into the exact
floor-log expression for k gives the ranges below:

  * in the middle range every required factor is already in the explicit
	2^(e2-1) and 10^-k terms;
  * above it, only 5^k|F remains, tested by the proved modular inverse table;
  * below it, an endpoint's odd F cannot supply the missing two, while a
	center is integral iff 2^(k-e2+1)|F.

At the upper false boundary binary64 has k>=24 and
5^24>2^55>F; binary32 has k>=11 and 5^11>2^25>F.  Therefore divisibility is
impossible there.  These cases are exhaustive and establish that each helper
is an iff test for exact grid membership, not a recovered-cache
approximation.
*/
inline constexpr bool is_integral_end_point(::std::uint_least64_t two_f, ::std::int_least32_t e2,
											::std::int_least32_t minus_k) noexcept
{
	if (e2 < -2)
	{
		return false;
	}
	if (e2 <= 9)
	{
		return true;
	}
	if (e2 <= 86)
	{
		return multiple_of_pow5(two_f, static_cast<::std::uint_least32_t>(minus_k));
	}
	return false;
}

inline constexpr bool is_integral_mid_point(::std::uint_least64_t two_f, ::std::int_least32_t e2,
											::std::int_least32_t minus_k) noexcept
{
	if (e2 < -4)
	{
		return multiple_of_pow2(two_f, minus_k - e2 + 1);
	}
	if (e2 <= 9)
	{
		return true;
	}
	if (e2 <= 86)
	{
		return multiple_of_pow5(two_f, static_cast<::std::uint_least32_t>(minus_k));
	}
	return false;
}

inline constexpr bool multiple_of_pow5_float32(::std::uint_least32_t value, ::std::uint_least32_t e5) noexcept
{
	auto m5{float_mod5_table[e5]};
	return value * m5.hi <= m5.lo;
}

inline constexpr bool is_integral_end_point_float32(::std::uint_least32_t two_f, ::std::int_least32_t e2,
													::std::int_least32_t minus_k) noexcept
{
	if (e2 < -1)
	{
		return false;
	}
	if (e2 <= 6)
	{
		return true;
	}
	if (e2 <= 39)
	{
		return multiple_of_pow5_float32(two_f, static_cast<::std::uint_least32_t>(minus_k));
	}
	return false;
}

inline constexpr bool is_integral_mid_point_float32(::std::uint_least32_t two_f, ::std::int_least32_t e2,
													::std::int_least32_t minus_k) noexcept
{
	if (e2 < -2)
	{
		return multiple_of_pow2(two_f, minus_k - e2 + 1);
	}
	if (e2 <= 6)
	{
		return true;
	}
	if (e2 <= 39)
	{
		return multiple_of_pow5_float32(two_f, static_cast<::std::uint_least32_t>(minus_k));
	}
	return false;
}

// This helper is reached only for a finite normal power of two (the caller has
// proved a zero raw mantissa and a raw exponent above one) after the integral
// shortcut has failed.  `cold` is therefore a control-flow/layout hint, not a
// compiler- or ISA-specific performance claim.  The interval calculation reads
// only its argument and inline constexpr caches; omitting the optional attribute
// changes neither its Schubfach interval nor its selected decimal.
template <typename flt>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
inline constexpr m10_result<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>
schubfach_asymmetric_interval(::std::int_least32_t e2) noexcept
{
	using trait = iec559_traits<flt>;

	constexpr ::std::int_least32_t mbits{trait::mbits};
	::std::int_least32_t const minus_k{(e2 * 1262611 - 524031) >> 22};
	::std::int_least32_t const plus_k{-minus_k};
	::std::int_least32_t const beta{e2 + mul_ln10_div_ln2_floor(plus_k)};
	::std::uint_least32_t const rshift{static_cast<::std::uint_least32_t>(63 - mbits - beta)};
	if constexpr (sizeof(flt) == sizeof(::std::uint_least64_t))
	{
		uint64x2 const pw{compute_pow10_float64[plus_k]};
		::std::uint_least64_t const pw_hi{pw.hi};
		::std::uint_least64_t const lower_endpoint{(pw_hi - (pw_hi >> (mbits + 2))) >> rshift};
		::std::uint_least64_t const upper_endpoint{(pw_hi + (pw_hi >> (mbits + 1))) >> rshift};
		bool const lower_endpoint_is_not_integer{!(2 <= e2 && e2 <= 4)};
		::std::uint_least64_t const xi{lower_endpoint + lower_endpoint_is_not_integer};
		::std::uint_least64_t q{upper_endpoint};
		q /= 10;
		if (q * 10 >= xi)
		{
			return {q, minus_k + 1};
		}
		q = ((pw_hi >> (rshift - 1)) + 1) >> 1;
		if (e2 == -77)
		{
			q -= (q & 1u);
		}
		else
		{
			q += (q < xi);
		}

		return {q, minus_k};
	}
	else
	{
		::std::uint_least64_t const pw{compute_pow10_float32[plus_k]};
		::std::uint_least64_t const lower_endpoint{(pw - (pw >> (mbits + 2))) >> rshift};
		::std::uint_least64_t const upper_endpoint{(pw + (pw >> (mbits + 1))) >> rshift};
		::std::uint_least64_t q{upper_endpoint};
		bool const lower_endpoint_is_not_integer{!(2 <= e2 && e2 <= 3)};
		::std::uint_least64_t const xi{lower_endpoint + lower_endpoint_is_not_integer};
		q /= 10;
		if (q * 10 >= xi)
		{
			return {static_cast<::std::uint_least32_t>(q), minus_k + 1};
		}
		q = ((pw >> (rshift - 1)) + 1) >> 1;
		if (e2 == -35)
		{
			q -= (q & 1u);
		}
		else
		{
			q += (q < xi);
		}
		return {static_cast<::std::uint_least32_t>(q), minus_k};
	}
}

// The shortest-conversion kernel has no observable mutation: its result is a
// function of m2, e2 and immutable inline constexpr caches.  That establishes
// the `pure` contract.  It is also the normal finite shortest-conversion
// workhorse, so `hot` is a profile-free layout/inlining hint derived from the
// dispatch domain, not from a target-specific benchmark cutoff.  A compiler
// lacking either optional attribute executes the identical arithmetic and tie
// selection.
template <typename flt>
#if __has_cpp_attribute(__gnu__::__pure__)
[[__gnu__::__pure__]]
#endif
#if __has_cpp_attribute(__gnu__::__hot__)
[[__gnu__::__hot__]]
#endif
inline constexpr m10_result<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>
dragonbox_main(typename iec559_traits<flt>::mantissa_type m2, ::std::int_least32_t e2) noexcept
{
	using trait = iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;

	constexpr ::std::size_t mbits{trait::mbits};
	constexpr ::std::size_t ebits{trait::ebits};
	constexpr ::std::uint_least32_t bias{(static_cast<::std::uint_least32_t>(1 << ebits) >> 1) - 1};
	constexpr ::std::int_least32_t exponent_bias{bias + mbits};
	constexpr mantissa_type mflags{static_cast<mantissa_type>(static_cast<mantissa_type>(1) << mbits)};
	constexpr ::std::int_least32_t kappa{::fast_io::details::dragonbox_kappa<flt>};
	constexpr ::std::uint_least32_t big_divisor{kappa == 2 ? 1000 : 100};
	constexpr ::std::uint_least32_t small_divisor{big_divisor / 10};
	constexpr ::std::uint_least32_t small_divisor_div2{small_divisor / 2};

	if (e2 == 0) [[unlikely]]
	{
		constexpr ::std::int_least32_t e2bias{1 - static_cast<::std::int_least32_t>(exponent_bias)};
		e2 = e2bias;
	}
	else
	{
		auto e2_temp{e2};
		auto const raw_m2{m2};
		e2 -= exponent_bias;
		m2 |= mflags;
		::std::uint_least32_t pos_e2{static_cast<::std::uint_least32_t>(-e2)};
		if (pos_e2 < mbits && multiple_of_pow2_unchecked(m2, pos_e2)) [[unlikely]]
		{
			return {static_cast<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>(m2 >> pos_e2), 0};
		}
		if constexpr (!(::fast_io::details::dragonbox_uses_binary32_core<flt> && sizeof(flt) < sizeof(float)))
		{
			if (raw_m2 == 0 && e2_temp > 1) [[unlikely]]
			{
				return schubfach_asymmetric_interval<flt>(e2);
			}
		}
	}
	bool const is_even{(m2 & 1u) == 0u};
	::std::int_least32_t const minus_k{mul_ln2_div_ln10_floor(e2) - kappa};
	::std::int_least32_t const plus_k{-minus_k};
	::std::int_least32_t const beta_minus_1{e2 + mul_ln10_div_ln2_floor(plus_k)};
	if constexpr (sizeof(flt) == sizeof(::std::uint_least64_t))
	{
		uint64x2 const pow10{compute_pow10_float64[plus_k]};
		::std::uint_least64_t const pow10_lo{pow10.lo};
		::std::uint_least64_t const pow10_hi{pow10.hi};
		::std::uint_least32_t const delta{static_cast<::std::uint_least32_t>(pow10_hi >> (63 - beta_minus_1))};
		::std::uint_least64_t const two_fc{static_cast<::std::uint_least64_t>(m2) << 1},
			two_fl{two_fc - 1}, two_fr{two_fc + 1};
		::std::uint_least64_t const zi{::fast_io::intrinsics::umulh(two_fr << beta_minus_1, ::fast_io::intrinsics::pack_ul64(pow10_lo, pow10_hi))};
		::std::uint_least64_t q;
		::std::uint_least32_t r;
		q = zi / big_divisor;
		r = static_cast<::std::uint_least32_t>(zi % big_divisor);
		if (r < delta)
		{
			if (r || is_even || !is_integral_end_point(two_fr, e2, minus_k))
			{
				return {q, minus_k + kappa + 1};
			}
			--q;
			r = big_divisor;
		}
		else if (r == delta)
		{
			if ((is_even && is_integral_end_point(two_fl, e2, minus_k)) ||
				mul_parity_float64(two_fl, pow10_lo, pow10_hi, beta_minus_1))
			{
				return {q, minus_k + kappa + 1};
			}
		}
		q *= 10;
		::std::uint_least32_t const dist{static_cast<::std::uint_least32_t>(r - delta / 2 + small_divisor_div2)};
		constexpr ::std::uint_least32_t distq_divisor_divisor{100};
		::std::uint_least32_t const dist_q{dist / distq_divisor_divisor};
		::std::uint_least32_t const dist_q_mul100{dist_q * distq_divisor_divisor};
		q += dist_q;
		if (dist == dist_q_mul100)
		{
			bool const approx_y_parity{(dist & 1u) != 0u};
			if ((mul_parity_float64(two_fc, pow10_lo, pow10_hi, beta_minus_1) != approx_y_parity) ||
				((q & 1) && is_integral_mid_point(two_fc, e2, minus_k)))
			{
				--q;
			}
		}
		return {q, minus_k + kappa};
	}
	else
	{
		::std::uint_least64_t const pow10{compute_pow10_float32[plus_k]};
		::std::uint_least32_t const two_fc{static_cast<::std::uint_least32_t>(m2) << 1},
			two_fl{two_fc - 1}, two_fr{two_fc + 1};
		::std::uint_least32_t const zi{::fast_io::intrinsics::umulh(two_fr << beta_minus_1, pow10)};
		::std::uint_least32_t q{zi / big_divisor};
		::std::uint_least32_t r{zi % big_divisor};
		::std::uint_least32_t const delta{static_cast<::std::uint_least32_t>(pow10 >> (63 - beta_minus_1))};
		if (r < delta)
		{
			if (r || is_even || !is_integral_end_point_float32(two_fr, e2, minus_k))
			{
				return {q, minus_k + kappa + 1};
			}
			--q;
			r = big_divisor;
		}
		else if (r == delta)
		{
			if ((is_even && is_integral_end_point_float32(two_fl, e2, minus_k)) ||
				mul_parity_float32(two_fl, pow10, beta_minus_1))
			{
				return {q, minus_k + kappa + 1};
			}
		}
		q *= 10;
		::std::uint_least32_t const dist{static_cast<::std::uint_least32_t>(r - delta / 2 + small_divisor_div2)};
		::std::uint_least32_t const dist_q{dist / small_divisor};
		::std::uint_least32_t const dist_q_mul100{dist_q * small_divisor};
		q += dist_q;
		if (dist == dist_q_mul100)
		{
			bool const approx_y_parity{((dist ^ small_divisor_div2) & 1u) != 0u};
			if ((mul_parity_float32(two_fc, pow10, beta_minus_1) != approx_y_parity) ||
				((q & 1) && is_integral_mid_point_float32(two_fc, e2, minus_k)))
			{
				--q;
			}
		}
		return {q, minus_k + kappa};
	}
}

struct dragonbox_mul_result
{
	::std::uint_least64_t integer_part{};
	bool is_integer{};
};

struct dragonbox_mul_parity_result
{
	bool parity{};
	bool is_integer{};
};

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool dragonbox_nearest_normal_left_closed(bool negative, bool is_even) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		return is_even;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_odd)
	{
		return !is_even;
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
[[nodiscard]] inline constexpr bool dragonbox_nearest_normal_right_closed(bool negative, bool is_even) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		return is_even;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_odd)
	{
		return !is_even;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_plus_infinity)
	{
		return negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_minus_infinity)
	{
		return !negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_zero)
	{
		return true;
	}
	else
	{
		return false;
	}
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool dragonbox_nearest_shorter_left_closed(bool negative) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		return true;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_odd)
	{
		return false;
	}
	else
	{
		return ::fast_io::details::dragonbox_nearest_normal_left_closed<rounding>(negative, true);
	}
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool dragonbox_nearest_shorter_right_closed(bool negative) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		return true;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_odd)
	{
		return false;
	}
	else
	{
		return ::fast_io::details::dragonbox_nearest_normal_right_closed<rounding>(negative, true);
	}
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool
dragonbox_nearest_binary_tie_prefer_down(bool negative, ::std::uint_least64_t decimal_significand) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		return (decimal_significand & 1u) != 0u;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_odd)
	{
		return (decimal_significand & 1u) == 0u;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_plus_infinity)
	{
		return negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_minus_infinity)
	{
		return !negative;
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_zero)
	{
		return true;
	}
	else
	{
		return false;
	}
}

[[nodiscard]] inline constexpr dragonbox_mul_result
dragonbox_compute_mul_float32(::std::uint_least64_t u, ::std::uint_least64_t cache) noexcept
{
	auto const lower{u * cache};
	return {::fast_io::intrinsics::umulh(u, cache),
			(static_cast<::std::uint_least32_t>(lower >> 32u) == 0u)};
}

[[nodiscard]] inline constexpr dragonbox_mul_parity_result
dragonbox_compute_mul_parity_float32(::std::uint_least64_t two_f, ::std::uint_least64_t cache,
									 ::std::int_least32_t beta) noexcept
{
	auto const lower{two_f * cache};
	return {(lower & (::std::uint_least64_t{1u} << static_cast<unsigned>(64 - beta))) != 0u,
			(static_cast<::std::uint_least32_t>(lower >> static_cast<unsigned>(32 - beta)) == 0u)};
}

[[nodiscard]] inline constexpr dragonbox_mul_result
dragonbox_compute_mul_float64(::std::uint_least64_t u, ::std::uint_least64_t cache_lo,
							  ::std::uint_least64_t cache_hi) noexcept
{
	::std::uint_least64_t high0{};
	auto const low0{::fast_io::intrinsics::umul(u, cache_lo, high0)};
	auto const mid{static_cast<::std::uint_least64_t>(high0 + u * cache_hi)};
	return {::fast_io::intrinsics::umulh(u, ::fast_io::intrinsics::pack_ul64(cache_lo, cache_hi)),
			(low0 | mid) == 0u};
}

[[nodiscard]] inline constexpr dragonbox_mul_parity_result
dragonbox_compute_mul_parity_float64(::std::uint_least64_t two_f, ::std::uint_least64_t cache_lo,
									 ::std::uint_least64_t cache_hi, ::std::int_least32_t beta) noexcept
{
	::std::uint_least64_t high0{};
	auto const low0{::fast_io::intrinsics::umul(two_f, cache_lo, high0)};
	auto const mid{static_cast<::std::uint_least64_t>(high0 + two_f * cache_hi)};
	return {(mid & (::std::uint_least64_t{1u} << static_cast<unsigned>(64 - beta))) != 0u,
			((mid << static_cast<unsigned>(beta)) | (low0 >> static_cast<unsigned>(64 - beta))) == 0u};
}

template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr m10_result<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>
dragonbox_nearest_shorter_interval([[maybe_unused]] typename iec559_traits<flt>::mantissa_type m2,
								   ::std::int_least32_t e2, bool negative) noexcept
{
	using trait = iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr ::std::int_least32_t mbits{static_cast<::std::int_least32_t>(trait::mbits)};
	::std::int_least32_t const minus_k{(e2 * 1262611 - 524031) >> 22};
	::std::int_least32_t const plus_k{-minus_k};
	::std::int_least32_t const beta{e2 + mul_ln10_div_ln2_floor(plus_k)};
	::std::uint_least32_t const rshift{static_cast<::std::uint_least32_t>(63 - mbits - beta)};
	bool const include_left{::fast_io::details::dragonbox_nearest_shorter_left_closed<rounding>(negative)};
	bool const include_right{::fast_io::details::dragonbox_nearest_shorter_right_closed<rounding>(negative)};
	if constexpr (sizeof(flt) == sizeof(::std::uint_least64_t))
	{
		uint64x2 const pw{compute_pow10_float64[plus_k]};
		::std::uint_least64_t xi{(pw.hi - (pw.hi >> (mbits + 2))) >> rshift};
		::std::uint_least64_t zi{(pw.hi + (pw.hi >> (mbits + 1))) >> rshift};
		if (!include_right && 0 <= e2 && e2 <= 3)
		{
			--zi;
		}
		if (!include_left || !(2 <= e2 && e2 <= 4))
		{
			++xi;
		}
		auto q{zi / 10u};
		if (q * 10u >= xi)
		{
			return {static_cast<mantissa_type>(q), minus_k + 1};
		}
		q = ((pw.hi >> static_cast<unsigned>(rshift - 1u)) + 1u) >> 1u;
		if (::fast_io::details::dragonbox_nearest_binary_tie_prefer_down<rounding>(negative, q) && e2 == -77)
		{
			--q;
		}
		else if (q < xi)
		{
			++q;
		}
		return {static_cast<mantissa_type>(q), minus_k};
	}
	else
	{
		::std::uint_least64_t const pw{compute_pow10_float32[plus_k]};
		::std::uint_least64_t xi{(pw - (pw >> (mbits + 2))) >> rshift};
		::std::uint_least64_t zi{(pw + (pw >> (mbits + 1))) >> rshift};
		if (!include_right && 0 <= e2 && e2 <= 2)
		{
			--zi;
		}
		if (!include_left || !(2 <= e2 && e2 <= 3))
		{
			++xi;
		}
		auto q{zi / 10u};
		if (q * 10u >= xi)
		{
			return {static_cast<mantissa_type>(q), minus_k + 1};
		}
		q = ((pw >> static_cast<unsigned>(rshift - 1u)) + 1u) >> 1u;
		if (::fast_io::details::dragonbox_nearest_binary_tie_prefer_down<rounding>(negative, q) && e2 == -35)
		{
			--q;
		}
		else if (q < xi)
		{
			++q;
		}
		return {static_cast<mantissa_type>(q), minus_k};
	}
}

template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr m10_result<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>
dragonbox_main_nearest_policy(typename iec559_traits<flt>::mantissa_type m2, ::std::int_least32_t e2,
							  bool negative) noexcept
{
	using trait = iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;

	constexpr ::std::size_t mbits{trait::mbits};
	constexpr ::std::size_t ebits{trait::ebits};
	constexpr ::std::uint_least32_t bias{(static_cast<::std::uint_least32_t>(1 << ebits) >> 1) - 1};
	constexpr ::std::int_least32_t exponent_bias{bias + mbits};
	constexpr mantissa_type mflags{static_cast<mantissa_type>(static_cast<mantissa_type>(1) << mbits)};
	constexpr ::std::int_least32_t kappa{::fast_io::details::dragonbox_kappa<flt>};
	constexpr ::std::uint_least32_t big_divisor{kappa == 2 ? 1000 : 100};
	constexpr ::std::uint_least32_t small_divisor{big_divisor / 10};
	constexpr ::std::uint_least32_t small_divisor_div2{small_divisor / 2};

	if (e2 == 0) [[unlikely]]
	{
		constexpr ::std::int_least32_t e2bias{1 - static_cast<::std::int_least32_t>(exponent_bias)};
		e2 = e2bias;
	}
	else
	{
		auto const raw_mantissa{m2};
		e2 -= exponent_bias;
		m2 |= mflags;
		::std::uint_least32_t pos_e2{static_cast<::std::uint_least32_t>(-e2)};
		if (pos_e2 < mbits && multiple_of_pow2_unchecked(m2, pos_e2)) [[unlikely]]
		{
			return {static_cast<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>(m2 >> pos_e2), 0};
		}
		if constexpr (!(::fast_io::details::dragonbox_uses_binary32_core<flt> && sizeof(flt) < sizeof(float)))
		{
			if (!raw_mantissa) [[unlikely]]
			{
				return ::fast_io::details::dragonbox_nearest_shorter_interval<flt, rounding>(m2, e2, negative);
			}
		}
	}
	bool const is_even{(m2 & 1u) == 0u};
	bool const include_left{::fast_io::details::dragonbox_nearest_normal_left_closed<rounding>(negative, is_even)};
	bool const include_right{::fast_io::details::dragonbox_nearest_normal_right_closed<rounding>(negative, is_even)};
	::std::int_least32_t const minus_k{mul_ln2_div_ln10_floor(e2) - kappa};
	::std::int_least32_t const plus_k{-minus_k};
	::std::int_least32_t const beta{e2 + mul_ln10_div_ln2_floor(plus_k)};
	if constexpr (sizeof(flt) == sizeof(::std::uint_least64_t))
	{
		uint64x2 const pow10{compute_pow10_float64[plus_k]};
		::std::uint_least64_t const pow10_lo{pow10.lo};
		::std::uint_least64_t const pow10_hi{pow10.hi};
		::std::uint_least32_t const delta{static_cast<::std::uint_least32_t>(pow10_hi >> (63 - beta))};
		::std::uint_least64_t const two_fc{static_cast<::std::uint_least64_t>(m2) << 1},
			two_fl{two_fc - 1}, two_fr{two_fc + 1};
		auto const z_result{::fast_io::details::dragonbox_compute_mul_float64(
			two_fr << static_cast<unsigned>(beta), pow10_lo, pow10_hi)};
		::std::uint_least64_t q{z_result.integer_part / big_divisor};
		::std::uint_least32_t r{static_cast<::std::uint_least32_t>(z_result.integer_part % big_divisor)};
		/*
		Endpoint membership must be decided in the exact decimal grid, not
		from `z_result.is_integer`.  The latter says that the discarded limbs
		of multiplication by the finite recovered cache are zero.  It is not
		equivalent to exact divisibility of the binary endpoint: for example,
		an endpoint can be an integer multiple of 10^k while the upward
		recovered cache leaves a nonzero discarded limb.

		For r==0, q lies on the right boundary exactly when
		is_integral_end_point(two_fr,e2,minus_k) holds.  The helper proves
		that predicate from the source integers by factoring
		10^k=2^k*5^k; its modular-inverse table proof above is an iff test
		for the required power of five.  Hence r!=0, an included endpoint,
		or a nonintegral endpoint all admit q; only an open integral endpoint
		must decrement and continue on the finer grid.  The r==delta branch
		is the symmetric left-boundary statement.  In the final small-grid
		tie, is_integral_mid_point applies the same exact factorization to
		the center, so the selected binary-to-decimal tie policy is consulted
		if and only if the candidate is the exact center.
		*/
		if (r < delta)
		{
			if (r || include_right ||
				!::fast_io::details::is_integral_end_point(
					two_fr, e2, minus_k))
			{
				return {q, minus_k + kappa + 1};
			}
			--q;
			r = big_divisor;
		}
		else if (r == delta)
		{
			auto const x_result{
				::fast_io::details::dragonbox_compute_mul_parity_float64(two_fl, pow10_lo, pow10_hi, beta)};
			if (x_result.parity ||
				(include_left &&
				 ::fast_io::details::is_integral_end_point(
					 two_fl, e2, minus_k)))
			{
				return {q, minus_k + kappa + 1};
			}
		}
		q *= 10;
		::std::uint_least32_t const dist{static_cast<::std::uint_least32_t>(r - delta / 2 + small_divisor_div2)};
		constexpr ::std::uint_least32_t distq_divisor_divisor{100};
		::std::uint_least32_t const dist_q{dist / distq_divisor_divisor};
		::std::uint_least32_t const dist_q_mul100{dist_q * distq_divisor_divisor};
		q += dist_q;
		if (dist == dist_q_mul100)
		{
			bool const approx_y_parity{(dist & 1u) != 0u};
			auto const y_result{
				::fast_io::details::dragonbox_compute_mul_parity_float64(two_fc, pow10_lo, pow10_hi, beta)};
			if (y_result.parity != approx_y_parity ||
				(::fast_io::details::dragonbox_nearest_binary_tie_prefer_down<rounding>(
					 negative, q) &&
				 ::fast_io::details::is_integral_mid_point(
					 two_fc, e2, minus_k)))
			{
				--q;
			}
		}
		return {q, minus_k + kappa};
	}
	else
	{
		::std::uint_least64_t const pow10{compute_pow10_float32[plus_k]};
		::std::uint_least32_t const delta{static_cast<::std::uint_least32_t>(pow10 >> (63 - beta))};
		::std::uint_least32_t const two_fc{static_cast<::std::uint_least32_t>(m2) << 1},
			two_fl{two_fc - 1}, two_fr{two_fc + 1};
		auto const z_result{::fast_io::details::dragonbox_compute_mul_float32(
			static_cast<::std::uint_least64_t>(two_fr) << static_cast<unsigned>(beta), pow10)};
		::std::uint_least32_t q{static_cast<::std::uint_least32_t>(z_result.integer_part / big_divisor)};
		::std::uint_least32_t r{static_cast<::std::uint_least32_t>(z_result.integer_part % big_divisor)};
		/*
		The binary32 cache has the same recovered-endpoint distinction as
		the binary64 cache.  Its exact helpers use the identical
		2^k*5^k divisibility theorem in the 32-bit source domain, so these
		three tests are the representation-width instance of the proof above.
		*/
		if (r < delta)
		{
			if (r || include_right ||
				!::fast_io::details::is_integral_end_point_float32(
					two_fr, e2, minus_k))
			{
				return {q, minus_k + kappa + 1};
			}
			--q;
			r = big_divisor;
		}
		else if (r == delta)
		{
			auto const x_result{
				::fast_io::details::dragonbox_compute_mul_parity_float32(two_fl, pow10, beta)};
			if (x_result.parity ||
				(include_left &&
				 ::fast_io::details::is_integral_end_point_float32(
					 two_fl, e2, minus_k)))
			{
				return {q, minus_k + kappa + 1};
			}
		}
		q *= 10;
		::std::uint_least32_t const dist{static_cast<::std::uint_least32_t>(r - delta / 2 + small_divisor_div2)};
		::std::uint_least32_t const dist_q{dist / small_divisor};
		::std::uint_least32_t const dist_q_mul{dist_q * small_divisor};
		q += dist_q;
		if (dist == dist_q_mul)
		{
			bool const approx_y_parity{((dist ^ small_divisor_div2) & 1u) != 0u};
			auto const y_result{::fast_io::details::dragonbox_compute_mul_parity_float32(two_fc, pow10, beta)};
			if (y_result.parity != approx_y_parity ||
				(::fast_io::details::dragonbox_nearest_binary_tie_prefer_down<rounding>(
					 negative, q) &&
				 ::fast_io::details::is_integral_mid_point_float32(
					 two_fc, e2, minus_k)))
			{
				--q;
			}
		}
		return {q, minus_k + kappa};
	}
}

template <typename flt, bool right_closed>
[[nodiscard]] inline constexpr m10_result<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>
dragonbox_main_directed(typename iec559_traits<flt>::mantissa_type m2, ::std::int_least32_t e2) noexcept
{
	using trait = iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;

	constexpr ::std::size_t mbits{trait::mbits};
	constexpr ::std::size_t ebits{trait::ebits};
	constexpr ::std::uint_least32_t bias{(static_cast<::std::uint_least32_t>(1 << ebits) >> 1) - 1};
	constexpr ::std::int_least32_t exponent_bias{bias + mbits};
	constexpr mantissa_type mflags{static_cast<mantissa_type>(static_cast<mantissa_type>(1) << mbits)};
	constexpr ::std::int_least32_t kappa{::fast_io::details::dragonbox_kappa<flt>};
	constexpr ::std::uint_least32_t big_divisor{kappa == 2 ? 1000 : 100};

	bool shorter_interval{};
	if (e2 == 0) [[unlikely]]
	{
		constexpr ::std::int_least32_t e2bias{1 - static_cast<::std::int_least32_t>(exponent_bias)};
		e2 = e2bias;
	}
	else
	{
		auto const raw_mantissa{m2};
		shorter_interval = right_closed && raw_mantissa == 0 && e2 != 1;
		e2 -= exponent_bias;
		m2 |= mflags;
		::std::uint_least32_t pos_e2{static_cast<::std::uint_least32_t>(-e2)};
		if (pos_e2 < mbits && multiple_of_pow2_unchecked(m2, pos_e2)) [[unlikely]]
		{
			return {static_cast<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>(m2 >> pos_e2), 0};
		}
	}
	::std::uint_least64_t const two_fc{static_cast<::std::uint_least64_t>(m2) << 1u};
	::std::int_least32_t const minus_k{mul_ln2_div_ln10_floor(e2 - static_cast<::std::int_least32_t>(shorter_interval)) -
									   kappa};
	::std::int_least32_t const plus_k{-minus_k};
	::std::int_least32_t const beta{e2 + mul_ln10_div_ln2_floor(plus_k)};
	if constexpr (sizeof(flt) == sizeof(::std::uint_least64_t))
	{
		uint64x2 const pow10{compute_pow10_float64[plus_k]};
		::std::uint_least64_t const pow10_lo{pow10.lo};
		::std::uint_least64_t const pow10_hi{pow10.hi};
		::std::uint_least32_t const delta{static_cast<::std::uint_least32_t>(
			pow10_hi >> static_cast<unsigned>(63 - beta + static_cast<::std::int_least32_t>(shorter_interval)))};
		auto const xi_or_zi{::fast_io::details::dragonbox_compute_mul_float64(
			two_fc << static_cast<unsigned>(beta), pow10_lo, pow10_hi)};
		::std::uint_least64_t q{xi_or_zi.integer_part / big_divisor};
		::std::uint_least32_t r{static_cast<::std::uint_least32_t>(xi_or_zi.integer_part % big_divisor)};
		if constexpr (!right_closed)
		{
			if (!xi_or_zi.is_integer)
			{
				++r;
				if (r == big_divisor)
				{
					r = 0;
					++q;
				}
			}
			if (r)
			{
				++q;
				r = big_divisor - r;
			}
			auto const upper_endpoint{
				::fast_io::details::dragonbox_compute_mul_parity_float64(
					two_fc + 2u, pow10_lo, pow10_hi, beta)};
			/*
			For the left-closed interval [x,next), `r==delta` places the
			large-divisor ceiling exactly at the scaled right endpoint iff the
			endpoint product is integral.  That endpoint is open and must then be
			rejected.  If the product is nonintegral, parity distinguishes the two
			adjacent fixed-point floors.  Consequently equality admits the
			ceiling exactly when both predicates are false:

			    !upper_endpoint.parity && !upper_endpoint.is_integer.

			Testing parity alone incorrectly admitted open integral endpoints,
			producing a coefficient one or two digits too short and forcing the
			post-hoc roundtrip search to repair it.
			*/
			if (r < delta ||
				(r == delta && !upper_endpoint.parity &&
				 !upper_endpoint.is_integer))
			{
				return {q, minus_k + kappa + 1};
			}
			q *= 10u;
			q -= r / 100u;
		}
		else
		{
			if (r < delta ||
				(r == delta &&
				 ::fast_io::details::dragonbox_compute_mul_parity_float64(
					 two_fc - (shorter_interval ? 1u : 2u), pow10_lo, pow10_hi, beta)
					 .parity))
			{
				return {q, minus_k + kappa + 1};
			}
			q *= 10u;
			q += r / 100u;
		}
		return {q, minus_k + kappa};
	}
	else
	{
		::std::uint_least64_t const pow10{compute_pow10_float32[plus_k]};
		::std::uint_least32_t const delta{static_cast<::std::uint_least32_t>(
			pow10 >> static_cast<unsigned>(63 - beta + static_cast<::std::int_least32_t>(shorter_interval)))};
		auto xi_or_zi{::fast_io::details::dragonbox_compute_mul_float32(
			two_fc << static_cast<unsigned>(beta), pow10)};
		if constexpr (!right_closed)
		{
			if (e2 <= -80)
			{
				xi_or_zi.is_integer = false;
			}
		}
		::std::uint_least32_t q{static_cast<::std::uint_least32_t>(xi_or_zi.integer_part / big_divisor)};
		::std::uint_least32_t r{static_cast<::std::uint_least32_t>(xi_or_zi.integer_part % big_divisor)};
		if constexpr (!right_closed)
		{
			if (!xi_or_zi.is_integer)
			{
				++r;
				if (r == big_divisor)
				{
					r = 0;
					++q;
				}
			}
			if (r)
			{
				++q;
				r = big_divisor - r;
			}
			auto const upper_endpoint{
				::fast_io::details::dragonbox_compute_mul_parity_float32(
					static_cast<::std::uint_least32_t>(
						two_fc + 2u),
					pow10, beta)};
			/*
			This is the binary32 instance of the open-endpoint proof above.
			The two exceptional exponents already force x_result nonintegral;
			the endpoint product nevertheless retains an independent integrality
			bit, so it cannot be replaced by its parity bit.
			*/
			if (r < delta ||
				(r == delta && !upper_endpoint.parity &&
				 !upper_endpoint.is_integer))
			{
				return {q, minus_k + kappa + 1};
			}
			q *= 10u;
			q -= r / 10u;
		}
		else
		{
			if (r < delta ||
				(r == delta &&
				 ::fast_io::details::dragonbox_compute_mul_parity_float32(
					 static_cast<::std::uint_least32_t>(two_fc - (shorter_interval ? 1u : 2u)), pow10, beta)
					 .parity))
			{
				return {q, minus_k + kappa + 1};
			}
			q *= 10u;
			q += r / 10u;
		}
		return {q, minus_k + kappa};
	}
}

template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr m10_result<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>
dragonbox_main_policy(typename iec559_traits<flt>::mantissa_type m2, ::std::int_least32_t e2,
					  bool negative) noexcept
{
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		return ::fast_io::details::dragonbox_main_nearest_policy<flt, rounding>(m2, e2, negative);
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::toward_zero)
	{
		return ::fast_io::details::dragonbox_main_directed<flt, false>(m2, e2);
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::away_from_zero)
	{
		return ::fast_io::details::dragonbox_main_directed<flt, true>(m2, e2);
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::toward_plus_infinity)
	{
		return negative ? ::fast_io::details::dragonbox_main_directed<flt, false>(m2, e2)
						: ::fast_io::details::dragonbox_main_directed<flt, true>(m2, e2);
	}
	else
	{
		return negative ? ::fast_io::details::dragonbox_main_directed<flt, true>(m2, e2)
						: ::fast_io::details::dragonbox_main_directed<flt, false>(m2, e2);
	}
}

struct dragonbox_decimal_adjusted_mantissa
{
	::std::uint_least64_t mantissa{};
	::std::int_least32_t power2{};
};

struct dragonbox_decimal_uint128
{
	::std::uint_least64_t low{};
	::std::uint_least64_t high{};
};

template <typename flt>
inline constexpr bool dragonbox_decimal_adjusted_supported{
	(iec559_traits<flt>::mbits <= iec559_traits<float>::mbits &&
	 iec559_traits<flt>::ebits <= iec559_traits<float>::ebits) ||
	sizeof(flt) == sizeof(::std::uint_least64_t)};

[[nodiscard]] inline constexpr ::std::int_least32_t
dragonbox_decimal_binary_power(::std::int_least32_t exponent) noexcept
{
	return static_cast<::std::int_least32_t>((((152170 + 65536) * exponent) >> 16) + 63);
}

[[nodiscard]] inline constexpr dragonbox_decimal_uint128
dragonbox_decimal_mul_64x128_high(::std::uint_least64_t value, ::fast_io::details::uint64x2 cache) noexcept
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

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr ::std::uint_least64_t
dragonbox_decimal_round_mantissa(bool negative, ::std::uint_least64_t mantissa, bool has_tail, bool is_tie) noexcept
{
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		if ((mantissa & 1u) != 0u)
		{
			if (!is_tie || ::fast_io::details::floating_rounding_nearest_tie_round_up<rounding>(negative, mantissa))
			{
				++mantissa;
			}
		}
	}
	else
	{
		if (::fast_io::details::floating_rounding_directed_round_up<rounding>(negative))
		{
			if ((mantissa & 1u) != 0u)
			{
				++mantissa;
			}
			else if (has_tail)
			{
				mantissa += ::std::uint_least64_t{2u};
			}
		}
	}
	return mantissa >> 1u;
}

template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool
dragonbox_decimal_compute_adjusted(::std::int_least64_t exponent, ::std::uint_least64_t significand,
								   bool negative,
								   dragonbox_decimal_adjusted_mantissa &answer) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	if constexpr (!::fast_io::details::dragonbox_decimal_adjusted_supported<flt>)
	{
		return false;
	}
	else
	{
		constexpr bool use_binary32_bounds{
			trait::mbits <= ::fast_io::details::iec559_traits<float>::mbits &&
			trait::ebits <= ::fast_io::details::iec559_traits<float>::ebits};
		constexpr auto mantissa_explicit_bits{static_cast<::std::int_least32_t>(trait::mbits)};
		constexpr auto minimum_exponent{
			-static_cast<::std::int_least32_t>((static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u)};
		constexpr auto infinite_power{
			static_cast<::std::int_least32_t>((static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u)};
		constexpr auto smallest_power10{use_binary32_bounds ? -65 : -342};
		constexpr auto largest_power10{use_binary32_bounds ? 38 : 308};
		constexpr auto min_round_to_even_power10{use_binary32_bounds ? -17 : -4};
		constexpr auto max_round_to_even_power10{use_binary32_bounds ? 10 : 23};
		constexpr auto max_finite_mantissa{(::std::uint_least64_t{1u} << mantissa_explicit_bits) - 1u};
		if (significand == 0)
		{
			answer = {};
			return true;
		}
		if (exponent < smallest_power10)
		{
			if constexpr (!::fast_io::details::floating_rounding_is_nearest<rounding>)
			{
				if (::fast_io::details::floating_rounding_directed_round_up<rounding>(negative))
				{
					answer = {.mantissa = 1u, .power2 = 0};
					return true;
				}
			}
			answer = {};
			return true;
		}
		if (exponent > largest_power10)
		{
			if constexpr (!::fast_io::details::floating_rounding_is_nearest<rounding>)
			{
				if (!::fast_io::details::floating_rounding_directed_round_up<rounding>(negative))
				{
					answer = {.mantissa = max_finite_mantissa, .power2 = infinite_power - 1};
					return true;
				}
			}
			answer = {.mantissa = 0, .power2 = infinite_power};
			return true;
		}
		if (exponent < -342 || exponent > 326)
		{
			return false;
		}
		auto const exponent32{static_cast<::std::int_least32_t>(exponent)};
		auto const leading_zeroes{static_cast<::std::int_least32_t>(::std::countl_zero(significand))};
		significand <<= static_cast<unsigned>(leading_zeroes);
		auto const cache{::fast_io::details::compute_pow10_float64_scan(exponent32)};
		auto const product{::fast_io::details::dragonbox_decimal_mul_64x128_high(significand, cache)};
		auto const upperbit{static_cast<::std::int_least32_t>(product.high >> 63u)};
		auto const shift{upperbit + 64 - mantissa_explicit_bits - 3};
		auto mantissa{product.high >> static_cast<unsigned>(shift)};
		auto power2{static_cast<::std::int_least32_t>(
			::fast_io::details::dragonbox_decimal_binary_power(exponent32) + upperbit - leading_zeroes - minimum_exponent)};
		if (power2 <= 0)
		{
			if (-power2 + 1 >= 64)
			{
				answer = {};
				return true;
			}
			auto const subnormal_shift{static_cast<unsigned>(-power2 + 1)};
			auto const subnormal_tail_mask{(::std::uint_least64_t{1u} << subnormal_shift) - 1u};
			bool const has_tail{(mantissa & subnormal_tail_mask) != 0u || product.low != 0u};
			mantissa >>= subnormal_shift;
			bool const is_tie{!has_tail && (mantissa & 1u) != 0u};
			mantissa = ::fast_io::details::dragonbox_decimal_round_mantissa<rounding>(
				negative, mantissa, has_tail, is_tie);
			answer.power2 = mantissa < (::std::uint_least64_t{1} << mantissa_explicit_bits) ? 0 : 1;
			answer.mantissa = mantissa;
			return true;
		}
		auto const shifted_back{mantissa << static_cast<unsigned>(shift)};
		bool const is_tie{product.low <= 1 && min_round_to_even_power10 <= exponent &&
						  exponent <= max_round_to_even_power10 && (mantissa & 1u) != 0u &&
						  shifted_back == product.high};
		bool const has_tail{shifted_back != product.high || product.low != 0u};
		mantissa = ::fast_io::details::dragonbox_decimal_round_mantissa<rounding>(
			negative, mantissa, has_tail, is_tie);
		if (mantissa >= (::std::uint_least64_t{2} << mantissa_explicit_bits))
		{
			mantissa = ::std::uint_least64_t{1} << mantissa_explicit_bits;
			++power2;
		}
		mantissa &= ~(::std::uint_least64_t{1} << mantissa_explicit_bits);
		if (power2 >= infinite_power)
		{
			if constexpr (!::fast_io::details::floating_rounding_is_nearest<rounding>)
			{
				if (!::fast_io::details::floating_rounding_directed_round_up<rounding>(negative))
				{
					answer = {.mantissa = max_finite_mantissa, .power2 = infinite_power - 1};
					return true;
				}
			}
			answer = {.mantissa = 0, .power2 = infinite_power};
			return true;
		}
		answer = {.mantissa = mantissa, .power2 = power2};
		return true;
	}
}

template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool dragonbox_decimal_roundtrips_to(
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> decimal_mantissa,
	::std::int_least32_t decimal_exponent,
	typename iec559_traits<flt>::mantissa_type binary_mantissa, ::std::int_least32_t binary_exponent,
	bool negative) noexcept
{
	dragonbox_decimal_adjusted_mantissa adjusted;
	if (!::fast_io::details::dragonbox_decimal_compute_adjusted<flt, rounding>(
			decimal_exponent, decimal_mantissa, negative, adjusted))
	{
		return false;
	}
	return adjusted.mantissa == binary_mantissa && adjusted.power2 == binary_exponent;
}

template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool dragonbox_decimal_printable_roundtrips_to(
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> decimal_mantissa,
	::std::int_least32_t decimal_exponent,
	typename iec559_traits<flt>::mantissa_type binary_mantissa, ::std::int_least32_t binary_exponent,
	bool negative) noexcept
{
	::std::uint_least64_t decimal_mantissa_limit{1u};
	for (::std::uint_least32_t i{}; i != ::fast_io::details::iec559_traits<flt>::m10digits; ++i)
	{
		decimal_mantissa_limit *= 10u;
	}
	return decimal_mantissa &&
		   static_cast<::std::uint_least64_t>(decimal_mantissa) < decimal_mantissa_limit &&
		   ::fast_io::details::dragonbox_decimal_roundtrips_to<flt, rounding>(
			   decimal_mantissa, decimal_exponent, binary_mantissa, binary_exponent, negative);
}

template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool dragonbox_correct_shortest_roundtrip_extend(
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> base, ::std::int_least32_t exponent,
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> &m10, ::std::int_least32_t &e10,
	typename iec559_traits<flt>::mantissa_type m2, ::std::int_least32_t e2, bool negative) noexcept
{
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> add_limit{1u};
	for (::std::uint_least32_t extension{}; extension != 3u; ++extension)
	{
		auto const next_base{
			static_cast<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>(base * 10u)};
		if (next_base / 10u != base)
		{
			break;
		}
		base = next_base;
		add_limit = static_cast<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>(add_limit * 10u);
		--exponent;
		for (::fast_io::details::dragonbox_decimal_mantissa_type<flt> add{}; add != add_limit; ++add)
		{
			auto const extended{
				static_cast<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>(base + add)};
			if (::fast_io::details::dragonbox_decimal_printable_roundtrips_to<flt, rounding>(
					extended, exponent, m2, e2, negative))
			{
				m10 = extended;
				e10 = exponent;
				return true;
			}
		}
		for (::fast_io::details::dragonbox_decimal_mantissa_type<flt> sub{1u}; sub != add_limit && sub <= base; ++sub)
		{
			auto const extended{
				static_cast<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>(base - sub)};
			if (::fast_io::details::dragonbox_decimal_printable_roundtrips_to<flt, rounding>(
					extended, exponent, m2, e2, negative))
			{
				m10 = extended;
				e10 = exponent;
				return true;
			}
		}
	}
	return false;
}

template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
inline constexpr void dragonbox_correct_shortest_roundtrip(
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> &m10,
	::std::int_least32_t &e10,
	typename iec559_traits<flt>::mantissa_type m2,
	::std::int_least32_t e2, bool negative) noexcept
{
	if constexpr (::fast_io::details::dragonbox_decimal_adjusted_supported<flt>)
	{
		if (::fast_io::details::dragonbox_decimal_printable_roundtrips_to<flt, rounding>(m10, e10, m2, e2, negative))
		{
			return;
		}
		auto const next{static_cast<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>(m10 + 1u)};
		if (::fast_io::details::dragonbox_decimal_printable_roundtrips_to<flt, rounding>(next, e10, m2, e2, negative))
		{
			m10 = next;
			return;
		}
		if (m10)
		{
			auto const previous{static_cast<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>(m10 - 1u)};
			if (::fast_io::details::dragonbox_decimal_printable_roundtrips_to<flt, rounding>(
					previous, e10, m2, e2, negative))
			{
				m10 = previous;
				return;
			}
		}
		auto const nearest{::fast_io::details::da::trim_trailing_zeros(
			::fast_io::details::dragonbox_main<flt>(m2, e2))};
		auto const nearest_v{nearest.m10};
		auto const nearest_e10{nearest.e10};
		if (::fast_io::details::dragonbox_decimal_printable_roundtrips_to<flt, rounding>(
				nearest_v, nearest_e10, m2, e2, negative))
		{
			m10 = nearest_v;
			e10 = nearest_e10;
			return;
		}
		auto const nearest_next{
			static_cast<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>(nearest_v + 1u)};
		if (::fast_io::details::dragonbox_decimal_printable_roundtrips_to<flt, rounding>(
				nearest_next, nearest_e10, m2, e2, negative))
		{
			m10 = nearest_next;
			e10 = nearest_e10;
			return;
		}
		if (nearest_v)
		{
			auto const nearest_previous{
				static_cast<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>(nearest_v - 1u)};
			if (::fast_io::details::dragonbox_decimal_printable_roundtrips_to<flt, rounding>(
					nearest_previous, nearest_e10, m2, e2, negative))
			{
				m10 = nearest_previous;
				e10 = nearest_e10;
				return;
			}
		}
		if (::fast_io::details::dragonbox_correct_shortest_roundtrip_extend<flt, rounding>(
				nearest_v, nearest_e10, m10, e10, m2, e2, negative))
		{
			return;
		}
		(void)::fast_io::details::dragonbox_correct_shortest_roundtrip_extend<flt, rounding>(
			m10, e10, m10, e10, m2, e2, negative);
	}
}

// This policy wrapper, like the kernel above, reads only its arguments and
// immutable generated tables; correction changes local carriers only.  Thus
// `pure` is valid for every rounding-policy instantiation.  `hot` describes its
// role as the common finite shortest entry rather than a measured per-ISA
// threshold.  Both attributes are optional optimizer contracts: without them,
// returned digits, exponent and rounding-boundary decisions are unchanged.
template <typename flt, ::fast_io::manipulators::floating_rounding rounding =
							::fast_io::manipulators::floating_rounding::nearest_to_even>
#if __has_cpp_attribute(__gnu__::__pure__)
[[__gnu__::__pure__]]
#endif
#if __has_cpp_attribute(__gnu__::__hot__)
[[__gnu__::__hot__]]
#endif
inline constexpr m10_result<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>
dragonbox_impl(typename iec559_traits<flt>::mantissa_type m2, ::std::int_least32_t e2, bool negative) noexcept
{
	constexpr bool da_supported{
		(::fast_io::details::iec559_traits<flt>::mbits == 23u &&
		 ::fast_io::details::iec559_traits<flt>::ebits == 8u) ||
		(::fast_io::details::iec559_traits<flt>::mbits == 52u &&
		 ::fast_io::details::iec559_traits<flt>::ebits == 11u)};
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even && da_supported)
	{
		auto const result{::fast_io::details::da::trim_trailing_zeros(
			::fast_io::details::da::to_decimal<flt>(m2, e2))};
		return {result.m10, result.e10};
	}
	auto [m10, e10] =
		[]([[maybe_unused]] typename iec559_traits<flt>::mantissa_type mantissa,
		   [[maybe_unused]] ::std::int_least32_t exponent,
		   [[maybe_unused]] bool sign) constexpr noexcept {
			if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
			{
				return dragonbox_main<flt>(mantissa, exponent);
			}
			else
			{
				return ::fast_io::details::dragonbox_main_policy<flt, rounding>(mantissa, exponent, sign);
			}
		}(m2, e2, negative);
	// m10 should not ==0
	auto trimmed{::fast_io::details::da::trim_trailing_zeros(
		::fast_io::details::m10_result<decltype(m10)>{m10, e10})};
	if constexpr (
		!da_supported &&
		(rounding !=
			 ::fast_io::manipulators::floating_rounding::
				 nearest_to_even ||
		 (::fast_io::details::dragonbox_uses_binary32_core<flt> &&
		  sizeof(flt) < sizeof(float))))
	{
		/*
		A narrow format deliberately runs a binary32 arithmetic core although
		its parsing interval belongs to the original smaller lattice.  Its raw
		carrier therefore needs the interval membership correction below.

		Binary32 and binary64 do not: dragonbox_main_nearest_policy constructs
		their exact midpoint interval, while dragonbox_main_directed constructs
		[x,next) or (prev,x] directly.  Nearest-policy endpoint and center
		equalities are decided by the exact 2^k*5^k divisibility predicates
		above, independently of recovered-cache residue bits; the directed
		left-closed equality branch likewise rejects its open integral endpoint
		with the proved parity/integrality conjunction.  Hence the returned
		carrier is already a member of the exact source interval and has been
		obtained with the larger divisor whenever that grid was nonempty; it is
		shortest by construction.  Rechecking it with a decimal-to-binary
		roundtrip search cannot change the result and previously added roughly
		3--7 ns/value on M4.  The `da_supported` gate removes that redundant
		search only for the two representations whose interval was constructed
		directly.
		*/
		::fast_io::details::dragonbox_correct_shortest_roundtrip<flt, rounding>(
			trimmed.m10, trimmed.e10, m2, e2, negative);
		return ::fast_io::details::da::trim_trailing_zeros(trimmed);
	}
	else
	{
		return trimmed;
	}
}

template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
inline constexpr void dragonbox_shorten_decimal_to_target(
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> &m10, ::std::int_least32_t &e10,
	typename iec559_traits<flt>::mantissa_type m2, ::std::int_least32_t e2, bool negative) noexcept
{
	using decimal_type = ::fast_io::details::dragonbox_decimal_mantissa_type<flt>;
	if (!m10)
	{
		return;
	}
	auto const len{static_cast<::std::uint_least32_t>(chars_len<10, true>(m10))};
	constexpr auto max_digits{::fast_io::details::iec559_traits<flt>::m10digits};
	auto const max_candidate_digits{len < max_digits ? len : max_digits};
	for (::std::uint_least32_t desired{1u}; desired <= max_candidate_digits; ++desired)
	{
		auto const cut{static_cast<::std::uint_least32_t>(len - desired)};
		if (19u < cut)
		{
			continue;
		}
		::std::uint_least64_t divisor{1u};
		for (::std::uint_least32_t i{}; i != cut; ++i)
		{
			divisor *= 10u;
		}
		auto const quotient{static_cast<decimal_type>(m10 / divisor)};
		auto const remainder{static_cast<::std::uint_least64_t>(m10 - static_cast<decimal_type>(quotient * divisor))};
		auto const candidate_e10{static_cast<::std::int_least32_t>(e10 + static_cast<::std::int_least32_t>(cut))};
		auto try_candidate = [&](decimal_type candidate) constexpr noexcept {
			if (!candidate)
			{
				return false;
			}
			if (::fast_io::details::dragonbox_decimal_printable_roundtrips_to<flt, rounding>(
					candidate, candidate_e10, m2, e2, negative))
			{
				m10 = candidate;
				e10 = candidate_e10;
				auto const trimmed{::fast_io::details::da::trim_trailing_zeros(
					::fast_io::details::m10_result<decimal_type>{m10, e10})};
				m10 = trimmed.m10;
				e10 = trimmed.e10;
				return true;
			}
			return false;
		};
		auto const round_up{remainder && (divisor - remainder < remainder)};
		auto const lower{quotient};
		auto const upper{static_cast<decimal_type>(quotient + static_cast<decimal_type>(remainder != 0u))};
		if (round_up)
		{
			if (try_candidate(upper) || try_candidate(lower))
			{
				return;
			}
			if (try_candidate(static_cast<decimal_type>(upper + 1u)))
			{
				return;
			}
			if (lower && try_candidate(static_cast<decimal_type>(lower - 1u)))
			{
				return;
			}
		}
		else
		{
			if (try_candidate(lower) || try_candidate(upper))
			{
				return;
			}
			if (lower && try_candidate(static_cast<decimal_type>(lower - 1u)))
			{
				return;
			}
			if (try_candidate(static_cast<decimal_type>(upper + 1u)))
			{
				return;
			}
		}
	}
}

template <::std::size_t n>
struct dragonbox_bfloat16_high_fallback_table
{
	::std::uint_least16_t values[n]{};
};

inline constexpr ::std::int_least32_t dragonbox_bfloat16_high_fallback_min_exponent{244};
inline constexpr ::std::int_least32_t dragonbox_bfloat16_high_fallback_max_exponent{254};
inline constexpr ::std::uint_least32_t dragonbox_bfloat16_high_fallback_exponent_count{
	static_cast<::std::uint_least32_t>(dragonbox_bfloat16_high_fallback_max_exponent -
									   dragonbox_bfloat16_high_fallback_min_exponent + 1)};
inline constexpr ::std::uint_least32_t dragonbox_bfloat16_mantissa_count{128u};
inline constexpr ::std::int_least32_t dragonbox_bfloat16_high_fallback_e10_bias{33};
inline constexpr ::std::uint_least32_t dragonbox_bfloat16_high_fallback_m10_mask{0x0FFFu};

template <typename flt,
	::fast_io::manipulators::floating_rounding rounding,
	::std::int_least32_t exponent>
[[nodiscard]] inline constexpr dragonbox_bfloat16_high_fallback_table<
	dragonbox_bfloat16_mantissa_count>
dragonbox_make_bfloat16_high_fallback_page() noexcept
{
	static_assert(
		dragonbox_bfloat16_high_fallback_min_exponent <= exponent &&
		exponent <= dragonbox_bfloat16_high_fallback_max_exponent);
	dragonbox_bfloat16_high_fallback_table<
		dragonbox_bfloat16_mantissa_count> page;
	for (::std::uint_least32_t mantissa{};
		 mantissa != dragonbox_bfloat16_mantissa_count; ++mantissa)
	{
		auto [m10, e10] = ::fast_io::details::dragonbox_impl<
			float, rounding>(
			static_cast<::fast_io::details::iec559_traits<
				float>::mantissa_type>(mantissa << 16u),
			exponent, false);
		::fast_io::details::dragonbox_shorten_decimal_to_target<
			flt, rounding>(
			m10, e10,
			static_cast<typename ::fast_io::details::
				iec559_traits<flt>::mantissa_type>(mantissa),
			exponent, false);
		/*
		Exhaustive construction over the six canonical unsigned interval
		classes proves m10<2^12 and 33<=e10<=38 for every entry in this
		high-bfloat16 band.  The two fields are therefore disjoint in the
		16-bit packing below.  A policy outside those classes is never
		instantiated here; signed ±infinity policies select one canonical
		class before lookup.
		*/
		page.values[mantissa] =
			static_cast<::std::uint_least16_t>(
				(static_cast<::std::uint_least32_t>(
					 e10 -
					 dragonbox_bfloat16_high_fallback_e10_bias)
				 << 12u) |
				m10);
	}
	return page;
}

/*
Each exponent page is a separate constexpr variable.  The numeric result is
identical to one nested 11x128 loop, but the split resets the compiler's
constant-evaluation step budget after 128 exact shortening proofs.  The final
table assembly below performs only 1,408 integer copies and emits one compact
contiguous object; it creates neither run-time indirection nor eleven linked
tables.
*/
template <typename flt,
	::fast_io::manipulators::floating_rounding rounding,
	::std::int_least32_t exponent>
inline constexpr auto dragonbox_bfloat16_high_fallback_page_cache{
	::fast_io::details::
		dragonbox_make_bfloat16_high_fallback_page<
			flt, rounding, exponent>()};

template <typename flt,
	::fast_io::manipulators::floating_rounding rounding,
	::std::size_t... offsets>
[[nodiscard]] inline constexpr dragonbox_bfloat16_high_fallback_table<
	dragonbox_bfloat16_high_fallback_exponent_count *
	dragonbox_bfloat16_mantissa_count>
dragonbox_make_bfloat16_high_fallback_table(
	::std::index_sequence<offsets...>) noexcept
{
	dragonbox_bfloat16_high_fallback_table<
		dragonbox_bfloat16_high_fallback_exponent_count *
		dragonbox_bfloat16_mantissa_count> table;
	auto const copy_page =
		[&]<::std::size_t offset>(
			::std::integral_constant<::std::size_t, offset>) constexpr
	{
		auto const &page{
			::fast_io::details::
				dragonbox_bfloat16_high_fallback_page_cache<
					flt, rounding,
					dragonbox_bfloat16_high_fallback_min_exponent +
						static_cast<::std::int_least32_t>(
							offset)>};
		for (::std::size_t index{};
			 index != dragonbox_bfloat16_mantissa_count; ++index)
		{
			table.values[
				offset * dragonbox_bfloat16_mantissa_count +
				index] = page.values[index];
		}
	};
	(copy_page(
		 ::std::integral_constant<::std::size_t, offsets>{}),
	 ...);
	return table;
}

template <typename flt,
	::fast_io::manipulators::floating_rounding rounding>
inline constexpr auto dragonbox_bfloat16_high_fallback_table_cache{
	::fast_io::details::
		dragonbox_make_bfloat16_high_fallback_table<flt, rounding>(
			::std::make_index_sequence<
				dragonbox_bfloat16_high_fallback_exponent_count>{})};

template <typename flt,
	::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr m10_result<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>
dragonbox_bfloat16_high_fallback(
	typename iec559_traits<flt>::mantissa_type m2, ::std::int_least32_t e2) noexcept
{
	using decimal_type = ::fast_io::details::dragonbox_decimal_mantissa_type<flt>;
	auto const packed{
		::fast_io::details::
			dragonbox_bfloat16_high_fallback_table_cache<
				flt, rounding>.values
			[static_cast<::std::uint_least32_t>(e2 - dragonbox_bfloat16_high_fallback_min_exponent) *
				 dragonbox_bfloat16_mantissa_count +
			 static_cast<::std::uint_least32_t>(m2)]};
	return {static_cast<decimal_type>(packed & dragonbox_bfloat16_high_fallback_m10_mask),
			static_cast<::std::int_least32_t>(dragonbox_bfloat16_high_fallback_e10_bias +
											  static_cast<::std::int_least32_t>(packed >> 12u))};
}

static_assert(
	sizeof(dragonbox_bfloat16_high_fallback_table<
		dragonbox_bfloat16_high_fallback_exponent_count *
		dragonbox_bfloat16_mantissa_count>) ==
	2816u);

template <typename flt>
[[nodiscard]] inline constexpr punning_result<float> dragonbox_narrow_float_punned(
	typename iec559_traits<flt>::mantissa_type m2, ::std::int_least32_t e2,
	bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	if constexpr (trait::mbits == 7u && trait::ebits == 8u)
	{
		return {static_cast<::fast_io::details::iec559_traits<float>::mantissa_type>(
					static_cast<::std::uint_least32_t>(m2) << 16u),
				static_cast<::std::uint_least32_t>(e2), negative};
	}
	else if constexpr (trait::mbits == 10u && trait::ebits == 5u)
	{
		if (e2 != 0)
		{
			return {static_cast<::fast_io::details::iec559_traits<float>::mantissa_type>(
						static_cast<::std::uint_least32_t>(m2) << 13u),
					static_cast<::std::uint_least32_t>(e2 + 112), negative};
		}
		if (m2)
		{
			// A binary16 subnormal is m2 * 2^-24.  If k is the index of
			// its leading bit, its normalized binary32 exponent is k-24;
			// biasing by 127 gives k+103.  Shifting that leading bit to
			// binary32's implicit position constructs the exact mantissa,
			// without executing a floating conversion instruction.
			// The enclosing nonzero test proves `bit_width` is at least one. Convert before subtracting so the bit index
			// remains in the unsigned representation domain without an implicit sign change.
			auto const leading_bit{static_cast<::std::uint_least32_t>(
				::std::bit_width(static_cast<::std::uint_least32_t>(m2))) - 1u};
			constexpr ::std::uint_least32_t float_mantissa_mask{0x7FFFFFu};
			return {
				(static_cast<::std::uint_least32_t>(m2) << (23u - leading_bit)) &
					float_mantissa_mask,
				leading_bit + 103u, negative};
		}
		return {0u, 0u, negative};
	}
	else
	{
		static_assert(trait::mbits == 7u || trait::mbits == 10u,
			"narrow binary32 widening requires the binary16 or bfloat16 field layout");
		return {0u, 0u, negative};
	}
}

template <typename flt>
[[nodiscard]] inline constexpr float dragonbox_narrow_float_from_fields(
	typename iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
	auto const widened{
		::fast_io::details::dragonbox_narrow_float_punned<flt>(
			mantissa, static_cast<::std::int_least32_t>(exponent), negative)};
	constexpr ::std::uint_least32_t float_sign_shift{31u};
	constexpr ::std::uint_least32_t float_exponent_shift{23u};
	auto const raw{
		(static_cast<::std::uint_least32_t>(widened.sign) << float_sign_shift) |
		(widened.exponent << float_exponent_shift) |
		static_cast<::std::uint_least32_t>(widened.mantissa)};
	// The field construction above is an exact representation mapping, so the
	// final operation is a bit copy rather than a floating conversion.  This is
	// essential for bfloat16 subnormals: a target narrowing instruction is
	// permitted to flush the intermediate binary32 subnormal before a later
	// widening could recover the original value.
	return ::fast_io::bit_cast<float>(raw);
}

/**
 * @brief Maps one already-classified finite IEEE binary32 value to the exactly
 *        equal IEEE binary64 value without executing a floating conversion.
 *
 * The caller must reject raw exponent 255 before entering this function.  That
 * ordering is part of the contract: an sNaN is formatted from its original
 * binary32 fields and can never be quieted, raise FE_INVALID, or lose payload
 * bits in an FCVT.  Integer construction also keeps binary32 subnormals exact
 * when a target's DAZ/FTZ state would make `static_cast<double>(float)` unsafe.
 *
 * For a normal source, the binary32 value is
 *
 *   (2^23 + M) * 2^(E - 150).
 *
 * The constructed binary64 significand is `(2^23 + M) * 2^29` and its biased
 * exponent is `E + 896`, which denotes the same product exactly.  For a
 * subnormal, let H=floor(log2(M)).  The constructed significand is
 * `M * 2^(52-H)` and biased exponent `H+874`, hence its value is exactly
 * `M * 2^-149`.  Zero retains only the original sign bit.  These three cases
 * exhaust every finite binary32 encoding.
 *
 * The returned floating scalar remains by value so the established binary64
 * low-level ABI continues to use a floating register where the target ABI does.
 */
[[nodiscard]] inline constexpr double
dragonbox_binary32_finite_fields_to_binary64(
	::std::uint_least32_t mantissa, ::std::uint_least32_t exponent,
	bool negative) noexcept
{
	static_assert(::fast_io::details::iec559_traits<float>::mbits == 23u &&
		::fast_io::details::iec559_traits<float>::ebits == 8u);
	static_assert(::fast_io::details::iec559_traits<double>::mbits == 52u &&
		::fast_io::details::iec559_traits<double>::ebits == 11u);
	::std::uint_least64_t raw{
		static_cast<::std::uint_least64_t>(negative) << 63u};
	if (exponent)
	{
		raw |= static_cast<::std::uint_least64_t>(exponent + 896u) << 52u;
		raw |= static_cast<::std::uint_least64_t>(mantissa) << 29u;
	}
	else if (mantissa)
	{
		// The branch proves a nonzero 23-bit mantissa, so `bit_width` is in [1, 23]. Convert that proved positive
		// result before unsigned subtraction instead of relying on an implementation warning-prone signed conversion.
		auto const leading_bit{static_cast<::std::uint_least32_t>(
			::std::bit_width(mantissa)) - 1u};
		raw |= static_cast<::std::uint_least64_t>(leading_bit + 874u) << 52u;
		raw |= static_cast<::std::uint_least64_t>(
			mantissa ^ (static_cast<::std::uint_least32_t>(1u) << leading_bit))
			<< (52u - leading_bit);
	}
	return ::fast_io::bit_cast<double>(raw);
}

// The hybrid narrow path calls this widening-and-shortening repair only after a
// direct narrow candidate cannot be accepted.  `cold` records that fallback
// topology rather than a measured frequency for every target, while `noinline`
// keeps the widened float carrier and round-trip repair state out of the common
// narrow caller.  If either attribute is unavailable, the same conversion and
// target-format shortening are performed; only placement and live ranges may
// differ.
template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr m10_result<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>
dragonbox_impl_narrow_from_float(typename iec559_traits<flt>::mantissa_type m2,
								 ::std::int_least32_t e2, bool negative) noexcept
{
	auto [float_mantissa, float_exponent, float_sign] =
		::fast_io::details::dragonbox_narrow_float_punned<flt>(m2, e2, negative);
	auto [m10, e10] = ::fast_io::details::dragonbox_impl<float, rounding>(
		float_mantissa, static_cast<::std::int_least32_t>(float_exponent), float_sign);
	::fast_io::details::dragonbox_shorten_decimal_to_target<flt, rounding>(m10, e10, m2, e2, negative);
	return {m10, e10};
}

template <typename flt>
[[nodiscard]] inline constexpr bool dragonbox_narrow_raw_candidate_needs_fallback(
	typename iec559_traits<flt>::mantissa_type m2, ::std::int_least32_t e2) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	if constexpr (trait::mbits == 10u && trait::ebits == 5u)
	{
		return m2 == 0u && (e2 == 8 || e2 == 9);
	}
	else if constexpr (trait::mbits == 7u && trait::ebits == 8u)
	{
		if (244 <= e2)
		{
			return true;
		}
		if (m2 != 0u)
		{
			return false;
		}
		switch (e2)
		{
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
		case 14:
		case 21:
		case 44:
		case 48:
		case 49:
		case 50:
		case 51:
		case 62:
		case 63:
		case 64:
		case 68:
		case 92:
		case 93:
		case 94:
		case 107:
		case 108:
		case 109:
		case 110:
		case 136:
		case 137:
		case 157:
		case 164:
		case 187:
		case 188:
		case 189:
		case 190:
		case 191:
		case 211:
		case 223:
		case 224:
			return true;
		default:
			return false;
		}
	}
	else
	{
		return true;
	}
}

/*
Canonical narrow-policy exception theorem
=========================================

The signless parsing interval has only six forms: nearest-even, nearest-odd,
nearest-toward-zero, nearest-away, directed [x,next), and directed (prev,x].
Nearest toward +infinity selects nearest-away for a positive source and
nearest-toward-zero after sign reflection; nearest toward -infinity selects the
opposite pair.  Directed +/- infinity analogously select away/toward-zero.
Thus all ten public policies reduce to these six unsigned interval classes.

Exhaustive integer evaluation of every finite 16-bit magnitude proves that the
raw narrow Dragonbox carrier fails membership only when the stored fraction is
zero and the biased exponent belongs to one of the sets below.  No arbitrary
mantissa is exceptional.  The switch is therefore both smaller and faster than
a 65,536-bit policy bitmap, while expressing the mathematical cause: only an
asymmetric binade boundary can expose the binary32-core/narrow-lattice
difference.  A listed exponent uses an exact narrow-lattice witness; every
other carrier is already in the target interval and is shortest because
Dragonbox tried the coarser decimal grid first.
*/
inline constexpr ::std::size_t dragonbox_bfloat16_low_exception_table_extent{
	225u};
inline constexpr ::std::int_least32_t
	dragonbox_bfloat16_low_exception_e10_bias{45};
inline constexpr ::std::uint_least64_t
	dragonbox_bfloat16_low_exception_common_masks[]{
		0xc00f100000207f80ULL,
		0x0000780070000011ULL,
		0xf800001020000000ULL,
		0x0000000180080000ULL};

struct dragonbox_bfloat16_low_exception_table
{
	::std::uint_least32_t values[
		dragonbox_bfloat16_low_exception_table_extent]{};
};

[[nodiscard]] inline constexpr dragonbox_bfloat16_low_exception_table
dragonbox_make_bfloat16_low_exception_table() noexcept
{
	dragonbox_bfloat16_low_exception_table table;
	auto const set = [&](::std::size_t e2, ::std::uint_least32_t m10,
						 ::std::int_least32_t e10) constexpr
	{
		/*
		m10<2^16 for every proof witness and -45<=e10<=27.  Biasing e10
		makes both fields unsigned and disjoint; the encoding consequently
		depends on neither signed shifts nor a machine character set.
		*/
		table.values[e2] =
			(static_cast<::std::uint_least32_t>(
				 e10 +
				 dragonbox_bfloat16_low_exception_e10_bias)
			 << 16u) |
			m10;
	};
	set(7u, 752u, -39);
	set(8u, 151u, -38);
	set(9u, 301u, -38);
	set(10u, 602u, -38);
	set(11u, 1204u, -38);
	set(12u, 241u, -37);
	set(13u, 481u, -37);
	set(14u, 963u, -37);
	set(21u, 1233u, -35);
	set(44u, 1034u, -28);
	set(48u, 166u, -26);
	set(49u, 331u, -26);
	set(50u, 662u, -26);
	set(51u, 1323u, -26);
	set(62u, 271u, -22);
	set(63u, 542u, -22);
	set(64u, 1084u, -22);
	set(68u, 174u, -20);
	set(92u, 291u, -13);
	set(93u, 582u, -13);
	set(94u, 1164u, -13);
	set(107u, 954u, -9);
	set(108u, 191u, -8);
	set(109u, 381u, -8);
	set(110u, 763u, -8);
	set(136u, 512u, 0);
	set(137u, 1024u, 0);
	set(157u, 1074u, 6);
	set(164u, 1374u, 8);
	set(187u, 1153u, 15);
	set(188u, 231u, 16);
	set(189u, 461u, 16);
	set(190u, 922u, 16);
	set(191u, 185u, 17);
	set(211u, 194u, 23);
	set(223u, 792u, 26);
	set(224u, 159u, 27);
	return table;
}

inline constexpr auto dragonbox_bfloat16_low_exception_table_cache{
	::fast_io::details::dragonbox_make_bfloat16_low_exception_table()};

static_assert(
	sizeof(dragonbox_bfloat16_low_exception_table) ==
	dragonbox_bfloat16_low_exception_table_extent *
		sizeof(::std::uint_least32_t));
static_assert(
	sizeof(dragonbox_bfloat16_low_exception_table) <= 1024u &&
	sizeof(dragonbox_bfloat16_low_exception_table) <= 100u * 1024u);
static_assert(
	sizeof(dragonbox_bfloat16_low_exception_common_masks) ==
	4u * sizeof(::std::uint_least64_t));

template <typename flt,
	::fast_io::manipulators::floating_rounding canonical_rounding>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
[[nodiscard]] inline constexpr bool
dragonbox_narrow_canonical_raw_candidate_needs_fallback(
	typename iec559_traits<flt>::mantissa_type m2,
	::std::int_least32_t e2) noexcept
{
	if (m2 != 0u)
	{
		return false;
	}
	using trait = ::fast_io::details::iec559_traits<flt>;
	if constexpr (trait::mbits == 10u && trait::ebits == 5u)
	{
		if constexpr (
			canonical_rounding ==
				::fast_io::manipulators::floating_rounding::
					nearest_to_odd)
		{
			return e2 == 8 || e2 == 28 || e2 == 29;
		}
		else if constexpr (
			canonical_rounding ==
				::fast_io::manipulators::floating_rounding::
					nearest_toward_zero)
		{
			return e2 == 8 || e2 == 9 || e2 == 28 || e2 == 29;
		}
		else if constexpr (
			canonical_rounding ==
				::fast_io::manipulators::floating_rounding::
					nearest_away_from_zero)
		{
			return e2 == 8;
		}
		else if constexpr (
			canonical_rounding ==
				::fast_io::manipulators::floating_rounding::
					away_from_zero)
		{
			return e2 == 1;
		}
		else
		{
			static_assert(
				canonical_rounding ==
				::fast_io::manipulators::floating_rounding::
					toward_zero);
			return false;
		}
	}
	else
	{
		static_assert(trait::mbits == 7u && trait::ebits == 8u);
		if constexpr (
			canonical_rounding ==
				::fast_io::manipulators::floating_rounding::
					toward_zero)
		{
			return false;
		}
		else if constexpr (
			canonical_rounding ==
				::fast_io::manipulators::floating_rounding::
					away_from_zero)
		{
			return e2 == 1;
		}
		else
		{
			/*
			The four masks are the exact characteristic function of the
			35-exponent set written in the theorem.  Dividing e2 by 64 selects
			one word and e2 mod 64 selects one bit, so the test is equivalent
			to the former 35-case switch.  A stored exponent field is in
			[0,254], proving both the array bound and the shift bound.  This
			32-byte representation also prevents Clang from outlining the
			switch and charging a function call to every ordinary value.
			*/
			auto const exponent{
				static_cast<::std::uint_least32_t>(e2)};
			bool const common{
				((dragonbox_bfloat16_low_exception_common_masks[
					  exponent >> 6u] >>
					 (exponent & 63u)) &
				 1u) != 0u};
			if constexpr (
				canonical_rounding ==
					::fast_io::manipulators::floating_rounding::
						nearest_away_from_zero)
			{
				return common || e2 == 136 || e2 == 137;
			}
			else
			{
				static_assert(
					canonical_rounding ==
						::fast_io::manipulators::
							floating_rounding::nearest_to_odd ||
					canonical_rounding ==
						::fast_io::manipulators::
							floating_rounding::
								nearest_toward_zero);
				return common;
			}
		}
	}
}

template <typename flt,
	::fast_io::manipulators::floating_rounding rounding>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
[[nodiscard]] inline constexpr bool
dragonbox_narrow_policy_raw_candidate_needs_fallback(
	typename iec559_traits<flt>::mantissa_type m2,
	::std::int_least32_t e2, bool negative) noexcept
{
	using enum ::fast_io::manipulators::floating_rounding;
	if constexpr (rounding == nearest_toward_plus_infinity)
	{
		return negative
			? ::fast_io::details::
				dragonbox_narrow_canonical_raw_candidate_needs_fallback<
					flt, nearest_toward_zero>(m2, e2)
			: ::fast_io::details::
				dragonbox_narrow_canonical_raw_candidate_needs_fallback<
					flt, nearest_away_from_zero>(m2, e2);
	}
	else if constexpr (rounding == nearest_toward_minus_infinity)
	{
		return negative
			? ::fast_io::details::
				dragonbox_narrow_canonical_raw_candidate_needs_fallback<
					flt, nearest_away_from_zero>(m2, e2)
			: ::fast_io::details::
				dragonbox_narrow_canonical_raw_candidate_needs_fallback<
					flt, nearest_toward_zero>(m2, e2);
	}
	else if constexpr (rounding == toward_plus_infinity)
	{
		return !negative &&
			::fast_io::details::
				dragonbox_narrow_canonical_raw_candidate_needs_fallback<
					flt, away_from_zero>(m2, e2);
	}
	else if constexpr (rounding == toward_minus_infinity)
	{
		return negative &&
			::fast_io::details::
				dragonbox_narrow_canonical_raw_candidate_needs_fallback<
					flt, away_from_zero>(m2, e2);
	}
	else
	{
		static_assert(
			rounding == nearest_to_odd ||
			rounding == nearest_toward_zero ||
			rounding == nearest_away_from_zero ||
			rounding == toward_zero ||
			rounding == away_from_zero);
		return ::fast_io::details::
			dragonbox_narrow_canonical_raw_candidate_needs_fallback<
				flt, rounding>(m2, e2);
	}
}

template <typename flt,
	::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr
	m10_result<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>
dragonbox_bfloat16_high_policy(
	typename iec559_traits<flt>::mantissa_type m2,
	::std::int_least32_t e2, bool negative) noexcept
{
	using enum ::fast_io::manipulators::floating_rounding;
	if constexpr (rounding == nearest_toward_plus_infinity)
	{
		return negative
			? ::fast_io::details::dragonbox_bfloat16_high_fallback<
				flt, nearest_toward_zero>(m2, e2)
			: ::fast_io::details::dragonbox_bfloat16_high_fallback<
				flt, nearest_away_from_zero>(m2, e2);
	}
	else if constexpr (rounding == nearest_toward_minus_infinity)
	{
		return negative
			? ::fast_io::details::dragonbox_bfloat16_high_fallback<
				flt, nearest_away_from_zero>(m2, e2)
			: ::fast_io::details::dragonbox_bfloat16_high_fallback<
				flt, nearest_toward_zero>(m2, e2);
	}
	else if constexpr (rounding == toward_plus_infinity)
	{
		return negative
			? ::fast_io::details::dragonbox_bfloat16_high_fallback<
				flt, toward_zero>(m2, e2)
			: ::fast_io::details::dragonbox_bfloat16_high_fallback<
				flt, away_from_zero>(m2, e2);
	}
	else if constexpr (rounding == toward_minus_infinity)
	{
		return negative
			? ::fast_io::details::dragonbox_bfloat16_high_fallback<
				flt, away_from_zero>(m2, e2)
			: ::fast_io::details::dragonbox_bfloat16_high_fallback<
				flt, toward_zero>(m2, e2);
	}
	else
	{
		static_assert(
			rounding == nearest_to_odd ||
			rounding == nearest_toward_zero ||
			rounding == nearest_away_from_zero ||
			rounding == toward_zero ||
			rounding == away_from_zero);
		return ::fast_io::details::dragonbox_bfloat16_high_fallback<
			flt, rounding>(m2, e2);
	}
}

/*
Exact low-exception carriers
============================

Every exception admitted by
`dragonbox_narrow_canonical_raw_candidate_needs_fallback` has m2=0 and is
therefore the exact power x=2^(e2-bias).  Let [A,B], (A,B], [A,B), or (A,B) be
the unsigned target interval selected by the policy.  A decimal c*10^q belongs
to it precisely when

       A_den*c*2^q*5^q  relation  A_num
and    B_den*c*2^q*5^q  relation  B_num,

after moving a negative q to the other side.  All terms are integers and the
relations retain the endpoint closures, so these comparisons have no
floating-point or rounding assumption.  For every case below, substituting
the returned (c,q) satisfies both inequalities.  Repeating the same integer
comparison after deleting the final decimal digit proves that the coarser
decimal lattice is empty; hence the carrier is shortest.  The exception-set
proof immediately above proves that these are the only powers for which the
raw narrow seed needs replacement.

The three nearest classes share the 35 bfloat16 rows because none of their
accepted carriers is a midpoint: endpoint closure changes the rejected
one-digit-shorter candidate, not the interior replacement.  Nearest-away has
two additional binade boundaries, e2=136 and e2=137.  A directed-away interval
has one exceptional smallest normal.  Keeping these finite proof witnesses as
immediates avoids both a full-domain atlas and a call to the general membership
engine on the hot path.
*/
template <typename flt,
	::fast_io::manipulators::floating_rounding canonical_rounding>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
[[nodiscard]] inline constexpr
	m10_result<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>
dragonbox_narrow_canonical_low_exception(
	::std::int_least32_t e2) noexcept
{
	using enum ::fast_io::manipulators::floating_rounding;
	using trait = ::fast_io::details::iec559_traits<flt>;
	if constexpr (canonical_rounding == toward_zero)
	{
		// Its exception set is empty; this total return is compile-time-only.
		return {};
	}
	if constexpr (trait::mbits == 10u && trait::ebits == 5u)
	{
		if constexpr (canonical_rounding == away_from_zero)
		{
			/*
			x=2^-14 has the directed-away magnitude interval
			(previous,x].  61*10^-6 lies strictly above the largest subnormal
			and below x; its one-digit predecessor 6*10^-5 is below the
			interval.  It is therefore the shortest narrow-lattice witness.
			*/
			return {61u, -6};
		}
		else
		{
			switch (e2)
			{
			case 8:
			{
				if constexpr (canonical_rounding == nearest_toward_zero)
				{
					return {7812u, -6};
				}
				else
				{
					return {7813u, -6};
				}
			}
			case 9:
				return {1563u, -5};
			case 28:
				return {8192u, 0};
			case 29:
				return {1639u, 1};
			default:
				/*
				The caller reaches this switch only after the policy-specific
				exception predicate.  The default makes the helper total for
				constant evaluation; it is unreachable under that proved
				precondition.
				*/
				return {};
			}
		}
	}
	else
	{
		static_assert(trait::mbits == 7u && trait::ebits == 8u);
		if constexpr (canonical_rounding == away_from_zero)
		{
			/*
			The bfloat16 smallest normal is 2^-126.  The same exact endpoint
			inequalities give 117*10^-40 as the shortest decimal inside its
			directed-away magnitude interval.
			*/
			return {117u, -40};
		}
		else
		{
			if constexpr (canonical_rounding == nearest_away_from_zero)
			{
				/*
				The two extra nearest-away rows share the same packed witness
				table; the policy predicate is what makes them unreachable to
				the other nearest classes.
				*/
			}
			auto const packed{
				::fast_io::details::
					dragonbox_bfloat16_low_exception_table_cache
						.values[static_cast<::std::size_t>(e2)]};
			return {
				static_cast<::fast_io::details::
					dragonbox_decimal_mantissa_type<flt>>(
						packed & 0xFFFFu),
				static_cast<::std::int_least32_t>(packed >> 16u) -
					dragonbox_bfloat16_low_exception_e10_bias};
		}
	}
}

/*
Sign reflection turns the ten public policies into the same five non-RNE
unsigned classes used by the exception predicate.  Applying the identical map
here is necessary and sufficient: |x| and the positive decimal coefficient are
unchanged by reflection, while only the open/closed endpoint direction swaps.
*/
template <typename flt,
	::fast_io::manipulators::floating_rounding rounding>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
[[nodiscard]] inline constexpr
	m10_result<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>
dragonbox_narrow_policy_low_exception(
	::std::int_least32_t e2, bool negative) noexcept
{
	using enum ::fast_io::manipulators::floating_rounding;
	if constexpr (rounding == nearest_toward_plus_infinity)
	{
		return negative
			? ::fast_io::details::
				dragonbox_narrow_canonical_low_exception<
					flt, nearest_toward_zero>(e2)
			: ::fast_io::details::
				dragonbox_narrow_canonical_low_exception<
					flt, nearest_away_from_zero>(e2);
	}
	else if constexpr (rounding == nearest_toward_minus_infinity)
	{
		return negative
			? ::fast_io::details::
				dragonbox_narrow_canonical_low_exception<
					flt, nearest_away_from_zero>(e2)
			: ::fast_io::details::
				dragonbox_narrow_canonical_low_exception<
					flt, nearest_toward_zero>(e2);
	}
	else if constexpr (rounding == toward_plus_infinity)
	{
		return ::fast_io::details::
			dragonbox_narrow_canonical_low_exception<
				flt, away_from_zero>(e2);
	}
	else if constexpr (rounding == toward_minus_infinity)
	{
		return ::fast_io::details::
			dragonbox_narrow_canonical_low_exception<
				flt, away_from_zero>(e2);
	}
	else
	{
		static_assert(
			rounding == nearest_to_odd ||
			rounding == nearest_toward_zero ||
			rounding == nearest_away_from_zero ||
			rounding == toward_zero ||
			rounding == away_from_zero);
		return ::fast_io::details::
			dragonbox_narrow_canonical_low_exception<
				flt, rounding>(e2);
	}
}

template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline constexpr m10_result<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>
dragonbox_impl_narrow_hybrid(typename iec559_traits<flt>::mantissa_type m2,
								 ::std::int_least32_t e2, bool negative) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		if constexpr (::fast_io::details::iec559_traits<flt>::mbits == 7u &&
					  ::fast_io::details::iec559_traits<flt>::ebits == 8u)
		{
			if (dragonbox_bfloat16_high_fallback_min_exponent <= e2 &&
				e2 <= dragonbox_bfloat16_high_fallback_max_exponent) [[unlikely]]
			{
				return ::fast_io::details::
					dragonbox_bfloat16_high_fallback<
						flt, ::fast_io::manipulators::
								 floating_rounding::
									 nearest_to_even>(m2, e2);
			}
		}
		if constexpr (::fast_io::details::iec559_traits<flt>::mbits == 10u &&
					  ::fast_io::details::iec559_traits<flt>::ebits == 5u)
		{
			if (m2 == 0u)
			{
				if (e2 == 8)
				{
					return {7812u, -6};
				}
				if (e2 == 9)
				{
					return {1563u, -5};
				}
			}
		}
		auto direct{::fast_io::details::dragonbox_main<flt>(m2, e2)};
		if (direct.m10 &&
			!::fast_io::details::dragonbox_narrow_raw_candidate_needs_fallback<flt>(m2, e2))
		{
			return ::fast_io::details::da::trim_trailing_zeros(direct);
		}
	}
	else
	{
		constexpr bool bfloat16{
			iec559_traits<flt>::mbits == 7u && iec559_traits<flt>::ebits == 8u};
		if constexpr (bfloat16)
		{
			if (dragonbox_bfloat16_high_fallback_min_exponent <= e2 &&
				e2 <= dragonbox_bfloat16_high_fallback_max_exponent)
				[[unlikely]]
			{
				/*
				The 11*128-entry canonical table stores only numeric carriers,
				not ASCII.  Each object is exactly 2,816 bytes; all six interval
				classes together occupy 16,896 bytes if a program instantiates
				every rounding policy.  Sign-directed policies select a class
				before lookup, so no sign dimension or duplicate full-domain
				atlas exists.
				*/
				return ::fast_io::details::
					dragonbox_bfloat16_high_policy<
						flt, rounding>(m2, e2, negative);
			}
		}
		auto const direct{
			::fast_io::details::da::trim_trailing_zeros(
				::fast_io::details::dragonbox_main_policy<
					flt, rounding>(m2, e2, negative))};
		if (!::fast_io::details::
				dragonbox_narrow_policy_raw_candidate_needs_fallback<
					flt, rounding>(m2, e2, negative))
		{
			return direct;
		}
		/*
		The preceding predicate proves m2=0 and identifies one row of the exact
		integer witnesses above.  Returning that witness inline leaves no
		general membership call, stack frame, or hidden full-domain table in the
		common path.
		*/
		return ::fast_io::details::
			dragonbox_narrow_policy_low_exception<
				flt, rounding>(e2, negative);
	}
	return ::fast_io::details::dragonbox_impl_narrow_from_float<flt, rounding>(m2, e2, negative);
}

inline constexpr ::std::size_t dragonbox_narrow_shortest_page_shift{6u};
inline constexpr ::std::size_t dragonbox_narrow_shortest_page_size{
	static_cast<::std::size_t>(1u) << dragonbox_narrow_shortest_page_shift};
inline constexpr ::std::size_t dragonbox_narrow_shortest_page_count{
	static_cast<::std::size_t>(1u) << (15u - dragonbox_narrow_shortest_page_shift)};

template <typename flt>
inline constexpr ::std::uint_least32_t
dragonbox_narrow_shortest_coefficient_bits{
	iec559_traits<flt>::mbits == 10u ? 15u : 12u};

template <typename flt>
inline constexpr ::std::uint_least32_t
dragonbox_narrow_shortest_coefficient_mask{
	(static_cast<::std::uint_least32_t>(1u)
	 << dragonbox_narrow_shortest_coefficient_bits<flt>) -
	1u};

template <typename flt>
inline constexpr ::std::uint_least32_t
dragonbox_narrow_shortest_e10_bits{
	iec559_traits<flt>::mbits == 10u ? 4u : 7u};

template <typename flt>
inline constexpr ::std::int_least32_t
dragonbox_narrow_shortest_e10_bias{
	iec559_traits<flt>::mbits == 10u ? 8 : 41};

template <typename flt>
inline constexpr ::std::uint_least32_t
dragonbox_narrow_shortest_length_bits{
	iec559_traits<flt>::mbits == 10u ? 3u : 2u};

template <typename flt>
inline constexpr ::std::uint_least32_t
dragonbox_narrow_shortest_e10_shift{
	dragonbox_narrow_shortest_coefficient_bits<flt>};

template <typename flt>
inline constexpr ::std::uint_least32_t
dragonbox_narrow_shortest_length_shift{
	dragonbox_narrow_shortest_e10_shift<flt> +
	dragonbox_narrow_shortest_e10_bits<flt>};

template <typename flt>
inline constexpr ::std::uint_least32_t
dragonbox_narrow_shortest_layout_shift{
	dragonbox_narrow_shortest_length_shift<flt> +
	dragonbox_narrow_shortest_length_bits<flt>};

/*
Exact scientific spelling length
--------------------------------

Let a nonzero decimal carrier have L coefficient digits and scientific
exponent X.  Its coefficient occupies one character when L=1 and L+1
characters otherwise (the extra character is the radix point).  The exponent
suffix occupies

	e, sign, max(2,digits(|X|))

characters.  The unsigned negation below is defined even for INT32_MIN and
`chars_len` supplies the exact decimal digit count.  Saturating both additions
makes the helper valid for precision modes whose virtual coefficient length is
chosen by the caller; shortest carriers are far below the saturation bound.

Every decimal notation selector uses this same function.  In particular, a
one-digit coefficient has length five for a two-digit exponent (`1e-03`), not
four.  Therefore an equal-length fixed spelling such as `0.001` or `10000`
wins the documented fixed-on-equality rule.  The helper also accounts for the
third and later exponent digits required by binary64, binary80 and binary128.
*/
[[nodiscard]] inline constexpr ::std::size_t
print_rsv_fp_scientific_length(
	::std::int_least32_t scientific_exponent,
	::std::size_t coefficient_digits) noexcept
{
	auto exponent_magnitude{
		static_cast<::std::uint_least32_t>(scientific_exponent)};
	if (scientific_exponent < 0)
	{
		exponent_magnitude = 0u - exponent_magnitude;
	}
	auto exponent_digits{static_cast<::std::size_t>(
		::fast_io::details::chars_len<10u, true>(exponent_magnitude))};
	if (exponent_digits < 2u)
	{
		exponent_digits = 2u;
	}
	constexpr auto maximum{
		(::std::numeric_limits<::std::size_t>::max)()};
	auto result{coefficient_digits};
	auto const point_size{
		static_cast<::std::size_t>(coefficient_digits != 1u)};
	result = maximum - result < point_size
		? maximum
		: result + point_size;
	auto const suffix_size{exponent_digits + 2u};
	return maximum - result < suffix_size
		? maximum
		: result + suffix_size;
}

/*
Scientific-only raw-exponent bands
----------------------------------

Let a finalized shortest decimal be M*10^e, let L be the number of digits of
M, and put X=e+L-1.  Then 10^X<=M*10^e<10^(X+1).  Binary32 and binary64
shortest carriers satisfy L<=9 and L<=17 respectively.

For a raw normal exponent r and bias b, the source magnitude lies in
[2^(r-b),2^(r-b+1)).  A decimal which rounds to that source lies strictly
inside the two adjacent finite floats: nearest policies are bounded by their
midpoints, while directed policies are bounded by the target and the relevant
neighbour.  The sign reverses interval orientation but leaves magnitudes and
spelling lengths unchanged.

At the lower boundary r<=b-15, the successor magnitude is at most 2^-14.
Since 2^-14<10^-4, every admissible decimal has X<=-5.

At the upper binary32 boundary r>=b+47, even the predecessor of the smallest
source is

	2^47-2^23 = 140737479966720 > 10^14,

so X>=14.  For binary64 r>=b+74, that predecessor is

	2^74-2^21 = 18889465931478578757632 > 10^22,

so X>=22.

Ignoring a sign/showpos character common to both spellings, fixed has length
L-X+1 for X<0 and X+1 for X>=L.  Scientific length is computed exactly by
print_rsv_fp_scientific_length:

* X<=-5: fixed is at least L+6.  Scientific is 5 for L=1 and L+5 for
  L>1 while |X|<100; every additional exponent digit is dominated by the
  corresponding growth of -X.
* binary32 X>=14: fixed is at least 15, scientific at most L+5<=14.
* binary64 22<=X<100: fixed is at least 23, scientific at most L+5<=22.
  For X>=100 fixed is at least 101 while scientific is at most L+6<=23.

Thus scientific is strictly shorter throughout all four bands.  Equality is
impossible, so decimal's fixed-on-equality rule is not bypassed.  Comma and
uppercase E preserve length; JSON's optional ".0" can only lengthen an
integral fixed spelling.  Special values and zero return before this predicate,
and raw exponent zero is excluded, so no subnormal normalization invariant is
changed.

The presentation theorem itself is rounding-policy independent.  The current
caller is deliberately narrower: only nearest-even binary32/binary64 reaches
the DA carrier used below.  Extending another policy requires first proving
that policy's shortest carrier; this predicate alone cannot justify reusing the
nearest-even conversion.
*/
template <typename flt>
[[nodiscard]] inline constexpr bool
print_rsvflt_da_scientific_is_strictly_shorter(
	::std::uint_least32_t raw_exponent) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	static_assert(
		(trait::mbits == 23u && trait::ebits == 8u) ||
		(trait::mbits == 52u && trait::ebits == 11u));
	constexpr ::std::uint_least32_t bias{
		(static_cast<::std::uint_least32_t>(1u)
		 << (trait::ebits - 1u)) -
		1u};
	constexpr ::std::uint_least32_t lower_raw_exponent{bias - 15u};
	constexpr ::std::uint_least32_t upper_raw_exponent{
		bias + (trait::mbits == 23u ? 47u : 74u)};
	return raw_exponent != 0u &&
		(raw_exponent <= lower_raw_exponent ||
		 upper_raw_exponent <= raw_exponent);
}

/*
Compact binary16/bfloat16 nearest-even carrier table
====================================================

For every non-sign raw pattern r, the canonical shortest carrier is

    C(r) = (m(r), e(r)),             value = m(r) * 10^e(r).

Exhaustive construction below proves the tighter representation-specific
bounds

    binary16:  m < 2^15, -8 <= e <= 4,  1 <= length <= 5;
    bfloat16:  m < 2^12, -41 <= e <= 38, 1 <= length <= 4.

Special encodings retain the zero sentinel.  The decimal length is mathematically
derivable from m, but retaining its two or three bits removes a dependent
integer-logarithm chain from the hot renderer.  The remaining high bits encode
one of the following exhaustive default-decimal layouts:

    0 scientific, 1 integer with appended zeroes,
    2 fixed with an internal point, 3 fixed below one.

Consequently the injective packing

    P(r) = m | biased_e << M | (length-1) << (M+E)
             | layout << (M+E+L)

uses 24 bits for binary16 and 23 bits for bfloat16.  The layout is a redundant
projection of `(m,e)`, so it changes no numeric result; it merely removes the
same branch arithmetic from every rendering.  The three-byte
little-significance representation is an arithmetic serialization, not a host
object representation.  Explicit byte extraction during construction and
reconstruction during lookup therefore prove identical values on little
endian, big endian, ASCII, and EBCDIC targets.

There are 2^15 magnitude patterns, hence one instantiated table occupies
exactly 3 * 2^15 = 98,304 bytes = 96 KiB.  The static assertions below make
the project's 100-KiB-per-table budget a compile-time invariant.  This table
replaces both the former 128-KiB carrier table and the 352-KiB pre-rendered
ASCII table: presentation remains code-unit generic and no second full-domain
table can be pulled into the linked image.
*/
struct dragonbox_narrow_shortest_packed
{
	::std::uint_least8_t bytes[3]{};
};

template <::std::size_t size>
struct dragonbox_narrow_shortest_page
{
	dragonbox_narrow_shortest_packed values[size]{};
};

template <::std::size_t page_count, ::std::size_t page_size>
struct alignas(64) dragonbox_narrow_shortest_table
{
	dragonbox_narrow_shortest_packed values[page_count * page_size]{};
};

template <typename flt, ::std::size_t page>
[[nodiscard]] inline constexpr dragonbox_narrow_shortest_page<dragonbox_narrow_shortest_page_size>
dragonbox_make_narrow_shortest_page() noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	dragonbox_narrow_shortest_page<dragonbox_narrow_shortest_page_size> result;
	constexpr mantissa_type exponent_mask{(static_cast<mantissa_type>(1u) << trait::ebits) - 1u};
	for (::std::size_t index{}; index != dragonbox_narrow_shortest_page_size; ++index)
	{
		auto const raw{static_cast<mantissa_type>((page << dragonbox_narrow_shortest_page_shift) | index)};
		if (!raw)
		{
			continue;
		}
		auto const value{::std::bit_cast<flt>(raw)};
		auto const binary{::fast_io::details::get_punned_result(value)};
		if (binary.exponent == exponent_mask)
		{
			continue;
		}
		m10_result<::fast_io::details::dragonbox_decimal_mantissa_type<flt>> decimal;
		if (::fast_io::details::dragonbox_narrow_raw_candidate_needs_fallback<flt>(
				binary.mantissa, static_cast<::std::int_least32_t>(binary.exponent)))
		{
			auto const float_binary{::fast_io::details::dragonbox_narrow_float_punned<flt>(
				binary.mantissa, static_cast<::std::int_least32_t>(binary.exponent), false)};
			auto [m10, e10] = ::fast_io::details::dragonbox_impl<
				float, ::fast_io::manipulators::floating_rounding::nearest_to_even>(
				float_binary.mantissa, static_cast<::std::int_least32_t>(float_binary.exponent), false);
			::fast_io::details::dragonbox_shorten_decimal_to_target<
				flt, ::fast_io::manipulators::floating_rounding::nearest_to_even>(
				m10, e10, binary.mantissa, static_cast<::std::int_least32_t>(binary.exponent), false);
			decimal = {m10, e10};
		}
		else
		{
			decimal = ::fast_io::details::da::trim_trailing_zeros(
				::fast_io::details::dragonbox_main<flt>(
					binary.mantissa, static_cast<::std::int_least32_t>(binary.exponent)));
		}
		auto const length{static_cast<::std::uint_least32_t>(
			chars_len<10, true>(decimal.m10))};
		auto const real_exponent{static_cast<::std::int_least32_t>(
			decimal.e10 + static_cast<::std::int_least32_t>(length) - 1)};
		::std::uint_least32_t fixed_length{};
		if (static_cast<::std::int_least32_t>(length) <= real_exponent)
		{
			fixed_length =
				static_cast<::std::uint_least32_t>(real_exponent + 1);
		}
		else if (0 <= real_exponent)
		{
			fixed_length = length + 1u +
				static_cast<::std::uint_least32_t>(
					static_cast<::std::int_least32_t>(length) !=
					real_exponent + 1);
		}
		else
		{
			fixed_length =
				static_cast<::std::uint_least32_t>(-real_exponent) +
				length + 1u;
		}
		auto const scientific_length{
			::fast_io::details::print_rsv_fp_scientific_length(
				real_exponent, length)};
		::std::uint_least32_t layout{};
		if (scientific_length >= fixed_length)
		{
			layout =
				static_cast<::std::int_least32_t>(length) <=
						real_exponent
					? 1u
				: 0 <= real_exponent ? 2u
									 : 3u;
		}
		auto const packed{
			static_cast<::std::uint_least32_t>(decimal.m10) |
			(static_cast<::std::uint_least32_t>(
				 decimal.e10 +
				 dragonbox_narrow_shortest_e10_bias<flt>)
			 << dragonbox_narrow_shortest_e10_shift<flt>) |
			((length - 1u)
			 << dragonbox_narrow_shortest_length_shift<flt>) |
			(layout
			 << dragonbox_narrow_shortest_layout_shift<flt>)};
		/*
		These assertions are evaluated independently for every one of the 32,768
		generated patterns.  They are the constructive range proof for the
		22-bit encoding, rather than an assumption made by the run-time lookup.
		*/
		if ((static_cast<::std::uint_least32_t>(decimal.m10) &
			 ~dragonbox_narrow_shortest_coefficient_mask<flt>) != 0u ||
			decimal.e10 <
				-dragonbox_narrow_shortest_e10_bias<flt> ||
			(static_cast<::std::int_least32_t>(1u)
				 << dragonbox_narrow_shortest_e10_bits<flt>) <=
				decimal.e10 +
					dragonbox_narrow_shortest_e10_bias<flt> ||
			(static_cast<::std::uint_least32_t>(1u)
				 << dragonbox_narrow_shortest_length_bits<flt>) <
				length)
		{
			::fast_io::fast_terminate();
		}
		result.values[index].bytes[0] =
			static_cast<::std::uint_least8_t>(packed);
		result.values[index].bytes[1] =
			static_cast<::std::uint_least8_t>(packed >> 8u);
		result.values[index].bytes[2] =
			static_cast<::std::uint_least8_t>(packed >> 16u);
	}
	return result;
}

template <typename flt, ::std::size_t page>
inline constexpr auto dragonbox_narrow_shortest_page_cache{
	::fast_io::details::dragonbox_make_narrow_shortest_page<flt, page>()};

template <::std::size_t page, ::std::size_t page_count, ::std::size_t page_size>
inline constexpr void dragonbox_copy_narrow_shortest_page(
	dragonbox_narrow_shortest_table<page_count, page_size> &table,
	dragonbox_narrow_shortest_page<page_size> const &source) noexcept
{
	for (::std::size_t index{}; index != page_size; ++index)
	{
		table.values[page * page_size + index] = source.values[index];
	}
}

#if defined(__clang__) && 17 <= __clang_major__ && __clang_major__ < 21
/// Copies one bounded block of narrow shortest-carrier pages during constant
/// table construction.
///
/// Clang 17--20 reject the equivalent 512-operand fold at their default
/// expression-nesting limit of 256. Clang 21 is the first measured release
/// whose default accepts that original expression, so newer Clang and every
/// GCC release retain it below. A 64-page block stays conservatively below the
/// affected limit while preserving page order and bytes. A pack-expanded
/// initializer was also byte-identical, but made the Clang 17 probe compile
/// 2.26 times more slowly than this bounded fold.
template <typename flt, ::std::size_t first_page,
	::std::size_t page_count, ::std::size_t page_size,
	::std::size_t... offsets>
inline constexpr void dragonbox_copy_narrow_shortest_page_block(
	dragonbox_narrow_shortest_table<page_count, page_size> &table,
	::std::index_sequence<offsets...>) noexcept
{
	static_assert(sizeof...(offsets) <= 64u);
	(::fast_io::details::dragonbox_copy_narrow_shortest_page<
		 first_page + offsets>(table,
		 ::fast_io::details::dragonbox_narrow_shortest_page_cache<
			 flt, first_page + offsets>), ...);
}
#endif

template <typename flt, ::std::size_t... pages>
[[nodiscard]] inline constexpr dragonbox_narrow_shortest_table<sizeof...(pages),
															   dragonbox_narrow_shortest_page_size>
	dragonbox_make_narrow_shortest_table(::std::index_sequence<pages...>) noexcept
{
	dragonbox_narrow_shortest_table<sizeof...(pages), dragonbox_narrow_shortest_page_size> result;
#if defined(__clang__) && 17 <= __clang_major__ && __clang_major__ < 21
	static_assert(sizeof...(pages) == dragonbox_narrow_shortest_page_count);
	// Eight independent 64-page folds preserve the generated table byte for
	// byte without requiring users to raise Clang 17--20's language limit.
	::fast_io::details::dragonbox_copy_narrow_shortest_page_block<flt, 0u>(
		result, ::std::make_index_sequence<64u>{});
	::fast_io::details::dragonbox_copy_narrow_shortest_page_block<flt, 64u>(
		result, ::std::make_index_sequence<64u>{});
	::fast_io::details::dragonbox_copy_narrow_shortest_page_block<flt, 128u>(
		result, ::std::make_index_sequence<64u>{});
	::fast_io::details::dragonbox_copy_narrow_shortest_page_block<flt, 192u>(
		result, ::std::make_index_sequence<64u>{});
	::fast_io::details::dragonbox_copy_narrow_shortest_page_block<flt, 256u>(
		result, ::std::make_index_sequence<64u>{});
	::fast_io::details::dragonbox_copy_narrow_shortest_page_block<flt, 320u>(
		result, ::std::make_index_sequence<64u>{});
	::fast_io::details::dragonbox_copy_narrow_shortest_page_block<flt, 384u>(
		result, ::std::make_index_sequence<64u>{});
	::fast_io::details::dragonbox_copy_narrow_shortest_page_block<flt, 448u>(
		result, ::std::make_index_sequence<64u>{});
#else
	(::fast_io::details::dragonbox_copy_narrow_shortest_page<pages>(
		 result, ::fast_io::details::dragonbox_narrow_shortest_page_cache<flt, pages>),
	 ...);
#endif
	return result;
}

template <typename flt>
// Apply the same non-interposable table policy used by the DA caches when its
// narrow "hidden" argument has the ASCII execution-set spelling.  GCC reports
// attribute support under IBM1047 but rejects that translated string argument,
// so __has_cpp_attribute alone is insufficient.  Lookup contents are generated
// identically when the optional linkage decoration is omitted.
#if __has_cpp_attribute(__gnu__::__visibility__) && 'A' == 0x41
[[__gnu__::__visibility__("hidden")]]
#endif
inline constexpr auto dragonbox_narrow_shortest_table_cache{
	::fast_io::details::dragonbox_make_narrow_shortest_table<flt>(
		::std::make_index_sequence<dragonbox_narrow_shortest_page_count>{})};

static_assert(sizeof(dragonbox_narrow_shortest_packed) == 3u);
static_assert(
	sizeof(dragonbox_narrow_shortest_table<
		dragonbox_narrow_shortest_page_count,
		dragonbox_narrow_shortest_page_size>) ==
	3u * (static_cast<::std::size_t>(1u) << 15u));
static_assert(
	sizeof(dragonbox_narrow_shortest_table<
		dragonbox_narrow_shortest_page_count,
		dragonbox_narrow_shortest_page_size>) <= 100u * 1024u);

template <typename flt>
struct dragonbox_narrow_shortest_result
{
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10;
	::std::int_least32_t e10;
	::std::uint_least32_t length;
	::std::uint_least32_t decimal_layout;
};

template <typename flt>
[[nodiscard]] inline constexpr dragonbox_narrow_shortest_result<flt>
dragonbox_narrow_shortest_lookup(typename iec559_traits<flt>::mantissa_type m2,
								 ::std::int_least32_t e2) noexcept
{
	using decimal_type = ::fast_io::details::dragonbox_decimal_mantissa_type<flt>;
	auto const raw{(static_cast<::std::uint_least32_t>(e2) << iec559_traits<flt>::mbits) |
				   static_cast<::std::uint_least32_t>(m2)};
	auto const &serialized{
		::fast_io::details::dragonbox_narrow_shortest_table_cache<
			flt>.values[raw]};
	/*
	The table's byte order is defined by the equations in its construction, so
	this reconstruction is host-endian independent.  Three independent byte
	loads also obey the exact 96-KiB object bound at the final entry; a tempting
	unaligned four-byte load would read one byte past the array and would not be
	a valid optimization even on hardware that tolerates it.
	*/
	auto const packed{
		static_cast<::std::uint_least32_t>(serialized.bytes[0]) |
		(static_cast<::std::uint_least32_t>(serialized.bytes[1]) << 8u) |
		(static_cast<::std::uint_least32_t>(serialized.bytes[2]) << 16u)};
	auto const coefficient{
		static_cast<decimal_type>(
			packed &
			dragonbox_narrow_shortest_coefficient_mask<flt>)};
	constexpr auto e10_mask{
		(static_cast<::std::uint_least32_t>(1u)
		 << dragonbox_narrow_shortest_e10_bits<flt>) -
		1u};
	constexpr auto length_mask{
		(static_cast<::std::uint_least32_t>(1u)
		 << dragonbox_narrow_shortest_length_bits<flt>) -
		1u};
	return {
		coefficient,
		static_cast<::std::int_least32_t>(
			(packed >> dragonbox_narrow_shortest_e10_shift<flt>) &
			e10_mask) -
			dragonbox_narrow_shortest_e10_bias<flt>,
		((packed >> dragonbox_narrow_shortest_length_shift<flt>) &
			length_mask) +
			1u,
		packed >> dragonbox_narrow_shortest_layout_shift<flt>};
}

// These fixed-length digit leaves are called by nearly every presentation
// writer, and callers usually know the width at compile time.  `always_inline`
// is a code-shape hypothesis intended to propagate that width and remove a leaf
// call; it is not an ISA gate and has no independent arithmetic meaning.  A
// compiler without the attribute may still inline normally, and otherwise emits
// exactly the same digits through an ordinary call.
template <typename flt, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline constexpr void print_rsv_fp_digits_len(
	char_type *iter, ::fast_io::details::dragonbox_decimal_mantissa_type<flt> value,
	::std::uint_least32_t length) noexcept
{
	if constexpr (::fast_io::details::dragonbox_uses_binary32_core<flt>)
	{
		// GCC x86 keeps the known output length efficiently in jeaiii_main_len;
		// the other audited compiler/ISA paths use the fixed nine-digit hash
		// primitive and pass the same logical length.  Both are exact integer-to-
		// digit writers.  This is a code-generation partition: revalidate calls,
		// branches, register pressure and stores before changing its compiler fence.
		// GCC 13--16 form the measured Linux System V x86-64 LP64 matrix; GCC 13 is
		// the continuous lower bound, so later GNU frontends inherit the latest
		// proved writer.  x32, MinGW, the Microsoft ABI and non-Linux x86-64 use the
		// fixed hash writer.  The two writers are semantically identical; moving the
		// lower bound requires whole-caller assembly and size evidence.
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__ && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
		::fast_io::details::jeaiii::jeaiii_main_len<true>(iter, value, length);
#else
		::fast_io::details::jeaiii::jeaiii_hash<9u>(iter, static_cast<::std::uint_least32_t>(value), length);
#endif
	}
	else
	{
		::fast_io::details::jeaiii::jeaiii_main_len<true>(iter, value, length);
	}
}

// The variable-length companion has the same high-fan-out leaf role.  Forcing
// inlining exposes the proven decimal length to surrounding punctuation code;
// this is a profile-free code-generation policy rather than a measured target
// cutoff.  Attribute absence changes only whether a call remains, never the
// returned end pointer or emitted character sequence.
template <typename flt, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#endif
inline constexpr char_type *print_rsv_fp_digits(
	char_type *iter, ::fast_io::details::dragonbox_decimal_mantissa_type<flt> value) noexcept
{
	if constexpr (::fast_io::details::dragonbox_uses_binary32_core<flt>)
	{
		// Same GCC-x86 digit-writer partition as print_rsv_fp_digits_len.  The GCC
		// path lets jeaiii compute the end directly; the alternative computes the
		// proven base-10 length and invokes the fixed nine-digit writer.  Output is
		// identical, so this fence must remain justified by assembly rather than by
		// floating-point semantics.  GCC 13--16 form the measured Linux System V
		// x86-64 LP64 matrix, and later GNU frontends inherit the GCC-16 policy from
		// the continuous GCC-13 lower bound.  Every other ABI uses the fixed hash
		// writer until its calls, stores, live ranges and linked text are re-audited.
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__ && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
		return ::fast_io::details::jeaiii::jeaiii_main<false>(iter, value);
#else
		auto const length{static_cast<::std::uint_least32_t>(chars_len<10, true>(value))};
		::fast_io::details::jeaiii::jeaiii_hash<9u>(iter, static_cast<::std::uint_least32_t>(value), length);
		return iter + length;
#endif
	}
	else
	{
		return ::fast_io::details::jeaiii::jeaiii_main<false>(iter, value);
	}
}

template <bool comma, ::std::integral char_type, my_unsigned_integral U>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_rsv_fp_decimal_scientific_common_impl(char_type *iter, U m10,
																								  ::std::uint_least32_t m10len) noexcept
{
	auto itp1{iter + 1};
	::fast_io::details::jeaiii::jeaiii_main_len<true>(itp1, m10, m10len);
	*iter = *itp1;
	*itp1 = char_literal_v<comma ? u8',' : u8'.', char_type>;
	return itp1 + m10len;
}

template <bool comma, ::std::integral char_type, my_unsigned_integral U>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_rsv_fp_decimal_common_impl(char_type *iter, U m10,
																					   ::std::uint_least32_t m10len) noexcept
{
	if (m10len == 1) [[unlikely]]
	{
		*iter = ::fast_io::char_literal_add<char_type>(static_cast<::std::uint_least32_t>(m10));
		++iter;
		return iter;
	}
	else
	{
		return print_rsv_fp_decimal_scientific_common_impl<comma>(iter, m10, m10len);
	}
}

template <typename flt, bool uppercase_e, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_rsv_fp_e_impl(char_type *iter, ::std::int_least32_t e10) noexcept
{
	*iter = char_literal_v < uppercase_e ? u8'E' : u8'e', char_type > ;
	++iter;
	::std::uint_least32_t ue10{static_cast<::std::uint_least32_t>(e10)};
	if (e10 < 0)
	{
		ue10 = 0u - ue10;
		*iter = char_literal_v<u8'-', char_type>;
	}
	else
	{
		*iter = char_literal_v<u8'+', char_type>;
	}
	++iter;
	return prt_rsv_exponent_impl<iec559_traits<flt>::e10digits, true>(iter, ue10);
}

template <::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *fill_zeros_impl(char_type *iter, ::std::size_t n) noexcept
{
	return ::fast_io::details::my_fill_n(
		iter, n, char_literal_v<u8'0', char_type>);
}

template <bool comma, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *fill_zero_point_impl(char_type *iter) noexcept
{
	if constexpr (!::fast_io::details::is_ascii<char_type> &&
		(::std::same_as<char_type, char> || ::std::same_as<char_type, wchar_t>))
	{
		*iter = ::fast_io::char_literal_v<u8'0', char_type>;
		++iter;
		*iter = ::fast_io::char_literal_v<(comma ? u8',' : u8'.'), char_type>;
		return iter + 1u;
	}
	else if constexpr (comma)
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("0,", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"0,", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"0,", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"0,", iter);
		}
		else
		{
			return copy_string_literal(u8"0,", iter);
		}
	}
	else
	{
		if constexpr (::std::same_as<char_type, char>)
		{
			return copy_string_literal("0.", iter);
		}
		else if constexpr (::std::same_as<char_type, wchar_t>)
		{
			return copy_string_literal(L"0.", iter);
		}
		else if constexpr (::std::same_as<char_type, char16_t>)
		{
			return copy_string_literal(u"0.", iter);
		}
		else if constexpr (::std::same_as<char_type, char32_t>)
		{
			return copy_string_literal(U"0.", iter);
		}
		else
		{
			return copy_string_literal(u8"0.", iter);
		}
	}
}

template <typename flt, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *fixed_case0_full_integer(char_type *iter,
																			   ::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10,
																			   ::std::int_least32_t olength,
																			   ::std::int_least32_t real_exp) noexcept
{
	::fast_io::details::print_rsv_fp_digits_len<flt>(iter, m10, static_cast<::std::uint_least32_t>(olength));
	iter += olength;
	return fill_zeros_impl(iter, static_cast<::std::uint_least32_t>(real_exp + 1 - olength));
}

template <bool comma, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_rsv_fp_append_json_float_zero(char_type *iter) noexcept
{
	*iter = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	++iter;
	*iter = char_literal_v<u8'0', char_type>;
	++iter;
	return iter;
}

template <typename flt, bool comma, bool json_float, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *fixed_case0_full_integer_maybe_json(
	char_type *iter, ::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10, ::std::int_least32_t olength,
	::std::int_least32_t real_exp) noexcept
{
	iter = fixed_case0_full_integer<flt>(iter, m10, olength, real_exp);
	if constexpr (json_float)
	{
		return ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(iter);
	}
	else
	{
		return iter;
	}
}

template <typename flt, bool comma, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
fixed_case1_integer_and_point(char_type *iter, ::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10,
							  ::std::int_least32_t olength, ::std::int_least32_t real_exp) noexcept
{
	auto eposition(real_exp + 1);
	if (olength == eposition)
	{
		::fast_io::details::print_rsv_fp_digits_len<flt>(iter, m10, static_cast<::std::uint_least32_t>(olength));
		iter += olength;
	}
	else
	{
		auto tmp{iter};
		::fast_io::details::print_rsv_fp_digits_len<flt>(iter + 1, m10,
														 static_cast<::std::uint_least32_t>(olength));
		iter += olength + 1;
		my_copy_n(tmp + 1, static_cast<::std::uint_least32_t>(eposition), tmp);
		tmp[eposition] = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	}
	return iter;
}

template <typename flt, bool comma, bool json_float, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
fixed_case1_integer_and_point_maybe_json(char_type *iter,
										 ::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10,
										 ::std::int_least32_t olength,
										 ::std::int_least32_t real_exp) noexcept
{
	auto eposition(real_exp + 1);
	if (olength == eposition)
	{
		::fast_io::details::print_rsv_fp_digits_len<flt>(iter, m10, static_cast<::std::uint_least32_t>(olength));
		iter += olength;
		if constexpr (json_float)
		{
			return ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(iter);
		}
	}
	else
	{
		auto tmp{iter};
		::fast_io::details::print_rsv_fp_digits_len<flt>(iter + 1u, m10,
														 static_cast<::std::uint_least32_t>(olength));
		// `olength` is a positive signed digit count; retain that pointer-difference domain when accounting for the point.
		iter += olength + 1;
		my_copy_n(tmp + 1u, static_cast<::std::uint_least32_t>(eposition), tmp);
		tmp[eposition] = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	}
	return iter;
}

template <typename flt, bool comma, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *fixed_case2_all_point(char_type *iter,
																			::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10,
																			::std::int_least32_t olength, ::std::int_least32_t real_exp) noexcept
{
	iter = fill_zero_point_impl<comma>(iter);
	iter = fill_zeros_impl(iter, static_cast<::std::uint_least32_t>(-real_exp - 1));
	::fast_io::details::print_rsv_fp_digits_len<flt>(iter, m10, static_cast<::std::uint_least32_t>(olength));
	iter += olength;
	return iter;
}

template <typename flt, bool comma, bool json_float = false, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_rsv_fp_fixed_decision_with_length_impl(
	char_type *iter, ::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10,
	::std::int_least32_t e10, ::std::int_least32_t olength) noexcept
{
	::std::int_least32_t const real_exp(static_cast<::std::int_least32_t>(e10 + olength - 1));
	if (olength <= real_exp)
	{
		return fixed_case0_full_integer_maybe_json<flt, comma, json_float>(iter, m10, olength, real_exp);
	}
	else if (0 <= real_exp && real_exp < olength)
	{
		return fixed_case1_integer_and_point_maybe_json<flt, comma, json_float>(iter, m10, olength, real_exp);
	}
	else
	{
		return fixed_case2_all_point<flt, comma>(iter, m10, olength, real_exp);
	}
}

template <typename flt, bool comma, bool json_float = false, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_rsv_fp_fixed_decision_impl(
	char_type *iter, ::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10,
	::std::int_least32_t e10) noexcept
{
	::std::int_least32_t olength(static_cast<::std::int_least32_t>(chars_len<10, true>(m10)));
	::std::int_least32_t const real_exp(static_cast<::std::int_least32_t>(e10 + olength - 1));
	if (olength <= real_exp)
	{
		return fixed_case0_full_integer_maybe_json<flt, comma, json_float>(iter, m10, olength, real_exp);
	}
	else if (0 <= real_exp && real_exp < olength)
	{
		return fixed_case1_integer_and_point_maybe_json<flt, comma, json_float>(iter, m10, olength, real_exp);
	}
	else
	{
		return fixed_case2_all_point<flt, comma>(iter, m10, olength, real_exp);
	}
}

template <typename flt, bool comma, bool uppercase_e, ::fast_io::manipulators::floating_format mt,
		  bool json_float = false, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_rsv_fp_decision_with_length_impl(
	char_type *iter, ::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10,
	::std::int_least32_t e10, ::std::int_least32_t olength) noexcept
{
	if constexpr (mt == ::fast_io::manipulators::floating_format::general)
	{
		/*
		Let L be the number of decimal digits in m10.  The represented value is
		m10*10^e10, so moving the radix behind the leading digit gives scientific
		exponent X=e10+L-1.  The no-precision general presentation uses the
		printf-g default decision: fixed iff -4<=X<6.  Testing e10 itself would
		be incorrect after trailing-zero removal (for example,
		123456789*10^2 has e10=2 but X=10).
		*/
		auto const scientific_exponent{
			static_cast<::std::int_least32_t>(e10 + olength - 1)};
		if (-4 <= scientific_exponent && scientific_exponent < 6)
		{
			return print_rsv_fp_fixed_decision_with_length_impl<flt, comma, json_float>(
				iter, m10, e10, olength);
		}
		return print_rsv_fp_decision_with_length_impl<
			flt, comma, uppercase_e, ::fast_io::manipulators::floating_format::scientific, false>(
			iter, m10, e10, olength);
	}
	else if constexpr (mt == ::fast_io::manipulators::floating_format::scientific)
	{
		if (m10 < 10u) [[unlikely]]
		{
			*iter = ::fast_io::char_literal_add<char_type>(static_cast<::std::uint_least32_t>(m10));
			++iter;
		}
		else
		{
			auto iterp1{iter};
			++iterp1;
			::fast_io::details::print_rsv_fp_digits_len<flt>(
				iterp1, m10, static_cast<::std::uint_least32_t>(olength));
			auto const new_iter{iterp1 + olength};
			e10 += olength - 1;
			*iter = *iterp1;
			*iterp1 = char_literal_v < comma ? u8',' : u8'.', char_type > ;
			iter = new_iter;
		}
		return print_rsv_fp_e_impl<flt, uppercase_e>(iter, e10);
	}
	else // The only remaining floating_format enumerator here is decimal.
	{
		::std::int_least32_t const real_exp{static_cast<::std::int_least32_t>(e10 + olength - 1)};
		::std::uint_least32_t fixed_length{};
		if (olength <= real_exp)
		{
			fixed_length = static_cast<::std::uint_least32_t>(real_exp + 1);
		}
		else if (0 <= real_exp && real_exp < olength)
		{
			fixed_length = static_cast<::std::uint_least32_t>(olength + 2);
			if (olength == real_exp + 1)
			{
				--fixed_length;
			}
		}
		else
		{
			fixed_length = static_cast<::std::uint_least32_t>(static_cast<::std::uint_least32_t>(-real_exp) +
															  static_cast<::std::uint_least32_t>(olength) + 1u);
		}
		auto const scientific_length{
			::fast_io::details::print_rsv_fp_scientific_length(
				real_exp, static_cast<::std::size_t>(olength))};
		if (scientific_length < fixed_length)
		{
			// scientific decision
			iter = print_rsv_fp_decimal_common_impl<comma>(iter, m10, static_cast<::std::uint_least32_t>(olength));
			return print_rsv_fp_e_impl<flt, uppercase_e>(iter, real_exp);
		}
		if (olength <= real_exp)
		{
			return fixed_case0_full_integer_maybe_json<flt, comma, json_float>(iter, m10, olength, real_exp);
		}
		else if (0 <= real_exp)
		{
			return fixed_case1_integer_and_point_maybe_json<flt, comma, json_float>(iter, m10, olength, real_exp);
		}
		else
		{
			return fixed_case2_all_point<flt, comma>(iter, m10, olength, real_exp);
		}
	}
}

template <typename flt, bool comma, bool uppercase_e, ::fast_io::manipulators::floating_format mt,
		  bool json_float = false, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_rsv_fp_decision_impl(
	char_type *iter, ::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10,
	::std::int_least32_t e10) noexcept
{
	if constexpr (mt == ::fast_io::manipulators::floating_format::general)
	{
		auto const length{static_cast<::std::int_least32_t>(
			chars_len<10, true>(m10))};
		auto const scientific_exponent{
			static_cast<::std::int_least32_t>(e10 + length - 1)};
		/*
		This is the same -4<=X<6 theorem as the length-carrying overload.
		Computing L here rather than reusing e10 is necessary: canonicalization
		may remove any number of coefficient zeroes while preserving both the
		value and X.
		*/
		if (-4 <= scientific_exponent && scientific_exponent < 6)
		{
			return print_rsv_fp_fixed_decision_impl<flt, comma, json_float>(iter, m10, e10);
		}
		return print_rsv_fp_decision_impl<flt, comma, uppercase_e,
										  ::fast_io::manipulators::floating_format::scientific,
										  false>(iter, m10, e10);
	}
	else if constexpr (mt == ::fast_io::manipulators::floating_format::scientific)
	{
		if (m10 < 10u) [[unlikely]]
		{
			*iter = ::fast_io::char_literal_add<char_type>(static_cast<::std::uint_least32_t>(m10));
			++iter;
		}
		else
		{
			auto iterp1{iter};
			++iterp1;
			auto new_iter{::fast_io::details::print_rsv_fp_digits<flt>(iterp1, m10)};
			e10 += static_cast<::std::int_least32_t>(static_cast<::std::uint_least32_t>(new_iter - iterp1) - 1u);
			*iter = *iterp1;
			*iterp1 = char_literal_v < comma ? u8',' : u8'.', char_type > ;
			iter = new_iter;
		}
		return print_rsv_fp_e_impl<flt, uppercase_e>(iter, e10);
	}
	else
	{
		::std::int_least32_t olength{static_cast<::std::int_least32_t>(chars_len<10, true>(m10))};
		::std::int_least32_t const real_exp{static_cast<::std::int_least32_t>(e10 + olength - 1)};
		::std::uint_least32_t fixed_length{};
		if (olength <= real_exp)
		{
			fixed_length = static_cast<::std::uint_least32_t>(real_exp + 1);
		}
		else if (0 <= real_exp && real_exp < olength)
		{
			fixed_length = static_cast<::std::uint_least32_t>(olength + 2);
			if (olength == real_exp + 1)
			{
				--fixed_length;
			}
		}
		else
		{
			fixed_length = static_cast<::std::uint_least32_t>(static_cast<::std::uint_least32_t>(-real_exp) +
															  static_cast<::std::uint_least32_t>(olength) + 1u);
		}
		auto const scientific_length{
			::fast_io::details::print_rsv_fp_scientific_length(
				real_exp, static_cast<::std::size_t>(olength))};
		if (scientific_length < fixed_length)
		{
			iter = print_rsv_fp_decimal_common_impl<comma>(iter, m10, static_cast<::std::uint_least32_t>(olength));
			return print_rsv_fp_e_impl<flt, uppercase_e>(iter, real_exp);
		}
		if (olength <= real_exp)
		{
			return fixed_case0_full_integer_maybe_json<flt, comma, json_float>(iter, m10, olength, real_exp);
		}
		else if (0 <= real_exp)
		{
			return fixed_case1_integer_and_point_maybe_json<flt, comma, json_float>(iter, m10, olength, real_exp);
		}
		else
		{
			return fixed_case2_all_point<flt, comma>(iter, m10, olength, real_exp);
		}
	}
}

// Compatibility alias for existing precision and decfloat call sites.  It
// refers to the generated DA storage and introduces no second power table.
// This alias shares numeric data only; format-specific rendering remains in
// the individual scientific, fixed and decimal writers.
inline constexpr auto const &print_rsv_fp_pow10_0_to_19_table{
	::fast_io::details::da::uint64_power10_table};

// MSVC x64 has the two-word multiplication primitives required by the proved
// P16--P17 DA carrier even though it has no language-level unsigned 128-bit
// scalar.  Keep the runtime selector in one non-inlined COMDAT so the two
// precision cases are not cloned into every presentation and precision mode.
// A failed interval proof writes nothing and leaves the limb-based exact
// formatter authoritative; success contains exactly `significant` digits and
// the scientific exponent of their leading digit.
//
// The closed P16--P17 range is intentional.  MSVC 19.51 Compiler Explorer
// differential runs accepted 2.2 million values across all four decimal
// presentations, both significant modes and all six nearest policies.  The
// attempted P18--P19 extension did not preserve the byte stream and was
// rejected; those precisions therefore retain the exact fallback until their
// complete carrier-to-character boundary has an independent proof.
#if defined(_MSC_VER) && defined(_M_X64) && !defined(__clang__) && \
	!defined(__SIZEOF_INT128__) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
[[msvc::noinline]]
[[nodiscard]] inline constexpr ::fast_io::details::da::binary64_scientific_precision_result
binary64_scientific_precision_msvc_runtime(
	::std::uint_least64_t mantissa, ::std::uint_least32_t exponent,
	::std::size_t significant) noexcept
{
	constexpr ::std::uint_least64_t implicit_bit{
		static_cast<::std::uint_least64_t>(1u) << 52u};
	auto const binary_significand{mantissa | implicit_bit};
	switch (significant)
	{
	case 16u:
		return ::fast_io::details::da::
			compute_binary64_scientific_precision<16u>(binary_significand, exponent);
	case 17u:
		return ::fast_io::details::da::
			compute_binary64_scientific_precision<17u>(binary_significand, exponent);
	default:
		return {};
	}
}
#endif

// This precision specialization is attached to the native-u128 exact backend:
// P20-P38 results require u128 coefficients, while
// P16-P19 share the same dispatch and fallback infrastructure.  All four are
// mathematically representable in u64; the separately proved MSVC-x64 P16-P17
// carrier above is the only no-u128 exception.  P18-P19 and every other target
// without native u128 retain exact materialization.  Wide carriers must never
// be enabled by narrowing their coefficient.
// The prefix-window backend uses u128 limbs to retain exactly the requested
// decimal prefix, one guard digit and sticky state.  When u128 is unavailable,
// the enclosing floating implementation keeps the older exact expansion; this
// availability split changes representation only, never rounding policy.
#if defined(__SIZEOF_INT128__)
template <::std::size_t digits, bool comma, bool uppercase_e,
	::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
print_rsvflt_binary64_scientific_precision_impl(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t exponent) noexcept
{
	static_assert(16u <= digits && digits <= 19u);
	constexpr ::std::uint_least64_t implicit_bit{static_cast<::std::uint_least64_t>(1ULL) << 52u};
	// The fixed-width proof uses the normalized 53-bit significand.
	// Subnormal values retain the exact-window fallback until their wider
	// coefficient range has a separate compile-time normalization proof; the
	// caller checks that domain before entering this normal-only helper.
	auto const binary_significand{mantissa | implicit_bit};
	auto const converted{::fast_io::details::da::
		compute_binary64_scientific_precision<digits>(binary_significand, exponent)};
	if (!converted.success)
	{
		return nullptr;
	}
	// P16, P17 and P18 share the same 18-digit primitive and punctuation shape;
	// appended zeroes occupy scratch later overwritten by the exponent.  P19
	// fills all nineteen coefficient positions and therefore needs a distinct
	// offset.  Sharing a primitive does not imply that compilers merge template
	// text: a runtime P16/P17 body was rejected because it retained scale/length
	// values and lengthened the hot dependency chain in both M4 and x86 audits.
	if constexpr (digits < 19u)
	{
		constexpr ::std::uint_least64_t scale{digits == 16u ? 100u :
			(digits == 17u ? 10u : 1u)};
		::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
			iter, converted.significand * scale, 18u);
	}
	else
	{
		::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
			iter + 1u, converted.significand, 19u);
	}
	*iter = iter[1];
	iter[1] = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	return ::fast_io::details::print_rsv_fp_e_impl<double, uppercase_e>(
		iter + digits + 1u, converted.exponent);
}

// One outlined runtime entry prevents the fixed-point core from being copied by
// every precision-mode and rounding-policy instantiation.  Its scalar arguments
// avoid an aggregate return/copy at the boundary; each selected P16-P19 case is
// still compile-time specialized inside.  noinline is therefore a text-size and
// live-range boundary, not part of rounding semantics.
template <bool comma, bool uppercase_e, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *print_rsvflt_binary64_scientific_precision_runtime_impl(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t exponent, ::std::size_t significant) noexcept
{
	switch (significant)
	{
	case 16u:
		return ::fast_io::details::
			print_rsvflt_binary64_scientific_precision_impl<16u, comma, uppercase_e>(
				iter, mantissa, exponent);
	case 17u:
		return ::fast_io::details::
			print_rsvflt_binary64_scientific_precision_impl<17u, comma, uppercase_e>(
				iter, mantissa, exponent);
	case 18u:
		return ::fast_io::details::
			print_rsvflt_binary64_scientific_precision_impl<18u, comma, uppercase_e>(
				iter, mantissa, exponent);
	default:
		return ::fast_io::details::
			print_rsvflt_binary64_scientific_precision_impl<19u, comma, uppercase_e>(
				iter, mantissa, exponent);
	}
}

// The Linux x86 Clang policies in this precision section were audited through
// isolated scientific-preserve and scientific-significant consumers on Compiler
// Explorer Clang 16, 17, 18, 19, 20, 21, 22 and current trunk Clang 24. Every
// tested compiler changes the complete target instructions for each forced
// Clang-23 policy: P16--P19 outer placement, P20--P33 selection/runtime body,
// P34 placement, direct-negative and aligned block streams, the long stream, and
// P20--P22 direct emission. Thus the exact Clang-23 predicates below are measured
// code-generation transitions; no supported intermediate release was skipped.
//
// Placement is a code-generation policy, not part of DA correctness. Linux
// System V x86-64 LP64 Clang 23 and GCC 13 or later probe P16-P19 before the
// large exact-window prologue so a successful carrier does not inherit its frame
// and live ranges.  Other x86 ABIs and compiler majors keep the probe inside
// that window, as does AArch64, where cache-base and fallback state are already
// live. Both placements compute the same carrier. Compiler Explorer complete-
// dispatch probes for Clang 22 and trunk Clang 24 are not instruction-identical:
// each forced body adds one call despite removing one instruction. Clang therefore
// remains an exact measured transition; GNU uses its continuous lower bound.
// Moving either requires hit/miss frame, spill, call, branch and linked-text evidence.
inline constexpr bool binary64_scientific_precision_outer_dispatch{
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	((defined(__clang__) && __clang_major__ == 23) || \
	 (defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__)) && \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC))
	true
#else
	false
#endif
};

// Apple Clang 23 on M4 places the complete P20-P33 probes and the P34
// equality probe behind the existing direct-scientific gate so P1-P19 retain
// their entry shape and cache-base live range.
// The Apple-AArch64 condition intentionally applies this measured family
// policy to later Apple frontends; that is a conservative code-generation
// hypothesis, not a new correctness domain.  Other targets probe before exact
// block materialization.  Arithmetic and fallback are identical; validate
// hit/miss branch layout, register residency, spills and calls when moving it.
inline constexpr bool binary64_scientific_wide_precision_direct_block{
#if defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	true
#else
	false
#endif
};

// Clang 23 Linux System V x86-64 LP64 retains scale and writer length efficiently
// as runtime values, so one P20-P33 body avoids fourteen template copies.  Every
// other compiler/ABI keeps constant P20-P24 cases and a shared P25-P33 tail where
// compile-time scales and widths remove address/length work.  Both forms use the
// same proved carrier and shared data.  Revalidate text size, front-end pressure,
// dependency chains, spills and calls before changing the exact transition.
// In the isolated scientific-preserve consumer, Clang 22's selected body has 17
// calls and its runtime body has 23, versus 16 in the fallback; trunk Clang 24
// likewise emits a different complete instruction stream.
inline constexpr bool binary64_scientific_wide_precision_runtime_is_extended{
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__clang__) && __clang_major__ == 23 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	true
#else
	false
#endif
};

// Linux LP64 GNU C++ 13--16 leaves a second numeric-to-character copy in the
// compact significant fixed/decimal path.  Direct P64--P128 block streaming
// removes that copy and measured progressively larger wins on all four
// frontends; Clang 18--23 and Apple AArch64 already optimize the compact copy to
// parity, so the extra probe and outlined bodies only increase text there.
// GCC 13 is the measured continuous lower bound; later GNU frontends inherit
// the GCC-16 direct-stream policy.  Because the
// dispatch is an `if constexpr`, Apple M-series and every other excluded target
// instantiate the unchanged fallback and are byte-identical to the baseline;
// this was checked with Apple Clang 21, Clang 23 and GCC 15 on Apple AArch64.
// The arithmetic helper itself is target independent.  Widen only with
// hit/miss latency, frame, call-graph and linked-text-size evidence before
// moving that lower bound or extending it to another ABI.
inline constexpr bool binary64_significant_fixed_direct_block{
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__ && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	true
#else
	false
#endif
};
#endif

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool print_rsv_fp_decimal_tie_round_up(
	bool negative, ::std::uint_least64_t rounded_down) noexcept
{
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
[[nodiscard]] inline constexpr bool print_rsv_fp_decimal_round_up(bool negative, ::std::uint_least64_t rounded_down,
																  ::std::uint_least64_t remainder,
																  ::std::uint_least64_t divisor) noexcept
{
	if (!remainder)
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
		if (half < remainder)
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

using exact_precision_limb_type = ::std::uint_least32_t;
using exact_precision_wide_type = ::std::uint_least64_t;
using exact_precision_multiplier_type = ::std::uint_least64_t;
inline constexpr exact_precision_multiplier_type exact_precision_limb_base{1000000000u};
inline constexpr unsigned exact_precision_limb_digits{9u};
inline constexpr unsigned exact_precision_pow5_chunk{14u};
inline constexpr exact_precision_multiplier_type exact_precision_pow5_multiplier{6103515625ull};
inline constexpr unsigned exact_precision_pow2_chunk{34u};
inline constexpr exact_precision_multiplier_type exact_precision_pow2_multiplier{17179869184ull};

[[nodiscard]] inline constexpr bool exact_precision_multiplier_fits(
	exact_precision_multiplier_type multiplier) noexcept
{
	constexpr auto maximum{(::std::numeric_limits<exact_precision_wide_type>::max)()};
	return multiplier <=
		(maximum - (multiplier - 1u)) / (exact_precision_limb_base - 1u);
}

static_assert(::fast_io::details::exact_precision_multiplier_fits(exact_precision_pow5_multiplier));
static_assert(::fast_io::details::exact_precision_multiplier_fits(exact_precision_pow2_multiplier));

template <typename flt>
inline constexpr ::std::size_t exact_precision_limb_capacity{
	sizeof(flt) <= sizeof(float)
		? 16u
		: (::fast_io::details::iec559_traits<flt>::mbits <=
				  ::fast_io::details::iec559_traits<double>::mbits &&
			   ::fast_io::details::iec559_traits<flt>::ebits <=
				  ::fast_io::details::iec559_traits<double>::ebits
			   ? 88u
			   : []() constexpr noexcept {
					 using trait = ::fast_io::details::iec559_traits<flt>;
					 constexpr ::std::size_t bias{
						 (static_cast<::std::size_t>(1u) << (trait::ebits - 1u)) - 1u};
					 constexpr ::std::size_t denominator_power{
						 bias + trait::mbits - 1u};
					 /*
					 The smallest subnormal is 2^-(bias+mbits-1), so its exact
					 decimal coefficient is at most (2^(mbits+1)-1)*5^k.  The
					 strict rational bounds log10(5) < 7/10 and log10(2) < 1/3
					 give a compile-time upper bound without floating arithmetic.
					 Two guard digits and one guard limb cover both ceilings.  For
					 binary80 and binary128 this yields 1283 and 1289 limbs,
					 respectively, enough for every finite exact expansion.
					 */
					 constexpr ::std::size_t coefficient_digits{
						 (denominator_power * 7u + 9u) / 10u +
						 (trait::mbits + 3u) / 3u + 2u};
					 return (coefficient_digits + exact_precision_limb_digits - 1u) /
							   exact_precision_limb_digits +
						   1u;
				   }())};

template <typename flt>
inline constexpr ::std::size_t exact_precision_digit_capacity{
	exact_precision_limb_capacity<flt> * exact_precision_limb_digits};

static_assert(exact_precision_digit_capacity<float> >= 114u);
static_assert(exact_precision_digit_capacity<double> >= 768u);

template <typename flt>
inline constexpr bool exact_precision_is_wide_binary{
	::fast_io::details::iec559_traits<double>::mbits <
		::fast_io::details::iec559_traits<flt>::mbits ||
	::fast_io::details::iec559_traits<double>::ebits <
		::fast_io::details::iec559_traits<flt>::ebits};

template <typename flt>
struct exact_precision_decimal
{
	unsigned char digits[exact_precision_digit_capacity<flt>];
	::std::size_t size;
	::std::int_least32_t exponent;
};

// A directed tiny-value result needs only one coefficient digit.  This compact
// view satisfies the established decimal presentation contract without
// allocating the complete binary80/binary128 exact-expansion buffer.
struct exact_precision_single_digit_decimal
{
	unsigned char digits[1u];
	::std::size_t size;
	::std::int_least32_t exponent;
};

inline constexpr void exact_precision_multiply_small(
	exact_precision_limb_type *limbs, ::std::size_t &size, exact_precision_multiplier_type multiplier) noexcept
{
	exact_precision_wide_type carry{};
	for (::std::size_t i{}; i != size; ++i)
	{
		auto const product{static_cast<exact_precision_wide_type>(limbs[i]) * multiplier + carry};
		limbs[i] = static_cast<exact_precision_limb_type>(product % exact_precision_limb_base);
		carry = product / exact_precision_limb_base;
	}
	for (; carry; carry /= exact_precision_limb_base)
	{
		limbs[size] = static_cast<exact_precision_limb_type>(carry % exact_precision_limb_base);
		++size;
	}
}

inline constexpr void exact_precision_add_small(
	exact_precision_limb_type *limbs, ::std::size_t &size,
	exact_precision_multiplier_type addend) noexcept
{
	exact_precision_wide_type carry{addend};
	for (::std::size_t index{}; carry && index != size; ++index)
	{
		auto const sum{static_cast<exact_precision_wide_type>(limbs[index]) + carry};
		limbs[index] = static_cast<exact_precision_limb_type>(sum % exact_precision_limb_base);
		carry = sum / exact_precision_limb_base;
	}
	for (; carry; carry /= exact_precision_limb_base)
	{
		limbs[size++] = static_cast<exact_precision_limb_type>(carry % exact_precision_limb_base);
	}
}

template <typename mantissa_type>
inline constexpr void exact_precision_initialize_wide_limbs(
	exact_precision_limb_type *limbs, ::std::size_t &size,
	mantissa_type mantissa) noexcept
{
	static_assert(sizeof(exact_precision_multiplier_type) < sizeof(mantissa_type));
	/*
	 A native binary128 mantissa must not use `% 1000000000` or
	 `/ 1000000000`: both operations lower to __umodti3/__udivti3 on
	 current GCC and Clang x86-64 runtimes.  Horner conversion in four
	 base-2^32 words is exact because the base-1e9 accumulator already
	 proves multiplication by 2^32 fits its uint64_t product.  It has a
	 fixed four-word cost and introduces no target-specific intrinsic.
	 */
	constexpr unsigned word_bits{32u};
	constexpr auto word_mask{static_cast<mantissa_type>(0xffffffffu)};
	constexpr exact_precision_multiplier_type word_base{
		static_cast<exact_precision_multiplier_type>(1u) << word_bits};
	constexpr auto word_count{
		(sizeof(mantissa_type) * ::std::numeric_limits<unsigned char>::digits +
		 word_bits - 1u) /
		word_bits};
	size = 1u;
	for (::std::size_t index{word_count}; index; --index)
	{
		::fast_io::details::exact_precision_multiply_small(limbs, size, word_base);
		auto const word{static_cast<exact_precision_multiplier_type>(
			(mantissa >> ((index - 1u) * word_bits)) & word_mask)};
		::fast_io::details::exact_precision_add_small(limbs, size, word);
	}
	while (1u < size && !limbs[size - 1u])
	{
		--size;
	}
}

[[nodiscard]] inline constexpr exact_precision_multiplier_type exact_precision_small_power(
	exact_precision_multiplier_type base, unsigned exponent) noexcept
{
	exact_precision_multiplier_type result{1u};
	for (; exponent; --exponent)
	{
		result *= base;
	}
	return result;
}

/*
For a wide binary value with a large positive exponent, the old exact path
started at the significand and multiplied the complete base-1e9 accumulator by
2^34 once for every chunk.  binary80/binary128 can need 478 such chunks.  The
cost is quadratic because the accumulator grows at each multiplication.

The small table below supplies every 64th chunk (2^2176).  It is deliberately
stored in the existing base-1e9 representation: after copying one anchor we
only have to multiply its limbs by the at-most-128-bit significand and process
the remaining 0..63 chunks.  Eight entries cover the full finite binary80 and
binary128 exponent domains and occupy about 40 KiB per instantiated wide type.
Unlike a table of all exponents, this is small enough for a header-only path
while removing the overwhelming majority of the growing multiplications.
*/
inline constexpr unsigned exact_precision_pow2_anchor_chunk_count{64u};

template <typename flt>
struct exact_precision_wide_pow2_anchor_table
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	constexpr static ::std::int_least32_t bias{
		(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
	constexpr static ::std::int_least32_t maximum_binary_exponent{
		(static_cast<::std::int_least32_t>(1u) << trait::ebits) - 2 - bias -
		static_cast<::std::int_least32_t>(trait::mbits)};
	constexpr static ::std::size_t maximum_chunk_count{
		(static_cast<::std::size_t>(maximum_binary_exponent) +
		 exact_precision_pow2_chunk - 1u) /
		exact_precision_pow2_chunk};
	constexpr static ::std::size_t extent{
		maximum_chunk_count / exact_precision_pow2_anchor_chunk_count + 1u};

	exact_precision_limb_type
		limbs[extent][exact_precision_limb_capacity<flt>]{};
	::std::size_t sizes[extent]{};

	inline exact_precision_wide_pow2_anchor_table() noexcept
	{
		limbs[0u][0u] = 1u;
		sizes[0u] = 1u;
		for (::std::size_t entry{1u}; entry != extent; ++entry)
		{
			auto const previous_size{sizes[entry - 1u]};
			for (::std::size_t index{}; index != previous_size; ++index)
			{
				limbs[entry][index] = limbs[entry - 1u][index];
			}
			sizes[entry] = previous_size;
			for (unsigned chunk{};
				 chunk != exact_precision_pow2_anchor_chunk_count; ++chunk)
			{
				::fast_io::details::exact_precision_multiply_small(
					limbs[entry], sizes[entry],
					exact_precision_pow2_multiplier);
			}
		}
	}
};

template <typename flt>
[[nodiscard]] inline exact_precision_wide_pow2_anchor_table<flt> const &
exact_precision_wide_pow2_anchor_table_instance() noexcept
{
	/*
	Keep the table runtime-lazy.  Eager constexpr construction exceeds the
	default evaluator step budget in GCC when a translation unit instantiates
	many independent precision frontends.  The table is built once only after a
	wide positive-exponent fallback is actually reached; normal short-precision
	window users pay neither initialization nor code-generation cost.  Constant
	evaluation deliberately stays on the pre-existing exact loop below.
	*/
	static exact_precision_wide_pow2_anchor_table<flt> const table{};
	return table;
}

/*
The exact-decimal scalar mode can request every digit of binary80/binary128.
For a minimum binary128 subnormal, growing a base-1e9 accumulator through all
1178 multiplications by 5^14 is quadratic in the 11,529-digit result.  Anchors
remain two chunks apart, so lookup leaves at most one full 5^14 multiplication.

Building every dense anchor on the first extreme conversion cost about 1.2 ms
on the measured x86-64 target.  Use three tiers instead:

  * exponents below 896 retain the compact per-conversion loop;
  * the first 64 dense anchors (896..2660) are constant-initialized;
  * later ranges have constant-initialized segment seeds and independently
    lazy 8-anchor dense blocks.

Each seed is derived from the preceding constexpr seed in a separate constant
evaluation.  This bounds GCC's per-initializer evaluator work while avoiding a
generated numeric source table.  A first request for a very high exponent now
constructs only its 8-anchor block, not all preceding blocks.  The ragged
blocks and seeds are shared by binary80, binary128 and IBM double-double.

This table is intentionally separate from exact_precision_from_binary.  That
function remains the rounding-precision fallback used by existing modes; only
the new exact-decimal proxy opts into this time/memory tradeoff.
*/
inline constexpr unsigned exact_decimal_pow5_anchor_chunk_count{2u};
inline constexpr unsigned exact_decimal_pow5_anchor_stride{
	exact_precision_pow5_chunk * exact_decimal_pow5_anchor_chunk_count};
inline constexpr unsigned exact_decimal_pow5_anchor_minimum_exponent{
	exact_precision_pow5_chunk * 64u};
inline constexpr unsigned exact_decimal_pow5_anchor_maximum_exponent{16494u};
inline constexpr ::std::size_t exact_decimal_pow5_anchor_first_index{
	exact_decimal_pow5_anchor_minimum_exponent /
	exact_decimal_pow5_anchor_stride};
inline constexpr ::std::size_t exact_decimal_pow5_anchor_last_index{
	exact_decimal_pow5_anchor_maximum_exponent /
	exact_decimal_pow5_anchor_stride};
inline constexpr ::std::size_t exact_decimal_pow5_anchor_extent{
	exact_decimal_pow5_anchor_last_index -
	exact_decimal_pow5_anchor_first_index + 1u};
inline constexpr ::std::size_t exact_decimal_pow5_hot_anchor_extent{64u};
inline constexpr ::std::size_t exact_decimal_pow5_runtime_first_index{
	exact_decimal_pow5_anchor_first_index +
	exact_decimal_pow5_hot_anchor_extent};
inline constexpr ::std::size_t exact_decimal_pow5_runtime_segment_extent{8u};
inline constexpr ::std::size_t exact_decimal_pow5_runtime_anchor_extent{
	exact_decimal_pow5_anchor_last_index -
	exact_decimal_pow5_runtime_first_index + 1u};
inline constexpr ::std::size_t exact_decimal_pow5_runtime_segment_count{
	(exact_decimal_pow5_runtime_anchor_extent +
	 exact_decimal_pow5_runtime_segment_extent - 1u) /
	exact_decimal_pow5_runtime_segment_extent};

static_assert(exact_decimal_pow5_anchor_minimum_exponent %
				  exact_decimal_pow5_anchor_stride ==
			  0u);

[[nodiscard]] inline constexpr ::std::size_t
exact_decimal_pow5_anchor_limb_bound(::std::size_t anchor_index) noexcept
{
	/* log10(5) < 7/10.  The extra digit keeps the bound strict at an
	integer product before conversion to base-1e9 limbs. */
	auto const exponent{anchor_index * exact_decimal_pow5_anchor_stride};
	auto const digits{(exponent * 7u + 9u) / 10u + 1u};
	return (digits + exact_precision_limb_digits - 1u) /
		   exact_precision_limb_digits;
}

template <::std::size_t first_index, ::std::size_t extent>
[[nodiscard]] inline consteval ::std::size_t
exact_decimal_pow5_anchor_block_flat_capacity() noexcept
{
	::std::size_t result{};
	for (::std::size_t entry{}; entry != extent; ++entry)
	{
		result += ::fast_io::details::
			exact_decimal_pow5_anchor_limb_bound(first_index + entry);
	}
	return result;
}

template <::std::size_t extent>
struct exact_decimal_pow5_hot_anchor_table
{
	inline static constexpr ::std::size_t flat_capacity{
		::fast_io::details::exact_decimal_pow5_anchor_block_flat_capacity<
			exact_decimal_pow5_anchor_first_index, extent>()};
	exact_precision_limb_type limbs[flat_capacity]{};
	::std::size_t offsets[extent]{};
	::std::size_t sizes[extent]{};

	inline constexpr exact_decimal_pow5_hot_anchor_table() noexcept
	{
		limbs[0u] = 1u;
		sizes[0u] = 1u;
		for (::std::size_t chunk{};
			 chunk != exact_decimal_pow5_anchor_first_index *
						  exact_decimal_pow5_anchor_chunk_count;
			 ++chunk)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, sizes[0u], exact_precision_pow5_multiplier);
		}
		for (::std::size_t entry{1u}; entry != extent; ++entry)
		{
			offsets[entry] = offsets[entry - 1u] +
							 ::fast_io::details::exact_decimal_pow5_anchor_limb_bound(
								 exact_decimal_pow5_anchor_first_index + entry - 1u);
			auto *const destination{limbs + offsets[entry]};
			auto const *const source{limbs + offsets[entry - 1u]};
			auto const previous_size{sizes[entry - 1u]};
			for (::std::size_t index{}; index != previous_size; ++index)
			{
				destination[index] = source[index];
			}
			sizes[entry] = previous_size;
			for (unsigned chunk{};
				 chunk != exact_decimal_pow5_anchor_chunk_count; ++chunk)
			{
				::fast_io::details::exact_precision_multiply_small(
					destination, sizes[entry],
					exact_precision_pow5_multiplier);
			}
		}
	}
};

struct exact_decimal_pow5_shared_storage_tag
{};

template <typename storage_tag>
struct exact_decimal_pow5_anchor_storage
{
	inline static constexpr exact_decimal_pow5_hot_anchor_table<
		exact_decimal_pow5_hot_anchor_extent>
		hot{};
};

template <typename flt>
struct exact_decimal_pow5_compact_anchor_storage
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	inline static constexpr ::std::size_t maximum_denominator_power{
		(static_cast<::std::size_t>(1u) << (trait::ebits - 1u)) - 1u +
		trait::mbits - 1u};
	inline static constexpr ::std::size_t extent{
		maximum_denominator_power / exact_decimal_pow5_anchor_stride -
		exact_decimal_pow5_anchor_first_index + 1u};
	inline static constexpr exact_decimal_pow5_hot_anchor_table<extent> hot{};
};

template <::std::size_t anchor_index>
struct exact_decimal_pow5_anchor_seed
{
	inline static constexpr ::std::size_t capacity{
		::fast_io::details::exact_decimal_pow5_anchor_limb_bound(anchor_index)};
	exact_precision_limb_type limbs[capacity]{};
	::std::size_t size{};
};

template <typename storage_tag, ::std::size_t segment>
struct exact_decimal_pow5_anchor_seed_holder
{
	inline static constexpr ::std::size_t anchor_index{
		exact_decimal_pow5_runtime_first_index +
		segment * exact_decimal_pow5_runtime_segment_extent};
	using seed_type = exact_decimal_pow5_anchor_seed<anchor_index>;

	[[nodiscard]] inline static consteval seed_type make() noexcept
	{
		seed_type result{};
		::std::size_t previous_index{};
		if constexpr (segment == 0u)
		{
			previous_index = exact_decimal_pow5_runtime_first_index - 1u;
			constexpr auto previous_entry{
				exact_decimal_pow5_hot_anchor_extent - 1u};
			auto const previous_size{
				exact_decimal_pow5_anchor_storage<storage_tag>::hot
					.sizes[previous_entry]};
			auto const *const previous{
				exact_decimal_pow5_anchor_storage<storage_tag>::hot.limbs +
				exact_decimal_pow5_anchor_storage<storage_tag>::hot
					.offsets[previous_entry]};
			for (::std::size_t index{}; index != previous_size; ++index)
			{
				result.limbs[index] = previous[index];
			}
			result.size = previous_size;
		}
		else
		{
			previous_index =
				exact_decimal_pow5_anchor_seed_holder<
					storage_tag, segment - 1u>::anchor_index;
			auto const &previous{
				exact_decimal_pow5_anchor_seed_holder<
					storage_tag, segment - 1u>::value};
			for (::std::size_t index{}; index != previous.size; ++index)
			{
				result.limbs[index] = previous.limbs[index];
			}
			result.size = previous.size;
		}
		for (::std::size_t chunk{};
			 chunk != (anchor_index - previous_index) *
						  exact_decimal_pow5_anchor_chunk_count;
			 ++chunk)
		{
			::fast_io::details::exact_precision_multiply_small(
				result.limbs, result.size,
				exact_precision_pow5_multiplier);
		}
		return result;
	}

	inline static constexpr seed_type value{make()};
};

struct exact_decimal_pow5_anchor_view
{
	exact_precision_limb_type const *limbs{};
	::std::size_t size{};
};

struct exact_decimal_pow5_constexpr_anchor_view
{
	exact_decimal_pow5_anchor_view anchor{};
	::std::size_t anchor_index{};
};

template <typename storage_tag, ::std::size_t segment>
[[nodiscard]] inline constexpr exact_decimal_pow5_constexpr_anchor_view
exact_decimal_pow5_constexpr_seed_lookup() noexcept
{
	auto const &seed{
		::fast_io::details::exact_decimal_pow5_anchor_seed_holder<
			storage_tag, segment>::value};
	return {{seed.limbs, seed.size},
			::fast_io::details::exact_decimal_pow5_anchor_seed_holder<
				storage_tag, segment>::anchor_index};
}

using exact_decimal_pow5_constexpr_seed_lookup_function =
	exact_decimal_pow5_constexpr_anchor_view (*)() noexcept;

template <typename storage_tag, typename sequence>
struct exact_decimal_pow5_constexpr_seed_dispatch;

template <typename storage_tag, ::std::size_t... segments>
struct exact_decimal_pow5_constexpr_seed_dispatch<
	storage_tag, ::std::index_sequence<segments...>>
{
	inline static constexpr exact_decimal_pow5_constexpr_seed_lookup_function
		functions[]{
			::fast_io::details::exact_decimal_pow5_constexpr_seed_lookup<
				storage_tag, segments>...};
};

template <typename storage_tag>
[[nodiscard]] inline constexpr exact_decimal_pow5_constexpr_anchor_view
exact_decimal_pow5_constexpr_anchor_lookup(
	::std::size_t anchor_index) noexcept
{
	if (anchor_index < exact_decimal_pow5_runtime_first_index)
	{
		auto const entry{
			anchor_index - exact_decimal_pow5_anchor_first_index};
		return {{exact_decimal_pow5_anchor_storage<storage_tag>::hot.limbs +
					 exact_decimal_pow5_anchor_storage<storage_tag>::hot.offsets[entry],
				 exact_decimal_pow5_anchor_storage<storage_tag>::hot.sizes[entry]},
				anchor_index};
	}
	auto const segment{
		(anchor_index - exact_decimal_pow5_runtime_first_index) /
		exact_decimal_pow5_runtime_segment_extent};
	using dispatch = ::fast_io::details::exact_decimal_pow5_constexpr_seed_dispatch<
		storage_tag,
		::std::make_index_sequence<exact_decimal_pow5_runtime_segment_count>>;
	return dispatch::functions[segment]();
}

/* All lazy blocks perform the same copy-and-grow operation.  Keeping that
runtime-only work in one outlined helper prevents every function-local-static
wrapper from cloning the limb loop into exact-only text. */
template <typename storage_tag>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline void exact_decimal_pow5_initialize_runtime_anchor_block(
	exact_precision_limb_type *limbs, ::std::size_t *offsets,
	::std::size_t *sizes, ::std::size_t first_index, ::std::size_t extent,
	exact_precision_limb_type const *seed, ::std::size_t seed_size) noexcept
{
	::fast_io::details::non_overlapped_copy_n(seed, seed_size, limbs);
	sizes[0u] = seed_size;
	for (::std::size_t entry{1u}; entry != extent; ++entry)
	{
		offsets[entry] = offsets[entry - 1u] +
						 ::fast_io::details::exact_decimal_pow5_anchor_limb_bound(
							 first_index + entry - 1u);
		auto *const destination{limbs + offsets[entry]};
		auto const *const source{limbs + offsets[entry - 1u]};
		auto const previous_size{sizes[entry - 1u]};
		::fast_io::details::non_overlapped_copy_n(
			source, previous_size, destination);
		sizes[entry] = previous_size;
		for (unsigned chunk{};
			 chunk != exact_decimal_pow5_anchor_chunk_count; ++chunk)
		{
			::fast_io::details::exact_precision_multiply_small(
				destination, sizes[entry], exact_precision_pow5_multiplier);
		}
	}
}

template <typename storage_tag, ::std::size_t segment>
struct exact_decimal_pow5_runtime_anchor_block
{
	inline static constexpr ::std::size_t first_index{
		exact_decimal_pow5_runtime_first_index +
		segment * exact_decimal_pow5_runtime_segment_extent};
	inline static constexpr ::std::size_t remaining{
		exact_decimal_pow5_anchor_last_index - first_index + 1u};
	inline static constexpr ::std::size_t extent{
		remaining < exact_decimal_pow5_runtime_segment_extent
			? remaining
			: exact_decimal_pow5_runtime_segment_extent};
	inline static constexpr ::std::size_t flat_capacity{
		::fast_io::details::exact_decimal_pow5_anchor_block_flat_capacity<
			first_index, extent>()};
	exact_precision_limb_type limbs[flat_capacity]{};
	::std::size_t offsets[extent]{};
	::std::size_t sizes[extent]{};

	inline exact_decimal_pow5_runtime_anchor_block() noexcept
	{
		auto const &seed{
			::fast_io::details::exact_decimal_pow5_anchor_seed_holder<
				storage_tag, segment>::value};
		::fast_io::details::
			exact_decimal_pow5_initialize_runtime_anchor_block<storage_tag>(
				limbs, offsets, sizes, first_index, extent, seed.limbs, seed.size);
	}
};

template <typename storage_tag, ::std::size_t segment>
[[nodiscard]] inline exact_decimal_pow5_anchor_view
exact_decimal_pow5_runtime_anchor_lookup(::std::size_t anchor_index) noexcept
{
	static exact_decimal_pow5_runtime_anchor_block<storage_tag, segment> const
		block{};
	auto const entry{anchor_index - block.first_index};
	return {block.limbs + block.offsets[entry], block.sizes[entry]};
}

using exact_decimal_pow5_runtime_anchor_lookup_function =
	exact_decimal_pow5_anchor_view (*)(::std::size_t) noexcept;

template <typename storage_tag, typename sequence>
struct exact_decimal_pow5_runtime_anchor_dispatch;

template <typename storage_tag, ::std::size_t... segments>
struct exact_decimal_pow5_runtime_anchor_dispatch<
	storage_tag, ::std::index_sequence<segments...>>
{
	inline static constexpr exact_decimal_pow5_runtime_anchor_lookup_function functions[]{
		::fast_io::details::exact_decimal_pow5_runtime_anchor_lookup<
			storage_tag, segments>...};
};

template <typename storage_tag>
using exact_decimal_pow5_runtime_anchor_dispatch_type =
	exact_decimal_pow5_runtime_anchor_dispatch<storage_tag,
											   ::std::make_index_sequence<
												   exact_decimal_pow5_runtime_segment_count>>;

template <typename storage_tag>
[[nodiscard]] inline exact_decimal_pow5_anchor_view
exact_decimal_pow5_anchor_lookup(::std::size_t anchor_index) noexcept
{
	if (anchor_index < exact_decimal_pow5_runtime_first_index)
	{
		auto const entry{
			anchor_index - exact_decimal_pow5_anchor_first_index};
		return {exact_decimal_pow5_anchor_storage<storage_tag>::hot.limbs +
					exact_decimal_pow5_anchor_storage<storage_tag>::hot.offsets[entry],
				exact_decimal_pow5_anchor_storage<storage_tag>::hot.sizes[entry]};
	}
	auto const segment{
		(anchor_index - exact_decimal_pow5_runtime_first_index) /
		exact_decimal_pow5_runtime_segment_extent};
	return exact_decimal_pow5_runtime_anchor_dispatch_type<
		storage_tag>::functions[segment](anchor_index);
}

template <::std::size_t capacity>
inline constexpr void exact_precision_add_limbs(
	exact_precision_limb_type (&limbs)[capacity], ::std::size_t &size,
	exact_precision_limb_type const *addend, ::std::size_t addend_size) noexcept
{
	exact_precision_multiplier_type carry{};
	::std::size_t index{};
	for (; index < addend_size || carry; ++index)
	{
		if (size == index)
		{
			limbs[size++] = 0u;
		}
		auto const sum{static_cast<exact_precision_multiplier_type>(limbs[index]) +
					   (index < addend_size ? addend[index] : 0u) + carry};
		limbs[index] = static_cast<exact_precision_limb_type>(
			sum % exact_precision_limb_base);
		carry = sum / exact_precision_limb_base;
	}
}

template <::std::size_t capacity, typename mantissa_type>
inline constexpr void exact_precision_multiply_anchor_by_mantissa(
	exact_precision_limb_type (&limbs)[capacity], ::std::size_t &size,
	exact_precision_limb_type const *anchor, ::std::size_t anchor_size,
	mantissa_type mantissa) noexcept
{
	constexpr unsigned word_bits{32u};
	constexpr auto word_mask{static_cast<mantissa_type>(0xffffffffu)};
	constexpr exact_precision_multiplier_type word_base{
		static_cast<exact_precision_multiplier_type>(1u) << word_bits};
	constexpr auto word_count{
		(sizeof(mantissa_type) * ::std::numeric_limits<unsigned char>::digits +
		 word_bits - 1u) /
		word_bits};
	limbs[0u] = 0u;
	size = 1u;
	for (::std::size_t index{word_count}; index; --index)
	{
		::fast_io::details::exact_precision_multiply_small(
			limbs, size, word_base);
		auto const word{static_cast<exact_precision_multiplier_type>(
			(mantissa >> ((index - 1u) * word_bits)) & word_mask)};
		if (word)
		{
			exact_precision_limb_type addend[capacity]{};
			for (::std::size_t copy{}; copy != anchor_size; ++copy)
			{
				addend[copy] = anchor[copy];
			}
			auto addend_size{anchor_size};
			::fast_io::details::exact_precision_multiply_small(
				addend, addend_size, word);
			::fast_io::details::exact_precision_add_limbs(
				limbs, size, addend, addend_size);
		}
	}
}

/*
Exact-decimal's sparse power-of-five anchor is very long while an IEEE
significand occupies at most four base-1e9 limbs.  Treating the significand as
four base-2^32 Horner words makes three complete passes over the anchor for
each word (multiply, form an addend, add).  A rectangular base-1e9 multiply
needs one pass per short-operand limb and keeps every partial product below
1e18, so uint64_t is sufficient without a native 128-bit divide.

Keep this helper exact-decimal-only.  The established precision/rounding
fallback above deliberately retains its original multiplication path.
*/
template <::std::size_t capacity, typename mantissa_type>
inline constexpr void exact_decimal_multiply_anchor_by_mantissa(
	exact_precision_limb_type (&limbs)[capacity], ::std::size_t &size,
	exact_precision_limb_type const *anchor, ::std::size_t anchor_size,
	mantissa_type mantissa) noexcept
{
	constexpr ::std::size_t mantissa_limb_capacity{
		(sizeof(mantissa_type) * ::std::numeric_limits<unsigned char>::digits +
		 28u) /
			29u +
		1u};
	exact_precision_limb_type mantissa_limbs[mantissa_limb_capacity]{};
	::std::size_t mantissa_size{};
	if constexpr (sizeof(mantissa_type) <=
				  sizeof(exact_precision_multiplier_type))
	{
		using division_type = ::std::conditional_t<
			(sizeof(mantissa_type) < sizeof(exact_precision_multiplier_type)),
			exact_precision_multiplier_type, mantissa_type>;
		constexpr auto division_base{
			static_cast<division_type>(exact_precision_limb_base)};
		for (; mantissa;)
		{
			auto const current{static_cast<division_type>(mantissa)};
			mantissa_limbs[mantissa_size++] =
				static_cast<exact_precision_limb_type>(current % division_base);
			mantissa = static_cast<mantissa_type>(current / division_base);
		}
	}
	else
	{
		::fast_io::details::exact_precision_initialize_wide_limbs(
			mantissa_limbs, mantissa_size, mantissa);
	}
	if (!mantissa_size)
	{
		limbs[0u] = 0u;
		size = 1u;
		return;
	}

	auto const result_bound{anchor_size + mantissa_size};
	for (::std::size_t index{}; index != result_bound; ++index)
	{
		limbs[index] = 0u;
	}
	for (::std::size_t anchor_index{}; anchor_index != anchor_size;
		 ++anchor_index)
	{
		exact_precision_multiplier_type carry{};
		for (::std::size_t mantissa_index{};
			 mantissa_index != mantissa_size; ++mantissa_index)
		{
			auto const output_index{anchor_index + mantissa_index};
			auto const product{
				static_cast<exact_precision_multiplier_type>(
					anchor[anchor_index]) *
					mantissa_limbs[mantissa_index] +
				limbs[output_index] + carry};
			limbs[output_index] = static_cast<exact_precision_limb_type>(
				product % exact_precision_limb_base);
			carry = product / exact_precision_limb_base;
		}
		limbs[anchor_index + mantissa_size] =
			static_cast<exact_precision_limb_type>(carry);
	}
	size = result_bound;
	while (1u < size && !limbs[size - 1u])
	{
		--size;
	}
}

struct exact_decimal_two_digit_table
{
	unsigned char digits[200u]{};

	consteval exact_decimal_two_digit_table() noexcept
	{
		for (unsigned value{}; value != 100u; ++value)
		{
			digits[value * 2u] = static_cast<unsigned char>(value / 10u);
			digits[value * 2u + 1u] =
				static_cast<unsigned char>(value % 10u);
		}
	}
};

inline constexpr exact_decimal_two_digit_table
	exact_decimal_two_digit_table_instance{};

inline constexpr void exact_decimal_write_four_digits(
	unsigned char *destination, exact_precision_limb_type value) noexcept
{
	auto const high{value / 100u};
	auto const low{value - high * 100u};
	auto const *const high_digits{
		exact_decimal_two_digit_table_instance.digits + high * 2u};
	auto const *const low_digits{
		exact_decimal_two_digit_table_instance.digits + low * 2u};
	destination[0u] = high_digits[0u];
	destination[1u] = high_digits[1u];
	destination[2u] = low_digits[0u];
	destination[3u] = low_digits[1u];
}

inline constexpr void exact_decimal_write_nine_digits(
	unsigned char *destination, exact_precision_limb_type value) noexcept
{
	auto const first{value / 100000000u};
	auto const tail{value - first * 100000000u};
	auto const middle{tail / 10000u};
	auto const last{tail - middle * 10000u};
	destination[0u] = static_cast<unsigned char>(first);
	::fast_io::details::exact_decimal_write_four_digits(destination + 1u,
														middle);
	::fast_io::details::exact_decimal_write_four_digits(destination + 5u,
														last);
}

template <typename flt>
[[nodiscard]] inline constexpr exact_precision_decimal<flt>
exact_decimal_from_limbs(exact_precision_limb_type const *limbs,
						 ::std::size_t limb_size, ::std::int_least32_t exponent) noexcept
{
	exact_precision_decimal<flt> decimal{};
	decimal.exponent = exponent;
	unsigned char top_digits[exact_precision_limb_digits]{};
	::fast_io::details::exact_decimal_write_nine_digits(
		top_digits, limbs[limb_size - 1u]);
	::std::size_t first{};
	for (; first + 1u != exact_precision_limb_digits && !top_digits[first];
		 ++first)
	{
	}
	for (; first != exact_precision_limb_digits; ++first)
	{
		decimal.digits[decimal.size++] = top_digits[first];
	}
	for (auto index{limb_size - 1u}; index; --index)
	{
		::fast_io::details::exact_decimal_write_nine_digits(
			decimal.digits + decimal.size, limbs[index - 1u]);
		decimal.size += exact_precision_limb_digits;
	}
	return decimal;
}

struct exact_decimal_layout
{
	::std::size_t size{};
	::std::int_least32_t exponent{};
};

[[nodiscard]] inline constexpr exact_decimal_layout
exact_decimal_layout_from_limbs(exact_precision_limb_type const *limbs,
								::std::size_t limb_size, ::std::int_least32_t exponent) noexcept
{
	auto top{limbs[limb_size - 1u]};
	::std::size_t top_digits{};
	for (; top; top /= 10u)
	{
		++top_digits;
	}
	auto size{(limb_size - 1u) * exact_precision_limb_digits + top_digits};
	::std::size_t trailing_zeroes{};
	::std::size_t index{};
	for (; index + 1u != limb_size && !limbs[index]; ++index)
	{
		trailing_zeroes += exact_precision_limb_digits;
	}
	for (auto value{limbs[index]}; value && value % 10u == 0u; value /= 10u)
	{
		++trailing_zeroes;
	}
	return {size - trailing_zeroes,
			exponent + static_cast<::std::int_least32_t>(trailing_zeroes)};
}

template <typename flt>
inline constexpr exact_precision_decimal<flt> exact_precision_from_binary(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr ::std::int_least32_t bias{
		(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
	::std::int_least32_t binary_exponent{};
	if (exponent)
	{
		mantissa |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << trait::mbits);
		binary_exponent = static_cast<::std::int_least32_t>(exponent) - bias -
						  static_cast<::std::int_least32_t>(trait::mbits);
	}
	else
	{
		binary_exponent = 1 - bias - static_cast<::std::int_least32_t>(trait::mbits);
	}

	exact_precision_limb_type limbs[exact_precision_limb_capacity<flt>]{};
	::std::size_t limb_size{};
	bool initialized_from_anchor{};
	if constexpr (::fast_io::details::exact_precision_is_wide_binary<flt>)
	{
		if (!::std::is_constant_evaluated() && 0 < binary_exponent)
		{
			auto const chunk_count{static_cast<::std::uint_least32_t>(
				binary_exponent / static_cast<::std::int_least32_t>(
									  exact_precision_pow2_chunk))};
			auto const anchor_index{static_cast<::std::size_t>(
				chunk_count / exact_precision_pow2_anchor_chunk_count)};
			if (anchor_index)
			{
				auto const &anchor{::fast_io::details::
									   exact_precision_wide_pow2_anchor_table_instance<flt>()};
				::fast_io::details::exact_precision_multiply_anchor_by_mantissa(
					limbs, limb_size, anchor.limbs[anchor_index],
					anchor.sizes[anchor_index], mantissa);
				binary_exponent -= static_cast<::std::int_least32_t>(
					anchor_index * exact_precision_pow2_anchor_chunk_count *
					exact_precision_pow2_chunk);
				initialized_from_anchor = true;
			}
		}
	}
	if (!initialized_from_anchor)
	{
		if constexpr (sizeof(mantissa_type) <= sizeof(exact_precision_multiplier_type))
		{
			// Perform the base-1e9 quotient in a type that can represent both the
			// original mantissa and the divisor.  This is semantically the same integer
			// division as the former compound assignment, but makes its narrowing proof
			// explicit for binary16/bfloat16 front ends: the quotient never exceeds the
			// input mantissa.
			using division_type = ::std::conditional_t<
				(sizeof(mantissa_type) < sizeof(exact_precision_multiplier_type)),
				exact_precision_multiplier_type, mantissa_type>;
			constexpr auto division_base{static_cast<division_type>(exact_precision_limb_base)};
			for (; mantissa;)
			{
				auto const current{static_cast<division_type>(mantissa)};
				// The remainder is in [0, 1e9), exactly the limb_type domain.
				limbs[limb_size] = static_cast<exact_precision_limb_type>(
					current % division_base);
				++limb_size;
				mantissa = static_cast<mantissa_type>(current / division_base);
			}
		}
		else
		{
			::fast_io::details::exact_precision_initialize_wide_limbs(
				limbs, limb_size, mantissa);
		}
	}
	auto decimal_exponent{binary_exponent < 0 ? binary_exponent : 0};
	if (binary_exponent < 0)
	{
		auto count{static_cast<::std::uint_least32_t>(-binary_exponent)};
		for (; exact_precision_pow5_chunk <= count; count -= exact_precision_pow5_chunk)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size, exact_precision_pow5_multiplier);
		}
		if (count)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size, ::fast_io::details::exact_precision_small_power(5u, count));
		}
	}
	else
	{
		auto count{static_cast<::std::uint_least32_t>(binary_exponent)};
		for (; exact_precision_pow2_chunk <= count; count -= exact_precision_pow2_chunk)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size, exact_precision_pow2_multiplier);
		}
		if (count)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size, static_cast<exact_precision_multiplier_type>(1u) << count);
		}
	}

	exact_precision_decimal<flt> decimal{};
	decimal.exponent = decimal_exponent;
	auto top{limbs[limb_size - 1u]};
	unsigned char reversed[exact_precision_limb_digits + 1u];
	::std::size_t reversed_size{};
	for (; top; top /= 10u)
	{
		reversed[reversed_size++] = static_cast<unsigned char>(top % 10u);
	}
	while (reversed_size)
	{
		decimal.digits[decimal.size++] = reversed[--reversed_size];
	}
	for (auto i{limb_size - 1u}; i; --i)
	{
		auto value{limbs[i - 1u]};
		auto position{decimal.size + exact_precision_limb_digits};
		decimal.size = position;
		for (unsigned digit{}; digit != exact_precision_limb_digits; ++digit)
		{
			decimal.digits[--position] = static_cast<unsigned char>(value % 10u);
			value /= 10u;
		}
	}
	while (decimal.size != 1u && decimal.digits[decimal.size - 1u] == 0u)
	{
		--decimal.size;
		++decimal.exponent;
	}
	return decimal;
}

/* Exact-decimal precise-size needs only the normalized coefficient length and
decimal exponent.  Keep this limb-only twin separate from the established
rounding backend above: it avoids materializing thousands of digit bytes while
leaving every ordinary precision/rounding instantiation untouched. */
template <typename flt>
[[nodiscard]] inline constexpr exact_decimal_layout
exact_precision_layout_from_binary(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr ::std::int_least32_t bias{
		(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
	::std::int_least32_t binary_exponent{};
	if (exponent)
	{
		mantissa |= static_cast<mantissa_type>(
			static_cast<mantissa_type>(1u) << trait::mbits);
		binary_exponent =
			static_cast<::std::int_least32_t>(exponent) - bias -
			static_cast<::std::int_least32_t>(trait::mbits);
	}
	else
	{
		binary_exponent = 1 - bias -
						  static_cast<::std::int_least32_t>(trait::mbits);
	}

	exact_precision_limb_type limbs[exact_precision_limb_capacity<flt>]{};
	::std::size_t limb_size{};
	bool initialized_from_anchor{};
	if constexpr (::fast_io::details::exact_precision_is_wide_binary<flt>)
	{
		if (!::std::is_constant_evaluated() && 0 < binary_exponent)
		{
			auto const chunk_count{static_cast<::std::uint_least32_t>(
				binary_exponent / static_cast<::std::int_least32_t>(
									  exact_precision_pow2_chunk))};
			auto const anchor_index{static_cast<::std::size_t>(
				chunk_count / exact_precision_pow2_anchor_chunk_count)};
			if (anchor_index)
			{
				auto const &anchor{
					::fast_io::details::
						exact_precision_wide_pow2_anchor_table_instance<flt>()};
				::fast_io::details::exact_precision_multiply_anchor_by_mantissa(
					limbs, limb_size, anchor.limbs[anchor_index],
					anchor.sizes[anchor_index], mantissa);
				binary_exponent -= static_cast<::std::int_least32_t>(
					anchor_index * exact_precision_pow2_anchor_chunk_count *
					exact_precision_pow2_chunk);
				initialized_from_anchor = true;
			}
		}
	}
	if (!initialized_from_anchor)
	{
		if constexpr (sizeof(mantissa_type) <=
					  sizeof(exact_precision_multiplier_type))
		{
			using division_type = ::std::conditional_t<
				(sizeof(mantissa_type) <
				 sizeof(exact_precision_multiplier_type)),
				exact_precision_multiplier_type, mantissa_type>;
			constexpr auto division_base{
				static_cast<division_type>(exact_precision_limb_base)};
			for (; mantissa;)
			{
				auto const current{static_cast<division_type>(mantissa)};
				limbs[limb_size++] = static_cast<exact_precision_limb_type>(
					current % division_base);
				mantissa = static_cast<mantissa_type>(
					current / division_base);
			}
		}
		else
		{
			::fast_io::details::exact_precision_initialize_wide_limbs(
				limbs, limb_size, mantissa);
		}
	}
	auto const decimal_exponent{binary_exponent < 0 ? binary_exponent : 0};
	if (binary_exponent < 0)
	{
		auto count{static_cast<::std::uint_least32_t>(-binary_exponent)};
		for (; exact_precision_pow5_chunk <= count;
			 count -= exact_precision_pow5_chunk)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size, exact_precision_pow5_multiplier);
		}
		if (count)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size,
				::fast_io::details::exact_precision_small_power(5u, count));
		}
	}
	else
	{
		auto count{static_cast<::std::uint_least32_t>(binary_exponent)};
		for (; exact_precision_pow2_chunk <= count;
			 count -= exact_precision_pow2_chunk)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size, exact_precision_pow2_multiplier);
		}
		if (count)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size,
				static_cast<exact_precision_multiplier_type>(1u) << count);
		}
	}
	return ::fast_io::details::exact_decimal_layout_from_limbs(
		limbs, limb_size, decimal_exponent);
}

template <bool layout_only, typename flt>
inline constexpr ::std::conditional_t<layout_only, exact_decimal_layout,
									  exact_precision_decimal<flt>>
exact_decimal_from_binary_impl(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	auto const original_mantissa{mantissa};
	constexpr ::std::int_least32_t bias{
		(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
	constexpr ::std::size_t maximum_denominator_power{
		static_cast<::std::size_t>(bias) + trait::mbits - 1u};
	static_assert(maximum_denominator_power <=
					  exact_decimal_pow5_anchor_maximum_exponent,
				  "extend the shared exact-decimal pow5 anchor domain for this format");
	if constexpr (maximum_denominator_power <
				  exact_decimal_pow5_anchor_minimum_exponent)
	{
		if constexpr (layout_only)
		{
			return ::fast_io::details::exact_precision_layout_from_binary<flt>(
				original_mantissa, exponent);
		}
		else
		{
			return ::fast_io::details::exact_precision_from_binary<flt>(
				original_mantissa, exponent);
		}
	}
	else
	{
		::std::int_least32_t binary_exponent{};
		if (exponent)
		{
			mantissa |= static_cast<mantissa_type>(
				static_cast<mantissa_type>(1u) << trait::mbits);
			binary_exponent =
				static_cast<::std::int_least32_t>(exponent) - bias -
				static_cast<::std::int_least32_t>(trait::mbits);
		}
		else
		{
			binary_exponent = 1 - bias -
							  static_cast<::std::int_least32_t>(trait::mbits);
		}
		/* Canonicalize S*2^e before selecting the power of five.  Every
		removed binary factor saves one decimal factor and cannot introduce a
		base-ten trailing zero because the remaining significand is odd. */
		for (; binary_exponent < 0 &&
			   (mantissa & static_cast<mantissa_type>(1u)) == 0u;
			 ++binary_exponent)
		{
			mantissa >>= 1u;
		}
		if (binary_exponent >= 0 ||
			static_cast<::std::uint_least32_t>(-binary_exponent) <
				exact_decimal_pow5_anchor_minimum_exponent)
		{
			/* Passing canonicalized fields back would change their raw-field
			meaning, so the established path receives the saved original input. */
			if constexpr (layout_only)
			{
				return ::fast_io::details::exact_precision_layout_from_binary<flt>(
					original_mantissa, exponent);
			}
			else
			{
				return ::fast_io::details::exact_precision_from_binary<flt>(
					original_mantissa, exponent);
			}
		}

		exact_precision_limb_type
			limbs[exact_precision_limb_capacity<flt>]{};
		::std::size_t limb_size{};
		auto count{static_cast<::std::uint_least32_t>(-binary_exponent)};
		auto const chunk_count{count / exact_precision_pow5_chunk};
		auto const anchor_index{static_cast<::std::size_t>(
			chunk_count / exact_decimal_pow5_anchor_chunk_count)};
		auto selected_anchor_index{anchor_index};
		exact_decimal_pow5_anchor_view anchor{};
		if constexpr (maximum_denominator_power <
					  exact_decimal_pow5_runtime_first_index *
						  exact_decimal_pow5_anchor_stride)
		{
			using compact_storage =
				::fast_io::details::exact_decimal_pow5_compact_anchor_storage<flt>;
			auto const entry{
				anchor_index - exact_decimal_pow5_anchor_first_index};
			anchor = {compact_storage::hot.limbs +
						  compact_storage::hot.offsets[entry],
					  compact_storage::hot.sizes[entry]};
		}
		else if (::std::is_constant_evaluated())
		{
			auto const selected{
				::fast_io::details::exact_decimal_pow5_constexpr_anchor_lookup<
					::fast_io::details::exact_decimal_pow5_shared_storage_tag>(
					anchor_index)};
			anchor = selected.anchor;
			selected_anchor_index = selected.anchor_index;
		}
		else
		{
			anchor = ::fast_io::details::exact_decimal_pow5_anchor_lookup<
				::fast_io::details::exact_decimal_pow5_shared_storage_tag>(
				anchor_index);
		}
		::fast_io::details::exact_decimal_multiply_anchor_by_mantissa(
			limbs, limb_size, anchor.limbs, anchor.size, mantissa);
		count -= static_cast<::std::uint_least32_t>(
			selected_anchor_index * exact_decimal_pow5_anchor_stride);
		for (; exact_precision_pow5_chunk <= count;
			 count -= exact_precision_pow5_chunk)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size, exact_precision_pow5_multiplier);
		}
		if (count)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size,
				::fast_io::details::exact_precision_small_power(5u, count));
		}

		if constexpr (layout_only)
		{
			return ::fast_io::details::exact_decimal_layout_from_limbs(
				limbs, limb_size, binary_exponent);
		}
		else
		{
			auto const decimal{
				::fast_io::details::exact_decimal_from_limbs<flt>(
					limbs, limb_size, binary_exponent)};
			/* The normalized significand is odd, so S*5^k cannot end in zero. */
			return decimal;
		}
	}
}

template <typename flt>
inline constexpr exact_precision_decimal<flt>
exact_decimal_from_binary(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent) noexcept
{
	return ::fast_io::details::exact_decimal_from_binary_impl<false, flt>(
		mantissa, exponent);
}

template <typename flt>
[[nodiscard]] inline constexpr exact_decimal_layout
exact_decimal_layout_from_binary(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent) noexcept
{
	return ::fast_io::details::exact_decimal_from_binary_impl<true, flt>(
		mantissa, exponent);
}

template <::fast_io::manipulators::floating_rounding rounding, typename flt>
[[nodiscard]] inline constexpr bool exact_precision_round_up(
	exact_precision_decimal<flt> const &decimal, ::std::int_least32_t keep, bool negative) noexcept
{
	if (keep < 0)
	{
		if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
		{
			return false;
		}
		else
		{
			return ::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
		}
	}
	auto const first{decimal.digits[static_cast<::std::size_t>(keep)]};
	bool tail{};
	for (auto i{static_cast<::std::size_t>(keep) + 1u}; i != decimal.size; ++i)
	{
		tail |= decimal.digits[i] != 0u;
	}
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		if (first < 5u)
		{
			return false;
		}
		if (5u < first || tail)
		{
			return true;
		}
		auto const rounded_down{keep ? decimal.digits[static_cast<::std::size_t>(keep) - 1u] : 0u};
		return ::fast_io::details::print_rsv_fp_decimal_tie_round_up<rounding>(negative, rounded_down);
	}
	else
	{
		return ::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
	}
}

template <::fast_io::manipulators::floating_rounding rounding, typename flt>
inline constexpr void exact_precision_round(
	exact_precision_decimal<flt> &decimal, ::std::int_least32_t keep, bool negative) noexcept
{
	if (static_cast<::std::int_least32_t>(decimal.size) <= keep)
	{
		return;
	}
	auto const round_up{::fast_io::details::exact_precision_round_up<rounding>(decimal, keep, negative)};
	auto const old_size{decimal.size};
	auto const target_exponent{static_cast<::std::int_least32_t>(
		decimal.exponent + static_cast<::std::int_least32_t>(old_size) - keep)};
	if (keep <= 0)
	{
		decimal.size = 1u;
		decimal.digits[0] = static_cast<unsigned char>(round_up);
		decimal.exponent = target_exponent;
		return;
	}
	decimal.size = static_cast<::std::size_t>(keep);
	decimal.exponent = target_exponent;
	if (!round_up)
	{
		return;
	}
	for (auto i{decimal.size}; i; --i)
	{
		if (++decimal.digits[i - 1u] != 10u)
		{
			return;
		}
		decimal.digits[i - 1u] = 0u;
	}
	decimal.digits[0] = 1u;
	decimal.size = 1u;
	decimal.exponent = target_exponent + keep;
}

template <typename decimal_type>
inline constexpr void exact_precision_trim(decimal_type &decimal) noexcept
{
	while (decimal.size != 1u)
	{
		// A nonzero final digit terminates the trim immediately.  Keeping this
		// return inside the loop preserves the established multi-digit nonzero
		// fast path: it performs the same size test and final-digit test without
		// falling through to the zero-only canonicalization below.
		if (decimal.digits[decimal.size - 1u] != 0u)
		{
			return;
		}
		--decimal.size;
		++decimal.exponent;
	}
	// Every pair (0, e) represents the same real number.  A non-preserving
	// precision mode must therefore discard the obsolete rounding quantum as
	// well as coefficient zeroes; otherwise general formatting can select
	// scientific notation solely from e (for example, 0e-05 instead of 0).
	// Preserving modes do not call this trim after rounding, so their requested
	// fractional field remains intact.  The canonical non-preserving zero is
	// exactly (coefficient=0, exponent=0), matching the scalar carrier path.
	if (decimal.digits[0] == 0u)
	{
		decimal.exponent = 0;
	}
}

[[nodiscard]] inline constexpr ::std::size_t exact_precision_saturating_add(
	::std::size_t left, ::std::size_t right) noexcept
{
	constexpr auto maximum{(::std::numeric_limits<::std::size_t>::max)()};
	return maximum - left < right ? maximum : left + right;
}

// The compact exact window represents a decimal prefix and the intermediate
// 64-by-128 products in native unsigned 128-bit integers.  This outer gate is
// therefore a representation-capability boundary: targets without that scalar
// type retain the complete limb-based exact expansion below, with identical
// precision and rounding semantics.  Enabling the window there would require a
// proved two-word replacement for every coefficient operation, not merely an
// ISA-specific spelling for multiplication.
#if defined(__SIZEOF_INT128__)
// Keep only the requested prefix, one guard digit, and a sticky bit. For a
// finite binary value M*2^e, the prefix is obtained from
// floor(M*2^e*10^(requested-1-real_exponent)); the discarded binary and
// decimal remainders are combined into tail_nonzero. This avoids building the
// complete (up to 1074-place) exact decimal expansion for a short precision.
inline constexpr ::std::size_t exact_precision_window_digit_capacity{384u};
inline constexpr ::std::size_t exact_precision_window_binary_limb_capacity{24u};
inline constexpr ::std::size_t exact_precision_window_decimal_limb_capacity{20u};
inline constexpr ::std::uint_least64_t exact_precision_window_decimal_limb_base{
	10000000000000000000ull};
inline constexpr unsigned exact_precision_window_decimal_limb_width{19u};
inline constexpr unsigned exact_precision_window_pow5_chunk{27u};
inline constexpr ::std::uint_least64_t exact_precision_window_pow5_multiplier{
	7450580596923828125ull};

struct exact_precision_window_decimal
{
	unsigned char digits[exact_precision_window_digit_capacity];
	::std::size_t size;
	::std::int_least32_t exponent;
};

struct exact_precision_window_result
{
	exact_precision_window_decimal decimal;
	::std::int_least32_t real_exponent;
	bool tail_nonzero;
	bool success;
};

inline constexpr ::std::size_t exact_precision_compact_window_digit_capacity{160u};

struct exact_precision_compact_window_decimal
{
	unsigned char digits[exact_precision_compact_window_digit_capacity];
	::std::size_t size;
	::std::int_least32_t exponent;
};

struct exact_precision_compact_window_result
{
	exact_precision_compact_window_decimal decimal;
	::std::int_least32_t real_exponent;
	bool tail_nonzero;
	bool success;
};

struct exact_precision_binary32_pow5_table_type
{
	::std::uint_least64_t values[18u]{};

	inline constexpr exact_precision_binary32_pow5_table_type() noexcept
	{
		values[0] = 1u;
		for (::std::size_t index{1u}; index != 18u; ++index)
		{
			values[index] = values[index - 1u] * 5u;
		}
	}

	[[nodiscard]] inline static constexpr ::std::size_t size() noexcept
	{
		return 18u;
	}

	[[nodiscard]] inline constexpr ::std::uint_least64_t operator[](
		::std::size_t index) const noexcept
	{
		return values[index];
	}
};

inline constexpr exact_precision_binary32_pow5_table_type
	exact_precision_binary32_pow5_table{};

struct exact_precision_binary32_negative_power10_table_type
{
	::std::uint_least32_t values[18u]{};

	inline constexpr exact_precision_binary32_negative_power10_table_type() noexcept
	{
		for (::std::uint_least32_t index{1u}; index != 18u; ++index)
		{
			auto const binary_exponent{-
				(::fast_io::details::mul_ln10_div_ln2_floor(
					static_cast<::std::int_least32_t>(index)) + 1)};
			auto const shift{static_cast<unsigned>(
				-static_cast<::std::int_least32_t>(index) - binary_exponent + 23)};
			auto const numerator{static_cast<::std::uint_least64_t>(1u) << shift};
			auto const divisor{exact_precision_binary32_pow5_table[index]};
			auto significand{numerator / divisor};
			auto const remainder{numerator % divisor};
			auto const twice_remainder{remainder << 1u};
			if (divisor < twice_remainder ||
				(divisor == twice_remainder && (significand & 1u)))
			{
				++significand;
			}
			auto exponent_field{static_cast<::std::uint_least32_t>(binary_exponent + 127)};
			if (significand == (static_cast<::std::uint_least64_t>(1ULL) << 24u))
			{
				significand >>= 1u;
				++exponent_field;
			}
			values[index] = (exponent_field << 23u) |
				static_cast<::std::uint_least32_t>(significand - (static_cast<::std::uint_least64_t>(1ULL) << 23u));
		}
	}

	[[nodiscard]] inline constexpr ::std::uint_least32_t operator[](
		::std::size_t index) const noexcept
	{
		return values[index];
	}
};

inline constexpr exact_precision_binary32_negative_power10_table_type
	exact_precision_binary32_negative_power10_table{};

struct exact_precision_binary32_power10_exponent_index_table_type
{
	unsigned char values[256u]{};

	inline constexpr exact_precision_binary32_power10_exponent_index_table_type() noexcept
	{
		using trait = ::fast_io::details::iec559_traits<float>;
		for (::std::size_t index{1u}; index != 18u; ++index)
		{
			auto const exponent{
				exact_precision_binary32_negative_power10_table[index] >> trait::mbits};
			values[exponent] = static_cast<unsigned char>(index);
		}
	}

	[[nodiscard]] inline constexpr unsigned char operator[](
		::std::size_t exponent) const noexcept
	{
		return values[exponent];
	}
};

// Each of the first seventeen negative powers has a distinct binary32 exponent
// field. Generate the reverse map so the runtime exact-value check is two table
// loads and a comparison instead of another logarithm approximation.
inline constexpr exact_precision_binary32_power10_exponent_index_table_type
	exact_precision_binary32_power10_exponent_index_table{};

[[nodiscard]] inline constexpr ::std::int_least32_t
exact_precision_correct_binary32_negative_real_exponent(
	::std::uint_least32_t mantissa, ::std::int_least32_t binary_exponent,
	::std::int_least32_t real_exponent) noexcept
{
	if (0 <= real_exponent || 0 <= binary_exponent)
	{
		return real_exponent;
	}
	auto const decimal_exponent_magnitude{
		static_cast<::std::uint_least32_t>(-real_exponent)};
	if (exact_precision_binary32_pow5_table.size() <= decimal_exponent_magnitude)
	{
		return real_exponent;
	}
	auto const binary_threshold_exponent{-binary_exponent -
		static_cast<::std::int_least32_t>(decimal_exponent_magnitude)};
	if (63 < binary_threshold_exponent)
	{
		return real_exponent - 1;
	}
	if (binary_threshold_exponent < 0)
	{
		return real_exponent;
	}
	// M*2^e >= 10^-n exactly when M*5^n >= 2^(-e-n).  The first
	// seventeen negative decimal powers fit this comparison in uint64_t.
	auto const scaled{static_cast<::std::uint_least64_t>(mantissa) *
		exact_precision_binary32_pow5_table[decimal_exponent_magnitude]};
	auto const threshold{static_cast<::std::uint_least64_t>(1u) << binary_threshold_exponent};
	return scaled < threshold ? real_exponent - 1 : real_exponent;
}

[[nodiscard]] inline constexpr ::std::int_least32_t
exact_precision_correct_binary32_raw_negative_real_exponent(
	::std::uint_least32_t mantissa, ::std::uint_least32_t exponent,
	::std::int_least32_t real_exponent) noexcept
{
	constexpr ::std::int_least32_t bias{127};
	constexpr ::std::int_least32_t mantissa_bits{23};
	::std::int_least32_t binary_exponent{};
	if (exponent)
	{
		mantissa |= static_cast<::std::uint_least32_t>(1U) << mantissa_bits;
		binary_exponent = static_cast<::std::int_least32_t>(exponent) - bias - mantissa_bits;
	}
	else
	{
		binary_exponent = 1 - bias - mantissa_bits;
	}
	return ::fast_io::details::exact_precision_correct_binary32_negative_real_exponent(
		mantissa, binary_exponent, real_exponent);
}

inline constexpr void exact_precision_copy_character_digits_numeric(
	unsigned char *destination, char const *source, ::std::size_t count) noexcept
{
	constexpr auto zero{static_cast<unsigned char>(char_literal_v<u8'0', char>)};
	// Runtime memcpy is used only when the compiler advertises the builtin; it
	// permits one unaligned eight-byte load/subtract/store without depending on a
	// hosted C library.  Constant evaluation and unsupported compilers use the
	// byte loop, which is the semantic reference.
#if FAST_IO_HAS_BUILTIN(__builtin_memcpy)
	if (!::std::is_constant_evaluated() && 8u <= count)
	{
		// Every source byte is at least the literal zero, so the packed subtraction
		// cannot borrow across byte lanes on either little- or big-endian targets.
		::std::uint_least64_t packed;
		__builtin_memcpy(__builtin_addressof(packed), source, sizeof(packed));
		packed -= static_cast<::std::uint_least64_t>(0x0101010101010101ULL) * zero;
		__builtin_memcpy(destination, __builtin_addressof(packed), sizeof(packed));
		destination += 8u;
		source += 8u;
		count -= 8u;
	}
#endif
	for (::std::size_t position{}; position != count; ++position)
	{
		destination[position] = static_cast<unsigned char>(
			static_cast<unsigned char>(source[position]) - zero);
	}
}

[[nodiscard]] inline constexpr __uint128_t exact_precision_window_umul256_high(
	__uint128_t left, ::std::uint_least64_t right_high,
	::std::uint_least64_t right_low) noexcept
{
	auto const left_low{static_cast<::std::uint_least64_t>(left)};
	auto const left_high{static_cast<::std::uint_least64_t>(left >> 64u)};
	auto const product00{static_cast<__uint128_t>(left_low) * right_low};
	auto const product01{static_cast<__uint128_t>(left_low) * right_high};
	auto const product10{static_cast<__uint128_t>(left_high) * right_low};
	auto const product11{static_cast<__uint128_t>(left_high) * right_high};
	auto const middle1{product10 + static_cast<::std::uint_least64_t>(product00 >> 64u)};
	auto const middle2{product01 + static_cast<::std::uint_least64_t>(middle1)};
	return product11 + (middle1 >> 64u) + (middle2 >> 64u);
}

[[nodiscard]] inline constexpr ::std::uint_least32_t exact_precision_window_uint128_mod1e9(
	__uint128_t value) noexcept
{
	// Ryu's reciprocal reduction. Only the low 61 bits of the shifted high
	// product are needed before the final uint32_t truncation.
	::std::uint_least64_t multiplied;
	// GCC 14--15 with BMI2 otherwise retain zero-valued carry temporaries in
	// callee-saved registers for this reciprocal reduction.  The mulx sequence
	// computes only product limb 2, the sole limb used after the 29-bit shift,
	// and carries are explicit.  GCC 13 and GCC 16 or later, Clang, non-x86 and
	// non-BMI2 targets use the portable u128 expression.  Thus future GNU
	// frontends inherit the latest GCC-16 lowering instead of falling outside the
	// algorithm.  The split is correct only while random
	// differential tests prove limb identity and assembly audits confirm no
	// helper call or new spill on the selected GCC majors.  GNU extended assembly
	// and the register contract are closed to the measured Linux System V LP64
	// artifact; every other ABI uses the exact portable carry expression.
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__BMI2__) && defined(__GNUC__) && !defined(__clang__) && \
	14 <= __GNUC__ && __GNUC__ < 16 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	if (!__builtin_is_constant_evaluated())
	{
		auto const left_low{static_cast<::std::uint_least64_t>(value)};
		auto const left_high{static_cast<::std::uint_least64_t>(value >> 64u)};
		::std::uint_least64_t middle;
		constexpr auto right_low{static_cast<::std::uint_least64_t>(0x31680A88F8953031ULL)};
		constexpr auto right_high{static_cast<::std::uint_least64_t>(0x89705F4136B4A597ULL)};
		::std::uint_least64_t scratch;
		::std::uint_least64_t high;
		// Independent BMI2 products and explicit adc operations form limb 2;
		// higher limbs are mathematically irrelevant to the reduction.
		__asm__("movq %4, %%rdx\n\t"
				"mulxq %6, %2, %1\n\t"
				"mulxq %7, %2, %0\n\t"
				"addq %2, %1\n\t"
				"adcq $0, %0\n\t"
				"movq %5, %%rdx\n\t"
				"mulxq %6, %2, %3\n\t"
				"addq %2, %1\n\t"
				"adcq $0, %0\n\t"
				"addq %3, %0\n\t"
				"mulxq %7, %2, %1\n\t"
				"addq %2, %0"
				: "=&r"(multiplied), "=&r"(middle), "=&r"(scratch), "=&r"(high)
				: "r"(left_low), "r"(left_high), "r"(right_low), "r"(right_high)
				: "rdx", "cc");
	}
	else
#endif
	{
		multiplied = static_cast<::std::uint_least64_t>(
			::fast_io::details::exact_precision_window_umul256_high(
				value, static_cast<::std::uint_least64_t>(0x89705F4136B4A597ULL), static_cast<::std::uint_least64_t>(0x31680A88F8953031ULL)));
	}
	auto const shifted{static_cast<::std::uint_least32_t>(multiplied >> 29u)};
	return static_cast<::std::uint_least32_t>(value) - 1000000000u * shifted;
}

[[nodiscard]] inline constexpr ::std::uint_least32_t exact_precision_window_mul_shift_mod1e9(
	::std::uint_least64_t mantissa, ::std::uint_least64_t const *multiplier,
	::std::uint_least32_t shift) noexcept
{
	auto const product0{static_cast<__uint128_t>(mantissa) * multiplier[0]};
	auto const product1{static_cast<__uint128_t>(mantissa) * multiplier[1]};
	auto const product2{static_cast<__uint128_t>(mantissa) * multiplier[2]};
	auto const middle{product1 + static_cast<::std::uint_least64_t>(product0 >> 64u)};
	auto const high{product2 + static_cast<::std::uint_least64_t>(middle >> 64u)};
	return ::fast_io::details::exact_precision_window_uint128_mod1e9(
		high >> (shift - 128u));
}

[[nodiscard]] inline constexpr ::std::uint_least32_t exact_precision_window_log10_pow2(
	::std::uint_least32_t exponent) noexcept
{
	return (exponent * 78913u) >> 18u;
}

struct exact_precision_window_decimal_limb_reciprocal_table_type
{
	::std::uint_least64_t values[9u]{};

	inline constexpr exact_precision_window_decimal_limb_reciprocal_table_type() noexcept
	{
		::std::uint_least64_t divisor{1u};
		for (::std::size_t power{1u}; power != 9u; ++power)
		{
			divisor *= 10u;
			values[power] =
				(::std::numeric_limits<::std::uint_least64_t>::max)() / divisor + 1u;
		}
	}

	[[nodiscard]] inline constexpr ::std::uint_least64_t operator[](
		::std::size_t power) const noexcept
	{
		return values[power];
	}
};

inline constexpr exact_precision_window_decimal_limb_reciprocal_table_type
	exact_precision_window_decimal_limb_reciprocal_table{};

[[nodiscard]] inline constexpr unsigned exact_precision_window_decimal_limb_digit(
	::std::uint_least32_t block, unsigned position) noexcept
{
	// For block < 1e9 and 1 <= k <= 8, ceil(2^64 / 10^k) has less
	// than 1e-10 quotient error. The smallest nonzero fractional gap is
	// 1e-8, so the high product is exactly floor(block / 10^k).
	auto const power{8u - position};
	::std::uint_least64_t quotient{block};
	if (power)
	{
		quotient = static_cast<::std::uint_least64_t>(
			(static_cast<__uint128_t>(block) *
				::fast_io::details::exact_precision_window_decimal_limb_reciprocal_table[power]) >>
			64u);
	}
	return static_cast<unsigned>(quotient % 10u);
}

[[nodiscard]] inline constexpr bool exact_precision_window_positive_binary_tail_nonzero(
	::std::uint_least64_t mantissa, ::std::uint_least32_t binary_exponent,
	::std::size_t discarded_decimal_digits) noexcept
{
	if (!discarded_decimal_digits)
	{
		return false;
	}
	// A binary64 mantissa cannot contain 5^24. Divisibility by 10^n
	// otherwise requires at least n factors of both 2 and 5.
	if (23u < discarded_decimal_digits)
	{
		return true;
	}
	auto const count{static_cast<::std::uint_least32_t>(discarded_decimal_digits)};
	auto const binary_factors{binary_exponent +
							  static_cast<::std::uint_least32_t>(::std::countr_zero(mantissa))};
	return binary_factors < count || !::fast_io::details::multiple_of_pow5(mantissa, count);
}

inline constexpr bool exact_precision_window_multiply_small(
	::std::uint_least64_t *limbs, ::std::size_t &size,
	::std::uint_least64_t multiplier) noexcept
{
	__uint128_t carry{};
	for (::std::size_t i{}; i != size; ++i)
	{
		auto const product{static_cast<__uint128_t>(limbs[i]) * multiplier + carry};
		limbs[i] = static_cast<::std::uint_least64_t>(product);
		carry = product >> 64u;
	}
	if (carry)
	{
		if (size == exact_precision_window_binary_limb_capacity)
		{
			return false;
		}
		limbs[size++] = static_cast<::std::uint_least64_t>(carry);
	}
	return true;
}

inline constexpr bool exact_precision_window_shift_left(
	::std::uint_least64_t *limbs, ::std::size_t &size, unsigned shift) noexcept
{
	auto const word_shift{static_cast<::std::size_t>(shift / 64u)};
	auto const bit_shift{shift % 64u};
	if (exact_precision_window_binary_limb_capacity - size < word_shift + static_cast<unsigned>(bit_shift != 0u))
	{
		return false;
	}
	if (bit_shift)
	{
		::std::uint_least64_t carry{};
		for (::std::size_t i{}; i != size; ++i)
		{
			auto const value{limbs[i]};
			limbs[i] = static_cast<::std::uint_least64_t>((value << bit_shift) | carry);
			carry = value >> (64u - bit_shift);
		}
		if (carry)
		{
			limbs[size++] = carry;
		}
	}
	if (word_shift)
	{
		for (auto i{size}; i; --i)
		{
			limbs[i - 1u + word_shift] = limbs[i - 1u];
		}
		for (::std::size_t i{}; i != word_shift; ++i)
		{
			limbs[i] = 0u;
		}
		size += word_shift;
	}
	return true;
}

inline constexpr bool exact_precision_window_shift_right(
	::std::uint_least64_t *limbs, ::std::size_t &size, unsigned shift) noexcept
{
	auto const word_shift{static_cast<::std::size_t>(shift / 64u)};
	auto const bit_shift{shift % 64u};
	bool tail_nonzero{};
	if (size <= word_shift)
	{
		for (::std::size_t i{}; i != size; ++i)
		{
			tail_nonzero = tail_nonzero || limbs[i] != 0u;
		}
		size = 0u;
		return tail_nonzero;
	}
	for (::std::size_t i{}; i != word_shift; ++i)
	{
		tail_nonzero = tail_nonzero || limbs[i] != 0u;
	}
	if (bit_shift)
	{
		auto const mask{(static_cast<::std::uint_least64_t>(1u) << bit_shift) - 1u};
		tail_nonzero = tail_nonzero || (limbs[word_shift] & mask) != 0u;
	}
	auto const new_size{size - word_shift};
	for (::std::size_t i{}; i != new_size; ++i)
	{
		auto value{limbs[i + word_shift] >> bit_shift};
		if (bit_shift && i + word_shift + 1u != size)
		{
			value |= limbs[i + word_shift + 1u] << (64u - bit_shift);
		}
		limbs[i] = value;
	}
	size = new_size;
	while (size && !limbs[size - 1u])
	{
		--size;
	}
	return tail_nonzero;
}

struct exact_precision_window_division
{
	::std::uint_least64_t quotient;
	::std::uint_least64_t remainder;
};

struct exact_precision_window_division_128
{
	__uint128_t quotient;
	::std::uint_least64_t remainder;
};

[[nodiscard]] inline constexpr exact_precision_window_division_128
exact_precision_window_divide_128_by_decimal_limb_full(__uint128_t current) noexcept
{
	constexpr auto divisor{exact_precision_window_decimal_limb_base};
	if (!::std::is_constant_evaluated())
	{
		constexpr auto uint128_max{(::std::numeric_limits<__uint128_t>::max)()};
		constexpr auto reciprocal{uint128_max / divisor};
		auto quotient{::fast_io::details::exact_precision_window_umul256_high(
			current, static_cast<::std::uint_least64_t>(reciprocal >> 64u),
			static_cast<::std::uint_least64_t>(reciprocal))};
		auto remainder{current - quotient * divisor};
		// Multiplication by floor((2^128-1)/d) is low by at most one for
		// every 128-bit dividend, so a single correction is sufficient.
		if (divisor <= remainder)
		{
			++quotient;
			remainder -= divisor;
		}
		return {quotient, static_cast<::std::uint_least64_t>(remainder)};
	}
	return {current / divisor, static_cast<::std::uint_least64_t>(current % divisor)};
}

// A P20-P33 coefficient no longer fits u64.  Split the rounded u128 carrier at
// the exact window's existing 1e19 limb boundary, so scientific precision and
// the generic exact path share both reciprocal arithmetic and digit writers.
// Keeping rendering outside the DA conversion also lets fixed and decimal
// layouts reuse the mathematical carrier without forcing their different
// punctuation and zero-padding rules through this scientific-only writer.
template <::std::size_t digits, bool comma, bool uppercase_e,
	::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
print_rsvflt_binary64_scientific_wide_precision_impl(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t exponent) noexcept
{
	static_assert(20u <= digits && digits <= 33u);
	constexpr ::std::uint_least64_t implicit_bit{static_cast<::std::uint_least64_t>(1ULL) << 52u};
	auto const converted{::fast_io::details::da::
		compute_binary64_scientific_wide_precision<digits>(
			mantissa | implicit_bit, exponent)};
	if (!converted.success)
	{
		return nullptr;
	}
	constexpr ::std::size_t high_digits{digits - 19u};
	auto const division{
		::fast_io::details::exact_precision_window_divide_128_by_decimal_limb_full(
			converted.significand)};
	auto const high{static_cast<::std::uint_least64_t>(division.quotient)};
	::fast_io::details::jeaiii::jeaiii_main_len<false, false>(
		iter + 1u, high, static_cast<::std::uint_least32_t>(high_digits));
	::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
		iter + high_digits + 1u, division.remainder, 19u);
	*iter = iter[1];
	iter[1] = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	return ::fast_io::details::print_rsv_fp_e_impl<double, uppercase_e>(
		iter + digits + 1u, converted.exponent);
}

// P34's complete two-word fraction is materially larger than the P20-P33
// arithmetic and is consumed by preserved scientific plus the significant and
// significant-preserve presentation writers.  A 34-digit coefficient needs at
// most 113 bits; ten of the remaining fifteen bits store real_exponent+512,
// whose normal binary64 range is [-308,308].  Zero remains the failure sentinel
// because a successful rounded coefficient is nonzero.  This packing changes
// lifetime and code size only.
//
// Apple Clang 23/M4 keeps the arithmetic inline: an outlined packed call cost
// about 1--1.5 ns on fixed/general P34 in paired one-thread measurements.  The
// Apple-AArch64 condition is an explicit family policy inferred from that M4
// evidence; it must be remeasured if another frontend gives it a different
// register-return shape.  The measured Linux System V x86-64 compilers instead
// share one coalesced body returning two registers.  Across the complete
// nine-mode probe this removed 2.9--7.9 KiB versus cloning, while P34 remained
// within about 0--4.5% of cloned latency and retained a 34--50% win over exact
// fallback. GCC 13 is the continuous GNU lower bound. Clang 22 and current
// trunk Clang 24 each add two calls when this boundary is forced, so only the
// measured Clang 23 artifact uses it. Other compilers and
// ABIs retain the ordinary compiler cost model. This conditional is code-generation
// policy only; every form returns the same packed coefficient and exponent.
// Move a bound only after frame, register-return, linked-text and hit/miss evidence.
#if defined(__clang__) && defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
FAST_IO_GNU_ALWAYS_INLINE
#elif defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC)) && \
	((defined(__clang__) && __clang_major__ == 23) || \
	 (defined(__GNUC__) && !defined(__clang__) && \
	  13 <= __GNUC__)) && \
	__has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
[[nodiscard]] inline constexpr __uint128_t
compute_binary64_p34_precision_carrier(
	::std::uint_least64_t mantissa,
	::std::uint_least32_t exponent) noexcept
{
	constexpr ::std::uint_least64_t implicit_bit{
		static_cast<::std::uint_least64_t>(1ULL) << 52u};
	auto const converted{::fast_io::details::da::
		compute_binary64_scientific_p34_precision(
			mantissa | implicit_bit, exponent)};
	if (!converted.success)
	{
		return 0u;
	}
	return converted.significand |
		(static_cast<__uint128_t>(converted.exponent + 512) << 113u);
}

inline constexpr __uint128_t binary64_p34_precision_coefficient_mask{
	(static_cast<__uint128_t>(1u) << 113u) - 1u};

// P34 retains a constant scientific writer because paired M4 AB/BA measurements
// showed a 3--5% regression when this path inherited the runtime P35-P38 width.
// The coefficient still comes from the shared P34 DA proof; only presentation is
// specialized.  Its fixed 15-digit high part and padded 19-digit low limb let the
// compiler encode every offset and writer length immediately, while an ambiguity
// rejection occurs before the destination is modified.  The caller admits only
// finite normal binary64, preserved scientific output and the six nearest rules.
template <bool comma, bool uppercase_e, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
print_rsvflt_binary64_scientific_p34_precision_impl(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t exponent) noexcept
{
	auto const carrier{::fast_io::details::
		compute_binary64_p34_precision_carrier(mantissa, exponent)};
	if (!carrier)
	{
		return nullptr;
	}
	constexpr ::std::size_t significant{34u};
	constexpr ::std::size_t high_digits{significant - 19u};
	auto const division{
		::fast_io::details::exact_precision_window_divide_128_by_decimal_limb_full(
			carrier & binary64_p34_precision_coefficient_mask)};
	::fast_io::details::jeaiii::jeaiii_main_len<false, false>(
		iter + 1u, static_cast<::std::uint_least64_t>(division.quotient),
		static_cast<::std::uint_least32_t>(high_digits));
	::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
		iter + high_digits + 1u, division.remainder, 19u);
	*iter = iter[1];
	iter[1] = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	return ::fast_io::details::print_rsv_fp_e_impl<double, uppercase_e>(
		iter + significant + 1u,
		static_cast<::std::int_least32_t>(carrier >> 113u) - 512);
}

// One Clang x86-64 entry shares runtime scale, split position and writer length
// across P20-P33, bounding emitted text and front-end pressure.  The recorded
// compiler transition is Clang 23. Complete Clang 22 and trunk Clang 24 probes
// are not instruction-identical when this body is selected, so both use the
// conservative constant-width frontend until paired runtime data proves a win.
// Other targets keep constant P20-P24 writer widths because the measured GCC/M4
// assembly folds the scale/offset and shortens the dependency chain.  Both
// forms reuse the same DA cache, power-of-ten data and 1e19 split.  This
// source-level sharing does not promise identical renderer text: punctuation
// remains specialized.  The Clang-23 runtime-body decision is measured; the
// optional `noinline` spelling merely enforces that sharing boundary.  Without
// attribute support the compiler may clone or inline the body, but success,
// fallback and emitted characters remain identical.
template <bool comma, bool uppercase_e, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *
print_rsvflt_binary64_scientific_wide_precision_runtime_impl(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t exponent, ::std::size_t significant) noexcept
{
	// Clang 23 Linux System V x86-64 LP64 keeps runtime scale and length in
	// registers without a frame or division helper.  Other configurations dispatch
	// P20-P24 to constant specializations to preserve immediate offsets and fixed
	// writer lengths.  Both branches emit identical character sequences; moving
	// the lower bound requires text-size, front-end, spill, call and dependency-
	// chain evidence.
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__clang__) && __clang_major__ == 23 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	constexpr ::std::uint_least64_t implicit_bit{static_cast<::std::uint_least64_t>(1ULL) << 52u};
	auto const extra_digit_multiplier{
		print_rsv_fp_pow10_0_to_19_table[significant - 16u]};
	constexpr __uint128_t decimal_limb{exact_precision_window_decimal_limb_base};
	auto const normalization_threshold{decimal_limb *
		print_rsv_fp_pow10_0_to_19_table[significant - 19u]};
	auto const normalized_significand{decimal_limb *
		print_rsv_fp_pow10_0_to_19_table[significant - 20u]};
	auto const converted{::fast_io::details::da::
		compute_binary64_scientific_wide_precision(mantissa | implicit_bit,
			exponent, extra_digit_multiplier, normalization_threshold,
			normalized_significand)};
	if (!converted.success)
	{
		return nullptr;
	}
	auto const high_digits{significant - 19u};
	auto const division{
		::fast_io::details::exact_precision_window_divide_128_by_decimal_limb_full(
			converted.significand)};
	auto const high{static_cast<::std::uint_least64_t>(division.quotient)};
	::fast_io::details::jeaiii::jeaiii_main_len<false, false>(
		iter + 1u, high, static_cast<::std::uint_least32_t>(high_digits));
	::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
		iter + high_digits + 1u, division.remainder, 19u);
	*iter = iter[1];
	iter[1] = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	return ::fast_io::details::print_rsv_fp_e_impl<double, uppercase_e>(
		iter + significant + 1u, converted.exponent);
#else
	switch (significant)
	{
	case 20u:
		return ::fast_io::details::
			print_rsvflt_binary64_scientific_wide_precision_impl<20u, comma, uppercase_e>(
				iter, mantissa, exponent);
	case 21u:
		return ::fast_io::details::
			print_rsvflt_binary64_scientific_wide_precision_impl<21u, comma, uppercase_e>(
				iter, mantissa, exponent);
	case 22u:
		return ::fast_io::details::
			print_rsvflt_binary64_scientific_wide_precision_impl<22u, comma, uppercase_e>(
				iter, mantissa, exponent);
	case 23u:
		return ::fast_io::details::
			print_rsvflt_binary64_scientific_wide_precision_impl<23u, comma, uppercase_e>(
				iter, mantissa, exponent);
	default:
		return ::fast_io::details::
			print_rsvflt_binary64_scientific_wide_precision_impl<24u, comma, uppercase_e>(
				iter, mantissa, exponent);
	}
#endif
}

// P25-P33 still use a u64 decimal scale and the proved one-sided DA error
// interval, while their rounded carrier fits u128.  Keep one shared runtime
// body so these nine precisions reuse the existing DA cache, power-of-ten
// table, reciprocal 1e19 split, and dynamic high-limb writer.  The shared range
// is benchmark-backed by the surrounding precision matrix; forcing this
// particular outline is a text/front-end policy, not another numeric bound.  If
// `noinline` is unavailable, ordinary inlining changes no coefficient, exponent
// or rejection condition.
template <bool comma, bool uppercase_e, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *
print_rsvflt_binary64_scientific_extended_precision_runtime_impl(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t exponent, ::std::size_t significant) noexcept
{
	// This local proof keeps every dynamic table and writer index bounded even
	// though the caller has already selected the same precision interval.
	if (8u < significant - 25u)
	{
		return nullptr;
	}
	// GCC 13--15 constant-fold P25-P28 scale and writer widths into the selected
	// x86 hot shape.  GCC 16 and later inherit the shared runtime tail, as do
	// P29-P33 on every GNU release, to bound emitted text and
	// front-end pressure.  This partition changes no arithmetic; retain it only
	// while the supported assembly matrix confirms constant folding, the intended
	// dependency chain, and the absence of new spills or calls.  The retained
	// evidence is specific to Linux System V x86-64 LP64; other ABIs use the
	// shared runtime tail and do not instantiate these specializations.
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__ && __GNUC__ < 16 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	switch (significant)
	{
	case 25u:
		return ::fast_io::details::
			print_rsvflt_binary64_scientific_wide_precision_impl<25u, comma, uppercase_e>(
				iter, mantissa, exponent);
	case 26u:
		return ::fast_io::details::
			print_rsvflt_binary64_scientific_wide_precision_impl<26u, comma, uppercase_e>(
				iter, mantissa, exponent);
	case 27u:
		return ::fast_io::details::
			print_rsvflt_binary64_scientific_wide_precision_impl<27u, comma, uppercase_e>(
				iter, mantissa, exponent);
	case 28u:
		return ::fast_io::details::
			print_rsvflt_binary64_scientific_wide_precision_impl<28u, comma, uppercase_e>(
				iter, mantissa, exponent);
	default:
		break;
	}
#endif
	constexpr ::std::uint_least64_t implicit_bit{static_cast<::std::uint_least64_t>(1ULL) << 52u};
	auto const extra_digit_multiplier{
		print_rsv_fp_pow10_0_to_19_table[significant - 16u]};
	constexpr __uint128_t decimal_limb{exact_precision_window_decimal_limb_base};
	auto const normalization_threshold{decimal_limb *
		print_rsv_fp_pow10_0_to_19_table[significant - 19u]};
	auto const normalized_significand{decimal_limb *
		print_rsv_fp_pow10_0_to_19_table[significant - 20u]};
	auto const converted{::fast_io::details::da::
		compute_binary64_scientific_wide_precision(mantissa | implicit_bit,
			exponent, extra_digit_multiplier, normalization_threshold,
			normalized_significand)};
	if (!converted.success)
	{
		return nullptr;
	}
	auto const high_digits{significant - 19u};
	auto const division{
		::fast_io::details::exact_precision_window_divide_128_by_decimal_limb_full(
			converted.significand)};
	auto const high{static_cast<::std::uint_least64_t>(division.quotient)};
	::fast_io::details::jeaiii::jeaiii_main_len<false, false>(
		iter + 1u, high, static_cast<::std::uint_least32_t>(high_digits));
	::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
		iter + high_digits + 1u, division.remainder, 19u);
	*iter = iter[1];
	iter[1] = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	return ::fast_io::details::print_rsv_fp_e_impl<double, uppercase_e>(
		iter + significant + 1u, converted.exponent);
}

// The fixed-fractional DA triangle always has 2..15 integer digits before an
// all-nine carry and P=4..14 requested fractional digits.  Therefore its writer
// never needs the leading-zero or integer-only branches of the general fixed
// formatter.  It writes the coefficient one character to the right, moves only
// the proved integer prefix into place, and fills the vacated character with the
// decimal point.  If DA normalized 10^K to 10^(K-1), real_exponent advanced by
// one and exactly one final zero is appended.  The reserve-print contract gives
// enough space for the coefficient, point and that zero for every character
// type; no byte-oriented operation is used here.
template <bool comma, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
print_rsvflt_binary64_fixed_fractional_da_narrow_write(
	char_type *iter,
	::fast_io::details::da::binary64_fixed_fractional_precision_result converted,
	::std::size_t fractional_precision) noexcept
{
	auto const integer_digits{
		static_cast<::std::size_t>(converted.real_exponent + 1)};
	auto const significand{static_cast<::std::uint_least64_t>(converted.significand)};
	::fast_io::details::jeaiii::jeaiii_main_len<false>(iter + 1u, significand,
		static_cast<::std::uint_least32_t>(converted.significant));
	::fast_io::details::my_copy_n(iter + 1u, integer_digits, iter);
	iter[integer_digits] = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	iter += converted.significant + 1u;
	auto const emitted_fractional_digits{converted.significant - integer_digits};
	if (emitted_fractional_digits < fractional_precision)
	{
		iter = ::fast_io::details::fill_zeros_impl(
			iter, fractional_precision - emitted_fractional_digits);
	}
	return iter;
}

template <bool comma, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
print_rsvflt_binary64_fixed_fractional_da_wide_write(
	char_type *iter,
	::fast_io::details::da::binary64_fixed_fractional_precision_result converted,
	::std::size_t fractional_precision) noexcept
{
	// K20..K28 in the selected triangle fit u128.  Splitting at the existing
	// 10^19 decimal limb reuses the exact-window reciprocal and fixed-width low
	// writer.  The quotient has at most nine digits here and therefore narrows to
	// u64 exactly; no u128 division helper is introduced.
	auto const division{
		::fast_io::details::exact_precision_window_divide_128_by_decimal_limb_full(
			converted.significand)};
	auto const high{static_cast<::std::uint_least64_t>(division.quotient)};
	auto const high_digits{converted.significant - 19u};
	::fast_io::details::jeaiii::jeaiii_main_len<false, false>(
		iter + 1u, high, static_cast<::std::uint_least32_t>(high_digits));
	::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
		iter + high_digits + 1u, division.remainder, 19u);
	auto const integer_digits{
		static_cast<::std::size_t>(converted.real_exponent + 1)};
	::fast_io::details::my_copy_n(iter + 1u, integer_digits, iter);
	iter[integer_digits] = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	iter += converted.significant + 1u;
	auto const emitted_fractional_digits{converted.significant - integer_digits};
	if (emitted_fractional_digits < fractional_precision)
	{
		iter = ::fast_io::details::fill_zeros_impl(
			iter, fractional_precision - emitted_fractional_digits);
	}
	return iter;
}

// Narrow and wide entries are deliberately separate outlined boundaries.  A
// merged K16..K28 writer kept the scale, reciprocal and punctuation state live
// together and lengthened the dependency chain in the audited M4 and x86-64
// compiler matrices.  Outlining also prevents every rounding, format and
// character instantiation of the large precision dispatcher from cloning this
// arithmetic.  The attribute probe affects only code layout; a compiler without
// the attribute retains identical rounding and fallback.
// The split is an observed M4/x86-64 code-generation result; extending that
// evidence to an unmeasured compiler is explicitly a layout hypothesis, never a
// change to the proved K16..K28 arithmetic domain.
// Both entries receive the magnitude iterator after the public formatter has
// already emitted its sign; adding `negative` here would duplicate '-' rather
// than repair rounding, which the proved nearest-only carrier has completed.
//
// GCC 15 at -Ofast changes its translation-unit inlining budget when the low-P
// entry below is present: this already-outlined narrow writer and an unrelated
// exact fallback are expanded, making the complete linked text 2,064 bytes
// larger. Applying `Os` only to this shared leaf restores the calls chosen by
// the baseline and limits the complete delta to 544 bytes. The low-P common
// corpus still improves by 8.8%--28.3%, while its broad controls remain within
// 1.15%. GCC 13, 14 and 16 and Clang do not exhibit that layout transition, so
// the exact GCC 15 exception is code-generation policy rather than an arithmetic
// or availability condition. The evidence is specific to Linux System V x86-64
// LP64, so every other ABI keeps the ordinary optimization level. Revalidate
// whole-object symbol sizes, calls and hit/miss latency before changing it;
// future GNU versions remain normally optimized rather than inheriting the
// GCC 15 exception.
template <bool comma, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && __GNUC__ == 15 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC)) && \
	__has_cpp_attribute(__gnu__::__optimize__) && ('A' == 0x41)
[[__gnu__::__optimize__("Os")]]
#endif
inline constexpr char_type *
print_rsvflt_binary64_fixed_fractional_da_narrow_impl(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t exponent, ::std::size_t fractional_precision) noexcept
{
	constexpr ::std::uint_least64_t implicit_bit{static_cast<::std::uint_least64_t>(1ULL) << 52u};
	auto const converted{::fast_io::details::da::
		compute_binary64_fixed_fractional_precision(
			mantissa | implicit_bit, exponent, fractional_precision)};
	if (!converted.success || 19u < converted.significant)
	{
		return nullptr;
	}
	return ::fast_io::details::print_rsvflt_binary64_fixed_fractional_da_narrow_write<
		comma>(iter, converted, fractional_precision);
}

// This is the wide half of the measured split documented above.  The optional
// outline keeps its u128 split/write state separate; without it, the same wide
// carrier test and exact fallback are preserved.
template <bool comma, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *
print_rsvflt_binary64_fixed_fractional_da_wide_impl(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t exponent, ::std::size_t fractional_precision) noexcept
{
	constexpr ::std::uint_least64_t implicit_bit{static_cast<::std::uint_least64_t>(1ULL) << 52u};
	auto const converted{::fast_io::details::da::
		compute_binary64_fixed_fractional_precision(
			mantissa | implicit_bit, exponent, fractional_precision)};
	if (!converted.success || converted.significant <= 19u)
	{
		return nullptr;
	}
	return ::fast_io::details::print_rsvflt_binary64_fixed_fractional_da_wide_write<
		comma>(iter, converted, fractional_precision);
}

// Linux System V x86-64 Clang 23 produces the best P15--P128 miss layout when
// the low-P probe is the `else` arm of the existing exact fixed window: across
// the complete sweep its mean control change is +0.11%. A separate Clang 22
// physical-core ABBA/BAAB sweep instead gives the standalone precision-first
// probe a +0.34% mean control change, a -1.09% worst row and no row below -2%.
// Clang 22, GCC and other targets use the standalone placement below. Current
// trunk Clang 24 also remains standalone because its complete precision caller
// is not instruction-identical to 23 and has no paired runtime admission. Thus
// the exact Clang 23 predicate is measured on both sides, not an untested hole.
// Both placements call the same outlined converter and exact fallback, so this
// policy changes branch placement and text, never digits.
inline constexpr bool binary64_fixed_fractional_low_integrated_dispatch{
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__clang__) && __clang_major__ == 23 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	true
#else
	false
#endif
};

// The public formatter has already emitted the sign and rejected zero,
// subnormal and non-finite inputs before entering this normal-only leaf. It is
// outlined so runtime scale, division and digit-placement state are shared by
// the six nearest rounding rules and do not lengthen their exact fallback.
// A null return means that the cache proof could not exclude a half boundary.
//
// On success rounded_integer is abs(value)*10^P rounded to an integer. Because
// P is at least one, both fixed and fractional-decimal presentations necessarily
// contain a radix point and JSON needs no additional `.0` suffix. If the
// rounded width is at most P, emit `0.`, leading zeroes and the coefficient.
// Otherwise write the coefficient one position to the right, move only its
// integer prefix left, and replace the vacated byte with the radix point. The
// existing digit and literal writers preserve native character width, comma
// selection and EBCDIC encoding without an ASCII staging copy.
template <bool comma, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *
print_rsvflt_binary64_fixed_fractional_da_low_impl(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t exponent,
	::std::size_t fractional_precision) noexcept
{
	auto const binary_exponent{
		static_cast<::std::int_least32_t>(exponent) - 1075};
	auto const decimal_exponent{
		::fast_io::details::da::compute_decimal_exponent_reduced(
			binary_exponent)};
	auto const range_probe{static_cast<::std::int_least64_t>(decimal_exponent) +
		static_cast<::std::int_least64_t>(fractional_precision)};
	if (range_probe < -16 || 0 < range_probe)
	{
		return nullptr;
	}
	constexpr ::std::uint_least64_t implicit_bit{
		static_cast<::std::uint_least64_t>(1ULL) << 52u};
	auto const converted{::fast_io::details::da::
		compute_binary64_fixed_fractional_low_precision(
			mantissa | implicit_bit, exponent, fractional_precision,
			decimal_exponent)};
	if (!converted.success)
	{
		return nullptr;
	}
	auto const decimal_digits{
		static_cast<::std::size_t>(converted.decimal_digits)};
	if (decimal_digits <= fractional_precision)
	{
		iter = ::fast_io::details::fill_zero_point_impl<comma>(iter);
		iter = ::fast_io::details::fill_zeros_impl(
			iter, fractional_precision - decimal_digits);
		::fast_io::details::print_rsv_fp_digits_len<double>(iter,
			converted.rounded_integer, converted.decimal_digits);
		return iter + decimal_digits;
	}
	auto const integer_digits{decimal_digits - fractional_precision};
	auto *const digits_begin{iter + 1u};
	::fast_io::details::print_rsv_fp_digits_len<double>(digits_begin,
		converted.rounded_integer, converted.decimal_digits);
	::fast_io::details::my_copy_n(digits_begin, integer_digits, iter);
	iter[integer_digits] =
		::fast_io::char_literal_v<comma ? u8',' : u8'.', char_type>;
	return iter + decimal_digits + 1u;
}

[[nodiscard]] inline constexpr exact_precision_window_division
exact_precision_window_divide_128_by_decimal_limb(
	::std::uint_least64_t high, ::std::uint_least64_t low) noexcept
{
	// GCC expresses the proved high<divisor precondition as one divq and avoids an
	// out-of-line __udivti3 helper in the audited Linux System V x86-64 LP64
	// artifact. Clang intentionally uses the exact reciprocal implementation
	// below: Clang 23's CodeGen StringLiteralParser crashes while instantiating
	// this extended-asm operand in large precision matrices, even in the default
	// AT&T dialect. Native MSVC, clang-cl, x32, MinGW, non-Linux x86-64 and other
	// targets use that same reciprocal path; constexpr evaluation uses language
	// division. All three implementations return the same quotient/remainder pair.
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	!defined(_MSC_VER) && defined(__GNUC__) && !defined(__clang__) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	if (!__builtin_is_constant_evaluated())
	{
		// high is the preceding base-1e19 remainder, so high < divisor and
		// the quotient fits in one word. Spell the native division directly;
		// otherwise x86 compilers lower this cold path to __udivti3.
		::std::uint_least64_t quotient;
		::std::uint_least64_t remainder;
		constexpr auto divisor{exact_precision_window_decimal_limb_base};
		__asm__("divq %4"
				: "=&a"(quotient), "=&d"(remainder)
				: "0"(low), "1"(high), "rm"(divisor)
				: "cc");
		return {quotient, remainder};
	}
#endif
	auto const current{(static_cast<__uint128_t>(high) << 64u) | low};
	if (!::std::is_constant_evaluated())
	{
		constexpr auto divisor{exact_precision_window_decimal_limb_base};
		constexpr auto uint128_max{(::std::numeric_limits<__uint128_t>::max)()};
		constexpr auto reciprocal{uint128_max / divisor};
		auto quotient{::fast_io::details::exact_precision_window_umul256_high(
			current, static_cast<::std::uint_least64_t>(reciprocal >> 64u),
			static_cast<::std::uint_least64_t>(reciprocal))};
		auto remainder{current - quotient * divisor};
		// floor((2^128-1)/d) underestimates 2^128/d. Since current < d*2^64,
		// the quotient estimate is low by at most one.
		if (divisor <= remainder)
		{
			++quotient;
			remainder -= divisor;
		}
		return {static_cast<::std::uint_least64_t>(quotient),
			static_cast<::std::uint_least64_t>(remainder)};
	}
	return {static_cast<::std::uint_least64_t>(current / exact_precision_window_decimal_limb_base),
			static_cast<::std::uint_least64_t>(current % exact_precision_window_decimal_limb_base)};
}

[[nodiscard]] inline constexpr ::std::uint_least64_t exact_precision_window_divide_decimal_limb(
	::std::uint_least64_t *limbs, ::std::size_t &size) noexcept
{
	::std::uint_least64_t remainder{};
	for (auto i{size}; i; --i)
	{
		auto const division{::fast_io::details::exact_precision_window_divide_128_by_decimal_limb(
			remainder, limbs[i - 1u])};
		limbs[i - 1u] = division.quotient;
		remainder = division.remainder;
	}
	while (size && !limbs[size - 1u])
	{
		--size;
	}
	return remainder;
}

inline constexpr ::std::size_t exact_precision_window_decimal_limb_digits(
	::std::uint_least64_t value) noexcept
{
	::std::size_t digits{1u};
	for (; 10u <= value; value /= 10u)
	{
		++digits;
	}
	return digits;
}

inline constexpr exact_precision_window_result exact_precision_window_materialize(
	::std::uint_least64_t *binary_limbs, ::std::size_t binary_size,
	::std::size_t requested_digits, ::std::int_least32_t real_exponent,
	bool tail_nonzero, bool use_integer_exponent) noexcept
{
	exact_precision_window_result result{};
	::std::uint_least64_t decimal_limbs[exact_precision_window_decimal_limb_capacity];
	::std::size_t decimal_size{};
	while (binary_size)
	{
		if (decimal_size == exact_precision_window_decimal_limb_capacity)
		{
			return result;
		}
		decimal_limbs[decimal_size++] =
			::fast_io::details::exact_precision_window_divide_decimal_limb(binary_limbs, binary_size);
	}
	if (!decimal_size)
	{
		return result;
	}
	auto const top_digits{::fast_io::details::exact_precision_window_decimal_limb_digits(
		decimal_limbs[decimal_size - 1u])};
	auto const total_digits{
		top_digits + (decimal_size - 1u) * exact_precision_window_decimal_limb_width};
	if (use_integer_exponent)
	{
		result.real_exponent = static_cast<::std::int_least32_t>(total_digits - 1u);
	}
	else
	{
		result.real_exponent = real_exponent + static_cast<::std::int_least32_t>(total_digits) -
							   static_cast<::std::int_least32_t>(requested_digits);
	}
	auto const output_size{requested_digits < total_digits ? requested_digits : total_digits};
	::std::size_t output_position{};
	for (auto i{decimal_size}; i; --i)
	{
		auto const width{i == decimal_size
							 ? top_digits
							 : static_cast<::std::size_t>(exact_precision_window_decimal_limb_width)};
		auto const value{decimal_limbs[i - 1u]};
		if (output_position == output_size)
		{
			tail_nonzero = tail_nonzero || value != 0u;
			continue;
		}
		auto const available{output_size - output_position};
		auto const take{width < available ? width : available};
		auto const skipped{width - take};
		auto const divisor{::fast_io::details::print_rsv_fp_pow10_0_to_19_table[skipped]};
		auto prefix{value / divisor};
		auto const begin{output_position};
		output_position += take;
		for (auto position{output_position}; position != begin;)
		{
			result.decimal.digits[--position] = static_cast<unsigned char>(prefix % 10u);
			prefix /= 10u;
		}
		if (skipped)
		{
			tail_nonzero = tail_nonzero || value % divisor != 0u;
		}
	}
	for (; output_position != requested_digits; ++output_position)
	{
		result.decimal.digits[output_position] = 0u;
	}
	result.decimal.size = requested_digits;
	result.decimal.exponent = result.real_exponent + 1 -
							  static_cast<::std::int_least32_t>(requested_digits);
	result.tail_nonzero = tail_nonzero;
	result.success = true;
	return result;
}

/*
Binary80 and binary128 can require more than eleven thousand exact coefficient
digits even when the caller requests only a short scientific field.  The
bounded window below encloses positive 5^s by two normalized 512-bit dyadic
endpoints.  Runtime and compiler-constant precision both supply the explicit
normal or subnormal binary exponent, and the reciprocal table covers negative
decimal scaling for large magnitudes.  The code accepts a prefix only when
shifting both endpoints produces the same integer.  Consequently approximation
affects only the acceptance rate: an accepted prefix is the exact floor.

For a requested window of r decimal digits and k=floor(log10(x)), set

  s = r - 1 - k,  y = x*10^s = M*2^(e+s)*5^s.

The window stores one guard digit, so r<=130 for every P1--P128 precision
request.  Since y<10^130<2^432, the 512-bit interval leaves at least eighty
bits beyond the result.  Each outward-rounded multiplication changes the
endpoint ratio by less than 1+2^-508.  At most thirteen power-table levels are
needed because |s|<=5095 for every finite binary80/binary128 value.  Induction
over interval multiplication bounds the final relative width by

  (1 + 2^-508)^(4*5095-1) - 1 < 2^-493,

and hence its absolute width after decimal scaling by less than 2^-61.  The
equality test remains authoritative even on the rare integer boundary where
that narrow interval could straddle two floors.

The positive table is generated by constexpr squaring from exact 5.  The
reciprocal table starts from the proved adjacent dyadics around 1/5 and applies
the same outward-rounded squaring recurrence.  Neither contains handwritten
cached constants.  The same arithmetic serves every output character type,
punctuation choice, format and rounding policy; only the existing presentation
layer interprets the prefix, guard and sticky state.
*/
#if defined(__SIZEOF_INT128__)
inline constexpr ::std::size_t exact_precision_wide_window_limb_count{8u};
inline constexpr ::std::size_t exact_precision_wide_window_product_limb_count{16u};
inline constexpr ::std::size_t exact_precision_wide_window_power_count{13u};
inline constexpr ::std::size_t exact_precision_wide_window_maximum_digits{130u};

struct exact_precision_wide_window_interval
{
	::std::uint_least64_t lower[exact_precision_wide_window_limb_count]{};
	::std::uint_least64_t upper[exact_precision_wide_window_limb_count]{};
	::std::int_least32_t exponent{};
};

struct exact_precision_wide_window_product
{
	::std::uint_least64_t limbs[exact_precision_wide_window_product_limb_count]{};
};

[[nodiscard]] inline constexpr unsigned exact_precision_wide_window_bit_width(
	::std::uint_least64_t value) noexcept
{
	// A nonzero 64-bit value has a count in [0, 63]. Keep both operands in the unsigned result domain so the
	// subtraction states that range proof directly and strict conversion diagnostics cannot mask real callers.
	return value ? 64u - static_cast<unsigned>(::std::countl_zero(value)) : 0u;
}

[[nodiscard]] inline constexpr exact_precision_wide_window_product
exact_precision_wide_window_multiply_endpoint(
	::std::uint_least64_t const *left,
	::std::uint_least64_t const *right) noexcept
{
	exact_precision_wide_window_product result{};
	for (::std::size_t left_index{};
		 left_index != exact_precision_wide_window_limb_count; ++left_index)
	{
		__uint128_t carry{};
		for (::std::size_t right_index{};
			 right_index != exact_precision_wide_window_limb_count; ++right_index)
		{
			auto const output_index{left_index + right_index};
			auto const product{
				static_cast<__uint128_t>(left[left_index]) * right[right_index] +
				result.limbs[output_index] + carry};
			result.limbs[output_index] =
				static_cast<::std::uint_least64_t>(product);
			carry = product >> 64u;
		}
		result.limbs[left_index + exact_precision_wide_window_limb_count] =
			static_cast<::std::uint_least64_t>(carry);
	}
	return result;
}

[[nodiscard]] inline constexpr unsigned exact_precision_wide_window_product_bit_width(
	exact_precision_wide_window_product const &product) noexcept
{
	for (auto index{exact_precision_wide_window_product_limb_count}; index; --index)
	{
		auto const value{product.limbs[index - 1u]};
		if (value)
		{
			return static_cast<unsigned>((index - 1u) * 64u) +
				::fast_io::details::exact_precision_wide_window_bit_width(value);
		}
	}
	return 0u;
}

struct exact_precision_wide_window_shifted_endpoint
{
	::std::uint_least64_t limbs[exact_precision_wide_window_limb_count]{};
	bool overflow{};
};

[[nodiscard]] inline constexpr exact_precision_wide_window_shifted_endpoint
exact_precision_wide_window_shift_endpoint(
	exact_precision_wide_window_product const &product,
	unsigned shift, bool round_up) noexcept
{
	exact_precision_wide_window_shifted_endpoint result{};
	auto const word_shift{static_cast<::std::size_t>(shift / 64u)};
	auto const bit_shift{shift % 64u};
	for (::std::size_t index{}; index != exact_precision_wide_window_limb_count;
		 ++index)
	{
		auto const source{word_shift + index};
		if (source >= exact_precision_wide_window_product_limb_count)
		{
			break;
		}
		auto value{product.limbs[source] >> bit_shift};
		if (bit_shift && source + 1u < exact_precision_wide_window_product_limb_count)
		{
			value |= product.limbs[source + 1u] << (64u - bit_shift);
		}
		result.limbs[index] = value;
	}
	if (!round_up || !shift)
	{
		return result;
	}
	bool discarded{};
	for (::std::size_t index{};
		 index != word_shift && index != exact_precision_wide_window_product_limb_count;
		 ++index)
	{
		discarded = discarded || product.limbs[index] != 0u;
	}
	if (bit_shift && word_shift < exact_precision_wide_window_product_limb_count)
	{
		auto const mask{
			(static_cast<::std::uint_least64_t>(1u) << bit_shift) - 1u};
		discarded = discarded || (product.limbs[word_shift] & mask) != 0u;
	}
	if (!discarded)
	{
		return result;
	}
	for (::std::size_t index{}; index != exact_precision_wide_window_limb_count;
		 ++index)
	{
		if (++result.limbs[index])
		{
			return result;
		}
	}
	result.overflow = true;
	return result;
}

[[nodiscard]] inline constexpr exact_precision_wide_window_interval
exact_precision_wide_window_multiply_interval(
	exact_precision_wide_window_interval const &left,
	exact_precision_wide_window_interval const &right) noexcept
{
	auto product{::fast_io::details::exact_precision_wide_window_multiply_endpoint(
		left.upper, right.upper)};
	auto const product_bits{
		::fast_io::details::exact_precision_wide_window_product_bit_width(product)};
	auto shift{product_bits <= 512u ? 0u : product_bits - 512u};
	auto upper{::fast_io::details::exact_precision_wide_window_shift_endpoint(
		product, shift, true)};
	if (upper.overflow)
	{
		++shift;
		upper = ::fast_io::details::exact_precision_wide_window_shift_endpoint(
			product, shift, true);
	}
	product = ::fast_io::details::exact_precision_wide_window_multiply_endpoint(
		left.lower, right.lower);
	auto const lower{::fast_io::details::exact_precision_wide_window_shift_endpoint(
		product, shift, false)};
	exact_precision_wide_window_interval result{};
	for (::std::size_t index{}; index != exact_precision_wide_window_limb_count;
		 ++index)
	{
		result.lower[index] = lower.limbs[index];
		result.upper[index] = upper.limbs[index];
	}
	result.exponent = left.exponent + right.exponent +
		static_cast<::std::int_least32_t>(shift);
	return result;
}

[[nodiscard]] inline constexpr exact_precision_wide_window_interval
exact_precision_wide_window_one() noexcept
{
	exact_precision_wide_window_interval result{};
	result.lower[exact_precision_wide_window_limb_count - 1u] =
		static_cast<::std::uint_least64_t>(1u) << 63u;
	result.upper[exact_precision_wide_window_limb_count - 1u] =
		static_cast<::std::uint_least64_t>(1u) << 63u;
	result.exponent = -511;
	return result;
}

[[nodiscard]] inline constexpr exact_precision_wide_window_interval
exact_precision_wide_window_power_base() noexcept
{
	exact_precision_wide_window_interval result{};
	// 5*2^509 is an exact normalized 512-bit significand.  The admitted
	// subnormal domain proves a positive decimal shift, so no reciprocal cache
	// is instantiated or stored.
	result.lower[exact_precision_wide_window_limb_count - 1u] =
		(static_cast<::std::uint_least64_t>(1u) << 63u) |
		(static_cast<::std::uint_least64_t>(1u) << 61u);
	result.upper[exact_precision_wide_window_limb_count - 1u] =
		result.lower[exact_precision_wide_window_limb_count - 1u];
	result.exponent = -509;
	return result;
}

/*
Construct the normalized enclosure of 5^-1 without floating arithmetic.
Writing

  1/5 = (2^514/5) * 2^-514

puts the significand in [2^511,2^512).  Base-2^64 long division computes
L=floor(2^514/5); because five does not divide a power of two, U=L+1 is the
strict upper endpoint.  Thus L*2^-514 < 1/5 < U*2^-514, the interval width is
one 512-bit ulp, and repeated outward-rounded squaring has the same enclosure
invariant as the positive-power table.  This reciprocal table is required for
large-magnitude wide values, where the decimal normalization shift is negative.
*/
[[nodiscard]] inline constexpr exact_precision_wide_window_interval
exact_precision_wide_window_reciprocal_base() noexcept
{
	exact_precision_wide_window_interval result{};
	::std::uint_least64_t remainder{};
	for (::std::size_t index{9u}; index; --index)
	{
		auto const source_index{index - 1u};
		auto const source_word{source_index == 8u
			? static_cast<::std::uint_least64_t>(4u)
			: static_cast<::std::uint_least64_t>(0u)};
		auto const dividend{(static_cast<__uint128_t>(remainder) << 64u) |
			source_word};
		auto const quotient{static_cast<::std::uint_least64_t>(dividend / 5u)};
		remainder = static_cast<::std::uint_least64_t>(dividend % 5u);
		if (source_index < exact_precision_wide_window_limb_count)
		{
			result.lower[source_index] = quotient;
		}
	}
	for (::std::size_t index{}; index != exact_precision_wide_window_limb_count;
		 ++index)
	{
		result.upper[index] = result.lower[index];
	}
	for (::std::size_t index{}; index != exact_precision_wide_window_limb_count;
		 ++index)
	{
		if (++result.upper[index])
		{
			break;
		}
	}
	result.exponent = -514;
	return result;
}

struct exact_precision_wide_window_power_table_type
{
	exact_precision_wide_window_interval values[
		exact_precision_wide_window_power_count]{};

	inline constexpr exact_precision_wide_window_power_table_type() noexcept
	{
		values[0] = ::fast_io::details::exact_precision_wide_window_power_base();
		for (::std::size_t index{1u};
			 index != exact_precision_wide_window_power_count; ++index)
		{
			values[index] = ::fast_io::details::
				exact_precision_wide_window_multiply_interval(
					values[index - 1u], values[index - 1u]);
		}
	}
};

inline constexpr exact_precision_wide_window_power_table_type
	exact_precision_wide_window_positive_power_table{};

struct exact_precision_wide_window_reciprocal_power_table_type
{
	exact_precision_wide_window_interval values[
		exact_precision_wide_window_power_count]{};

	inline constexpr exact_precision_wide_window_reciprocal_power_table_type() noexcept
	{
		values[0] =
			::fast_io::details::exact_precision_wide_window_reciprocal_base();
		for (::std::size_t index{1u};
			 index != exact_precision_wide_window_power_count; ++index)
		{
			values[index] = ::fast_io::details::
				exact_precision_wide_window_multiply_interval(
					values[index - 1u], values[index - 1u]);
		}
	}
};

inline constexpr exact_precision_wide_window_reciprocal_power_table_type
	exact_precision_wide_window_reciprocal_power_table{};

[[nodiscard]] inline constexpr exact_precision_wide_window_interval
exact_precision_wide_window_power5(unsigned exponent) noexcept
{
	auto result{::fast_io::details::exact_precision_wide_window_one()};
	// The first selected factor multiplied by the exact singleton [1,1] is
	// itself; direct assignment removes one 512x512 product without changing
	// either outward endpoint.
	bool initialized{};
	for (::std::size_t index{}; exponent; ++index, exponent >>= 1u)
	{
		if (exponent & 1u)
		{
			auto const &factor{
				exact_precision_wide_window_positive_power_table.values[index]};
			if (!initialized)
			{
				result = factor;
				initialized = true;
			}
			else
			{
				result = ::fast_io::details::
					exact_precision_wide_window_multiply_interval(result, factor);
			}
		}
	}
	return result;
}

[[nodiscard]] inline constexpr exact_precision_wide_window_interval
exact_precision_wide_window_reciprocal_power5(unsigned exponent) noexcept
{
	auto result{::fast_io::details::exact_precision_wide_window_one()};
	// As above, assignment is exactly multiplication by the singleton [1,1].
	bool initialized{};
	for (::std::size_t index{}; exponent; ++index, exponent >>= 1u)
	{
		if (exponent & 1u)
		{
				auto const &factor{
				::fast_io::details::
					exact_precision_wide_window_reciprocal_power_table.values[index]};
			if (!initialized)
			{
				result = factor;
				initialized = true;
			}
			else
			{
				result = ::fast_io::details::
					exact_precision_wide_window_multiply_interval(result, factor);
			}
		}
	}
	return result;
}

struct exact_precision_wide_window_mantissa_product
{
	::std::uint_least64_t limbs[10u]{};
};

template <typename mantissa_type>
[[nodiscard]] inline constexpr exact_precision_wide_window_mantissa_product
exact_precision_wide_window_multiply_mantissa(
	::std::uint_least64_t const *endpoint, mantissa_type mantissa) noexcept
{
	static_assert(sizeof(mantissa_type) <= sizeof(__uint128_t));
	auto const wide{static_cast<__uint128_t>(mantissa)};
	::std::uint_least64_t const words[2u]{
		static_cast<::std::uint_least64_t>(wide),
		static_cast<::std::uint_least64_t>(wide >> 64u)};
	exact_precision_wide_window_mantissa_product result{};
	for (::std::size_t word_index{}; word_index != 2u; ++word_index)
	{
		__uint128_t carry{};
		for (::std::size_t factor_index{};
			 factor_index != exact_precision_wide_window_limb_count; ++factor_index)
		{
			auto const output_index{word_index + factor_index};
			auto const product{static_cast<__uint128_t>(words[word_index]) *
				endpoint[factor_index] + result.limbs[output_index] + carry};
			result.limbs[output_index] =
				static_cast<::std::uint_least64_t>(product);
			carry = product >> 64u;
		}
		result.limbs[word_index + exact_precision_wide_window_limb_count] =
			static_cast<::std::uint_least64_t>(carry);
	}
	return result;
}

inline constexpr void exact_precision_wide_window_shift_mantissa_product(
	::std::uint_least64_t *output,
	exact_precision_wide_window_mantissa_product const &product,
	unsigned shift) noexcept
{
	auto const word_shift{static_cast<::std::size_t>(shift / 64u)};
	auto const bit_shift{shift % 64u};
	for (::std::size_t index{}; index != 10u; ++index)
	{
		auto const source{word_shift + index};
		if (10u <= source)
		{
			output[index] = 0u;
			continue;
		}
		auto value{product.limbs[source] >> bit_shift};
		if (bit_shift && source + 1u < 10u)
		{
			value |= product.limbs[source + 1u] << (64u - bit_shift);
		}
		output[index] = value;
	}
}

template <typename mantissa_type>
[[nodiscard]] inline constexpr unsigned exact_precision_wide_window_mantissa_bit_width(
	mantissa_type mantissa) noexcept
{
	auto const wide{static_cast<__uint128_t>(mantissa)};
	auto const high{static_cast<::std::uint_least64_t>(wide >> 64u)};
	if (high)
	{
		return 64u + ::fast_io::details::exact_precision_wide_window_bit_width(high);
	}
	return ::fast_io::details::exact_precision_wide_window_bit_width(
		static_cast<::std::uint_least64_t>(wide));
}

template <typename mantissa_type>
[[nodiscard]] inline constexpr unsigned exact_precision_wide_window_mantissa_countr_zero(
	mantissa_type mantissa) noexcept
{
	auto const wide{static_cast<__uint128_t>(mantissa)};
	auto const low{static_cast<::std::uint_least64_t>(wide)};
	if (low)
	{
		return static_cast<unsigned>(::std::countr_zero(low));
	}
	return 64u + static_cast<unsigned>(::std::countr_zero(
		static_cast<::std::uint_least64_t>(wide >> 64u)));
}

template <typename mantissa_type>
[[nodiscard]] inline constexpr bool
exact_precision_wide_window_scaled_tail_nonzero(
	mantissa_type mantissa, ::std::int_least32_t binary_exponent,
	::std::int_least32_t decimal_shift) noexcept
{
	auto reduced{static_cast<__uint128_t>(mantissa)};
	if (decimal_shift < 0)
	{
		auto count{static_cast<unsigned>(-decimal_shift)};
		/*
		A nonzero significand of at most 114 bits contains no more than 49
		factors of five.  Rejecting a larger denominator immediately avoids
		__uint128 division in the overwhelmingly common reciprocal-power case.
		For the remaining exact-power candidates, division by the constant five
		determines whether the decimal denominator was cancelled completely.
		*/
		if (49u < count)
		{
			return true;
		}
		for (; count; --count)
		{
			if (reduced % 5u)
			{
				return true;
			}
			reduced /= 5u;
		}
	}
	auto const scaled_binary_exponent{
		static_cast<::std::int_least64_t>(binary_exponent) + decimal_shift};
	if (0 <= scaled_binary_exponent)
	{
		return false;
	}
	auto const required_twos{
		static_cast<::std::uint_least64_t>(-scaled_binary_exponent)};
	if (sizeof(__uint128_t) *
			::std::numeric_limits<unsigned char>::digits <
		required_twos)
	{
		return true;
	}
	return ::fast_io::details::
		exact_precision_wide_window_mantissa_countr_zero(reduced) <
		required_twos;
}

[[nodiscard]] inline constexpr ::std::int_least32_t
exact_precision_wide_window_log10_pow2_seed(
	::std::int_least32_t binary_exponent) noexcept
{
	auto const product{static_cast<::std::int_least64_t>(binary_exponent) * 1262611};
	constexpr ::std::int_least64_t denominator{
		static_cast<::std::int_least64_t>(1u) << 22u};
	if (0 <= product)
	{
		return static_cast<::std::int_least32_t>(product / denominator);
	}
	return static_cast<::std::int_least32_t>(
		-((-product + denominator - 1u) / denominator));
}

// Outlining keeps the 512-bit interval state outside each presentation caller;
// the attribute affects placement only and cannot change an accepted prefix.
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
[[nodiscard]] inline constexpr exact_precision_window_result
exact_precision_wide_window_from_significand(
	__uint128_t mantissa, unsigned mantissa_bits,
	::std::int_least32_t binary_exponent,
	::std::size_t requested_digits) noexcept
{
	exact_precision_window_result failure{};
	if (!mantissa || !mantissa_bits || 114u < mantissa_bits ||
		!requested_digits ||
		exact_precision_wide_window_maximum_digits < requested_digits)
	{
		return failure;
	}
	auto const binary_floor{binary_exponent + static_cast<::std::int_least32_t>(
		::fast_io::details::exact_precision_wide_window_mantissa_bit_width(mantissa) - 1u)};
	auto real_exponent{
		::fast_io::details::exact_precision_wide_window_log10_pow2_seed(binary_floor)};
	for (unsigned attempt{}; attempt != 4u; ++attempt)
	{
		auto const decimal_shift64{
			static_cast<::std::int_least64_t>(requested_digits) - 1 - real_exponent};
		if (decimal_shift64 < -8191 || 8191 < decimal_shift64)
		{
			return failure;
		}
		auto const decimal_shift{static_cast<::std::int_least32_t>(decimal_shift64)};
		auto const power{decimal_shift < 0
			? ::fast_io::details::exact_precision_wide_window_reciprocal_power5(
				static_cast<unsigned>(-decimal_shift))
			: ::fast_io::details::exact_precision_wide_window_power5(
				static_cast<unsigned>(decimal_shift))};
		auto const scaled_exponent{
			static_cast<::std::int_least64_t>(power.exponent) + binary_exponent +
			decimal_shift};
		if (0 <= scaled_exponent || scaled_exponent < -640)
		{
			return failure;
		}
		auto const lower_product{
			::fast_io::details::exact_precision_wide_window_multiply_mantissa(
				power.lower, mantissa)};
		auto const upper_product{
			::fast_io::details::exact_precision_wide_window_multiply_mantissa(
				power.upper, mantissa)};
		::std::uint_least64_t lower[10u]{};
		::std::uint_least64_t upper[10u]{};
		auto const shift{static_cast<unsigned>(-scaled_exponent)};
		::fast_io::details::exact_precision_wide_window_shift_mantissa_product(
			lower, lower_product, shift);
		::fast_io::details::exact_precision_wide_window_shift_mantissa_product(
			upper, upper_product, shift);
		bool equal{true};
		::std::size_t size{10u};
		for (::std::size_t index{}; index != 10u; ++index)
		{
			equal = equal && lower[index] == upper[index];
		}
		if (!equal)
		{
			return failure;
		}
		while (size && !lower[size - 1u])
		{
			--size;
		}
		if (!size)
		{
			--real_exponent;
			continue;
		}
		auto const tail_nonzero{::fast_io::details::
			exact_precision_wide_window_scaled_tail_nonzero(
				mantissa, binary_exponent, decimal_shift)};
		auto generated{::fast_io::details::exact_precision_window_materialize(
			lower, size, requested_digits, real_exponent, tail_nonzero, false)};
		if (!generated.success)
		{
			return failure;
		}
		if (generated.real_exponent != real_exponent)
		{
			real_exponent = generated.real_exponent;
			continue;
		}
		return generated;
	}
	return failure;
}

/// @brief Retains the original binary80/binary128 subnormal-window entry.
/// @details Subnormal and minimum-normal values share this binary exponent;
///          callers with an explicit normal significand may therefore reuse
///          the same proved 512-bit interval without constructing the complete
///          eleven-thousand-digit exact coefficient.
[[nodiscard]] inline constexpr exact_precision_window_result
exact_precision_wide_subnormal_window_from_binary(
	__uint128_t mantissa, unsigned mantissa_bits,
	::std::size_t requested_digits) noexcept
{
	constexpr ::std::int_least32_t bias{16383};
	return ::fast_io::details::exact_precision_wide_window_from_significand(
		mantissa, mantissa_bits,
		1 - bias - static_cast<::std::int_least32_t>(mantissa_bits),
		requested_digits);
}
#endif

// The DA arithmetic below is target-independent.  This boolean is a
// code-generation and ABI policy, not a numerical capability test.
// Paired AB/BA runs on an i9-14900HX showed GCC 15 retaining the one-compare
// materializer miss shape while improving all four P18/P19 presentations by
// 25--45%. GCC 14 is deliberately below this bound: two physical-core AB/BA
// sweeps find no stable all-format win, while the candidate adds 1.5--1.7 KiB,
// 357--392 instructions and four calls. Clang 23 and GCC 13 showed link-order-sensitive miss layouts, so
// they retain byte-identical exact-materializer code until independently better
// placement is proved.  Only Linux System V x86-64 LP64 was measured; x32,
// MinGW, the Microsoft ABI and non-Linux x86-64 retain the exact path.
// GCC 15 is the continuous family lower bound: later GNU frontends inherit the
// accepted materializer unless a complete hit/miss and link-order counterexample
// is measured.  Revalidate frame size, calls, branch placement and both link
// orders before narrowing that policy.
inline constexpr bool binary64_p18_p19_materializer_enabled{
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && 15 <= __GNUC__ && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	true
#else
	false
#endif
};

// Materialize a completed DA P18/P19 coefficient into the compact numeric-digit
// contract shared by every decimal presentation.  The caller supplies the
// decomposed binary64 significand.  Normals have the implicit bit restored;
// subnormals fail before a cache lookup.  Converting the final binary exponent
// back to raw form proves [1, 2046]; every other representation is rejected.
//
// The cached decimal power is a lower endpoint.  Its proved one-sided error
// bound causes compute_binary64_scientific_precision to reject the complete
// interval which could cross one half, including every exact tie.  An accepted
// coefficient is therefore the common result of all six nearest policies.
// Failure writes nothing and leaves the existing exact prefix/guard/sticky
// materializer authoritative.  Keep this body outlined so the DA products,
// cache cursor and nineteen-byte scratch do not extend any miss-path live range.
// The noinline attribute changes placement only.  A compiler without that
// attribute may inline the identical arithmetic and write contract; it does
// does not enter the compiler/ISA policy above.
template <typename window_result_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr bool exact_precision_window_try_materialize_binary64_p18_p19(
	window_result_type &result, ::std::uint_least64_t mantissa,
	::std::int_least32_t binary_exponent, ::std::size_t significant) noexcept
{
	constexpr auto implicit_bit{static_cast<::std::uint_least64_t>(1u) << 52u};
	if (mantissa < implicit_bit || 19u < significant || significant < 18u)
	{
		return false;
	}
	auto const raw_exponent{binary_exponent + 1075};
	if (raw_exponent <= 0 || 2047 <= raw_exponent)
	{
		return false;
	}
	::fast_io::details::da::binary64_scientific_precision_result converted;
	if (significant == 18u)
	{
		converted = ::fast_io::details::da::compute_binary64_scientific_precision<18u>(
			mantissa, static_cast<::std::uint_least32_t>(raw_exponent));
	}
	else
	{
		converted = ::fast_io::details::da::compute_binary64_scientific_precision<19u>(
			mantissa, static_cast<::std::uint_least32_t>(raw_exponent));
	}
	if (!converted.success)
	{
		return false;
	}
	// recursive=true always emits nineteen positions.  P18 has one leading zero;
	// P19 begins at position zero.  Emit directly into the result, then convert
	// the selected spelling to numeric digits in place.  For P18, increasing
	// indices make the one-byte left shift safe: each source position is read
	// before that position is overwritten.  The nineteenth byte is outside
	// decimal.size and therefore does not participate in later rounding.
	::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
		result.decimal.digits, converted.significand, 19u);
	constexpr auto zero{::fast_io::char_literal_v<u8'0', unsigned char>};
	auto const source_offset{19u - significant};
	for (::std::size_t index{}; index != significant; ++index)
	{
		result.decimal.digits[index] = static_cast<unsigned char>(
			result.decimal.digits[index + source_offset] - zero);
	}
	result.real_exponent = converted.exponent;
	result.decimal.size = significant;
	result.decimal.exponent = converted.exponent + 1 -
		static_cast<::std::int_least32_t>(significant);
	result.tail_nonzero = false;
	result.success = true;
	return true;
}

// Positive binary64 block materialization owns a table walk plus prefix/sticky
// state.  Keep it behind an outlined boundary so compact-window callers do not
// inherit that frame or clone the loop.  Attribute availability changes only
// placement; failure and produced digits are identical without it.  The boolean
// policy is compile-time: binary32, directed policies, preserving policies and
// fractional precision instantiate the legacy body, while eligible binary64
// nearest policies add only one closed P18/P19 miss comparison.
template <bool da_nearest_eligible = false, typename window_result_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr bool exact_precision_window_materialize_positive_binary64(
	window_result_type &result, ::std::uint_least64_t mantissa,
	::std::uint_least32_t binary_exponent, ::std::size_t requested_digits) noexcept
{
	if constexpr (da_nearest_eligible)
	{
		if (requested_digits - 19u < 2u &&
			::fast_io::details::exact_precision_window_try_materialize_binary64_p18_p19(
				result, mantissa, static_cast<::std::int_least32_t>(binary_exponent),
				requested_digits - 1u))
		{
			return true;
		}
	}
	auto const index{static_cast<::std::uint_least32_t>((binary_exponent + 15u) / 16u)};
	if (::fast_io::details::ryu::table_size <= index)
	{
		return false;
	}
	auto const power10_bits{16u * index + 120u};
	auto const length{static_cast<::std::size_t>(
		(::fast_io::details::exact_precision_window_log10_pow2(16u * index) + 25u) / 9u)};
	bool found_nonzero{};
	bool tail_nonzero{};
	::std::size_t total_digits{};
	::std::size_t output_position{};
	for (auto i{length}; i; --i)
	{
		auto const block_index{i - 1u};
		auto const digits{::fast_io::details::exact_precision_window_mul_shift_mod1e9(
			mantissa << 8u,
			::fast_io::details::ryu::pow10_split[::fast_io::details::ryu::power_offset[index] + block_index],
			power10_bits - binary_exponent + 8u)};
		::std::size_t width{9u};
		if (!found_nonzero)
		{
			if (!digits)
			{
				continue;
			}
			found_nonzero = true;
			width = ::fast_io::details::exact_precision_window_decimal_limb_digits(digits);
			total_digits = width + block_index * 9u;
			result.real_exponent = static_cast<::std::int_least32_t>(total_digits - 1u);
		}
		if (output_position == requested_digits)
		{
			tail_nonzero = ::fast_io::details::exact_precision_window_positive_binary_tail_nonzero(
				mantissa, binary_exponent, total_digits);
			break;
		}
		auto const available{requested_digits - output_position};
		auto const take{width < available ? width : available};
		// `digits` is reduced modulo 10^9, so `width` and `take` are at most
		// nine and the tenth byte is never observed.  The shared uint64 digit
		// writer nevertheless has a defensive ten-digit switch arm.  GCC 15
		// retains that unreachable arm during interprocedural bounds analysis and
		// diagnoses a nine-byte object under -Wstringop-overflow.  Providing the
		// writer's full syntactic capacity removes that false positive without
		// changing the proved copy bound or the optimized code.
		char block[10u];
		::fast_io::details::print_rsv_fp_digits_len<double>(
			block, digits, static_cast<::std::uint_least32_t>(width));
		::fast_io::details::exact_precision_copy_character_digits_numeric(
			result.decimal.digits + output_position, block, take);
		output_position += take;
		if (output_position == requested_digits)
		{
			tail_nonzero = ::fast_io::details::exact_precision_window_positive_binary_tail_nonzero(
				mantissa, binary_exponent, total_digits - requested_digits);
			break;
		}
	}
	if (!found_nonzero)
	{
		return false;
	}
	for (; output_position != requested_digits; ++output_position)
	{
		result.decimal.digits[output_position] = 0u;
	}
	result.decimal.size = requested_digits;
	result.decimal.exponent = result.real_exponent + 1 -
							  static_cast<::std::int_least32_t>(requested_digits);
	result.tail_nonzero = tail_nonzero;
	result.success = true;
	return true;
}

template <typename window_result_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool exact_precision_window_materialize_negative_binary_core(
	window_result_type &result, ::std::uint_least64_t mantissa,
	::std::int_least32_t binary_exponent, ::std::size_t requested_digits,
	::std::int_least32_t real_exponent) noexcept
{
	if (0 <= binary_exponent || 0 <= real_exponent || !requested_digits)
	{
		return false;
	}
	auto const exponent_magnitude{static_cast<::std::uint_least32_t>(-binary_exponent)};
	auto const index{exponent_magnitude / 16u};
	if (index + 1u >= ::fast_io::details::ryu::table_size_2)
	{
		return false;
	}
	auto const shift{static_cast<::std::uint_least32_t>(
		::fast_io::details::ryu::addtional_bits_2 +
		(exponent_magnitude - 16u * index) + 8u)};
	auto const first_decimal_position{static_cast<::std::size_t>(-real_exponent)};
	auto block_index{static_cast<::std::uint_least32_t>((first_decimal_position - 1u) / 9u)};
	auto block_offset{static_cast<::std::uint_least32_t>((first_decimal_position - 1u) % 9u)};
	::std::size_t output_position{};
	while (output_position != requested_digits)
	{
		::std::uint_least32_t block{};
		auto const minimum_block{
			static_cast<::std::uint_least32_t>(::fast_io::details::ryu::min_block_2[index])};
		if (minimum_block <= block_index)
		{
			auto const power_index{static_cast<::std::uint_least32_t>(
				::fast_io::details::ryu::pow10_offset_2[index]) +
				block_index - minimum_block};
			if (power_index < static_cast<::std::uint_least32_t>(
					::fast_io::details::ryu::pow10_offset_2[index + 1u]))
			{
				block = ::fast_io::details::exact_precision_window_mul_shift_mod1e9(
					mantissa << 8u,
					::fast_io::details::ryu::pow10_split_2[power_index], shift);
			}
		}
		auto const available{static_cast<::std::size_t>(9u - block_offset)};
		auto const remaining{requested_digits - output_position};
		auto const take{available < remaining ? available : remaining};
		char block_digits[9u];
		if (::std::is_constant_evaluated())
		{
			// This constant-evaluation leaf deliberately avoids the shared digit-pair
			// table.  GCC 13 and 15 with ASan/UBSan reject a null check on a pointer
			// into that constexpr table even though the address is provably non-null.
			// `block` is reduced modulo 10^9 by the producer.  At every iteration,
			// Euclidean division gives block = 10*q + r with 0 <= r < 10; storing r
			// from right to left therefore emits its unique nine-digit expansion,
			// including leading zeroes.  Runtime evaluation retains the audited
			// fixed-width writer below, so this workaround has no runtime branch,
			// table access or instruction-selection effect after optimization.
			auto remaining_block{block};
			for (::std::size_t position{9u}; position;)
			{
				--position;
				block_digits[position] = ::fast_io::char_literal_add<char>(
					remaining_block % 10u);
				remaining_block /= 10u;
			}
		}
		else
		{
			::fast_io::details::print_rsv_fp_digits_len<double>(block_digits, block, 9u);
		}
		::fast_io::details::exact_precision_copy_character_digits_numeric(
			result.decimal.digits + output_position, block_digits + block_offset, take);
		output_position += take;
		block_offset = 0u;
		++block_index;
	}
	if (!result.decimal.digits[0])
	{
		return false;
	}
	result.decimal.size = requested_digits;
	result.decimal.exponent = real_exponent + 1 -
		static_cast<::std::int_least32_t>(requested_digits);
	result.real_exponent = real_exponent;
	auto const binary_factors{static_cast<::std::uint_least32_t>(::std::countr_zero(mantissa))};
	auto const terminating_decimal_position{static_cast<::std::size_t>(
		exponent_magnitude - (binary_factors < exponent_magnitude ? binary_factors : exponent_magnitude))};
	result.tail_nonzero = first_decimal_position + requested_digits - 1u <
		terminating_decimal_position;
	result.success = true;
	return true;
}

template <bool da_nearest_eligible = false, typename window_result_type>
inline constexpr bool exact_precision_window_materialize_mixed_binary(
	window_result_type &result, ::std::uint_least64_t mantissa,
	::std::int_least32_t binary_exponent, ::std::size_t requested_digits) noexcept
{
	if constexpr (da_nearest_eligible)
	{
		if (requested_digits - 19u < 2u &&
			::fast_io::details::exact_precision_window_try_materialize_binary64_p18_p19(
				result, mantissa, binary_exponent, requested_digits - 1u))
		{
			return true;
		}
	}
	// For 1 <= M*2^e with -52 <= e < 0, floor(M*2^e) fits in uint64_t.
	// Its remaining prefix comes from the same 1e9 fractional blocks as Ryu;
	// e-countr_zero(M) gives the exact terminating decimal position and sticky bit.
	if (0 <= binary_exponent || binary_exponent < -52 || !requested_digits)
	{
		return false;
	}
	auto const exponent_magnitude{static_cast<::std::uint_least32_t>(-binary_exponent)};
	auto const integer{exponent_magnitude < 64u ? mantissa >> exponent_magnitude : 0u};
	if (!integer)
	{
		return false;
	}
	auto const integer_digits{static_cast<::std::size_t>(
		::fast_io::details::chars_len<10u, true>(integer))};
	result.real_exponent = static_cast<::std::int_least32_t>(integer_digits - 1u);
	auto const integer_take{requested_digits < integer_digits ? requested_digits : integer_digits};
	char integer_buffer[16u];
	::fast_io::details::print_rsv_fp_digits_len<double>(
		integer_buffer, integer, static_cast<::std::uint_least32_t>(integer_digits));
	::fast_io::details::exact_precision_copy_character_digits_numeric(
		result.decimal.digits, integer_buffer, integer_take);
	auto const binary_factors{static_cast<::std::uint_least32_t>(::std::countr_zero(mantissa))};
	auto const terminating_fractional_position{static_cast<::std::size_t>(
		exponent_magnitude -
		(binary_factors < exponent_magnitude ? binary_factors : exponent_magnitude))};
	bool tail_nonzero{};
	if (requested_digits < integer_digits)
	{
		auto const discarded_integer_digits{integer_digits - requested_digits};
		auto const divisor{
			::fast_io::details::print_rsv_fp_pow10_0_to_19_table[discarded_integer_digits]};
		tail_nonzero = integer % divisor != 0u || terminating_fractional_position != 0u;
	}
	else
	{
		auto const requested_fractional_digits{requested_digits - integer_digits};
		auto const materialized_fractional_digits{
			requested_fractional_digits < terminating_fractional_position
				? requested_fractional_digits
				: terminating_fractional_position};
		auto const index{exponent_magnitude / 16u};
		if (index + 1u >= ::fast_io::details::ryu::table_size_2)
		{
			return false;
		}
		auto const shift{static_cast<::std::uint_least32_t>(
			::fast_io::details::ryu::addtional_bits_2 +
			(exponent_magnitude - 16u * index) + 8u)};
		::std::size_t output_position{integer_digits};
		::std::uint_least32_t block_index{};
		while (output_position != integer_digits + materialized_fractional_digits)
		{
			::std::uint_least32_t block{};
			auto const minimum_block{
				static_cast<::std::uint_least32_t>(::fast_io::details::ryu::min_block_2[index])};
			if (minimum_block <= block_index)
			{
				auto const power_index{static_cast<::std::uint_least32_t>(
					::fast_io::details::ryu::pow10_offset_2[index]) +
					block_index - minimum_block};
				if (power_index < static_cast<::std::uint_least32_t>(
						::fast_io::details::ryu::pow10_offset_2[index + 1u]))
				{
					block = ::fast_io::details::exact_precision_window_mul_shift_mod1e9(
						mantissa << 8u,
						::fast_io::details::ryu::pow10_split_2[power_index], shift);
				}
			}
			char block_digits[9u];
			::fast_io::details::print_rsv_fp_digits_len<double>(block_digits, block, 9u);
			auto const remaining{integer_digits + materialized_fractional_digits - output_position};
			auto const take{remaining < 9u ? remaining : 9u};
			::fast_io::details::exact_precision_copy_character_digits_numeric(
				result.decimal.digits + output_position, block_digits, take);
			output_position += take;
			++block_index;
		}
		tail_nonzero = requested_fractional_digits < terminating_fractional_position;
	}
	auto const exact_size{integer_digits + terminating_fractional_position};
	result.decimal.size = tail_nonzero || requested_digits < exact_size
		? requested_digits
		: exact_size;
	result.decimal.exponent = result.real_exponent + 1 -
		static_cast<::std::int_least32_t>(result.decimal.size);
	result.tail_nonzero = tail_nonzero;
	result.success = true;
	return true;
}

// Code-generation policy for direct block emission.  Apple Clang 23 on M4 and
// Linux System V x86-64 LP64 GCC 13--16/Clang 23 are the recorded matrix: the
// selected AArch64 and Clang-x86 negative-exponent shapes keep block-stream state
// in registers, whereas the disabled GCC-x86 shape carries stream state through
// the stack.  Other AArch64 frontends inherit a conservative ISA-family
// hypothesis. On x86, GCC 13 is a continuous family lower bound. Clang 22 and
// trunk Clang 24 direct-negative consumers expand from 16 calls to 28, so only
// the measured Clang 23 direct shape is selected; other ABIs use the
// compact exact window. Positive and mixed streams have separate gates
// because their live ranges differ.  Every disabled branch derives the same
// prefix, guard and sticky state.  Revalidate frame, spills, calls, block-loop
// throughput and linked text before moving a gate.
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
inline constexpr bool exact_precision_window_direct_negative_scientific{true};
inline constexpr bool exact_precision_window_direct_nonnegative_scientific{true};
#elif defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__clang__) && __clang_major__ == 23 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
inline constexpr bool exact_precision_window_direct_negative_scientific{true};
inline constexpr bool exact_precision_window_direct_nonnegative_scientific{false};
#elif defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__ && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
inline constexpr bool exact_precision_window_direct_negative_scientific{false};
inline constexpr bool exact_precision_window_direct_nonnegative_scientific{true};
#else
inline constexpr bool exact_precision_window_direct_negative_scientific{false};
inline constexpr bool exact_precision_window_direct_nonnegative_scientific{false};
#endif

// Code-generation policy for complete aligned negative-scientific limbs.  The
// fixed-width digit writer proves that block_digits contains exactly the nine
// base-10 characters of block.  Once a prior limb has emitted the leading
// digit and radix point, offset == 0 && count == 9 therefore makes copying the
// nine-object representation equivalent to the element loop below.  That edge
// cannot contain the guard (requested_end < block_end is then false), and its
// only remaining control effect is the materialized-end test preserved at the
// copy site.  Partial first/last limbs and constant evaluation retain the
// scalar path, so exponent correction, guard/sticky construction and rounding
// are unchanged.
//
// This is an empirical object-layout and register-allocation policy, not an
// AArch64 semantic or ISA-capability test.  Darwin LP64 Apple Clang 21,
// upstream Clang 22/23 and GCC 15 have physical-M4 timing evidence.  Nine
// independent Clang-22 P18/P32/P64/P128 runs over forty complete presentation
// cells improved the geometric aggregate by 1.85%; the P64 and P128 aggregates
// improved by 2.30% and 3.40%, respectively.  The linked text increase was
// eight bytes, and 163,840 paired outputs were byte-identical.  AArch64 ELF
// Clang 19, 20 and trunk produce the same complete consumer instructions,
// frame, calls and text with the policy forced on or off, so excluding those
// versions would change no generated code.  Clang 21/23 cross objects for Apple
// M1, Cortex A76/A78/X1 and Neoverse N1/V1/N2 inline all five character-width
// copies without a new relocation or callee-save register, and their exact
// packed block improves the corresponding llvm-mca models.  The continuous
// Clang-family policy therefore avoids both a hole between measured releases
// and a speculative upper bound; a future measured counterexample must narrow
// it.  Linux LP64 GCC 15 remains deliberately excluded: its char and char8_t
// functions add a 16-byte frame and its wide matrix adds 584--872 text bytes
// with broader register-allocation movement. Isolated scientific-preserve
// target functions from Compiler Explorer prove that the x86 transformation is
// not code-generation-neutral outside 23: Clang 22 changes 423 instructions to
// 441, and trunk Clang 24 changes 422 to 440, with different normalized opcode
// digests in both cases. Neither compiler has paired x86 latency admission for
// that larger body, so the Linux x86 selection remains exact Clang 23. Admit
// another major only after real paired measurement. Other compiler families and
// ABIs use the equivalent scalar edge.
#if (defined(__aarch64__) || defined(__arm64__)) && defined(__LP64__) && \
	(defined(__APPLE__) || defined(__linux__)) && defined(__clang__)
inline constexpr bool exact_precision_window_staged_negative_aligned_memcpy{true};
#elif (defined(__aarch64__) || defined(__arm64__)) && defined(__LP64__) && \
	defined(__APPLE__) && defined(__GNUC__) && !defined(__clang__) && \
	15 <= __GNUC__
inline constexpr bool exact_precision_window_staged_negative_aligned_memcpy{true};
#elif defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__clang__) && __clang_major__ == 23 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
inline constexpr bool exact_precision_window_staged_negative_aligned_memcpy{true};
#else
inline constexpr bool exact_precision_window_staged_negative_aligned_memcpy{false};
#endif

// GCC 13--16 Linux System V x86-64 LP64 keep the boundary-carrier and mixed-
// stream state in the measured register allocation.  GCC 13 is the continuous
// lower bound, so later GNU frontends inherit the GCC-16 placement.  Other ABIs
// reuse their nonnegative-stream decision and compact exact fallback.  This
// policy changes dispatch placement only, not arithmetic or rounding.
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__ && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
inline constexpr bool exact_precision_window_direct_boundary_carrier{true};
inline constexpr bool exact_precision_window_direct_mixed_scientific{true};
#else
inline constexpr bool exact_precision_window_direct_boundary_carrier{false};
inline constexpr bool exact_precision_window_direct_mixed_scientific{
	exact_precision_window_direct_nonnegative_scientific};
#endif

// Clang 23 x86-64 benefits from direct nonnegative and mixed block streams once
// the requested scientific coefficient reaches 35 digits.  Two independently
// linked AB/BA binaries (20 balanced samples per cell) improved P35-P128 by
// 11.7--12.4% on the common corpus and 6.0% on the broad-exponent corpus.  The
// P34 control remained within 2.5%.  GCC 13 and GCC 15 instead regressed their
// broad P65-P128 ranges by 3.2--5.6%. Clang 22 and trunk Clang 24 increase the
// isolated scientific-preserve consumer from 16 calls to 30 when this stream is
// forced, so only measured Clang 23 selects it. Other majors and ABIs retain the
// compact exact window until paired P35-P128 data admit them.
// Both paths derive the same prefix, guard and sticky bits and share the same
// rounding writer.  The gate therefore changes code generation, not semantics.
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__clang__) && __clang_major__ == 23 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
inline constexpr bool exact_precision_window_long_nonnegative_scientific{true};
inline constexpr bool exact_precision_window_long_mixed_scientific{true};
#else
inline constexpr bool exact_precision_window_long_nonnegative_scientific{false};
inline constexpr bool exact_precision_window_long_mixed_scientific{false};
#endif

// The exact scaled negative-scientific specialization is enabled as an AArch64
// family policy.  Apple Clang 23 on M4 keeps the u128 multiply/shift result
// register-resident in the measured caller; other AArch64 frontends inherit a
// conservative hypothesis and must be re-audited.  Other ISAs retain the exact
// block stream.  The arithmetic domain checks remain authoritative either way.
#if defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
inline constexpr bool exact_precision_window_scaled_negative_scientific{true};
#else
inline constexpr bool exact_precision_window_scaled_negative_scientific{false};
#endif

// Binary32 carries less mantissa/block state than binary64, and the audited
// direct stream remains register-resident on every enabled target.  This is a
// code-generation observation, not a mathematical property of binary32; retain
// the unconditional enable only while the compiler matrix confirms that shape.
inline constexpr bool exact_precision_window_direct_binary32_negative_scientific{true};
inline constexpr bool exact_precision_window_direct_binary32_positive_scientific{true};

// Direct positive-integer emission is disabled on GCC x86-64 because the
// measured inline block writer expands the caller's frame and dependency chain.
// GCC 13--15 instead use a terminal outlined writer. Paired broad-corpus runs
// improve GCC 13/15 fixed and decimal P15-P128 by 6.5--10.2%; a separate
// physical-core GCC 14 P15-P32 fractional-preserve audit improves the broad
// corpus by 10.2--10.4% and the combined corpus by 5.1%, with common values
// neutral. The GCC 14 boundary costs 524 linked-text bytes, 141 instructions
// and one call, a bounded tradeoff for the measured latency win. P1-P14 miss
// controls remain bounded and every output hash agrees. The measured GCC 16
// negative transition keeps the exact fallback; GCC 17 and later inherit the
// latest profitable GNU outline unless a whole-caller counterexample is
// measured. Other ABIs retain the fallback.
// Every choice emits the identical integer and zero suffix; this is compiler
// scheduling policy, not a rounding rule.
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__GNUC__) && !defined(__clang__) && !(defined(__arm64ec__) || defined(_M_ARM64EC))
inline constexpr bool exact_precision_window_direct_positive_fixed{false};
#else
inline constexpr bool exact_precision_window_direct_positive_fixed{true};
#endif

inline constexpr bool exact_precision_window_outlined_positive_fixed{
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && \
	13 <= __GNUC__ && __GNUC__ != 16 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	true
#else
	false
#endif
};

struct exact_precision_scaled_fixed_pow5_table_type
{
	::std::uint_least64_t values[18u]{};

	inline constexpr exact_precision_scaled_fixed_pow5_table_type() noexcept
	{
		values[0] = 1u;
		for (::std::size_t index{1u}; index != 18u; ++index)
		{
			values[index] = values[index - 1u] * 5u;
		}
	}

	[[nodiscard]] inline constexpr ::std::uint_least64_t operator[](
		::std::size_t index) const noexcept
	{
		return values[index];
	}
};

inline constexpr exact_precision_scaled_fixed_pow5_table_type
	exact_precision_scaled_fixed_pow5_table{};

struct exact_precision_scaled_fixed_wide_pow5_table_type
{
	__uint128_t values[33u]{};

	inline constexpr exact_precision_scaled_fixed_wide_pow5_table_type() noexcept
	{
		values[0] = 1u;
		for (::std::size_t index{1u}; index != 33u; ++index)
		{
			values[index] = values[index - 1u] * 5u;
		}
	}

	[[nodiscard]] inline constexpr __uint128_t operator[](
		::std::size_t index) const noexcept
	{
		return values[index];
	}
};

inline constexpr exact_precision_scaled_fixed_wide_pow5_table_type
	exact_precision_scaled_fixed_wide_pow5_table{};

template <::std::integral char_type>
inline constexpr ::std::size_t exact_precision_window_print_u128_digits(
	char_type *digits, __uint128_t value) noexcept
{
	constexpr auto uint64_max{(::std::numeric_limits<::std::uint_least64_t>::max)()};
	if (value <= uint64_max)
	{
		auto const value64{static_cast<::std::uint_least64_t>(value)};
		auto const length{static_cast<::std::size_t>(
			::fast_io::details::chars_len<10u, false>(value64))};
		::fast_io::details::jeaiii::jeaiii_main_len<false>(
			digits, value64, static_cast<::std::uint_least32_t>(length));
		return length;
	}
	auto const low_division{
		::fast_io::details::exact_precision_window_divide_128_by_decimal_limb_full(value)};
	if (!low_division.quotient)
	{
		auto const length{::fast_io::details::exact_precision_window_decimal_limb_digits(
			low_division.remainder)};
		::fast_io::details::jeaiii::jeaiii_main_len<false>(digits,
			low_division.remainder, static_cast<::std::uint_least32_t>(length));
		return length;
	}
	auto const high_division{
		::fast_io::details::exact_precision_window_divide_128_by_decimal_limb_full(
			low_division.quotient)};
	::std::size_t length{};
	if (!high_division.quotient)
	{
		auto const high{static_cast<::std::uint_least64_t>(low_division.quotient)};
		length = ::fast_io::details::exact_precision_window_decimal_limb_digits(high);
		::fast_io::details::jeaiii::jeaiii_main_len<false>(digits, high,
			static_cast<::std::uint_least32_t>(length));
	}
	else
	{
		auto const high{static_cast<::std::uint_least64_t>(high_division.quotient)};
		length = ::fast_io::details::exact_precision_window_decimal_limb_digits(high);
		::fast_io::details::jeaiii::jeaiii_main_len<false>(digits, high,
			static_cast<::std::uint_least32_t>(length));
		::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
			digits + length, high_division.remainder, 19u);
		length += 19u;
	}
	::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
		digits + length, low_division.remainder, 19u);
	return length + 19u;
}

// These thresholds choose between exact scaled/prefix writers after all
// mathematical product and shift bounds have passed.  Apple Clang 23/M4 and the
// measured x86-64 compilers use different cutoffs to preserve register residency
// and caller branch shapes.  Later Apple frontends inherit the Apple-AArch64
// values as a conservative layout hypothesis.  They are assembly policy, not
// correctness limits: revalidate frame size, text size, dependency chains and
// both hit/miss branches per target before changing either value.
#if defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
inline constexpr ::std::size_t exact_precision_scaled_fixed_full_exponent_precision_limit{20u};
inline constexpr ::std::size_t exact_precision_scaled_fixed_fraction_exact_minimum{29u};
#else
inline constexpr ::std::size_t exact_precision_scaled_fixed_full_exponent_precision_limit{26u};
inline constexpr ::std::size_t exact_precision_scaled_fixed_fraction_exact_minimum{};
#endif

// The scaled fixed specialization owns a u128 product and a 39-character digit
// staging area after the proved exponent/precision gate.  Outlining prevents
// that frame from joining the general precision dispatcher.  The target
// thresholds above are benchmark-derived; the optional placement attribute is
// not itself an arithmetic precondition.  On a compiler without `noinline`, the
// exact quotient/remainder rounding and resulting fixed field are unchanged.
template <bool wide_power, bool comma,
	::fast_io::manipulators::floating_rounding rounding, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *exact_precision_window_try_print_scaled_binary64_fixed(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::int_least32_t binary_exponent, ::std::size_t precision,
	bool negative) noexcept
{
	// For x = mantissa * 2^binary_exponent, scaling by p decimal places is exact:
	//
	//   x * 10^p = (mantissa * 5^p) * 2^(binary_exponent + p).
	//
	// The pow5 multiplication introduces no approximation.  A negative final
	// binary exponent yields the exact quotient/remainder for rounding by a power
	// of two; a nonnegative exponent yields the exact integer by a left shift.  The
	// caller's precision/exponent gate proves the selected pow5 product, shift and
	// possible rounding carry fit u128.  Rejection uses exact block materialization.
	__uint128_t product;
	if constexpr (wide_power)
	{
		product = static_cast<__uint128_t>(mantissa) *
			::fast_io::details::exact_precision_scaled_fixed_wide_pow5_table[precision];
	}
	else
	{
		product = static_cast<__uint128_t>(mantissa) *
			::fast_io::details::exact_precision_scaled_fixed_pow5_table[precision];
	}
	auto const scaled_exponent{binary_exponent +
		static_cast<::std::int_least32_t>(precision)};
	__uint128_t scaled;
	::std::uint_least64_t remainder{};
	::std::uint_least64_t divisor{1u};
	if (scaled_exponent < 0)
	{
		auto const shift{static_cast<unsigned>(-scaled_exponent)};
		divisor <<= shift;
		scaled = product >> shift;
		remainder = static_cast<::std::uint_least64_t>(product & (divisor - 1u));
	}
	else
	{
		scaled = product << static_cast<unsigned>(scaled_exponent);
	}
	if (::fast_io::details::print_rsv_fp_decimal_round_up<rounding>(
			negative, static_cast<::std::uint_least64_t>(scaled), remainder, divisor))
	{
		++scaled;
	}
	char_type digits[39u];
	auto const length{
		::fast_io::details::exact_precision_window_print_u128_digits(digits, scaled)};
	if (length <= precision)
	{
		*iter++ = char_literal_v<u8'0', char_type>;
		*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
		iter = ::fast_io::details::fill_zeros_impl(iter, precision - length);
		for (::std::size_t position{}; position != length; ++position)
		{
			*iter++ = digits[position];
		}
	}
	else
	{
		auto const integer_length{length - precision};
		for (::std::size_t position{}; position != integer_length; ++position)
		{
			*iter++ = digits[position];
		}
		*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
		for (auto position{integer_length}; position != length; ++position)
		{
			*iter++ = digits[position];
		}
	}
	return iter;
}

// This scientific specialization similarly carries a u128 product, exact
// remainder and decimal carry writer only after its proved negative-exponent
// checks succeed.  Keeping it out of line is a frame/live-range hypothesis that
// must be rebenchmarked independently of the mathematical domain below.  If the
// attribute is unavailable, inlining preserves every rejection, tie decision
// and emitted exponent.
template <bool small_result, typename flt, bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_rounding rounding, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *exact_precision_window_try_print_scaled_binary64_scientific(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::int_least32_t binary_exponent, ::std::int_least32_t real_exponent,
	::std::size_t significant, bool negative, bool corrected_exponent = false) noexcept
{
	if (0 <= binary_exponent || 0 <= real_exponent || !significant)
	{
		return nullptr;
	}
	auto const scale_distance{static_cast<::std::int_least64_t>(significant) - 1 -
		static_cast<::std::int_least64_t>(real_exponent)};
	if (scale_distance < 0 || 28 < scale_distance)
	{
		return nullptr;
	}
	auto const scale{static_cast<::std::size_t>(scale_distance)};
	auto const scaled_exponent{binary_exponent +
		static_cast<::std::int_least32_t>(scale)};
	if (0 <= scaled_exponent || scaled_exponent < -63)
	{
		return nullptr;
	}
	// scale_distance = significant - 1 - real_exponent places the leading digit
	// on the requested scientific grid.  The exact identity is
	//
	//   x * 10^scale = (mantissa * 5^scale) * 2^(binary_exponent + scale).
	//
	// Limiting scale to 28 bounds 5^scale and the u128 product.  Requiring the
	// final power of two in [-63, -1] keeps the exact divisor and remainder in
	// u64.  Thus no approximation enters the rounding comparison; rejected
	// inputs continue through exact block materialization.
	auto const product{static_cast<__uint128_t>(mantissa) *
		::fast_io::details::exact_precision_scaled_fixed_wide_pow5_table[scale]};
	auto const shift{static_cast<unsigned>(-scaled_exponent)};
	auto const divisor{static_cast<::std::uint_least64_t>(1u) << shift};
	auto scaled{product >> shift};
	auto const remainder{static_cast<::std::uint_least64_t>(product & (divisor - 1u))};
	if (::fast_io::details::print_rsv_fp_decimal_round_up<rounding>(
			negative, static_cast<::std::uint_least64_t>(scaled), remainder, divisor))
	{
		++scaled;
	}
	if constexpr (small_result)
	{
		// A result with at most nineteen significant digits is below 10^19,
		// including the one-decade carry case.  Keep this hot precision window
		// on the u64 jeaiii writer.  Writing at iter + 1 lets the first digit
		// move left over the generated field while the point replaces it, so
		// this path needs neither a temporary array nor a second digit copy.
		auto const scaled64{static_cast<::std::uint_least64_t>(scaled)};
		auto const length{static_cast<::std::size_t>(
			::fast_io::details::chars_len<10u, false>(scaled64))};
		if (length < significant)
		{
			// A shortest carrier can place an exact power-of-ten boundary one
			// decade too high. Recompute once at the exact decimal exponent.
			if (corrected_exponent)
			{
				return nullptr;
			}
			return ::fast_io::details::
				exact_precision_window_try_print_scaled_binary64_scientific<
					small_result, flt, comma, uppercase_e, rounding>(iter, mantissa,
						binary_exponent, real_exponent - 1, significant, negative, true);
		}
		if (significant + 1u < length)
		{
			return nullptr;
		}
		::fast_io::details::jeaiii::jeaiii_main_len<false>(
			iter + 1, scaled64, static_cast<::std::uint_least32_t>(length));
		*iter = iter[1];
		auto output_end{iter + 1};
		if (1u < significant)
		{
			iter[1] = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
			output_end = iter + significant + 1u;
		}
		if (length == significant + 1u)
		{
			++real_exponent;
		}
		return ::fast_io::details::print_rsv_fp_e_impl<flt, uppercase_e>(
			output_end, real_exponent);
	}
	else
	{
		char_type digits[39u];
		auto const length{
			::fast_io::details::exact_precision_window_print_u128_digits(digits, scaled)};
		if (length < significant)
		{
			// A shortest carrier can place an exact power-of-ten boundary one
			// decade too high. Recompute once at the exact decimal exponent.
			if (corrected_exponent)
			{
				return nullptr;
			}
			return ::fast_io::details::
				exact_precision_window_try_print_scaled_binary64_scientific<
					small_result, flt, comma, uppercase_e, rounding>(iter, mantissa,
						binary_exponent, real_exponent - 1, significant, negative, true);
		}
		if (significant + 1u < length)
		{
			return nullptr;
		}
		*iter++ = digits[0];
		if (1u < significant)
		{
			*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
			for (::std::size_t position{1u}; position != significant; ++position)
			{
				*iter++ = digits[position];
			}
		}
		if (length == significant + 1u)
		{
			++real_exponent;
		}
		return ::fast_io::details::print_rsv_fp_e_impl<flt, uppercase_e>(
			iter, real_exponent);
	}
}

template <bool comma, bool json_float,
	::fast_io::manipulators::floating_rounding rounding, ::std::integral char_type>
inline constexpr char_type *exact_precision_window_try_print_exact_mixed_fixed(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::int_least32_t binary_exponent, ::std::size_t precision,
	bool negative) noexcept
{
	if (0 <= binary_exponent)
	{
		return nullptr;
	}
	auto const exponent_magnitude{static_cast<::std::uint_least32_t>(-binary_exponent)};
	auto const index{exponent_magnitude / 16u};
	// Reject exponents outside the generated Ryu block domain before touching
	// the destination.  Binary64 subnormals remain inside this bound.
	if (index + 1u >= ::fast_io::details::ryu::table_size_2)
	{
		return nullptr;
	}
	auto const integer{exponent_magnitude < 64u ? mantissa >> exponent_magnitude : 0u};
	auto const integer_digits{static_cast<::std::uint_least32_t>(
		::fast_io::details::chars_len<10u, true>(integer))};
	auto const binary_factors{static_cast<::std::uint_least32_t>(::std::countr_zero(mantissa))};
	auto const terminating_fractional_position{static_cast<::std::size_t>(
		exponent_magnitude -
		(binary_factors < exponent_magnitude ? binary_factors : exponent_magnitude))};
	auto const exact{terminating_fractional_position <= precision};
	if (!exact && precision == (::std::numeric_limits<::std::size_t>::max)())
	{
		return nullptr;
	}
	auto const materialized_fractional_position{
		exact ? terminating_fractional_position : precision + 1u};
	auto const begin{iter};
	::fast_io::details::print_rsv_fp_digits_len<double>(iter, integer, integer_digits);
	iter += integer_digits;
	if (precision)
	{
		*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	}
	unsigned guard{};
	if (materialized_fractional_position)
	{
		auto const shift{static_cast<::std::uint_least32_t>(
			::fast_io::details::ryu::addtional_bits_2 +
			(exponent_magnitude - 16u * index) + 8u)};
		auto const minimum_block{
			static_cast<::std::uint_least32_t>(::fast_io::details::ryu::min_block_2[index])};
		// Blocks before min_block_2 are known to be zero.  Emit that prefix in
		// one fill instead of entering the reciprocal multiply loop once per
		// nine zeroes; this is the common fixed-precision subnormal case.
		auto const leading_zero_end{static_cast<::std::size_t>(minimum_block) * 9u};
		auto const skipped_zero_end{materialized_fractional_position < leading_zero_end
			? materialized_fractional_position : leading_zero_end};
		auto const output_zero_end{skipped_zero_end < precision
			? skipped_zero_end : precision};
		iter = ::fast_io::details::fill_zeros_impl(iter, output_zero_end);
		::std::size_t output_position{skipped_zero_end};
		::std::uint_least32_t block_index{minimum_block};
		while (output_position != materialized_fractional_position)
		{
			::std::uint_least32_t block{};
			if (minimum_block <= block_index)
			{
				auto const power_index{static_cast<::std::uint_least32_t>(
					::fast_io::details::ryu::pow10_offset_2[index]) +
					block_index - minimum_block};
				if (power_index < static_cast<::std::uint_least32_t>(
						::fast_io::details::ryu::pow10_offset_2[index + 1u]))
				{
					block = ::fast_io::details::exact_precision_window_mul_shift_mod1e9(
						mantissa << 8u,
						::fast_io::details::ryu::pow10_split_2[power_index], shift);
				}
			}
			auto const remaining{materialized_fractional_position - output_position};
			auto const take{remaining < 9u ? remaining : 9u};
			auto const output_take{output_position < precision
				? (take < precision - output_position ? take : precision - output_position)
				: 0u};
			if (output_take == 9u)
			{
				::fast_io::details::print_rsv_fp_digits_len<double>(iter, block, 9u);
			}
			else if (output_take)
			{
				char_type block_digits[9u];
				::fast_io::details::print_rsv_fp_digits_len<double>(block_digits, block, 9u);
				for (::std::size_t position{}; position != output_take; ++position)
				{
					iter[position] = block_digits[position];
				}
			}
			if (!exact && output_position <= precision && precision < output_position + take)
			{
				guard = ::fast_io::details::exact_precision_window_decimal_limb_digit(
					block, static_cast<unsigned>(precision - output_position));
			}
			iter += output_take;
			output_position += take;
			++block_index;
		}
	}
	if (exact)
	{
		iter = ::fast_io::details::fill_zeros_impl(
			iter, precision - terminating_fractional_position);
	}
	else
	{
		auto const tail_nonzero{precision + 1u < terminating_fractional_position};
		auto const discarded_nonzero{guard != 0u || tail_nonzero};
		bool round_up{};
		if (discarded_nonzero)
		{
			if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
			{
				if (5u < guard || (guard == 5u && tail_nonzero))
				{
					round_up = true;
				}
				else if (guard == 5u)
				{
					auto const zero{char_literal_v<u8'0', char_type>};
					auto const rounded_down{static_cast<::std::uint_least64_t>(iter[-1] - zero)};
					round_up = ::fast_io::details::print_rsv_fp_decimal_tie_round_up<rounding>(
						negative, rounded_down);
				}
			}
			else
			{
				round_up = ::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
			}
		}
		if (round_up)
		{
			auto const zero{char_literal_v<u8'0', char_type>};
			auto const nine{char_literal_v<u8'9', char_type>};
			auto const point{char_literal_v<(comma ? u8',' : u8'.'), char_type>};
			auto position{iter};
			while (position != begin)
			{
				--position;
				if (*position == point)
				{
					continue;
				}
				if (*position != nine)
				{
					*position = static_cast<char_type>(*position + 1);
					round_up = false;
					break;
				}
				*position = zero;
			}
			if (round_up)
			{
				for (auto move{iter}; move != begin; --move)
				{
					*move = move[-1];
				}
				*begin = char_literal_v<u8'1', char_type>;
				++iter;
			}
		}
	}
	if constexpr (json_float)
	{
		if (!precision)
		{
			iter = ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(iter);
		}
	}
	return iter;
}

// The significant fixed/decimal probe is shared by all rounding policies.  Its
// digit blocks, guard digit, sticky predicate and carry scan are policy
// independent; only this final Boolean differs.  Nearest policies bypass the
// switch for every non-tie, while directed policies enter it only after proving
// that discarded digits are nonzero.  This preserves the exact guard/sticky
// proof above without cloning the complete block stream ten times.
[[nodiscard]] inline constexpr bool exact_precision_window_runtime_round_up(
	::fast_io::manipulators::floating_rounding rounding, bool negative,
	unsigned guard, bool tail_nonzero,
	::std::uint_least64_t rounded_down) noexcept
{
	using enum ::fast_io::manipulators::floating_rounding;
	// The public enum intentionally stores the six nearest policies first.  Keep
	// the comparison branchless, but make that ABI-independent source assumption
	// fail at compile time if the declaration is ever reordered.
	static_assert(static_cast<unsigned>(nearest_to_even) == 0u &&
		static_cast<unsigned>(nearest_to_odd) == 1u &&
		static_cast<unsigned>(nearest_toward_plus_infinity) == 2u &&
		static_cast<unsigned>(nearest_toward_minus_infinity) == 3u &&
		static_cast<unsigned>(nearest_toward_zero) == 4u &&
		static_cast<unsigned>(nearest_away_from_zero) == 5u);
	auto const nearest{static_cast<unsigned>(rounding) <=
		static_cast<unsigned>(nearest_away_from_zero)};
	if (nearest)
	{
		if (guard < 5u)
		{
			return false;
		}
		if (5u < guard || tail_nonzero)
		{
			return true;
		}
		switch (rounding)
		{
		case nearest_to_even:
			return (rounded_down & 1u) != 0u;
		case nearest_to_odd:
			return (rounded_down & 1u) == 0u;
		case nearest_toward_plus_infinity:
			return !negative;
		case nearest_toward_minus_infinity:
			return negative;
		case nearest_toward_zero:
			return false;
		case nearest_away_from_zero:
			return true;
		default:
			::fast_io::fast_terminate();
		}
	}
	switch (rounding)
	{
	case toward_plus_infinity:
		return !negative;
	case toward_minus_infinity:
		return negative;
	case toward_zero:
		return false;
	case away_from_zero:
		return true;
	default:
		// current_environment is resolved by the public precision dispatcher
		// before this internal exact writer can be selected.
		::fast_io::fast_terminate();
	}
}

// This writer is reached only from the P>=64 significant probe.  A mixed
// binary64 value has at most sixteen integer digits, hence its fractional
// precision is positive; a value below one has F=P+(-e-1)>=64.  JSON's
// integer-only suffix is therefore unreachable here and remains a single
// runtime decision in the outer significant probe instead of doubling these
// block-stream instantiations.
template <bool comma, ::std::integral char_type>
inline constexpr char_type *exact_precision_window_try_print_exact_mixed_fixed_runtime(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::int_least32_t binary_exponent, ::std::size_t precision,
	bool negative, ::fast_io::manipulators::floating_rounding rounding) noexcept
{
	if (0 <= binary_exponent)
	{
		return nullptr;
	}
	auto const exponent_magnitude{static_cast<::std::uint_least32_t>(-binary_exponent)};
	auto const index{exponent_magnitude / 16u};
	if (index + 1u >= ::fast_io::details::ryu::table_size_2)
	{
		return nullptr;
	}
	auto const integer{exponent_magnitude < 64u ? mantissa >> exponent_magnitude : 0u};
	auto const integer_digits{static_cast<::std::uint_least32_t>(
		::fast_io::details::chars_len<10u, true>(integer))};
	auto const binary_factors{static_cast<::std::uint_least32_t>(::std::countr_zero(mantissa))};
	auto const terminating_fractional_position{static_cast<::std::size_t>(
		exponent_magnitude -
		(binary_factors < exponent_magnitude ? binary_factors : exponent_magnitude))};
	auto const exact{terminating_fractional_position <= precision};
	if (!exact && precision == (::std::numeric_limits<::std::size_t>::max)())
	{
		return nullptr;
	}
	auto const materialized_fractional_position{
		exact ? terminating_fractional_position : precision + 1u};
	auto const begin{iter};
	::fast_io::details::print_rsv_fp_digits_len<double>(iter, integer, integer_digits);
	iter += integer_digits;
	if (precision)
	{
		*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	}
	unsigned guard{};
	if (materialized_fractional_position)
	{
		auto const shift{static_cast<::std::uint_least32_t>(
			::fast_io::details::ryu::addtional_bits_2 +
			(exponent_magnitude - 16u * index) + 8u)};
		auto const minimum_block{
			static_cast<::std::uint_least32_t>(::fast_io::details::ryu::min_block_2[index])};
		auto const leading_zero_end{static_cast<::std::size_t>(minimum_block) * 9u};
		auto const skipped_zero_end{materialized_fractional_position < leading_zero_end
			? materialized_fractional_position : leading_zero_end};
		auto const output_zero_end{skipped_zero_end < precision
			? skipped_zero_end : precision};
		iter = ::fast_io::details::fill_zeros_impl(iter, output_zero_end);
		::std::size_t output_position{skipped_zero_end};
		::std::uint_least32_t block_index{minimum_block};
		while (output_position != materialized_fractional_position)
		{
			::std::uint_least32_t block{};
			auto const power_index{static_cast<::std::uint_least32_t>(
				::fast_io::details::ryu::pow10_offset_2[index]) +
				block_index - minimum_block};
			if (power_index < static_cast<::std::uint_least32_t>(
					::fast_io::details::ryu::pow10_offset_2[index + 1u]))
			{
				block = ::fast_io::details::exact_precision_window_mul_shift_mod1e9(
					mantissa << 8u,
					::fast_io::details::ryu::pow10_split_2[power_index], shift);
			}
			auto const remaining{materialized_fractional_position - output_position};
			auto const take{remaining < 9u ? remaining : 9u};
			auto const output_take{output_position < precision
				? (take < precision - output_position ? take : precision - output_position)
				: 0u};
			if (output_take == 9u)
			{
				::fast_io::details::print_rsv_fp_digits_len<double>(iter, block, 9u);
			}
			else if (output_take)
			{
				char_type block_digits[9u];
				::fast_io::details::print_rsv_fp_digits_len<double>(block_digits, block, 9u);
				for (::std::size_t position{}; position != output_take; ++position)
				{
					iter[position] = block_digits[position];
				}
			}
			if (!exact && output_position <= precision && precision < output_position + take)
			{
				guard = ::fast_io::details::exact_precision_window_decimal_limb_digit(
					block, static_cast<unsigned>(precision - output_position));
			}
			iter += output_take;
			output_position += take;
			++block_index;
		}
	}
	if (exact)
	{
		iter = ::fast_io::details::fill_zeros_impl(
			iter, precision - terminating_fractional_position);
	}
	else
	{
		// After cancelling t=min(countr_zero(m),-e) powers of two,
		// m*2^e = (m/2^t)*5^(-e-t)*10^(-(-e-t)).  The numerator is not
		// divisible by two, so the digit at terminating_fractional_position
		// is nonzero.  Hence any guard before that position proves a nonzero
		// sticky suffix without scanning later blocks.
		auto const tail_nonzero{precision + 1u < terminating_fractional_position};
		auto const discarded_nonzero{guard != 0u || tail_nonzero};
		bool round_up{};
		if (discarded_nonzero)
		{
			auto const zero{char_literal_v<u8'0', char_type>};
			auto const rounded_down{static_cast<::std::uint_least64_t>(iter[-1] - zero)};
			round_up = ::fast_io::details::exact_precision_window_runtime_round_up(
				rounding, negative, guard, tail_nonzero, rounded_down);
		}
		if (round_up)
		{
			auto const zero{char_literal_v<u8'0', char_type>};
			auto const nine{char_literal_v<u8'9', char_type>};
			auto const point{char_literal_v<(comma ? u8',' : u8'.'), char_type>};
			auto position{iter};
			while (position != begin)
			{
				--position;
				if (*position == point)
				{
					continue;
				}
				if (*position != nine)
				{
					*position = static_cast<char_type>(*position + 1);
					round_up = false;
					break;
				}
				*position = zero;
			}
			if (round_up)
			{
				for (auto move{iter}; move != begin; --move)
				{
					*move = move[-1];
				}
				*begin = char_literal_v<u8'1', char_type>;
				++iter;
			}
		}
	}
	return iter;
}

template <typename flt, bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_rounding rounding, ::std::integral char_type>
inline constexpr char_type *exact_precision_window_try_print_negative_scientific(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::int_least32_t binary_exponent, ::std::int_least32_t real_exponent,
	::std::size_t significant, bool negative, bool corrected_exponent = false) noexcept
{
	if (0 <= binary_exponent || 0 <= real_exponent || !significant)
	{
		return nullptr;
	}
	auto const exponent_magnitude{static_cast<::std::uint_least32_t>(-binary_exponent)};
	auto const index{exponent_magnitude / 16u};
	if (index + 1u >= ::fast_io::details::ryu::table_size_2)
	{
		return nullptr;
	}
	auto const binary_factors{static_cast<::std::uint_least32_t>(::std::countr_zero(mantissa))};
	auto const terminating_fractional_position{static_cast<::std::size_t>(
		exponent_magnitude -
		(binary_factors < exponent_magnitude ? binary_factors : exponent_magnitude))};
	auto const first_significant_position{static_cast<::std::size_t>(-real_exponent - 1)};
	if (terminating_fractional_position <= first_significant_position)
	{
		return nullptr;
	}
	auto const available_significant{
		terminating_fractional_position - first_significant_position};
	auto const exact{available_significant <= significant};
	if (!exact && significant == (::std::numeric_limits<::std::size_t>::max)())
	{
		return nullptr;
	}
	auto const requested_end{first_significant_position + significant};
	if (requested_end < first_significant_position)
	{
		return nullptr;
	}
	auto const materialized_fractional_position{
		exact ? terminating_fractional_position : requested_end + 1u};
	auto const shift{static_cast<::std::uint_least32_t>(
		::fast_io::details::ryu::addtional_bits_2 +
		(exponent_magnitude - 16u * index) + 8u)};
	auto const minimum_block{
		static_cast<::std::uint_least32_t>(::fast_io::details::ryu::min_block_2[index])};
	auto const begin{iter};
	::std::size_t produced{};
	unsigned guard{};
	auto block_index{first_significant_position / 9u};
	for (auto output_position{block_index * 9u};
		output_position != materialized_fractional_position;
		output_position += 9u, ++block_index)
	{
		::std::uint_least32_t block{};
		if (minimum_block <= block_index)
		{
			auto const power_index{static_cast<::std::uint_least32_t>(
				::fast_io::details::ryu::pow10_offset_2[index]) +
				static_cast<::std::uint_least32_t>(block_index) - minimum_block};
			if (power_index < static_cast<::std::uint_least32_t>(
					::fast_io::details::ryu::pow10_offset_2[index + 1u]))
			{
				block = ::fast_io::details::exact_precision_window_mul_shift_mod1e9(
					mantissa << 8u,
					::fast_io::details::ryu::pow10_split_2[power_index], shift);
			}
		}
		auto const block_end{output_position + 9u};
		auto const copy_begin{first_significant_position < output_position
			? output_position : first_significant_position};
		auto const copy_limit{requested_end < block_end ? requested_end : block_end};
		if (copy_begin < copy_limit)
		{
			char_type block_digits[9u];
			::fast_io::details::print_rsv_fp_digits_len<double>(block_digits, block, 9u);
			auto offset{copy_begin - output_position};
			auto count{copy_limit - copy_begin};
			if constexpr (::fast_io::details::
				exact_precision_window_staged_negative_aligned_memcpy)
			{
				if (produced && offset == 0u && count == 9u &&
					!::std::is_constant_evaluated())
				{
					::fast_io::freestanding::my_memcpy(
						iter, block_digits, sizeof(block_digits));
					iter += 9u;
					produced += 9u;
					// A complete copy makes requested_end < block_end impossible, so
					// the common guard update below is empty on this edge.  Preserve the
					// loop's terminal test before advancing to the next decimal limb.
					if (materialized_fractional_position <= block_end)
					{
						break;
					}
					continue;
				}
			}
			if (!produced)
			{
				// A shortest carrier may round across a power-of-ten boundary.  In
				// that case its exponent is exactly one above the exact expansion.
				// Retry the direct stream at that exponent before emitting anything.
				if (block_digits[offset] == char_literal_v<u8'0', char_type>)
				{
					if (corrected_exponent)
					{
						return nullptr;
					}
					return ::fast_io::details::exact_precision_window_try_print_negative_scientific<
						flt, comma, uppercase_e, rounding>(iter, mantissa, binary_exponent,
							real_exponent - 1, significant, negative, true);
				}
				*iter++ = block_digits[offset++];
				++produced;
				--count;
				if (1u < significant)
				{
					*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
				}
			}
			for (::std::size_t position{}; position != count; ++position)
			{
				iter[position] = block_digits[offset + position];
			}
			iter += count;
			produced += count;
		}
		if (!exact && output_position <= requested_end && requested_end < block_end)
		{
			guard = ::fast_io::details::exact_precision_window_decimal_limb_digit(
				block, static_cast<unsigned>(requested_end - output_position));
		}
		if (materialized_fractional_position <= block_end)
		{
			break;
		}
	}
	if (!produced)
	{
		return nullptr;
	}
	if (produced < significant)
	{
		iter = ::fast_io::details::fill_zeros_impl(iter, significant - produced);
	}
	if (!exact)
	{
		auto const tail_nonzero{requested_end + 1u < terminating_fractional_position};
		auto const discarded_nonzero{guard != 0u || tail_nonzero};
		bool round_up{};
		if (discarded_nonzero)
		{
			if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
			{
				if (5u < guard || (guard == 5u && tail_nonzero))
				{
					round_up = true;
				}
				else if (guard == 5u)
				{
					auto const zero{char_literal_v<u8'0', char_type>};
					auto const rounded_down{static_cast<::std::uint_least64_t>(iter[-1] - zero)};
					round_up = ::fast_io::details::print_rsv_fp_decimal_tie_round_up<rounding>(
						negative, rounded_down);
				}
			}
			else
			{
				round_up = ::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
			}
		}
		if (round_up)
		{
			auto const zero{char_literal_v<u8'0', char_type>};
			auto const nine{char_literal_v<u8'9', char_type>};
			auto const point{char_literal_v<(comma ? u8',' : u8'.'), char_type>};
			auto position{iter};
			while (position != begin)
			{
				--position;
				if (*position == point)
				{
					continue;
				}
				if (*position != nine)
				{
					*position = static_cast<char_type>(*position + 1);
					round_up = false;
					break;
				}
				*position = zero;
			}
			if (round_up)
			{
				*begin = char_literal_v<u8'1', char_type>;
				++real_exponent;
			}
		}
	}
	return ::fast_io::details::print_rsv_fp_e_impl<flt, uppercase_e>(iter, real_exponent);
}

template <typename flt, bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_rounding rounding, ::std::integral char_type>
inline constexpr char_type *exact_precision_window_try_print_mixed_scientific(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::int_least32_t binary_exponent, ::std::size_t significant,
	bool negative) noexcept
{
	if (0 <= binary_exponent || binary_exponent < -52 || !significant)
	{
		return nullptr;
	}
	auto const exponent_magnitude{static_cast<::std::uint_least32_t>(-binary_exponent)};
	auto const integer{mantissa >> exponent_magnitude};
	if (!integer)
	{
		return nullptr;
	}
	auto const integer_digits{static_cast<::std::size_t>(
		::fast_io::details::chars_len<10u, true>(integer))};
	char_type integer_buffer[16u];
	::fast_io::details::print_rsv_fp_digits_len<double>(
		integer_buffer, integer, static_cast<::std::uint_least32_t>(integer_digits));
	auto const binary_factors{static_cast<::std::uint_least32_t>(::std::countr_zero(mantissa))};
	auto const terminating_fractional_position{static_cast<::std::size_t>(
		exponent_magnitude -
		(binary_factors < exponent_magnitude ? binary_factors : exponent_magnitude))};
	auto const total_digits{integer_digits + terminating_fractional_position};
	auto const materialized{significant < total_digits ? significant + 1u : total_digits};
	auto const begin{iter};
	::std::size_t produced{};
	unsigned guard{};
	bool have_guard{};
	auto emit_digit{[&](char_type digit) constexpr noexcept
	{
		*iter++ = digit;
		++produced;
		if (produced == 1u && 1u < significant)
		{
			*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
		}
	}};
	auto const integer_materialized{integer_digits < materialized ? integer_digits : materialized};
	for (::std::size_t position{}; position != integer_materialized; ++position)
	{
		if (position < significant)
		{
			emit_digit(integer_buffer[position]);
		}
		else
		{
			guard = static_cast<unsigned>(
				integer_buffer[position] - char_literal_v<u8'0', char_type>);
			have_guard = true;
		}
	}
	if (integer_digits < materialized)
	{
		auto const index{exponent_magnitude / 16u};
		if (index + 1u >= ::fast_io::details::ryu::table_size_2)
		{
			return nullptr;
		}
		auto const shift{static_cast<::std::uint_least32_t>(
			::fast_io::details::ryu::addtional_bits_2 +
			(exponent_magnitude - 16u * index) + 8u)};
		auto const minimum_block{
			static_cast<::std::uint_least32_t>(::fast_io::details::ryu::min_block_2[index])};
		auto const fractional_materialized{materialized - integer_digits};
		for (::std::size_t output_position{}, block_index{};
			output_position < fractional_materialized;
			output_position += 9u, ++block_index)
		{
			::std::uint_least32_t block{};
			if (minimum_block <= block_index)
			{
				auto const power_index{static_cast<::std::uint_least32_t>(
					::fast_io::details::ryu::pow10_offset_2[index]) +
					static_cast<::std::uint_least32_t>(block_index) - minimum_block};
				if (power_index < static_cast<::std::uint_least32_t>(
						::fast_io::details::ryu::pow10_offset_2[index + 1u]))
				{
					block = ::fast_io::details::exact_precision_window_mul_shift_mod1e9(
						mantissa << 8u,
						::fast_io::details::ryu::pow10_split_2[power_index], shift);
				}
			}
			auto const remaining{fractional_materialized - output_position};
			auto const take{remaining < 9u ? remaining : 9u};
			auto const global_position{integer_digits + output_position};
			auto const output_take{global_position < significant
				? (take < significant - global_position ? take : significant - global_position)
				: 0u};
			if (output_take == 9u)
			{
				::fast_io::details::print_rsv_fp_digits_len<double>(iter, block, 9u);
			}
			else if (output_take)
			{
				char_type block_digits[9u];
				::fast_io::details::print_rsv_fp_digits_len<double>(block_digits, block, 9u);
				for (::std::size_t position{}; position != output_take; ++position)
				{
					iter[position] = block_digits[position];
				}
			}
			iter += output_take;
			produced += output_take;
			if (global_position <= significant && significant < global_position + take)
			{
				guard = ::fast_io::details::exact_precision_window_decimal_limb_digit(
					block, static_cast<unsigned>(significant - global_position));
				have_guard = true;
			}
		}
	}
	if (produced < significant)
	{
		iter = ::fast_io::details::fill_zeros_impl(iter, significant - produced);
	}
	auto real_exponent{static_cast<::std::int_least32_t>(integer_digits - 1u)};
	if (have_guard)
	{
		bool tail_nonzero{};
		if (significant + 1u < integer_digits)
		{
			for (auto position{significant + 1u}; position != integer_digits; ++position)
			{
				tail_nonzero = tail_nonzero ||
					integer_buffer[position] != char_literal_v<u8'0', char_type>;
			}
			tail_nonzero = tail_nonzero || terminating_fractional_position != 0u;
		}
		else if (significant + 1u == integer_digits)
		{
			tail_nonzero = terminating_fractional_position != 0u;
		}
		else
		{
			auto const guard_fractional_position{significant - integer_digits};
			tail_nonzero = guard_fractional_position + 1u < terminating_fractional_position;
		}
		auto const discarded_nonzero{guard != 0u || tail_nonzero};
		bool round_up{};
		if (discarded_nonzero)
		{
			if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
			{
				if (5u < guard || (guard == 5u && tail_nonzero))
				{
					round_up = true;
				}
				else if (guard == 5u)
				{
					auto const zero{char_literal_v<u8'0', char_type>};
					auto const rounded_down{static_cast<::std::uint_least64_t>(iter[-1] - zero)};
					round_up = ::fast_io::details::print_rsv_fp_decimal_tie_round_up<rounding>(
						negative, rounded_down);
				}
			}
			else
			{
				round_up = ::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
			}
		}
		if (round_up)
		{
			auto const zero{char_literal_v<u8'0', char_type>};
			auto const nine{char_literal_v<u8'9', char_type>};
			auto const point{char_literal_v<(comma ? u8',' : u8'.'), char_type>};
			auto position{iter};
			while (position != begin)
			{
				--position;
				if (*position == point)
				{
					continue;
				}
				if (*position != nine)
				{
					*position = static_cast<char_type>(*position + 1);
					round_up = false;
					break;
				}
				*position = zero;
			}
			if (round_up)
			{
				*begin = char_literal_v<u8'1', char_type>;
				++real_exponent;
			}
		}
	}
	return ::fast_io::details::print_rsv_fp_e_impl<flt, uppercase_e>(iter, real_exponent);
}

// This positive-binary32 scientific writer contains a 39-character staging
// buffer and a carry scan.  Outlining prevents those live ranges from merging
// into the precision dispatcher on a rare positive-integer retry; the
// attribute does not alter guard, sticky or rounding decisions.
template <typename flt, bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_rounding rounding, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *exact_precision_window_try_print_positive_binary32_scientific(
	char_type *iter, ::std::uint_least32_t mantissa,
	::std::uint_least32_t binary_exponent, ::std::size_t significant,
	bool negative) noexcept
{
	if (104u < binary_exponent || !significant)
	{
		return nullptr;
	}
	auto const integer{static_cast<__uint128_t>(mantissa) << binary_exponent};
	auto const low_division{
		::fast_io::details::exact_precision_window_divide_128_by_decimal_limb_full(integer)};
	char_type digits[39u];
	::std::size_t total_digits{};
	if (!low_division.quotient)
	{
		total_digits = ::fast_io::details::exact_precision_window_decimal_limb_digits(
			low_division.remainder);
		::fast_io::details::jeaiii::jeaiii_main_len<false>(digits,
			low_division.remainder, static_cast<::std::uint_least32_t>(total_digits));
	}
	else
	{
		auto const high_division{
			::fast_io::details::exact_precision_window_divide_128_by_decimal_limb_full(
				low_division.quotient)};
		if (!high_division.quotient)
		{
			auto const high{static_cast<::std::uint_least64_t>(low_division.quotient)};
			total_digits =
				::fast_io::details::exact_precision_window_decimal_limb_digits(high);
			::fast_io::details::jeaiii::jeaiii_main_len<false>(digits, high,
				static_cast<::std::uint_least32_t>(total_digits));
		}
		else
		{
			auto const high{static_cast<::std::uint_least64_t>(high_division.quotient)};
			total_digits =
				::fast_io::details::exact_precision_window_decimal_limb_digits(high);
			::fast_io::details::jeaiii::jeaiii_main_len<false>(digits, high,
				static_cast<::std::uint_least32_t>(total_digits));
			::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
				digits + total_digits, high_division.remainder, 19u);
			total_digits += 19u;
		}
		::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
			digits + total_digits, low_division.remainder, 19u);
		total_digits += 19u;
	}
	auto const begin{iter};
	auto real_exponent{static_cast<::std::int_least32_t>(total_digits - 1u)};
	*iter++ = digits[0];
	if (1u < significant)
	{
		*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	}
	auto const copied{total_digits < significant ? total_digits : significant};
	for (::std::size_t position{1u}; position < copied; ++position)
	{
		*iter++ = digits[position];
	}
	if (copied < significant)
	{
		iter = ::fast_io::details::fill_zeros_impl(iter, significant - copied);
	}
	if (significant < total_digits)
	{
		auto const zero{char_literal_v<u8'0', char_type>};
		auto const guard{static_cast<unsigned>(digits[significant] - zero)};
		bool tail_nonzero{};
		for (auto position{significant + 1u}; position != total_digits; ++position)
		{
			tail_nonzero = tail_nonzero || digits[position] != zero;
		}
		auto const discarded_nonzero{guard != 0u || tail_nonzero};
		bool round_up{};
		if (discarded_nonzero)
		{
			if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
			{
				if (5u < guard || (guard == 5u && tail_nonzero))
				{
					round_up = true;
				}
				else if (guard == 5u)
				{
					auto const rounded_down{
						static_cast<::std::uint_least64_t>(iter[-1] - zero)};
					round_up = ::fast_io::details::print_rsv_fp_decimal_tie_round_up<rounding>(
						negative, rounded_down);
				}
			}
			else
			{
				round_up =
					::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
			}
		}
		if (round_up)
		{
			auto const nine{char_literal_v<u8'9', char_type>};
			auto const point{char_literal_v<(comma ? u8',' : u8'.'), char_type>};
			auto position{iter};
			while (position != begin)
			{
				--position;
				if (*position == point)
				{
					continue;
				}
				if (*position != nine)
				{
					*position = static_cast<char_type>(*position + 1);
					round_up = false;
					break;
				}
				*position = zero;
			}
			if (round_up)
			{
				*begin = char_literal_v<u8'1', char_type>;
				++real_exponent;
			}
		}
	}
	return ::fast_io::details::print_rsv_fp_e_impl<flt, uppercase_e>(iter, real_exponent);
}

template <typename flt, bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_rounding rounding, ::std::integral char_type>
inline constexpr char_type *exact_precision_window_try_print_positive_scientific(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t binary_exponent, ::std::size_t significant,
	bool negative) noexcept
{
	if (!significant)
	{
		return nullptr;
	}
	auto const index{static_cast<::std::uint_least32_t>((binary_exponent + 15u) / 16u)};
	if (::fast_io::details::ryu::table_size <= index)
	{
		return nullptr;
	}
	auto const power10_bits{16u * index + 120u};
	auto const length{static_cast<::std::size_t>(
		(::fast_io::details::exact_precision_window_log10_pow2(16u * index) + 25u) / 9u)};
	auto const begin{iter};
	bool found_nonzero{};
	bool have_guard{};
	::std::size_t total_digits{};
	::std::size_t consumed{};
	::std::size_t produced{};
	unsigned guard{};
	for (auto i{length}; i; --i)
	{
		auto const block_index{i - 1u};
		auto const digits{::fast_io::details::exact_precision_window_mul_shift_mod1e9(
			mantissa << 8u,
			::fast_io::details::ryu::pow10_split[
				::fast_io::details::ryu::power_offset[index] + block_index],
			power10_bits - binary_exponent + 8u)};
		::std::size_t width{9u};
		if (!found_nonzero)
		{
			if (!digits)
			{
				continue;
			}
			found_nonzero = true;
			width = ::fast_io::details::exact_precision_window_decimal_limb_digits(digits);
			total_digits = width + block_index * 9u;
		}
		char_type block_digits[9u];
		::fast_io::details::print_rsv_fp_digits_len<double>(
			block_digits, digits, static_cast<::std::uint_least32_t>(width));
		if (consumed < significant)
		{
			auto const available{significant - consumed};
			auto count{width < available ? width : available};
			::std::size_t offset{};
			if (!produced)
			{
				*iter++ = block_digits[offset++];
				++produced;
				--count;
				if (1u < significant)
				{
					*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
				}
			}
			for (::std::size_t position{}; position != count; ++position)
			{
				iter[position] = block_digits[offset + position];
			}
			iter += count;
			produced += count;
		}
		if (consumed <= significant && significant < consumed + width)
		{
			guard = static_cast<unsigned>(
				block_digits[significant - consumed] - char_literal_v<u8'0', char_type>);
			have_guard = true;
			break;
		}
		consumed += width;
	}
	if (!found_nonzero || !produced)
	{
		return nullptr;
	}
	if (produced < significant)
	{
		iter = ::fast_io::details::fill_zeros_impl(iter, significant - produced);
	}
	auto real_exponent{static_cast<::std::int_least32_t>(total_digits - 1u)};
	if (have_guard)
	{
		auto const discarded_after_guard{total_digits - significant - 1u};
		auto const tail_nonzero{::fast_io::details::
			exact_precision_window_positive_binary_tail_nonzero(
				mantissa, binary_exponent, discarded_after_guard)};
		auto const discarded_nonzero{guard != 0u || tail_nonzero};
		bool round_up{};
		if (discarded_nonzero)
		{
			if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
			{
				if (5u < guard || (guard == 5u && tail_nonzero))
				{
					round_up = true;
				}
				else if (guard == 5u)
				{
					auto const zero{char_literal_v<u8'0', char_type>};
					auto const rounded_down{static_cast<::std::uint_least64_t>(iter[-1] - zero)};
					round_up = ::fast_io::details::print_rsv_fp_decimal_tie_round_up<rounding>(
						negative, rounded_down);
				}
			}
			else
			{
				round_up = ::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
			}
		}
		if (round_up)
		{
			auto const zero{char_literal_v<u8'0', char_type>};
			auto const nine{char_literal_v<u8'9', char_type>};
			auto const point{char_literal_v<(comma ? u8',' : u8'.'), char_type>};
			auto position{iter};
			while (position != begin)
			{
				--position;
				if (*position == point)
				{
					continue;
				}
				if (*position != nine)
				{
					*position = static_cast<char_type>(*position + 1);
					round_up = false;
					break;
				}
				*position = zero;
			}
			if (round_up)
			{
				*begin = char_literal_v<u8'1', char_type>;
				++real_exponent;
			}
		}
	}
	return ::fast_io::details::print_rsv_fp_e_impl<flt, uppercase_e>(iter, real_exponent);
}

// Preserve the negative block-stream materializer as a separate live-range
// boundary.  Its table cursor and sticky state need not enter callers that use
// the positive or generic paths.  The wrapper delegates field-for-field when a
// compiler cannot express noinline.
template <bool da_nearest_eligible = false, typename window_result_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr bool exact_precision_window_materialize_negative_binary64(
	window_result_type &result, ::std::uint_least64_t mantissa,
	::std::int_least32_t binary_exponent, ::std::size_t requested_digits,
	::std::int_least32_t real_exponent) noexcept
{
	if constexpr (da_nearest_eligible)
	{
		if (requested_digits - 19u < 2u &&
			::fast_io::details::exact_precision_window_try_materialize_binary64_p18_p19(
				result, mantissa, binary_exponent, requested_digits - 1u))
		{
			return true;
		}
	}
	return ::fast_io::details::exact_precision_window_materialize_negative_binary_core(
		result, mantissa, binary_exponent, requested_digits, real_exponent);
}

// Prepare a presentation-independent exact prefix for length evaluation by
// composing the same proved block materializers used by precision emission.
// The result stores only requested decimal digits, one guard digit at the
// caller-selected width, and a sticky bit; this wrapper neither writes an
// output character nor chooses punctuation.  The three arithmetic domains are
// exhaustive for M*2^e: negative values below one, mixed values with an integer
// and fractional part, and nonnegative binary exponents.  Each domain delegates
// directly to emission's existing numeric-prefix primitive.
// A false `success` is an ambiguity/capacity rejection and requires the caller
// to retain its complete exact fallback.
template <typename flt, bool da_nearest_eligible = false>
[[nodiscard]] inline constexpr exact_precision_compact_window_result
exact_precision_compact_window_from_binary(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t requested_digits,
	::std::int_least32_t real_exponent) noexcept
{
	static_assert(::std::same_as<flt, float> || ::std::same_as<flt, double>);
	using trait = ::fast_io::details::iec559_traits<flt>;
	auto binary_mantissa{static_cast<::std::uint_least64_t>(mantissa)};
	::std::int_least32_t binary_exponent{};
	if (exponent)
	{
		binary_mantissa |= static_cast<::std::uint_least64_t>(1ULL) << trait::mbits;
		constexpr ::std::int_least32_t bias{
			(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
		binary_exponent = static_cast<::std::int_least32_t>(exponent) - bias -
			static_cast<::std::int_least32_t>(trait::mbits);
	}
	else
	{
		binary_exponent = 1 -
			((static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1) -
			static_cast<::std::int_least32_t>(trait::mbits);
	}

	exact_precision_compact_window_result result{};
	if (binary_exponent < 0 && real_exponent < 0)
	{
		if constexpr (::std::same_as<flt, float>)
		{
			::fast_io::details::exact_precision_window_materialize_negative_binary_core(
				result, binary_mantissa, binary_exponent, requested_digits,
				real_exponent);
		}
		else
		{
			::fast_io::details::exact_precision_window_materialize_negative_binary64<
				da_nearest_eligible>(result, binary_mantissa, binary_exponent,
					requested_digits, real_exponent);
		}
	}
	else if (binary_exponent < 0)
	{
		::fast_io::details::exact_precision_window_materialize_mixed_binary<
			da_nearest_eligible && ::std::same_as<flt, double>>(
				result, binary_mantissa, binary_exponent, requested_digits);
	}
	else
	{
		::fast_io::details::exact_precision_window_materialize_positive_binary64<
			da_nearest_eligible && ::std::same_as<flt, double>>(
				result, binary_mantissa,
				static_cast<::std::uint_least32_t>(binary_exponent), requested_digits);
	}
	return result;
}

template <::std::integral char_type>
inline constexpr char_type *exact_precision_window_print_positive_binary64_integer(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t binary_exponent) noexcept
{
	auto const index{static_cast<::std::uint_least32_t>((binary_exponent + 15u) / 16u)};
	if (::fast_io::details::ryu::table_size <= index)
	{
		return nullptr;
	}
	auto const power10_bits{16u * index + 120u};
	auto const length{static_cast<::std::size_t>(
		(::fast_io::details::exact_precision_window_log10_pow2(16u * index) + 25u) / 9u)};
	bool found_nonzero{};
	for (auto i{length}; i; --i)
	{
		auto const block_index{i - 1u};
		auto const digits{::fast_io::details::exact_precision_window_mul_shift_mod1e9(
			mantissa << 8u,
			::fast_io::details::ryu::pow10_split[::fast_io::details::ryu::power_offset[index] + block_index],
			power10_bits - binary_exponent + 8u)};
		::std::uint_least32_t width{9u};
		if (!found_nonzero)
		{
			if (!digits)
			{
				continue;
			}
			found_nonzero = true;
			width = static_cast<::std::uint_least32_t>(
				::fast_io::details::exact_precision_window_decimal_limb_digits(digits));
		}
		::fast_io::details::print_rsv_fp_digits_len<double>(iter, digits, width);
		iter += width;
	}
	return found_nonzero ? iter : nullptr;
}

// Let x be a positive finite binary64 value and P the requested significant
// width.  Fixed notation retains the quantum 10^(floor(log10(x))+1-P).  If
// x >= 1 and P covers the integer field, that quantum is exactly
// 10^(-(P-integer_digits)); if x < 1 it is exactly
// 10^(-(P+leading_fractional_zeroes)).  Therefore the significant request can
// be delegated to the exact fractional-grid writer without changing the guard,
// sticky bit, tie parity, directed-rounding sign, carry propagation or preserved
// zero count.  The caller admits decimal format only when the existing length
// rule selects fixed notation; removing format from this writer's template
// identity lets fixed and decimal instantiations share one emitted function.
//
// Write e=floor(log10(x)) and F=P-e-1.  The fractional writer rounds on the
// exact significant quantum q=10^-F=10^(e+1-P).  If its carry changes
// 9...*10^e to 1...*10^(e+1), the already rounded numeric value is unchanged by
// deleting a terminal zero, while a P-digit spelling at the new exponent must
// expose only F-1 fractional places and therefore the display quantum
// 10^(-(F-1))=10^(e+2-P).  Thus the deletion changes neither the rounding
// decision nor the value; it solely restores the requested P coefficient
// digits.  When F=0 there is no fractional zero to delete and the exact integer
// spelling is necessarily retained.
//
// The shortest real exponent can be one decade high only at a rounded power-of-
// ten carrier.  Mixed values verify their integer length, and values below one
// verify the purported first significant digit, before touching the output.
// The positive-integer branch verifies the emitted exact length before adding
// preserved zeroes; its fallback, if ever needed, overwrites that provisional
// integer from the same destination.  Consequently every successful return has
// the same field and no rejected probe can expose provisional characters.  Keep
// the block cursor and carry scan outlined so a failed runtime probe does not
// extend the caller's live ranges or duplicate the loop body.
template <bool comma, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *exact_precision_window_try_print_significant_fixed(
	char_type *iter, ::std::uint_least64_t raw_mantissa,
	::std::uint_least32_t raw_exponent, ::std::int_least32_t real_exponent,
	::std::size_t significant, bool negative, bool json_float, bool decimal_format,
	::fast_io::manipulators::floating_rounding rounding) noexcept
{
	// The sole caller proves significant >= 64.  Keep notation and magnitude
	// admission here so fixed and decimal call sites, and every rounding policy,
	// share both the test and the binary-carrier reconstruction.
	auto const covers_integer_field{real_exponent < 0 ||
		static_cast<::std::size_t>(real_exponent) < significant};
	if (!covers_integer_field || (decimal_format && real_exponent < -4))
	{
		return nullptr;
	}
	using trait = ::fast_io::details::iec559_traits<double>;
	auto mantissa{raw_mantissa};
	::std::int_least32_t binary_exponent{};
	constexpr ::std::int_least32_t bias{
		(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
	if (raw_exponent)
	{
		mantissa |= static_cast<::std::uint_least64_t>(1ULL) << trait::mbits;
		binary_exponent = static_cast<::std::int_least32_t>(raw_exponent) - bias -
			static_cast<::std::int_least32_t>(trait::mbits);
	}
	else
	{
		binary_exponent = 1 - bias - static_cast<::std::int_least32_t>(trait::mbits);
	}
	if (0 <= binary_exponent)
	{
		if (real_exponent < 0)
		{
			return nullptr;
		}
		auto const predicted_integer_digits{
			static_cast<::std::size_t>(real_exponent) + 1u};
		if (significant < predicted_integer_digits)
		{
			return nullptr;
		}
		auto integer_end{
			::fast_io::details::exact_precision_window_print_positive_binary64_integer(
				iter, mantissa, static_cast<::std::uint_least32_t>(binary_exponent))};
		if (!integer_end)
		{
			return nullptr;
		}
		auto const integer_digits{static_cast<::std::size_t>(integer_end - iter)};
		if (significant < integer_digits)
		{
			return nullptr;
		}
		auto const zeroes{significant - integer_digits};
		if (zeroes)
		{
			*integer_end++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
			return ::fast_io::details::fill_zeros_impl(integer_end, zeroes);
		}
		if (json_float)
		{
			return ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(
				integer_end);
		}
		return integer_end;
	}

	auto const exponent_magnitude{static_cast<::std::uint_least32_t>(-binary_exponent)};
	if (0 <= real_exponent)
	{
		auto const integer{exponent_magnitude < 64u
			? mantissa >> exponent_magnitude : 0u};
		if (!integer)
		{
			return nullptr;
		}
		auto const integer_digits{static_cast<::std::size_t>(
			::fast_io::details::chars_len<10u, true>(integer))};
		if (integer_digits != static_cast<::std::size_t>(real_exponent) + 1u ||
			significant < integer_digits)
		{
			return nullptr;
		}
		auto const fractional_precision{significant - integer_digits};
		auto const result{::fast_io::details::
			exact_precision_window_try_print_exact_mixed_fixed_runtime<
				comma>(iter, mantissa, binary_exponent,
					fractional_precision, negative, rounding)};
		if (!result || !fractional_precision)
		{
			return result;
		}
		// Rounding 9... to 10... moves the leading digit one place to the
		// left.  The fractional-grid writer preserves the old quantum and hence
		// emits one additional terminal zero; significant-preserve instead keeps
		// P coefficient digits after the decade carry.  At the old integer-field
		// boundary the decimal point has become the new least integer digit, which
		// proves both the carry and the removable final zero.
		constexpr auto point{char_literal_v<(comma ? u8',' : u8'.'), char_type>};
		if (iter[integer_digits] == point)
		{
			return result;
		}
		if (fractional_precision != 1u)
		{
			return result - 1u;
		}
		// With F=1 the redundant zero is the entire fractional field.  Removing
		// only that digit would expose a dangling radix point.  Ordinary fixed
		// output removes both characters; JSON deliberately retains the existing
		// point and zero because its integer-valued floating representation still
		// requires a fractional marker.  No character is synthesized here, so the
		// comma and every character type keep the exact literal chosen above.
		if (json_float)
		{
			return result;
		}
		return result - 2u;
	}
	else
	{
		auto const first_significant_position{
			static_cast<::std::size_t>(-real_exponent - 1)};
		constexpr auto size_max{(::std::numeric_limits<::std::size_t>::max)()};
		if (size_max - significant < first_significant_position)
		{
			return nullptr;
		}
		auto const index{exponent_magnitude / 16u};
		if (index + 1u >= ::fast_io::details::ryu::table_size_2)
		{
			return nullptr;
		}
		auto const block_index{
			static_cast<::std::uint_least32_t>(first_significant_position / 9u)};
		auto const minimum_block{static_cast<::std::uint_least32_t>(
			::fast_io::details::ryu::min_block_2[index])};
		if (block_index < minimum_block)
		{
			return nullptr;
		}
		auto const power_index{static_cast<::std::uint_least32_t>(
			::fast_io::details::ryu::pow10_offset_2[index]) +
			block_index - minimum_block};
		if (static_cast<::std::uint_least32_t>(
				::fast_io::details::ryu::pow10_offset_2[index + 1u]) <= power_index)
		{
			return nullptr;
		}
		auto const shift{static_cast<::std::uint_least32_t>(
			::fast_io::details::ryu::addtional_bits_2 +
			(exponent_magnitude - 16u * index) + 8u)};
		auto const block{::fast_io::details::exact_precision_window_mul_shift_mod1e9(
			mantissa << 8u, ::fast_io::details::ryu::pow10_split_2[power_index], shift)};
		auto const first_digit{::fast_io::details::exact_precision_window_decimal_limb_digit(
			block, static_cast<unsigned>(first_significant_position % 9u))};
		if (!first_digit)
		{
			return nullptr;
		}
		auto const fractional_precision{significant + first_significant_position};
		auto const result{::fast_io::details::
			exact_precision_window_try_print_exact_mixed_fixed_runtime<
				comma>(iter, mantissa, binary_exponent,
					fractional_precision, negative, rounding)};
		if (!result)
		{
			return nullptr;
		}
		// The same decade carry is visible one place before the input's first
		// significant digit.  For e=-1 that place is the integer digit; for
		// smaller e it is the preceding fractional zero.  The direct writer's
		// preserved field guarantees that its last character is the redundant
		// zero corresponding to the old quantum.
		auto const zero{char_literal_v<u8'0', char_type>};
		auto const carried{first_significant_position
			? iter[first_significant_position + 1u] != zero
			: iter[0] != zero};
		if (!carried)
		{
			return result;
		}
		// The sole caller has P>=64, so F=P+(-e-1)>=64 below one.  Only the
		// terminal zero belonging to the pre-carry quantum is redundant here;
		// the F=1 point-only case is outside this helper's admitted domain.
		return result - 1u;
	}
}

template <typename flt, ::std::integral char_type>
inline constexpr char_type *exact_precision_window_try_print_positive_integer(
	char_type *iter, typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	if constexpr (trait::mbits != 52u || trait::ebits != 11u)
	{
		return nullptr;
	}
	else
	{
		constexpr ::std::int_least32_t bias{
			(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
		if (!exponent)
		{
			return nullptr;
		}
		mantissa |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << trait::mbits);
		auto const binary_exponent{static_cast<::std::int_least32_t>(exponent) - bias -
								   static_cast<::std::int_least32_t>(trait::mbits)};
		if (binary_exponent < 0)
		{
			return nullptr;
		}
		return ::fast_io::details::exact_precision_window_print_positive_binary64_integer(
			iter, static_cast<::std::uint_least64_t>(mantissa),
			static_cast<::std::uint_least32_t>(binary_exponent));
	}
}

// The generic fallback owns the maximum limb array and repeated binary shifts.
// Keeping it outlined prevents every direct/compact failure site from acquiring
// that stack frame or a cloned copy.  This is a code-size/frame boundary only;
// its exact arithmetic is unchanged when the attribute is unavailable.
template <typename flt>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr exact_precision_window_result exact_precision_window_from_binary(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t requested_digits,
	::std::int_least32_t real_exponent) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	static_assert(sizeof(mantissa_type) <= sizeof(::std::uint_least64_t));
	exact_precision_window_result failure{};
	constexpr ::std::int_least32_t bias{
		(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
	::std::int_least32_t binary_exponent{};
	if (exponent)
	{
		mantissa |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << trait::mbits);
		binary_exponent = static_cast<::std::int_least32_t>(exponent) - bias -
						  static_cast<::std::int_least32_t>(trait::mbits);
	}
	else
	{
		binary_exponent = 1 - bias - static_cast<::std::int_least32_t>(trait::mbits);
	}
	::std::uint_least64_t limbs[exact_precision_window_binary_limb_capacity]{};
	limbs[0] = static_cast<::std::uint_least64_t>(mantissa);
	::std::size_t size{1u};
	if constexpr (trait::mbits == 52u && trait::ebits == 11u)
	{
		if (binary_exponent < 0 && real_exponent < 0)
		{
			if (::fast_io::details::exact_precision_window_materialize_negative_binary64(
					failure, static_cast<::std::uint_least64_t>(mantissa), binary_exponent,
					requested_digits, real_exponent))
			{
				return failure;
			}
		}
	}
	if (0 <= binary_exponent)
	{
		if constexpr (trait::mbits == 52u && trait::ebits == 11u)
		{
			::fast_io::details::exact_precision_window_materialize_positive_binary64(
				failure, static_cast<::std::uint_least64_t>(mantissa),
				static_cast<::std::uint_least32_t>(binary_exponent), requested_digits);
			return failure;
		}
		if (!::fast_io::details::exact_precision_window_shift_left(
				limbs, size, static_cast<unsigned>(binary_exponent)))
		{
			return failure;
		}
		return ::fast_io::details::exact_precision_window_materialize(
			limbs, size, requested_digits, real_exponent, false, true);
	}
	auto const decimal_shift{static_cast<::std::int_least64_t>(requested_digits) - 1 -
							 static_cast<::std::int_least64_t>(real_exponent)};
	if (decimal_shift < 0 || 600 < decimal_shift)
	{
		return failure;
	}
	auto count{static_cast<unsigned>(decimal_shift)};
	for (; exact_precision_window_pow5_chunk <= count; count -= exact_precision_window_pow5_chunk)
	{
		if (!::fast_io::details::exact_precision_window_multiply_small(
				limbs, size, exact_precision_window_pow5_multiplier))
		{
			return failure;
		}
	}
	if (count)
	{
		if (!::fast_io::details::exact_precision_window_multiply_small(
				limbs, size, ::fast_io::details::exact_precision_small_power(5u, count)))
		{
			return failure;
		}
	}
	auto const scaled_binary_exponent{static_cast<::std::int_least64_t>(binary_exponent) + decimal_shift};
	bool tail_nonzero{};
	if (scaled_binary_exponent < 0)
	{
		tail_nonzero = ::fast_io::details::exact_precision_window_shift_right(
			limbs, size, static_cast<unsigned>(-scaled_binary_exponent));
	}
	else if (!::fast_io::details::exact_precision_window_shift_left(
				 limbs, size, static_cast<unsigned>(scaled_binary_exponent)))
	{
		return failure;
	}
	return ::fast_io::details::exact_precision_window_materialize(
		limbs, size, requested_digits, real_exponent, tail_nonzero, false);
}
#endif

template <::std::integral char_type, typename decimal_type>
inline constexpr char_type *exact_precision_copy(
	char_type *iter, decimal_type const &decimal,
	::std::size_t first, ::std::size_t last) noexcept
{
	if constexpr (sizeof(decimal.digits) == sizeof(decimal.digits[0]))
	{
		/*
		The compact directed-rounding carrier has capacity and size one.  Every
		caller supplies a slice satisfying 0 <= first <= last <= decimal.size;
		therefore its only nonempty slice is [0, 1).  Spelling that invariant here
		keeps the general block-copy loop out of this instantiation and prevents an
		optimizer diagnostic from speculating about digits[1] or digits[2].
		*/
		if (first != last)
		{
			*iter = ::fast_io::char_literal_add<char_type>(decimal.digits[0]);
			++iter;
		}
		return iter;
	}
	if constexpr (sizeof(char_type) == 1u && ::fast_io::details::is_ascii<char_type>)
	{
		// Copy each complete eight-digit numeric block through an unaligned u64,
		// then add the ASCII zero bias lane-wise.  The builtin spelling carries the
		// same byte-copy semantics without violating alignment or aliasing and lets a
		// freestanding compiler inline the fixed width.  Constant evaluation and
		// compilers without the builtin continue through the per-digit loop below.
#if FAST_IO_HAS_BUILTIN(__builtin_memcpy)
		if (!::std::is_constant_evaluated())
		{
			for (; 8u <= last - first; first += 8u, iter += 8u)
			{
				::std::uint_least64_t packed;
				__builtin_memcpy(__builtin_addressof(packed), decimal.digits + first,
					sizeof(packed));
				packed += static_cast<::std::uint_least64_t>(0x3030303030303030ULL);
				__builtin_memcpy(iter, __builtin_addressof(packed), sizeof(packed));
			}
		}
#endif
	}
	for (; first != last; ++first)
	{
		*iter = ::fast_io::char_literal_add<char_type>(decimal.digits[first]);
		++iter;
	}
	return iter;
}

template <typename decimal_type>
[[nodiscard]] inline constexpr ::std::size_t
exact_precision_fractional_general_rounded_virtual_size(
	decimal_type const &decimal, ::std::size_t precision) noexcept
{
	// Rounding at the 10^-P grid normally leaves the last stored coefficient
	// digit at exponent -P.  A carry through all retained nines canonicalizes
	// the coefficient to one digit and raises decimal.exponent; the omitted
	// suffix zeroes remain part of the requested quantum and must participate in
	// general's fixed/scientific decision.  Exact values that terminated before
	// P never call this helper, so their intrinsic integer suffix zeroes are not
	// mistaken for preserved fractional zeroes.
	//
	// This relation uses only the decimal exponent and saturating size
	// arithmetic.  It deliberately remains outside the native-u128 gate below:
	// the generic exact formatter and precise-size model require the same virtual
	// coefficient on targets such as MSVC that use the limb-based fallback.
	::std::size_t padding{};
	if (0 <= decimal.exponent)
	{
		padding = ::fast_io::details::exact_precision_saturating_add(
			precision, static_cast<::std::size_t>(decimal.exponent));
	}
	else
	{
		auto const magnitude{static_cast<::std::size_t>(
			-static_cast<::std::int_least64_t>(decimal.exponent))};
		if (magnitude < precision)
		{
			padding = precision - magnitude;
		}
	}
	return ::fast_io::details::exact_precision_saturating_add(
		decimal.size, padding);
}

// This table is generated by the native-u128 exact prefix materializer and is
// consumed only by the u128-gated binary32 power-of-ten writer below.  Keep the
// data and its construction in the same representation domain: otherwise a
// target without native u128 sees declarations whose defining arithmetic has
// already been removed, even though its portable exact formatter never reads
// the table.  The gate changes availability of this optimization only; the
// generic exact path remains the semantic fallback.
#if defined(__SIZEOF_INT128__)
inline constexpr ::std::size_t exact_precision_binary32_power10_digit_capacity{80u};

struct exact_precision_binary32_power10_decimal_entry
{
	unsigned char digits[exact_precision_binary32_power10_digit_capacity]{};
	::std::size_t size{};
	::std::int_least32_t real_exponent{};
};

struct exact_precision_binary32_power10_decimal_table_type
{
	exact_precision_binary32_power10_decimal_entry values[18u]{};

	inline constexpr exact_precision_binary32_power10_decimal_table_type() noexcept
	{
		using trait = ::fast_io::details::iec559_traits<float>;
		constexpr auto mantissa_mask{
			(static_cast<::std::uint_least32_t>(1u) << trait::mbits) - 1u};
		constexpr ::std::int_least32_t bias{
			(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
		for (::std::size_t index{1u}; index != 18u; ++index)
		{
			auto const raw{exact_precision_binary32_negative_power10_table[index]};
			auto const exponent{raw >> trait::mbits};
			auto binary_mantissa{raw & mantissa_mask};
			binary_mantissa |= static_cast<::std::uint_least32_t>(1u) << trait::mbits;
			auto const binary_exponent{static_cast<::std::int_least32_t>(exponent) -
				bias - static_cast<::std::int_least32_t>(trait::mbits)};
			// The hidden bit above proves a nonzero mantissa. Perform the zero-based bit index in the signed domain used
			// by the binary exponent rather than mixing `bit_width`'s signed result with an unsigned literal.
			auto const binary_floor_exponent{binary_exponent +
				static_cast<::std::int_least32_t>(::std::bit_width(binary_mantissa) - 1)};
			auto real_exponent{::fast_io::details::mul_ln2_div_ln10_floor(
				binary_floor_exponent) + 1};
			real_exponent =
				::fast_io::details::exact_precision_correct_binary32_negative_real_exponent(
					binary_mantissa, binary_exponent, real_exponent);
			auto const exponent_magnitude{static_cast<::std::uint_least32_t>(-binary_exponent)};
			auto const binary_factors{
				static_cast<::std::uint_least32_t>(::std::countr_zero(binary_mantissa))};
			auto const terminating_decimal_position{static_cast<::std::size_t>(
				exponent_magnitude - (binary_factors < exponent_magnitude
					? binary_factors : exponent_magnitude))};
			auto const first_significant_position{static_cast<::std::size_t>(-real_exponent - 1)};
			auto const size{terminating_decimal_position - first_significant_position};
			exact_precision_compact_window_result result{};
			if (size <= exact_precision_binary32_power10_digit_capacity &&
				::fast_io::details::exact_precision_window_materialize_negative_binary_core(
					result, binary_mantissa, binary_exponent, size, real_exponent))
			{
				for (::std::size_t position{}; position != size; ++position)
				{
					values[index].digits[position] = result.decimal.digits[position];
				}
				values[index].size = size;
				values[index].real_exponent = real_exponent;
			}
		}
	}

	[[nodiscard]] inline constexpr exact_precision_binary32_power10_decimal_entry const &
	operator[](::std::size_t index) const noexcept
	{
		return values[index];
	}
};

// These are the exact finite decimal expansions of the binary32 values nearest
// 10^-1 through 10^-17.  Generate them from the same integer block proof used
// by the general window instead of maintaining a handwritten digit table.
inline constexpr exact_precision_binary32_power10_decimal_table_type
	exact_precision_binary32_power10_decimal_table{};
#endif

template <bool comma, bool json_float, ::std::integral char_type, typename decimal_type>
inline constexpr char_type *exact_precision_fixed(
	char_type *iter, decimal_type const &decimal, ::std::size_t virtual_size,
	bool force_fractional, ::std::size_t fractional_precision) noexcept
{
	auto const real_exponent{decimal.exponent + static_cast<::std::int_least32_t>(decimal.size) - 1};
	auto const point{real_exponent + 1};
	if (force_fractional && fractional_precision && 0 < point)
	{
		auto const integer_digits{static_cast<::std::size_t>(point)};
		if (integer_digits <= decimal.size && virtual_size == decimal.size)
		{
			iter = ::fast_io::details::exact_precision_copy(
				iter, decimal, 0u, integer_digits);
			*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
			iter = ::fast_io::details::exact_precision_copy(
				iter, decimal, integer_digits, decimal.size);
			auto const present{decimal.size - integer_digits};
			if (present < fractional_precision)
			{
				iter = ::fast_io::details::fill_zeros_impl(
					iter, fractional_precision - present);
			}
			return iter;
		}
	}
	bool wrote_point{};
	if (point <= 0)
	{
		*iter++ = char_literal_v<u8'0', char_type>;
		if (decimal.digits[0] != 0u || force_fractional)
		{
			*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
			wrote_point = true;
			iter = ::fast_io::details::fill_zeros_impl(iter, static_cast<::std::size_t>(-point));
			iter = ::fast_io::details::exact_precision_copy(iter, decimal, 0u, decimal.size);
			iter = ::fast_io::details::fill_zeros_impl(iter, virtual_size - decimal.size);
		}
	}
	else
	{
		auto const integer_digits{static_cast<::std::size_t>(point)};
		if (integer_digits < virtual_size)
		{
			auto const from_decimal{integer_digits < decimal.size ? integer_digits : decimal.size};
			iter = ::fast_io::details::exact_precision_copy(iter, decimal, 0u, from_decimal);
			if (from_decimal < integer_digits)
			{
				iter = ::fast_io::details::fill_zeros_impl(iter, integer_digits - from_decimal);
			}
			*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
			wrote_point = true;
			if (integer_digits < decimal.size)
			{
				iter = ::fast_io::details::exact_precision_copy(iter, decimal, integer_digits, decimal.size);
			}
			if (decimal.size < virtual_size)
			{
				iter = ::fast_io::details::fill_zeros_impl(iter, virtual_size -
																	 (decimal.size < integer_digits ? integer_digits : decimal.size));
			}
		}
		else
		{
			iter = ::fast_io::details::exact_precision_copy(iter, decimal, 0u, decimal.size);
			iter = ::fast_io::details::fill_zeros_impl(iter, integer_digits - decimal.size);
		}
	}
	if (force_fractional)
	{
		auto const present{point <= 0 ? static_cast<::std::size_t>(-point) + virtual_size : (point < static_cast<::std::int_least32_t>(virtual_size) ? virtual_size - static_cast<::std::size_t>(point) : 0u)};
		if (!wrote_point && fractional_precision)
		{
			*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
			wrote_point = true;
		}
		if (present < fractional_precision)
		{
			iter = ::fast_io::details::fill_zeros_impl(iter, fractional_precision - present);
		}
	}
	if constexpr (json_float)
	{
		if (!wrote_point)
		{
			iter = ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(iter);
		}
	}
	return iter;
}

template <typename flt, bool comma, bool uppercase_e, ::std::integral char_type,
		  typename decimal_type>
inline constexpr char_type *exact_precision_scientific(
	char_type *iter, decimal_type const &decimal,
	::std::size_t fractional_precision, bool preserve) noexcept
{
	auto const real_exponent{decimal.exponent + static_cast<::std::int_least32_t>(decimal.size) - 1};
	*iter++ = ::fast_io::char_literal_add<char_type>(decimal.digits[0]);
	auto const available{decimal.size - 1u};
	auto const used{available < fractional_precision ? available : fractional_precision};
	if (used || (preserve && fractional_precision))
	{
		*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
		iter = ::fast_io::details::exact_precision_copy(iter, decimal, 1u, used + 1u);
		if (preserve && used < fractional_precision)
		{
			iter = ::fast_io::details::fill_zeros_impl(iter, fractional_precision - used);
		}
	}
	return ::fast_io::details::print_rsv_fp_e_impl<flt, uppercase_e>(iter, real_exponent);
}

// Keep the ordinary scientific renderer as the shared leaf for every existing
// caller.  This forwarding boundary is instantiated only by an explicitly
// measured placement policy; flatten plus always-inline makes that caller reuse
// the exact same encoding-independent body without maintaining a second copy
// of its digit, punctuation or exponent rules.
template <typename flt, bool comma, bool uppercase_e, ::std::integral char_type,
		  typename decimal_type>
#if __has_cpp_attribute(__gnu__::__flatten__)
[[__gnu__::__flatten__]]
#endif
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
exact_precision_scientific_direct(
	char_type *iter, decimal_type const &decimal,
	::std::size_t fractional_precision, bool preserve) noexcept
{
	return ::fast_io::details::exact_precision_scientific<
		flt, comma, uppercase_e>(iter, decimal, fractional_precision, preserve);
}

// GCC 15 on Linux System V x86-64 LP64 stopped inlining the final scientific
// renderer into the binary64 P34 decimal dispatcher after the P35-P38
// extension changed its instantiation graph.  Paired forward/reverse data from
// the single-destination benchmark DSO recovered about 5.0% for significant
// and 3.2% for preserving char output at a bounded 1,152-byte linked-text cost.
// Unconditional inlining was rejected after growing that DSO by 52,096 bytes.
// A five-destination audit also rejected widening this policy: enabling char,
// wchar_t, char8_t, char16_t and char32_t together cost 12,288 bytes, while no
// non-char P34 pair cleared a two percent win.  The char-only policy reduced
// that complete DSO delta to 5,120 bytes and held every non-char P34/P35/P38/P39
// control within 0.75%. GCC 15 is therefore the continuous GNU lower bound:
// later GNU majors inherit the placement instead of being rejected for novelty.
// GCC 14 is a measured rejection rather than a version hole: its significant
// result is neutral across two physical cores, while significant-preserve
// regresses by 0.7--1.1% and adds 24--70 text bytes. This compiler/ABI policy
// controls placement only; x32, MinGW, ARM64EC, Clang, GCC 13 and GCC 14 retain
// the shared leaf.
inline constexpr bool binary64_p34_decimal_direct_scientific{
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && 15 <= __GNUC__ && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	true
#else
	false
#endif
};

template <typename flt, bool comma, bool uppercase_e,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  bool json_float, bool direct_scientific = false,
		  ::std::integral char_type, typename decimal_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_rsvflt_rounded_precision_define_impl(
	char_type *iter, decimal_type const &decimal, ::std::size_t precision,
	::std::size_t significant) noexcept
{
	constexpr bool fractional{::fast_io::details::floating_precision_is_fractional<precision_mode>};
	constexpr bool preserve{::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>};
	constexpr auto int32_max{(::std::numeric_limits<::std::int_least32_t>::max)()};
	if constexpr (format == ::fast_io::manipulators::floating_format::scientific)
	{
		auto const fractional_digits{fractional ? precision : significant - 1u};
		if constexpr (direct_scientific)
		{
			return ::fast_io::details::exact_precision_scientific_direct<
				flt, comma, uppercase_e>(iter, decimal, fractional_digits, preserve);
		}
		return ::fast_io::details::exact_precision_scientific<flt, comma, uppercase_e>(
			iter, decimal, fractional_digits, preserve);
	}
	auto virtual_size{decimal.size};
	if constexpr (preserve &&
		(!fractional ||
		 format == ::fast_io::manipulators::floating_format::general))
	{
		if (virtual_size < significant)
		{
			virtual_size = significant;
		}
	}
	if constexpr (format == ::fast_io::manipulators::floating_format::fixed ||
				  (fractional && format == ::fast_io::manipulators::floating_format::decimal))
	{
		return ::fast_io::details::exact_precision_fixed<comma, json_float>(
			iter, decimal, virtual_size, fractional && preserve, precision);
	}
	auto const virtual_padding{virtual_size - decimal.size};
	bool fixed{};
	if constexpr (
		format == ::fast_io::manipulators::floating_format::general)
	{
		auto const rounded_exponent{
			decimal.exponent +
			static_cast<::std::int_least32_t>(decimal.size) - 1};
		if constexpr (
			precision_mode ==
				::fast_io::manipulators::floating_precision::
					charconv_significant)
		{
			fixed = -4 <= rounded_exponent &&
				(rounded_exponent < 0 ||
				 static_cast<::std::size_t>(rounded_exponent) <
					 significant);
		}
		else if constexpr (fractional && preserve)
		{
			if (virtual_padding <= static_cast<::std::size_t>(int32_max))
			{
				auto const virtual_exponent{
					static_cast<::std::int_least64_t>(decimal.exponent) -
					static_cast<::std::int_least64_t>(virtual_padding)};
				fixed = -5 < virtual_exponent && virtual_exponent < 7;
			}
		}
		else
		{
			fixed = -4 <= rounded_exponent && rounded_exponent < 6;
		}
	}
	else if (virtual_padding <= static_cast<::std::size_t>(int32_max))
	{
		auto const virtual_exponent{static_cast<::std::int_least64_t>(decimal.exponent) -
									static_cast<::std::int_least64_t>(virtual_padding)};
		fixed = -5 < virtual_exponent && virtual_exponent < 7;
	}
	if constexpr (format == ::fast_io::manipulators::floating_format::decimal)
	{
		auto const rounded_exponent{decimal.exponent + static_cast<::std::int_least32_t>(decimal.size) - 1};
		::std::size_t fixed_length{};
		if (0 <= rounded_exponent)
		{
			auto const integer_digits{static_cast<::std::size_t>(rounded_exponent) + 1u};
			if (virtual_size <= static_cast<::std::size_t>(rounded_exponent))
			{
				fixed_length = integer_digits;
			}
			else
			{
				fixed_length = ::fast_io::details::exact_precision_saturating_add(
					virtual_size, virtual_size == integer_digits ? 1u : 2u);
			}
		}
		else
		{
			fixed_length = ::fast_io::details::exact_precision_saturating_add(
				virtual_size, static_cast<::std::size_t>(-rounded_exponent) + 1u);
		}
		auto const scientific_length{
			::fast_io::details::print_rsv_fp_scientific_length(
				rounded_exponent, virtual_size)};
		fixed = scientific_length >= fixed_length;
	}
	if (fixed)
	{
		// Fractional preserving mode carries the requested decimal quantum even
		// when general notation selects the fixed presentation.  The compact decimal
		// stores only significant digits, so the fixed writer must append the missing
		// suffix through P.  In particular, a rounded zero at P=1..4 is `0.0` through
		// `0.0000`; suppressing force_fractional would incorrectly canonicalize it to
		// `0`.  Non-preserving and significant modes retain their established path.
		return ::fast_io::details::exact_precision_fixed<comma, json_float>(
			iter, decimal, virtual_size, fractional && preserve, precision);
	}
	if constexpr (direct_scientific)
	{
		return ::fast_io::details::exact_precision_scientific_direct<
			flt, comma, uppercase_e>(iter, decimal, virtual_size - 1u, preserve);
	}
	return ::fast_io::details::exact_precision_scientific<flt, comma, uppercase_e>(
		iter, decimal, virtual_size - 1u, preserve);
}

// The common binary64 significant-precision path needs a native 128-bit
// coefficient because 10^33 does not fit in uint64_t.  This is a representation
// capability gate, not an ISA-specific rounding rule.  Targets without native
// u128 retain the existing exact materializer for every value and format.
#if defined(__SIZEOF_INT128__)
// Presentation-independent result of rounding one positive normal binary64
// magnitude to P significant decimal digits, 23 <= P <= 33.  It represents
//
//   coefficient * 10^(real_exponent + 1 - P).
//
// Since coefficient < 10^33 < 2^110, bits 112..121 hold real_exponent + 512
// and bits 122..126 hold P - 23.  A normal finite binary64 has decimal leading
// exponent in [-308, 308], so ten biased exponent bits are complete.  Packing
// all state into sixteen bytes preserves register return on AArch64 and bounds
// live ranges on the compiler/format combinations selected below.  Zero is the
// failure sentinel; a successful coefficient cannot be zero.
struct binary64_common_significant_precision_carrier
{
	__uint128_t storage{};

	[[nodiscard]] inline constexpr bool success() const noexcept
	{
		return storage != 0u;
	}

	[[nodiscard]] inline constexpr __uint128_t coefficient() const noexcept
	{
		return storage & ((static_cast<__uint128_t>(1u) << 112u) - 1u);
	}

	[[nodiscard]] inline constexpr ::std::int_least32_t real_exponent() const noexcept
	{
		return static_cast<::std::int_least32_t>((storage >> 112u) & 0x3ffu) - 512;
	}

	[[nodiscard]] inline constexpr ::std::size_t digits() const noexcept
	{
		return static_cast<::std::size_t>((storage >> 122u) & 0x1fu) + 23u;
	}
};

static_assert(sizeof(binary64_common_significant_precision_carrier) ==
	sizeof(__uint128_t));

// Compute the shared DA result for P20-P33.  The all-presentation production
// dispatcher starts at P23, while the separately measured scientific-only
// dispatcher uses P20-P22 without the packed carrier.  A complete
// P34 needs M=10^19, another fractional word and a different error bound, so it
// deliberately uses a separate all-layout dispatcher rather than widening this
// carrier and perturbing the P23-P33 assembly.  P23 is the production
// performance boundary: paired common- and broad-exponent measurements on Apple
// M4 and recent x86-64 Clang/GCC found P23-P33 to be the first contiguous
// interval which improves all four renderers.  P16-P19 retain their established
// paths; P20-P22 use this arithmetic only for scientific output, where a
// separately outlined renderer amortizes the shorter high limb on the measured
// x86-64 compilers.  These cutoffs are code-generation policy, not mathematics.
// The sole dispatcher proves a finite-normal raw exponent in [1, 2046] and
// 20 <= significant <= 33 before entering this helper.  Keeping that
// qualification at the selected public/slow branch avoids repeating comparisons
// in every accepted conversion; any future caller must establish the same
// precondition before using this helper.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr
::fast_io::details::da::binary64_scientific_wide_precision_result
compute_binary64_common_significant_precision(
	::std::uint_least64_t mantissa, ::std::uint_least32_t raw_exponent,
	::std::size_t significant) noexcept
{
	FAST_IO_ASSUME(raw_exponent - 1u < 2046u);
	FAST_IO_ASSUME(significant - 20u < 14u);
	constexpr ::std::uint_least64_t implicit_bit{static_cast<::std::uint_least64_t>(1ULL) << 52u};
	auto const multiplier{
		::fast_io::details::print_rsv_fp_pow10_0_to_19_table[significant - 16u]};
	constexpr __uint128_t decimal_limb{static_cast<::std::uint_least64_t>(10000000000000000000ULL)};
	auto const normalization_threshold{decimal_limb *
		::fast_io::details::print_rsv_fp_pow10_0_to_19_table[significant - 19u]};
	auto const normalized_significand{decimal_limb *
		::fast_io::details::print_rsv_fp_pow10_0_to_19_table[significant - 20u]};

	// For P20-P33 the generated-power indices are respectively 4..17, 1..14
	// and 0..13.  Every access is inside the shared 10^0..10^19 table,
	// multiplier fits u64, and normalization_threshold <= 10^33 < 2^110.
	// Supplying 1e19*10^(P-20) directly avoids u128 division by ten and keeps
	// both normalization alternatives on the same generated table.
	return ::fast_io::details::da::compute_binary64_scientific_wide_precision(
		mantissa | implicit_bit, raw_exponent, multiplier,
		normalization_threshold, normalized_significand);
}

[[nodiscard]] inline constexpr
binary64_common_significant_precision_carrier
compute_binary64_common_significant_precision_carrier(
	::std::uint_least64_t mantissa, ::std::uint_least32_t raw_exponent,
	::std::size_t significant) noexcept
{
	// The carrier spends five metadata bits on P-23; unlike the arithmetic
	// helper, it is valid only for the independently guarded P23-P33 interval.
	FAST_IO_ASSUME(significant - 23u < 11u);
	auto const converted{::fast_io::details::
		compute_binary64_common_significant_precision(
			mantissa, raw_exponent, significant)};
	if (!converted.success)
	{
		return {};
	}
	auto const metadata{
		(static_cast<__uint128_t>(converted.exponent + 512) << 112u) |
		(static_cast<__uint128_t>(significant - 23u) << 122u)};
	return {converted.significand | metadata};
}

// The cached power is a lower endpoint.  Before multiplication by M, the
// omitted cache/product tail is less than 2049/2048 fixed-point units.  The DA
// computation rejects the complete interval
//
//   [2^63 - (M + floor((M - 1)/2048)), 2^63].
//
// Restoring the one-sided tail therefore cannot change an accepted comparison
// with one half.  Exact and potentially ambiguous ties fail, so every accepted
// result is common to all six nearest policies.  Directed policies need lower
// or upper endpoint selection and never dispatch this path.
template <bool preserve_trailing_zero = false>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr
::fast_io::details::exact_precision_compact_window_decimal
materialize_binary64_common_significant_precision(
	__uint128_t coefficient, ::std::int_least32_t real_exponent,
	::std::size_t significant) noexcept
{
	::fast_io::details::exact_precision_compact_window_decimal decimal{};
	decimal.size = significant;
	decimal.exponent = real_exponent + 1 -
		static_cast<::std::int_least32_t>(significant);

	// P20-P38 always crosses the exact window's existing 1e19 limb.  Reuse its
	// reciprocal division and digit writers; no precision table or format-
	// specific power cache is introduced.
	auto const division{::fast_io::details::
		exact_precision_window_divide_128_by_decimal_limb_full(coefficient)};
	auto const high_digits{significant - 19u};
	::fast_io::details::jeaiii::jeaiii_main_len<false, false>(
		decimal.digits, static_cast<::std::uint_least64_t>(division.quotient),
		static_cast<::std::uint_least32_t>(high_digits));
	::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
		decimal.digits + high_digits, division.remainder, 19u);

	// The unsigned-char jeaiii writer and char_literal_v<u8'0', unsigned char>
	// use the same char8 digit-code base, independently of the ordinary execution
	// character set.  Their difference is therefore the numeric digit [0,9] under
	// both ASCII and EBCDIC; the renderer later widens that numeric value.
	constexpr auto zero{::fast_io::char_literal_v<u8'0', unsigned char>};
	for (::std::size_t index{}; index != significant; ++index)
	{
		decimal.digits[index] = static_cast<unsigned char>(
			decimal.digits[index] - zero);
	}

	if constexpr (!preserve_trailing_zero)
	{
		// Removing one or more trailing coefficient zeroes and incrementing
		// decimal.exponent by the same count preserves the represented value.
		// Trimming must precede non-preserving presentation because general and
		// decimal choose their layout from the final coefficient length.
		::fast_io::details::exact_precision_trim(decimal);
	}
	return decimal;
}

// Keep the compact-carrier materializer as a normal inline boundary.  The
// measured packed configurations profit when the compiler shares this body and
// passes only the sixteen-byte carrier, while the direct configurations below
// enter the scalar overload above and keep its state in the caller.  An explicit
// noinline attribute was rejected because it cost up to about 1.5 ns/value on
// M4; compiler inlining remains permitted when its cost model favors it.
inline constexpr ::fast_io::details::exact_precision_compact_window_decimal
materialize_binary64_common_significant_precision(
	binary64_common_significant_precision_carrier carrier) noexcept
{
	return ::fast_io::details::materialize_binary64_common_significant_precision(
		carrier.coefficient(), carrier.real_exponent(), carrier.digits());
}

template <bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_format format, bool json_float,
	bool direct_scientific = false, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
render_binary64_common_significant_precision(
	char_type *iter, __uint128_t coefficient,
	::std::int_least32_t real_exponent, ::std::size_t significant) noexcept
{
	auto const decimal{::fast_io::details::
		materialize_binary64_common_significant_precision(
			coefficient, real_exponent, significant)};
	// Rounding and digit construction are presentation-independent.  Existing
	// renderers retain comma-point, JSON suffix and character conversion rules.
	return ::fast_io::details::print_rsvflt_rounded_precision_define_impl<
		double, comma, uppercase_e, format,
		::fast_io::manipulators::floating_precision::significant, json_float,
		direct_scientific>(
			iter, decimal, significant, significant);
}

// Packing changes register lifetime rather than arithmetic. The policy below is
// derived from post-sign AB/BA measurements, matching the production
// insertion point.  Apple Clang 23/M4 direct results improved scientific,
// fixed, general and decimal by 2.04%, 2.72%, 1.00% and 3.22%.  On i9-14900HX,
// GCC 13 direct won both common- and broad-exponent aggregates for all formats.
// Physical-core GCC 14 AB/BA runs over every P23--P33 common and broad cell also
// select the all-format direct form: on cores 3 and 4, respectively, general
// improves by 3.9%/4.6%, scientific by 3.9%/4.6%, fixed by 4.3%/4.9%, and
// decimal by 2.6%/3.4%; every output hash agrees and linked text shrinks by
// 121--298 bytes. GCC 15 direct instead wins fixed/decimal but regresses
// scientific/general by up to 7.61%/12.38%; GCC 15 is therefore the continuous
// lower bound for that narrower format policy, and later GNU frontends inherit
// it. Clang 23 direct wins fixed/general while decimal regresses about 2.7%.
// Clang 22 and trunk Clang 24 complete precision dispatchers are not
// instruction-identical to 23 under the direct policy, so only measured Clang
// 23 receives its format split. The x86 arms are restricted to Linux System V
// x86-64 LP64; x32, MinGW, the Microsoft ABI and non-Linux x86-64 do not inherit
// those empirical schedules.
template <::fast_io::manipulators::floating_format format>
inline constexpr bool binary64_common_significant_precision_direct{
#if defined(__clang__) && 23 <= __clang_major__ && defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	true
#elif defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && \
	(__GNUC__ == 13 || __GNUC__ == 14) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	true
#elif defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && 15 <= __GNUC__ && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	format == ::fast_io::manipulators::floating_format::fixed ||
		format == ::fast_io::manipulators::floating_format::decimal
#elif defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__clang__) && __clang_major__ == 23 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	format == ::fast_io::manipulators::floating_format::fixed ||
		format == ::fast_io::manipulators::floating_format::general
#else
	false
#endif
};

// Try the common non-preserving P23-P33 path.  The direct form keeps the DA
// result live through immediate materialization; the compact form deliberately
// crosses the sixteen-byte carrier to shorten live ranges on the measured
// compiler/format combinations above.  Both consume the same proved result and
// renderer, and both return null before writing when the ambiguity test fails.
template <bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_format format, bool json_float,
	::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
print_rsvflt_binary64_common_significant_precision_impl(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t raw_exponent, ::std::size_t significant) noexcept
{
	static_assert(format == ::fast_io::manipulators::floating_format::scientific ||
		format == ::fast_io::manipulators::floating_format::fixed ||
		format == ::fast_io::manipulators::floating_format::general ||
		format == ::fast_io::manipulators::floating_format::decimal);
	if constexpr (::fast_io::details::
		binary64_common_significant_precision_direct<format>)
	{
		auto const converted{::fast_io::details::
			compute_binary64_common_significant_precision(
				mantissa, raw_exponent, significant)};
		if (!converted.success)
		{
			return nullptr;
		}
		return ::fast_io::details::render_binary64_common_significant_precision<
			comma, uppercase_e, format, json_float>(iter, converted.significand,
				converted.exponent, significant);
	}
	else
	{
		auto const carrier{::fast_io::details::
			compute_binary64_common_significant_precision_carrier(
				mantissa, raw_exponent, significant)};
		if (!carrier.success())
		{
			return nullptr;
		}
		auto const decimal{::fast_io::details::
			materialize_binary64_common_significant_precision(carrier)};
		return ::fast_io::details::print_rsvflt_rounded_precision_define_impl<
			double, comma, uppercase_e, format,
			::fast_io::manipulators::floating_precision::significant, json_float>(
				iter, decimal, carrier.digits(), carrier.digits());
	}
}

// Keep the runtime P23-P33 range dispatch physically separate from the public
// precision entry.  The call is taken only for the eleven enabled precisions;
// all smaller and larger requests retain a compact fall-through body.  This
// boundary is backed by full P1-P128 AB/BA data: the equivalent always-inlined
// probe expanded the M4 low-P entry, whereas placing the probe after Dragonbox
// added its conversion latency to every hit.  The wrapper changes no arithmetic
// or fallback condition.  On a compiler without `noinline`, its normal inlining
// decision may change text size but cannot change the carrier or exact fallback.
template <bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_format format, bool json_float,
	::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *
print_rsvflt_binary64_common_significant_precision_dispatch(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t raw_exponent, ::std::size_t significant) noexcept
{
	return ::fast_io::details::
		print_rsvflt_binary64_common_significant_precision_impl<
			comma, uppercase_e, format, json_float>(
				iter, mantissa, raw_exponent, significant);
}

// P34 keeps its original constant renderer arguments.  Paired M4 AB/BA data
// demonstrated that passing its width through the P35-P38 runtime dispatcher
// regressed several output modes by 2.4--5.4%.  This small adapter invokes the
// existing general renderer rather than cloning it; its separate presentation
// boundary is therefore code-generation evidence, not a second decimal rule.
template <bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_format format, bool json_float,
	::fast_io::manipulators::floating_precision precision_mode =
		::fast_io::manipulators::floating_precision::significant,
	::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *
print_rsvflt_binary64_p34_precision_dispatch(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t raw_exponent) noexcept
{
	constexpr bool significant_mode{
		precision_mode ==
			::fast_io::manipulators::floating_precision::significant ||
		precision_mode == ::fast_io::manipulators::floating_precision::
			significant_preserve_trailing_zero};
	constexpr bool scientific_fractional_mode{
		format == ::fast_io::manipulators::floating_format::scientific &&
		precision_mode ==
			::fast_io::manipulators::floating_precision::fractional};
	static_assert(significant_mode || scientific_fractional_mode);
	constexpr ::std::size_t output_precision{
		scientific_fractional_mode ? 33u : 34u};
	auto const carrier{::fast_io::details::
		compute_binary64_p34_precision_carrier(mantissa, raw_exponent)};
	if (!carrier)
	{
		return nullptr;
	}
	constexpr bool direct_scientific{
		::fast_io::details::binary64_p34_decimal_direct_scientific &&
		format == ::fast_io::manipulators::floating_format::decimal &&
		::std::same_as<char_type, char>};
	if constexpr (precision_mode ==
		::fast_io::manipulators::floating_precision::significant)
	{
		return ::fast_io::details::render_binary64_common_significant_precision<
			comma, uppercase_e, format, json_float, direct_scientific>(iter,
			carrier & binary64_p34_precision_coefficient_mask,
			static_cast<::std::int_least32_t>(carrier >> 113u) - 512, 34u);
	}
	else
	{
		constexpr bool preserve{::fast_io::details::
			floating_precision_preserves_trailing_zero<precision_mode>};
		auto const decimal{::fast_io::details::
			materialize_binary64_common_significant_precision<preserve>(
				carrier & binary64_p34_precision_coefficient_mask,
				static_cast<::std::int_least32_t>(carrier >> 113u) - 512, 34u)};
		return ::fast_io::details::print_rsvflt_rounded_precision_define_impl<
			double, comma, uppercase_e, format, precision_mode, json_float,
			direct_scientific>(
				iter, decimal, output_precision, 34u);
	}
}

// P35-P38 coefficients consume up to 127 bits, leaving no room to pack the
// decimal exponent as P34 does. A coefficient/exponent aggregate is larger
// than sixteen bytes and is returned indirectly by both AArch64 and System V
// x86-64 ABIs. Keep the mathematical result scalarized inside a constant-width
// helper, then return the coefficient in the native u128 result registers and
// write the exponent through one caller-owned scalar. Zero is an unambiguous
// failure sentinel because a successful P35 coefficient is at least 10^34.
// These definitions intentionally follow the complete P34 presentation block:
// their emitted arithmetic must not perturb the measured alignment or address
// order of the unchanged P34 fused and generic controls.
template <::std::size_t digits>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr __uint128_t
compute_binary64_p35_p38_precision_constant_carrier(
	::std::uint_least64_t binary_significand,
	::std::uint_least32_t raw_exponent,
	::std::int_least32_t &real_exponent) noexcept
{
	static_assert(35u <= digits && digits <= 38u);
	auto const converted{::fast_io::details::da::
		compute_binary64_scientific_p35_p38_precision<digits>(
			binary_significand, raw_exponent)};
	if (!converted.success)
	{
		return 0u;
	}
	real_exponent = converted.exponent;
	return converted.significand;
}

// One outlined runtime carrier owns all four constant arithmetic
// specializations. This measured shape bounds the global numeric text at about
// 1.9--2.6 KiB instead of cloning the 128x128 carry chain into every format,
// precision mode and destination character type. The outline is a uniform
// floating-layer code-size policy, not an ISA or compiler semantic choice.
// Compilers without the optional GNU attribute retain ordinary inline cost
// modelling; widening this policy with a target conditional requires paired
// latency, linked-text, return-ABI and spill evidence.
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
[[nodiscard]] inline constexpr __uint128_t
compute_binary64_p35_p38_precision_carrier(
	::std::uint_least64_t mantissa,
	::std::uint_least32_t raw_exponent,
	::std::size_t significant,
	::std::int_least32_t &real_exponent) noexcept
{
	constexpr ::std::uint_least64_t implicit_bit{
		static_cast<::std::uint_least64_t>(1ULL) << 52u};
	auto const binary_significand{mantissa | implicit_bit};
	switch (significant)
	{
	case 35u:
		return ::fast_io::details::
			compute_binary64_p35_p38_precision_constant_carrier<35u>(
				binary_significand, raw_exponent, real_exponent);
	case 36u:
		return ::fast_io::details::
			compute_binary64_p35_p38_precision_constant_carrier<36u>(
				binary_significand, raw_exponent, real_exponent);
	case 37u:
		return ::fast_io::details::
			compute_binary64_p35_p38_precision_constant_carrier<37u>(
				binary_significand, raw_exponent, real_exponent);
	case 38u:
		return ::fast_io::details::
			compute_binary64_p35_p38_precision_constant_carrier<38u>(
				binary_significand, raw_exponent, real_exponent);
	default:
		return 0u;
	}
}

// P35-P38 share one runtime presentation boundary and one global numeric
// carrier.  Their 117--127-bit coefficients leave no exponent-packing space,
// so the carrier returns u128 and writes one scalar exponent.  A separate copy
// of the general renderer is unnecessary: every mode delegates punctuation,
// widening, EBCDIC and JSON behavior to the existing renderer.  Every numeric
// failure precedes all writes; the caller admits only finite normal binary64 and
// the six nearest policies, for which rejection of every possible tie proves a
// common coefficient.
template <bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_format format, bool json_float,
	::fast_io::manipulators::floating_precision precision_mode =
		::fast_io::manipulators::floating_precision::significant,
	::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *
print_rsvflt_binary64_p35_p38_precision_dispatch(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t raw_exponent,
	::std::size_t significant) noexcept
{
	constexpr bool significant_mode{
		precision_mode ==
			::fast_io::manipulators::floating_precision::significant ||
		precision_mode == ::fast_io::manipulators::floating_precision::
			significant_preserve_trailing_zero};
	constexpr bool scientific_fractional_mode{
		format == ::fast_io::manipulators::floating_format::scientific &&
		::fast_io::details::
			floating_precision_is_fractional<precision_mode>};
	static_assert(significant_mode || scientific_fractional_mode);
	if (significant - 35u >= 4u)
	{
		return nullptr;
	}
	::std::int_least32_t real_exponent;
	auto const coefficient{::fast_io::details::
		compute_binary64_p35_p38_precision_carrier(
			mantissa, raw_exponent, significant, real_exponent)};
	if (!coefficient)
	{
		return nullptr;
	}
	constexpr bool fused_scientific_preserve{
		format == ::fast_io::manipulators::floating_format::scientific &&
		::fast_io::details::
			floating_precision_preserves_trailing_zero<precision_mode>};
	if constexpr (fused_scientific_preserve)
	{
		auto const high_digits{significant - 19u};
		auto const division{::fast_io::details::
			exact_precision_window_divide_128_by_decimal_limb_full(coefficient)};
		::fast_io::details::jeaiii::jeaiii_main_len<false, false>(
			iter + 1u,
			static_cast<::std::uint_least64_t>(division.quotient),
			static_cast<::std::uint_least32_t>(high_digits));
		::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
			iter + high_digits + 1u, division.remainder, 19u);
		*iter = iter[1];
		iter[1] = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
		return ::fast_io::details::print_rsv_fp_e_impl<double, uppercase_e>(
			iter + significant + 1u, real_exponent);
	}
	else if constexpr (precision_mode ==
		::fast_io::manipulators::floating_precision::significant)
	{
		return ::fast_io::details::render_binary64_common_significant_precision<
			comma, uppercase_e, format, json_float>(iter,
				coefficient, real_exponent, significant);
	}
	else
	{
		constexpr bool preserve{::fast_io::details::
			floating_precision_preserves_trailing_zero<precision_mode>};
		auto const decimal{::fast_io::details::
			materialize_binary64_common_significant_precision<preserve>(
				coefficient, real_exponent, significant)};
		auto const output_precision{scientific_fractional_mode ?
			significant - 1u : significant};
		return ::fast_io::details::print_rsvflt_rounded_precision_define_impl<
			double, comma, uppercase_e, format, precision_mode, json_float>(
				iter, decimal, output_precision, significant);
	}
}

// Some precision semantics need the complete P23-P33 coefficient rather than
// the canonical trailing-zero-trimmed carrier used by the non-preserving
// significant dispatcher.  Compute and materialize that coefficient once, then
// delegate punctuation, JSON and destination-character handling to the common
// renderer.  The compile-time preserve parameter suppresses only the final
// trim; digit generation and its EBCDIC-independent normalization stay shared.
// The preserved decimal is exactly
//
//   digits[0..P) * 10^(real_exponent + 1 - P),
//
// with no second digit conversion or table.  The caller admits only normal
// binary64 and P23-P33, so the DA proof and ambiguity fallback are the same as
// for the non-preserving common dispatcher.
template <bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode,
	bool json_float, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *
print_rsvflt_binary64_common_precision_decimal_dispatch(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t raw_exponent, ::std::size_t significant,
	::std::size_t precision) noexcept
{
	auto const converted{::fast_io::details::
		compute_binary64_common_significant_precision(
			mantissa, raw_exponent, significant)};
	if (!converted.success)
	{
		return nullptr;
	}
	constexpr bool preserve{::fast_io::details::
		floating_precision_preserves_trailing_zero<precision_mode>};
	auto const decimal{::fast_io::details::
		materialize_binary64_common_significant_precision<preserve>(
			converted.significand, converted.exponent, significant)};
	return ::fast_io::details::print_rsvflt_rounded_precision_define_impl<
		double, comma, uppercase_e, format, precision_mode, json_float>(
			iter, decimal, precision, significant);
}

// P20-P22 fit the proved common-significant arithmetic domain but not the
// 112-bit packed carrier used by selected x86-64 presentation paths.  Keeping
// scientific rendering behind its own non-inlined boundary prevents the
// P23-P33 carrier policy and the other three presentations from inheriting the
// extra live range.  Balanced, independently linked AB/BA binaries on an
// i9-14900HX improved Clang 23 by 23.7--31.5%, GCC 13 by 42.0--46.3%, and GCC
// 15 by 43.8--45.9% across common and broad-exponent corpora. Physical-core
// GCC 14 AB/BA runs independently improve the combined P20--P22 corpus by
// 41.7--41.8%, with broad values faster by 43.5--43.9% and common values by
// 39.6--39.9%; every byte-differential output agrees. The specialized GCC 14
// body adds 1,900 linked-text bytes, 467 instructions and three calls, a bounded
// front-end cost accepted for the approximately 42% latency reduction. Apple
// Clang 23
// uses the already-direct common dispatcher in its public entry: widening that
// dispatcher's one range test to P20-P33 adds neither a miss-path comparison nor
// a second P23-P33 branch.  Three million finite-normal differential conversions
// produced no mismatch; P23-P33 controls moved by at most 0.68%. GCC 13 is now
// the continuous GNU lower bound, so later frontends inherit the direct carrier.
// The forced P20 path adds one call on Clang 22 and two on trunk Clang
// 24, so only measured Clang 23 selects this carrier. Other ABIs retain the exact
// fallback.
inline constexpr bool binary64_scientific_p20_p22_direct{
#if defined(__clang__) && 23 <= __clang_major__ && defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	true
#elif defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__clang__) && __clang_major__ == 23 && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	true
#elif defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__ && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
	true
#else
	false
#endif
};

// The caller has already proved a finite normal and P20-P22.  Directly carry
// the DA coefficient into the scientific renderer: packing would truncate its
// metadata convention, while materializing the same exact prefix, guard and
// sticky result preserves all six nearest rounding policies.  A null return is
// an ambiguity rejection before any character is written.
template <bool comma, bool uppercase_e, bool json_float,
	::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *
print_rsvflt_binary64_scientific_p20_p22_significant_dispatch(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t raw_exponent, ::std::size_t significant) noexcept
{
	auto const converted{::fast_io::details::
		compute_binary64_common_significant_precision(
			mantissa, raw_exponent, significant)};
	if (!converted.success)
	{
		return nullptr;
	}
	return ::fast_io::details::render_binary64_common_significant_precision<
		comma, uppercase_e,
		::fast_io::manipulators::floating_format::scientific, json_float>(
			iter, converted.significand, converted.exponent, significant);
}

// P16-P22 are the precision cliff immediately below the packed P23-P33
// carrier.  Materializing their accepted DA coefficient in one shared numeric-
// digit object avoids constructing the complete binary64 decimal expansion and
// prevents the arithmetic body from being cloned by presentation, punctuation,
// character type, JSON policy and the two significant-precision modes.  Twenty-
// two digits are sufficient because an accepted result is in [10^(P-1),10^P).
struct binary64_p16_p22_significant_decimal
{
	unsigned char digits[22u];
	::std::size_t size;
	::std::int_least32_t exponent;
};

// Keep the narrow width in the type system.  Besides folding the DA scale and
// source offset, this gives range diagnostics a direct proof that both the
// nineteen-byte scratch read and the twenty-two-byte decimal write are bounded.
// GCC cannot derive that bound from the surrounding runtime switch and reports
// a false stringop-overflow under -Werror when the loop width remains dynamic.
template <::std::size_t significant>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool
materialize_binary64_p16_p19_significant_decimal(
	::fast_io::details::binary64_p16_p22_significant_decimal &decimal,
	::std::uint_least64_t mantissa,
	::std::uint_least32_t raw_exponent) noexcept
{
	static_assert(16u <= significant && significant <= 19u);
	constexpr ::std::uint_least64_t implicit_bit{
		static_cast<::std::uint_least64_t>(1ULL) << 52u};
	auto const converted{::fast_io::details::da::
		compute_binary64_scientific_precision<significant>(
			mantissa | implicit_bit, raw_exponent)};
	if (!converted.success)
	{
		return false;
	}
	decimal.size = significant;
	decimal.exponent = converted.exponent + 1 -
		static_cast<::std::int_least32_t>(significant);

	// A fixed nineteen-position writer is smaller than instantiating the generic
	// runtime-length integer writer.  Its leading zero positions are scratch;
	// increasing indices make the subsequent left shift overlap-safe.
	::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
		decimal.digits, converted.significand, 19u);
	constexpr ::std::size_t source_offset{19u - significant};
	constexpr auto zero{::fast_io::char_literal_v<u8'0', unsigned char>};
	for (::std::size_t index{}; index != significant; ++index)
	{
		decimal.digits[index] = static_cast<unsigned char>(
			decimal.digits[index + source_offset] - zero);
	}
	return true;
}

// The narrow and wide DA computations use the same cached lower endpoint.  If
// M is the decimal extension multiplier, their omitted cache/product tail is
// bounded by M + floor((M-1)/2048) fixed-point units.  Each helper rejects the
// complete interval which that error can move across one half, including exact
// ties.  Consequently every accepted coefficient is common to all six nearest
// policies; a rejection writes nothing and leaves the exact prefix/guard/sticky
// formatter authoritative.
//
// This normal-binary64 helper is intentionally outlined.  It gives every
// presentation and destination character type one arithmetic/materialization
// body instead of instantiating seven precision cases per semantic wrapper.
// The attribute changes code placement only; compilers without it retain the
// identical interval proof and fallback contract.
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
[[nodiscard]] inline constexpr bool materialize_binary64_p16_p22_significant_decimal(
	::fast_io::details::binary64_p16_p22_significant_decimal &decimal,
	::std::uint_least64_t mantissa, ::std::uint_least32_t raw_exponent,
	::std::size_t significant) noexcept
{
	if (significant < 20u)
	{
		switch (significant)
		{
		case 16u:
			return ::fast_io::details::
				materialize_binary64_p16_p19_significant_decimal<16u>(
					decimal, mantissa, raw_exponent);
		case 17u:
			return ::fast_io::details::
				materialize_binary64_p16_p19_significant_decimal<17u>(
					decimal, mantissa, raw_exponent);
		case 18u:
			return ::fast_io::details::
				materialize_binary64_p16_p19_significant_decimal<18u>(
					decimal, mantissa, raw_exponent);
		case 19u:
			return ::fast_io::details::
				materialize_binary64_p16_p19_significant_decimal<19u>(
					decimal, mantissa, raw_exponent);
		default:
			return false;
		}
	}

	if (22u < significant)
	{
		return false;
	}
	auto const converted{::fast_io::details::
		compute_binary64_common_significant_precision(
			mantissa, raw_exponent, significant)};
	if (!converted.success)
	{
		return false;
	}
	decimal.size = significant;
	decimal.exponent = converted.exponent + 1 -
		static_cast<::std::int_least32_t>(significant);
	auto const division{::fast_io::details::
		exact_precision_window_divide_128_by_decimal_limb_full(
			converted.significand)};
	auto const high_digits{significant - 19u};
	::fast_io::details::jeaiii::jeaiii_main_len<false, false>(
		decimal.digits, static_cast<::std::uint_least64_t>(division.quotient),
		static_cast<::std::uint_least32_t>(high_digits));
	::fast_io::details::jeaiii::jeaiii_main_len<false, true>(
		decimal.digits + high_digits, division.remainder, 19u);
	// The unsigned-char jeaiii writer and char_literal_v<u8'0', unsigned char>
	// use the same char8 digit-code base, independently of the ordinary execution
	// character set.  Subtraction produces numeric digits [0,9] under both ASCII
	// and EBCDIC; the renderer widens them to the requested destination type.
	constexpr auto zero{::fast_io::char_literal_v<u8'0', unsigned char>};
	for (::std::size_t index{}; index != significant; ++index)
	{
		decimal.digits[index] = static_cast<unsigned char>(
			decimal.digits[index] - zero);
	}
	return true;
}

// Probe placement is an ISA front-end policy, not a numeric capability test.
// Apple Clang 23/M4 keeps the outlined probe in the public entry: three complete
// P1-P128 AB/BA runs left miss ranges within noise while P23-P33 improved by
// 39.6--49.6%.  On i9-14900HX, the same single public miss branch regressed
// Clang 23 and GCC 13/15 fixed P1-P22 by 1.7--5.3% geometrically, with individual
// rows worse by up to 16.9%; x86-64 therefore probes only after entering the
// existing slow dispatcher.  Apple GCC 15 likewise regressed scientific P1-P22
// by 5.5% geometrically with the public branch.  The fast placement is therefore
// the narrowest portable compile-time proxy for the measured Apple Clang 23/M4
// artifact: it does not identify the runtime CPU, so other Apple AArch64
// processors and OS revisions remain an explicit family-level hypothesis.
// Clang 23 is the lower bound and later Clang majors inherit the placement;
// GCC and non-Apple AArch64 take the conservative slow placement. Both
// placements call the identical proved carrier and exact fallback.
inline constexpr bool binary64_common_significant_precision_public_dispatch{
#if defined(__clang__) && 23 <= __clang_major__ && defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	true
#else
	false
#endif
};
#endif

// Binary32 exact power-of-ten scientific output reuses the u128 prefix-window
// representation and its guard/sticky rounding.  Compilers without native u128
// retain the generic exact decimal path; the availability gate changes neither
// accepted formats nor rounding semantics.
#if defined(__SIZEOF_INT128__)
// Separate binary32 power-of-ten emission from its prefix materializer: the
// writer owns punctuation, padding and the backward rounding carry scan, while
// the caller owns exact digits and sticky state.  Outlining keeps both live
// ranges from overlapping; it changes no character or rounding rule.  Without
// `noinline`, the same writer may be folded into its caller with identical
// guard/sticky and carry behavior.
template <typename flt, bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_rounding rounding, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *exact_precision_print_binary32_power10_scientific(
	char_type *iter, exact_precision_binary32_power10_decimal_entry const &decimal,
	::std::size_t significant, bool negative) noexcept
{
	auto const begin{iter};
	auto real_exponent{decimal.real_exponent};
	*iter++ = ::fast_io::char_literal_add<char_type>(decimal.digits[0]);
	if (1u < significant)
	{
		*iter++ = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	}
	auto const copied{decimal.size < significant ? decimal.size : significant};
	iter = ::fast_io::details::exact_precision_copy(iter, decimal, 1u, copied);
	if (copied < significant)
	{
		iter = ::fast_io::details::fill_zeros_impl(iter, significant - copied);
	}
	if (significant < decimal.size)
	{
		auto const guard{static_cast<unsigned>(decimal.digits[significant])};
		auto const tail_nonzero{significant + 1u < decimal.size};
		auto const discarded_nonzero{guard != 0u || tail_nonzero};
		bool round_up{};
		if (discarded_nonzero)
		{
			if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
			{
				if (5u < guard || (guard == 5u && tail_nonzero))
				{
					round_up = true;
				}
				else if (guard == 5u)
				{
					auto const zero{char_literal_v<u8'0', char_type>};
					auto const rounded_down{
						static_cast<::std::uint_least64_t>(iter[-1] - zero)};
					round_up = ::fast_io::details::print_rsv_fp_decimal_tie_round_up<rounding>(
						negative, rounded_down);
				}
			}
			else
			{
				round_up =
					::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
			}
		}
		if (round_up)
		{
			auto const zero{char_literal_v<u8'0', char_type>};
			auto const nine{char_literal_v<u8'9', char_type>};
			auto const point{char_literal_v<(comma ? u8',' : u8'.'), char_type>};
			auto position{iter};
			while (position != begin)
			{
				--position;
				if (*position == point)
				{
					continue;
				}
				if (*position != nine)
				{
					*position = static_cast<char_type>(*position + 1);
					round_up = false;
					break;
				}
				*position = zero;
			}
			if (round_up)
			{
				*begin = char_literal_v<u8'1', char_type>;
				++real_exponent;
			}
		}
	}
	return ::fast_io::details::print_rsv_fp_e_impl<flt, uppercase_e>(iter, real_exponent);
}

template <bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_rounding rounding, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
exact_precision_try_print_binary32_power10_scientific(
	char_type *iter, ::std::uint_least32_t mantissa, ::std::uint_least32_t exponent,
	::std::size_t significant, bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<float>;
	if (255u < exponent)
	{
		return nullptr;
	}
	auto const index{static_cast<::std::size_t>(
		exact_precision_binary32_power10_exponent_index_table[exponent])};
	if (!index)
	{
		return nullptr;
	}
	auto const magnitude_bits{(exponent << trait::mbits) | mantissa};
	if (::fast_io::details::exact_precision_binary32_negative_power10_table[index] !=
		magnitude_bits)
	{
		return nullptr;
	}
	// The matched raw value has a compile-time generated exact expansion.  Copy
	// that prefix and round it directly, avoiding the runtime 1e9 block stream.
	return ::fast_io::details::exact_precision_print_binary32_power10_scientific<
		float, comma, uppercase_e, rounding>(iter,
			exact_precision_binary32_power10_decimal_table[index],
			significant, negative);
}

template <::fast_io::manipulators::floating_rounding rounding, typename decimal_type>
inline constexpr void exact_precision_window_round(
	decimal_type &decimal, ::std::int_least32_t keep,
	bool negative, bool tail_nonzero) noexcept
{
	auto const first{decimal.digits[static_cast<::std::size_t>(keep)]};
	auto const discarded_nonzero{first != 0u || tail_nonzero};
	bool round_up{};
	if (discarded_nonzero)
	{
		if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
		{
			if (5u < first || (first == 5u && tail_nonzero))
			{
				round_up = true;
			}
			else if (first == 5u)
			{
				auto const rounded_down{keep ? decimal.digits[static_cast<::std::size_t>(keep) - 1u] : 0u};
				round_up = ::fast_io::details::print_rsv_fp_decimal_tie_round_up<rounding>(
					negative, rounded_down);
			}
		}
		else
		{
			round_up = ::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
		}
	}
	auto const target_exponent{decimal.exponent + static_cast<::std::int_least32_t>(decimal.size) - keep};
	if (!keep)
	{
		decimal.size = 1u;
		decimal.digits[0] = static_cast<unsigned char>(round_up);
		decimal.exponent = target_exponent;
		return;
	}
	decimal.size = static_cast<::std::size_t>(keep);
	decimal.exponent = target_exponent;
	if (!round_up)
	{
		return;
	}
	for (auto i{decimal.size}; i; --i)
	{
		if (++decimal.digits[i - 1u] != 10u)
		{
			return;
		}
		decimal.digits[i - 1u] = 0u;
	}
	decimal.digits[0] = 1u;
	decimal.size = 1u;
	decimal.exponent = target_exponent + keep;
}

// Numeric window construction, precision interpretation and decimal rounding
// are independent of the destination character and punctuation.  Keeping them
// in one runtime-policy function prevents the full format x rounding x
// precision-mode matrix from cloning the 512-bit arithmetic.  The caller still
// instantiates the established presentation function, so this sharing changes
// neither layout nor character semantics.
#if defined(__SIZEOF_INT128__)
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr bool exact_precision_wide_prepare(
	unsigned char *output_digits, ::std::size_t &output_size,
	::std::int_least32_t &output_exponent, ::std::size_t &significant,
	__uint128_t significand, unsigned mantissa_bits,
	::std::int_least32_t binary_exponent,
	::std::size_t precision,
	::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode,
	::fast_io::manipulators::floating_rounding rounding,
	bool negative) noexcept
{
	using precision_enum = ::fast_io::manipulators::floating_precision;
	using format_enum = ::fast_io::manipulators::floating_format;
	auto const fractional{precision_mode == precision_enum::fractional ||
		precision_mode == precision_enum::fractional_preserve_trailing_zero};
	auto const preserve{
		precision_mode == precision_enum::significant_preserve_trailing_zero ||
		precision_mode == precision_enum::fractional_preserve_trailing_zero};
	if (128u < precision)
	{
		return false;
	}
	exact_precision_window_result generated{};
	::std::int_least32_t keep{};
	if (fractional)
	{
		if (format == format_enum::scientific)
		{
			significant = precision + 1u;
			keep = static_cast<::std::int_least32_t>(significant);
			generated = ::fast_io::details::
				exact_precision_wide_window_from_significand(
					significand, mantissa_bits, binary_exponent,
					significant + 1u);
		}
		else
		{
			auto const exponent_probe{::fast_io::details::
				exact_precision_wide_window_from_significand(
					significand, mantissa_bits, binary_exponent, 2u)};
			if (!exponent_probe.success)
			{
				return false;
			}
			auto const requested_keep{
				static_cast<::std::int_least64_t>(exponent_probe.real_exponent) + 1 +
				static_cast<::std::int_least64_t>(precision)};
			if (requested_keep < 0 ||
				exact_precision_wide_window_maximum_digits <=
					static_cast<::std::uint_least64_t>(requested_keep))
			{
				return false;
			}
			keep = static_cast<::std::int_least32_t>(requested_keep);
			significant = static_cast<::std::size_t>(keep);
			generated = ::fast_io::details::
				exact_precision_wide_window_from_significand(
					significand, mantissa_bits, binary_exponent,
					significant + 1u);
		}
	}
	else
	{
		significant = precision ? precision : 1u;
		keep = static_cast<::std::int_least32_t>(significant);
		generated = ::fast_io::details::
			exact_precision_wide_window_from_significand(
				significand, mantissa_bits, binary_exponent,
				significant + 1u);
	}
	if (!generated.success)
	{
		return false;
	}
	if (!generated.tail_nonzero)
	{
		::fast_io::details::exact_precision_trim(generated.decimal);
	}
	auto const rounded{
		static_cast<::std::int_least32_t>(generated.decimal.size) > keep};
	if (rounded)
	{
		auto const guard{
			generated.decimal.digits[static_cast<::std::size_t>(keep)]};
		auto const rounded_down{keep
			? generated.decimal.digits[static_cast<::std::size_t>(keep) - 1u]
			: 0u};
		auto const round_up{::fast_io::details::
			exact_precision_window_runtime_round_up(
				rounding, negative, guard, generated.tail_nonzero,
				rounded_down)};
		auto const target_exponent{
			generated.decimal.exponent +
			static_cast<::std::int_least32_t>(generated.decimal.size) - keep};
		if (!keep)
		{
			generated.decimal.size = 1u;
			generated.decimal.digits[0] =
				static_cast<unsigned char>(round_up);
			generated.decimal.exponent = target_exponent;
		}
		else
		{
			generated.decimal.size = static_cast<::std::size_t>(keep);
			generated.decimal.exponent = target_exponent;
			if (round_up)
			{
				bool carry{true};
				for (auto index{generated.decimal.size}; index; --index)
				{
					if (++generated.decimal.digits[index - 1u] != 10u)
					{
						carry = false;
						break;
					}
					generated.decimal.digits[index - 1u] = 0u;
				}
				if (carry)
				{
					generated.decimal.digits[0] = 1u;
					generated.decimal.size = 1u;
					generated.decimal.exponent = target_exponent + keep;
				}
			}
		}
	}
	if (!preserve)
	{
		::fast_io::details::exact_precision_trim(generated.decimal);
	}
	if (fractional && preserve && format == format_enum::general)
	{
		significant = rounded
			? ::fast_io::details::
				exact_precision_fractional_general_rounded_virtual_size(
					generated.decimal, precision)
			: generated.decimal.size;
	}
	output_size = generated.decimal.size;
	output_exponent = generated.decimal.exponent;
	for (::std::size_t index{}; index != output_size; ++index)
	{
		output_digits[index] = generated.decimal.digits[index];
	}
	return true;
}

// Select the strongest available optimizer barrier by attribute capability.
// `noipa` prevents policy-specific clones; `noinline` is the semantic-equivalent
// placement fallback and no target architecture participates in this choice.
template <::std::integral char_type, typename decimal_type>
#if __has_cpp_attribute(gnu::noipa)
[[gnu::noipa]]
#elif __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *exact_precision_wide_call_fixed(
	char_type *iter, decimal_type const &decimal, ::std::size_t virtual_size,
	bool force_fractional, ::std::size_t fractional_precision,
	char_type *(*function)(char_type *, decimal_type const &, ::std::size_t,
		bool, ::std::size_t) noexcept) noexcept
{
	return function(iter, decimal, virtual_size, force_fractional,
		fractional_precision);
}

// The scientific adapter needs the same capability-selected boundary as the
// fixed adapter above so neither presentation is favored by code placement.
template <::std::integral char_type, typename decimal_type>
#if __has_cpp_attribute(gnu::noipa)
[[gnu::noipa]]
#elif __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *exact_precision_wide_call_scientific(
	char_type *iter, decimal_type const &decimal,
	::std::size_t fractional_precision, bool preserve,
	char_type *(*function)(char_type *, decimal_type const &, ::std::size_t,
		bool) noexcept) noexcept
{
	return function(iter, decimal, fractional_precision, preserve);
}

// Keep the presentation policy as data.  The two opaque tail-call adapters
// prevent a compiler from inlining every fixed/scientific body into a sparse
// 4 x 4 switch when a program instantiates only one policy; without that
// boundary GCC grows a two-format binary by more than 100 KiB.  The arithmetic
// below is the runtime spelling of print_rsvflt_rounded_precision_define_impl
// and preserves its notation-choice inequalities exactly.  The preparation
// step supplies fractional-general's carry-aware virtual width for every
// accepted normal or subnormal window; preserve mode can therefore share this
// runtime presentation without losing the requested decimal quantum.
// Keep numeric construction and runtime presentation outlined together.  GCC's
// `noipa` prevents caller-policy cloning; compilers without that capability use
// noinline and preserve the same prefix/guard/sticky and output contracts.
template <typename flt, bool comma, bool uppercase_e, bool json_float,
		  ::std::integral char_type, typename decimal_type>
#if __has_cpp_attribute(gnu::noipa)
[[gnu::noipa]]
#elif __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *exact_precision_wide_runtime_present(
	char_type *iter, decimal_type const &decimal, ::std::size_t precision,
	::std::size_t significant,
	::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode) noexcept
{
	using format_enum = ::fast_io::manipulators::floating_format;
	using precision_enum = ::fast_io::manipulators::floating_precision;
	constexpr auto int32_max{(::std::numeric_limits<::std::int_least32_t>::max)()};
	auto const fractional{precision_mode == precision_enum::fractional ||
		precision_mode == precision_enum::fractional_preserve_trailing_zero};
	auto const preserve{
		precision_mode == precision_enum::significant_preserve_trailing_zero ||
		precision_mode == precision_enum::fractional_preserve_trailing_zero};
	if (format == format_enum::scientific)
	{
		auto const fractional_digits{fractional ? precision : significant - 1u};
		return ::fast_io::details::exact_precision_wide_call_scientific(
			iter, decimal, fractional_digits, preserve,
			&::fast_io::details::exact_precision_scientific<
				flt, comma, uppercase_e, char_type, decimal_type>);
	}
	auto virtual_size{decimal.size};
	if (preserve &&
		(!fractional || format == format_enum::general) &&
		virtual_size < significant)
	{
		virtual_size = significant;
	}
	if (format == format_enum::fixed ||
		(fractional && format == format_enum::decimal))
	{
		return ::fast_io::details::exact_precision_wide_call_fixed(
			iter, decimal, virtual_size, fractional && preserve, precision,
			&::fast_io::details::exact_precision_fixed<
				comma, json_float, char_type, decimal_type>);
	}
	auto const virtual_padding{virtual_size - decimal.size};
	bool fixed{};
	if (format == format_enum::general)
	{
		auto const rounded_exponent{
			decimal.exponent +
			static_cast<::std::int_least32_t>(decimal.size) - 1};
		if (precision_mode == precision_enum::charconv_significant)
		{
			fixed = -4 <= rounded_exponent &&
				(rounded_exponent < 0 ||
				 static_cast<::std::size_t>(rounded_exponent) <
					 significant);
		}
		else if (fractional && preserve)
		{
			if (virtual_padding <= static_cast<::std::size_t>(int32_max))
			{
				auto const virtual_exponent{
					static_cast<::std::int_least64_t>(decimal.exponent) -
					static_cast<::std::int_least64_t>(virtual_padding)};
				fixed = -5 < virtual_exponent && virtual_exponent < 7;
			}
		}
		else
		{
			fixed = -4 <= rounded_exponent && rounded_exponent < 6;
		}
	}
	else if (virtual_padding <= static_cast<::std::size_t>(int32_max))
	{
		auto const virtual_exponent{
			static_cast<::std::int_least64_t>(decimal.exponent) -
			static_cast<::std::int_least64_t>(virtual_padding)};
		fixed = -5 < virtual_exponent && virtual_exponent < 7;
	}
	if (format == format_enum::decimal)
	{
		auto const rounded_exponent{
			decimal.exponent + static_cast<::std::int_least32_t>(decimal.size) - 1};
		::std::size_t fixed_length{};
		if (0 <= rounded_exponent)
		{
			auto const integer_digits{
				static_cast<::std::size_t>(rounded_exponent) + 1u};
			if (virtual_size <= static_cast<::std::size_t>(rounded_exponent))
			{
				fixed_length = integer_digits;
			}
			else
			{
				fixed_length = ::fast_io::details::exact_precision_saturating_add(
					virtual_size, virtual_size == integer_digits ? 1u : 2u);
			}
		}
		else
		{
			fixed_length = ::fast_io::details::exact_precision_saturating_add(
				virtual_size, static_cast<::std::size_t>(-rounded_exponent) + 1u);
		}
		auto const scientific_length{
			::fast_io::details::print_rsv_fp_scientific_length(
				rounded_exponent, virtual_size)};
		fixed = scientific_length >= fixed_length;
	}
	if (fixed)
	{
		return ::fast_io::details::exact_precision_wide_call_fixed(
			iter, decimal, virtual_size, fractional && preserve, precision,
			&::fast_io::details::exact_precision_fixed<
				comma, json_float, char_type, decimal_type>);
	}
	return ::fast_io::details::exact_precision_wide_call_scientific(
		iter, decimal, virtual_size - 1u, preserve,
		&::fast_io::details::exact_precision_scientific<
			flt, comma, uppercase_e, char_type, decimal_type>);
}

// Keep numeric construction outlined as well: a format/rounding instantiation
// contributes only one call site, while the 512-bit proof machinery remains a
// single shared body.  The attribute capability changes placement, not results.
template <typename flt, bool comma, bool uppercase_e, bool json_float,
		  ::std::integral char_type>
#if __has_cpp_attribute(gnu::noipa)
[[gnu::noipa]]
#elif __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *exact_precision_wide_try_print(
	char_type *iter,
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t precision, bool negative,
	::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode,
	::fast_io::manipulators::floating_rounding rounding) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	constexpr ::std::int_least32_t bias{
		(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
	auto significand{static_cast<__uint128_t>(mantissa)};
	::std::int_least32_t binary_exponent{};
	if (exponent)
	{
		significand |= static_cast<__uint128_t>(1u) << trait::mbits;
		binary_exponent = static_cast<::std::int_least32_t>(exponent) -
			bias - static_cast<::std::int_least32_t>(trait::mbits);
	}
	else
	{
		binary_exponent =
			1 - bias - static_cast<::std::int_least32_t>(trait::mbits);
	}
	exact_precision_compact_window_decimal decimal{};
	::std::size_t significant{};
	if (!::fast_io::details::exact_precision_wide_prepare(
			decimal.digits, decimal.size, decimal.exponent, significant,
			significand, trait::mbits, binary_exponent, precision,
			format, precision_mode, rounding, negative))
	{
		return nullptr;
	}
	return ::fast_io::details::exact_precision_wide_runtime_present<
		flt, comma, uppercase_e, json_float>(
			iter, decimal, precision, significant, format, precision_mode);
}
#endif

template <typename flt, bool comma, bool uppercase_e,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  ::fast_io::manipulators::floating_rounding rounding, bool json_float,
		  bool direct_only, bool skip_direct,
		  ::std::integral char_type>
inline constexpr char_type *print_rsvflt_exact_precision_window_impl(
	char_type *iter, typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t precision, bool negative,
	::std::int_least32_t initial_real_exponent) noexcept
{
	static_assert(!(direct_only && skip_direct));
	constexpr bool fractional{::fast_io::details::floating_precision_is_fractional<precision_mode>};
	constexpr bool preserve{::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>};
	constexpr auto int32_max{(::std::numeric_limits<::std::int_least32_t>::max)()};
	auto real_exponent{initial_real_exponent};
	for (unsigned attempt{}; attempt != 2u; ++attempt)
	{
		::std::size_t significant{};
		::std::int_least32_t keep{};
		if constexpr (fractional)
		{
			if constexpr (format == ::fast_io::manipulators::floating_format::scientific)
			{
				significant = ::fast_io::details::exact_precision_saturating_add(precision, 1u);
				if (exact_precision_window_digit_capacity <= significant)
				{
					return nullptr;
				}
				keep = static_cast<::std::int_least32_t>(significant);
			}
			else
			{
				if (precision > static_cast<::std::size_t>(int32_max))
				{
					return nullptr;
				}
				auto const requested_keep{static_cast<::std::int_least64_t>(real_exponent) + 1 +
										  static_cast<::std::int_least64_t>(precision)};
				if (requested_keep < 0 ||
					exact_precision_window_digit_capacity <= static_cast<::std::uint_least64_t>(requested_keep))
				{
					return nullptr;
				}
				keep = static_cast<::std::int_least32_t>(requested_keep);
				significant = static_cast<::std::size_t>(keep);
			}
		}
		else
		{
			significant = precision ? precision : 1u;
			if (exact_precision_window_digit_capacity <= significant)
			{
				return nullptr;
			}
			keep = static_cast<::std::int_least32_t>(significant);
		}
		// The DA precision carrier is admitted only for positive-magnitude finite
		// normal binary64 values under a nearest policy.  Its one-sided ambiguity
		// interval rejects every exact or possible tie, so all six nearest policies
		// agree on a successful carrier.  The fused scientific writer additionally
		// requires preserved requested width.  Subnormals, directed rounding,
		// non-preserving output and every ambiguity failure continue through exact
		// materialization.  This function is itself defined inside the native-u128
		// exact-window block, so no second capability test is required here.
		if constexpr (::std::same_as<flt, double> &&
			format == ::fast_io::manipulators::floating_format::scientific &&
			preserve && ::fast_io::details::floating_rounding_is_nearest<rounding> &&
				!binary64_scientific_precision_outer_dispatch && !skip_direct)
		{
			// AArch64 reaches the shared DA helper after the public caller selects
			// this exact-window placement.  P16-P19 use the contract stated above.
			if (significant - 16u < 4u && exponent)
			{
				auto const fixed_width_result{::fast_io::details::
					print_rsvflt_binary64_scientific_precision_runtime_impl<comma, uppercase_e>(
						iter, static_cast<::std::uint_least64_t>(mantissa), exponent, significant)};
				if (fixed_width_result)
				{
					return fixed_width_result;
				}
			}
		}
		if constexpr (::std::same_as<flt, double> || ::std::same_as<flt, float>)
		{
			auto const requested_digits{static_cast<::std::size_t>(keep) + 1u};
			if (requested_digits <= exact_precision_compact_window_digit_capacity)
			{
				using trait = ::fast_io::details::iec559_traits<flt>;
				auto binary_mantissa{static_cast<::std::uint_least64_t>(mantissa)};
				::std::int_least32_t binary_exponent{};
				if (exponent)
				{
					if constexpr (::std::same_as<flt, double> &&
						!binary64_scientific_wide_precision_direct_block &&
						format == ::fast_io::manipulators::floating_format::scientific &&
						preserve && ::fast_io::details::floating_rounding_is_nearest<rounding> &&
						!skip_direct)
					{
						// Successful P16-P19 values returned at the narrow gate; subnormals
						// cannot enter this normal block.  Clang x86 shares P20-P33 in one
						// runtime body.  Other compilers use constant P20-P24 entries followed
						// by the shared P25-P33 tail, preserving the assembly policy above.
						auto const wide_distance{significant - 20u};
						constexpr ::std::size_t first_interval_size{
							binary64_scientific_wide_precision_runtime_is_extended ? 14u : 5u};
						if (wide_distance < first_interval_size) [[unlikely]]
						{
							auto const fixed_width_result{::fast_io::details::
								print_rsvflt_binary64_scientific_wide_precision_runtime_impl<comma, uppercase_e>(
									iter, binary_mantissa, exponent, significant)};
							if (fixed_width_result)
							{
								return fixed_width_result;
							}
						}
						if constexpr (!binary64_scientific_wide_precision_runtime_is_extended)
						{
							if (wide_distance - 5u < 9u) [[unlikely]]
							{
								auto const fixed_width_result{::fast_io::details::
									print_rsvflt_binary64_scientific_extended_precision_runtime_impl<comma, uppercase_e>(
										iter, binary_mantissa, exponent, significant)};
								if (fixed_width_result)
								{
									return fixed_width_result;
								}
							}
						}
						// Preserve P34's measured constant-width fused writer. P35-P38
						// enter the separate runtime-width dispatcher only after their DA
						// proof has excluded the closed half-boundary ambiguity interval.
						if (significant == 34u) [[unlikely]]
						{
							auto const fixed_width_result{::fast_io::details::
								print_rsvflt_binary64_scientific_p34_precision_impl<
									comma, uppercase_e>(iter, binary_mantissa, exponent)};
							if (fixed_width_result)
							{
								return fixed_width_result;
							}
						}
						else if (significant - 35u < 4u) [[unlikely]]
						{
							auto const fixed_width_result{::fast_io::details::
								print_rsvflt_binary64_p35_p38_precision_dispatch<
									comma, uppercase_e, format, json_float, precision_mode>(
										iter, binary_mantissa, exponent, significant)};
							if (fixed_width_result)
							{
								return fixed_width_result;
							}
						}
					}
					binary_mantissa |= static_cast<::std::uint_least64_t>(1ULL) << trait::mbits;
					constexpr ::std::int_least32_t bias{
						(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
					binary_exponent = static_cast<::std::int_least32_t>(exponent) - bias -
						static_cast<::std::int_least32_t>(trait::mbits);
				}
				else
				{
					binary_exponent = 1 -
						((static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1) -
						static_cast<::std::int_least32_t>(trait::mbits);
				}
				if constexpr (!skip_direct && fractional && preserve &&
					format == ::fast_io::manipulators::floating_format::fixed)
				{
					// The direct exact writer amortizes its binary decomposition only for
					// wide fixed fields.  Keep the established compact-window path for
					// short precision requests, where it has lower front-end cost.
					if (15u <= precision)
					{
						auto const exact_result{::fast_io::details::
							exact_precision_window_try_print_exact_mixed_fixed<comma, json_float, rounding>(
								iter, binary_mantissa, binary_exponent, precision, negative)};
						if (exact_result)
						{
							return exact_result;
						}
					}
				}
				if constexpr (!skip_direct && preserve &&
					format == ::fast_io::manipulators::floating_format::scientific &&
					((::std::same_as<flt, float> && ::fast_io::details::
						exact_precision_window_direct_binary32_negative_scientific) ||
					 (::std::same_as<flt, float> && ::fast_io::details::
						exact_precision_window_direct_binary32_positive_scientific) ||
					 ::fast_io::details::exact_precision_window_direct_negative_scientific ||
					 ::fast_io::details::exact_precision_window_direct_nonnegative_scientific ||
					 ::fast_io::details::exact_precision_window_direct_mixed_scientific ||
					 ::fast_io::details::exact_precision_window_long_nonnegative_scientific ||
					 ::fast_io::details::exact_precision_window_long_mixed_scientific))
				{
					// Wide scientific fields use the same guard/sticky block stream,
					// skipping fractional leading zeroes instead of materializing and
					// copying an intermediate numeric digit window.
					constexpr ::std::size_t direct_scientific_minimum_precision{
						::std::same_as<flt, float> ? 6u : 15u};
					if (direct_scientific_minimum_precision <= precision)
					{
						if constexpr (::std::same_as<flt, double> &&
							binary64_scientific_wide_precision_direct_block &&
							::fast_io::details::floating_rounding_is_nearest<rounding>)
						{
							// Apple AArch64 reuses the direct-scientific branch so P1-P19
							// retain their entry comparisons and live ranges.  This is the
							// placement policy documented by
							// binary64_scientific_wide_precision_direct_block.
							auto const wide_distance{significant - 20u};
							if (exponent && wide_distance < 5u) [[unlikely]]
							{
								auto const fixed_width_result{::fast_io::details::
									print_rsvflt_binary64_scientific_wide_precision_runtime_impl<comma, uppercase_e>(
										iter, static_cast<::std::uint_least64_t>(mantissa), exponent, significant)};
								if (fixed_width_result)
								{
									return fixed_width_result;
								}
							}
							if (exponent && wide_distance - 5u < 9u) [[unlikely]]
							{
								auto const fixed_width_result{::fast_io::details::
									print_rsvflt_binary64_scientific_extended_precision_runtime_impl<comma, uppercase_e>(
										iter, static_cast<::std::uint_least64_t>(mantissa), exponent, significant)};
								if (fixed_width_result)
								{
									return fixed_width_result;
								}
							}
							// Apple AArch64 keeps the P34 equality probe at the same
							// direct-scientific placement as the established wide ranges,
							// preserving its constant fused call shape and the P1-P19 entry.
							if (exponent && significant == 34u) [[unlikely]]
							{
								auto const fixed_width_result{::fast_io::details::
									print_rsvflt_binary64_scientific_p34_precision_impl<
										comma, uppercase_e>(iter,
											static_cast<::std::uint_least64_t>(mantissa), exponent)};
								if (fixed_width_result)
								{
									return fixed_width_result;
								}
							}
							else if (exponent && significant - 35u < 4u) [[unlikely]]
							{
								auto const fixed_width_result{::fast_io::details::
									print_rsvflt_binary64_p35_p38_precision_dispatch<
										comma, uppercase_e, format, json_float, precision_mode>(
											iter, static_cast<::std::uint_least64_t>(mantissa),
											exponent, significant)};
								if (fixed_width_result)
								{
									return fixed_width_result;
								}
							}
						}
						char_type *direct_result{};
						if (0 <= binary_exponent)
						{
							if constexpr (::std::same_as<flt, float> && ::fast_io::details::
								exact_precision_window_direct_binary32_positive_scientific)
							{
								direct_result = ::fast_io::details::
									exact_precision_window_try_print_positive_binary32_scientific<
									flt, comma, uppercase_e, rounding>(iter,
										static_cast<::std::uint_least32_t>(binary_mantissa),
										static_cast<::std::uint_least32_t>(binary_exponent),
										significant, negative);
							}
							else if constexpr (::fast_io::details::
								exact_precision_window_direct_nonnegative_scientific ||
								::fast_io::details::exact_precision_window_long_nonnegative_scientific)
							{
								if (::fast_io::details::exact_precision_window_direct_nonnegative_scientific ||
									35u <= significant)
								{
									direct_result = ::fast_io::details::
										exact_precision_window_try_print_positive_scientific<
											flt, comma, uppercase_e, rounding>(iter, binary_mantissa,
											static_cast<::std::uint_least32_t>(binary_exponent),
											significant, negative);
								}
							}
						}
						else if (real_exponent < 0)
						{
							if constexpr (::std::same_as<flt, double> && ::fast_io::details::
								exact_precision_window_scaled_negative_scientific)
							{
								// The uint128 scaling path is a low-latency specialization for the
								// common decimal interval [1e-4, 1).  Check its complete arithmetic
								// domain here so distant exponents do not pay an out-of-line call.
								if (15u <= precision && significant <= 28u && -4 <= real_exponent)
								{
									auto const scale_distance{
										static_cast<::std::int_least64_t>(significant) - 1 -
										static_cast<::std::int_least64_t>(real_exponent)};
									auto const scaled_binary_exponent{
										static_cast<::std::int_least64_t>(binary_exponent) + scale_distance};
									if (scale_distance <= 28 && -63 <= scaled_binary_exponent &&
										scaled_binary_exponent < 0)
									{
										if (significant <= 19u)
										{
											direct_result = ::fast_io::details::
												exact_precision_window_try_print_scaled_binary64_scientific<
													true, flt, comma, uppercase_e, rounding>(iter,
														binary_mantissa, binary_exponent, real_exponent,
														significant, negative);
										}
										else
										{
											direct_result = ::fast_io::details::
												exact_precision_window_try_print_scaled_binary64_scientific<
													false, flt, comma, uppercase_e, rounding>(iter,
														binary_mantissa, binary_exponent, real_exponent,
														significant, negative);
										}
									}
								}
							}
							if (!direct_result)
							{
								if constexpr ((::std::same_as<flt, float> && ::fast_io::details::
										exact_precision_window_direct_binary32_negative_scientific) ||
									::fast_io::details::exact_precision_window_direct_negative_scientific)
								{
									direct_result = ::fast_io::details::
										exact_precision_window_try_print_negative_scientific<
											flt, comma, uppercase_e, rounding>(iter, binary_mantissa,
											binary_exponent, real_exponent, significant, negative);
								}
							}
						}
						else
						{
							// The integer-prefix setup amortizes from two full fractional
							// blocks onward; shorter mixed fields keep the compact window.
							if constexpr (::fast_io::details::
								exact_precision_window_direct_mixed_scientific ||
								::fast_io::details::exact_precision_window_long_mixed_scientific)
							{
								auto const fractional_binary_bits{
									static_cast<::std::uint_least32_t>(-binary_exponent)};
								auto const fractional_mask{
									(static_cast<::std::uint_least64_t>(1ULL) << fractional_binary_bits) - 1u};
								if (18u <= precision &&
									(::fast_io::details::exact_precision_window_direct_mixed_scientific ||
										35u <= significant) &&
									(binary_mantissa & fractional_mask))
								{
									direct_result = ::fast_io::details::
										exact_precision_window_try_print_mixed_scientific<
											flt, comma, uppercase_e, rounding>(iter, binary_mantissa,
											binary_exponent, significant, negative);
								}
							}
						}
						if (direct_result)
						{
							return direct_result;
						}
					}
				}
				if constexpr (direct_only)
				{
					return nullptr;
				}
				exact_precision_compact_window_result generated;
				bool generated_success{};
				if (binary_exponent < 0 && real_exponent < 0)
				{
					if constexpr (::std::same_as<flt, float>)
					{
						generated_success = ::fast_io::details::
							exact_precision_window_materialize_negative_binary_core(
								generated, binary_mantissa, binary_exponent, requested_digits,
								real_exponent);
					}
					else
					{
						generated_success = ::fast_io::details::
							exact_precision_window_materialize_negative_binary64<
								::fast_io::details::binary64_p18_p19_materializer_enabled &&
								::fast_io::details::floating_rounding_is_nearest<rounding> &&
								!preserve && !fractional>(
								generated, binary_mantissa, binary_exponent, requested_digits,
								real_exponent);
					}
				}
				else if (binary_exponent < 0)
				{
					generated_success = ::fast_io::details::
						exact_precision_window_materialize_mixed_binary<
							::std::same_as<flt, double> &&
							::fast_io::details::binary64_p18_p19_materializer_enabled &&
							::fast_io::details::floating_rounding_is_nearest<rounding> &&
							!preserve && !fractional>(
							generated, binary_mantissa, binary_exponent, requested_digits);
				}
				else
				{
					generated_success =
						::fast_io::details::exact_precision_window_materialize_positive_binary64<
							::std::same_as<flt, double> &&
							::fast_io::details::binary64_p18_p19_materializer_enabled &&
							::fast_io::details::floating_rounding_is_nearest<rounding> &&
							!preserve && !fractional>(
							generated, binary_mantissa,
							static_cast<::std::uint_least32_t>(binary_exponent), requested_digits);
				}
				if (generated_success)
				{
					if (generated.real_exponent != real_exponent)
					{
						real_exponent = generated.real_exponent;
						continue;
					}
					if (!generated.tail_nonzero)
					{
						::fast_io::details::exact_precision_trim(generated.decimal);
					}
					auto const rounded{
						static_cast<::std::int_least32_t>(generated.decimal.size) > keep};
					if (rounded)
					{
						::fast_io::details::exact_precision_window_round<rounding>(
							generated.decimal, keep, negative, generated.tail_nonzero);
					}
					if constexpr (!preserve)
					{
						::fast_io::details::exact_precision_trim(generated.decimal);
					}
					if constexpr (fractional && preserve &&
						format == ::fast_io::manipulators::floating_format::general)
					{
						significant = rounded
							? ::fast_io::details::
								exact_precision_fractional_general_rounded_virtual_size(
									generated.decimal, precision)
							: generated.decimal.size;
					}
					return ::fast_io::details::print_rsvflt_rounded_precision_define_impl<
						flt, comma, uppercase_e, format, precision_mode, json_float>(
							iter, generated.decimal, precision, significant);
				}
			}
		}
		if constexpr (direct_only)
		{
			return nullptr;
		}
		auto generated{::fast_io::details::exact_precision_window_from_binary<flt>(
			mantissa, exponent, static_cast<::std::size_t>(keep) + 1u, real_exponent)};
		if (!generated.success)
		{
			return nullptr;
		}
		if (generated.real_exponent != real_exponent)
		{
			real_exponent = generated.real_exponent;
			continue;
		}
		if (!generated.tail_nonzero)
		{
			::fast_io::details::exact_precision_trim(generated.decimal);
		}
		auto const rounded{
			static_cast<::std::int_least32_t>(generated.decimal.size) > keep};
		if (rounded)
		{
			::fast_io::details::exact_precision_window_round<rounding>(
				generated.decimal, keep, negative, generated.tail_nonzero);
		}
		if constexpr (!preserve)
		{
			::fast_io::details::exact_precision_trim(generated.decimal);
		}
		if constexpr (fractional && preserve &&
			format == ::fast_io::manipulators::floating_format::general)
		{
			significant = rounded
				? ::fast_io::details::
					exact_precision_fractional_general_rounded_virtual_size(
						generated.decimal, precision)
				: generated.decimal.size;
		}
		return ::fast_io::details::print_rsvflt_rounded_precision_define_impl<
			flt, comma, uppercase_e, format, precision_mode, json_float>(
				iter, generated.decimal, precision, significant);
	}
	return nullptr;
}

template <typename flt, bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode,
	::fast_io::manipulators::floating_rounding rounding, bool json_float,
	bool direct_only, bool skip_direct, ::std::integral char_type>
inline constexpr char_type *print_rsvflt_exact_precision_window_dispatch_impl(
	char_type *iter, typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t precision, bool negative,
	::std::int_least32_t initial_real_exponent) noexcept
{
	if constexpr (binary64_scientific_precision_outer_dispatch &&
		::std::same_as<flt, double> &&
		format == ::fast_io::manipulators::floating_format::scientific &&
		::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode> &&
		::fast_io::details::floating_rounding_is_nearest<rounding> && !skip_direct)
	{
		auto const significant{
			::fast_io::details::floating_precision_is_fractional<precision_mode>
				? ::fast_io::details::exact_precision_saturating_add(precision, 1u)
				: (precision ? precision : 1u)};
		// Normal P16-P19 values enter the one shared x86-64 DA boundary before
		// the exact-window prologue.  Subnormal and adjacent precision cases call
		// the established implementation directly.
		if (exponent && significant - 16u < 4u)
		{
			auto const fixed_width_result{::fast_io::details::
				print_rsvflt_binary64_scientific_precision_runtime_impl<comma, uppercase_e>(
					iter, static_cast<::std::uint_least64_t>(mantissa), exponent, significant)};
			if (fixed_width_result)
			{
				return fixed_width_result;
			}
		}
	}
	return ::fast_io::details::print_rsvflt_exact_precision_window_impl<
		flt, comma, uppercase_e, format, precision_mode, rounding, json_float,
		direct_only, skip_direct>(iter, mantissa, exponent, precision, negative,
		initial_real_exponent);
}
#endif

// Arithmetic/presentation body shared by the ordinary outlined fallback and
// print/concat's optimizer-proven constant proxy.  Keeping placement attributes
// on the wrapper below lets the ordinary dynamic path retain its established
// frame while the selected constant arm can expose every integer operation to
// constant propagation.  Both callers therefore use one rounding algorithm.
template <typename flt, bool comma, bool uppercase_e,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  ::fast_io::manipulators::floating_rounding rounding, bool json_float,
		  ::std::integral char_type>
inline constexpr char_type *
print_rsvflt_exact_precision_body_impl(
	char_type *iter, typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t precision, bool negative) noexcept
{
	constexpr bool fractional{::fast_io::details::floating_precision_is_fractional<precision_mode>};
	constexpr bool preserve{::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>};
	constexpr auto int32_max{(::std::numeric_limits<::std::int_least32_t>::max)()};
	auto decimal{::fast_io::details::exact_precision_from_binary<flt>(mantissa, exponent)};
	auto const real_exponent{
		decimal.exponent + static_cast<::std::int_least32_t>(decimal.size) - 1};
	::std::size_t significant{};
	::std::int_least32_t keep{};
	if constexpr (fractional)
	{
		if constexpr (format == ::fast_io::manipulators::floating_format::scientific)
		{
			significant = ::fast_io::details::exact_precision_saturating_add(precision, 1u);
			keep = significant > static_cast<::std::size_t>(int32_max) ? int32_max : static_cast<::std::int_least32_t>(significant);
		}
		else if (precision > static_cast<::std::size_t>(int32_max))
		{
			keep = int32_max;
			significant = static_cast<::std::size_t>(keep);
		}
		else
		{
			auto const requested_keep{static_cast<::std::int_least64_t>(real_exponent) + 1 +
									  static_cast<::std::int_least64_t>(precision)};
			keep = int32_max < requested_keep ? int32_max : static_cast<::std::int_least32_t>(requested_keep);
			significant = keep < 0 ? 0u : static_cast<::std::size_t>(keep);
		}
	}
	else
	{
		significant = precision ? precision : 1u;
		keep = significant > static_cast<::std::size_t>(int32_max) ? int32_max : static_cast<::std::int_least32_t>(significant);
	}
	auto const rounded{static_cast<::std::int_least32_t>(decimal.size) > keep};
	::fast_io::details::exact_precision_round<rounding>(decimal, keep, negative);
	if constexpr (!preserve)
	{
		::fast_io::details::exact_precision_trim(decimal);
	}
	if constexpr (fractional && preserve &&
		format == ::fast_io::manipulators::floating_format::general)
	{
		significant = rounded
			? ::fast_io::details::
				exact_precision_fractional_general_rounded_virtual_size(
					decimal, precision)
			: decimal.size;
	}
	return ::fast_io::details::print_rsvflt_rounded_precision_define_impl<
		flt, comma, uppercase_e, format, precision_mode, json_float>(
		iter, decimal, precision, significant);
}

// This entry is the full decimal-expansion fallback after compact/direct
// precision attempts have declined the value.  `cold` follows that control-flow
// domain and is not a measured frequency assertion for every input corpus;
// `noinline` prevents the expansion object and rounding state from enlarging the
// common dynamic caller.  The shared body above owns every numeric decision.
template <typename flt, bool comma, bool uppercase_e,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  ::fast_io::manipulators::floating_rounding rounding, bool json_float,
		  ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *print_rsvflt_exact_precision_define_impl(
	char_type *iter,
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t precision,
	bool negative) noexcept
{
	return ::fast_io::details::print_rsvflt_exact_precision_body_impl<
		flt, comma, uppercase_e, format, precision_mode, rounding, json_float>(
			iter, mantissa, exponent, precision, negative);
}

template <typename flt, ::fast_io::manipulators::floating_rounding rounding, bool preserve_trailing_zero = false>
inline constexpr void print_rsv_fp_round_to_significant(
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> &m10, ::std::int_least32_t &e10,
	::std::size_t precision,
	bool negative) noexcept
{
	using mantissa_type = ::fast_io::details::dragonbox_decimal_mantissa_type<flt>;
	if (!m10)
	{
		return;
	}
	if (!precision)
	{
		precision = 1u;
	}
	auto len{static_cast<::std::uint_least32_t>(chars_len<10, true>(m10))};
	if (precision < len)
	{
		auto const cut{static_cast<::std::uint_least32_t>(len - precision)};
		if (cut < 20u)
		{
			auto const divisor{::fast_io::details::print_rsv_fp_pow10_0_to_19_table[cut]};
			auto quotient{static_cast<::std::uint_least64_t>(m10 / divisor)};
			auto const remainder{static_cast<::std::uint_least64_t>(m10 - static_cast<mantissa_type>(quotient * divisor))};
			if (::fast_io::details::print_rsv_fp_decimal_round_up<rounding>(negative, quotient, remainder, divisor))
			{
				++quotient;
			}
			m10 = static_cast<mantissa_type>(quotient);
			e10 += static_cast<::std::int_least32_t>(cut);
			if (precision < 20u &&
				quotient == ::fast_io::details::print_rsv_fp_pow10_0_to_19_table[precision])
			{
				m10 = static_cast<mantissa_type>(quotient / 10u);
				++e10;
			}
		}
		return;
	}
	if constexpr (!preserve_trailing_zero)
	{
		return;
	}
	auto const carrier_precision{precision < ::fast_io::details::iec559_traits<flt>::m10digits
									 ? precision
									 : ::fast_io::details::iec559_traits<flt>::m10digits};
	auto const pad{static_cast<::std::uint_least32_t>(carrier_precision - len)};
	if (pad && pad < 20u)
	{
		auto const multiplier{::fast_io::details::print_rsv_fp_pow10_0_to_19_table[pad]};
		auto const next{static_cast<mantissa_type>(m10 * multiplier)};
		if (next / multiplier == m10)
		{
			m10 = next;
			e10 -= static_cast<::std::int_least32_t>(pad);
		}
	}
}

template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
inline constexpr void print_rsv_fp_round_to_fractional(
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> &m10, ::std::int_least32_t &e10,
	::std::size_t precision,
	bool negative) noexcept
{
	using mantissa_type = ::fast_io::details::dragonbox_decimal_mantissa_type<flt>;
	if (!m10)
	{
		return;
	}
	if (0 <= e10 ||
		precision >= static_cast<::std::size_t>(
						 -static_cast<::std::int_least64_t>(e10)))
	{
		/*
		The requested quantum 10^-P is no coarser than the carrier exactly when
		-P<=e10.  For nonnegative e10 this is immediate; otherwise it is the
		unsigned comparison P>=-int64(e10).  This single mathematical guard also
		proves that the fallthrough magnitude is representable by int_least32_t,
		so the following narrowing cannot wrap even at the minimum exponent.  It
		replaces, rather than adds to, the ordinary hot-path exponent comparison.
		*/
		return;
	}
	auto const target_e10{-static_cast<::std::int_least32_t>(precision)};
	auto const cut{static_cast<::std::uint_least32_t>(target_e10 - e10)};
	if (20u <= cut)
	{
		if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
		{
			m10 = 0u;
		}
		else
		{
			m10 = static_cast<mantissa_type>(
				::fast_io::details::floating_rounding_directed_round_up<rounding>(negative));
		}
		e10 = target_e10;
		return;
	}
	auto const divisor{::fast_io::details::print_rsv_fp_pow10_0_to_19_table[cut]};
	auto quotient{static_cast<::std::uint_least64_t>(m10 / divisor)};
	auto const remainder{static_cast<::std::uint_least64_t>(m10 - static_cast<mantissa_type>(quotient * divisor))};
	if (::fast_io::details::print_rsv_fp_decimal_round_up<rounding>(negative, quotient, remainder, divisor))
	{
		++quotient;
	}
	m10 = static_cast<mantissa_type>(quotient);
	e10 = target_e10;
}

template <typename mantissa_type>
inline constexpr void print_rsv_fp_trim_trailing_zero(mantissa_type &m10, ::std::int_least32_t &e10) noexcept
{
	if (!m10)
	{
		// Every pair (0, e10) denotes the same real number.  Canonicalize it to
		// (0, 0) so a non-preserving fixed/general renderer cannot manufacture a
		// fractional zero from the stale quantum left by a coarse rounding step.
		// Preserving modes do not call this trim operation; their dedicated
		// writers retain the requested number of trailing zeroes explicitly.
		e10 = 0;
		return;
	}
	for (; m10 % 10u == 0u;)
	{
		m10 /= 10u;
		++e10;
	}
}

template <bool comma, ::std::integral char_type>
inline constexpr char_type *print_rsv_fp_append_point_zeros(char_type *iter, ::std::size_t precision) noexcept
{
	if (!precision)
	{
		return iter;
	}
	*iter = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	++iter;
	return ::fast_io::details::fill_zeros_impl(iter, precision);
}

// This terminal wrapper belongs to the native-u128 exact-block domain: its
// integer materializer uses a 64-by-128 multiply/shift primitive and is not
// declared by the portable fallback.  Matching the wrapper's availability to
// that primitive prevents an otherwise dead template from naming an unavailable
// function when __SIZEOF_INT128__ is deliberately disabled.  The only call site
// is under the same gate, so this is a declaration-domain constraint and emits
// no additional branch on supported targets.
#if defined(__SIZEOF_INT128__)
// A finite binary64 entering this helper has its implicit bit restored and a
// final binary exponent in [0, 971].  Consequently index <= 61 < table_size,
// and the represented positive integer has at least one nonzero decimal block;
// the underlying integer writer therefore cannot return null.  Keep the block
// stream and zero padding behind one terminal call so the selected GCC majors
// do not extend the caller's table cursor and output state across the suffix.
// The optional assume attributes communicate the proved bounds only.  A
// compiler without that C++23 attribute executes the same calls and writes.
template <bool comma, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *exact_precision_window_print_positive_binary64_fixed(
	char_type *iter, ::std::uint_least64_t mantissa,
	::std::uint_least32_t binary_exponent, ::std::size_t precision) noexcept
{
	FAST_IO_ASSUME(binary_exponent <= 971u);
	FAST_IO_ASSUME(mantissa != 0u);
	auto const integer_end{::fast_io::details::
		exact_precision_window_print_positive_binary64_integer(
			iter, mantissa, binary_exponent)};
	FAST_IO_ASSUME(integer_end != nullptr);
	return ::fast_io::details::print_rsv_fp_append_point_zeros<comma>(
		integer_end, precision);
}
#endif

/// Selects only the known-length integer digit writer used by fixed precision layout.
/// Both policies consume the same mantissa/length and write the same code units. `optimized` preserves the platform's
/// established jeaiii placement; `positional` uses division from the final position, which an enclosing constant-valued
/// caller can reduce to immediate stores. The layout, rounding, punctuation, padding and endpoint logic are shared.
/// A focused GCC 15 architecture A/B kept the target caller at 0x6f/zero calls,
/// while duplicating this complete layout in the constant layer increased total
/// text from 81,733 to 84,413 bytes and compile time from 4.67 s to 10.55 s.
/// The default `optimized` specialization is the pre-change expression and its
/// GCC 13/15 and Clang 23 run-time normalized instruction hashes are identical.
enum class floating_fixed_precision_digit_writer : unsigned char
{
	optimized,
	positional
};

template <typename flt, bool comma, bool json_float = false,
	floating_fixed_precision_digit_writer digit_writer =
		floating_fixed_precision_digit_writer::optimized,
	::std::integral char_type>
inline constexpr char_type *print_rsv_fp_fixed_precision_impl(char_type *iter,
															  ::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10,
															  ::std::int_least32_t e10,
															  ::std::size_t precision) noexcept
{
	auto const print_digits{[] (char_type *destination,
		::fast_io::details::dragonbox_decimal_mantissa_type<flt> value,
		::std::uint_least32_t length) constexpr noexcept {
		if constexpr (
			digit_writer == floating_fixed_precision_digit_writer::positional)
		{
			for (auto index{length}; index != 0u; value /= 10u)
			{
				destination[--index] =
					::fast_io::char_literal_add<char_type>(value % 10u);
			}
		}
		else
		{
			::fast_io::details::print_rsv_fp_digits_len<flt>(
				destination, value, length);
		}
	}};
	if (!m10)
	{
		*iter = char_literal_v<u8'0', char_type>;
		++iter;
		if constexpr (json_float)
		{
			if (!precision)
			{
				return ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(iter);
			}
		}
		return ::fast_io::details::print_rsv_fp_append_point_zeros<comma>(iter, precision);
	}
	auto const olength{static_cast<::std::int_least32_t>(chars_len<10, true>(m10))};
	auto const real_exp{static_cast<::std::int_least32_t>(e10 + olength - 1)};
	if (0 <= real_exp)
	{
		auto const integer_digits{static_cast<::std::int_least32_t>(real_exp + 1)};
		if (olength <= integer_digits)
		{
			print_digits(iter, m10, static_cast<::std::uint_least32_t>(olength));
			iter += olength;
			iter = ::fast_io::details::fill_zeros_impl(
				iter, static_cast<::std::size_t>(integer_digits - olength));
			if constexpr (json_float)
			{
				if (!precision)
				{
					return ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(iter);
				}
			}
			return ::fast_io::details::print_rsv_fp_append_point_zeros<comma>(iter, precision);
		}
		auto tmp{iter};
		print_digits(iter + 1, m10,
			static_cast<::std::uint_least32_t>(olength));
		iter += olength + 1;
		::fast_io::details::my_copy_n(tmp + 1, static_cast<::std::size_t>(integer_digits), tmp);
		tmp[integer_digits] = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
		auto const fractional_digits{static_cast<::std::size_t>(olength - integer_digits)};
		if (fractional_digits < precision)
		{
			iter = ::fast_io::details::fill_zeros_impl(iter, precision - fractional_digits);
		}
		return iter;
	}
	*iter = char_literal_v<u8'0', char_type>;
	++iter;
	if (!precision)
	{
		return iter;
	}
	*iter = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
	++iter;
	auto const leading_zeroes{static_cast<::std::size_t>(-real_exp - 1)};
	if (leading_zeroes < precision)
	{
		iter = ::fast_io::details::fill_zeros_impl(iter, leading_zeroes);
		print_digits(iter, m10, static_cast<::std::uint_least32_t>(olength));
		iter += olength;
		auto const fractional_digits{leading_zeroes + static_cast<::std::size_t>(olength)};
		if (fractional_digits < precision)
		{
			iter = ::fast_io::details::fill_zeros_impl(iter, precision - fractional_digits);
		}
	}
	else
	{
		iter = ::fast_io::details::fill_zeros_impl(iter, precision);
	}
	return iter;
}

template <typename flt, bool comma, bool uppercase_e, bool preserve_trailing_zero = false, ::std::integral char_type>
inline constexpr char_type *print_rsv_fp_scientific_precision_impl(
	char_type *iter, ::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10, ::std::int_least32_t e10,
	::std::size_t precision) noexcept
{
	auto const olength{static_cast<::std::int_least32_t>(chars_len<10, true>(m10))};
	auto const real_exp{static_cast<::std::int_least32_t>(e10 + olength - 1)};
	auto itp1{iter + 1};
	::fast_io::details::print_rsv_fp_digits_len<flt>(itp1, m10, static_cast<::std::uint_least32_t>(olength));
	*iter = *itp1;
	if (precision)
	{
		auto const available{static_cast<::std::size_t>(olength - 1)};
		if constexpr (preserve_trailing_zero)
		{
			*itp1 = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
			iter = itp1 + olength;
			if (available < precision)
			{
				iter = ::fast_io::details::fill_zeros_impl(iter, precision - available);
			}
		}
		else
		{
			auto used{available < precision ? available : precision};
			for (; used && itp1[used] == char_literal_v<u8'0', char_type>; --used)
			{
			}
			if (used)
			{
				*itp1 = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
				iter = itp1 + used + 1u;
			}
			else
			{
				++iter;
			}
		}
	}
	else
	{
		++iter;
	}
	return ::fast_io::details::print_rsv_fp_e_impl<flt, uppercase_e>(iter, real_exp);
}

template <typename flt, bool comma, bool uppercase_e, ::fast_io::manipulators::floating_format mt,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  ::fast_io::manipulators::floating_rounding rounding, bool json_float = false,
		  ::std::integral char_type>
inline constexpr char_type *print_rsv_fp_precision_decision_impl(
	char_type *iter, ::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10, ::std::int_least32_t e10,
	::std::size_t precision, bool negative) noexcept
{
	constexpr bool uses_significant_precision{
		::fast_io::details::floating_precision_is_significant<precision_mode>};
	constexpr bool uses_fractional_precision{
		::fast_io::details::floating_precision_is_fractional<precision_mode>};
	constexpr bool preserve_trailing_zero{
		::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>};
	if constexpr (mt == ::fast_io::manipulators::floating_format::scientific)
	{
		// Fractional scientific precision P retains P+1 significant digits.
		// Saturation represents every larger-than-addressable request as an
		// unbounded digit budget; non-preserving output therefore keeps every
		// available exact digit, while preserving output is rejected by the
		// precise-reserve layout addition if its requested width cannot fit.
		auto significant_precision{
			::fast_io::details::exact_precision_saturating_add(precision, 1u)};
		if constexpr (uses_significant_precision)
		{
			significant_precision = precision ? precision : 1u;
			precision = significant_precision - 1u;
		}
		::fast_io::details::print_rsv_fp_round_to_significant<flt, rounding, preserve_trailing_zero>(
			m10, e10, significant_precision, negative);
		if constexpr (!preserve_trailing_zero)
		{
			::fast_io::details::print_rsv_fp_trim_trailing_zero(m10, e10);
		}
		return ::fast_io::details::print_rsv_fp_scientific_precision_impl<flt, comma, uppercase_e,
																		  preserve_trailing_zero>(
			iter, m10, e10, precision);
	}
	else if constexpr (uses_fractional_precision)
	{
		::fast_io::details::print_rsv_fp_round_to_fractional<flt, rounding>(m10, e10, precision, negative);
		if constexpr (!preserve_trailing_zero)
		{
			::fast_io::details::print_rsv_fp_trim_trailing_zero(m10, e10);
		}
		if constexpr (mt == ::fast_io::manipulators::floating_format::general)
		{
			if constexpr (preserve_trailing_zero)
			{
				// General chooses notation from the rounded coefficient exponent.  When
				// that choice is fixed, fractional preserving mode must retain the
				// requested 10^-P quantum; print_rsv_fp_fixed_decision_impl cannot do so
				// because the compact carrier contains no synthetic suffix zeroes.  The
				// precision writer appends exactly those missing zeroes.  Scientific
				// layout instead preserves the meaningful carrier digits.  Whether it
				// needs a synthetic terminal zero depends on exact tail/rounding state,
				// which is deliberately owned by the exact-window caller rather than this
				// scalar renderer.
				// This also proves the zero boundary: e10=-P selects fixed for P<=4 and
				// scientific for P>=5, matching the general rule used by the exact path.
				if (-5 < e10 && e10 < 7)
				{
					return ::fast_io::details::print_rsv_fp_fixed_precision_impl<
						flt, comma, json_float>(iter, m10, e10, precision);
				}
				return ::fast_io::details::print_rsv_fp_decision_impl<
					flt, comma, uppercase_e,
					::fast_io::manipulators::floating_format::scientific, false>(
						iter, m10, e10);
			}
			else
			{
				return ::fast_io::details::print_rsv_fp_decision_impl<
					flt, comma, uppercase_e,
					::fast_io::manipulators::floating_format::general, json_float>(
						iter, m10, e10);
			}
		}
		else
		{
			if constexpr (preserve_trailing_zero)
			{
				return ::fast_io::details::print_rsv_fp_fixed_precision_impl<flt, comma, json_float>(
					iter, m10, e10, precision);
			}
			else
			{
				return ::fast_io::details::print_rsv_fp_fixed_decision_impl<flt, comma, json_float>(
					iter, m10, e10);
			}
		}
	}
	else
	{
		::fast_io::details::print_rsv_fp_round_to_significant<flt, rounding, preserve_trailing_zero>(
			m10, e10, precision, negative);
		if constexpr (!preserve_trailing_zero)
		{
			::fast_io::details::print_rsv_fp_trim_trailing_zero(m10, e10);
		}
		if constexpr (mt == ::fast_io::manipulators::floating_format::fixed)
		{
			return ::fast_io::details::print_rsv_fp_fixed_decision_impl<flt, comma, json_float>(iter, m10, e10);
		}
		else if constexpr (
			mt == ::fast_io::manipulators::floating_format::general &&
			precision_mode ==
				::fast_io::manipulators::floating_precision::
					charconv_significant)
		{
			/*
			The standard general-with-precision decision is made after decimal
			rounding.  If L is the final coefficient length, its scientific
			exponent is X=e10+L-1.  P=0 is specified as P=1.  The two comparisons
			below are exactly -4<=X && X<P; spelling them in signed/unsigned
			pieces also proves that a very large size_t precision cannot narrow.
			*/
			auto const length{static_cast<::std::int_least32_t>(
				chars_len<10, true>(m10))};
			auto const scientific_exponent{
				static_cast<::std::int_least32_t>(e10 + length - 1)};
			auto const significant_precision{precision ? precision : 1u};
			if (-4 <= scientific_exponent &&
				(scientific_exponent < 0 ||
				 static_cast<::std::size_t>(scientific_exponent) <
					 significant_precision))
			{
				return ::fast_io::details::
					print_rsv_fp_fixed_decision_impl<
						flt, comma, json_float>(iter, m10, e10);
			}
			return ::fast_io::details::print_rsv_fp_decision_impl<
				flt, comma, uppercase_e,
				::fast_io::manipulators::floating_format::scientific,
				false>(iter, m10, e10);
		}
		else
		{
			return ::fast_io::details::print_rsv_fp_decision_impl<flt, comma, uppercase_e, mt, json_float>(
				iter, m10, e10);
		}
	}
}

template <typename flt, bool comma, bool uppercase_e, ::fast_io::manipulators::floating_format mt,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  ::fast_io::manipulators::floating_rounding rounding, bool json_float = false,
		  ::std::integral char_type>
inline constexpr char_type *print_rsv_fp_try_directed_carrier_decision_impl(
	char_type *iter, ::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10,
	::std::int_least32_t e10, ::std::size_t precision, bool negative,
	::std::uint_least32_t cut) noexcept
{
	static_assert(!::fast_io::details::floating_rounding_is_nearest<rounding>);
	using mantissa_type = ::fast_io::details::dragonbox_decimal_mantissa_type<flt>;
	constexpr bool uses_significant_precision{
		::fast_io::details::floating_precision_is_significant<precision_mode>};
	constexpr bool uses_fractional_precision{
		::fast_io::details::floating_precision_is_fractional<precision_mode>};
	constexpr bool preserve_trailing_zero{
		::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>};
	auto const divisor{::fast_io::details::print_rsv_fp_pow10_0_to_19_table[cut]};
	auto quotient{static_cast<::std::uint_least64_t>(m10 / divisor)};
	auto const remainder{
		static_cast<::std::uint_least64_t>(m10 - static_cast<mantissa_type>(quotient * divisor))};
	if (!remainder)
	{
		return nullptr;
	}
	if (::fast_io::details::print_rsv_fp_decimal_round_up<rounding>(
			negative, quotient, remainder, divisor))
	{
		++quotient;
	}
	m10 = static_cast<mantissa_type>(quotient);
	e10 += static_cast<::std::int_least32_t>(cut);
	if constexpr (!uses_fractional_precision ||
				  mt == ::fast_io::manipulators::floating_format::scientific)
	{
		auto significant_precision{precision + 1u};
		if constexpr (uses_significant_precision)
		{
			significant_precision = precision ? precision : 1u;
		}
		if (significant_precision < 20u &&
			quotient == ::fast_io::details::print_rsv_fp_pow10_0_to_19_table[significant_precision])
		{
			m10 = static_cast<mantissa_type>(quotient / 10u);
			++e10;
		}
	}
	if constexpr (!preserve_trailing_zero)
	{
		::fast_io::details::print_rsv_fp_trim_trailing_zero(m10, e10);
	}
	if constexpr (mt == ::fast_io::manipulators::floating_format::scientific)
	{
		if constexpr (uses_significant_precision)
		{
			auto const significant_precision{precision ? precision : 1u};
			precision = significant_precision - 1u;
		}
		return ::fast_io::details::print_rsv_fp_scientific_precision_impl<
			flt, comma, uppercase_e, preserve_trailing_zero>(iter, m10, e10, precision);
	}
	else if constexpr (uses_fractional_precision)
	{
		if constexpr (preserve_trailing_zero)
		{
			return ::fast_io::details::print_rsv_fp_fixed_precision_impl<flt, comma, json_float>(
				iter, m10, e10, precision);
		}
		else
		{
			return ::fast_io::details::print_rsv_fp_fixed_decision_impl<flt, comma, json_float>(
				iter, m10, e10);
		}
	}
	else if constexpr (mt == ::fast_io::manipulators::floating_format::fixed)
	{
		return ::fast_io::details::print_rsv_fp_fixed_decision_impl<flt, comma, json_float>(
			iter, m10, e10);
	}
	else if constexpr (
		mt == ::fast_io::manipulators::floating_format::general &&
		precision_mode ==
			::fast_io::manipulators::floating_precision::
				charconv_significant)
	{
		auto const length{static_cast<::std::int_least32_t>(
			chars_len<10, true>(m10))};
		auto const scientific_exponent{
			static_cast<::std::int_least32_t>(e10 + length - 1)};
		auto const significant_precision{precision ? precision : 1u};
		if (-4 <= scientific_exponent &&
			(scientific_exponent < 0 ||
			 static_cast<::std::size_t>(scientific_exponent) <
				 significant_precision))
		{
			return ::fast_io::details::print_rsv_fp_fixed_decision_impl<
				flt, comma, json_float>(iter, m10, e10);
		}
		return ::fast_io::details::print_rsv_fp_decision_impl<
			flt, comma, uppercase_e,
			::fast_io::manipulators::floating_format::scientific, false>(
				iter, m10, e10);
	}
	else
	{
		return ::fast_io::details::print_rsv_fp_decision_impl<flt, comma, uppercase_e, mt, json_float>(
			iter, m10, e10);
	}
}

template <typename flt, bool comma, bool uppercase_e, ::fast_io::manipulators::floating_format mt,
		  bool json_float, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_rsvflt_decimal_define_impl(
	char_type *iter, ::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10,
	::std::int_least32_t e10) noexcept
{
	// The fixed specialization is the common terminal materializer reached by the
	// generic fallback and by the finalize-only regular-miss leaf.  Both callers
	// supply the same finalized (m10, e10) pair, so converging here shares the
	// complete generic fixed renderer without changing presentation semantics.
	if constexpr (mt == ::fast_io::manipulators::floating_format::fixed)
	{
		return ::fast_io::details::print_rsv_fp_fixed_decision_impl<flt, comma, json_float>(iter, m10, e10);
	}
	else
	{
		return ::fast_io::details::print_rsv_fp_decision_impl<flt, comma, uppercase_e, mt, json_float>(
			iter, m10, e10);
	}
}

template <typename flt, bool comma, bool uppercase_e, ::fast_io::manipulators::floating_format mt,
		  bool json_float, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_rsvflt_decimal_with_length_define_impl(
	char_type *iter, ::fast_io::details::dragonbox_decimal_mantissa_type<flt> m10,
	::std::int_least32_t e10, ::std::uint_least32_t length) noexcept
{
	if constexpr (mt == ::fast_io::manipulators::floating_format::fixed)
	{
		return ::fast_io::details::print_rsv_fp_fixed_decision_with_length_impl<flt, comma, json_float>(
			iter, m10, e10, static_cast<::std::int_least32_t>(length));
	}
	else
	{
		return ::fast_io::details::print_rsv_fp_decision_with_length_impl<
			flt, comma, uppercase_e, mt, json_float>(
			iter, m10, e10, static_cast<::std::int_least32_t>(length));
	}
}

/*
ASCII binary16/bfloat16 default-decimal renderer
================================================

`decimal_layout` is generated from exactly the length comparison in
print_rsv_fp_decision_with_length_impl:

* zero means the scientific spelling is strictly shorter;
* one, two, and three mean fixed wins (including equality), partitioned by
  L<=X, 0<=X<L, and X<0, where X=e10+L-1.

These predicates are mutually exclusive and exhaustive.  Each switch arm below
therefore calls the same terminal leaf that the generic decision tree would
reach with the same `(m10,e10,L)`.  Moving those comparisons to table generation
does not select a new spelling; it partially evaluates a pure function of the
stored carrier.  Scientific exponent X is reconstructed by the defining
identity above.  This proves byte-for-byte equivalence with the character-
generic renderer, including the single-digit no-point case and the fixed/
scientific equal-length tie in favour of fixed.

This leaf is admitted only for ordinary `char` reserve output on an ASCII
execution set.  The fixed-width integer writer may initialize reserve scratch
past the logical end, so exact-bounds callers and every other character set keep
the generic renderer.  No character bytes are stored in the 96-KiB table.
*/
template <typename flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr char *
print_rsvflt_narrow_ascii_decimal(
	char *iter,
	::fast_io::details::dragonbox_narrow_shortest_result<flt> decimal) noexcept
{
	auto const length{
		static_cast<::std::int_least32_t>(decimal.length)};
	auto const real_exponent{
		static_cast<::std::int_least32_t>(
			decimal.e10 + length - 1)};
	/*
	ascii_bcd8 returns the eight decimal digits of m10, padded on the left
	with zeroes, in destination byte order.  Because 1<=L<=5, shifting the
	little-endian destination word by 8*(8-L) discards exactly the padding and
	leaves the L significant bytes at offsets [0,L).  Every later 64-bit store
	is permitted by the ordinary reserve contract; the returned pointer still
	marks only the logical spelling.
	*/
	auto const padded_digits{
		::fast_io::details::da::ascii_bcd8(decimal.m10) +
		::fast_io::details::da::ascii_zeroes};
	auto const digits{
		padded_digits >>
		static_cast<unsigned>(
			(8 - length) * 8)};
	auto const store_word =
		[](char *destination,
		   ::std::uint_least64_t word) constexpr noexcept
		{
			::fast_io::freestanding::my_memcpy(
				destination, __builtin_addressof(word),
				sizeof(word));
		};
	switch (decimal.decimal_layout)
	{
	case 0u:
		if (length == 1)
		{
			store_word(iter, digits);
			++iter;
		}
		else
		{
			/*
			`digits & 0xff` is the leading digit.  Moving every remaining
			byte left by one address and inserting '.' at byte one is exactly
			d.ddd; the fields are disjoint, so bitwise OR is lossless.
			*/
			auto const assembled{
				(digits &
				 static_cast<::std::uint_least64_t>(0xffu)) |
				(static_cast<::std::uint_least64_t>(u8'.')
				 << 8u) |
				((digits &
				  ~static_cast<::std::uint_least64_t>(0xffu))
				 << 8u)};
			store_word(iter, assembled);
			iter += length + 1;
		}
		return ::fast_io::details::da::
			print_ascii_exponent<false>(
				iter, real_exponent);
	case 1u:
		store_word(iter, digits);
		/*
		This arm proves L<=X, so the exact fixed spelling appends
		X+1-L zeroes.  Fixed won the stored length comparison, bounding
		X+1 by the corresponding scientific length (at most ten for a
		narrow carrier); the loop is therefore both reserve-safe and tiny.
		*/
		for (auto position{length};
			 position <= real_exponent; ++position)
		{
			iter[position] = static_cast<char>(u8'0');
		}
		return iter + real_exponent + 1;
	case 2u:
		{
			auto const point_position{real_exponent + 1};
			if (point_position == length)
			{
				store_word(iter, digits);
				return iter + length;
			}
			/*
			Here 1<=point_position<L<=5.  The mask retains the integer
			prefix, the upper digits move by one byte, and the point occupies
			the unique vacated byte.  Those three fields are disjoint and
			their concatenation is precisely the generic fixed arm.
			*/
			auto const shift{
				static_cast<unsigned>(
					point_position * 8)};
			auto const lower_mask{
				(static_cast<::std::uint_least64_t>(1u)
				 << shift) -
				1u};
			auto const assembled{
				(digits & lower_mask) |
				(static_cast<::std::uint_least64_t>(u8'.')
				 << shift) |
				((digits & ~lower_mask) << 8u)};
			store_word(iter, assembled);
			return iter + length + 1;
		}
	default:
		/*
		X<0 gives "0." followed by -X-1 zeroes and then the coefficient.
		The coefficient store begins at 1-X, so it cannot overwrite the
		prefix or padding.  It may initialize scratch after the logical end,
		which is exactly why exact-bounds callers are excluded.
		*/
		iter[0] = static_cast<char>(u8'0');
		iter[1] = static_cast<char>(u8'.');
		for (::std::int_least32_t position{2};
			 position < 1 - real_exponent; ++position)
		{
			iter[position] = static_cast<char>(u8'0');
		}
		store_word(iter + 1 - real_exponent, digits);
		return iter + 1 - real_exponent + length;
	}
}

// GCC 13 and later Linux System V x86-64 LP64 use a compact regular-normal entry around
// the SSSE3/SSE4.1 ASCII writer.  This prevents normalization and generic-renderer
// state from extending the predominant shortest path's live range. GCC 14 and later
// use the specialized subnormal and irregular entries below.  GCC 13 deliberately
// sends both rare classes to the generic fallback: applying the later-major
// placement to GCC 13 improved regular values but increased measured subnormal
// latency by about thirty percent. Later GNU majors inherit the GCC 16 schedule
// as a forward-family hypothesis; other x86 ABIs retain the portable entry.
// All entries consume the same DA conversion_result and fall back before
// committing an unsupported fixed layout.  Revalidate frame size, aggregate
// spills, calls, constants, branches and linked text before moving the GCC 13
// transition. The `noinline` attributes below enforce the measured
// layout; a compiler that cannot express them still executes the same conversions
// and fallback rules, but may merge their live ranges into the caller.
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__SSE4_1__) && defined(__SSSE3__) && defined(__GNUC__) && \
	!defined(__clang__) && 13 <= __GNUC__ && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
// The subnormal entry owns normalization and a possible fixed-format retry.
// Its separate boundary is measured for the selected GCC/x86 set, not inferred
// from subnormal arithmetic.
template <typename flt, bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_format mt, bool json_float,
	::std::integral char_type>
[[nodiscard]]
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline char_type *print_rsvflt_da_ascii_subnormal(
	char_type *iter,
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa) noexcept
{
	constexpr auto direct_flags{[]() constexpr noexcept {
		auto value{::fast_io::manipulators::floating_point_default_scalar_flags};
		value.uppercase_e = uppercase_e;
		value.comma = comma;
		value.floating = mt;
		value.json_float = json_float;
		return value;
	}()};
	auto const converted{::fast_io::details::da::compute_binary64(
		static_cast<::std::uint_least64_t>(mantissa), 1u)};
	auto significand{converted.significand * 10u +
		(converted.has_last_digit ? converted.last_digit : 0u)};
	auto decimal_exponent{converted.exponent};
	while (significand < static_cast<::std::uint_least64_t>(1000000000000000))
	{
		significand *= 10u;
		--decimal_exponent;
	}
	auto const shortened{significand / 10u};
	auto const last_digit{static_cast<::std::uint_least32_t>(
		significand - shortened * 10u)};
	::fast_io::details::da::conversion_result const normalized{
		shortened, decimal_exponent, last_digit, last_digit != 0u};
	auto const direct{::fast_io::details::da::print_ascii_shortest<
		flt, direct_flags, false, true>(iter, normalized)};
	if constexpr (mt != ::fast_io::manipulators::floating_format::fixed)
	{
		return direct;
	}
	else if (direct != nullptr)
	{
		return direct;
	}
	auto const finalized{::fast_io::details::da::trim_trailing_zeros(
		::fast_io::details::da::finalize<flt>(converted))};
	return ::fast_io::details::print_rsvflt_decimal_define_impl<
		flt, comma, uppercase_e, mt, json_float>(iter, finalized.m10, finalized.e10);
}

// A zero-mantissa normal uses the asymmetric power-of-two interval and is
// structurally uncommon.  Outlining is part of the same measured GCC/x86 layout;
// without the attribute, the irregular decimal result is byte-identical.
template <typename flt, bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_format mt, bool json_float,
	::std::integral char_type>
[[nodiscard]]
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline char_type *print_rsvflt_da_ascii_irregular(
	char_type *iter, ::std::uint_least32_t exponent) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	constexpr auto direct_flags{[]() constexpr noexcept {
		auto value{::fast_io::manipulators::floating_point_default_scalar_flags};
		value.uppercase_e = uppercase_e;
		value.comma = comma;
		value.floating = mt;
		value.json_float = json_float;
		return value;
	}()};
	constexpr ::std::uint_least64_t implicit_bit{
		static_cast<::std::uint_least64_t>(1u) << trait::mbits};
	constexpr ::std::int_least32_t exponent_offset{
		trait::mbits == 52u ? 1075 : 150};
	auto const converted{::fast_io::details::da::compute_irregular(
		implicit_bit, static_cast<::std::int_least32_t>(exponent) - exponent_offset)};
	auto const direct{::fast_io::details::da::print_ascii_shortest<flt, direct_flags>(iter, converted)};
	if constexpr (mt != ::fast_io::manipulators::floating_format::fixed)
	{
		return direct;
	}
	else if (direct != nullptr)
	{
		return direct;
	}
	auto const finalized{::fast_io::details::da::trim_trailing_zeros(
		::fast_io::details::da::finalize<flt>(converted))};
	return ::fast_io::details::print_rsvflt_decimal_define_impl<
		flt, comma, uppercase_e, mt, json_float>(iter, finalized.m10, finalized.e10);
}

template <typename flt, bool comma, bool uppercase_e, bool json_float,
		  ::std::integral char_type>
[[nodiscard]] inline char_type *print_rsvflt_da_ascii_finalize_fixed(
	char_type *iter, ::std::uint_least64_t significand,
	::std::int_least32_t exponent, ::std::uint_least32_t last_digit,
	bool has_last_digit) noexcept;

// The generic fallback retains conversion and presentation state for cases the
// direct ASCII writer cannot commit (notably fixed layouts).  The selected
// GCC/x86 matrix measured the outlined boundary; attribute absence changes only
// call layout and not the final DA normalization or bytes.
template <typename flt, bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_format mt, bool json_float,
	::std::integral char_type>
[[nodiscard]]
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline char_type *print_rsvflt_da_ascii_fallback(
	char_type *iter,
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent) noexcept
{
	constexpr auto direct_flags{[]() constexpr noexcept {
		auto value{::fast_io::manipulators::floating_point_default_scalar_flags};
		value.uppercase_e = uppercase_e;
		value.comma = comma;
		value.floating = mt;
		value.json_float = json_float;
		return value;
	}()};
#if __GNUC__ == 13
	using rare_trait = ::fast_io::details::iec559_traits<flt>;
	if constexpr (rare_trait::mbits == 52u && rare_trait::ebits == 11u &&
				  mt != ::fast_io::manipulators::floating_format::fixed)
	{
		/*
		The GCC-13 regular-normal caller returns before reaching this leaf for
		non-fixed presentations.  Consequently exponent zero means subnormal,
		while a nonzero exponent necessarily accompanies a zero explicit
		mantissa and therefore selects DA's asymmetric power-of-two interval.
		Encoding that proved precondition here removes the otherwise unreachable
		regular conversion and writer from every rare specialization.

		The subnormal expression is exactly `to_conversion_result` after its
		raw-exponent-zero normalization: binary64 receives the unchanged explicit
		mantissa and effective raw exponent one.  The irregular expression is the
		same function's other branch: implicit bit 2^52 at binary exponent
		`raw_exponent - 1075`.  Presentation and finalization are unchanged.
		Fixed is excluded because a regular direct writer may reject its layout
		before committing bytes and then legitimately reaches this fallback.
		*/
		if (exponent == 0u)
		{
			auto const converted{::fast_io::details::da::compute_binary64(
				static_cast<::std::uint_least64_t>(mantissa), 1u)};
			auto const finalized{::fast_io::details::da::trim_trailing_zeros(
				::fast_io::details::da::finalize<flt>(converted))};
			return ::fast_io::details::print_rsvflt_decimal_define_impl<
				flt, comma, uppercase_e, mt, json_float>(
				iter, finalized.m10, finalized.e10);
		}
		constexpr ::std::uint_least64_t implicit_bit{
			static_cast<::std::uint_least64_t>(1u) << rare_trait::mbits};
		constexpr ::std::int_least32_t exponent_offset{
			(static_cast<::std::int_least32_t>(1u) << (rare_trait::ebits - 1u)) - 1 +
			static_cast<::std::int_least32_t>(rare_trait::mbits)};
		auto const converted{::fast_io::details::da::compute_irregular(
			implicit_bit,
			static_cast<::std::int_least32_t>(exponent) - exponent_offset)};
		return ::fast_io::details::da::print_ascii_shortest<flt, direct_flags>(
			iter, converted);
	}
#endif
	auto const converted{::fast_io::details::da::to_conversion_result<flt>(
		mantissa, static_cast<::std::int_least32_t>(exponent))};
	if (exponent != 0u)
	{
		auto const direct{::fast_io::details::da::print_ascii_shortest<flt, direct_flags>(iter, converted)};
		if constexpr (mt != ::fast_io::manipulators::floating_format::fixed)
		{
			return direct;
		}
		else if (direct != nullptr)
		{
			return direct;
		}
	}
	if constexpr (mt == ::fast_io::manipulators::floating_format::fixed)
	{
		return ::fast_io::details::print_rsvflt_da_ascii_finalize_fixed<
			flt, comma, uppercase_e, json_float>(iter, converted.significand,
												 converted.exponent, converted.last_digit, converted.has_last_digit);
	}
	else
	{
		auto const finalized{::fast_io::details::da::trim_trailing_zeros(
			::fast_io::details::da::finalize<flt>(converted))};
		return ::fast_io::details::print_rsvflt_decimal_define_impl<
			flt, comma, uppercase_e, mt, json_float>(iter, finalized.m10, finalized.e10);
	}
}

// A direct fixed-layout miss has already completed DA conversion.  This rare
// leaf accepts scalar carrier fields rather than the original binary fields, so
// it cannot repeat the cache multiplication performed by to_conversion_result.
// Scalar parameters also avoid the stack-passed 24-byte conversion_result under
// the measured System V AMD64 ABI.  finalize() is an exact representation
// change, and trim_trailing_zeros() preserves its value while producing the
// canonical input expected by the character-generic fixed renderer.  The
// noinline boundary is a code-placement policy confined by the enclosing
// compiler/ABI guard; attribute availability cannot change the conversion.
template <typename flt, bool comma, bool uppercase_e, bool json_float,
		  ::std::integral char_type>
[[nodiscard]]
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline char_type *print_rsvflt_da_ascii_finalize_fixed(
	char_type *iter, ::std::uint_least64_t significand,
	::std::int_least32_t exponent, ::std::uint_least32_t last_digit,
	bool has_last_digit) noexcept
{
	::fast_io::details::da::conversion_result const converted{
		significand, exponent, last_digit, has_last_digit};
	auto const finalized{::fast_io::details::da::trim_trailing_zeros(
		::fast_io::details::da::finalize<flt>(converted))};
	return ::fast_io::details::print_rsvflt_decimal_define_impl<
		flt, comma, uppercase_e, ::fast_io::manipulators::floating_format::fixed,
		json_float>(iter, finalized.m10, finalized.e10);
}
#endif

template <bool showpos, bool uppercase, bool uppercase_e, bool comma, ::fast_io::manipulators::floating_format mt,
		  ::fast_io::manipulators::floating_rounding rounding =
			  ::fast_io::manipulators::floating_rounding::nearest_to_even,
		  bool nan_show_sign = true, bool nan_show_type = false, bool json_float = false,
		  typename flt, bool exact_bounds = false, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_rsvflt_fields_define_impl(
	char_type *iter, typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool sign) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return print_rsvflt_fields_define_impl<showpos, uppercase, uppercase_e, comma, mt,
											::fast_io::manipulators::floating_rounding::toward_plus_infinity,
											nan_show_sign, nan_show_type, json_float, flt, exact_bounds>(iter, mantissa, exponent, sign);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return print_rsvflt_fields_define_impl<showpos, uppercase, uppercase_e, comma, mt,
											::fast_io::manipulators::floating_rounding::toward_minus_infinity,
											nan_show_sign, nan_show_type, json_float, flt, exact_bounds>(iter, mantissa, exponent, sign);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return print_rsvflt_fields_define_impl<showpos, uppercase, uppercase_e, comma, mt,
											::fast_io::manipulators::floating_rounding::toward_zero,
											nan_show_sign, nan_show_type, json_float, flt, exact_bounds>(iter, mantissa, exponent, sign);
		default:
			return print_rsvflt_fields_define_impl<showpos, uppercase, uppercase_e, comma, mt,
											::fast_io::manipulators::floating_rounding::nearest_to_even,
											nan_show_sign, nan_show_type, json_float, flt, exact_bounds>(iter, mantissa, exponent, sign);
		}
	}
	if constexpr (::fast_io::manipulators::floating_format::fixed == mt && uppercase_e)
	{
		return print_rsvflt_fields_define_impl<showpos, uppercase, false, comma, mt, rounding, nan_show_sign,
										nan_show_type, json_float, flt, exact_bounds>(iter, mantissa, exponent, sign);
	}
	else
	{
		using trait = iec559_traits<flt>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr ::std::size_t mbits{trait::mbits};
		constexpr ::std::size_t ebits{trait::ebits};
		constexpr mantissa_type exponent_mask{(static_cast<mantissa_type>(1) << ebits) - 1};
		constexpr ::std::uint_least32_t exponent_mask_u32{static_cast<::std::uint_least32_t>(exponent_mask)};
		/*
		A precise reservation owns exactly the measured output interval: writing
		even a subsequently ignored byte past that interval would violate the C++
		object bound.  `exact_bounds` is therefore an emission-storage policy, not
		a conversion policy.  It disables DA ASCII leaves whose fixed-width stores
		require ordinary reserve slack; the identical decimal carrier then reaches
		the generic renderer, whose loops write exactly the characters they return.
		The narrow 96-KiB table contains carriers rather than characters and is
		therefore valid under both storage policies without an alternate lookup.
		*/
		if (exponent == exponent_mask_u32)
		{
			return prsv_fp_nan_impl<showpos, uppercase, nan_show_sign, nan_show_type, mbits>(iter, mantissa, sign);
		}
		iter = print_rsv_fp_sign_impl<showpos>(iter, sign);
		if (!mantissa && !exponent)
		{
			if constexpr (mt != ::fast_io::manipulators::floating_format::scientific)
			{
				*iter = char_literal_v<u8'0', char_type>;
				++iter;
				if constexpr (json_float)
				{
					return ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(iter);
				}
				return iter;
			}
			else
			{
				return prsv_fp_dece0<uppercase>(iter);
			}
		}
		if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even &&
						   ((trait::mbits == 23u && trait::ebits == 8u) ||
							(trait::mbits == 52u && trait::ebits == 11u)) &&
						   ::fast_io::details::da::scalar_shortest_supported<flt, char_type> &&
						   ::std::same_as<char_type, char> && ::fast_io::details::is_ascii<char_type>)
		{
			if constexpr (exact_bounds)
			{
				// DA conversion is part of the unique shortest-carrier proof and is
				// retained.  Only its ASCII presentation leaf permits staged stores;
				// finalizing the same carrier into the generic renderer preserves both
				// the spelling selected by the size model and the exact object bound.
				auto const converted{::fast_io::details::da::to_conversion_result<flt>(
					mantissa, static_cast<::std::int_least32_t>(exponent))};
				auto const finalized{::fast_io::details::da::trim_trailing_zeros(
					::fast_io::details::da::finalize<flt>(converted))};
				return ::fast_io::details::print_rsvflt_decimal_define_impl<
					flt, comma, uppercase_e, mt, json_float>(
						iter, finalized.m10, finalized.e10);
			}
#if FAST_IO_HAS_BUILTIN(__builtin_is_constant_evaluated)
			if (__builtin_is_constant_evaluated())
#else
			if (::std::is_constant_evaluated())
#endif
			{
				// Architecture-specific ASCII leaves intentionally optimize run-time
				// stores and are not required to be constexpr.  Constant evaluation
				// keeps the identical DA carrier but finalizes it through the generic
				// code-unit renderer, which is the semantic reference for every fast
				// leaf below.  The condition is a compile-time false in ordinary code,
				// so it adds no dynamic branch or alternate formatter type.
				auto const converted{
					::fast_io::details::da::to_conversion_result<flt>(
						mantissa,
						static_cast<::std::int_least32_t>(exponent))};
				auto const finalized{
					::fast_io::details::da::trim_trailing_zeros(
						::fast_io::details::da::finalize<flt>(converted))};
				return ::fast_io::details::print_rsvflt_decimal_define_impl<
					flt, comma, uppercase_e, mt, json_float>(
						iter, finalized.m10, finalized.e10);
			}
			constexpr auto direct_flags{[]() constexpr noexcept {
				auto value{::fast_io::manipulators::floating_point_default_scalar_flags};
				value.uppercase = uppercase;
				value.uppercase_e = uppercase_e;
				value.comma = comma;
				value.floating = mt;
				value.json_float = json_float;
				return value;
			}()};
			constexpr auto direct_scientific_flags{[]() constexpr noexcept {
				auto value{
					::fast_io::manipulators::
						floating_point_default_scalar_flags};
				value.uppercase = uppercase;
				value.uppercase_e = uppercase_e;
				value.comma = comma;
				value.floating =
					::fast_io::manipulators::
						floating_format::scientific;
				value.json_float = json_float;
				return value;
			}()};
			// GCC-specific shortest-emission boundaries.  Regular binary64 values
			// keep conversion and the selected renderer close; irregular/subnormal
			// values use a compiler-major-specific rare entry.  The portable branch
			// below is semantically identical. GCC 13--16 are the recorded Linux
			// System V x86-64 LP64 matrix; the continuous GCC-13 lower bound lets later
			// GNU majors inherit the GCC-16 schedule. Every other compiler/ABI uses the
			// portable entry. This fence protects code shape only and requires assembly,
			// benchmark and linked-text validation before moving the lower bound.
#if defined(__linux__) && defined(__x86_64__) && defined(__LP64__) && \
	defined(__SSE4_1__) && defined(__SSSE3__) && defined(__GNUC__) && \
	!defined(__clang__) && 13 <= __GNUC__ && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
			if constexpr (trait::mbits == 52u && trait::ebits == 11u)
			{
				if (exponent != 0u && mantissa != 0u)
				{
					auto const binary_significand{static_cast<::std::uint_least64_t>(mantissa) |
						(static_cast<::std::uint_least64_t>(1u) << trait::mbits)};
					auto const regular{::fast_io::details::da::compute_binary64(
						binary_significand, exponent)};
					if constexpr (
						mt ==
						::fast_io::manipulators::
							floating_format::decimal)
					{
						if (::fast_io::details::
								print_rsvflt_da_scientific_is_strictly_shorter<
									flt>(exponent))
						{
							return ::fast_io::details::da::
								print_ascii_shortest<
									flt,
									direct_scientific_flags>(
										iter, regular);
						}
					}
					// GCC 13-15 keep selected decimal fixed bands in the direct entry so
					// the already-known layout does not flow through the general decision.
					// GCC 16 uses the unified path after its constant/load schedule changed.
					// The exponent bands are formatting-equivalent and were chosen from
					// assembly hit/miss analysis, not from a numeric special case.
#if __GNUC__ < 16
					if constexpr (mt == ::fast_io::manipulators::floating_format::decimal)
					{
						// Unsigned subtraction encodes raw-exponent intervals
						// [1022, 1024] and [1072, 1075].  In these bands decimal's
						// generated length mask has already selected a fixed layout, so
						// the fixed-only entry can omit the general decision.  Keep this
						// condition synchronized with that mask and test both endpoints
						// plus their adjacent exponents for fixed/scientific agreement.
						if (static_cast<::std::uint_least32_t>(exponent - 1022u) <= 2u ||
							static_cast<::std::uint_least32_t>(exponent - 1072u) <= 3u)
						{
							return ::fast_io::details::da::print_ascii_shortest_fixed_direct<
								flt, direct_flags>(iter, regular);
						}
					}
#endif
					auto const direct{::fast_io::details::da::print_ascii_shortest<flt, direct_flags>(
						iter, regular)};
					if constexpr (mt != ::fast_io::manipulators::floating_format::fixed)
					{
						return direct;
					}
					else if (direct != nullptr)
					{
						return direct;
					}
					else
					{
						return ::fast_io::details::print_rsvflt_da_ascii_finalize_fixed<
							flt, comma, uppercase_e, json_float>(iter, regular.significand,
																 regular.exponent, regular.last_digit, regular.has_last_digit);
					}
				}
			}
			if constexpr (trait::mbits == 52u && trait::ebits == 11u)
			{
#if __GNUC__ == 13
				// GCC 13 obtains the compact regular leaf from the outer placement
				// boundary.  Reaching the non-fixed fallback therefore proves either a
				// subnormal encoding or the asymmetric zero-mantissa normal case; the
				// fallback records that precondition while retaining the same DA carrier
				// and presentation rules.  Keeping subnormal finalization in that single
				// leaf avoids the regression measured with the GCC 14--16 normalization
				// and ASCII-direct policy.
				return ::fast_io::details::print_rsvflt_da_ascii_fallback<
					flt, comma, uppercase_e, mt, json_float>(iter, mantissa, exponent);
#else
				if (exponent != 0u)
				{
					return ::fast_io::details::print_rsvflt_da_ascii_irregular<
						flt, comma, uppercase_e, mt, json_float>(iter, exponent);
				}
				if (exponent == 0u)
				{
					return ::fast_io::details::print_rsvflt_da_ascii_subnormal<
						flt, comma, uppercase_e, mt, json_float>(iter, mantissa);
				}
				return ::fast_io::details::print_rsvflt_da_ascii_fallback<
					flt, comma, uppercase_e, mt, json_float>(iter, mantissa, exponent);
#endif
			}
			else
			{
				// GCC 15 and 16 keep binary32 conversion in scalar fields, avoiding a
				// conversion_result aggregate across the renderer boundary. GCC 15
				// improves all presentations by 2.9--4.9% after the sign-carrier fix,
				// and later GNU majors inherit that all-format lower bound. GCC 14 has
				// a narrower code-generation result: physical-core AB/BA runs improve
				// decimal by 11.6--13.4%, but regress general by 14.4--15.2%, scientific
				// by 2.5--4.9%, and fixed by 3.3--4.0%. It therefore instantiates the
				// same outlined leaf only for decimal. Every corpus hash agrees; this is
				// placement policy, not a different DA carrier or presentation rule.
#if 14 <= __GNUC__
				constexpr bool use_binary32_split{
					15 <= __GNUC__ ||
					mt == ::fast_io::manipulators::floating_format::decimal};
				if constexpr (use_binary32_split)
				{
					if (exponent != 0u && mantissa != 0u)
					{
						::std::uint_least64_t significand;
						::std::int_least32_t decimal_exponent;
						::std::uint_least32_t last_digit;
						bool has_last_digit;
						auto const binary_significand{
							static_cast<::std::uint_least32_t>(mantissa) |
							(static_cast<::std::uint_least32_t>(1u) << trait::mbits)};
						::fast_io::details::da::compute_binary32_fields(
							binary_significand, exponent, significand, decimal_exponent,
							last_digit, has_last_digit);
						if constexpr (
							mt ==
							::fast_io::manipulators::
								floating_format::decimal)
						{
							if (::fast_io::details::
									print_rsvflt_da_scientific_is_strictly_shorter<
										flt>(exponent))
							{
								/*
								GCC 16's profitable scientific-band schedule is
								the inverse of the general decimal schedule below:
								the renderer is small enough to inline after the
								layout decision has been proved at the raw-exponent
								level, keeping the four SIMD divisors behind one base
								shortens their live ranges, and staged emission lets
								the x86 renderer assemble the complete scientific
								spelling with its single pshufb.  Two independent
								three-cycle CPU6 ABBA campaigns improved the complete
								public f32 conversion by 5.68% and 6.12%; the isolated
								fields result ranged from +0.08% to -0.90%, so the
								public call, not the isolated leaf, is the acceptance
								metric.  The staged choice adds 632 linked text bytes.

								GCC 14/15 retain the previously measured outlined
								entry.  The arithmetic identity is exact in either
								case: compute_binary32_fields has already produced
								the same four carrier fields, and both renderer
								entries instantiate print_ascii_shortest_fields with
								scientific flags.  Cached versus immediate divisor
								constants spell identical reciprocal integers; only
								load placement and the call boundary differ.  More
								specifically, staged=false and staged=true derive the
								same digit block, span, scientific exponent, optional
								interval digit and returned length.  The latter merely
								selects the x86 shuffle spelling proved equivalent to
								the scalar assembler in da/ascii.h.  Exhaustively
								comparing both renderers for every nonzero-mantissa
								normal binary32 encoding in the guaranteed-scientific
								raw-exponent bands covered 1,619,001,151 carriers with
								identical bytes and pointers.
								*/
#if 16 <= __GNUC__
								return ::fast_io::details::da::
									print_ascii_shortest_fields<
										flt,
										direct_scientific_flags,
										true, true>(
											iter, significand,
											decimal_exponent,
											last_digit,
											has_last_digit);
#else
								return ::fast_io::details::da::
									print_ascii_shortest_split<
										flt,
										direct_scientific_flags>(
											iter, significand,
											decimal_exponent,
											last_digit,
											has_last_digit);
#endif
							}
						}
						// This call boundary preserves the admitted major/format live
						// ranges. Recheck spills, calls, pshufb placement and linked text
						// if a future compiler requires another transition.
						auto const direct{
							::fast_io::details::da::print_ascii_shortest_split<
								flt, direct_flags>(iter, significand, decimal_exponent,
								last_digit, has_last_digit)};
						if constexpr (mt != ::fast_io::manipulators::floating_format::fixed)
						{
							return direct;
						}
						else if (direct != nullptr)
						{
							return direct;
						}
						else
						{
							return ::fast_io::details::print_rsvflt_da_ascii_fallback<
								flt, comma, uppercase_e, mt, json_float>(
									iter, mantissa, exponent);
						}
					}
				}
#endif
				auto const converted{::fast_io::details::da::to_conversion_result<flt>(
					mantissa, static_cast<::std::int_least32_t>(exponent))};
				if (exponent != 0u)
				{
					auto const direct{::fast_io::details::da::print_ascii_shortest<
						flt, direct_flags>(iter, converted)};
					if (direct != nullptr)
					{
						return direct;
					}
				}
				auto const finalized{::fast_io::details::da::trim_trailing_zeros(
					::fast_io::details::da::finalize<flt>(converted))};
				return ::fast_io::details::print_rsvflt_decimal_define_impl<
					flt, comma, uppercase_e, mt, json_float>(
						iter, finalized.m10, finalized.e10);
			}
#else
			// Clang, non-x86, baseline x86 and GCC versions outside the audited
			// specialization use the platform-neutral DA conversion and ASCII writer.
			// This is the semantic reference for every GCC-specific branch above.
			auto const converted{::fast_io::details::da::to_conversion_result<flt>(
				mantissa, static_cast<::std::int_least32_t>(exponent))};
			if constexpr (
				mt == ::fast_io::manipulators::floating_format::decimal)
			{
				if (::fast_io::details::
						print_rsvflt_da_scientific_is_strictly_shorter<
							flt>(exponent))
				{
					return ::fast_io::details::da::
						print_ascii_shortest<
							flt, direct_scientific_flags>(
								iter, converted);
				}
			}
			/*
			The binary64 ASCII leaf consumes a 15- or 16-digit provisional
			coefficient.  Every normal carrier satisfies that invariant.  A high
			subnormal satisfies it as well once compute_binary64 returns at least

			  10^14,

			so its raw binary exponent need not exclude the direct leaf.  Below
			this threshold the fixed-width digit block would count leading zero
			lanes as significant; finalize() is therefore still required.

			The carrier proof is compiler-independent, while admission to this leaf
			is an Apple-AArch64 code-generation policy.  Whole-call AB/BA measurements
			on Apple M4 accepted Apple Clang 21 and upstream Clang 22 and 23:
			high-subnormal latency fell by approximately 20--27%, the complete
			exceptional corpus improved, and finite-normal aggregate controls remained
			neutral.  For all four public shortest presentations, the normalized
			Clang 22 instruction streams are identical to Clang 23 after this branch
			is enabled; Compiler Explorer also compiles the complete four-writer probe
			with Clang 16--22.  Consequently this policy names the Apple-AArch64
			Clang family rather than a compiler-major whitelist.  GCC is excluded by
			positive counter-evidence, not by an unmeasured-version assumption: GCC 15
			improves the exceptional leaf but changes normal-path register allocation,
			grows the instantiated general/decimal text by 136/124 bytes, and regresses
			the repeated general normal control by 1.7--6.1%; AArch64 GCC 13 and 14 also
			grow the general frame from 64 to 80 bytes.  A future counterexample must
			narrow or widen this policy with output, normal-control, frame, call, and
			linked-text evidence; absence of a version measurement is not itself an
			exclusion criterion.  Other compiler, OS, and ISA families retain the former
			exponent-only predicate because this evidence does not model their complete
			instruction schedule.  Fixed format remains excluded because this exponent
			is outside its direct-layout interval and would only pay for a guaranteed
			null result.
			*/
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__)) && \
	defined(__clang__)
			constexpr bool ascii_accepts_normalized_subnormal{
				trait::mbits == 52u && trait::ebits == 11u &&
				mt != ::fast_io::manipulators::floating_format::fixed};
#else
			constexpr bool ascii_accepts_normalized_subnormal{};
#endif
			constexpr ::std::uint_least64_t binary64_minimum_direct_coefficient{
				static_cast<::std::uint_least64_t>(100000000000000)};
			if (exponent != 0u ||
				(ascii_accepts_normalized_subnormal &&
				 converted.significand >= binary64_minimum_direct_coefficient))
			{
				auto const direct{::fast_io::details::da::print_ascii_shortest<flt, direct_flags>(iter, converted)};
				if (direct != nullptr)
				{
					return direct;
				}
			}
			auto const finalized{::fast_io::details::da::trim_trailing_zeros(
				::fast_io::details::da::finalize<flt>(converted))};
			return ::fast_io::details::print_rsvflt_decimal_define_impl<flt, comma, uppercase_e, mt, json_float>(
				iter, finalized.m10, finalized.e10);
#endif
		}
		else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even &&
						   ((trait::mbits == 23u && trait::ebits == 8u) ||
							(trait::mbits == 52u && trait::ebits == 11u)) &&
						   ::fast_io::details::da::scalar_shortest_supported<flt, char_type> &&
						   ((sizeof(char_type) == 4u &&
							 (::std::same_as<char_type, wchar_t> || ::std::same_as<char_type, char32_t>)) ||
							(::std::same_as<char_type, char16_t> && trait::mbits == 52u && trait::ebits == 11u)))
		{
			/*
			DA's interval computation is independent of the destination character
			encoding.  Finalizing its carrier produces the same unique shortest
			(m10, e10) pair consumed by the character-generic renderer, whose digit
			and punctuation literals already model the destination character type.
			This route therefore changes conversion work only; it does
			not widen through a temporary byte buffer and it remains exact-store safe.

			The destination restriction is an empirical code-generation policy, not a
			numeric limitation.  Four-byte wchar_t/char32_t improved for both binary32
			and binary64 across the measured M-series, GCC x86-64, and Clang x86-64
			matrix; char16_t improved uniformly for binary64.  Binary32 char16_t and
			one-byte destinations remain on the existing carrier because their
			compiler/type pairs were not uniformly faster.  Revalidate latency and
			linked text for every admitted type-width pair.
			*/
			auto const converted{::fast_io::details::da::to_conversion_result<flt>(
				mantissa, static_cast<::std::int_least32_t>(exponent))};
			auto const finalized{::fast_io::details::da::trim_trailing_zeros(
				::fast_io::details::da::finalize<flt>(converted))};
			return ::fast_io::details::print_rsvflt_decimal_define_impl<
				flt, comma, uppercase_e, mt, json_float>(iter, finalized.m10, finalized.e10);
		}
		else if constexpr (::fast_io::details::dragonbox_uses_binary32_core<flt> &&
						   sizeof(flt) < sizeof(float) &&
						   rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
		{
			auto const decimal{::fast_io::details::dragonbox_narrow_shortest_lookup<flt>(
				mantissa, static_cast<::std::int_least32_t>(exponent))};
			if constexpr (
				!exact_bounds && mt ==
					::fast_io::manipulators::floating_format::decimal &&
				!comma && !uppercase_e && !json_float &&
				::std::same_as<char_type, char> &&
				::fast_io::details::is_ascii<char_type>)
			{
				return ::fast_io::details::
					print_rsvflt_narrow_ascii_decimal<flt>(
						iter, decimal);
			}
			else
			{
				return ::fast_io::details::
					print_rsvflt_decimal_with_length_define_impl<
						flt, comma, uppercase_e, mt, json_float>(
							iter, decimal.m10, decimal.e10,
							decimal.length);
			}
		}
		else
		{
			auto [m10, e10] =
				[]([[maybe_unused]] mantissa_type binary_mantissa,
				   [[maybe_unused]] ::std::int_least32_t binary_exponent,
				   [[maybe_unused]] bool negative) constexpr noexcept {
					if constexpr (::fast_io::details::dragonbox_uses_binary32_core<flt> &&
								  sizeof(flt) < sizeof(float))
					{
						return ::fast_io::details::dragonbox_impl_narrow_hybrid<flt, rounding>(
							binary_mantissa, binary_exponent, negative);
					}
					else
					{
						return ::fast_io::details::dragonbox_impl<flt, rounding>(
							binary_mantissa, binary_exponent, negative);
					}
				}(mantissa, static_cast<::std::int_least32_t>(exponent), sign);
			return ::fast_io::details::print_rsvflt_decimal_define_impl<flt, comma, uppercase_e, mt, json_float>(
				iter, m10, e10);
		}
	}
}

template <bool showpos, bool uppercase, bool uppercase_e, bool comma,
	::fast_io::manipulators::floating_format mt,
	::fast_io::manipulators::floating_rounding rounding =
		::fast_io::manipulators::floating_rounding::nearest_to_even,
	bool nan_show_sign = true, bool nan_show_type = false, bool json_float = false,
	typename flt, ::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_rsvflt_define_impl(
	char_type *iter, flt f) noexcept
{
	auto const [mantissa, exponent, sign]{
		::fast_io::details::get_punned_result(f)};
	return ::fast_io::details::print_rsvflt_fields_define_impl<
		showpos, uppercase, uppercase_e, comma, mt, rounding, nan_show_sign,
		nan_show_type, json_float, flt>(iter, mantissa, exponent, sign);
}

/*
`print_rsvflt_exact_bounds_define_impl` is the private entry for the precise-
reservation protocol.  It deliberately shares field extraction, conversion,
rounding and presentation with `print_rsvflt_define_impl`; the sole difference
is the compile-time storage contract passed to the final renderer.  Keeping the
policy at this boundary prevents an exact allocation from entering an ASCII
writer that is allowed to stage bytes beyond its logical cursor, while leaving
the ordinary reserve entry and all of its measured specializations unchanged.
*/
template <bool showpos, bool uppercase, bool uppercase_e, bool comma,
	::fast_io::manipulators::floating_format mt,
	::fast_io::manipulators::floating_rounding rounding =
		::fast_io::manipulators::floating_rounding::nearest_to_even,
	bool nan_show_sign = true, bool nan_show_type = false,
	bool json_float = false, typename flt, ::std::integral char_type>
inline constexpr char_type *print_rsvflt_exact_bounds_define_impl(
	char_type *iter, flt f) noexcept
{
	auto const [mantissa, exponent, sign]{
		::fast_io::details::get_punned_result(f)};
	return ::fast_io::details::print_rsvflt_fields_define_impl<
		showpos, uppercase, uppercase_e, comma, mt, rounding, nan_show_sign,
		nan_show_type, json_float, flt, true>(iter, mantissa, exponent, sign);
}

template <typename flt>
[[nodiscard]] inline constexpr bool dragonbox_decimal_carrier_is_binary_exact(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type binary_mantissa,
	::std::uint_least32_t raw_exponent,
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> decimal_mantissa,
	::std::int_least32_t decimal_exponent) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	if constexpr (sizeof(mantissa_type) > sizeof(::std::uint_least64_t))
	{
		return false;
	}
	else
	{
		constexpr ::std::int_least32_t bias{
			(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
		::std::int_least32_t binary_exponent{};
		if (raw_exponent)
		{
			binary_mantissa |= static_cast<mantissa_type>(static_cast<mantissa_type>(1u) << trait::mbits);
			binary_exponent = static_cast<::std::int_least32_t>(raw_exponent) - bias -
				static_cast<::std::int_least32_t>(trait::mbits);
		}
		else
		{
			binary_exponent = 1 - bias - static_cast<::std::int_least32_t>(trait::mbits);
		}
		auto decimal_odd{static_cast<::std::uint_least64_t>(decimal_mantissa)};
		auto decimal_binary_exponent{decimal_exponent};
		if (decimal_exponent < 0)
		{
			// A uint64_t cannot contain 5^28. Reject tiny shortest carriers before
			// entering the factor-removal loop; this is the common subnormal case.
			if (decimal_exponent < -27)
			{
				return false;
			}
			auto count{static_cast<::std::uint_least32_t>(-decimal_exponent)};
			for (; count; --count)
			{
				if (decimal_odd % 5u)
				{
					return false;
				}
				decimal_odd /= 5u;
			}
		}
		else
		{
			constexpr auto uint64_max{(::std::numeric_limits<::std::uint_least64_t>::max)()};
			auto count{static_cast<::std::uint_least32_t>(decimal_exponent)};
			for (; count; --count)
			{
				if (uint64_max / 5u < decimal_odd)
				{
					return false;
				}
				decimal_odd *= 5u;
			}
		}
		auto const decimal_zeroes{static_cast<::std::int_least32_t>(::std::countr_zero(decimal_odd))};
		decimal_odd >>= decimal_zeroes;
		decimal_binary_exponent += decimal_zeroes;

		auto binary_odd{static_cast<::std::uint_least64_t>(binary_mantissa)};
		auto const binary_zeroes{static_cast<::std::int_least32_t>(::std::countr_zero(binary_odd))};
		binary_odd >>= binary_zeroes;
		binary_exponent += binary_zeroes;
		return binary_odd == decimal_odd && binary_exponent == decimal_binary_exponent;
	}
}

/*
The general/fractional-preserving shortcut below is admitted only for a normal
binary64 value, K=requested>length and K<=digits10=15.  Let x be the exact
binary value and let d be its shortest round-trip decimal carrier.  If x lies
on the requested 10^-P grid, its canonical decimal expansion terminates within
K significant positions.  Padding d to K positions would then give a second
K-digit decimal which round-trips to x unless d is x itself.  That is
impossible: the separation bound represented by
numeric_limits<double>::digits10 makes the round-to-binary map injective on
decimal values of at most K significant digits.  Equivalently, two distinct
such decimal grid points are farther apart than one binary64 rounding cell,
independently of which nearest tie boundary is closed.  Therefore a false
result from dragonbox_decimal_carrier_is_binary_exact proves that rounding
discarded a nonzero exact tail; there is no third case in which d is inexact
while x was already on the requested grid.

For that nonexact case, K=P+R+1, where R is the leading decimal exponent.
Padding the L-digit carrier by K-L zeroes changes its stored exponent to

  (R-L+1) - (K-L) = -P.

The numerical carrier is unchanged, but the existing preserving renderer now
observes exactly the virtual coefficient required by the exact path.  Since
K<=15, both 10^(K-L) and the padded coefficient fit in uint64_t.  Binary-exact
values retain their unpadded carrier because they must not synthesize a suffix.
*/
struct binary64_general_fractional_carrier
{
	::std::uint_least64_t mantissa;
	::std::int_least32_t exponent;
};

// Keep the exactness test and carrier adjustment out of the public precision
// dispatcher.  In a ten-million-value uniform normal/P0--P128 audit this leaf
// was reached only 96 times (0.00096%), and short-circuiting avoided the
// exactness predicate in six of them.  Outlining reduced the GCC 15 caller
// growth from 253 to 100 text bytes; the Apple placement below retains only the
// small adjustment in its caller.  The attribute is solely a placement policy:
// constant evaluation and every arithmetic/output decision are identical when
// a compiler cannot express it.
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
[[nodiscard]] inline constexpr binary64_general_fractional_carrier
prepare_binary64_general_fractional_carrier(
	::std::uint_least64_t binary_mantissa, ::std::uint_least32_t raw_exponent,
	::std::uint_least64_t decimal_mantissa, ::std::int_least32_t decimal_exponent,
	::std::size_t length, ::std::size_t requested, ::std::size_t precision) noexcept
{
	// L<K is equivalent to decimal_exponent>-P.  When P<5 and the
	// unpadded exponent is also in general's (-5, 7) fixed interval, both
	// representations select the fixed precision renderer.  That renderer
	// already supplies exactly P fractional places, so binary exactness cannot
	// change the spelling and its factor-removal loop is unnecessary.
	if ((precision < 5u && -5 < decimal_exponent && decimal_exponent < 7) ||
		::fast_io::details::dragonbox_decimal_carrier_is_binary_exact<double>(
			binary_mantissa, raw_exponent, decimal_mantissa, decimal_exponent))
	{
		return {decimal_mantissa, decimal_exponent};
	}
	auto const padding{requested - length};
	return {decimal_mantissa *
			::fast_io::details::print_rsv_fp_pow10_0_to_19_table[padding],
		decimal_exponent - static_cast<::std::int_least32_t>(padding)};
}

// This logarithmic upper-bound test is reached only after the inexpensive raw-
// exponent filters.  Outlining keeps its reciprocal arithmetic out of the main
// precision branch layout.  The attribute controls placement only; the bound
// remains the same conservative sufficient condition on every compiler.
template <typename flt>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
[[nodiscard]] inline constexpr bool exact_precision_fractional_tiny_binary_bound(
	::std::uint_least32_t exponent, ::std::size_t precision) noexcept
{
	constexpr auto int32_max{(::std::numeric_limits<::std::int_least32_t>::max)()};
	if (static_cast<::std::size_t>(int32_max) < precision)
	{
		return false;
	}
	using trait = ::fast_io::details::iec559_traits<flt>;
	constexpr ::std::uint_least32_t exponent_bias{
		(static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u};
	// The magnitude is strictly below 2^(unbiased_exponent + 1).  If even
	// that upper bound is one decimal decade below the requested quantum,
	// every nearest policy rounds to zero.
	// Both alternatives are signed exponents. Explicitly convert the bias before subtraction; otherwise the normal
	// branch wraps negative exponents through unsigned arithmetic and relies on an implementation-defined conversion
	// when the logarithm helper accepts its signed argument.
	::std::int_least32_t const binary_upper_exponent{exponent
		? static_cast<::std::int_least32_t>(exponent) -
			  static_cast<::std::int_least32_t>(exponent_bias) + 1
		: 1 - static_cast<::std::int_least32_t>(exponent_bias)};
	auto const decimal_upper_exponent{
		::fast_io::details::mul_ln2_div_ln10_floor(binary_upper_exponent)};
	return static_cast<::std::int_least64_t>(decimal_upper_exponent) +
		static_cast<::std::int_least64_t>(precision) <= -2;
}

template <typename flt>
[[nodiscard]] inline constexpr bool exact_precision_fractional_tiny_wide_binary_bound(
	::std::uint_least32_t exponent, ::std::size_t precision) noexcept
{
	constexpr auto int32_max{(::std::numeric_limits<::std::int_least32_t>::max)()};
	if (static_cast<::std::size_t>(int32_max) < precision)
	{
		return false;
	}
	using trait = ::fast_io::details::iec559_traits<flt>;
	static_assert(::fast_io::details::exact_precision_is_wide_binary<flt>);
	constexpr ::std::uint_least32_t exponent_bias{
		(static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u};
	::std::int_least32_t const binary_upper_exponent{
		exponent ? static_cast<::std::int_least32_t>(exponent) -
					   static_cast<::std::int_least32_t>(exponent_bias) + 1
				 : 1 - static_cast<::std::int_least32_t>(exponent_bias)};
	/*
	 The shared binary32/binary64 logarithm helper intentionally uses a 32-bit
	 product because those exponents are below 1100.  Binary80/binary128 reach
	 16384, so that representation would overflow before the fixed-point shift.
	 Widening only this proof calculation preserves the same lower logarithmic
	 bound over the larger domain and does not change any formatting arithmetic.
	 */
	auto const decimal_upper_exponent{static_cast<::std::int_least32_t>(
		(static_cast<::std::int_least64_t>(binary_upper_exponent) * 1262611) >> 22)};
	return static_cast<::std::int_least64_t>(decimal_upper_exponent) +
		static_cast<::std::int_least64_t>(precision) <= -2;
}

template <typename flt, bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode,
	bool json_float, ::std::integral char_type>
inline constexpr char_type *print_rsvflt_fractional_tiny_zero_impl(
	char_type *iter, ::std::size_t precision) noexcept
{
	constexpr bool preserve{
		::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>};
	*iter = char_literal_v<u8'0', char_type>;
	++iter;
	if constexpr (format == ::fast_io::manipulators::floating_format::general)
	{
		// Fractional rounding represents zero as 0 * 10^-P.  A non-preserving
		// presentation must first canonicalize that equivalent pair to (0, 0),
		// exactly as print_rsv_fp_trim_trailing_zero does on the ordinary path;
		// otherwise the general-format decision would expose the stale quantum as
		// `0e-P`.  JSON still needs a fractional marker for canonical zero.
		if constexpr (!preserve)
		{
			if constexpr (json_float)
			{
				return ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(iter);
			}
			return iter;
		}
		// Preserving mode deliberately retains the requested quantum.  Since the
		// decimal coefficient zero has one digit, the established general rule
		// chooses fixed notation for -4 <= -P and scientific notation otherwise.
		// P=0 has no requested fractional zero, but JSON still requires `.0`.
		if (!precision)
		{
			if constexpr (json_float)
			{
				return ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(iter);
			}
			return iter;
		}
		if (precision < 5u)
		{
			return ::fast_io::details::print_rsv_fp_append_point_zeros<comma>(iter, precision);
		}
		// Every positive-precision caller has already proved that P fits int32;
		// the exponent negation is therefore representable.
		return ::fast_io::details::print_rsv_fp_e_impl<flt, uppercase_e>(
			iter, -static_cast<::std::int_least32_t>(precision));
	}
	else
	{
		if constexpr (json_float)
		{
			if (!precision || !preserve)
			{
				return ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(iter);
			}
		}
		if constexpr (preserve)
		{
			return ::fast_io::details::print_rsv_fp_append_point_zeros<comma>(iter, precision);
		}
		else
		{
			return iter;
		}
	}
}

// The slow dispatcher owns exact-window probes, fixed/scientific direct writers
// and the full-expansion fallback.  Keep that large control-flow graph outside
// the public precision entry so successful shortest-carrier and zero paths do
// not inherit its frame or front-end footprint.  noinline changes layout only.
template <typename flt, bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode,
	::fast_io::manipulators::floating_rounding rounding,
	bool json_float, ::std::integral char_type>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
inline constexpr char_type *print_rsvflt_precision_slow_path_impl(
	char_type *iter, typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t precision, bool negative,
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> rounded_m10 = {},
	::std::int_least32_t rounded_e10 = {}, ::std::size_t requested = {},
	::std::size_t = {}) noexcept
{
	// `requested` selects native-u128 DA shortcuts below.  Native MSVC does not
	// provide that integer type and therefore compiles every such block out;
	// retain the shared signature while making the intentionally unused parameter
	// explicit for `/W4 /WX` builds.
	(void)requested;
	// Equal decimal lengths do not prove that the shortest carrier is the
	// correctly rounded P-digit coefficient.  The shortest interval may select a
	// carrier on the opposite side of the exact value from the P-digit rounding
	// result; for example, a 16-digit binary64 shortest carrier can end in 5 while
	// the exact P16 coefficient ends in 4.  The public entry has already accepted
	// every carrier justified by a strict distance bound.  All remaining equal-
		// length cases must therefore reach the DA interval proof or the exact
		// prefix/guard/sticky fallback below.

		// Preserving scientific P16-P22 can consume the proved coefficient without
	// first materializing a numeric digit window.  Probe it at the former equal-
	// length exit: this both repairs that exit's invalid rounding assumption and
	// retains its short frame/dependency path.  The fused writer emits only after
	// the ambiguity interval succeeds, so an exact or possible tie still falls
	// through without modifying the destination.
#if defined(__SIZEOF_INT128__)
	if constexpr (::fast_io::details::dragonbox_uses_binary64_core<flt> &&
		format == ::fast_io::manipulators::floating_format::scientific &&
		::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode> &&
		::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		auto const significant{
			::fast_io::details::floating_precision_is_fractional<precision_mode>
				? ::fast_io::details::exact_precision_saturating_add(precision, 1u)
				: (precision ? precision : 1u)};
		if (exponent && significant - 16u < 4u)
		{
			auto const da_result{::fast_io::details::
				print_rsvflt_binary64_scientific_precision_runtime_impl<
					comma, uppercase_e>(iter,
						static_cast<::std::uint_least64_t>(mantissa), exponent,
						significant)};
			if (da_result)
			{
				return da_result;
			}
		}
		else if (exponent && significant - 20u < 3u)
		{
			auto const da_result{::fast_io::details::
				print_rsvflt_binary64_scientific_wide_precision_runtime_impl<
					comma, uppercase_e>(iter,
						static_cast<::std::uint_least64_t>(mantissa), exponent,
						significant)};
			if (da_result)
			{
				return da_result;
			}
		}
	}
#endif

	// P16-P19 have no compact all-presentation path elsewhere.  P20-P22 reuse
	// this path for general, fixed and decimal.  Non-preserving fractional
	// scientific output also uses it with P+1 significant digits, then trims the
	// numeric coefficient.  Significant scientific and preserving scientific
	// retain their measured fused writers, which avoid numeric-digit
	// materialization.  This is a presentation code-generation split only: every
	// accepted carrier below and in those writers comes from the same DA interval
	// proof, and every rejection reaches the same exact fallback.
#if defined(__SIZEOF_INT128__)
	if constexpr (::fast_io::details::dragonbox_uses_binary64_core<flt> &&
		(::fast_io::details::floating_precision_is_significant<precision_mode> ||
		 (format == ::fast_io::manipulators::floating_format::scientific &&
		  ::fast_io::details::floating_precision_is_fractional<precision_mode>)) &&
		::fast_io::details::floating_rounding_is_nearest<rounding> &&
		!(format == ::fast_io::manipulators::floating_format::scientific &&
		  ::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>))
	{
		auto const da_significant{
			::fast_io::details::floating_precision_is_fractional<precision_mode>
				? ::fast_io::details::exact_precision_saturating_add(precision, 1u)
				: precision};
		auto const p16_p19{da_significant - 16u < 4u};
		auto const p20_p22_generic{
			da_significant - 20u < 3u &&
			(format != ::fast_io::manipulators::floating_format::scientific ||
			 ::fast_io::details::floating_precision_is_fractional<precision_mode>)};
		if (exponent && (p16_p19 || p20_p22_generic))
		{
			::fast_io::details::binary64_p16_p22_significant_decimal decimal{};
			if (::fast_io::details::
				materialize_binary64_p16_p22_significant_decimal(
					decimal, static_cast<::std::uint_least64_t>(mantissa),
					exponent, da_significant))
			{
				if constexpr (!::fast_io::details::
					floating_precision_preserves_trailing_zero<precision_mode>)
				{
					::fast_io::details::exact_precision_trim(decimal);
				}
				return ::fast_io::details::print_rsvflt_rounded_precision_define_impl<
					double, comma, uppercase_e, format, precision_mode, json_float>(
						iter, decimal, precision, da_significant);
			}
		}
	}
#endif

	// Two measured semantics reuse complete DA coefficients.  General, fixed and
	// decimal significant-preserve use P23-P38 and avoid the full exact expansion
	// while retaining every requested trailing zero.  Scientific non-preserving
	// fractional P22-P37 requests P23-P38 significant digits, then trims exactly
	// as its existing renderer requires.  Paired M4 and x86-64 GCC/Clang runs
	// improve P23-P33 by roughly 30--53%; common-corpus M4 P34 improves by
	// 39--53%, and scientific fractional P33 improves by roughly 48--52%.  The
	// complete production P35-P38 extension was measured in one M4 process loading
	// baseline and candidate simultaneously: four forward/reverse-load rounds,
	// each with seven interleaved ABBA cycles, improved P35 by 41.7--60.3% and P38
	// by 24.5--35.7% across the ten admitted presentation modes.  Unchanged P34
	// controls stayed within 1.17% and P39 exact-fallback controls within 0.74%.
	// These paired controls justify the constant P34 and runtime P35-P38 split.
	// Preserving scientific output is deliberately excluded: its
	// existing fused writer avoids numeric-digit materialization, and replacing it
	// regressed paired M4 controls by 6--27%.
	// This split is presentation cost policy; every admitted success uses the same
	// one-sided DA interval proof, and every ambiguity rejection reaches the exact
	// fallback without having written output.
#if defined(__SIZEOF_INT128__)
	if constexpr (::std::same_as<flt, double> &&
		::fast_io::details::floating_rounding_is_nearest<rounding> &&
		((format != ::fast_io::manipulators::floating_format::scientific &&
		  ::fast_io::details::floating_precision_is_significant<precision_mode> &&
		  ::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>) ||
		 (format == ::fast_io::manipulators::floating_format::scientific &&
		  ::fast_io::details::floating_precision_is_fractional<precision_mode> &&
		  !::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>)))
	{
		auto const da_significant{
			::fast_io::details::floating_precision_is_fractional<precision_mode>
				? ::fast_io::details::exact_precision_saturating_add(precision, 1u)
				: (precision ? precision : 1u)};
		if (exponent && da_significant - 23u < 11u)
		{
			auto const da_result{::fast_io::details::
				print_rsvflt_binary64_common_precision_decimal_dispatch<
					comma, uppercase_e, format, precision_mode, json_float>(
						iter, static_cast<::std::uint_least64_t>(mantissa), exponent,
						da_significant, precision)};
			if (da_result)
			{
				return da_result;
			}
		}
		else if (exponent && da_significant == 34u)
		{
			auto const da_result{::fast_io::details::
				print_rsvflt_binary64_p34_precision_dispatch<
					comma, uppercase_e, format, json_float, precision_mode>(
						iter, static_cast<::std::uint_least64_t>(mantissa), exponent)};
			if (da_result)
			{
				return da_result;
			}
		}
		else if (exponent && da_significant - 35u < 4u)
		{
			auto const da_result{::fast_io::details::
				print_rsvflt_binary64_p35_p38_precision_dispatch<
					comma, uppercase_e, format, json_float, precision_mode>(
						iter, static_cast<::std::uint_least64_t>(mantissa), exponent,
						da_significant)};
			if (da_result)
			{
				return da_result;
			}
		}
	}
#endif
	// x86-64 and unmeasured targets defer the P23-P38 branches until this
	// already-outlined slow dispatcher.  Apple AArch64 performs the identical
	// probe in the public entry and compile-time removal prevents a duplicate.
	// The caller has emitted the sign and rejected zero/non-finite values.
#if defined(__SIZEOF_INT128__)
	if constexpr (!::fast_io::details::
		binary64_common_significant_precision_public_dispatch &&
		::std::same_as<flt, double> &&
		precision_mode == ::fast_io::manipulators::floating_precision::significant &&
		::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		if (exponent && precision - 23u < 11u)
		{
			auto const da_result{::fast_io::details::
				print_rsvflt_binary64_common_significant_precision_dispatch<
					comma, uppercase_e, format, json_float>(iter,
						static_cast<::std::uint_least64_t>(mantissa),
						exponent, precision)};
			if (da_result)
			{
				return da_result;
			}
		}
		else if (exponent && precision == 34u)
		{
			auto const da_result{::fast_io::details::
				print_rsvflt_binary64_p34_precision_dispatch<
					comma, uppercase_e, format, json_float>(iter,
						static_cast<::std::uint_least64_t>(mantissa), exponent)};
			if (da_result)
			{
				return da_result;
			}
		}
		else if (exponent && precision - 35u < 4u)
		{
			auto const da_result{::fast_io::details::
				print_rsvflt_binary64_p35_p38_precision_dispatch<
					comma, uppercase_e, format, json_float>(iter,
						static_cast<::std::uint_least64_t>(mantissa), exponent,
						precision)};
			if (da_result)
			{
				return da_result;
			}
		}
		else if constexpr (format ==
			::fast_io::manipulators::floating_format::scientific &&
			::fast_io::details::binary64_scientific_p20_p22_direct)
		{
			if (exponent && precision - 20u < 3u)
			{
				auto const da_result{::fast_io::details::
					print_rsvflt_binary64_scientific_p20_p22_significant_dispatch<
						comma, uppercase_e, json_float>(iter,
							static_cast<::std::uint_least64_t>(mantissa),
							exponent, precision)};
				if (da_result)
				{
					return da_result;
				}
			}
		}
	}
#endif
	// The production fixed-fractional DA helper uses one native-u128 result and
	// normalization contract for the complete K16..K28 triangle.  K20..K28 also
	// require the width for their coefficient; K16..K19 could fit u64 but are not
	// exposed as a second partial backend.  Targets without native u128 skip this
	// optimization and retain the exact algorithm.  Rounding, punctuation,
	// character type and EBCDIC behavior are unchanged; this capability guard is
	// not an ISA-specific rounding rule.
#if defined(__SIZEOF_INT128__)
	if constexpr (::std::same_as<flt, double> &&
		::fast_io::details::floating_rounding_is_nearest<rounding> &&
		::fast_io::details::floating_precision_is_fractional<precision_mode> &&
		::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode> &&
		(format == ::fast_io::manipulators::floating_format::fixed ||
		 format == ::fast_io::manipulators::floating_format::decimal))
	{
		// `requested` is K = r+1+P for this fixed/decimal precision request.
		// Qualify only the portable region which improved by at least five percent
		// in every paired M4, x86-GCC and x86-Clang row.  P15-P17 intentionally
		// remain on the established exact-scaled path.  The callee recomputes K
		// from the DA interval, so a shortest carrier at a power-of-ten boundary
		// can only cause a safe fallback, never select the wrong decimal scale.
		// Keep the two measured intervals explicit.  A zero-based single-range
		// spelling is mathematically equivalent, but Apple Clang 23 extends the
		// slow dispatcher's live ranges around the DA calls and regresses the
		// low-P accepted rows; the source-level union produces the shorter M4 hit
		// path.  This is a branch-layout policy, not an additional numeric domain.
		auto const portable_triangle{
			(4u <= precision && precision <= 13u &&
			 16u <= requested && requested <= precision + 15u) ||
			(precision == 14u && 16u <= requested && requested <= 28u)};
		if (exponent && portable_triangle)
		{
			// The public precision entry emitted the sign before calling this slow
			// dispatcher.  Pass the magnitude iterator unchanged; `negative` remains
			// relevant only to exact fallback rounding after a DA rejection.
			auto const da_result{requested <= 19u
				? ::fast_io::details::
					print_rsvflt_binary64_fixed_fractional_da_narrow_impl<comma>(
						iter, static_cast<::std::uint_least64_t>(mantissa),
						exponent, precision)
				: ::fast_io::details::
					print_rsvflt_binary64_fixed_fractional_da_wide_impl<comma>(
						iter, static_cast<::std::uint_least64_t>(mantissa),
						exponent, precision)};
			if (da_result)
			{
				// P>=4 guarantees an explicit decimal point, which also satisfies
				// json_float without a separate ".0" suffix.
				return da_result;
			}
		}
	}
#endif
	if constexpr (::fast_io::details::floating_precision_is_fractional<precision_mode> &&
		format != ::fast_io::manipulators::floating_format::scientific)
	{
		using trait = ::fast_io::details::iec559_traits<flt>;
		constexpr ::std::uint_least32_t exponent_bias{
			(static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u};
		if (!precision && exponent < exponent_bias)
		{
			bool round_up{};
			if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
			{
				constexpr auto half_exponent{exponent_bias - 1u};
				if (half_exponent < exponent ||
					(exponent == half_exponent &&
					 (mantissa || ::fast_io::details::print_rsv_fp_decimal_tie_round_up<rounding>(
								 negative, 0u))))
				{
					round_up = true;
				}
			}
			else
			{
				round_up =
					::fast_io::details::floating_rounding_directed_round_up<rounding>(negative);
			}
			if (round_up)
			{
				return ::fast_io::details::print_rsv_fp_precision_decision_impl<
					flt, comma, uppercase_e, format, precision_mode, rounding, json_float>(
						iter,
						static_cast<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>(1u),
						0, precision, negative);
			}
			return ::fast_io::details::print_rsvflt_fractional_tiny_zero_impl<
				flt, comma, uppercase_e, format, precision_mode, json_float>(iter, precision);
		}
	}

	::fast_io::details::dragonbox_decimal_mantissa_type<flt> slow_m10{};
	::std::int_least32_t slow_e10{};
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		slow_m10 = rounded_m10;
		slow_e10 = rounded_e10;
	}
	else
	{
		auto const converted{::fast_io::details::dragonbox_impl<
			flt, ::fast_io::manipulators::floating_rounding::nearest_to_even>(
			mantissa, static_cast<::std::int_least32_t>(exponent), negative)};
		slow_m10 = converted.m10;
		slow_e10 = converted.e10;
	}
	constexpr bool exact_carrier_output_supported{
		!::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode> ||
		format == ::fast_io::manipulators::floating_format::scientific ||
		(::fast_io::details::floating_precision_is_fractional<precision_mode> &&
		 format != ::fast_io::manipulators::floating_format::general)};
	if constexpr (exact_carrier_output_supported)
	{
		if (::fast_io::details::dragonbox_decimal_carrier_is_binary_exact<flt>(
				mantissa, exponent, slow_m10, slow_e10))
		{
			return ::fast_io::details::print_rsv_fp_precision_decision_impl<
				flt, comma, uppercase_e, format, precision_mode, rounding, json_float>(
				iter, slow_m10, slow_e10, precision, negative);
		}
	}
	else if constexpr (::fast_io::details::floating_precision_is_fractional<precision_mode> &&
		format == ::fast_io::manipulators::floating_format::general)
	{
		// General formatting does not append fractional zeroes.  An exact carrier
		// is therefore complete once the requested quantum is no coarser than its
		// final digit; coarser requests must still use the exact rounding path.
		auto const carrier_is_fine_enough{0 <= slow_e10 ||
			precision >= static_cast<::std::size_t>(-static_cast<::std::int_least64_t>(slow_e10))};
		if (carrier_is_fine_enough &&
			::fast_io::details::dragonbox_decimal_carrier_is_binary_exact<flt>(
				mantissa, exponent, slow_m10, slow_e10))
		{
			return ::fast_io::details::print_rsv_fp_precision_decision_impl<
				flt, comma, uppercase_e, format, precision_mode, rounding, json_float>(
					iter, slow_m10, slow_e10, precision, negative);
		}
	}
	// Positive binary64 integer detection uses the u128 exact-window helpers
	// to avoid decimal materialization before fixed/decimal punctuation.
	// Without u128, the subsequent exact path applies the same rounding and
	// trailing-zero policy.
#if defined(__SIZEOF_INT128__)
	if constexpr (::std::same_as<flt, double> &&
		::fast_io::details::floating_precision_is_fractional<precision_mode> &&
		(format == ::fast_io::manipulators::floating_format::fixed ||
		 format == ::fast_io::manipulators::floating_format::decimal))
	{
		auto positive_integer{
			::fast_io::details::exact_precision_window_try_print_positive_integer<flt>(
				iter, mantissa, exponent)};
		if (positive_integer)
		{
			if constexpr (::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>)
			{
				positive_integer = ::fast_io::details::print_rsv_fp_append_point_zeros<comma>(
					positive_integer, precision);
				if constexpr (json_float)
				{
					if (!precision)
					{
						return ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(
							positive_integer);
					}
				}
				return positive_integer;
			}
			else
			{
				if constexpr (json_float)
				{
					return ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(
						positive_integer);
				}
				return positive_integer;
			}
		}
	}
#endif
	if constexpr (!::fast_io::details::floating_rounding_is_nearest<rounding> &&
				  !(format == ::fast_io::manipulators::floating_format::general &&
					::fast_io::details::floating_precision_is_fractional<precision_mode>))
	{
		auto const slow_length{static_cast<::std::size_t>(chars_len<10, true>(slow_m10))};
		::std::size_t carrier_requested{};
		if constexpr (::fast_io::details::floating_precision_is_fractional<precision_mode>)
		{
			if constexpr (format == ::fast_io::manipulators::floating_format::scientific)
			{
				carrier_requested = ::fast_io::details::exact_precision_saturating_add(precision, 1u);
			}
			else
			{
				auto const real_exponent{
					slow_e10 + static_cast<::std::int_least32_t>(slow_length) - 1};
				if (0 <= real_exponent)
				{
					carrier_requested = ::fast_io::details::exact_precision_saturating_add(
						::fast_io::details::exact_precision_saturating_add(
							static_cast<::std::size_t>(real_exponent), precision),
						1u);
				}
				else
				{
					auto const leading_fractional_zeros{
						static_cast<::std::size_t>(-real_exponent)};
					if (leading_fractional_zeros <= precision)
					{
						carrier_requested = precision - leading_fractional_zeros + 1u;
					}
				}
			}
		}
		else
		{
			carrier_requested = precision ? precision : 1u;
		}
		if (carrier_requested && carrier_requested < slow_length)
		{
			auto const cut{slow_length - carrier_requested};
			if (cut < 20u)
			{
				auto const carrier_result{
					::fast_io::details::print_rsv_fp_try_directed_carrier_decision_impl<
						flt, comma, uppercase_e, format, precision_mode, rounding, json_float>(
						iter, slow_m10, slow_e10, precision, negative,
						static_cast<::std::uint_least32_t>(cut))};
				if (carrier_result)
				{
					return carrier_result;
				}
			}
		}
	}
	if constexpr (::fast_io::details::floating_precision_is_fractional<precision_mode> &&
			  format != ::fast_io::manipulators::floating_format::scientific)
	{
		constexpr auto int32_max{(::std::numeric_limits<::std::int_least32_t>::max)()};
		if (precision <= static_cast<::std::size_t>(int32_max))
		{
			auto const slow_length{static_cast<::std::int_least32_t>(chars_len<10, true>(slow_m10))};
			auto const slow_real_exponent{slow_e10 + slow_length - 1};
			auto const quantum_distance{static_cast<::std::int_least64_t>(slow_real_exponent) +
				static_cast<::std::int_least64_t>(precision)};
			if (quantum_distance <= -2)
			{
				if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
				{
					return ::fast_io::details::print_rsvflt_fractional_tiny_zero_impl<
						flt, comma, uppercase_e, format, precision_mode, json_float>(iter, precision);
				}
				else if (::fast_io::details::floating_rounding_directed_round_up<rounding>(negative))
				{
					return ::fast_io::details::print_rsv_fp_precision_decision_impl<
						flt, comma, uppercase_e, format, precision_mode, rounding, json_float>(
						iter, static_cast<::fast_io::details::dragonbox_decimal_mantissa_type<flt>>(1u),
						-static_cast<::std::int_least32_t>(precision), precision, negative);
				}
				else
				{
					return ::fast_io::details::print_rsvflt_fractional_tiny_zero_impl<
						flt, comma, uppercase_e, format, precision_mode, json_float>(iter, precision);
				}
				}
			}
		}
		// The compact prefix/guard/sticky window for binary32/binary64 uses native
		// u128 limbs.  Targets without that type bypass these direct and boundary-
		// carrier optimizations and enter the representation-independent slow path;
		// no value is converted through a narrower floating type.
#if defined(__SIZEOF_INT128__)
		if constexpr (::std::same_as<flt, double> || ::std::same_as<flt, float>)
	{
		auto const slow_length{static_cast<::std::int_least32_t>(chars_len<10, true>(slow_m10))};
		auto slow_real_exponent{slow_e10 + slow_length - 1};
		if constexpr (::std::same_as<flt, float> &&
			format == ::fast_io::manipulators::floating_format::scientific)
		{
			if (15u <= precision && slow_m10 == 1u)
			{
				slow_real_exponent = ::fast_io::details::
					exact_precision_correct_binary32_raw_negative_real_exponent(
						static_cast<::std::uint_least32_t>(mantissa), exponent,
						slow_real_exponent);
			}
		}
		if constexpr (::std::same_as<flt, double> &&
			format == ::fast_io::manipulators::floating_format::scientific &&
			::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode> &&
			::fast_io::details::exact_precision_window_direct_boundary_carrier)
		{
			// A carrier equal to one is the only shortest form that can have
			// rounded across a negative power-of-ten boundary. Send just this
			// uncommon case through the direct stream; GCC keeps the general
			// negative-exponent path on the lower-pressure compact window.
			if (15u <= precision && slow_m10 == 1u && slow_real_exponent < 0)
			{
				using trait = ::fast_io::details::iec559_traits<double>;
				auto binary_mantissa{static_cast<::std::uint_least64_t>(mantissa)};
				::std::int_least32_t binary_exponent{};
				if (exponent)
				{
					binary_mantissa |= static_cast<::std::uint_least64_t>(1ULL) << trait::mbits;
					constexpr ::std::int_least32_t bias{
						(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
					binary_exponent = static_cast<::std::int_least32_t>(exponent) - bias -
						static_cast<::std::int_least32_t>(trait::mbits);
				}
				else
				{
					binary_exponent = 1 -
						((static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1) -
						static_cast<::std::int_least32_t>(trait::mbits);
				}
				auto const significant{
					::fast_io::details::floating_precision_is_fractional<precision_mode>
						? ::fast_io::details::exact_precision_saturating_add(precision, 1u)
						: (precision ? precision : 1u)};
				auto const direct_result{::fast_io::details::
					exact_precision_window_try_print_negative_scientific<
						flt, comma, uppercase_e, rounding>(iter, binary_mantissa,
							binary_exponent, slow_real_exponent, significant, negative)};
				if (direct_result)
				{
					return direct_result;
				}
			}
		}
		if constexpr (::std::same_as<flt, float> &&
			format == ::fast_io::manipulators::floating_format::scientific &&
			::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode> &&
			::fast_io::details::exact_precision_window_direct_binary32_positive_scientific)
		{
			if (6u <= precision)
			{
				using trait = ::fast_io::details::iec559_traits<float>;
				auto binary_mantissa{static_cast<::std::uint_least32_t>(mantissa)};
				::std::int_least32_t binary_exponent{};
				if (exponent)
				{
					binary_mantissa |= static_cast<::std::uint_least32_t>(1u) << trait::mbits;
					constexpr ::std::int_least32_t bias{
						(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
					binary_exponent = static_cast<::std::int_least32_t>(exponent) - bias -
						static_cast<::std::int_least32_t>(trait::mbits);
				}
				else
				{
					binary_exponent = 1 -
						((static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1) -
						static_cast<::std::int_least32_t>(trait::mbits);
				}
				if (0 <= binary_exponent)
				{
					auto const significant{
						::fast_io::details::floating_precision_is_fractional<precision_mode>
							? ::fast_io::details::exact_precision_saturating_add(precision, 1u)
							: (precision ? precision : 1u)};
					auto const direct_result{::fast_io::details::
						exact_precision_window_try_print_positive_binary32_scientific<
							flt, comma, uppercase_e, rounding>(iter, binary_mantissa,
								static_cast<::std::uint_least32_t>(binary_exponent),
								significant, negative)};
					if (direct_result)
					{
						return direct_result;
					}
				}
			}
		}
		bool use_window{};
		if constexpr (::fast_io::details::floating_precision_is_fractional<precision_mode>)
		{
			if constexpr (format == ::fast_io::manipulators::floating_format::scientific)
			{
				use_window = precision < exact_precision_window_digit_capacity - 1u;
			}
			else if (precision <= static_cast<::std::size_t>(
									  (::std::numeric_limits<::std::int_least32_t>::max)()))
			{
				auto const requested_keep{static_cast<::std::int_least64_t>(slow_real_exponent) + 1 +
										  static_cast<::std::int_least64_t>(precision)};
				use_window = 0 <= requested_keep &&
							 static_cast<::std::uint_least64_t>(requested_keep) <
								 exact_precision_window_digit_capacity;
			}
		}
		else
		{
			auto const significant{precision ? precision : 1u};
			use_window = significant < exact_precision_window_digit_capacity;
		}
		if constexpr (::std::same_as<flt, float>)
		{
			use_window = use_window && slow_real_exponent < 0;
		}
		if (use_window)
		{
			bool split_direct{};
			if constexpr (format == ::fast_io::manipulators::floating_format::scientific &&
				::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>)
			{
				auto const split_significant{
					::fast_io::details::floating_precision_is_fractional<precision_mode>
						? ::fast_io::details::exact_precision_saturating_add(precision, 1u)
						: (precision ? precision : 1u)};
				auto const long_direct{35u <= split_significant};
				constexpr ::std::size_t direct_scientific_minimum_precision{
					::std::same_as<flt, float> ? 6u : 15u};
				if (precision < direct_scientific_minimum_precision)
				{
					split_direct = false;
				}
				else if (slow_real_exponent < 0)
				{
					split_direct = ::fast_io::details::
						exact_precision_window_direct_negative_scientific;
				}
				else
				{
					using trait = ::fast_io::details::iec559_traits<flt>;
					constexpr ::std::uint_least32_t nonnegative_binary_exponent_field{
						((static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u) +
						trait::mbits};
					split_direct = exponent < nonnegative_binary_exponent_field
						? (::fast_io::details::exact_precision_window_direct_mixed_scientific ||
							(long_direct && ::fast_io::details::
								exact_precision_window_long_mixed_scientific))
						: (::fast_io::details::exact_precision_window_direct_nonnegative_scientific ||
							(long_direct && ::fast_io::details::
								exact_precision_window_long_nonnegative_scientific));
				}
			}
			if (split_direct)
			{
				auto const direct_result{
					::fast_io::details::print_rsvflt_exact_precision_window_dispatch_impl<flt, comma,
						uppercase_e, format, precision_mode, rounding, json_float, true, false>(
							iter, mantissa, exponent, precision, negative, slow_real_exponent)};
				if (direct_result)
				{
					return direct_result;
				}
				auto const window_result{
					::fast_io::details::print_rsvflt_exact_precision_window_dispatch_impl<flt, comma,
						uppercase_e, format, precision_mode, rounding, json_float, false, true>(
							iter, mantissa, exponent, precision, negative, slow_real_exponent)};
				if (window_result)
				{
					return window_result;
				}
			}
			else
			{
				auto const window_result{
					::fast_io::details::print_rsvflt_exact_precision_window_dispatch_impl<flt, comma,
						uppercase_e, format, precision_mode, rounding, json_float, false, false>(
							iter, mantissa, exponent, precision, negative, slow_real_exponent)};
				if (window_result)
				{
					return window_result;
				}
			}
		}
	}
#endif
	return ::fast_io::details::print_rsvflt_exact_precision_define_impl<
		flt, comma, uppercase_e, format, precision_mode, rounding, json_float>(
		iter, mantissa, exponent, precision, negative);
}

// Recovering the real decimal exponent is independent of destination character,
// punctuation, notation and rounding policy.  Nearest-policy callers already
// own the shortest carrier and pass it through; directed-policy callers request
// the same nearest-even carrier that the unchanged slow body would construct.
// Keeping this conversion in one outlined binary64 function prevents the full
// semantic wrapper matrix from cloning Dragonbox while preserving the exact
// exponent used by the baseline path.
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
[[nodiscard]] inline constexpr ::std::int_least32_t
exact_precision_window_binary64_real_exponent(
	::std::uint_least64_t mantissa, ::std::uint_least32_t exponent,
	bool negative, ::std::uint_least64_t rounded_m10,
	::std::int_least32_t rounded_e10, bool carrier_available) noexcept
{
	if (!carrier_available)
	{
		auto const converted{::fast_io::details::dragonbox_impl<
			double, ::fast_io::manipulators::floating_rounding::nearest_to_even>(
				mantissa, static_cast<::std::int_least32_t>(exponent), negative)};
		rounded_m10 = converted.m10;
		rounded_e10 = converted.e10;
	}
	auto const length{static_cast<::std::int_least32_t>(
		::fast_io::details::chars_len<10, true>(rounded_m10))};
	return rounded_e10 + length - 1;
}

// The selector is inline at the public entry's existing slow-call site.  Thus a
// P1--P63 miss adds only one comparison before the original direct call, while
// the large slow body remains byte-identical and keeps its baseline register
// allocation.  P64--P128 calls the shared exponent/direct writers first.  All
// excluded targets discard the branch at compile time and instantiate exactly
// the original call.
template <typename flt, bool comma, bool uppercase_e,
	::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode,
	::fast_io::manipulators::floating_rounding rounding,
	bool json_float, ::std::integral char_type>
inline constexpr char_type *print_rsvflt_precision_slow_path_select_impl(
	char_type *iter, typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t precision, bool negative,
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> rounded_m10 = {},
	::std::int_least32_t rounded_e10 = {}, ::std::size_t requested = {},
	::std::size_t rounded_length = {}) noexcept
{
	// The direct significant-fixed writer is represented with native unsigned
	// 128-bit intermediates, and both its policy constant and implementation are
	// deliberately declared only inside the same capability gate.  Keep name
	// lookup inside that gate as well: MSVC has no __int128 scalar type and must
	// parse only the exact limb-based fallback below.  On native-u128 frontends
	// preprocessing removes no tokens from the former hot path, so its dispatch,
	// arithmetic and generated code remain unchanged.
#if defined(__SIZEOF_INT128__)
	if constexpr (::fast_io::details::binary64_significant_fixed_direct_block &&
		::std::same_as<flt, double> &&
		!::fast_io::details::floating_precision_is_fractional<precision_mode> &&
		::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode> &&
		(format == ::fast_io::manipulators::floating_format::fixed ||
		 format == ::fast_io::manipulators::floating_format::decimal))
	{
		if (64u <= precision) [[unlikely]]
		{
			constexpr bool carrier_available{
				::fast_io::details::floating_rounding_is_nearest<rounding>};
			auto const real_exponent{::fast_io::details::
				exact_precision_window_binary64_real_exponent(
					static_cast<::std::uint_least64_t>(mantissa), exponent, negative,
					static_cast<::std::uint_least64_t>(rounded_m10), rounded_e10,
					carrier_available)};
			auto const exact_result{::fast_io::details::
				exact_precision_window_try_print_significant_fixed<comma>(iter,
					static_cast<::std::uint_least64_t>(mantissa), exponent,
					real_exponent, precision, negative, json_float,
					format == ::fast_io::manipulators::floating_format::decimal,
					rounding)};
			if (exact_result)
			{
				return exact_result;
			}
		}
	}
#endif
	return ::fast_io::details::print_rsvflt_precision_slow_path_impl<
		flt, comma, uppercase_e, format, precision_mode, rounding, json_float>(
			iter, mantissa, exponent, precision, negative, rounded_m10,
			rounded_e10, requested, rounded_length);
}

/*
 Binary80 and binary128 do not use the binary32/binary64 shortest-carrier
 precision dispatcher below.  Its Schubfach cache and fixed-width endpoint
 arithmetic are deliberately specialized for the narrow exponent domains;
 forcing a wide type through that route both wastes work and can exceed those
 arithmetic preconditions.  A constrained overload keeps the ordinary public
 entry byte-identical while giving wide types the complete exact algorithm and
 the proved subnormal window.  The split is a type-domain distinction, not an
 ISA or compiler selection.
 */
template <bool showpos, bool uppercase, bool uppercase_e, bool comma,
	::fast_io::manipulators::floating_format mt,
	::fast_io::manipulators::floating_precision precision_mode =
		::fast_io::manipulators::floating_precision::significant,
	::fast_io::manipulators::floating_rounding rounding =
		::fast_io::manipulators::floating_rounding::nearest_to_even,
	bool nan_show_sign = true, bool nan_show_type = false,
	bool json_float = false, typename flt, ::std::integral char_type>
	requires(::fast_io::details::exact_precision_is_wide_binary<flt>)
inline constexpr char_type *print_rsvflt_precision_define_impl(
	char_type *iter, flt f, ::std::size_t precision) noexcept
{
	if constexpr (rounding ==
		::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return print_rsvflt_precision_define_impl<
				showpos, uppercase, uppercase_e, comma, mt, precision_mode,
				::fast_io::manipulators::floating_rounding::toward_plus_infinity,
				nan_show_sign, nan_show_type, json_float>(iter, f, precision);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return print_rsvflt_precision_define_impl<
				showpos, uppercase, uppercase_e, comma, mt, precision_mode,
				::fast_io::manipulators::floating_rounding::toward_minus_infinity,
				nan_show_sign, nan_show_type, json_float>(iter, f, precision);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return print_rsvflt_precision_define_impl<
				showpos, uppercase, uppercase_e, comma, mt, precision_mode,
				::fast_io::manipulators::floating_rounding::toward_zero,
				nan_show_sign, nan_show_type, json_float>(iter, f, precision);
		default:
			return print_rsvflt_precision_define_impl<
				showpos, uppercase, uppercase_e, comma, mt, precision_mode,
				::fast_io::manipulators::floating_rounding::nearest_to_even,
				nan_show_sign, nan_show_type, json_float>(iter, f, precision);
		}
	}
	if constexpr (::fast_io::manipulators::floating_format::fixed == mt &&
		uppercase_e)
	{
		return print_rsvflt_precision_define_impl<
			showpos, uppercase, false, comma, mt, precision_mode, rounding,
			nan_show_sign, nan_show_type, json_float>(iter, f, precision);
	}
	else
	{
		using trait = iec559_traits<flt>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr ::std::size_t mbits{trait::mbits};
		constexpr ::std::size_t ebits{trait::ebits};
		constexpr mantissa_type exponent_mask{
			(static_cast<mantissa_type>(1) << ebits) - 1};
		constexpr ::std::uint_least32_t exponent_mask_u32{
			static_cast<::std::uint_least32_t>(exponent_mask)};
		auto [mantissa, exponent, sign] = get_punned_result(f);
		if (exponent == exponent_mask_u32)
		{
			return prsv_fp_nan_impl<showpos, uppercase, nan_show_sign,
				nan_show_type, mbits>(iter, mantissa, sign);
		}
		iter = print_rsv_fp_sign_impl<showpos>(iter, sign);
		if (!mantissa && !exponent)
		{
			if constexpr (mt ==
				::fast_io::manipulators::floating_format::scientific)
			{
				*iter = char_literal_v<u8'0', char_type>;
				++iter;
				if constexpr (::fast_io::details::
					floating_precision_preserves_trailing_zero<precision_mode>)
				{
					if constexpr (::fast_io::details::
						floating_precision_is_significant<precision_mode>)
					{
						precision = precision ? precision - 1u : 0u;
					}
					if (precision)
					{
						*iter = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
						++iter;
						iter = ::fast_io::details::fill_zeros_impl(iter, precision);
					}
				}
				return print_rsv_fp_e_impl<flt, uppercase_e>(iter, 0);
			}
			else if constexpr (::fast_io::details::
				floating_precision_is_fractional<precision_mode>)
			{
				*iter = char_literal_v<u8'0', char_type>;
				++iter;
				if constexpr (json_float)
				{
					if (!precision || !::fast_io::details::
						floating_precision_preserves_trailing_zero<precision_mode>)
					{
						return ::fast_io::details::
							print_rsv_fp_append_json_float_zero<comma>(iter);
					}
				}
				if constexpr (::fast_io::details::
					floating_precision_preserves_trailing_zero<precision_mode>)
				{
					return ::fast_io::details::print_rsv_fp_append_point_zeros<comma>(
						iter, precision);
				}
				return iter;
			}
			else if constexpr (precision_mode == ::fast_io::manipulators::
				floating_precision::significant_preserve_trailing_zero)
			{
				*iter = char_literal_v<u8'0', char_type>;
				++iter;
				if constexpr (json_float)
				{
					if (precision <= 1u)
					{
						return ::fast_io::details::
							print_rsv_fp_append_json_float_zero<comma>(iter);
					}
				}
				return ::fast_io::details::print_rsv_fp_append_point_zeros<comma>(
					iter, precision ? precision - 1u : 0u);
			}
			else
			{
				*iter = char_literal_v<u8'0', char_type>;
				++iter;
				if constexpr (json_float)
				{
					return ::fast_io::details::
						print_rsv_fp_append_json_float_zero<comma>(iter);
				}
				return iter;
			}
		}
		if constexpr (::fast_io::details::
			floating_precision_is_fractional<precision_mode> &&
			mt != ::fast_io::manipulators::floating_format::scientific)
		{
			/*
			 The upper bound proves |x| < 0.1*10^-P.  Every nearest policy
			 therefore produces zero on the requested fractional grid; a
			 directed policy produces either zero or exactly one 10^-P quantum
			 according to sign and direction.  The proof is independent of the
			 significand width and avoids a full 11,000-digit expansion.
			 */
			if (::fast_io::details::
				exact_precision_fractional_tiny_wide_binary_bound<flt>(
					exponent, precision))
			{
				if constexpr (::fast_io::details::
					floating_rounding_is_nearest<rounding>)
				{
					return ::fast_io::details::print_rsvflt_fractional_tiny_zero_impl<
						flt, comma, uppercase_e, mt, precision_mode, json_float>(
							iter, precision);
				}
				else if (!::fast_io::details::
					floating_rounding_directed_round_up<rounding>(sign))
				{
					return ::fast_io::details::print_rsvflt_fractional_tiny_zero_impl<
						flt, comma, uppercase_e, mt, precision_mode, json_float>(
							iter, precision);
				}
				else
				{
					::fast_io::details::exact_precision_single_digit_decimal const quantum{
						{1u}, 1u, -static_cast<::std::int_least32_t>(precision)};
					return ::fast_io::details::print_rsvflt_rounded_precision_define_impl<
						flt, comma, uppercase_e, mt, precision_mode, json_float>(
							iter, quantum, precision, 0u);
				}
			}
		}
		// The interval implementation requires a native scalar capable of holding
		// the complete binary128 significand.  Other targets retain the exact path.
#if defined(__SIZEOF_INT128__)
		if (precision <= 128u)
		{
			if (auto window_end{
					::fast_io::details::exact_precision_wide_try_print<
						flt, comma, uppercase_e, json_float>(
							iter, mantissa, exponent, precision, sign,
							mt, precision_mode,
							rounding)})
			{
				return window_end;
			}
		}
#endif
		/*
		 Materializing the exact binary rational, rounding it on the requested
		 decimal grid and invoking the shared presentation layer is complete for
		 all four decimal formats, ten policies and four precision modes.
		 */
		return ::fast_io::details::print_rsvflt_exact_precision_define_impl<
			flt, comma, uppercase_e, mt, precision_mode, rounding, json_float>(
				iter, mantissa, exponent, precision, sign);
	}
}

template <bool showpos, bool uppercase, bool uppercase_e, bool comma, ::fast_io::manipulators::floating_format mt,
		  ::fast_io::manipulators::floating_precision precision_mode =
			  ::fast_io::manipulators::floating_precision::significant,
		  ::fast_io::manipulators::floating_rounding rounding =
			  ::fast_io::manipulators::floating_rounding::nearest_to_even,
		  bool nan_show_sign = true, bool nan_show_type = false, bool json_float = false,
		  typename flt, ::std::integral char_type>
inline constexpr char_type *print_rsvflt_precision_define_impl(
	char_type *iter, flt f, ::std::size_t precision) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return print_rsvflt_precision_define_impl<
				showpos, uppercase, uppercase_e, comma, mt, precision_mode,
				::fast_io::manipulators::floating_rounding::toward_plus_infinity,
				nan_show_sign, nan_show_type, json_float>(iter, f, precision);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return print_rsvflt_precision_define_impl<
				showpos, uppercase, uppercase_e, comma, mt, precision_mode,
				::fast_io::manipulators::floating_rounding::toward_minus_infinity,
				nan_show_sign, nan_show_type, json_float>(iter, f, precision);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return print_rsvflt_precision_define_impl<
				showpos, uppercase, uppercase_e, comma, mt, precision_mode,
				::fast_io::manipulators::floating_rounding::toward_zero,
				nan_show_sign, nan_show_type, json_float>(iter, f, precision);
		default:
			return print_rsvflt_precision_define_impl<
				showpos, uppercase, uppercase_e, comma, mt, precision_mode,
				::fast_io::manipulators::floating_rounding::nearest_to_even,
				nan_show_sign, nan_show_type, json_float>(iter, f, precision);
		}
	}
	if constexpr (::fast_io::manipulators::floating_format::fixed == mt && uppercase_e)
	{
		return print_rsvflt_precision_define_impl<showpos, uppercase, false, comma, mt, precision_mode,
												  rounding, nan_show_sign, nan_show_type, json_float>(iter, f,
																									  precision);
	}
	else
	{
		using trait = iec559_traits<flt>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr ::std::size_t mbits{trait::mbits};
		constexpr ::std::size_t ebits{trait::ebits};
		constexpr mantissa_type exponent_mask{(static_cast<mantissa_type>(1) << ebits) - 1};
		constexpr ::std::uint_least32_t exponent_mask_u32{static_cast<::std::uint_least32_t>(exponent_mask)};
		auto [mantissa, exponent, sign] = get_punned_result(f);
		if (exponent == exponent_mask_u32)
		{
			return prsv_fp_nan_impl<showpos, uppercase, nan_show_sign, nan_show_type, mbits>(iter, mantissa, sign);
		}
		if constexpr (::fast_io::details::dragonbox_uses_binary32_core<flt> && sizeof(flt) < sizeof(float))
		{
			auto const widened{
				::fast_io::details::dragonbox_narrow_float_from_fields<flt>(
					mantissa, exponent, sign)};
			return ::fast_io::details::print_rsvflt_precision_define_impl<
				showpos, uppercase, uppercase_e, comma, mt, precision_mode, rounding,
				nan_show_sign, nan_show_type, json_float>(iter, widened, precision);
		}
		iter = print_rsv_fp_sign_impl<showpos>(iter, sign);
		if (!mantissa && !exponent)
		{
			if constexpr (mt == ::fast_io::manipulators::floating_format::scientific)
			{
				*iter = char_literal_v<u8'0', char_type>;
				++iter;
				if constexpr (::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>)
				{
					if constexpr (::fast_io::details::floating_precision_is_significant<precision_mode>)
					{
						precision = precision ? precision - 1u : 0u;
					}
					if (precision)
					{
						*iter = char_literal_v<(comma ? u8',' : u8'.'), char_type>;
						++iter;
						iter = ::fast_io::details::fill_zeros_impl(iter, precision);
					}
				}
				return print_rsv_fp_e_impl<flt, uppercase_e>(iter, 0);
			}
			else if constexpr (::fast_io::details::floating_precision_is_fractional<precision_mode>)
			{
				*iter = char_literal_v<u8'0', char_type>;
				++iter;
				if constexpr (json_float)
				{
					if (!precision || !::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>)
					{
						return ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(iter);
					}
				}
				if constexpr (::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>)
				{
					return ::fast_io::details::print_rsv_fp_append_point_zeros<comma>(iter, precision);
				}
				return iter;
			}
			else if constexpr (precision_mode ==
							   ::fast_io::manipulators::floating_precision::significant_preserve_trailing_zero)
			{
				*iter = char_literal_v<u8'0', char_type>;
				++iter;
				if constexpr (json_float)
				{
					if (precision <= 1u)
					{
						return ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(iter);
					}
				}
				return ::fast_io::details::print_rsv_fp_append_point_zeros<comma>(
					iter, precision ? precision - 1u : 0u);
			}
			else
			{
				*iter = char_literal_v<u8'0', char_type>;
				++iter;
				if constexpr (json_float)
				{
					return ::fast_io::details::print_rsv_fp_append_json_float_zero<comma>(iter);
				}
				return iter;
			}
		}
		if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding> &&
			::fast_io::details::floating_precision_is_fractional<precision_mode> &&
			(mt == ::fast_io::manipulators::floating_format::fixed ||
				mt == ::fast_io::manipulators::floating_format::decimal))
		{
			// A nonzero magnitude with a raw exponent below bias - 1 is strictly
			// smaller than one half.  At zero fractional precision every nearest
			// policy therefore emits zero; decide this before either DA pipeline.
			constexpr ::std::uint_least32_t half_exponent{
				(static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 2u};
			if (!precision && exponent < half_exponent)
			{
				return ::fast_io::details::print_rsvflt_fractional_tiny_zero_impl<
					flt, comma, uppercase_e, mt, precision_mode, json_float>(iter, precision);
			}
			if constexpr (::std::same_as<flt, float> || ::std::same_as<flt, double>)
			{
				// Every binary32/binary64 subnormal and value in the smallest normal
				// exponent bin is below half of the decimal quantum through
				// -min_exponent10 fractional places. Avoid both decimal pipelines
				// when the rounded field is all zeroes.
				constexpr auto subnormal_zero_precision{static_cast<::std::size_t>(
					-(::std::numeric_limits<flt>::min_exponent10))};
				constexpr ::std::uint_least32_t exponent_bias{half_exponent + 1u};
				if (precision && exponent + 32u < exponent_bias)
				{
					if (exponent <= 1u && precision <= subnormal_zero_precision)
					{
						return ::fast_io::details::print_rsvflt_fractional_tiny_zero_impl<
							flt, comma, uppercase_e, mt, precision_mode, json_float>(iter, precision);
					}
					if (::fast_io::details::exact_precision_fractional_tiny_binary_bound<flt>(
							exponent, precision))
					{
						return ::fast_io::details::print_rsvflt_fractional_tiny_zero_impl<
							flt, comma, uppercase_e, mt, precision_mode, json_float>(iter, precision);
					}
				}
			}
		}
		// Direct binary32 power-of-ten scientific emission shares the u128 exact
		// window's guard/sticky machinery.  Without native u128 the generic exact
		// path remains authoritative; no precision is narrowed.
#if defined(__SIZEOF_INT128__)
		if constexpr (::std::same_as<flt, float> &&
			mt == ::fast_io::manipulators::floating_format::scientific &&
			::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode> &&
			::fast_io::details::exact_precision_window_direct_binary32_negative_scientific)
		{
			auto const significant{
				::fast_io::details::floating_precision_is_fractional<precision_mode>
					? ::fast_io::details::exact_precision_saturating_add(precision, 1u)
					: (precision ? precision : 1u)};
			if (significant < exact_precision_compact_window_digit_capacity)
			{
				auto const direct_result{::fast_io::details::
					exact_precision_try_print_binary32_power10_scientific<
						comma, uppercase_e, rounding>(iter,
							static_cast<::std::uint_least32_t>(mantissa), exponent,
							significant, sign)};
				if (direct_result)
				{
					return direct_result;
				}
			}
		}
#endif
		// Fractional `decimal` is normatively rendered as fixed before this point:
		// both modes round x*10^P, preserve the same P-place field and apply the
		// same JSON suffix rule.  They can therefore share the scaled/exact binary64
		// writer without changing a format decision.  The specialization requires
		// native u128 for mantissa*5^P and its exact remainder; targets without that
		// representation use the same exact precision fallback.
#if defined(__SIZEOF_INT128__)
		if constexpr (::std::same_as<flt, double> &&
			(mt == ::fast_io::manipulators::floating_format::fixed ||
			 mt == ::fast_io::manipulators::floating_format::decimal) &&
			precision_mode ==
				::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero)
		{
			// A normal binary64 with raw exponent at least bias + mantissa bits
			// has a nonnegative final binary exponent and is therefore an exact
			// integer.  For fractional fixed/decimal output its representation is
			// that integer followed by the requested decimal point and zero field;
			// no rounding policy can change those appended zeroes.  Admit this
			// proved subset at P1-P14 while retaining the existing P15+ window for
			// values with a fractional binary component.  P0 deliberately remains on
			// the generic path because its terminal renderer owns the JSON `.0` suffix
			// policy.  The terminal integer writer is already outlined and shared, so
			// this adds neither a table nor a second arithmetic implementation.
			constexpr ::std::uint_least32_t nonfractional_raw_exponent{
				((static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u) +
				static_cast<::std::uint_least32_t>(trait::mbits)};
			if (15u <= precision ||
				(precision && nonfractional_raw_exponent <= exponent))
			{
				auto binary_mantissa{static_cast<::std::uint_least64_t>(mantissa)};
				::std::int_least32_t binary_exponent{};
				if (exponent)
				{
					binary_mantissa |= static_cast<::std::uint_least64_t>(1ULL) << trait::mbits;
					constexpr ::std::int_least32_t bias{
						(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
					binary_exponent = static_cast<::std::int_least32_t>(exponent) - bias -
						static_cast<::std::int_least32_t>(trait::mbits);
				}
				else
				{
					binary_exponent = 1 -
						((static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1) -
						static_cast<::std::int_least32_t>(trait::mbits);
				}
				if (0 <= binary_exponent)
				{
					if constexpr (::fast_io::details::exact_precision_window_direct_positive_fixed)
					{
						auto positive_integer{::fast_io::details::
							exact_precision_window_print_positive_binary64_integer(
								iter, binary_mantissa,
								static_cast<::std::uint_least32_t>(binary_exponent))};
						if (positive_integer)
						{
							return ::fast_io::details::print_rsv_fp_append_point_zeros<comma>(
								positive_integer, precision);
						}
					}
					else if constexpr (::fast_io::details::
						exact_precision_window_outlined_positive_fixed)
					{
						return ::fast_io::details::
							exact_precision_window_print_positive_binary64_fixed<comma>(
								iter, binary_mantissa,
								static_cast<::std::uint_least32_t>(binary_exponent), precision);
					}
				}
				else
				{
					// Raw binary64 exponents 1012..1055 map exactly to final
					// binary shifts -63..-20 before adding decimal precision.  The
					// generated u64/u128 pow5 domains and the gates below prove that
					// mantissa * 5^p, the subsequent shift and a rounding carry fit.
					// These are arithmetic bounds; the target-specific cutoffs named by
					// exact_precision_scaled_fixed_* are separate code-generation policy.
					if (exponent - 1012u <= 43u)
					{
						auto const scaled_path_eligible{precision <= 17u ||
							(precision <= 32u &&
							 ((precision <= 28u && precision <= ::fast_io::details::
									exact_precision_scaled_fixed_full_exponent_precision_limit) ||
							  exponent <= 1025u))};
						if (scaled_path_eligible)
						{
							auto const fractional_binary_bits{
								static_cast<::std::uint_least32_t>(-binary_exponent)};
							auto const binary_factors{static_cast<::std::uint_least32_t>(
								::std::countr_zero(binary_mantissa))};
							auto const terminating_fractional_position{
								fractional_binary_bits -
								(binary_factors < fractional_binary_bits
									? binary_factors
									: fractional_binary_bits)};
							// Exact terminating cases use the prefix writer.  Nonterminating
							// cases use the scaled writer only inside the proved product/shift
							// bounds above.  The target threshold is code-generation policy;
							// it preserves audited register residency and branch placement and
							// must be revalidated independently from the arithmetic proof.
							if (terminating_fractional_position &&
								(precision < terminating_fractional_position ||
								 precision < ::fast_io::details::
									exact_precision_scaled_fixed_fraction_exact_minimum))
							{
								if (precision <= 17u)
								{
									return ::fast_io::details::
										exact_precision_window_try_print_scaled_binary64_fixed<
											false, comma, rounding>(iter, binary_mantissa,
											binary_exponent, precision, sign);
								}
								return ::fast_io::details::
									exact_precision_window_try_print_scaled_binary64_fixed<
										true, comma, rounding>(iter, binary_mantissa,
										binary_exponent, precision, sign);
							}
						}
					}
					auto const direct_result{::fast_io::details::
						exact_precision_window_try_print_exact_mixed_fixed<
							comma, json_float, rounding>(iter, binary_mantissa,
							binary_exponent, precision, sign)};
					if (direct_result)
					{
						return direct_result;
					}
				}
			}
			else if constexpr (::fast_io::details::
				binary64_fixed_fractional_low_integrated_dispatch &&
				::fast_io::details::floating_rounding_is_nearest<rounding>)
			{
				// The audited x86 Clang 23 placement reuses the failed P15/exact-
				// window branch here.
				// The enclosing condition proves P1--P14 and a noninteger normal
				// value; P0 retains the generic JSON suffix owner. The generated
				// exponent hull avoids materializing decimal-exponent state on misses.
				constexpr auto exponent_union{::fast_io::details::da::
					binary64_fixed_fractional_low_exponent_union};
				if (precision &&
					exponent - exponent_union.minimum <= exponent_union.span) [[unlikely]]
				{
					auto const low_result{::fast_io::details::
						print_rsvflt_binary64_fixed_fractional_da_low_impl<
							comma>(iter,
								static_cast<::std::uint_least64_t>(mantissa),
								exponent, precision)};
					if (low_result)
					{
						return low_result;
					}
				}
			}
		}
#endif
		// Compilers below or outside the integrated-placement transition use a
		// standalone precision-first probe. P15--P128 fail its first unsigned
		// comparison before reading exponent state; this is the measured GCC13--16
		// and Clang22 layout. The carrier uses a native u128 residual for its exact
		// half comparison, so targets without that representation keep the fallback.
#if defined(__SIZEOF_INT128__)
		if constexpr (!::fast_io::details::
			binary64_fixed_fractional_low_integrated_dispatch &&
			::std::same_as<flt, double> &&
			::fast_io::details::floating_rounding_is_nearest<rounding> &&
			::fast_io::details::floating_precision_is_fractional<precision_mode> &&
			::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode> &&
			(mt == ::fast_io::manipulators::floating_format::fixed ||
			 mt == ::fast_io::manipulators::floating_format::decimal))
		{
			if (precision - 1u < 14u && exponent)
			{
				constexpr auto exponent_union{::fast_io::details::da::
					binary64_fixed_fractional_low_exponent_union};
				if (exponent - exponent_union.minimum <= exponent_union.span) [[unlikely]]
				{
					auto const low_result{::fast_io::details::
						print_rsvflt_binary64_fixed_fractional_da_low_impl<
							comma>(iter,
								static_cast<::std::uint_least64_t>(mantissa), exponent,
								precision)};
					if (low_result)
					{
						return low_result;
					}
				}
			}
		}
#endif
		// Non-preserving significant P23-P33 shares one normal-binary64 DA
		// carrier across all four decimal presentations; P34-P38 use their retained
		// two-word coefficients.  Both enter outlined dispatchers so P1-P22 and P39+ do
		// not inherit their arithmetic or register pressure.  The sign is already
		// emitted, and zero and non-finite values returned above.  A null result is
		// an ambiguity rejection and leaves the existing shortest/exact path
		// authoritative.  Native u128 is a representation capability; all output
		// semantics have portable fallback.
#if defined(__SIZEOF_INT128__)
		if constexpr (::fast_io::details::
			binary64_common_significant_precision_public_dispatch &&
			::std::same_as<flt, double> &&
			precision_mode ==
				::fast_io::manipulators::floating_precision::significant &&
			::fast_io::details::floating_rounding_is_nearest<rounding>)
		{
			// The common arithmetic is proved for P20-P33, but its packed-carrier
			// branch encodes only P23-P33.  Extend the public interval only when
			// this measured scientific policy and the format's direct-carrier
			// policy are both selected.  Thus P20-P22 cannot enter the packed
			// representation, while every miss still executes one unsigned range
			// comparison and every accepted precision calls the existing dispatcher.
			constexpr bool include_p20_p22{
				mt == ::fast_io::manipulators::floating_format::scientific &&
				::fast_io::details::binary64_scientific_p20_p22_direct &&
				::fast_io::details::binary64_common_significant_precision_direct<mt>};
			constexpr ::std::size_t minimum{include_p20_p22 ? 20u : 23u};
			if (exponent && precision - minimum < 34u - minimum)
			{
				auto const da_result{::fast_io::details::
					print_rsvflt_binary64_common_significant_precision_dispatch<
						comma, uppercase_e, mt, json_float>(iter,
							static_cast<::std::uint_least64_t>(mantissa),
							exponent, precision)};
				if (da_result)
				{
					return da_result;
				}
			}
			else if (exponent && precision == 34u)
			{
				auto const da_result{::fast_io::details::
					print_rsvflt_binary64_p34_precision_dispatch<
						comma, uppercase_e, mt, json_float>(iter,
							static_cast<::std::uint_least64_t>(mantissa), exponent)};
				if (da_result)
				{
					return da_result;
				}
			}
			else if (exponent && precision - 35u < 4u)
			{
				auto const da_result{::fast_io::details::
					print_rsvflt_binary64_p35_p38_precision_dispatch<
						comma, uppercase_e, mt, json_float>(iter,
							static_cast<::std::uint_least64_t>(mantissa), exponent,
							precision)};
				if (da_result)
				{
					return da_result;
				}
			}
		}
#endif
		if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
		{
			/*
			The nearest-even shortest carrier is an interval witness, not the final
			precision result. If P cuts that carrier, the strict distance test below
			requires the retained decimal to be more than one carrier unit away from
			the halfway boundary. The exact binary value lies within one such unit,
			so it is on the same open side of the boundary; all six nearest policies
			therefore select the same P-digit coefficient. If the distance is at most
			one, the exact prefix/guard/sticky path remains authoritative and applies
			the requested tie policy. If P extends the carrier, the digits10
			separation proof below is likewise independent of which endpoint is closed.

			Consequently the five alternative nearest policies do not need five
			distinct shortest-roundtrip corrections before this proof. Sharing DA's
			nearest-even carrier removes work only; the final tie decision still uses
			`rounding`, and no directed policy enters this branch.
			*/
			auto const [m10, e10]{::fast_io::details::dragonbox_impl<flt,
				::fast_io::manipulators::floating_rounding::nearest_to_even>(
				mantissa, static_cast<::std::int_least32_t>(exponent), sign)};
			auto const length{static_cast<::std::size_t>(chars_len<10, true>(m10))};
			::std::size_t requested{};
			if constexpr (::fast_io::details::floating_precision_is_fractional<precision_mode>)
			{
				if constexpr (mt == ::fast_io::manipulators::floating_format::scientific)
				{
					requested = ::fast_io::details::exact_precision_saturating_add(precision, 1u);
				}
				else
				{
					auto const real_exponent{e10 + static_cast<::std::int_least32_t>(length) - 1};
					if (0 <= real_exponent)
					{
						requested = ::fast_io::details::exact_precision_saturating_add(
							::fast_io::details::exact_precision_saturating_add(
								static_cast<::std::size_t>(real_exponent), precision),
							1u);
					}
					else
					{
						auto const leading_fractional_zeros{static_cast<::std::size_t>(-real_exponent)};
						if (leading_fractional_zeros <= precision)
						{
							requested = precision - leading_fractional_zeros + 1u;
						}
					}
				}
			}
			else
			{
				requested = precision ? precision : 1u;
			}
			bool carrier_is_exact_enough{};
			if (requested && requested < length)
			{
				auto const cut{length - requested};
				if (cut < 20u)
				{
					auto const divisor{::fast_io::details::print_rsv_fp_pow10_0_to_19_table[cut]};
					auto const remainder{static_cast<::std::uint_least64_t>(m10 % divisor)};
					auto const twice_remainder{remainder << 1u};
					auto const distance{twice_remainder < divisor ? divisor - twice_remainder : twice_remainder - divisor};
					carrier_is_exact_enough = 1u < distance;
				}
			}
			else if (exponent && length <= requested &&
				requested <= static_cast<::std::size_t>((::std::numeric_limits<flt>::digits10)))
			{
				// The shortest carrier is already on the requested decimal grid.
				// Up to digits10 significant digits a binary ULP is strictly smaller
				// than half that grid quantum for a normal value, so padding the
				// carrier cannot cross a nearest-rounding boundary.  General fractional-
				// preserve is the exception when requested exceeds the carrier length:
				// a nonzero exact tail means rounding must preserve a synthetic 10^-P
				// zero, while a dyadic value already on that grid must not invent one.
				// The shortest carrier has no tail bit, so that case reaches the compact
				// exact window which distinguishes the two.  Equal length needs no
				// synthetic suffix and remains safe.  Subnormals lack the relative-error
				// bound and deliberately remain on the exact path.
				if constexpr (mt == ::fast_io::manipulators::floating_format::general &&
					precision_mode == ::fast_io::manipulators::floating_precision::
						fractional_preserve_trailing_zero)
				{
					if constexpr (::std::same_as<flt, double>)
					{
						// Only binary64 has the K<=15 fit and separation proof above.
						// Other floating representations keep the exact-window fallback.
						// This target test selects the measured compiler placement only;
						// both branches implement the same proved carrier transformation.
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
						/*
						Apple AArch64 keeps the exactness predicate as its own cold compiler-
						generated leaf while retaining the small carrier adjustment in this
						caller.  On Apple M4 with Clang 23 this placement grew linked text by
						188 bytes rather than 288 for the fully outlined variant.  Four-order
						measurements against the previous implementation improved admitted
						exact values by 2.0%--5.5%, nonexact fixed/scientific values by
						45%--51%, and the broad P1--P128 control by 0.6%--2.4%.  The guard
						therefore selects code placement only.  The predicate, padding
						arithmetic and renderer below are identical to the non-Apple helper.
						*/
						if (length == requested ||
							(precision < 5u && -5 < e10 && e10 < 7) ||
							::fast_io::details::dragonbox_decimal_carrier_is_binary_exact<flt>(
								mantissa, exponent, m10, e10))
						{
							carrier_is_exact_enough = true;
						}
						else
						{
							auto const padding{requested - length};
							auto const padded_m10{static_cast<
								::fast_io::details::dragonbox_decimal_mantissa_type<flt>>(
								m10 * ::fast_io::details::print_rsv_fp_pow10_0_to_19_table[padding])};
							auto const padded_e10{
								e10 - static_cast<::std::int_least32_t>(padding)};
							return ::fast_io::details::print_rsv_fp_precision_decision_impl<
								flt, comma, uppercase_e, mt, precision_mode, rounding,
								json_float>(iter, padded_m10, padded_e10, precision, sign);
						}
#else
						/*
						Non-Apple AArch64 and every other target call the outlined arithmetic
						leaf.  This is the same decimal predicate and adjustment as the Apple
						branch; only its compiler placement differs.  GCC 15 System V x86-64
						reduced caller growth from 253 to 100 bytes and added 512 linked text
						bytes.  Four-order measurements improved admitted exact values by
						5.1%--6.8% and nonexact fixed/scientific values by 43%--50%, while
						the broad P1--P128 control remained within 0.5%.
						*/
						if (length == requested)
						{
							carrier_is_exact_enough = true;
						}
						else
						{
							auto const adjusted{::fast_io::details::
								prepare_binary64_general_fractional_carrier(
									static_cast<::std::uint_least64_t>(mantissa), exponent,
									static_cast<::std::uint_least64_t>(m10), e10, length,
									requested, precision)};
							return ::fast_io::details::print_rsv_fp_precision_decision_impl<
								flt, comma, uppercase_e, mt, precision_mode, rounding,
								json_float>(iter, adjusted.mantissa, adjusted.exponent,
									precision, sign);
						}
#endif
					}
					else
					{
						carrier_is_exact_enough = length == requested;
					}
				}
				else
				{
					carrier_is_exact_enough = true;
				}
			}
			if (carrier_is_exact_enough)
			{
				return ::fast_io::details::print_rsv_fp_precision_decision_impl<
					flt, comma, uppercase_e, mt, precision_mode, rounding, json_float>(
					iter, m10, e10, precision, sign);
			}
			if constexpr (::fast_io::details::floating_precision_is_fractional<precision_mode> &&
					  mt != ::fast_io::manipulators::floating_format::scientific)
			{
				// A leading digit at least two places below the requested quantum is strictly
				// below its halfway boundary, so every nearest policy produces fractional zero.
				constexpr auto int32_max{(::std::numeric_limits<::std::int_least32_t>::max)()};
				if (precision <= static_cast<::std::size_t>(int32_max))
				{
					auto const real_exponent{
						e10 + static_cast<::std::int_least32_t>(length) - 1};
					auto const quantum_distance{
						static_cast<::std::int_least64_t>(real_exponent) +
						static_cast<::std::int_least64_t>(precision)};
					if (quantum_distance <= -2)
					{
						return ::fast_io::details::print_rsvflt_fractional_tiny_zero_impl<
							flt, comma, uppercase_e, mt, precision_mode, json_float>(iter, precision);
					}
				}
			}
			// MSVC x64 lacks a language-level u128 type, but P16--P17 require only
			// the portable two-word products used by the ordinary DA conversion.
			// Probe only after the shortest carrier and tiny-quantum exits, so P1--
			// P15 retain their entry path.  The DA interval rejects every ambiguous
			// half boundary before writing; the unchanged exact expansion handles
			// those misses and all subnormals.  GCC, Clang, other ABIs and other
			// MSVC architectures preprocess this code-generation policy away.
#if defined(_MSC_VER) && defined(_M_X64) && !defined(__clang__) && \
	!defined(__SIZEOF_INT128__) && \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
			if constexpr (::std::same_as<flt, double> &&
				::fast_io::details::floating_precision_is_significant<precision_mode>)
			{
				if (exponent && precision - 16u < 2u)
				{
					auto const converted{::fast_io::details::
							binary64_scientific_precision_msvc_runtime(
							static_cast<::std::uint_least64_t>(mantissa), exponent,
							precision)};
					if (converted.success)
					{
						auto decimal_exponent{converted.exponent + 1 -
							static_cast<::std::int_least32_t>(precision)};
						auto decimal_significand{converted.significand};
						auto length{static_cast<::std::uint_least32_t>(precision)};
						if constexpr (!::fast_io::details::
							floating_precision_preserves_trailing_zero<precision_mode>)
						{
							while (decimal_significand % 10u == 0u)
							{
								decimal_significand /= 10u;
								++decimal_exponent;
								--length;
							}
						}
						return ::fast_io::details::
							print_rsvflt_decimal_with_length_define_impl<
								flt, comma, uppercase_e, mt, json_float>(iter,
									decimal_significand, decimal_exponent, length);
					}
				}
			}
#endif
			// This compact binary32 scientific retry uses the same u128 exact-window
			// infrastructure.  Targets without u128 proceed directly to the general
			// slow path, preserving the requested rounding and character policy.
#if defined(__SIZEOF_INT128__)
			if constexpr (::std::same_as<flt, float> &&
				mt == ::fast_io::manipulators::floating_format::scientific)
			{
				auto real_exponent{
					e10 + static_cast<::std::int_least32_t>(length) - 1};
				if (15u <= precision && m10 == 1u)
				{
					real_exponent = ::fast_io::details::
						exact_precision_correct_binary32_raw_negative_real_exponent(
							static_cast<::std::uint_least32_t>(mantissa), exponent,
							real_exponent);
				}
				if (real_exponent < 0 && requested &&
					requested < exact_precision_compact_window_digit_capacity)
				{
					auto const compact_result{
						::fast_io::details::print_rsvflt_exact_precision_window_impl<
							flt, comma, uppercase_e, mt, precision_mode, rounding,
							json_float, false, false>(iter, mantissa, exponent, precision, sign,
								real_exponent)};
					if (compact_result)
					{
						return compact_result;
					}
				}
			}
#endif
			return ::fast_io::details::print_rsvflt_precision_slow_path_select_impl<
				flt, comma, uppercase_e, mt, precision_mode, rounding, json_float>(
				iter, mantissa, exponent, precision, sign, m10, e10, requested, length);
		}
		return ::fast_io::details::print_rsvflt_precision_slow_path_select_impl<
			flt, comma, uppercase_e, mt, precision_mode, rounding, json_float>(
			iter, mantissa, exponent, precision, sign);
	}
}

template <typename flt, ::fast_io::manipulators::floating_format mf>
inline constexpr ::std::size_t print_rsvflt_size_impl() noexcept
{
	using trait = iec559_traits<flt>;
	if constexpr (mf == ::fast_io::manipulators::floating_format::fixed)
	{
		// general's max length is equal to scientific's max length
		//(+/-)(significants+sep)
		::std::size_t sum{1}; // sign(+/-)
		sum += 2;             // 0./,
		sum += trait::e10max;
		sum += trait::m10digits;
		return sum;
	}
	else
	{
		// decimal and general's max lengths are equal to scientific's max length
		//(+/-)(significants+sep)(E/e)(+/-)e
		::std::size_t sum{1}; // sign(+/-)
		sum += trait::m10digits;
		++sum;    //./,
		sum += 2; //(E/e)(+/-)
		sum += trait::e10digits;
		return sum;
	}
}

template <typename flt, ::fast_io::manipulators::floating_format mt>
inline constexpr ::std::size_t print_rsv_cache{print_rsvflt_size_impl<flt, mt>()};

} // namespace fast_io::details

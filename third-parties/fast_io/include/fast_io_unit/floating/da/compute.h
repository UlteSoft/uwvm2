#pragma once

#include "cache.h"

namespace fast_io::details::da
{

/*
Nearest-to-even shortest-conversion theorem
===========================================

Write one positive finite source as x=m*2^e, where m is its integer
significand.  Parsing a real y with round-to-nearest, ties-to-even returns x
exactly when y lies in x's rounding cell.  For a regular value (equal gaps to
both neighbours) that cell is

    R(x) = [x-2^(e-1), x+2^(e-1)]                         (1)

with both midpoint endpoints included iff m is even and excluded iff m is odd.
At an exact normal power of two the predecessor gap is half the successor gap,
so the cell is instead

    R2(x) = [x-2^(e-2), x+2^(e-1)].                       (2)

Its centre significand is even, hence both endpoints are closed.  Signs need no
separate arithmetic proof: decimal negation maps the positive cell bijectively
onto the negative cell and the caller emits the sign after converting the
magnitude.

Let W be the width of the relevant cell: W=2^e in (1) and
W=(3/4)*2^e in (2).  Set d=floor(log10(W)) and scale by 10^(-(d+1)).
The scaled cell width is in [0.1,1).  Therefore it contains at most one integer
grid point and, if it contains none, contains a point of the one-decimal-place
grid.  The equality endpoint case cannot create a missing open interval:
10^d=2^e has only the e=d=0 solution, and the half-integer binary endpoints are
not both points of the 10^d grid.  Thus only two candidate grids are required:

    J * 10^(d+1),             or
    (10*I + D) * 10^d.                                      (3)

An integer-grid member has one fewer coefficient digit and is necessarily the
shortest choice.  Otherwise the fine-grid member is shortest because the coarse
grid has just been proved empty.  Removing trailing decimal zeroes later
preserves the real number and can only shorten it.

The cache/product lemma used below is the integer implementation of this
geometric argument.  With B=2^k (k=34 for binary32 and k=64 for binary64), the
aligned cached product supplies A=I+F/B.  Cache entries are normalized downward
endpoints; the binary32 high-word specialization deliberately uses the matching
upward endpoint.  The generated correction bits and discarded-product bound
prove that the exact scaled value and radius can differ from their integer
proxies by at most one B-unit.  After adding `even = 1-(m mod 2)`, the following
integer predicates are therefore exactly the endpoint-membership predicates:

    F < H             <=> I is in R(x),
    B-F <= H          <=> I+1 is in R(x).                    (4)

The strict/non-strict asymmetry is not arbitrary: the `+even` changes equality
from rejected for an odd significand to accepted for an even significand,
precisely reproducing (1).  Since the scaled cell is shorter than one, the two
predicates cannot both select different coarse candidates.

The last digit is a nearest-even rounding of 10F/B, subject to the asymmetric
lower bound in (2).  For B a power of two, an exact half-integer 10F/B can occur
only at F=B/4 or F=3B/4: from 20F=(2q+1)B, divisibility by five forces
2q+1 to be 5 or 15 modulo 20.  Ordinary add-half rounding already sends 7.5 to
the even digit 8; the explicit B/4 branch sends 2.5 to the even digit 2.
Binary64's `+6` is the minimal discrete correction for its downward 128-bit
cache.  Completeness follows by partitioning every exponent's significand
interval at the modular boundaries

    (C_q*m) mod 2^(s+64)

for (4) and for the nine digit half-points.  Away from the adjacent boundary
units monotonicity makes the approximate and exact decisions identical; the
boundary congruence classes give the `+6` correction and the B/4 exception.
Consequently this is an exhaustive integer-boundary proof, not an empirical
real-number comparison.

`conversion_result` stores exactly the two alternatives in (3).  If
`has_last_digit` is false, `significand * 10^(exponent+1)` is the proved coarse
candidate.  If true,
`(10*significand+last_digit) * 10^exponent` is the proved fine candidate.
`finalize()` merely materializes that identity; the direct ASCII writer consumes
the same fields and is therefore mathematically equivalent.
*/
struct conversion_result
{
	::std::uint_least64_t significand;
	::std::int_least32_t exponent;
	::std::uint_least32_t last_digit;
	bool has_last_digit;
};

/*
Convert the asymmetric cell (2) of a finite normal power of two.  The caller
passes the implicit-bit significand with a zero stored mantissa.  Every branch
below is one of the two grid-membership tests or the fine-grid construction
proved above; no presentation policy is mixed into this arithmetic.
*/
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr conversion_result compute_irregular(
	::std::uint_least64_t binary_significand, ::std::int_least32_t binary_exponent) noexcept
{
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	constexpr auto uint64_max{(::std::numeric_limits<::std::uint_least64_t>::max)()};
	/*
	The complete asymmetric width is

	    2^(e-2) + 2^(e-1) = (3/4)*2^e.

	Choosing d from that width proves the same two-grid bound as the regular
	case.  `shift` aligns x*10^(-(d+1)) so the six discarded guard bits and
	the following 64 fractional bits have the cache lemma's B=2^64 scale.
	*/
	auto const decimal_exponent{compute_decimal_exponent(binary_exponent, false)};
	auto const shift{static_cast<::std::uint_least8_t>(
		compute_exponent_shift(binary_exponent, decimal_exponent + 1) + extra_shift)};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(binary_significand << shift, power)};
	/*
	`umul64x128_high` returns the high 128 bits of the exact 192-bit integer
	product.  Dropping `extra_shift` guard bits therefore gives the proved
	lower fixed-point endpoint A=integral+fractional/2^64, with no floating
	operation and no dependence on the hardware rounding mode.
	*/
	auto integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	/*
	`half_ulp` is the upper radius 2^(e-1) on the B grid.  The lower radius is
	exactly half as large.  Unsigned carry implements B-F<=H without a branch
	or an overflowing mathematical addition; the strict lower comparison
	implements F<H/2.  Power-of-two midpoint endpoints are closed, and the
	downward-cache error convention is already incorporated in H, so equality
	needs no parity correction here.
	*/
	auto const half_ulp{power.hi >> (extra_shift + 1u - shift)};
	auto const round_up{half_ulp > uint64_max - fractional};
	auto const round_down{(half_ulp >> 1u) > fractional};
	// Only the upper coarse candidate changes I; the lower candidate is I itself.
	integral += round_up;
	/*
	If neither coarse candidate exists, the first expression chooses the
	nearest fine-grid digit, with exact halves biased downward as required by
	the irregular boundary certificate.  Put z=F-H/2.  On every path where the
	digit is live, `round_down` is false, hence z>=0 and the unsigned subtraction
	cannot wrap.  Since

	    floor((10*z + (B-1))/B) = ceil(10*z/B),

	`lower` is exactly the smallest digit whose decimal point is not below the
	closer lower boundary.  Taking max(nearest,lower) produces a member of (2).
	When either coarse predicate is true the digit is deliberately dead, so its
	value cannot affect the returned decimal.
	*/
	auto digit{static_cast<::std::uint_least32_t>(::fast_io::details::da::umul64x64_add_high(
		fractional, 10u, (static_cast<::std::uint_least64_t>(1) << 63u) - 1u))};
	auto const lower{static_cast<::std::uint_least32_t>(::fast_io::details::da::umul64x64_add_high(
		fractional - (half_ulp >> 1u), 10u, uint64_max))};
	if (digit < lower)
	{
		// The nearest tenth lay below the asymmetric cell; select its first member.
		digit = lower;
	}
	/*
	A coarse-grid hit omits the final digit.  Otherwise the fine-grid proof
	above supplies it.  The scaled width is <1, so `round_up` and `round_down`
	cannot select two distinct integer candidates.
	*/
	return {integral, decimal_exponent, digit, !(round_up || round_down)};
}

/*
Convert a nonzero regular finite binary32 value.  `raw_exponent` is the
effective table exponent: a subnormal is mapped to one while retaining its
subnormal significand, which is exactly the identity
m*2^-149 = m*2^(1-150).  Exact normal powers of two use `compute_irregular`;
every admitted value therefore has the symmetric cell (1).
*/
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void compute_binary32_fields(
	::std::uint_least32_t binary_significand, ::std::uint_least32_t raw_exponent,
	::std::uint_least64_t &integral_result, ::std::int_least32_t &decimal_exponent_result,
	::std::uint_least32_t &last_digit_result, bool &has_last_digit_result) noexcept
{
	constexpr ::std::uint_least8_t extra_shift{34u};
	// IEEE binary32 decodes exactly as m*2^(raw_exponent-150).
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 150};
	/*
	d=floor(e*log10(2)) is the regular cell-width exponent.  The generated
	table stores the same alignment proved exhaustively below; adding 34-6
	changes only the fixed-point radix from the shared cache guard width to
	B=2^34.
	*/
	auto const decimal_exponent{compute_decimal_exponent_reduced(binary_exponent)};
	auto const shift{static_cast<::std::uint_least8_t>(
		cached_data.exponent_shifts.data[static_cast<::std::size_t>(raw_exponent + 925u)] +
		(extra_shift - exponent_shift_cache::extra_shift))};
	auto const power_high{cached_data.powers[-decimal_exponent - 1].hi};
	/*
	The 64-bit high word is sufficient for binary32 because m has at most 24
	bits.  `power_high+1` is the matching upward endpoint of the truncated
	128-bit power, and `umulh` then yields A=I+F/2^34 with the one-unit error
	required by the cache/product lemma.
	*/
	auto const product{::fast_io::intrinsics::umulh(
		power_high + 1u, static_cast<::std::uint_least64_t>(binary_significand) << shift)};
	constexpr ::std::uint_least64_t fractional_mask{(static_cast<::std::uint_least64_t>(1) << extra_shift) - 1u};
	auto const fractional{product & fractional_mask};
	/*
	H is the symmetric half-ULP radius on the 34-bit grid.  The parity addend
	is one exactly when m is even.  Thus `F+H >= 2^34` admits I+1 and `H>F`
	admits I with precisely the open/closed endpoint rule in (1).
	*/
	auto const half_ulp{static_cast<::std::uint_least64_t>(
		(power_high >> (65u - shift)) + (1u - (binary_significand & 1u)))};
	auto const round_up{static_cast<bool>((fractional + half_ulp) >> extra_shift)};
	auto const round_down{half_ulp > fractional};
	auto const integral{static_cast<::std::uint_least64_t>((product >> extra_shift) + round_up)};
	/*
	If the coarse grid is empty, add-half computes nearest(10F/2^34).
	All exact decimal half cases reduce to F=B/4 or 3B/4 by the divisibility
	argument in the theorem.  The latter rounds upward from 7 to the even 8;
	the former needs the explicit lower-even correction below.
	*/
	auto digit{static_cast<::std::uint_least32_t>(
		(fractional * 10u + (static_cast<::std::uint_least64_t>(1) << (extra_shift - 1u))) >> extra_shift)};
	if (fractional == (static_cast<::std::uint_least64_t>(1) << (extra_shift - 2u)))
	{
		// 10*(B/4)/B=2.5; nearest-even selects the even lower digit 2.
		digit = 2u;
	}
	// These four assignments expose the same proved carrier without a return ABI copy.
	integral_result = integral;
	decimal_exponent_result = decimal_exponent;
	last_digit_result = digit;
	// A coarse member is shorter; only an empty coarse grid keeps the fine digit.
	has_last_digit_result = !(round_up || round_down);
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr conversion_result compute_binary32(
	::std::uint_least32_t binary_significand, ::std::uint_least32_t raw_exponent) noexcept
{
	conversion_result result;
	::fast_io::details::da::compute_binary32_fields(binary_significand, raw_exponent,
		result.significand, result.exponent, result.last_digit, result.has_last_digit);
	return result;
}

// High-product spelling of floor(binary_exponent * 78913 / 2^18).  If e is
// negative, converting it to u64 adds 2^64 before multiplication.  The resulting
// high-word offset is 78913 * 2^46, whose low 32 bits are zero; narrowing the
// high word to int32 therefore removes that offset.  The true result is in the
// int32 range for every binary32/binary64 conversion exponent.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::int_least32_t
compute_decimal_exponent_high_product(::std::int_least32_t binary_exponent) noexcept
{
	return static_cast<::std::int_least32_t>(::fast_io::intrinsics::umulh(
		static_cast<::std::uint_least64_t>(static_cast<::std::int_least64_t>(binary_exponent)),
		static_cast<::std::uint_least64_t>(78913) << 46u));
}

// Exhaust the complete reduced-reciprocal domain so the Apple code-generation
// spelling cannot silently diverge when either reciprocal constant is changed.
[[nodiscard]] inline constexpr bool verify_decimal_exponent_high_product() noexcept
{
	for (::std::int_least32_t exponent{-1074}; exponent <= 971; ++exponent)
	{
		if (compute_decimal_exponent_high_product(exponent) !=
			compute_decimal_exponent_reduced(exponent))
		{
			return false;
		}
	}
	return true;
}

static_assert(verify_decimal_exponent_high_product());

/*
Table-free binary32 alignment
-----------------------------

For a regular binary32 encoding, raw exponent r is in [1,254] and the exact
integer exponent supplied to DA is e=r-150.  The reduced reciprocal has already
proved

    d = floor(e*log10(2)) = (e*78913) >> 18

on the larger binary32/binary64 domain.  A normalized cached value for
10^(-(d+1)) needs the left alignment

    s = e + floor(-(d+1)*log2(10)) + 1 + 34
      = e + ((-(d+1)*217707) >> 16) + 35.

The second fixed-point identity is not assumed from an error estimate alone:
the verifier below exhausts all 254 normal raw exponents and compares the
formula with the generated authoritative shift table.  This is the complete
binary32 staged domain, not a sample.  Its result also proves s is the same
31..34 shift consumed by the existing product/rounding proof.
*/
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr
	::std::uint_least8_t compute_binary32_shift_without_table(
		::std::uint_least32_t raw_exponent,
		::std::int_least32_t binary_exponent,
		::std::int_least32_t decimal_exponent) noexcept
{
	(void)raw_exponent;
	return static_cast<::std::uint_least8_t>(
		::fast_io::details::da::compute_exponent_shift(
			binary_exponent, decimal_exponent + 1) +
		34u);
}

[[nodiscard]] inline constexpr bool
verify_binary32_shift_without_table() noexcept
{
	for (::std::uint_least32_t raw_exponent{1u};
		 raw_exponent != 255u; ++raw_exponent)
	{
		auto const binary_exponent{
			static_cast<::std::int_least32_t>(raw_exponent) - 150};
		auto const decimal_exponent{
			::fast_io::details::da::compute_decimal_exponent_reduced(
				binary_exponent)};
		auto const formula{
			::fast_io::details::da::compute_binary32_shift_without_table(
				raw_exponent, binary_exponent, decimal_exponent)};
		auto const table{
			static_cast<::std::uint_least8_t>(
				cached_data.exponent_shifts.data[
					static_cast<::std::size_t>(raw_exponent + 925u)] +
				(34u - exponent_shift_cache::extra_shift))};
		if (formula != table)
		{
			return false;
		}
	}
	return true;
}

static_assert(verify_binary32_shift_without_table());

// Staged binary32 conversion with the AArch64 power-table base exposed as a
// loop invariant.  Its fixed-point arithmetic and returned fields are
// identical to compute_binary32; only the address expression differs.  The
// empty asm keeps the base as one opaque register value so staged batches do
// not rematerialize it.  Retain this variant only while the supported assembly
// matrix confirms the shared base and no additional spill or call.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr conversion_result compute_binary32_staged(
	::std::uint_least32_t binary_significand, ::std::uint_least32_t raw_exponent) noexcept
{
	// GNU-driver AArch64 uses the reversed split cache layout whose prepared base
	// is profitable across staged values.  `_MSC_VER` excludes both native MSVC
	// and clang-cl because their source/ABI contracts are not covered by the GNU
	// extended-assembly artifact.  Apple Clang 23 on M4 is the performance-audited
	// case; other GNU-driver AArch64 frontends inherit only the source-capability
	// and semantic-equivalence hypothesis until their complete staged caller is
	// re-audited.  Every other configuration delegates to the field-for-field
	// identical ordinary conversion.
#if (defined(__aarch64__) || defined(__arm64__)) && !defined(_MSC_VER) && \
	(defined(__clang__) || (defined(__GNUC__) && !defined(__clang__)))
	constexpr ::std::uint_least8_t extra_shift{34u};
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 150};
	auto const decimal_exponent{compute_decimal_exponent_reduced(binary_exponent)};
	auto const shift{static_cast<::std::uint_least8_t>(
		cached_data.exponent_shifts.data[static_cast<::std::size_t>(raw_exponent + 925u)] +
		(extra_shift - exponent_shift_cache::extra_shift))};
	auto base{cached_data.powers.data + power10_cache::size + power10_cache::minimum_exponent};
	if (!::std::is_constant_evaluated())
	{
		/*
		This empty constraint changes no value.  It only prevents the optimizer
		from rematerializing a different cache-base expression between independent
		lanes; the address still denotes the identical constexpr cache object.
		Constant evaluation omits GNU assembly and uses that object directly.
		*/
		__asm__("" : "+r"(base));
	}
	/*
	Every arithmetic expression below is the expression proved in
	`compute_binary32_fields`: the split cache layout changes only how
	`power_high` is addressed.  In particular, the parity endpoint correction,
	the two coarse membership tests, and the B/4 nearest-even exception are
	field-for-field identical, proving staged and scalar carriers equal.
	*/
	auto const power_high{base[static_cast<::std::ptrdiff_t>(decimal_exponent)]};
	auto const product{::fast_io::intrinsics::umulh(
		power_high + 1u, static_cast<::std::uint_least64_t>(binary_significand) << shift)};
	constexpr ::std::uint_least64_t fractional_mask{(static_cast<::std::uint_least64_t>(1) << extra_shift) - 1u};
	auto const fractional{product & fractional_mask};
	auto const half_ulp{static_cast<::std::uint_least64_t>(
		(power_high >> (65u - shift)) + (1u - (binary_significand & 1u)))};
	auto const round_up{static_cast<bool>((fractional + half_ulp) >> extra_shift)};
	auto const round_down{half_ulp > fractional};
	auto const integral{static_cast<::std::uint_least64_t>((product >> extra_shift) + round_up)};
	auto digit{static_cast<::std::uint_least32_t>(
		(fractional * 10u + (static_cast<::std::uint_least64_t>(1) << (extra_shift - 1u))) >> extra_shift)};
	if (fractional == (static_cast<::std::uint_least64_t>(1) << (extra_shift - 2u)))
	{
		// Same exact B/4 -> 2.5 -> even 2 case as compute_binary32_fields.
		digit = 2u;
	}
	return {integral, decimal_exponent, digit, !(round_up || round_down)};
#else
	/*
	The table-free binary32 alignment theorem above remains available to an
	explicit vector backend, but it is not selected merely because x86 exposes
	BMI2 or AVX2.  Native i9-14900HX GCC16 AB/BA measurements of the complete
	prepare+emit caller made the scalar table-free spelling 1.5--1.9% slower:
	the two dependent fixed-point multiplies cost more than the hot one-byte
	load.  The ordinary implementation below is therefore the proved x86
	performance choice until an AVX-512 multi-lane backend amortizes those
	integer operations.  This branch changes scheduling only; the exhaustive
	formula/table equality above proves either spelling would return identical
	fields.
	*/
	return ::fast_io::details::da::compute_binary32(binary_significand, raw_exponent);
#endif
}

/*
Binary64 form of the regular theorem.  The caller supplies effective raw
exponent one for subnormals, retaining their 52-bit significand, and exact
normal powers of two use `compute_irregular`.  Thus this function sees exactly
the symmetric cell (1).  It differs from binary32 only in using the complete
128-bit downward cache and B=2^64.
*/
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr conversion_result compute_binary64(
	::std::uint_least64_t binary_significand, ::std::uint_least32_t raw_exponent) noexcept
{
	// IEEE binary64 decodes exactly as m*2^(raw_exponent-1075).
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 1075};
	::std::int_least32_t decimal_exponent;
	// On Apple AArch64, the unsigned high product below computes the same reduced
	// reciprocal as floor(binary_exponent * 78913 / 2^18).  For a negative
	// exponent, its unsigned 2^64 offset is multiplied by 78913 * 2^46; that
	// offset has 46 zero low bits and vanishes when the high word is narrowed to
	// int32.  The complete conversion domain fits int32.  This spelling preserves
	// the audited single-umulh Apple Clang 23/M4 assembly shape.  The native-u128
	// conjunct is a compiler-capability boundary for that audited configuration,
	// not part of the reciprocal identity.  All other targets use the constexpr
	// reciprocal whose equality is exhaustively checked above.
#if defined(__APPLE__) && defined(__SIZEOF_INT128__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	decimal_exponent = compute_decimal_exponent_high_product(binary_exponent);
#else
	decimal_exponent = compute_decimal_exponent_reduced(binary_exponent);
#endif
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	/*
	The generated shift is
	e+floor(-(d+1)*log2(10))+1+extra_shift.  Consequently the high 128 product
	bits contain six guard bits, followed by I and the complete 64-bit F of
	A=I+F/2^64.  The table entry is indexed only after callers have mapped a
	subnormal to effective exponent one and excluded non-finite exponent 2047.
	*/
	auto const shift{cached_data.exponent_shifts.data[raw_exponent]};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(binary_significand << shift, power)};
	auto integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	/*
	For an interior digit boundary, rounding 10F/B would add B/2.  The power
	cache is a downward endpoint, so the exact product may lie in the next few
	integer numerator units.  The complete modular boundary partition stated in
	the theorem proves that +6 is the smallest common bias which makes every
	non-tie boundary agree for all 2046 normal exponent classes and the
	effective-subnormal class.  It is format-specific: copying it to binary32,
	binary80, or binary128 would have no proof.  `umul64x64_add_high` computes

	    floor((10F + B/2 + 6)/B)

	exactly, without an intermediate 10F overflow.
	*/
	auto digit{static_cast<::std::uint_least32_t>(::fast_io::details::da::umul64x64_add_high(
		fractional, 10u, (static_cast<::std::uint_least64_t>(1) << 63u) + 6u))};
	if (fractional == (static_cast<::std::uint_least64_t>(1) << 62u))
	{
		/*
		F=B/4 is the only exact half where add-half chooses the odd upper
		digit: 10F/B=2.5, so nearest-even must select 2.  The other possible
		exact half is F=3B/4, where the ordinary upward result is 8 and already
		even.  Overriding after the +6 bias also proves the correction cannot
		turn the exact 2.5 tie into an upward decision.
		*/
		digit = 2u;
	}
	/*
	H is the symmetric half-ULP radius.  Adding one for even m closes both
	midpoints; odd m leaves them open.  Unsigned wrap of F+H is exactly
	B-F<=H, so `round_up` admits I+1.  `H>F` admits I.  The cell-width proof
	makes these mutually exclusive as choices of distinct coarse integers.
	*/
	auto const half_ulp{static_cast<::std::uint_least64_t>(
		(power.hi >> (extra_shift + 1u - shift)) + (1u - (binary_significand & 1u)))};
	auto const round_up{fractional + half_ulp < fractional};
	auto const round_down{half_ulp > fractional};
	// The upper coarse candidate is I+1; the lower candidate remains I.
	integral += round_up;
	/*
	When a coarse member exists it is shorter and the digit is dead.  Otherwise
	the proved nearest-even fine digit completes (10I+D)*10^d.
	*/
	return {integral, decimal_exponent, digit, !(round_up || round_down)};
}

struct binary64_scientific_precision_result
{
	// On success, significand contains exactly the requested number of decimal
	// digits and exponent is the base-ten exponent of its leading digit.  Failure
	// means only that the cached approximation could not prove rounding; the
	// exact precision path remains authoritative.
	::std::uint_least64_t significand;
	::std::int_least32_t exponent;
	bool success;
};

// Compute the correctly rounded P16-P19 scientific coefficient for a positive
// normal binary64 magnitude.  The caller supplies the implicit bit, so
// binary_significand is in [2^52, 2^53), and raw_exponent is in [1, 2046].
// This normal-only contract is essential: subnormals do not share the relative
// error bound used below and are intentionally handled by the exact path.
//
// The DA cache converts the binary value to a fixed-point decimal interval.  A
// provisional integer contributes either fifteen or sixteen leading digits;
// multiplier extends it to the requested width without materializing the
// hundreds of digits available from the exact binary expansion.
//
// P14-P15 need a quotient/remainder half-boundary proof rather than this
// multiplier interval.  A combined runtime P14/P15 entry was not retained.
// In two-link-order Apple-Clang-23/M4 measurements it added 2,504 bytes of text,
// regressed the aggregate normal controls by 0.74%, and regressed the P17/P20
// subnormal controls by 3.55%/3.02%.  A smaller P15-only entry made P15
// 4.9%--6.0% faster, but added 5,196 bytes and still regressed common/subnormal
// P20 by 2.62%/1.78%.  The extra quotient, scale and branch state therefore
// lengthens callers outside its target precision.  P14-P15 keep the exact
// fallback; this split is measured code-generation policy, not a missing
// mathematical rounding rule.
template <::std::size_t digits>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr binary64_scientific_precision_result
compute_binary64_scientific_precision(::std::uint_least64_t binary_significand,
	::std::uint_least32_t raw_exponent) noexcept
{
	static_assert(16u <= digits && digits <= 19u);
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 1075};
	::std::int_least32_t decimal_exponent;
	// Use the same proved Apple-AArch64 reciprocal identity and single-umulh
	// Apple Clang 23/M4 code-generation policy as compute_binary64.  The native-u128
	// conjunct delimits the audited compiler capability; precision changes neither
	// the identity's domain nor its integer result.  Every other configuration uses
	// the exhaustively equivalent reduced reciprocal below.
#if defined(__APPLE__) && defined(__SIZEOF_INT128__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	decimal_exponent = compute_decimal_exponent_high_product(binary_exponent);
#else
	decimal_exponent = compute_decimal_exponent_reduced(binary_exponent);
#endif
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	auto const shift{cached_data.exponent_shifts.data[raw_exponent]};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(
		binary_significand << shift, power)};
	auto const integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	auto const has_extra_digit{integral >= static_cast<::std::uint_least64_t>(1000000000000000ULL)};
	constexpr ::std::uint_least64_t extra_digit_multiplier{[]
	{
		::std::uint_least64_t value{1u};
		for (::std::size_t index{16u}; index != digits; ++index)
		{
			value *= 10u;
		}
		return value;
	}()};
	auto const multiplier{has_extra_digit ? extra_digit_multiplier :
		extra_digit_multiplier * 10u};
	auto const fractional_product{
		::fast_io::details::da::umul64x64(fractional, multiplier)};
	auto significand{integral * multiplier + fractional_product.hi};

	// Let A = I + F / 2^64 be the scaled value obtained from the cached lower
	// endpoint, where I is integral and F is fractional.  The provisional I has
	// either fifteen or sixteen digits.  M = multiplier extends it to the
	// requested width, and F * M = H * 2^64 + L contributes H to significand
	// while L = fractional_product.lo decides rounding.
	//
	// Each cached power is the lower 128-bit endpoint of its normalized exact
	// power.  With binary_significand < 2^53 and shift <= 6 (verified for the
	// complete normal domain in cache.h), the discarded cache and product tails
	// place the approximation less than 2049/2048 fixed-point units below the
	// exact value.  After scaling by M = multiplier, the greatest integral
	// distance which this one-sided error can cross is
	//
	//   B = ceil(M * 2049 / 2048) - 1
	//     = M + floor((M - 1) / 2048).
	//
	// Unsigned subtraction implements the closed interval test without another
	// branch: L - (half - B) <= B iff L is in [half - B, half].  The interval
	// therefore rejects every exact half and every value whose comparison with
	// half could change after restoring the omitted tail.
	// Every accepted value lies strictly on the same side of half as the exact
	// product.  All six nearest policies consequently agree here, since they
	// differ only at an exact tie; directed policies never dispatch this carrier.
	auto const ambiguity_bound{multiplier + ((multiplier - 1u) >> 11u)};
	constexpr ::std::uint_least64_t half{static_cast<::std::uint_least64_t>(1ULL) << 63u};
	if (fractional_product.lo - (half - ambiguity_bound) <= ambiguity_bound)
	{
		return {};
	}
	if (half < fractional_product.lo)
	{
		++significand;
	}
	auto real_exponent{decimal_exponent +
		static_cast<::std::int_least32_t>(has_extra_digit ? 16u : 15u)};
	constexpr ::std::uint_least64_t normalization_threshold{[]
	{
		::std::uint_least64_t value{1u};
		for (::std::size_t index{}; index != digits; ++index)
		{
			value *= 10u;
		}
		return value;
	}()};
	if (significand == normalization_threshold)
	{
		// Rounding 9.99... can produce 10^digits.  Dividing that coefficient
		// by ten and advancing the scientific exponent represents the same
		// decimal value while restoring the promised fixed coefficient width.
		significand = normalization_threshold / 10u;
		++real_exponent;
	}
	return {significand, real_exponent, true};
}

// This block owns every precision carrier whose production representation uses
// a native unsigned 128-bit type.  Scientific P20-P38 coefficients can exceed
// u64, so representing their complete domains requires u128.
// The fixed-fractional implementation below also keeps K16-K19 in the same u128
// result so K16-K28 has one compute/fallback contract and no second partial
// backend.  This is a representation choice, not an ISA rounding choice:
// targets without native u128 retain exact block materialization for the entire
// block, and no coefficient is ever narrowed to enable it.
#if defined(__SIZEOF_INT128__)
struct binary64_scientific_wide_precision_result
{
	// 10^38 < 2^127, so every rounded P20-P38 coefficient fits in u128.
	__uint128_t significand;
	::std::int_least32_t exponent;
	bool success;
};

// P20-P33 use the same cached lower endpoint and one-sided ambiguity proof as
// the u64 precision window above.  Preconditions are also identical:
// binary_significand is normalized, raw_exponent is in [1, 2046], and only a
// nearest policy may consume a success.  For a digit count P, callers provide
// extra_digit_multiplier = 10^(P-16), normalization_threshold = 10^P, and
// normalized_significand = 10^(P-1) from one consistent P.
//
// Only the rounded coefficient is wider.  M <= 10^18 and B remain in u64,
// while 10^33 < 2^110 fits in u128.  Keep M, B, and the half-boundary comparison
// in u64: the intended emitted shape is one 64x64-to-128 product followed by
// one u64 interval test.  Revalidate that shape, spills, and calls on AArch64
// and x86-64 when changing this helper.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr
binary64_scientific_wide_precision_result compute_binary64_scientific_wide_precision(
	::std::uint_least64_t binary_significand,
	::std::uint_least32_t raw_exponent,
	::std::uint_least64_t extra_digit_multiplier,
	__uint128_t normalization_threshold,
	__uint128_t normalized_significand) noexcept
{
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 1075};
	::std::int_least32_t decimal_exponent;
	// Same Apple-AArch64 reciprocal identity as the narrow precision carrier.
	// The outer __int128 availability guard already proves this helper can hold
	// the wide coefficient.  Apple Clang 23/M4 is the measured single-umulh case;
	// other targets use the exhaustively equivalent reduced reciprocal, so this
	// inner split controls exponent code generation only.
#if defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	decimal_exponent = compute_decimal_exponent_high_product(binary_exponent);
#else
	decimal_exponent = compute_decimal_exponent_reduced(binary_exponent);
#endif
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	auto const shift{cached_data.exponent_shifts.data[raw_exponent]};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(
		binary_significand << shift, power)};
	auto const integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	auto const has_extra_digit{integral >= static_cast<::std::uint_least64_t>(1000000000000000ULL)};
	auto const multiplier{has_extra_digit ? extra_digit_multiplier :
		extra_digit_multiplier * 10u};
	auto const fractional_product{
		::fast_io::details::da::umul64x64(fractional, multiplier)};
	auto significand{static_cast<__uint128_t>(integral) * multiplier +
		fractional_product.hi};
	// Same one-sided bound as the narrow carrier:
	// B = M + floor((M - 1) / 2048).  Reject [2^63 - B, 2^63] so every exact
	// or possible tie remains on the exact-materialization fallback.
	auto const ambiguity_bound{multiplier + ((multiplier - 1u) >> 11u)};
	constexpr ::std::uint_least64_t half{static_cast<::std::uint_least64_t>(1ULL) << 63u};
	if (fractional_product.lo - (half - ambiguity_bound) <= ambiguity_bound)
	{
		return {};
	}
	if (half < fractional_product.lo)
	{
		++significand;
	}
	auto real_exponent{decimal_exponent +
		static_cast<::std::int_least32_t>(has_extra_digit ? 16u : 15u)};
	if (significand == normalization_threshold)
	{
		// The caller supplies both constants so runtime precision dispatch can
		// share this body without a u128 division on the normalization edge.
		significand = normalized_significand;
		++real_exponent;
	}
	return {significand, real_exponent, true};
}

template <::std::size_t digits>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr
binary64_scientific_wide_precision_result compute_binary64_scientific_wide_precision(
	::std::uint_least64_t binary_significand,
	::std::uint_least32_t raw_exponent) noexcept
{
	// This one-fractional-word proof stops at P33.  M <= 10^18 fits in u64 and
	// 10^33 fits in u128.  P34's fifteen-digit provisional branch needs M=10^19,
	// whose ambiguity radius exceeds one half in this representation; the
	// independent two-word helper below covers both provisional widths.
	static_assert(20u <= digits && digits <= 33u);
	// Generate specialization constants at compile time without a P20-P33 table.
	// Runtime renderers obtain decimal scales from uint64_power10_table in
	// cache.h.
	constexpr ::std::uint_least64_t extra_digit_multiplier{[]
	{
		::std::uint_least64_t value{1u};
		for (::std::size_t index{16u}; index != digits; ++index)
		{
			value *= 10u;
		}
		return value;
	}()};
	constexpr __uint128_t normalization_threshold{[]
	{
		__uint128_t value{1u};
		for (::std::size_t index{}; index != digits; ++index)
		{
			value *= 10u;
		}
		return value;
	}()};
	return compute_binary64_scientific_wide_precision(binary_significand,
		raw_exponent, extra_digit_multiplier, normalization_threshold,
		normalization_threshold / 10u);
}

// The caller supplies a positive normalized 53-bit significand,
// 2^52 <= binary_significand < 2^53, and a finite-normal raw exponent in
// [1,2046].  P34 needs M=10^19 when the provisional integer has fifteen digits.
// Scaling the P20-P33 carrier's single 64-bit fraction by that M makes its
// discarded product tail exceed one half of an output quantum.  Retain another
// 64 product bits instead.  For X=binary_significand<<shift, cached endpoint C
// and exact endpoint C*, the cache contract gives C <= C* < C+1 and X < 2^59. Let
//
//   Q = floor(X*C / 2^6).
//
// Q contains the provisional integer followed by 128 fractional bits.  The
// discarded six product bits and cache error satisfy
//
//   0 <= X*C*/2^6 - Q < (X+2^6)/2^6 < 2^53+1.
//
// After multiplication by M, the exact value is therefore fewer than
// M*(2^53+1) units above the 128-bit fractional lower endpoint.  This is below
// 2^127 even for M=10^19.  Rejecting the closed interval
// [2^127-B,2^127], B=M*(2^53+1)-1, proves the strict comparison with one half;
// outside it nearest rounding is unique for all six nearest policies.  Both
// 10^34 and the rounded coefficient fit in 113 bits.  This numeric result is
// presentation-independent: significant P34 and scientific fractional P33
// both request the same 34-digit coefficient; trimming and radix placement
// occur only after this proof succeeds.  Keeping this helper separate preserves
// the established P20-P33 assembly and proof domain.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr
binary64_scientific_wide_precision_result
compute_binary64_scientific_p34_precision(
	::std::uint_least64_t binary_significand,
	::std::uint_least32_t raw_exponent) noexcept
{
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 1075};
	::std::int_least32_t decimal_exponent;
	// This is the same exhaustively equivalent exponent computation used by the
	// P20-P33 carrier.  Apple AArch64 retains its measured single-umulh spelling;
	// all other targets use the reduced reciprocal.  The conditional selects code
	// generation only and does not alter the P34 acceptance proof.
#if defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	decimal_exponent = compute_decimal_exponent_high_product(binary_exponent);
#else
	decimal_exponent = compute_decimal_exponent_reduced(binary_exponent);
#endif
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	auto const shift{cached_data.exponent_shifts.data[raw_exponent]};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const shifted_significand{binary_significand << shift};
	auto const upper_product{::fast_io::details::da::umul64x64(
		shifted_significand, power.hi)};
	auto const lower_product{::fast_io::details::da::umul64x64(
		shifted_significand, power.lo)};
	auto const product_middle{static_cast<::std::uint_least64_t>(
		upper_product.lo + lower_product.hi)};
	auto const product_high{static_cast<::std::uint_least64_t>(
		upper_product.hi + (product_middle < upper_product.lo))};
	auto const integral{product_high >> extra_shift};
	constexpr ::std::uint_least64_t sixteen_digit_threshold{
		static_cast<::std::uint_least64_t>(1000000000000000ULL)};
	auto const has_extra_digit{sixteen_digit_threshold <= integral};
	auto const fractional_high{static_cast<::std::uint_least64_t>(
		(product_high << (64u - extra_shift)) |
		(product_middle >> extra_shift))};
	auto const fractional_low{static_cast<::std::uint_least64_t>(
		(product_middle << (64u - extra_shift)) |
		(lower_product.lo >> extra_shift))};
	constexpr ::std::uint_least64_t extra_digit_multiplier{
		static_cast<::std::uint_least64_t>(1000000000000000000ULL)};
	auto const multiplier{has_extra_digit ? extra_digit_multiplier :
		extra_digit_multiplier * 10u};
	auto const fractional_low_product{::fast_io::details::da::umul64x64(
		fractional_low, multiplier)};
	auto const fractional_high_product{::fast_io::details::da::umul64x64(
		fractional_high, multiplier)};
	auto const fractional_product_middle{static_cast<::std::uint_least64_t>(
		fractional_high_product.lo + fractional_low_product.hi)};
	auto const fractional_product_high{static_cast<::std::uint_least64_t>(
		fractional_high_product.hi +
		(fractional_product_middle < fractional_high_product.lo))};
	auto const fractional_remainder{
		(static_cast<__uint128_t>(fractional_product_middle) << 64u) |
		fractional_low_product.lo};
	constexpr __uint128_t half{static_cast<__uint128_t>(1u) << 127u};
	constexpr __uint128_t maximum_ambiguity_bound{
		static_cast<__uint128_t>(10000000000000000000ULL) *
		((static_cast<__uint128_t>(1u) << 53u) + 1u) - 1u};
	static_assert(maximum_ambiguity_bound < half);
	auto const ambiguity_bound{static_cast<__uint128_t>(multiplier) *
		((static_cast<__uint128_t>(1u) << 53u) + 1u) - 1u};
	if (fractional_remainder - (half - ambiguity_bound) <= ambiguity_bound)
	{
		return {};
	}
	auto significand{static_cast<__uint128_t>(integral) * multiplier +
		fractional_product_high};
	if (half < fractional_remainder)
	{
		++significand;
	}
	auto real_exponent{decimal_exponent +
		static_cast<::std::int_least32_t>(has_extra_digit ? 16u : 15u)};
	constexpr __uint128_t decimal_limb{static_cast<::std::uint_least64_t>(10000000000000000000ULL)};
	constexpr __uint128_t normalization_threshold{
		decimal_limb * static_cast<::std::uint_least64_t>(1000000000000000ULL)};
	constexpr __uint128_t normalized_significand{
		decimal_limb * static_cast<::std::uint_least64_t>(100000000000000ULL)};
	static_assert(normalization_threshold < (static_cast<__uint128_t>(1u) << 113u));
	if (significand == normalization_threshold)
	{
		significand = normalized_significand;
		++real_exponent;
	}
	return {significand, real_exponent, true};
}

// Complete multiplication of two native 128-bit unsigned values. Splitting
// each operand at the 64-bit boundary gives four exact 64x64 products. Let
// p_ij denote limb i of the left operand times limb j of the right operand and
// B=2^64.  Since p_ij <= (B-1)^2, high(p_00) <= B-2, so
//
//   middle1 <= (B-1)^2 + (B-2) = B^2-B-1 < B^2.
//
// Likewise middle2 <= (B-1)^2+(B-1)=B^2-B, and its carry together with
// high(middle1) cannot overflow the returned high word because
//
//   p_11 + high(middle1) + high(middle2)
//     <= (B-1)^2 + (B-2) + (B-1) = B^2-2 < B^2.
//
// Their high halves are therefore exactly the two carries entering p_11. The
// returned pair is floor(product/2^128) and product mod 2^128 without division
// or an ISA-specific intrinsic contract.
struct uint128x2
{
	__uint128_t hi;
	__uint128_t lo;
};

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr uint128x2
umul128x128(__uint128_t left, __uint128_t right) noexcept
{
	auto const left_low{static_cast<::std::uint_least64_t>(left)};
	auto const left_high{static_cast<::std::uint_least64_t>(left >> 64u)};
	auto const right_low{static_cast<::std::uint_least64_t>(right)};
	auto const right_high{static_cast<::std::uint_least64_t>(right >> 64u)};
	auto const product00{static_cast<__uint128_t>(left_low) * right_low};
	auto const product01{static_cast<__uint128_t>(left_low) * right_high};
	auto const product10{static_cast<__uint128_t>(left_high) * right_low};
	auto const product11{static_cast<__uint128_t>(left_high) * right_high};
	auto const middle1{product10 +
		static_cast<::std::uint_least64_t>(product00 >> 64u)};
	auto const middle2{product01 +
		static_cast<::std::uint_least64_t>(middle1)};
	return {
		product11 + (middle1 >> 64u) + (middle2 >> 64u),
		(static_cast<__uint128_t>(
			static_cast<::std::uint_least64_t>(middle2)) << 64u) |
			static_cast<::std::uint_least64_t>(product00)};
}

// P38's fifteen-digit provisional branch uses M=10^23, for which the global
// P34 error bound exceeds one half of an output quantum. The low six bits
// discarded while forming Q=floor(X*C/2^6) are nevertheless known. If they
// are t, the cache endpoint contract C <= C* < C+1 proves
//
//   0 <= X*C*/2^6-Q < (X+t)/2^6.
//
// After decimal scaling, the largest integral displacement is exactly bounded
// by floor((M*(X+t)-1)/2^6). Form M*(X+t) as three 64-bit limbs, subtract one
// before shifting, and return false when any resulting bit reaches 2^127.
// Thus a true result is a proved local ambiguity radius; false requests the
// exact fallback and makes no rounding decision. This helper is mathematical
// word arithmetic only and deliberately contains no platform selection.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool
compute_binary64_p38_local_ambiguity_bound(
	__uint128_t multiplier,
	::std::uint_least64_t error_numerator,
	__uint128_t &bound) noexcept
{
	auto const multiplier_low{
		static_cast<::std::uint_least64_t>(multiplier)};
	auto const multiplier_high{
		static_cast<::std::uint_least64_t>(multiplier >> 64u)};
	auto const product_low{::fast_io::details::da::umul64x64(
		multiplier_low, error_numerator)};
	auto const product_high{::fast_io::details::da::umul64x64(
		multiplier_high, error_numerator)};
	auto middle{static_cast<::std::uint_least64_t>(
		product_low.hi + product_high.lo)};
	auto high{static_cast<::std::uint_least64_t>(
		product_high.hi + (middle < product_low.hi))};
	auto const low{static_cast<::std::uint_least64_t>(product_low.lo - 1u)};
	auto const borrow_low{product_low.lo == 0u};
	auto const middle_before_borrow{middle};
	middle = static_cast<::std::uint_least64_t>(middle - borrow_low);
	high = static_cast<::std::uint_least64_t>(high -
		(borrow_low && middle_before_borrow == 0u));
	// Shifting a three-limb value right by six fits u128 only when the high
	// limb is below 2^6. The following half comparison also rejects bit 127.
	if (64u <= high)
	{
		return false;
	}
	auto const shifted_high{static_cast<::std::uint_least64_t>(
		(middle >> 6u) | (high << 58u))};
	constexpr ::std::uint_least64_t half_high{
		static_cast<::std::uint_least64_t>(1ULL) << 63u};
	if (half_high <= shifted_high)
	{
		return false;
	}
	bound = (static_cast<__uint128_t>(shifted_high) << 64u) |
		static_cast<::std::uint_least64_t>(
			(low >> 6u) | (middle << 58u));
	return true;
}

// Convert a positive finite-normal binary64 magnitude to a correctly rounded
// P35-P38 decimal coefficient. The P34 proof already retains the provisional
// integer and 128 fractional bits of Q=floor(X*C/2^6), with
//
//   0 <= exact-Q < 2^53+1
//
// in those fractional units. P35-P37 and P38's sixteen-digit provisional
// branch multiply that bound by M and reject [2^127-B,2^127], where
// B=M*(2^53+1)-1 < 2^127. P38's M=10^23 branch uses the stricter proved local
// bound above. Outside the rejected closed interval the exact and cached
// values lie strictly on the same side of one half, so every one of the six
// nearest policies agrees. Directed policies never dispatch this result.
//
// Both the coefficient and its possible 10^digits normalization threshold fit
// u128 because 10^38 < 2^127. P39 is intentionally impossible in this result
// domain: 10^39 exceeds u128. Decimal powers are generated per specialization
// rather than stored in another table.
template <::std::size_t digits>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr
binary64_scientific_wide_precision_result
compute_binary64_scientific_p35_p38_precision(
	::std::uint_least64_t binary_significand,
	::std::uint_least32_t raw_exponent) noexcept
{
	static_assert(35u <= digits && digits <= 38u);
	auto const binary_exponent{
		static_cast<::std::int_least32_t>(raw_exponent) - 1075};
	::std::int_least32_t decimal_exponent;
	// The high-product spelling is exhaustively equivalent to the reduced
	// reciprocal over the complete binary64 exponent domain. Apple AArch64
	// keeps the measured single-umulh form used by the complete P20-P38 window;
	// this split changes exponent code generation only, not the rounding proof.
#if defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	decimal_exponent = compute_decimal_exponent_high_product(binary_exponent);
#else
	decimal_exponent = compute_decimal_exponent_reduced(binary_exponent);
#endif
	constexpr ::std::uint_least8_t extra_shift{
		exponent_shift_cache::extra_shift};
	auto const shift{cached_data.exponent_shifts.data[raw_exponent]};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const shifted_significand{binary_significand << shift};
	auto const upper_product{::fast_io::details::da::umul64x64(
		shifted_significand, power.hi)};
	auto const lower_product{::fast_io::details::da::umul64x64(
		shifted_significand, power.lo)};
	auto const product_middle{static_cast<::std::uint_least64_t>(
		upper_product.lo + lower_product.hi)};
	auto const product_high{static_cast<::std::uint_least64_t>(
		upper_product.hi + (product_middle < upper_product.lo))};
	auto const integral{product_high >> extra_shift};
	constexpr ::std::uint_least64_t sixteen_digit_threshold{
		static_cast<::std::uint_least64_t>(1000000000000000ULL)};
	auto const has_extra_digit{sixteen_digit_threshold <= integral};
	auto const fractional_high{static_cast<::std::uint_least64_t>(
		(product_high << (64u - extra_shift)) |
		(product_middle >> extra_shift))};
	auto const fractional_low{static_cast<::std::uint_least64_t>(
		(product_middle << (64u - extra_shift)) |
		(lower_product.lo >> extra_shift))};
	auto const fractional{
		(static_cast<__uint128_t>(fractional_high) << 64u) |
		fractional_low};
	constexpr __uint128_t extra_digit_multiplier{[]
	{
		__uint128_t value{1u};
		for (::std::size_t index{16u}; index != digits; ++index)
		{
			value *= 10u;
		}
		return value;
	}()};
	auto const multiplier{has_extra_digit ? extra_digit_multiplier :
		extra_digit_multiplier * 10u};
	constexpr __uint128_t half{static_cast<__uint128_t>(1u) << 127u};
	constexpr __uint128_t error_units{
		(static_cast<__uint128_t>(1u) << 53u) + 1u};
	__uint128_t ambiguity_bound;
	if constexpr (digits == 38u)
	{
		// The local proof below retains the six bits discarded by Q=floor(X*C/64)
		// and its helper divides the exact 192-bit bound by 64.  Fail compilation
		// if the cache guard width changes; silently reusing those constants would
		// invalidate both `discarded_mask` and the ambiguity-radius proof.
		static_assert(exponent_shift_cache::extra_shift == 6u);
		if (has_extra_digit)
		{
			ambiguity_bound = multiplier * error_units - 1u;
			static_assert(extra_digit_multiplier * error_units - 1u < half);
		}
		else
		{
			constexpr ::std::uint_least64_t discarded_mask{
				(static_cast<::std::uint_least64_t>(1u) << extra_shift) - 1u};
			auto const discarded_product_bits{static_cast<::std::uint_least64_t>(
				lower_product.lo & discarded_mask)};
			if (!::fast_io::details::da::
				compute_binary64_p38_local_ambiguity_bound(multiplier,
					shifted_significand + discarded_product_bits,
					ambiguity_bound))
			{
				return {};
			}
		}
	}
	else
	{
		ambiguity_bound = multiplier * error_units - 1u;
		constexpr auto maximum_multiplier{extra_digit_multiplier * 10u};
		static_assert(maximum_multiplier * error_units - 1u < half);
	}
	auto const fractional_product{
		::fast_io::details::da::umul128x128(fractional, multiplier)};
	if (fractional_product.lo - (half - ambiguity_bound) <= ambiguity_bound)
	{
		return {};
	}
	auto significand{static_cast<__uint128_t>(integral) * multiplier +
		fractional_product.hi};
	if (half < fractional_product.lo)
	{
		++significand;
	}
	auto real_exponent{decimal_exponent +
		static_cast<::std::int_least32_t>(has_extra_digit ? 16u : 15u)};
	constexpr __uint128_t normalization_threshold{[]
	{
		__uint128_t value{1u};
		for (::std::size_t index{}; index != digits; ++index)
		{
			value *= 10u;
		}
		return value;
	}()};
	if (significand == normalization_threshold)
	{
		significand = normalization_threshold / 10u;
		++real_exponent;
	}
	return {significand, real_exponent, true};
}

struct binary64_fixed_fractional_precision_result
{
	// The coefficient has `significant` decimal digits.  real_exponent is the
	// exponent of its leading digit after the possible all-nine carry.  A false
	// success flag means that the exact fixed-precision implementation must decide
	// the result; it does not describe a malformed input.
	__uint128_t significand;
	::std::int_least32_t real_exponent;
	::std::size_t significant;
	bool success;
};

struct binary64_fixed_fractional_low_precision_result
{
	// rounded_integer is round(abs(value) * 10^P). decimal_digits is its
	// post-carry decimal width, so the presentation layer can insert the radix
	// point without recounting digits. A false result is an ambiguity rejection;
	// the exact fixed-precision implementation remains authoritative.
	::std::uint_least64_t rounded_integer;
	::std::uint_least32_t decimal_digits;
	bool success;
};

// A single unsigned interval is cheaper at the public call site than computing
// a decimal exponent on every fixed value. Generate its conservative hull from
// the same reduced exponent function used by the converter: P1--P14 can succeed
// only where the cache exponent is in [-30,-1]. This is constexpr computation,
// not a stored exponent table. The unsigned `exponent - minimum <= span` test
// rejects values on either side, including raw exponent zero, with one compare.
struct binary64_fixed_fractional_low_exponent_union_type
{
	::std::uint_least32_t minimum;
	::std::uint_least32_t span;
};

[[nodiscard]] consteval binary64_fixed_fractional_low_exponent_union_type
make_binary64_fixed_fractional_low_exponent_union() noexcept
{
	::std::uint_least32_t minimum{2047u};
	::std::uint_least32_t maximum{};
	for (::std::uint_least32_t exponent{1u}; exponent != 2047u; ++exponent)
	{
		auto const decimal_exponent{compute_decimal_exponent_reduced(
			static_cast<::std::int_least32_t>(exponent) - 1075)};
		if (-30 <= decimal_exponent && decimal_exponent <= -1)
		{
			if (minimum == 2047u)
			{
				minimum = exponent;
			}
			maximum = exponent;
		}
	}
	return {minimum, maximum - minimum};
}

inline constexpr auto binary64_fixed_fractional_low_exponent_union{
	make_binary64_fixed_fractional_low_exponent_union()};

// Compute the nearest rounded integer for the low fixed-fractional window.
// The public dispatcher supplies a positive normal binary64 significand and a
// runtime precision P in [1,14]. Let e10 be decimal_exponent and let N be the
// 15- or 16-digit provisional width selected below. The DA cache product gives
// a lower fixed-point approximation
//
//   A = integral + fractional / 2^64
//
// to the exact normalized carrier C = abs(value) * 10^(-e10-1). The cache
// proof bounds its one-sided error by
//
//   0 <= C - A < (2049 / 2048) / 2^64.
//
// The requested result is round(abs(value) * 10^P). Before a decimal carry it
// has
//
//   K = e10 + N + 1 + P
//
// digits. For 1 <= K <= 15, define D = 10^(N-K). Then
// abs(value) * 10^P = C / D. Dividing integral by D supplies the candidate
// quotient; the exact half comparison, scaled by 2^64, is
//
//   ((integral mod D) * 2^64 + fractional) ? D * 2^63.
//
// Division by D cannot amplify the cache error. Since 2049/2048 is strictly
// between one and two low-word units, only the two integer residuals half-1 and
// half can conceal an exact half boundary. Rejecting that closed pair proves
// every accepted comparison. Away from an exact tie all six nearest policies
// choose the same endpoint, which is why their presentation paths may share
// this carrier. The generated 10^0..10^19 data are reused; this specialization
// adds no numeric table.
template <::std::size_t minimum_precision = 1u,
	::std::size_t maximum_precision = 14u>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr
binary64_fixed_fractional_low_precision_result
compute_binary64_fixed_fractional_low_precision(
	::std::uint_least64_t binary_significand,
	::std::uint_least32_t raw_exponent,
	::std::size_t fractional_precision,
	::std::int_least32_t decimal_exponent) noexcept
{
	if (fractional_precision - minimum_precision >
		maximum_precision - minimum_precision || !raw_exponent ||
		2046u < raw_exponent)
	{
		return {};
	}
	constexpr ::std::uint_least8_t extra_shift{
		exponent_shift_cache::extra_shift};
	auto const shift{cached_data.exponent_shifts.data[raw_exponent]};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(
		binary_significand << shift, power)};
	auto const integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) |
		(product.lo >> extra_shift))};
	constexpr ::std::uint_least64_t sixteen_digit_threshold{
		static_cast<::std::uint_least64_t>(1000000000000000ULL)};
	auto const provisional_digits{
		static_cast<::std::size_t>(integral >= sixteen_digit_threshold ?
			16u : 15u)};
	auto const significant_signed{
		static_cast<::std::int_least64_t>(decimal_exponent) +
		static_cast<::std::int_least64_t>(provisional_digits) + 1 +
		static_cast<::std::int_least64_t>(fractional_precision)};
	if (significant_signed < 1 || 15 < significant_signed)
	{
		return {};
	}
	auto const significant{static_cast<::std::size_t>(significant_signed)};
	auto const discarded{provisional_digits - significant};
	auto const divisor{uint64_power10_table[discarded]};
	auto rounded{static_cast<::std::uint_least64_t>(integral / divisor)};
	auto const integral_remainder{static_cast<::std::uint_least64_t>(
		integral - rounded * divisor)};
	auto const remainder{(static_cast<__uint128_t>(integral_remainder) << 64u) |
		fractional};
	auto const half{static_cast<__uint128_t>(divisor) << 63u};
	// `remainder` is the lower approximation. Exact values below half-1
	// cannot reach half; values above half are already on the upper side.
	// The two remaining residuals fall back so tie direction and endpoint
	// inclusiveness are decided by exact arithmetic.
	if (remainder - (half - 1u) <= 1u)
	{
		return {};
	}
	if (half < remainder)
	{
		++rounded;
	}
	auto decimal_digits{static_cast<::std::uint_least32_t>(significant)};
	if (rounded == uint64_power10_table[significant])
	{
		++decimal_digits;
	}
	return {rounded, decimal_digits, true};
}

// Convert the profitable runtime fixed-fractional window without constructing
// the complete exact binary expansion.  Write the positive normal input as
//
//   x = d * 10^r,  1 <= d < 10.
//
// Rounding x to P digits after the decimal point is rounding x * 10^P to an
// integer.  Before an all-nine carry, that integer has
//
//   K = r + 1 + P
//
// digits, so it is exactly the K-significant-digit scientific DA carrier.  The
// implementation nevertheless remains a separate runtime body: the scientific
// P16-P33 templates deliberately retain compile-time scales and writer widths
// where assembly measurements show shorter dependency chains.  Numeric cache
// data and the generated 10^0..10^19 table are shared.
//
// The accepted triangle is a performance policy established by paired M4,
// x86-64 GCC and x86-64 Clang measurements:
//
//   4 <= P <= 13 and 16 <= K <= P + 15, or
//   P == 14       and 16 <= K <= 28.
//
// It implies 2 <= r + 1 <= 15 before carry.  Thus the matching writer always
// has a nonempty integer part and a nonempty requested fractional part.  P15+
// remains on the existing exact scaled path.  Values outside this measured
// region, subnormals and every ambiguous half-boundary return failure.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr
binary64_fixed_fractional_precision_result compute_binary64_fixed_fractional_precision(
	::std::uint_least64_t binary_significand,
	::std::uint_least32_t raw_exponent,
	::std::size_t fractional_precision) noexcept
{
	// This helper is normal-only.  Keep the check local as well as at the call
	// site because cached_data.exponent_shifts has no subnormal error contract.
	// Rejecting P outside the measured interval before its signed conversion also
	// makes the subsequent K arithmetic defined for every size_t input.
	if (fractional_precision < 4u || 14u < fractional_precision ||
		!raw_exponent || 2046u < raw_exponent)
	{
		return {};
	}
	auto const binary_exponent{static_cast<::std::int_least32_t>(raw_exponent) - 1075};
	::std::int_least32_t decimal_exponent;
	// Apple AArch64 uses the proved unsigned-high-product spelling already used
	// by the scientific DA carrier.  For negative e, conversion to u64 adds 2^64;
	// multiplying by 78913*2^46 adds a high-word offset whose low 32 bits are
	// zero, so narrowing recovers floor(e*78913/2^18).  The exhaustive verifier
	// above covers the entire binary64 domain.  This target split is retained
	// because M-series assembly measurements require one umulh without a signed
	// correction chain in the measured Apple Clang 23/M4 caller.  Later Apple
	// front ends inherit that layout as a conservative hypothesis; other targets
	// generate their semantically equivalent form from the reduced reciprocal.
#if defined(__APPLE__) && \
	(defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64))
	decimal_exponent = compute_decimal_exponent_high_product(binary_exponent);
#else
	decimal_exponent = compute_decimal_exponent_reduced(binary_exponent);
#endif
	constexpr ::std::uint_least8_t extra_shift{exponent_shift_cache::extra_shift};
	auto const shift{cached_data.exponent_shifts.data[raw_exponent]};
	auto const power{cached_data.powers[-decimal_exponent - 1]};
	auto const product{::fast_io::details::da::umul64x128_high(
		binary_significand << shift, power)};
	auto const integral{product.hi >> extra_shift};
	auto const fractional{static_cast<::std::uint_least64_t>(
		(product.hi << (64u - extra_shift)) | (product.lo >> extra_shift))};
	auto const has_extra_digit{integral >= static_cast<::std::uint_least64_t>(1000000000000000ULL)};
	auto real_exponent{decimal_exponent +
		static_cast<::std::int_least32_t>(has_extra_digit ? 16u : 15u)};
	auto const significant_signed{static_cast<::std::int_least64_t>(real_exponent) + 1 +
		static_cast<::std::int_least64_t>(fractional_precision)};
	if (significant_signed < 16 || 33 < significant_signed)
	{
		return {};
	}
	auto const significant{static_cast<::std::size_t>(significant_signed)};
	auto const portable_triangle{
		(4u <= fractional_precision && fractional_precision <= 13u &&
		 significant <= fractional_precision + 15u) ||
		(fractional_precision == 14u && significant <= 28u)};
	if (!portable_triangle)
	{
		return {};
	}

	// K <= 33 makes 10^(K-16) fit u64.  The generated table is the same storage
	// used by scientific precision and the older exact-window code; this path
	// introduces no fixed-specific numeric table.
	auto const extra_digit_multiplier{uint64_power10_table[significant - 16u]};
	auto const multiplier{has_extra_digit ? extra_digit_multiplier :
		extra_digit_multiplier * 10u};
	auto const fractional_product{
		::fast_io::details::da::umul64x64(fractional, multiplier)};
	auto significand{static_cast<__uint128_t>(integral) * multiplier +
		fractional_product.hi};

	// Let A = I + F/2^64 be the cached lower approximation and M be multiplier.
	// The cache/product proof bounds the omitted one-sided tail by less than
	// 2049/2048 units.  Scaling by M can therefore cross at most
	//
	//   B = ceil(M * 2049 / 2048) - 1
	//     = M + floor((M - 1) / 2048)
	//
	// integer positions.  Rejecting the closed approximate interval
	// [2^63-B, 2^63] includes every exact tie and every comparison whose side can
	// change when the omitted tail is restored.  Hence each accepted comparison
	// is on the exact side of half, and all six nearest policies agree.  Directed
	// rounding never dispatches this helper.
	auto const ambiguity_bound{multiplier + ((multiplier - 1u) >> 11u)};
	constexpr ::std::uint_least64_t half{static_cast<::std::uint_least64_t>(1ULL) << 63u};
	if (fractional_product.lo - (half - ambiguity_bound) <= ambiguity_bound)
	{
		return {};
	}
	if (half < fractional_product.lo)
	{
		++significand;
	}

	// Build 10^K and 10^(K-1) from the shared u64 table.  Splitting at 10^19
	// avoids runtime u128 division while covering the complete K16..K33 domain.
	constexpr __uint128_t decimal_limb{static_cast<::std::uint_least64_t>(10000000000000000000ULL)};
	auto const normalization_threshold{significant <= 19u
		? static_cast<__uint128_t>(uint64_power10_table[significant])
		: decimal_limb * uint64_power10_table[significant - 19u]};
	auto const normalized_significand{significant <= 19u
		? static_cast<__uint128_t>(uint64_power10_table[significant - 1u])
		: decimal_limb * uint64_power10_table[significant - 20u]};
	if (significand == normalization_threshold)
	{
		// 9.99... rounded to 10^K is represented as 10^(K-1) with r+1.
		// The fixed writer appends the newly implied final zero, so the rounded
		// value and the requested P-place field are unchanged.
		significand = normalized_significand;
		++real_exponent;
	}
	return {significand, real_exponent, significant, true};
}
#endif

} // namespace fast_io::details::da

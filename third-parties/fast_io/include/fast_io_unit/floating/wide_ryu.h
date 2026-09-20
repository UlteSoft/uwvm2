#pragma once

#include "punning.h"
#if defined(__SIZEOF_INT128__)

#include "wide_ryu_cache.h"

namespace fast_io::details
{

struct wide_ryu_result
{
	__uint128_t m10{};
	::std::int_least32_t e10{};
	bool success{};
};

/*
Fixed-width nearest-even conversion for binary80 and binary128
==============================================================

This is the wide analogue of the Schubfach/DA interval construction.  Let the
positive finite source be

    x = m2 * 2^(e2+2),

where the subtraction of two in `e2` reserves two endpoint bits.  In the
regular case its nearest-even parsing interval, after multiplication by four,
has integer endpoints

    mm = 4*m2 - 1 - mm_shift,  mv = 4*m2,  mp = 4*m2 + 2.       (1)

`mm_shift` is one except at a normal power of two.  At such a power the
predecessor lies in the lower binade and is only half as far away, so (1)
subtracts one rather than two endpoint units.  This is exactly the asymmetric
interval proved for `wide_shortest_make_interval`.  An even m2 admits both
midpoints; an odd m2 rejects both.

The binary-to-decimal scaling has two exhaustive cases.

* e2>=0: divide the three integers in (1) by 5^q while compensating with a
  power of two.
* e2<0: multiply them by 5^i and compensate with a power of two.

The cache stores 249 significant bits of the required power.  Each split entry
is reconstructed from a 56-step base and a small exact 5^r; the generated
two-bit correction tables are the exhaustive error certificate for that
reconstruction.  `mulShift` forms the high quotient of the complete 128x256
product.  Therefore vr, vm, and vp below are the exact integer floors required
by the scaled interval, not floating approximations.  The five tables occupy
less than 10 KiB in total and each individual object is below 3 KiB.

After scaling, deleting a decimal digit maps (vm,vr,vp) to their floor
quotients by ten.  The deletion is legal exactly while

    floor(vp/10) > floor(vm/10),                                (2)

because then at least one shorter decimal grid point remains strictly between
the interval endpoints.  Repeating (2) proves minimal digit count.  The
trailing-zero predicates retain the endpoint-closure information lost by a
floor: equality with an accepted even endpoint is legal, while equality with
an open endpoint is not.

At termination, `last_removed_digit` and `vrIsTrailingZeros` are an exact
guard/sticky pair.  A guard five with zero sticky is the sole decimal midpoint.
Changing it to four when the retained coefficient is even implements
round-to-nearest, ties-to-even; every other guard uses the ordinary >=5 test.
The final increment also repairs a retained lower endpoint that is open.
Thus the returned `(m10,e10)` is the shortest member of precisely the same
rounding interval as the complete exact backend, with the same nearest decimal
and tie rule.

The cache arithmetic below is used only at run time.  Constant evaluation keeps
the complete dyadic/decimal proof backend.  Their equality is not an empirical
cross-check: both minimize decimal precision over the same interval (1), then
choose the closest bracketing grid point with the same even tie predicate.
That specification has one canonical result, so the two implementations are
extensionally equal for every finite binary80/binary128 field.
*/
template <typename flt>
	requires(
		(iec559_traits<flt>::mbits == 63u ||
		 iec559_traits<flt>::mbits == 112u) &&
		iec559_traits<flt>::ebits == 15u)
[[nodiscard]] inline ::fast_io::details::wide_ryu_result
wide_ryu_nearest_even(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t binary_exponent) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using namespace ::fast_io::details::wide_ryu;
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) -
		1u};
	if (binary_exponent == exponent_mask)
	{
		/*
		The caller classifies Inf/NaN before the fast core.  Keeping the guard
		here makes the field-level helper total and prevents a special exponent
		from indexing a cached power outside its proved range.
		*/
		return {};
	}
	if (!mantissa && !binary_exponent)
	{
		return {0u, 0, true};
	}

	constexpr auto bias{
		static_cast<::std::int_least32_t>(
			(static_cast<::std::uint_least32_t>(1u)
			 << (trait::ebits - 1u)) -
			1u)};
	auto m2{static_cast<__uint128_t>(mantissa)};
	::std::int_least32_t e2{};
	if (binary_exponent)
	{
		m2 |= static_cast<__uint128_t>(1u) << trait::mbits;
		e2 = static_cast<::std::int_least32_t>(
			binary_exponent) -
			bias - static_cast<::std::int_least32_t>(trait::mbits) -
			2;
	}
	else
	{
		e2 = 1 - bias -
			static_cast<::std::int_least32_t>(trait::mbits) - 2;
	}
	auto const accept_bounds{(m2 & 1u) == 0u};
	auto const mv{m2 << 2u};
	auto const mm_shift{
		static_cast<::std::uint_least32_t>(
			mantissa != 0u || binary_exponent == 0u)};

	__uint128_t vr{};
	__uint128_t vp{};
	__uint128_t vm{};
	::std::int_least32_t e10{};
	bool vm_is_trailing_zeros{};
	bool vr_is_trailing_zeros{};

	if (0 <= e2)
	{
		/*
		q=floor(log10(2^e2))-1 for e2>3 and zero otherwise.  Hence
		10^q is the largest selected decimal scale strictly below the binary
		scale, and the ensuing shift i lies in (128,256), the proved domain of
		mulShift.  Subtracting `(e2>3)` avoids a max operation without changing
		the q=0 boundary.
		*/
		auto const q{
			log10Pow2(e2) -
			static_cast<::std::uint_least32_t>(3 < e2)};
		e10 = static_cast<::std::int_least32_t>(q);
		auto const k{
			float_128_pow5_inv_bitcount +
			static_cast<::std::int_least32_t>(pow5bits(q)) - 1};
		auto const shift{
			-e2 + static_cast<::std::int_least32_t>(q) + k};
		::std::uint_least64_t power[4];
		generic_computeInvPow5(q, power);
		vr = mulShift(mv, power, shift);
		vp = mulShift(mv + 2u, power, shift);
		vm = mulShift(
			mv - 1u - mm_shift, power, shift);

		if (q <= 55u)
		{
			/*
			Only one of the consecutive endpoint numerators can contain
			5^q.  These mutually exclusive branches recover exact divisibility
			at a floor boundary.  q-1 is used for mv because its factor four
			already supplies the binary part of the decimal scale.
			*/
			if (mv % 5u == 0u)
			{
				vr_is_trailing_zeros =
					multipleOfPowerOf5(mv, q - 1u);
			}
			else if (accept_bounds)
			{
				vm_is_trailing_zeros =
					multipleOfPowerOf5(
						mv - 1u - mm_shift, q);
			}
			else
			{
				vp -= static_cast<__uint128_t>(
					multipleOfPowerOf5(mv + 2u, q));
			}
		}
	}
	else
	{
		/*
		For a negative binary exponent, i=-e2-q is nonnegative and the
		cached 5^i multiplication replaces division by 2^-e2.  The definitions
		of k and shift align the same 249-bit fixed-point quotient as the
		inverse-power branch, so both sides produce identical interval units at
		e2=0.
		*/
		auto const q{
			log10Pow5(-e2) -
			static_cast<::std::uint_least32_t>(-e2 > 1)};
		e10 = static_cast<::std::int_least32_t>(q) + e2;
		auto const i{
			-e2 - static_cast<::std::int_least32_t>(q)};
		auto const k{
			static_cast<::std::int_least32_t>(
				pow5bits(static_cast<::std::uint_least32_t>(i))) -
			float_128_pow5_bitcount};
		auto const shift{
			static_cast<::std::int_least32_t>(q) - k};
		::std::uint_least64_t power[4];
		generic_computePow5(
			static_cast<::std::uint_least32_t>(i), power);
		vr = mulShift(mv, power, shift);
		vp = mulShift(mv + 2u, power, shift);
		vm = mulShift(
			mv - 1u - mm_shift, power, shift);

		if (q <= 1u)
		{
			/*
			mv has two trailing binary zeroes.  Thus q<=1 makes its scaled
			decimal quotient exact.  The lower endpoint has one trailing zero
			exactly when mm_shift is one; an open upper endpoint is moved inward
			by one unit.
			*/
			vr_is_trailing_zeros = true;
			if (accept_bounds)
			{
				vm_is_trailing_zeros = mm_shift == 1u;
			}
			else
			{
				--vp;
			}
		}
		else if (q < 127u)
		{
			/*
			The negative e2 already supplies every required factor of five.
			Exact decimal divisibility is therefore equivalent to mv containing
			q-1 factors of two.  q<127 keeps the u128 mask shift defined.
			*/
			vr_is_trailing_zeros =
				multipleOfPowerOf2(mv, q - 1u);
		}
	}

	::std::uint_least32_t removed{};
	::std::uint_least8_t last_removed_digit{};
	while (vp / 10u > vm / 10u)
	{
		vm_is_trailing_zeros &=
			vm % 10u == 0u;
		vr_is_trailing_zeros &=
			last_removed_digit == 0u;
		last_removed_digit =
			static_cast<::std::uint_least8_t>(vr % 10u);
		vr /= 10u;
		vp /= 10u;
		vm /= 10u;
		++removed;
	}
	if (vm_is_trailing_zeros)
	{
		/*
		If the accepted lower endpoint remains a multiple of ten, equality
		survives another digit deletion even though the strict quotient test has
		converged.  Repeating until its first nonzero digit is both necessary and
		sufficient; an open endpoint never enters this branch.
		*/
		while (vm % 10u == 0u)
		{
			vr_is_trailing_zeros &=
				last_removed_digit == 0u;
			last_removed_digit =
				static_cast<::std::uint_least8_t>(vr % 10u);
			vr /= 10u;
			vp /= 10u;
			vm /= 10u;
			++removed;
		}
	}
	if (vr_is_trailing_zeros &&
		last_removed_digit == 5u &&
		(vr & 1u) == 0u)
	{
		/*
		The discarded suffix is exactly 500...0 and the retained integer is
		even.  Replacing guard five by four is an algebraic encoding of "do not
		increment"; it cannot affect any non-tie branch.
		*/
		last_removed_digit = 4u;
	}
	auto const output{
		vr + static_cast<__uint128_t>(
			(vr == vm &&
			 (!accept_bounds || !vm_is_trailing_zeros)) ||
			5u <= last_removed_digit)};
	return {
		output,
		static_cast<::std::int_least32_t>(
			e10 + static_cast<::std::int_least32_t>(removed)),
		true};
}

/*
Policy-complete fixed-width interval conversion
===============================================

The nearest-even leaf above deliberately retains Ryu's specialized endpoint
bookkeeping: it is the overwhelmingly common path and has already been reduced
to the smallest measured instruction schedule.  The other nine explicit
policies use the same cached products through the interval-normalization
theorem below.  Keeping this as a separate template prevents the extra endpoint
state from increasing nearest-even register pressure or binary size.

Write the positive target as x=m*2^b and measure every endpoint in quarter-ulp
units 2^(b-2), so X=4m.  The six nearest policies have the exact interval

    [or ( X-(1+s), X+2 )or],                                  (3)

where s=1 except at a normal power of two.  At that boundary the predecessor
comes from the lower binade and is one half-ulp below x, hence its midpoint is
only one quarter-ulp below x and s=0.  The endpoint brackets in (3) are chosen
independently by the requested tie policy.

For a directed policy let `right_closed` mean that parsing rounds values below
x upward to x.  Its interval is

    (X-d, X]  if right_closed,     [X, X+4) otherwise,         (4)

where d=2 at a normal power of two and d=4 elsewhere.  Equations (3) and (4)
are exactly the dyadic intervals constructed by wide_shortest_make_interval;
only their common denominator has been made explicit.

After cached-power scaling, suppose an endpoint y has floor F and `exact`
states whether y=F.  The integer decimal coefficients admitted at that scale
are the inclusive interval

    L = F_lower + (!lower_exact || !lower_closed),
    U = F_upper - ( upper_exact && !upper_closed).             (5)

Equation (5) handles all four combinations of open/closed and
integral/nonintegral endpoints without an endpoint-specific removal loop.
Moving to the next coarser decimal grid replaces [L,U] by

    [ceil(L/10), floor(U/10)].                                 (6)

If (6) is nonempty, let y be an admitted point on that coarser grid.  On the
current grid y has a coefficient divisible by ten.  Monotonicity of the
rounding interval also admits the target-adjacent current-grid point z between
x and y.  If z!=y, the normalized coefficient of y has fewer significant
digits than z; if z=y, both grids normalize to the identical decimal.
Therefore coarsening either improves significant-digit count or preserves the
same canonical value while floor(x/h)>=10.

The sole transition needing a stop is floor(x/h)<10.  A nonzero result already
has one significant digit there, so the 10h grid cannot improve the primary
minimum-digit objective.  Its admitted point can instead be a farther
one-digit decimal across a power-of-ten boundary.  The loop consequently stops
before that transition, as proved at the guard below.  Repeating (6) subject to
this stop computes the minimum significant-digit count and, among equally
short candidates, preserves the closest decimal/tie ordering.

At that grid, floor(x) and floor(x)+1 are the only candidates adjacent to x.
For nearest policies the removed guard/sticky pair chooses the closer one and
the policy resolves exact half-way cases.  For directed policies the requested
direction chooses one of the same pair.  Membership in [L,U] is checked before
returning, so if the preferred point lies beyond an open endpoint the other
adjacent point is selected.  This is the fixed-width form of the bracketing
completeness proof used by wide_shortest_from_binary.
*/
template <::fast_io::manipulators::floating_rounding rounding>
inline constexpr bool wide_ryu_policy_is_nearest{
	rounding ==
		::fast_io::manipulators::floating_rounding::nearest_to_even ||
	rounding ==
		::fast_io::manipulators::floating_rounding::nearest_to_odd ||
	rounding == ::fast_io::manipulators::floating_rounding::
					nearest_toward_plus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::
					nearest_toward_minus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::
					nearest_toward_zero ||
	rounding == ::fast_io::manipulators::floating_rounding::
					nearest_away_from_zero};

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool
wide_ryu_nearest_left_closed(bool negative, bool even) noexcept
{
	if constexpr (
		rounding ==
		::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		return even;
	}
	else if constexpr (
		rounding ==
		::fast_io::manipulators::floating_rounding::nearest_to_odd)
	{
		return !even;
	}
	else if constexpr (
		rounding == ::fast_io::manipulators::floating_rounding::
						nearest_toward_plus_infinity)
	{
		/*
		At the lower midpoint, the positive neighbor selected by a +infinity
		tie is x; after sign reflection it is x exactly when the source is
		nonnegative.
		*/
		return !negative;
	}
	else if constexpr (
		rounding == ::fast_io::manipulators::floating_rounding::
						nearest_toward_minus_infinity)
	{
		return negative;
	}
	else if constexpr (
		rounding == ::fast_io::manipulators::floating_rounding::
						nearest_away_from_zero)
	{
		return true;
	}
	else
	{
		// A lower midpoint is closer to zero than x, so ties-to-zero rejects it.
		return false;
	}
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool
wide_ryu_nearest_right_closed(bool negative, bool even) noexcept
{
	if constexpr (
		rounding ==
		::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		return even;
	}
	else if constexpr (
		rounding ==
		::fast_io::manipulators::floating_rounding::nearest_to_odd)
	{
		return !even;
	}
	else if constexpr (
		rounding == ::fast_io::manipulators::floating_rounding::
						nearest_toward_plus_infinity)
	{
		return negative;
	}
	else if constexpr (
		rounding == ::fast_io::manipulators::floating_rounding::
						nearest_toward_minus_infinity)
	{
		return !negative;
	}
	else if constexpr (
		rounding == ::fast_io::manipulators::floating_rounding::
						nearest_toward_zero)
	{
		return true;
	}
	else
	{
		// A right midpoint is farther from zero than x, so away rejects x.
		return false;
	}
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool
wide_ryu_directed_round_up(bool negative) noexcept
{
	if constexpr (
		rounding ==
			::fast_io::manipulators::floating_rounding::
				toward_plus_infinity)
	{
		return !negative;
	}
	else if constexpr (
		rounding ==
			::fast_io::manipulators::floating_rounding::
				toward_minus_infinity)
	{
		return negative;
	}
	else if constexpr (
		rounding ==
			::fast_io::manipulators::floating_rounding::away_from_zero)
	{
		return true;
	}
	else
	{
		static_assert(
			rounding ==
			::fast_io::manipulators::floating_rounding::toward_zero);
		return false;
	}
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool wide_ryu_tie_choose_upper(
	__uint128_t lower, bool negative) noexcept
{
	if constexpr (
		rounding ==
		::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		// Consecutive integers have opposite parity, so exactly one is even.
		return (lower & 1u) != 0u;
	}
	else if constexpr (
		rounding ==
		::fast_io::manipulators::floating_rounding::nearest_to_odd)
	{
		return (lower & 1u) == 0u;
	}
	else if constexpr (
		rounding == ::fast_io::manipulators::floating_rounding::
						nearest_toward_plus_infinity)
	{
		return !negative;
	}
	else if constexpr (
		rounding == ::fast_io::manipulators::floating_rounding::
						nearest_toward_minus_infinity)
	{
		return negative;
	}
	else if constexpr (
		rounding == ::fast_io::manipulators::floating_rounding::
						nearest_away_from_zero)
	{
		return true;
	}
	else
	{
		static_assert(
			rounding == ::fast_io::manipulators::floating_rounding::
							nearest_toward_zero);
		return false;
	}
}

template <typename flt,
	::fast_io::manipulators::floating_rounding rounding>
	requires(
		(iec559_traits<flt>::mbits == 63u ||
		 iec559_traits<flt>::mbits == 112u) &&
		iec559_traits<flt>::ebits == 15u &&
		rounding !=
			::fast_io::manipulators::floating_rounding::
				nearest_to_even &&
		rounding !=
			::fast_io::manipulators::floating_rounding::
				current_environment)
[[nodiscard]] inline ::fast_io::details::wide_ryu_result
wide_ryu_policy(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t binary_exponent, bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using namespace ::fast_io::details::wide_ryu;
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) -
		1u};
	if (binary_exponent == exponent_mask)
	{
		// Special encodings have no finite rounding interval or cache index.
		return {};
	}
	if (!mantissa && !binary_exponent)
	{
		return {0u, 0, true};
	}

	constexpr auto bias{
		static_cast<::std::int_least32_t>(
			(static_cast<::std::uint_least32_t>(1u)
			 << (trait::ebits - 1u)) -
			1u)};
	auto m2{static_cast<__uint128_t>(mantissa)};
	::std::int_least32_t e2{};
	if (binary_exponent)
	{
		m2 |= static_cast<__uint128_t>(1u) << trait::mbits;
		e2 = static_cast<::std::int_least32_t>(
			binary_exponent) -
			bias - static_cast<::std::int_least32_t>(trait::mbits) -
			2;
	}
	else
	{
		e2 = 1 - bias -
			static_cast<::std::int_least32_t>(trait::mbits) - 2;
	}

	auto const target_numerator{m2 << 2u};
	__uint128_t lower_numerator{};
	__uint128_t upper_numerator{};
	bool lower_closed{};
	bool upper_closed{};
	if constexpr (
		::fast_io::details::wide_ryu_policy_is_nearest<rounding>)
	{
		auto const asymmetric{
			static_cast<__uint128_t>(
				mantissa != 0u || binary_exponent == 0u)};
		lower_numerator =
			target_numerator - 1u - asymmetric;
		upper_numerator = target_numerator + 2u;
		auto const even{(m2 & 1u) == 0u};
		lower_closed = ::fast_io::details::
			wide_ryu_nearest_left_closed<rounding>(
				negative, even);
		upper_closed = ::fast_io::details::
			wide_ryu_nearest_right_closed<rounding>(
				negative, even);
	}
	else
	{
		auto const right_closed{
			::fast_io::details::
				wide_ryu_directed_round_up<rounding>(negative)};
		if (right_closed)
		{
			/*
			At a normal power of two, the predecessor distance is half one
			current-binade ulp, hence two quarter units.  All other finite
			fields, including subnormals, use four quarter units.
			*/
			auto const predecessor_distance{
				(!mantissa && 1u < binary_exponent) ? 2u : 4u};
			lower_numerator =
				target_numerator - predecessor_distance;
			upper_numerator = target_numerator;
			lower_closed = false;
			upper_closed = true;
		}
		else
		{
			lower_numerator = target_numerator;
			upper_numerator = target_numerator + 4u;
			lower_closed = true;
			upper_closed = false;
		}
	}

	__uint128_t lower_floor{};
	__uint128_t target_floor{};
	__uint128_t upper_floor{};
	::std::int_least32_t e10{};
	::std::uint_least32_t divisibility_power{};
	bool scale_uses_power5{};
	if (0 <= e2)
	{
		auto const q{
			log10Pow2(e2) -
			static_cast<::std::uint_least32_t>(3 < e2)};
		e10 = static_cast<::std::int_least32_t>(q);
		auto const k{
			float_128_pow5_inv_bitcount +
			static_cast<::std::int_least32_t>(pow5bits(q)) - 1};
		auto const shift{
			-e2 + static_cast<::std::int_least32_t>(q) + k};
		::std::uint_least64_t power[4];
		generic_computeInvPow5(q, power);
		lower_floor =
			mulShift(lower_numerator, power, shift);
		target_floor =
			mulShift(target_numerator, power, shift);
		upper_floor =
			mulShift(upper_numerator, power, shift);
		divisibility_power = q;
		scale_uses_power5 = true;
	}
	else
	{
		auto const q{
			log10Pow5(-e2) -
			static_cast<::std::uint_least32_t>(-e2 > 1)};
		e10 = static_cast<::std::int_least32_t>(q) + e2;
		auto const i{
			-e2 - static_cast<::std::int_least32_t>(q)};
			auto const k{
				static_cast<::std::int_least32_t>(
					pow5bits(static_cast<::std::uint_least32_t>(i))) -
				float_128_pow5_bitcount};
		auto const shift{
			static_cast<::std::int_least32_t>(q) - k};
		::std::uint_least64_t power[4];
		generic_computePow5(
			static_cast<::std::uint_least32_t>(i), power);
		lower_floor =
			mulShift(lower_numerator, power, shift);
		target_floor =
			mulShift(target_numerator, power, shift);
		upper_floor =
			mulShift(upper_numerator, power, shift);
		divisibility_power = q;
	}

	auto const scaled_integer =
		[=](__uint128_t numerator) noexcept
	{
		if (scale_uses_power5)
		{
			/*
			n*2^e2/10^q = n*2^(e2-q)/5^q and e2>=q.
			The remaining denominator is exactly 5^q.  A nonzero u128
			cannot contain 5^q for q>floor(log_5(2^128))=55.
			*/
			return divisibility_power <= 55u &&
				multipleOfPowerOf5(
					numerator, divisibility_power);
		}
		/*
		n*5^(-e2-q)/2^q has the odd factor entirely in the numerator,
		so integrality is exactly divisibility by 2^q.  No nonzero u128
		is divisible by 2^128.
		*/
		return divisibility_power < 128u &&
			multipleOfPowerOf2(
				numerator, divisibility_power);
	};
	auto const lower_exact{scaled_integer(lower_numerator)};
	auto const target_exact{scaled_integer(target_numerator)};
	auto const upper_exact{scaled_integer(upper_numerator)};

	/*
	Normalize the real endpoints to the inclusive integer interval (5).
	The additions/subtractions are at most one.  Cached scaling leaves at
	least one admitted coefficient around the finite target, so the lower
	adjustment cannot overtake u128 max and an open upper endpoint cannot
	underflow zero.
	*/
	auto lower{
		lower_floor +
		static_cast<__uint128_t>(
			!lower_exact || !lower_closed)};
	auto upper{
		upper_floor -
		static_cast<__uint128_t>(
			upper_exact && !upper_closed)};

	::std::uint_least32_t removed{};
	::std::uint_least8_t guard{};
	bool sticky{!target_exact};
	for (;;)
	{
		/*
		Significant-digit minimality is not decimal-grid coarseness
		------------------------------------------------------------

		Let the current grid spacing be h and write x/h=t.  Once
		floor(t)<10, moving to spacing 10h cannot reduce the number of
		significant digits of a nonzero result below one.  More importantly,
		blindly accepting that coarser grid can change the canonical closest
		one-digit result.

		This is observable at the positive binary128 minimum subnormal under
		toward-minus-infinity.  Its parsing interval is

		    [x,2x),  x = 2^-16494 = 6.475...e-4966.

		On the h=1e-4966 grid, 7h is the closest admitted one-digit decimal.
		The 10h grid is also nonempty and contains 10h=1e-4965, but that
		decimal still has one significant digit after normalization and is
		farther from x.  The old loop therefore returned 1e-4965 at run time
		while the exact constant-evaluation path correctly returned 7e-4966.

		The following stop is complete, not special-case arithmetic.  If a
		nonzero point y on the 10h grid belongs to the monotone parsing
		interval containing x, the current-grid point z nearest x on the same
		side satisfies x <= z <= y or y <= z <= x, hence z belongs as well.
		When floor(t)<10, z has one decimal digit, except z=10h, which
		normalizes to exactly the same one-digit value as y.  Thus the finer
		grid always supplies a closer one-digit candidate or the identical
		candidate; no coarser grid can improve significant-digit count or the
		canonical distance/tie ordering.  Stopping here restores the proven
		equivalence with wide_shortest_from_window and the full exact backend.
		*/
		if (target_floor < 10u)
		{
			break;
		}
		auto const next_lower{
			lower / 10u +
			static_cast<__uint128_t>(lower % 10u != 0u)};
		auto const next_upper{upper / 10u};
		if (next_upper < next_lower)
		{
			break;
		}
		/*
		The old guard becomes lower-order discarded information before the
		next more-significant digit becomes the new guard.  Therefore sticky
		is true exactly when any digit after the new guard, including a
		nonintegral cache-scale tail, is nonzero.
		*/
		sticky |= guard != 0u;
		guard = static_cast<::std::uint_least8_t>(
			target_floor % 10u);
		target_floor /= 10u;
		lower = next_lower;
		upper = next_upper;
		++removed;
	}

	bool prefer_upper{};
	if constexpr (
		::fast_io::details::wide_ryu_policy_is_nearest<rounding>)
	{
		if (guard != 5u)
		{
			prefer_upper = 5u < guard;
		}
		else if (sticky)
		{
			prefer_upper = true;
		}
		else
		{
			prefer_upper =
				::fast_io::details::
					wide_ryu_tie_choose_upper<rounding>(
						target_floor, negative);
		}
	}
	else
	{
		prefer_upper =
			::fast_io::details::
				wide_ryu_directed_round_up<rounding>(negative);
	}

	auto const lower_candidate{target_floor};
	auto const upper_candidate{target_floor + 1u};
	auto const preferred{
		prefer_upper ? upper_candidate : lower_candidate};
	auto const alternate{
		prefer_upper ? lower_candidate : upper_candidate};
	auto const in_interval =
		[=](__uint128_t candidate) noexcept
	{
		return lower <= candidate && candidate <= upper;
	};
	__uint128_t output{};
	if (in_interval(preferred))
	{
		output = preferred;
	}
	else if (in_interval(alternate))
	{
		output = alternate;
	}
	else
	{
		/*
		This can occur only if a cache arithmetic invariant is violated: by
		the bracketing theorem, one target-adjacent grid point belongs to the
		first nonempty interval.  A checked result keeps this new policy core
		total while the caller retains the exact reference as a cold fallback.
		*/
		return {};
	}
	while (output && output % 10u == 0u)
	{
		output /= 10u;
		++e10;
	}
	return {
		output,
		static_cast<::std::int_least32_t>(
			e10 + static_cast<::std::int_least32_t>(removed)),
		true};
}

} // namespace fast_io::details

#endif

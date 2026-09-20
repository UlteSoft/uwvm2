#pragma once

#include "punning.h"
#include "roundtrip.h"
#include "wide_ryu.h"

#if defined(__SIZEOF_INT128__)

namespace fast_io::details
{

/*
`wide_shortest_result` is the presentation-independent carrier for exact
binary80/binary128 shortest conversion.  A binary80 shortest coefficient may
contain 21 decimal digits, so a uint64_t carrier is not sufficient.  Every
currently admitted IEC 60559 domain needs at most 37 digits and therefore fits
in native uint128 together with the one-unit upper candidate.
*/
struct wide_shortest_result
{
	__uint128_t m10{};
	::std::int_least32_t e10{};
	bool success{};
};

inline constexpr void wide_shortest_trim(
	__uint128_t &coefficient, ::std::int_least32_t &exponent) noexcept
{
	while (coefficient && coefficient % 10u == 0u)
	{
		coefficient /= 10u;
		++exponent;
	}
}

template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool wide_shortest_prefer_upper(
	__uint128_t prefix, unsigned char guard, bool after_guard,
	bool negative) noexcept
{
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		if (5u < guard || (guard == 5u && after_guard))
		{
			return true;
		}
		if (guard < 5u)
		{
			return false;
		}
		if constexpr (rounding ==
					  ::fast_io::manipulators::floating_rounding::nearest_to_even)
		{
			return (prefix & 1u) != 0u;
		}
		else if constexpr (rounding ==
						   ::fast_io::manipulators::floating_rounding::nearest_to_odd)
		{
			return (prefix & 1u) == 0u;
		}
		else if constexpr (rounding == ::fast_io::manipulators::
										   floating_rounding::nearest_toward_plus_infinity)
		{
			return !negative;
		}
		else if constexpr (rounding == ::fast_io::manipulators::
										   floating_rounding::nearest_toward_minus_infinity)
		{
			return negative;
		}
		else if constexpr (rounding == ::fast_io::manipulators::
										   floating_rounding::nearest_away_from_zero)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return ::fast_io::details::floating_rounding_directed_round_up<rounding>(
			negative);
	}
}

/*
Convert an arbitrary positive dyadic `significand * 2^binary_exponent` to the
same canonical exact decimal used by the precision backend.  Interval
endpoints need one more significand bit than the source format, so they cannot
be forged into a raw floating field and passed to exact_precision_from_binary.
The base-1e9 recurrence and its capacity proof are otherwise identical.
*/
template <typename flt>
[[nodiscard]] inline constexpr ::fast_io::details::exact_precision_decimal<flt>
wide_shortest_exact_from_significand(
	__uint128_t significand, ::std::int_least32_t binary_exponent) noexcept
{
	::fast_io::details::exact_precision_limb_type
		limbs[::fast_io::details::exact_precision_limb_capacity<flt>]{};
	::std::size_t limb_size{};
	::fast_io::details::exact_precision_initialize_wide_limbs(
		limbs, limb_size, significand);
	auto decimal_exponent{binary_exponent < 0 ? binary_exponent : 0};
	if (binary_exponent < 0)
	{
		auto count{static_cast<::std::uint_least32_t>(-binary_exponent)};
		for (; ::fast_io::details::exact_precision_pow5_chunk <= count;
			 count -= ::fast_io::details::exact_precision_pow5_chunk)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size,
				::fast_io::details::exact_precision_pow5_multiplier);
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
		for (; ::fast_io::details::exact_precision_pow2_chunk <= count;
			 count -= ::fast_io::details::exact_precision_pow2_chunk)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size,
				::fast_io::details::exact_precision_pow2_multiplier);
		}
		if (count)
		{
			::fast_io::details::exact_precision_multiply_small(
				limbs, limb_size,
				static_cast<::fast_io::details::exact_precision_multiplier_type>(1u)
					<< count);
		}
	}

	::fast_io::details::exact_precision_decimal<flt> decimal{};
	decimal.exponent = decimal_exponent;
	auto top{limbs[limb_size - 1u]};
	unsigned char reversed[::fast_io::details::exact_precision_limb_digits + 1u]{};
	::std::size_t reversed_size{};
	for (; top; top /= 10u)
	{
		reversed[reversed_size++] = static_cast<unsigned char>(top % 10u);
	}
	while (reversed_size)
	{
		decimal.digits[decimal.size++] = reversed[--reversed_size];
	}
	for (auto index{limb_size - 1u}; index; --index)
	{
		auto value{limbs[index - 1u]};
		auto position{
			decimal.size + ::fast_io::details::exact_precision_limb_digits};
		decimal.size = position;
		for (unsigned digit{};
			 digit != ::fast_io::details::exact_precision_limb_digits; ++digit)
		{
			decimal.digits[--position] =
				static_cast<unsigned char>(value % 10u);
			value /= 10u;
		}
	}
	while (decimal.size != 1u && !decimal.digits[decimal.size - 1u])
	{
		--decimal.size;
		++decimal.exponent;
	}
	return decimal;
}

inline constexpr ::std::size_t wide_shortest_compact_digits{40u};

struct wide_shortest_compact_decimal
{
	unsigned char digits[wide_shortest_compact_digits]{};
	::std::size_t stored_size{};
	::std::size_t full_size{};
	::std::int_least32_t exponent{};
	bool zero{};
};

/*
The caller invokes this helper in separate full expressions for each endpoint.
Only the 40-byte quotient escapes, so the three binary80/binary128 exact
objects have disjoint lifetimes and one exact buffer is the semantic peak.
Forty digits cover the 37-digit carrier, a possible carry digit, and two proof
digits.  If a longer canonical endpoint shares the stored prefix, its final
digit is nonzero and therefore the omitted tail is known to be positive.
*/
template <typename flt>
[[nodiscard]] inline constexpr ::fast_io::details::wide_shortest_compact_decimal
wide_shortest_compact_from_significand(
	__uint128_t significand, ::std::int_least32_t binary_exponent) noexcept
{
	if (!significand)
	{
		wide_shortest_compact_decimal result{};
		result.digits[0] = 0u;
		result.stored_size = result.full_size = 1u;
		result.zero = true;
		return result;
	}
	/*
	Only the leading 40 digits and exact knowledge of equality can affect a
	comparison with a <=38-digit candidate.  The 512-bit interval window proves
	the leading digits by requiring its lower and upper floors to agree.  A
	zero residual also proves exact termination and permits the compact result.
	Any conservative positive residual leaves equality undecided and therefore
	uses the complete expansion below.
	*/
	auto const window{
		::fast_io::details::exact_precision_wide_window_from_significand(
			significand,
			::fast_io::details::ibm_double_double_u128_bit_width(significand),
			binary_exponent, wide_shortest_compact_digits)};
	/*
	`tail_nonzero` is a conservative residual predicate, not an equality
	oracle.  It may be true for an exactly terminated decimal endpoint (for
	example an integer midpoint ending in zero).  Synthesizing a positive digit
	from that predicate would move a closed endpoint outward and reject the
	endpoint itself.  The compact comparison object therefore consumes a window
	only when the residual is proved zero.  Otherwise the complete expansion
	below determines the canonical length and whether a genuinely positive tail
	exists.  Runtime nearest-even wide conversion will use its fixed-width fast
	core; this branch remains the exact semantic reference for constant
	evaluation and uncommon policies.
	*/
	if (window.success && !window.tail_nonzero)
	{
		wide_shortest_compact_decimal result{};
		result.stored_size = window.decimal.size;
		result.full_size = result.stored_size;
		result.exponent = static_cast<::std::int_least32_t>(
			window.real_exponent + 1 -
			static_cast<::std::int_least32_t>(result.full_size));
		for (::std::size_t index{}; index != result.stored_size; ++index)
		{
			result.digits[index] = window.decimal.digits[index];
		}
		return result;
	}
	auto const exact{
		::fast_io::details::wide_shortest_exact_from_significand<flt>(
			significand, binary_exponent)};
	wide_shortest_compact_decimal result{};
	result.full_size = exact.size;
	result.stored_size = exact.size < wide_shortest_compact_digits
							 ? exact.size
							 : wide_shortest_compact_digits;
	result.exponent = exact.exponent;
	for (::std::size_t index{}; index != result.stored_size; ++index)
	{
		result.digits[index] = exact.digits[index];
	}
	return result;
}

[[nodiscard]] inline constexpr unsigned wide_shortest_u128_digits(
	__uint128_t value) noexcept
{
	unsigned size{1u};
	for (; 10u <= value; value /= 10u)
	{
		++size;
	}
	return size;
}

/* Return negative, zero, or positive according as candidate is below, equal
   to, or above endpoint. */
[[nodiscard]] inline constexpr int wide_shortest_compare_candidate(
	__uint128_t coefficient, ::std::int_least32_t exponent,
	::fast_io::details::wide_shortest_compact_decimal const &endpoint) noexcept
{
	if (endpoint.zero)
	{
		return coefficient ? 1 : 0;
	}
	unsigned char candidate_digits[wide_shortest_compact_digits]{};
	auto const candidate_size{
		static_cast<::std::size_t>(
			::fast_io::details::wide_shortest_u128_digits(coefficient))};
	auto cursor{candidate_size};
	for (auto value{coefficient}; cursor; value /= 10u)
	{
		candidate_digits[--cursor] = static_cast<unsigned char>(value % 10u);
	}
	auto const candidate_real_exponent{
		static_cast<::std::int_least64_t>(exponent) +
		static_cast<::std::int_least64_t>(candidate_size) - 1};
	auto const endpoint_real_exponent{
		static_cast<::std::int_least64_t>(endpoint.exponent) +
		static_cast<::std::int_least64_t>(endpoint.full_size) - 1};
	if (candidate_real_exponent != endpoint_real_exponent)
	{
		return candidate_real_exponent < endpoint_real_exponent ? -1 : 1;
	}
	auto const compare_size{candidate_size < endpoint.stored_size
								? endpoint.stored_size
								: candidate_size};
	for (::std::size_t index{}; index != compare_size; ++index)
	{
		auto const candidate_digit{
			index < candidate_size ? candidate_digits[index] : 0u};
		auto const endpoint_digit{
			index < endpoint.stored_size ? endpoint.digits[index] : 0u};
		if (candidate_digit != endpoint_digit)
		{
			return candidate_digit < endpoint_digit ? -1 : 1;
		}
	}
	// The candidate has at most 38 digits.  A canonical endpoint longer than
	// the retained 40 digits has a positive omitted tail, so equality of the
	// retained prefix still places the candidate strictly below it.
	return endpoint.stored_size < endpoint.full_size ? -1 : 0;
}

struct wide_shortest_interval
{
	wide_shortest_compact_decimal lower;
	wide_shortest_compact_decimal upper;
	bool lower_closed{};
	bool upper_closed{};
};

[[nodiscard]] inline constexpr ::fast_io::details::ibm_double_double_dyadic
wide_shortest_ibm_add_dyadics(
	::fast_io::details::ibm_double_double_dyadic left,
	::fast_io::details::ibm_double_double_dyadic right,
	::std::int_least32_t exponent_adjust = 0) noexcept
{
	/*
	Zero has no intrinsic dyadic exponent.  Treating its placeholder exponent
	as a scale would require an unbounded shift at the minimum subnormal.  The
	identity 0+x=x instead transfers the nonzero operand's exact scale and then
	applies exponent_adjust; for midpoint construction that adjustment is -1,
	so (0+x)/2 becomes x*2^-1 exactly.
	*/
	if (!left.significand)
	{
		right.exponent = static_cast<::std::int_least32_t>(
			right.exponent + exponent_adjust);
		right.negative = false;
		return right;
	}
	if (!right.significand)
	{
		left.exponent = static_cast<::std::int_least32_t>(
			left.exponent + exponent_adjust);
		left.negative = false;
		return left;
	}
	auto const common_exponent{
		left.exponent < right.exponent ? left.exponent : right.exponent};
	auto const left_shift{
		static_cast<unsigned>(left.exponent - common_exponent)};
	auto const right_shift{
		static_cast<unsigned>(right.exponent - common_exponent)};
	if (127u <= left_shift || 127u <= right_shift)
	{
		return {};
	}
	auto significand{(left.significand << left_shift) +
		(right.significand << right_shift)};
	auto exponent{static_cast<::std::int_least32_t>(
		common_exponent + exponent_adjust)};
	for (; significand && (significand & 1u) == 0u;
		 significand >>= 1u)
	{
		++exponent;
	}
	return {significand, exponent, false, significand != 0u};
}

/*
For a positive IBM value with top binary exponent E, the representable lattice
within its binade has quantum

  q(E) = 2^max(E-105,-1074).

This follows from p=106 until the low binary64 component reaches its own
minimum subnormal exponent.  The successor always adds q(E).  The predecessor
also subtracts q(E), except at an exact power of two: the preceding binade has
top exponent E-1 and therefore quantum q(E-1).  This is the same asymmetric
boundary responsible for shorter IEEE intervals, derived here from the IBM
lattice rather than from a synthetic IEEE field.

After aligning target and quantum at the smaller exponent, both integers have
at most 107 bits.  Addition/subtraction is consequently exact in uint128.  The
smallest target subtracts to zero; the largest target adds one final quantum
to the virtual finite successor used solely by the overflow interval.  Thus no
floating instruction, libm call, errno update, or exception flag participates
in neighbor construction.
*/
[[nodiscard]] inline constexpr ::fast_io::details::ibm_double_double_dyadic
wide_shortest_ibm_neighbor(
	::fast_io::details::ibm_double_double_dyadic target,
	bool successor) noexcept
{
	auto const width{
		::fast_io::details::ibm_double_double_u128_bit_width(
			target.significand)};
	auto const top_exponent{static_cast<::std::int_least32_t>(
		target.exponent + static_cast<::std::int_least32_t>(width - 1u))};
	auto quantum_exponent{static_cast<::std::int_least32_t>(
		top_exponent - 105)};
	if (!successor && target.significand == 1u)
	{
		--quantum_exponent;
	}
	if (quantum_exponent < -1074)
	{
		quantum_exponent = -1074;
	}
	auto const common_exponent{
		target.exponent < quantum_exponent ? target.exponent
											 : quantum_exponent};
	auto const target_shift{static_cast<unsigned>(
		target.exponent - common_exponent)};
	auto const quantum_shift{static_cast<unsigned>(
		quantum_exponent - common_exponent)};
	if (127u <= target_shift || 127u <= quantum_shift)
	{
		return {};
	}
	auto const target_integer{target.significand << target_shift};
	auto const quantum_integer{static_cast<__uint128_t>(1u) << quantum_shift};
	__uint128_t neighbor_integer{};
	if (successor)
	{
		if ((~static_cast<__uint128_t>(0u)) - target_integer <
			quantum_integer)
		{
			return {};
		}
		neighbor_integer = target_integer + quantum_integer;
	}
	else
	{
		if (target_integer < quantum_integer)
		{
			return {};
		}
		neighbor_integer = target_integer - quantum_integer;
	}
	auto neighbor_exponent{common_exponent};
	for (; neighbor_integer && (neighbor_integer & 1u) == 0u;
		 neighbor_integer >>= 1u)
	{
		++neighbor_exponent;
	}
	return {neighbor_integer, neighbor_exponent, false, true};
}

[[nodiscard]] inline constexpr bool wide_shortest_ibm_even(
	::fast_io::details::ibm_double_double_dyadic const &value) noexcept
{
	auto const width{
		::fast_io::details::ibm_double_double_u128_bit_width(
			value.significand)};
	auto const top_exponent{static_cast<::std::int_least32_t>(
		value.exponent + static_cast<::std::int_least32_t>(width - 1u))};
	/*
	IBM double-double has p=106 until the low binary64 component reaches its
	minimum exponent.  Its local lattice quantum is therefore

	  2^max(top_exponent-105,-1074).

	Expressing the target in units of that quantum gives the integer whose low
	bit is the nearest-even tie bit.  This also handles powers of two: 1 is
	2^105 local quanta and is even on both asymmetric sides.
	*/
	auto const quantum_exponent{
		top_exponent - 105 < -1074 ? -1074 : top_exponent - 105};
	if (value.exponent <= quantum_exponent)
	{
		return (value.significand & 1u) == 0u;
	}
	return true;
}

template <::fast_io::manipulators::floating_rounding rounding, typename flt>
	requires(::fast_io::details::fp_floating_point_is_ibm_double_double<flt>)
[[nodiscard]] inline constexpr ::fast_io::details::wide_shortest_interval
wide_shortest_make_ibm_interval(flt value, bool negative) noexcept
{
	auto target{
		::fast_io::details::get_ibm_double_double_dyadic(value)};
	target.negative = false;
	auto const previous{
		::fast_io::details::wide_shortest_ibm_neighbor(target, false)};
	auto const next{
		::fast_io::details::wide_shortest_ibm_neighbor(target, true)};
	wide_shortest_interval result{};
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		auto const lower_midpoint{
			::fast_io::details::wide_shortest_ibm_add_dyadics(
				previous, target, -1)};
		auto const upper_midpoint{
			::fast_io::details::wide_shortest_ibm_add_dyadics(
				target, next, -1)};
		result.lower = ::fast_io::details::
			wide_shortest_compact_from_significand<flt>(
				lower_midpoint.significand, lower_midpoint.exponent);
		result.upper = ::fast_io::details::
			wide_shortest_compact_from_significand<flt>(
				upper_midpoint.significand, upper_midpoint.exponent);
		auto const even{::fast_io::details::wide_shortest_ibm_even(target)};
		result.lower_closed = ::fast_io::details::
			dragonbox_nearest_normal_left_closed<rounding>(negative, even);
		result.upper_closed = ::fast_io::details::
			dragonbox_nearest_normal_right_closed<rounding>(negative, even);
	}
	else
	{
		auto const right_closed{
			::fast_io::details::floating_rounding_directed_round_up<rounding>(
				negative)};
		auto const &lower{right_closed ? previous : target};
		auto const &upper{right_closed ? target : next};
		result.lower = ::fast_io::details::
			wide_shortest_compact_from_significand<flt>(
				lower.significand, lower.exponent);
		result.upper = ::fast_io::details::
			wide_shortest_compact_from_significand<flt>(
				upper.significand, upper.exponent);
		result.lower_closed = !right_closed;
		result.upper_closed = right_closed;
	}
	return result;
}

template <typename flt,
		  ::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr ::fast_io::details::wide_shortest_interval
wide_shortest_make_interval(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t raw_exponent, bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u)};
	__uint128_t significand{static_cast<__uint128_t>(mantissa)};
	::std::int_least32_t binary_exponent{};
	if (raw_exponent)
	{
		significand |= static_cast<__uint128_t>(1u) << trait::mbits;
		binary_exponent = static_cast<::std::int_least32_t>(raw_exponent) -
						  bias - static_cast<::std::int_least32_t>(trait::mbits);
	}
	else
	{
		binary_exponent = 1 - bias -
						  static_cast<::std::int_least32_t>(trait::mbits);
	}
	wide_shortest_interval result{};
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		auto const shorter{!mantissa && 1u < raw_exponent};
		if (shorter)
		{
			result.lower = ::fast_io::details::
				wide_shortest_compact_from_significand<flt>(
					(significand << 2u) - 1u, binary_exponent - 2);
			result.lower_closed = ::fast_io::details::
				dragonbox_nearest_shorter_left_closed<rounding>(negative);
			result.upper_closed = ::fast_io::details::
				dragonbox_nearest_shorter_right_closed<rounding>(negative);
		}
		else
		{
			result.lower = ::fast_io::details::
				wide_shortest_compact_from_significand<flt>(
					(significand << 1u) - 1u, binary_exponent - 1);
			auto const even{(significand & 1u) == 0u};
			result.lower_closed = ::fast_io::details::
				dragonbox_nearest_normal_left_closed<rounding>(negative, even);
			result.upper_closed = ::fast_io::details::
				dragonbox_nearest_normal_right_closed<rounding>(negative, even);
		}
		result.upper = ::fast_io::details::
			wide_shortest_compact_from_significand<flt>(
				(significand << 1u) + 1u, binary_exponent - 1);
	}
	else
	{
		auto const right_closed{
			::fast_io::details::floating_rounding_directed_round_up<rounding>(
				negative)};
		if (right_closed)
		{
			auto const shorter{!mantissa && 1u < raw_exponent};
			result.lower = ::fast_io::details::
				wide_shortest_compact_from_significand<flt>(
					shorter ? (significand << 1u) - 1u : significand - 1u,
					shorter ? binary_exponent - 1 : binary_exponent);
			result.upper = ::fast_io::details::
				wide_shortest_compact_from_significand<flt>(
					significand, binary_exponent);
			result.lower_closed = false;
			result.upper_closed = true;
		}
		else
		{
			result.lower = ::fast_io::details::
				wide_shortest_compact_from_significand<flt>(
					significand, binary_exponent);
			result.upper = ::fast_io::details::
				wide_shortest_compact_from_significand<flt>(
					significand + 1u, binary_exponent);
			result.lower_closed = true;
			result.upper_closed = false;
		}
	}
	return result;
}

[[nodiscard]] inline constexpr bool wide_shortest_interval_contains(
	::fast_io::details::wide_shortest_interval const &interval,
	__uint128_t coefficient, ::std::int_least32_t exponent) noexcept
{
	auto const lower_order{::fast_io::details::wide_shortest_compare_candidate(
		coefficient, exponent, interval.lower)};
	if (lower_order < 0 || (!lower_order && !interval.lower_closed))
	{
		return false;
	}
	auto const upper_order{::fast_io::details::wide_shortest_compare_candidate(
		coefficient, exponent, interval.upper)};
	return upper_order < 0 || (!upper_order && interval.upper_closed);
}

/*
The window contains max_digits+1 proved decimal digits of the exact target.
At precision P, digits [0,P) form L and digit P is the guard.  The remaining
window digits plus tail_nonzero are exactly the sticky predicate; unlike the
canonical full expansion, zero padding is therefore harmless.  The candidate
exponent is real_exponent+1-P, derived directly from scientific notation and
independent of the unmaterialized exact coefficient length.  Trying the two
bracketing grid points in increasing P is the same completeness argument used
by the full-expansion fallback below.
*/
template <::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr ::fast_io::details::wide_shortest_result
wide_shortest_from_window(
	::fast_io::details::exact_precision_window_result const &window,
	::fast_io::details::wide_shortest_interval const &interval,
	::std::size_t maximum_digits, bool negative) noexcept
{
	if (!window.success || window.decimal.size <= maximum_digits)
	{
		return {};
	}
	__uint128_t prefix{};
	for (::std::size_t precision{1u}; precision <= maximum_digits; ++precision)
	{
		prefix = prefix * 10u + window.decimal.digits[precision - 1u];
		auto const candidate_exponent{static_cast<::std::int_least32_t>(
			window.real_exponent + 1 -
			static_cast<::std::int_least32_t>(precision))};
		auto const guard{window.decimal.digits[precision]};
		bool retained_after_guard{};
		for (::std::size_t index{precision + 1u};
			 !retained_after_guard && index != window.decimal.size; ++index)
		{
			retained_after_guard =
				window.decimal.digits[index] != 0u;
		}
		auto const after_guard{
			retained_after_guard || window.tail_nonzero};
		/*
		The fixed-width window proves every stored digit, but `tail_nonzero`
		is deliberately conservative: a nonzero residual interval proves that
		the omitted tail *may* be positive, not that the exact dyadic tail is.
		That distinction is immaterial unless the guard is exactly five and
		every retained digit after it is zero.  In precisely that case the
		nearest policy must distinguish an exact midpoint from a value above it.

		Example: (2^63+1)/4 has canonical decimal tail ".25".  A 22-digit
		window stores an additional proved zero but can retain a conservative
		residual; treating that residual as sticky chooses ...3 instead of the
		nearest-even ...2.  No interval-width argument can recover the missing
		equality bit.  Returning failure here delegates to the complete exact
		expansion, which computes sticky as the existence of a later canonical
		digit and therefore distinguishes equality.  For guard<5 or guard>5,
		any possible tail lies strictly on the already selected side of one half.
		For directed policies sticky never participates in prefer_upper.  Thus
		this is the only ambiguous branch and the fallback is both necessary and
		sufficient.
		*/
		if constexpr (
			::fast_io::details::floating_rounding_is_nearest<
				rounding>)
		{
			if (guard == 5u && !retained_after_guard &&
				window.tail_nonzero)
			{
				return {};
			}
		}
		auto const prefer_upper{
			::fast_io::details::wide_shortest_prefer_upper<rounding>(
				prefix, guard, after_guard, negative)};
		auto const try_candidate = [&](bool upper) constexpr noexcept
		{
			auto coefficient{
				prefix + static_cast<__uint128_t>(upper)};
			auto exponent{candidate_exponent};
			::fast_io::details::wide_shortest_trim(coefficient, exponent);
			return ::fast_io::details::wide_shortest_result{
				coefficient, exponent,
				::fast_io::details::wide_shortest_interval_contains(
					interval, coefficient, exponent)};
		};
		auto const preferred{try_candidate(prefer_upper)};
		if (preferred.success)
		{
			return preferred;
		}
		auto const alternate{try_candidate(!prefer_upper)};
		if (alternate.success)
		{
			return alternate;
		}
	}
	return {};
}

/*
Let the canonical exact expansion be D*10^e with N digits.  For a requested
P, L=floor(D/10^(N-P))*10^(e+N-P) and U=L+10^(e+N-P) bracket the exact binary
value.  Every IEC 60559 rounding preimage is one monotone interval containing
that exact value.  Therefore, if any P-digit decimal round-trips, L or U does;
a farther grid point cannot enter the interval without the nearer bracketing
point entering first.  Trying only those two candidates is complete.

P is visited in increasing order, proving minimal significant-digit count.
When both candidates round-trip, `wide_shortest_prefer_upper` applies the exact
decimal-distance/tie policy used by the narrow Dragonbox implementation.  The
IEC 60559 max_digits10 separation bound guarantees a successful bracketing
candidate by `m10digits` (21 for binary80, 37 for binary128), including every
directed and nearest endpoint convention.  A 37-digit lower carrier is at most
10^37-1, so its one-unit upper candidate is at most 10^37.  Since
10^37 < 2^128, both candidates are strictly representable in uint128.
*/
template <typename flt,
		  ::fast_io::manipulators::floating_rounding rounding>
#if __has_cpp_attribute(__gnu__::__pure__)
[[__gnu__::__pure__]]
#endif
[[nodiscard]] inline constexpr ::fast_io::details::wide_shortest_result
wide_shortest_from_binary(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t binary_exponent, bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	static_assert(sizeof(mantissa_type) <= sizeof(__uint128_t));
	static_assert(trait::m10digits <= 38u);
	static_assert(rounding !=
					  ::fast_io::manipulators::floating_rounding::current_environment,
				  "wide shortest materialization requires one explicit rounding policy");
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u)};
	if (binary_exponent == exponent_mask)
	{
		return {};
	}
	if (!mantissa && !binary_exponent)
	{
		return {0u, 0, true};
	}
	if constexpr (
		rounding ==
			::fast_io::manipulators::floating_rounding::
				nearest_to_even &&
		(trait::mbits == 63u || trait::mbits == 112u) &&
		trait::ebits == 15u)
	{
		/*
		Runtime/constant-path equivalence theorem
		-----------------------------------------

		wide_ryu_nearest_even and the exact backend below construct the same
		nearest-even parsing interval from `(mantissa,binary_exponent)`.  Each
		visits decimal grids in increasing precision, and at the first nonempty
		grid each chooses the closest bracketing point with an even decimal tie.
		The chosen carrier is therefore unique.  Replacing one implementation by
		the other according to evaluation mode cannot change `(m10,e10)`.

		The builtin constant-evaluation predicate folds to false in an ordinary
		run-time instantiation; it is not a data-dependent branch and introduces
		no dynamic dispatch overhead.  Constant evaluation deliberately retains
		the table-independent exact proof below, while run time avoids its
		5--14 microsecond big-integer endpoint materialization.  The enclosing
		compiler-constant proxy already uses `__builtin_constant_p` to enter the
		constant arm before this field function, so compile-known ftoa calls do
		not instantiate a dynamic dispatcher.
		*/
#if FAST_IO_HAS_BUILTIN(__builtin_is_constant_evaluated)
		if (!__builtin_is_constant_evaluated())
#else
		if (!::std::is_constant_evaluated())
#endif
		{
			auto const fast{
				::fast_io::details::
					wide_ryu_nearest_even<flt>(
						mantissa, binary_exponent)};
			/*
			The field guards above exclude the only two non-success domains
			(special encodings and zero).  wide_ryu_nearest_even is total on
			every remaining finite magnitude: both exponent signs construct a
			cache index inside [0,4928], and the digit-removal loop terminates
			because integer division strictly decreases a positive endpoint.
			Returning unconditionally records that proof for the optimizer.  A
			checked fallthrough would retain the multi-kilobyte exact backend in
			the emitted runtime function and add a never-taken branch.
			*/
			return {fast.m10, fast.e10, fast.success};
		}
	}
	else if constexpr (
		(trait::mbits == 63u || trait::mbits == 112u) &&
		trait::ebits == 15u)
	{
		/*
		The other nine explicit policies use the policy-complete fixed-width
		interval core.  Equations (3)--(6) in wide_ryu.h prove that its inclusive
		integer interval is exactly the interval materialized below:
		nearest policies use the same asymmetric midpoints and independently
		chosen endpoint closures; directed policies use the same target/neighbor
		pair and one closed target endpoint.  Both algorithms minimize normalized
		significant digits and then apply the same distance/tie policy to the two
		target-adjacent points.  The fixed core's `target_floor < 10` stop is the
		power-of-ten boundary needed for this equivalence: beyond it a coarser
		grid cannot reduce a nonzero result below one digit and may only replace
		the exact backend's closer one-digit point by a farther one.  Its proof
		in wide_ryu.h shows that every earlier coarsening either reduces digit
		count or preserves the identical normalized decimal.  Their successful
		carrier is consequently identical.

		As for nearest-even, constant evaluation retains the table-independent
		exact construction.  At run time the field guards above exclude special
		encodings and zero.  For every remaining finite field, equations (5) and
		(6) preserve a nonempty interval until the first failed or canonical
		one-digit coarsening, and the target-bracketing theorem proves that one
		of floor(x) and floor(x)+1 is in that interval.  Thus the helper is total
		on this domain.  Returning
		unconditionally records the proof for the optimizer, removes a
		never-taken branch, and prevents the exact big-integer backend from being
		retained in the emitted run-time function.
		*/
#if FAST_IO_HAS_BUILTIN(__builtin_is_constant_evaluated)
		if (!__builtin_is_constant_evaluated())
#else
		if (!::std::is_constant_evaluated())
#endif
		{
			auto const fast{
				::fast_io::details::
					wide_ryu_policy<flt, rounding>(
						mantissa, binary_exponent, negative)};
			return {fast.m10, fast.e10, fast.success};
		}
	}
	auto const interval{
		::fast_io::details::wide_shortest_make_interval<flt, rounding>(
			mantissa, binary_exponent, negative)};
	__uint128_t exact_significand{static_cast<__uint128_t>(mantissa)};
	::std::int_least32_t exact_binary_exponent{};
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u)};
	if (binary_exponent)
	{
		exact_significand |= static_cast<__uint128_t>(1u) << trait::mbits;
		exact_binary_exponent = static_cast<::std::int_least32_t>(
			binary_exponent) - bias - static_cast<::std::int_least32_t>(trait::mbits);
	}
	else
	{
		exact_binary_exponent =
			1 - bias - static_cast<::std::int_least32_t>(trait::mbits);
	}
	constexpr auto maximum_digits{
		static_cast<::std::size_t>(trait::m10digits)};
	auto const window{
		::fast_io::details::exact_precision_wide_window_from_significand(
			exact_significand,
			::fast_io::details::ibm_double_double_u128_bit_width(
				exact_significand),
			exact_binary_exponent, maximum_digits + 1u)};
	if (window.success)
	{
		auto const result{
			::fast_io::details::wide_shortest_from_window<rounding>(
				window, interval, maximum_digits, negative)};
		if (result.success)
		{
			return result;
		}
	}
	auto const decimal{::fast_io::details::exact_precision_from_binary<flt>(
		mantissa, binary_exponent)};
	__uint128_t prefix{};
	auto const limit{
		decimal.size < maximum_digits ? decimal.size : maximum_digits};
	for (::std::size_t precision{1u}; precision <= limit; ++precision)
	{
		prefix = prefix * 10u + decimal.digits[precision - 1u];
		auto const candidate_exponent{static_cast<::std::int_least32_t>(
			decimal.exponent +
			static_cast<::std::int_least32_t>(decimal.size - precision))};
		if (precision == decimal.size)
		{
			return {prefix, candidate_exponent,
					::fast_io::details::wide_shortest_interval_contains(
						interval, prefix, candidate_exponent)};
		}
		auto const guard{decimal.digits[precision]};
		// exact_precision_from_binary removes every trailing decimal zero.  Thus
		// a digit after the guard exists iff the discarded tail after it is nonzero.
		auto const after_guard{precision + 1u < decimal.size};
		auto const prefer_upper{
			::fast_io::details::wide_shortest_prefer_upper<rounding>(
				prefix, guard, after_guard, negative)};
		auto try_candidate = [&](bool upper) constexpr noexcept {
			auto coefficient{
				prefix + static_cast<__uint128_t>(upper)};
			auto exponent{candidate_exponent};
			::fast_io::details::wide_shortest_trim(coefficient, exponent);
			return ::fast_io::details::wide_shortest_result{
				coefficient, exponent,
				::fast_io::details::wide_shortest_interval_contains(
					interval, coefficient, exponent)};
		};
		auto const preferred{try_candidate(prefer_upper)};
		if (preferred.success)
		{
			return preferred;
		}
		auto const alternate{try_candidate(!prefer_upper)};
		if (alternate.success)
		{
			return alternate;
		}
	}
	return {};
}

/*
IBM double-double shortest conversion uses the same decimal-grid completeness
theorem as the binary80/binary128 path, but its interval endpoints come from
the ABI's real adjacent values.  This distinction is essential at powers of
two, in the reduced-precision underflow region and at overflow.  Once the
interval and the target's exact dyadic expansion are known, trying the lower
and upper decimal grid neighbors in increasing precision is representation
independent; the proof preceding wide_shortest_from_binary applies verbatim.
Thirty-five digits cover `max_digits10` for the 106-bit domain, and 10^35 is
strictly smaller than 2^117, so every candidate and its one-unit successor fit
the uint128 carrier.
*/
template <::fast_io::manipulators::floating_rounding rounding, typename flt>
	requires(::fast_io::details::fp_floating_point_is_ibm_double_double<flt>)
[[nodiscard]] inline constexpr ::fast_io::details::wide_shortest_result
wide_shortest_from_ibm_double_double(flt value) noexcept
{
	static_assert(rounding !=
		::fast_io::manipulators::floating_rounding::current_environment);
	auto const target{
		::fast_io::details::get_ibm_double_double_dyadic(value)};
	if (!target.success)
	{
		return {};
	}
	if (!target.significand)
	{
		return {0u, 0, true};
	}
	auto const interval{
		::fast_io::details::wide_shortest_make_ibm_interval<rounding>(
			value, target.negative)};
	constexpr ::std::size_t maximum_digits{35u};
	auto const window{
		::fast_io::details::exact_precision_wide_window_from_significand(
			target.significand,
			::fast_io::details::ibm_double_double_u128_bit_width(
				target.significand),
			target.exponent, maximum_digits + 1u)};
	if (window.success)
	{
		auto const result{
			::fast_io::details::wide_shortest_from_window<rounding>(
				window, interval, maximum_digits, target.negative)};
		if (result.success)
		{
			return result;
		}
	}
	auto const decimal{
		::fast_io::details::wide_shortest_exact_from_significand<flt>(
			target.significand, target.exponent)};
	__uint128_t prefix{};
	auto const limit{
		decimal.size < maximum_digits ? decimal.size : maximum_digits};
	for (::std::size_t precision{1u}; precision <= limit; ++precision)
	{
		prefix = prefix * 10u + decimal.digits[precision - 1u];
		auto const candidate_exponent{static_cast<::std::int_least32_t>(
			decimal.exponent +
			static_cast<::std::int_least32_t>(decimal.size - precision))};
		if (precision == decimal.size)
		{
			return {prefix, candidate_exponent,
				::fast_io::details::wide_shortest_interval_contains(
					interval, prefix, candidate_exponent)};
		}
		auto const guard{decimal.digits[precision]};
		auto const after_guard{precision + 1u < decimal.size};
		auto const prefer_upper{
			::fast_io::details::wide_shortest_prefer_upper<rounding>(
				prefix, guard, after_guard, target.negative)};
		auto const try_candidate = [&](bool upper) constexpr noexcept
		{
			auto coefficient{
				prefix + static_cast<__uint128_t>(upper)};
			auto exponent{candidate_exponent};
			::fast_io::details::wide_shortest_trim(coefficient, exponent);
			return ::fast_io::details::wide_shortest_result{
				coefficient, exponent,
				::fast_io::details::wide_shortest_interval_contains(
					interval, coefficient, exponent)};
		};
		auto const preferred{try_candidate(prefer_upper)};
		if (preferred.success)
		{
			return preferred;
		}
		auto const alternate{try_candidate(!prefer_upper)};
		if (alternate.success)
		{
			return alternate;
		}
	}
	return {};
}

} // namespace fast_io::details

#endif

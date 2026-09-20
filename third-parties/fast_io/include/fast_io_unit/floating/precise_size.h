#pragma once

/*
The precise-reserve protocol needs the number of output code units before it
owns an output range.  Consequently a floating implementation cannot discover
that number by formatting into scratch storage: runtime precision has no finite
type-level bound, and a scratch copy would duplicate the dominant digit-writing
work.

This file is intentionally a presentation layer over the existing floating
arithmetic.  Shortest conversion reuses the DA/Dragonbox decimal carrier;
precision conversion reuses the exact binary-to-decimal expansion and rounding
helpers; hexadecimal conversion reuses the punned IEC 60559 fields.  The code
below then evaluates only the same mutually exclusive layout predicates as the
writers in dragonbox/impl.h and hexfloat.h.  Keeping this layer separate makes
the proof obligation explicit: arithmetic chooses a unique rounded decimal,
and these functions count the punctuation, padding and exponent characters of
that decimal without materializing any character.

Dependencies are deliberately limited to the floating unit.  The including
header must make punning.h, hexfloat.h and dragonbox/impl.h available before
this file.  No output-core concept, compiler policy or ISA policy is modeled
here; character encodings, including EBCDIC, change code-unit values but not
the number of code units emitted by these numeric grammars.
*/

namespace fast_io::details
{

[[nodiscard]] inline constexpr ::std::size_t
floating_precise_add(::std::size_t left, ::std::size_t right) noexcept
{
	return ::fast_io::details::intrinsics::add_or_overflow_die(left, right);
}

[[nodiscard]] inline constexpr ::std::size_t
floating_precise_decimal_digits(::std::uint_least32_t value) noexcept
{
	return static_cast<::std::size_t>(::fast_io::details::chars_len<10u, true>(value));
}

[[nodiscard]] inline constexpr ::std::uint_least32_t
floating_precise_exponent_magnitude(::std::int_least32_t exponent) noexcept
{
	auto magnitude{static_cast<::std::uint_least32_t>(exponent)};
	if (exponent < 0)
	{
		magnitude = 0u - magnitude;
	}
	return magnitude;
}

template <typename flt>
[[nodiscard]] inline constexpr ::std::size_t
floating_precise_decimal_exponent_size(::std::int_least32_t exponent) noexcept
{
	auto digits{floating_precise_decimal_digits(
		floating_precise_exponent_magnitude(exponent))};
	// Decimal scientific output pads a one-digit exponent to two digits.  The
	// trait's e10digits bounds the magnitude but does not alter that minimum.
	if (digits < 2u)
	{
		digits = 2u;
	}
	return digits + 2u; // exponent marker and mandatory exponent sign
}

[[nodiscard]] inline constexpr ::std::size_t
floating_precise_hex_exponent_size(::std::int_least32_t exponent) noexcept
{
	// Hexadecimal output deliberately has no decimal-style leading-zero rule.
	return floating_precise_decimal_digits(
			   floating_precise_exponent_magnitude(exponent)) +
		   2u;
}

template <bool showpos>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_sign_size(bool negative) noexcept
{
	return static_cast<::std::size_t>(showpos || negative);
}

template <bool showpos, bool nan_show_sign, bool nan_show_type,
		  ::std::size_t mbits, typename mantissa_type>
[[nodiscard]] inline constexpr ::std::size_t
floating_precise_special_size(mantissa_type mantissa, bool negative) noexcept
{
	if (!mantissa)
	{
		// Infinity always follows the ordinary sign policy and has a three-code-unit literal.
		return floating_precise_sign_size<showpos>(negative) + 3u;
	}
	::std::size_t size{3u}; // nan
	if constexpr (nan_show_sign)
	{
		size += floating_precise_sign_size<showpos>(negative);
	}
	if constexpr (nan_show_type)
	{
		constexpr mantissa_type quiet_bit{
			::fast_io::details::fp_quiet_nan_mantissa_mask<mantissa_type, mbits>()};
		if (negative && mantissa == quiet_bit)
		{
			size += 5u; // (ind)
		}
		else if (::fast_io::details::fp_nan_is_signaling<mantissa_type, mbits>(mantissa))
		{
			size += 6u; // (snan)
		}
	}
	return size;
}

template <typename flt, bool json_float>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_fixed_size_with_length(
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> /*mantissa*/,
	::std::int_least32_t exponent, ::std::int_least32_t length) noexcept
{
	auto const real_exponent{static_cast<::std::int_least32_t>(exponent + length - 1)};
	if (length <= real_exponent)
	{
		return static_cast<::std::size_t>(real_exponent + 1) + (json_float ? 2u : 0u);
	}
	if (0 <= real_exponent)
	{
		if (length == real_exponent + 1)
		{
			return static_cast<::std::size_t>(length) + (json_float ? 2u : 0u);
		}
		return static_cast<::std::size_t>(length + 1);
	}
	return static_cast<::std::size_t>(1 - real_exponent + length);
}

template <typename flt>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_scientific_size_with_length(
	::std::int_least32_t exponent, ::std::int_least32_t length) noexcept
{
	auto const real_exponent{static_cast<::std::int_least32_t>(exponent + length - 1)};
	auto const coefficient_size{static_cast<::std::size_t>(length) +
								static_cast<::std::size_t>(length != 1)};
	return coefficient_size + floating_precise_decimal_exponent_size<flt>(real_exponent);
}

template <typename flt, ::fast_io::manipulators::floating_format format, bool json_float>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_decimal_layout_size_with_length(
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> mantissa,
	::std::int_least32_t exponent, ::std::int_least32_t length) noexcept
{
	if constexpr (format == ::fast_io::manipulators::floating_format::fixed)
	{
		return floating_precise_fixed_size_with_length<flt, json_float>(mantissa, exponent, length);
	}
	else if constexpr (format == ::fast_io::manipulators::floating_format::general)
	{
		/*
		Sizing must repeat emission's notation predicate exactly; otherwise an
		exact-boundary to_chars call could approve the fixed length and then emit
		a longer scientific spelling (or conversely return a false overflow).
		For decimal=(mantissa,exponent) with L digits, the scientific exponent is
		X=exponent+L-1, hence the general rule is -4<=X<6.
		*/
		auto const scientific_exponent{
			static_cast<::std::int_least32_t>(exponent + length - 1)};
		if (-4 <= scientific_exponent && scientific_exponent < 6)
		{
			return floating_precise_fixed_size_with_length<flt, json_float>(mantissa, exponent, length);
		}
		return floating_precise_scientific_size_with_length<flt>(exponent, length);
	}
	else if constexpr (format == ::fast_io::manipulators::floating_format::scientific)
	{
		return floating_precise_scientific_size_with_length<flt>(exponent, length);
	}
	else
	{
		// `decimal` compares the complete fixed and scientific spellings before
		// rendering.  Reusing the writer's exact exponent-aware model is necessary
		// at powers where the two lengths tie.
		auto const real_exponent{static_cast<::std::int_least32_t>(exponent + length - 1)};
		::std::uint_least32_t fixed_length{};
		if (length <= real_exponent)
		{
			fixed_length = static_cast<::std::uint_least32_t>(real_exponent + 1);
		}
		else if (0 <= real_exponent && real_exponent < length)
		{
			fixed_length = static_cast<::std::uint_least32_t>(length + 2);
			if (length == real_exponent + 1)
			{
				--fixed_length;
			}
		}
		else
		{
			fixed_length = static_cast<::std::uint_least32_t>(-real_exponent) +
						   static_cast<::std::uint_least32_t>(length) + 1u;
		}
		auto const scientific_estimate{
			::fast_io::details::print_rsv_fp_scientific_length(
				real_exponent, static_cast<::std::size_t>(length))};
		if (scientific_estimate < fixed_length)
		{
			return floating_precise_scientific_size_with_length<flt>(exponent, length);
		}
		return floating_precise_fixed_size_with_length<flt, json_float>(mantissa, exponent, length);
	}
}

template <typename flt, ::fast_io::manipulators::floating_format format, bool json_float>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_decimal_layout_size(
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> mantissa,
	::std::int_least32_t exponent) noexcept
{
	auto const length{static_cast<::std::int_least32_t>(
		::fast_io::details::chars_len<10u, true>(mantissa))};
	return floating_precise_decimal_layout_size_with_length<flt, format, json_float>(
		mantissa, exponent, length);
}

template <typename flt, ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding, bool json_float>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_shortest_finite_nonzero_size(
	flt value, typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return floating_precise_shortest_finite_nonzero_size<flt, format,
																 ::fast_io::manipulators::floating_rounding::toward_plus_infinity, json_float>(
				value, mantissa, exponent, negative);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return floating_precise_shortest_finite_nonzero_size<flt, format,
																 ::fast_io::manipulators::floating_rounding::toward_minus_infinity, json_float>(
				value, mantissa, exponent, negative);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return floating_precise_shortest_finite_nonzero_size<flt, format,
																 ::fast_io::manipulators::floating_rounding::toward_zero, json_float>(
				value, mantissa, exponent, negative);
		default:
			return floating_precise_shortest_finite_nonzero_size<flt, format,
																 ::fast_io::manipulators::floating_rounding::nearest_to_even, json_float>(
				value, mantissa, exponent, negative);
		}
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even &&
					   ((trait::mbits == 23u && trait::ebits == 8u) ||
						(trait::mbits == 52u && trait::ebits == 11u)))
	{
		auto const converted{::fast_io::details::da::to_conversion_result<flt>(
			mantissa, static_cast<::std::int_least32_t>(exponent))};
		auto const decimal{::fast_io::details::da::trim_trailing_zeros(
			::fast_io::details::da::finalize<flt>(converted))};
		return floating_precise_decimal_layout_size<flt, format, json_float>(
			decimal.m10, decimal.e10);
	}
	else if constexpr (::fast_io::details::dragonbox_uses_binary32_core<flt> &&
					   sizeof(flt) < sizeof(float) &&
					   rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		auto const decimal{::fast_io::details::dragonbox_narrow_shortest_lookup<flt>(
			mantissa, static_cast<::std::int_least32_t>(exponent))};
		return floating_precise_decimal_layout_size_with_length<flt, format, json_float>(
			decimal.m10, decimal.e10, static_cast<::std::int_least32_t>(decimal.length));
	}
	#if defined(__SIZEOF_INT128__)
	else if constexpr (
		::fast_io::details::fp_floating_point_is_ibm_double_double<flt>)
	{
		auto const decimal{
			::fast_io::details::wide_shortest_from_ibm_double_double<rounding>(
				static_cast<long double>(value))};
		if (!decimal.success)
		{
			::fast_io::fast_terminate();
		}
		return floating_precise_decimal_layout_size<flt, format, json_float>(
			decimal.m10, decimal.e10);
	}
	else if constexpr (
		::fast_io::details::print_floating_decimal_exact_supported<flt>)
	{
		auto const decimal{
			::fast_io::details::wide_shortest_from_binary<flt, rounding>(
				mantissa, exponent, negative)};
		if (!decimal.success)
		{
			::fast_io::fast_terminate();
		}
		return floating_precise_decimal_layout_size<flt, format, json_float>(
			decimal.m10, decimal.e10);
	}
	#endif
	else
	{
		auto const decimal{[&]() constexpr noexcept {
			if constexpr (::fast_io::details::dragonbox_uses_binary32_core<flt> &&
						  sizeof(flt) < sizeof(float))
			{
				return ::fast_io::details::dragonbox_impl_narrow_hybrid<flt, rounding>(
					mantissa, static_cast<::std::int_least32_t>(exponent), negative);
			}
			else
			{
				return ::fast_io::details::dragonbox_impl<flt, rounding>(
					mantissa, static_cast<::std::int_least32_t>(exponent), negative);
			}
		}()};
		return floating_precise_decimal_layout_size<flt, format, json_float>(
			decimal.m10, decimal.e10);
	}
}

template <bool showpos, bool nan_show_sign, bool nan_show_type,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_rounding rounding, bool json_float, typename flt>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_shortest_size(flt value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr mantissa_type exponent_mask{
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u};
	auto const [mantissa, exponent, negative]{::fast_io::details::get_punned_result(value)};
	if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
	{
		return floating_precise_special_size<showpos, nan_show_sign, nan_show_type,
											 trait::mbits>(mantissa, negative);
	}
	auto const sign_size{floating_precise_sign_size<showpos>(negative)};
	if (!mantissa && !exponent)
	{
		if constexpr (format == ::fast_io::manipulators::floating_format::scientific)
		{
			/*
			Scientific charconv emits one coefficient digit, `e`, an explicit
			exponent sign, and at least two exponent digits.  For zero these are
			exactly `0e+00`, hence five code units plus the optional value sign.
			This is the same grammar used by prsv_fp_dece0; equality of the size
			and writer formulas is the exact-boundary store proof.
			*/
			return sign_size + 5u;
		}
		return sign_size + 1u + (json_float ? 2u : 0u);
	}
	return sign_size +
		   floating_precise_shortest_finite_nonzero_size<flt, format, rounding, json_float>(
			   value, mantissa, exponent, negative);
}

/*
Some ABIs must transport a narrow floating value as integer IEC 60559 fields:
forming a native by-value argument can itself narrow a subnormal before either
the size model or renderer observes it.  This precise-only entry performs the
same mutually exclusive special/sign/zero classification as
`floating_precise_shortest_size`, but starts from the already preserved fields.
The finite conversion routines below consume those fields directly; their
`value` parameter exists only to keep the current-environment recursion typed,
and precise support excludes that mutable policy for decimal output.
*/
template <bool showpos, bool nan_show_sign, bool nan_show_type,
	::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_rounding rounding, bool json_float,
	typename flt>
[[nodiscard]] inline constexpr ::std::size_t
floating_precise_shortest_fields_size(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr mantissa_type exponent_mask{
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u};
	if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
	{
		return floating_precise_special_size<showpos, nan_show_sign,
			nan_show_type, trait::mbits>(mantissa, negative);
	}
	auto const sign_size{floating_precise_sign_size<showpos>(negative)};
	if (!mantissa && !exponent)
	{
		if constexpr (format ==
			::fast_io::manipulators::floating_format::scientific)
		{
			/*
			The integer-field proxy must report the same `0e+00` length as the
			native-value path.  Both classify zero by exponent==mantissa==0, so
			five code units plus sign is representation-independent.
			*/
			return sign_size + 5u;
		}
		return sign_size + 1u + (json_float ? 2u : 0u);
	}
	return sign_size +
		floating_precise_shortest_finite_nonzero_size<
			flt, format, rounding, json_float>(
				flt{}, mantissa, exponent, negative);
}

/*
`floating_precise_decimal_metadata` is the presentation-independent quotient
of decimal generation.  Every length grammar observes only whether the
coefficient is zero, its digit count, and its power-of-ten exponent; it never
observes an interior coefficient digit.  Keeping exactly those three facts is
what allows general, decimal, fixed and scientific callers to share one
rounding/materialization body without formatting into scratch storage.
*/
struct floating_precise_decimal_metadata
{
	unsigned char digits[1u];
	::std::size_t size;
	::std::int_least32_t exponent;
};

struct floating_precise_decimal_metadata_result
{
	floating_precise_decimal_metadata decimal;
	bool success;
};

struct floating_precise_rounded_metadata
{
	floating_precise_decimal_metadata raw;
	floating_precise_decimal_metadata trimmed;
};

struct floating_precise_rounded_metadata_result
{
	floating_precise_rounded_metadata metadata;
	bool success;
};

template <bool preserve, typename decimal_type>
[[nodiscard]] inline constexpr floating_precise_decimal_metadata
floating_precise_make_decimal_metadata(decimal_type const &decimal) noexcept
{
	auto size{decimal.size};
	auto exponent{decimal.exponent};
	if constexpr (!preserve)
	{
		while (size != 1u && decimal.digits[size - 1u] == 0u)
		{
			--size;
			++exponent;
		}
		// Zero has no intrinsic decimal quantum.  Canonicalizing its exponent is
		// required because general formatting uses the exponent to select fixed
		// versus scientific notation, exactly as `exact_precision_trim` does.
		if (size == 1u && decimal.digits[0] == 0u)
		{
			exponent = 0;
		}
	}
	return {{static_cast<unsigned char>(decimal.digits[0] != 0u)}, size, exponent};
}

template <typename decimal_type>
[[nodiscard]] inline constexpr floating_precise_rounded_metadata
floating_precise_make_rounded_metadata(decimal_type const &decimal) noexcept
{
	return {floating_precise_make_decimal_metadata<true>(decimal),
			floating_precise_make_decimal_metadata<false>(decimal)};
}

template <typename decimal_type>
[[nodiscard]] inline constexpr floating_precise_rounded_metadata
floating_precise_make_fractional_rounded_metadata(
	decimal_type const &decimal, ::std::size_t precision,
	bool rounded) noexcept
{
	auto result{floating_precise_make_rounded_metadata(decimal)};
	if (rounded)
	{
		// The emitter restores zeroes omitted when a carry canonicalizes a
		// coefficient rounded on the 10^-P grid.  Metadata has no digit array, so
		// represent that same virtual coefficient by its width and final exponent.
		// A performed fractional rounding implies P fits int_least32_t; requests
		// beyond that bound are finer than every finite expansion and never enter
		// this branch.
		result.raw.size = ::fast_io::details::
			exact_precision_fractional_general_rounded_virtual_size(
				decimal, precision);
		result.raw.exponent = -static_cast<::std::int_least32_t>(precision);
	}
	return result;
}

/*
The wide emitter proves that at most P+1 decimal digits plus one sticky bit
determine every window-accepted P<=128 result.  Precise reservation needs only
the selected coefficient's nonzero state, width and exponent, so it must not
reconstruct the complete binary80/binary128 expansion merely to discard all
interior digits.  This adapter deliberately calls the same runtime-policy
window as emission for both normal and subnormal values.  The callee performs
the unique rounding and trailing-zero canonicalization;
`floating_precise_make_decimal_metadata<true>` then projects that
already-selected decimal without applying either operation a second time.

Keeping format, precision mode and rounding as data preserves one arithmetic
body per floating representation instead of cloning it across the 4 x 4 x 10
presentation matrix.  The capability guard is arithmetic-only: targets without
a native scalar uint128 continue through the complete exact fallback below.
*/
#if defined(__SIZEOF_INT128__)
struct floating_precise_wide_window_metadata_result
{
	floating_precise_decimal_metadata decimal;
	::std::size_t significant;
	bool success;
};

// `noipa` prevents policy clones where available; `noinline` is the
// placement-only fallback and leaves the numeric contract unchanged.
template <typename flt>
#if __has_cpp_attribute(gnu::noipa)
[[gnu::noipa]]
#elif __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
[[nodiscard]] inline constexpr floating_precise_wide_window_metadata_result
floating_precise_prepare_wide_window_metadata(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t precision, bool negative,
	::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode,
	::fast_io::manipulators::floating_rounding rounding) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	static_assert(::fast_io::details::exact_precision_is_wide_binary<flt>);
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
	::fast_io::details::exact_precision_compact_window_decimal decimal{};
	::std::size_t significant{};
	if (!::fast_io::details::exact_precision_wide_prepare(
			decimal.digits, decimal.size, decimal.exponent, significant,
			significand, trait::mbits, binary_exponent, precision,
			format, precision_mode, rounding, negative))
	{
		return {};
	}
	return {floating_precise_make_decimal_metadata<true>(decimal), significant, true};
}
#endif

template <bool json_float, typename decimal_type>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_exact_fixed_size(
	decimal_type const &decimal, ::std::size_t virtual_size,
	bool force_fractional, ::std::size_t fractional_precision) noexcept
{
	auto const real_exponent{
		decimal.exponent + static_cast<::std::int_least32_t>(decimal.size) - 1};
	auto const point{real_exponent + 1};
	if (force_fractional && fractional_precision && 0 < point)
	{
		auto const integer_digits{static_cast<::std::size_t>(point)};
		if (integer_digits <= decimal.size && virtual_size == decimal.size)
		{
			auto size{floating_precise_add(decimal.size, 1u)};
			auto const present{decimal.size - integer_digits};
			if (present < fractional_precision)
			{
				size = floating_precise_add(size, fractional_precision - present);
			}
			return size;
		}
	}

	::std::size_t size{};
	bool wrote_point{};
	if (point <= 0)
	{
		size = 1u;
		if (decimal.digits[0] != 0u || force_fractional)
		{
			size = floating_precise_add(2u, static_cast<::std::size_t>(-point));
			size = floating_precise_add(size, virtual_size);
			wrote_point = true;
		}
	}
	else
	{
		auto const integer_digits{static_cast<::std::size_t>(point)};
		if (integer_digits < virtual_size)
		{
			size = floating_precise_add(virtual_size, 1u);
			wrote_point = true;
		}
		else
		{
			size = integer_digits;
		}
	}
	if (force_fractional)
	{
		auto const present{point <= 0
							   ? floating_precise_add(static_cast<::std::size_t>(-point), virtual_size)
							   : (point < static_cast<::std::int_least32_t>(virtual_size)
									  ? virtual_size - static_cast<::std::size_t>(point)
									  : 0u)};
		if (!wrote_point && fractional_precision)
		{
			size = floating_precise_add(size, 1u);
			wrote_point = true;
		}
		if (present < fractional_precision)
		{
			size = floating_precise_add(size, fractional_precision - present);
		}
	}
	if constexpr (json_float)
	{
		if (!wrote_point)
		{
			size = floating_precise_add(size, 2u);
		}
	}
	return size;
}

template <typename flt, typename decimal_type>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_exact_scientific_size(
	decimal_type const &decimal, ::std::size_t fractional_precision, bool preserve) noexcept
{
	auto const real_exponent{
		decimal.exponent + static_cast<::std::int_least32_t>(decimal.size) - 1};
	auto const available{decimal.size - 1u};
	auto const used{available < fractional_precision ? available : fractional_precision};
	::std::size_t coefficient_size{1u};
	if (used || (preserve && fractional_precision))
	{
		coefficient_size = floating_precise_add(coefficient_size, 1u);
		coefficient_size = floating_precise_add(
			coefficient_size, preserve ? fractional_precision : used);
	}
	return floating_precise_add(
		coefficient_size, floating_precise_decimal_exponent_size<flt>(real_exponent));
}

template <typename flt, ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_precision precision_mode, bool json_float,
		  typename decimal_type>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_rounded_precision_size(
	decimal_type const &decimal, ::std::size_t precision, ::std::size_t significant) noexcept
{
	constexpr bool fractional{
		::fast_io::details::floating_precision_is_fractional<precision_mode>};
	constexpr bool preserve{
		::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>};
	if constexpr (format == ::fast_io::manipulators::floating_format::scientific)
	{
		auto const fractional_digits{fractional ? precision : significant - 1u};
		return floating_precise_exact_scientific_size<flt>(
			decimal, fractional_digits, preserve);
	}
	auto virtual_size{decimal.size};
	if constexpr (preserve && !fractional)
	{
		if (virtual_size < significant)
		{
			virtual_size = significant;
		}
	}
	if constexpr (format == ::fast_io::manipulators::floating_format::fixed ||
				  (fractional && format == ::fast_io::manipulators::floating_format::decimal))
	{
		return floating_precise_exact_fixed_size<json_float>(
			decimal, virtual_size, fractional && preserve, precision);
	}

	auto const virtual_padding{virtual_size - decimal.size};
	bool fixed{};
	constexpr auto int32_max{(::std::numeric_limits<::std::int_least32_t>::max)()};
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
			/*
			Ordinary general presentation is selected from the rounded value's
			scientific exponent X, exactly as in print_rsv_fp_decision_impl.
			The coefficient exponent is not invariant under trailing-zero
			canonicalization and therefore cannot decide the notation.
			*/
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
		auto const rounded_exponent{
			decimal.exponent + static_cast<::std::int_least32_t>(decimal.size) - 1};
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
		return floating_precise_exact_fixed_size<json_float>(
			decimal, virtual_size, fractional && preserve, precision);
	}
	return floating_precise_exact_scientific_size<flt>(
		decimal, virtual_size - 1u, preserve);
}

// Every helper in this block consumes the native-u128 wide window.  The gate is
// an arithmetic representation boundary; no ISA performance policy is implied.
#if defined(__SIZEOF_INT128__)
struct floating_precise_wide_window_size_result
{
	::std::size_t size;
	bool success;
};

[[nodiscard]] inline constexpr bool floating_precise_runtime_rounding_is_nearest(
	::fast_io::manipulators::floating_rounding rounding) noexcept
{
	using enum ::fast_io::manipulators::floating_rounding;
	return rounding == nearest_to_even || rounding == nearest_to_odd ||
		rounding == nearest_toward_plus_infinity ||
		rounding == nearest_toward_minus_infinity ||
		rounding == nearest_toward_zero || rounding == nearest_away_from_zero;
}

[[nodiscard]] inline constexpr bool floating_precise_runtime_directed_round_up(
	::fast_io::manipulators::floating_rounding rounding, bool negative) noexcept
{
	using enum ::fast_io::manipulators::floating_rounding;
	switch (rounding)
	{
	case toward_plus_infinity:
		return !negative;
	case toward_minus_infinity:
		return negative;
	case away_from_zero:
		return true;
	default:
		return false;
	}
}

/*
This is the size-only counterpart of `exact_precision_wide_runtime_present`.
The same runtime policy representation that shares numeric window generation
must also share layout selection; otherwise each of 320 compile-time policies
would clone the fixed/scientific decision around a common arithmetic call.
Only the three observable metadata fields participate, and every inequality is
identical to `floating_precise_rounded_precision_size` above.
*/
template <typename flt, bool json_float>
[[nodiscard]] inline constexpr ::std::size_t
floating_precise_wide_runtime_rounded_size(
	floating_precise_decimal_metadata const &decimal, ::std::size_t precision,
	::std::size_t significant,
	::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode) noexcept
{
	using format_enum = ::fast_io::manipulators::floating_format;
	using precision_enum = ::fast_io::manipulators::floating_precision;
	auto const fractional{precision_mode == precision_enum::fractional ||
		precision_mode == precision_enum::fractional_preserve_trailing_zero};
	auto const preserve{
		precision_mode == precision_enum::significant_preserve_trailing_zero ||
		precision_mode == precision_enum::fractional_preserve_trailing_zero};
	if (format == format_enum::scientific)
	{
		auto const fractional_digits{fractional ? precision : significant - 1u};
		return floating_precise_exact_scientific_size<flt>(
			decimal, fractional_digits, preserve);
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
		return floating_precise_exact_fixed_size<json_float>(
			decimal, virtual_size, fractional && preserve, precision);
	}
	auto const virtual_padding{virtual_size - decimal.size};
	bool fixed{};
	constexpr auto int32_max{(::std::numeric_limits<::std::int_least32_t>::max)()};
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
		return floating_precise_exact_fixed_size<json_float>(
			decimal, virtual_size, fractional && preserve, precision);
	}
	return floating_precise_exact_scientific_size<flt>(
		decimal, virtual_size - 1u, preserve);
}

/*
Tiny fractional values and bounded normal/subnormal windows are combined behind
one outlined size operation.  Consequently a caller policy contributes only a
call with enum data.  A false result has one meaning: the bounded proof did not
cover the value/grid, so the caller must execute the complete exact fallback.
*/
template <typename flt, bool json_float>
#if __has_cpp_attribute(gnu::noipa)
[[gnu::noipa]]
#elif __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
[[nodiscard]] inline constexpr floating_precise_wide_window_size_result
floating_precise_wide_window_size(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t precision, bool negative,
	::fast_io::manipulators::floating_format format,
	::fast_io::manipulators::floating_precision precision_mode,
	::fast_io::manipulators::floating_rounding rounding) noexcept
{
	using format_enum = ::fast_io::manipulators::floating_format;
	using precision_enum = ::fast_io::manipulators::floating_precision;
	static_assert(::fast_io::details::exact_precision_is_wide_binary<flt>);
	auto const fractional{precision_mode == precision_enum::fractional ||
		precision_mode == precision_enum::fractional_preserve_trailing_zero};
	auto const preserve{
		precision_mode == precision_enum::significant_preserve_trailing_zero ||
		precision_mode == precision_enum::fractional_preserve_trailing_zero};
	if (fractional && format != format_enum::scientific &&
		::fast_io::details::exact_precision_fractional_tiny_wide_binary_bound<flt>(
			exponent, precision))
	{
		auto const digit{static_cast<unsigned char>(
			!floating_precise_runtime_rounding_is_nearest(rounding) &&
			floating_precise_runtime_directed_round_up(rounding, negative))};
		floating_precise_decimal_metadata const decimal{
			{digit}, 1u,
			digit || preserve ? -static_cast<::std::int_least32_t>(precision) : 0};
		return {floating_precise_wide_runtime_rounded_size<flt, json_float>(
			decimal, precision, 0u, format, precision_mode), true};
	}
	if (128u < precision)
	{
		return {};
	}
	auto const window{floating_precise_prepare_wide_window_metadata<flt>(
		mantissa, exponent, precision, negative, format, precision_mode,
		rounding)};
	if (!window.success)
	{
		return {};
	}
	return {floating_precise_wide_runtime_rounded_size<flt, json_float>(
		window.decimal, precision, window.significant, format, precision_mode), true};
}
#endif

template <typename flt, bool json_float>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_carrier_fixed_precision_size(
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> mantissa,
	::std::int_least32_t exponent, ::std::size_t precision) noexcept
{
	if (!mantissa)
	{
		if constexpr (json_float)
		{
			if (!precision)
			{
				return 3u;
			}
		}
		return precision ? floating_precise_add(2u, precision) : 1u;
	}
	auto const length{static_cast<::std::int_least32_t>(
		::fast_io::details::chars_len<10u, true>(mantissa))};
	auto const real_exponent{static_cast<::std::int_least32_t>(exponent + length - 1)};
	if (0 <= real_exponent)
	{
		auto const integer_digits{static_cast<::std::size_t>(real_exponent) + 1u};
		::std::size_t size{};
		if (length <= static_cast<::std::int_least32_t>(integer_digits))
		{
			size = integer_digits;
			if constexpr (json_float)
			{
				if (!precision)
				{
					return floating_precise_add(size, 2u);
				}
			}
			return precision
					   ? floating_precise_add(size, floating_precise_add(1u, precision))
					   : size;
		}
		size = static_cast<::std::size_t>(length) + 1u;
		auto const present{static_cast<::std::size_t>(length) - integer_digits};
		if (present < precision)
		{
			size = floating_precise_add(size, precision - present);
		}
		return size;
	}
	// The writer emits exactly the requested fractional field in this branch:
	// leading zeroes, available carrier digits and padding partition P but never
	// change its total width.
	return precision ? floating_precise_add(2u, precision) : 1u;
}

template <typename flt, bool preserve>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_carrier_scientific_precision_size(
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> mantissa,
	::std::int_least32_t exponent, ::std::size_t precision) noexcept
{
	auto const length{static_cast<::std::size_t>(
		::fast_io::details::chars_len<10u, true>(mantissa))};
	auto const real_exponent{exponent + static_cast<::std::int_least32_t>(length) - 1};
	::std::size_t coefficient_size{1u};
	if (precision)
	{
		auto const available{length - 1u};
		if constexpr (preserve)
		{
			coefficient_size = floating_precise_add(2u, precision);
		}
		else
		{
			auto used{available < precision ? available : precision};
			if (used)
			{
				auto prefix{static_cast<::std::uint_least64_t>(mantissa)};
				auto const discarded{available - used};
				if (discarded)
				{
					prefix /= ::fast_io::details::print_rsv_fp_pow10_0_to_19_table[discarded];
				}
				for (; used && prefix % 10u == 0u; --used)
				{
					prefix /= 10u;
				}
				if (used)
				{
					coefficient_size = used + 2u;
				}
			}
		}
	}
	return floating_precise_add(
		coefficient_size, floating_precise_decimal_exponent_size<flt>(real_exponent));
}

template <typename flt, ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  ::fast_io::manipulators::floating_rounding rounding, bool json_float>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_carrier_precision_size(
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> mantissa,
	::std::int_least32_t exponent, ::std::size_t precision, bool negative) noexcept
{
	constexpr bool significant{
		::fast_io::details::floating_precision_is_significant<precision_mode>};
	constexpr bool fractional{
		::fast_io::details::floating_precision_is_fractional<precision_mode>};
	constexpr bool preserve{
		::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>};
	if constexpr (format == ::fast_io::manipulators::floating_format::scientific)
	{
		auto significant_precision{::fast_io::details::exact_precision_saturating_add(precision, 1u)};
		if constexpr (significant)
		{
			significant_precision = precision ? precision : 1u;
			precision = significant_precision - 1u;
		}
		::fast_io::details::print_rsv_fp_round_to_significant<flt, rounding, preserve>(
			mantissa, exponent, significant_precision, negative);
		if constexpr (!preserve)
		{
			::fast_io::details::print_rsv_fp_trim_trailing_zero(mantissa, exponent);
		}
		return floating_precise_carrier_scientific_precision_size<flt, preserve>(
			mantissa, exponent, precision);
	}
	else if constexpr (fractional)
	{
		::fast_io::details::print_rsv_fp_round_to_fractional<flt, rounding>(
			mantissa, exponent, precision, negative);
		if constexpr (!preserve)
		{
			::fast_io::details::print_rsv_fp_trim_trailing_zero(mantissa, exponent);
		}
		if constexpr (format == ::fast_io::manipulators::floating_format::general)
		{
			if constexpr (preserve)
			{
				if (-5 < exponent && exponent < 7)
				{
					return floating_precise_carrier_fixed_precision_size<flt, json_float>(
						mantissa, exponent, precision);
				}
				return floating_precise_decimal_layout_size<flt,
															::fast_io::manipulators::floating_format::scientific, false>(mantissa, exponent);
			}
			return floating_precise_decimal_layout_size<flt, format, json_float>(mantissa, exponent);
		}
		else if constexpr (preserve)
		{
			return floating_precise_carrier_fixed_precision_size<flt, json_float>(
				mantissa, exponent, precision);
		}
		else
		{
			return floating_precise_fixed_size_with_length<flt, json_float>(mantissa, exponent,
																			static_cast<::std::int_least32_t>(
																				::fast_io::details::chars_len<10u, true>(mantissa)));
		}
	}
	else
	{
		::fast_io::details::print_rsv_fp_round_to_significant<flt, rounding, preserve>(
			mantissa, exponent, precision, negative);
		if constexpr (!preserve)
		{
			::fast_io::details::print_rsv_fp_trim_trailing_zero(mantissa, exponent);
		}
		if constexpr (format == ::fast_io::manipulators::floating_format::fixed)
		{
			return floating_precise_fixed_size_with_length<flt, json_float>(mantissa, exponent,
																			static_cast<::std::int_least32_t>(
																				::fast_io::details::chars_len<10u, true>(mantissa)));
		}
		else if constexpr (
			format == ::fast_io::manipulators::floating_format::general &&
			precision_mode ==
				::fast_io::manipulators::floating_precision::
					charconv_significant)
		{
			auto const length{static_cast<::std::int_least32_t>(
				::fast_io::details::chars_len<10u, true>(mantissa))};
			auto const scientific_exponent{
				static_cast<::std::int_least32_t>(
					exponent + length - 1)};
			auto const significant_precision{precision ? precision : 1u};
			if (-4 <= scientific_exponent &&
				(scientific_exponent < 0 ||
				 static_cast<::std::size_t>(scientific_exponent) <
					 significant_precision))
			{
				return floating_precise_fixed_size_with_length<
					flt, json_float>(mantissa, exponent, length);
			}
			return floating_precise_scientific_size_with_length<flt>(
				exponent, length);
		}
		return floating_precise_decimal_layout_size<flt, format, json_float>(mantissa, exponent);
	}
}

// P16--P19 metadata needs only the already rounded DA coefficient, not its
// character spelling.  Native-u128 targets can instantiate all four proved
// widths.  MSVC x64 has the same audited two-word products but no language-level
// u128 type; it reuses the single outlined P16--P17 selector shared with the
// emitter.  No other no-u128 target is inferred to have that arithmetic ABI.
#if defined(__SIZEOF_INT128__) || \
	(defined(_MSC_VER) && defined(_M_X64) && !defined(__clang__) && \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC)))
[[nodiscard]] inline constexpr floating_precise_decimal_metadata_result
floating_precise_try_binary64_narrow_da_metadata(
	::std::uint_least64_t mantissa, ::std::uint_least32_t exponent,
	::std::size_t significant) noexcept
{
	if (!exponent || significant < 16u || 19u < significant)
	{
		return {};
	}
	::fast_io::details::da::binary64_scientific_precision_result converted{};
	// Native-u128 targets instantiate the complete P16--P19 switch.  The
	// alternative branch below is exactly the closed MSVC-x64 P16--P17 policy
	// documented by the enclosing capability gate.
#if defined(__SIZEOF_INT128__)
	constexpr ::std::uint_least64_t implicit_bit{
		static_cast<::std::uint_least64_t>(1ULL) << 52u};
	switch (significant)
	{
	case 16u:
		converted = ::fast_io::details::da::compute_binary64_scientific_precision<16u>(
			mantissa | implicit_bit, exponent);
		break;
	case 17u:
		converted = ::fast_io::details::da::compute_binary64_scientific_precision<17u>(
			mantissa | implicit_bit, exponent);
		break;
	case 18u:
		converted = ::fast_io::details::da::compute_binary64_scientific_precision<18u>(
			mantissa | implicit_bit, exponent);
		break;
	default:
		converted = ::fast_io::details::da::compute_binary64_scientific_precision<19u>(
			mantissa | implicit_bit, exponent);
		break;
	}
#else
	// MSVC P18--P19 failed the independent byte-stream differential proof.  The
	// closed P16--P17 selector therefore rejects them before any metadata can be
	// consumed, leaving the complete limb expansion authoritative.
	if (17u < significant)
	{
		return {};
	}
	converted = ::fast_io::details::binary64_scientific_precision_msvc_runtime(
		mantissa, exponent, significant);
#endif
	if (!converted.success)
	{
		return {};
	}

	// A successful DA result is the unique nearest rounded P-digit
	// coefficient: its ambiguity interval rejects every exact or possible tie,
	// so all six nearest policies agree.  Layout needs only the coefficient's
	// post-trim length and its leading exponent.  Dividing trailing zeroes from
	// the coefficient and increasing its quantum exponent preserves the value;
	// preserving modes reconstruct the requested virtual width from P.
	auto coefficient{converted.significand};
	auto digits{significant};
	for (; coefficient % 10u == 0u; coefficient /= 10u)
	{
		--digits;
	}
	floating_precise_decimal_metadata decimal{
		{1u}, digits, converted.exponent + 1 - static_cast<::std::int_least32_t>(digits)};
	return {decimal, true};
}
#endif

// Every helper below this point consumes native-u128 exact-window state.  This
// gate is a representation requirement; no compiler or ISA performance policy
// is encoded in the precise-size presentation layer.
#if defined(__SIZEOF_INT128__)
[[nodiscard]] inline constexpr floating_precise_decimal_metadata_result
floating_precise_try_binary64_wide_da_metadata(
	::std::uint_least64_t mantissa, ::std::uint_least32_t exponent,
	::std::size_t significant) noexcept
{
	if (!exponent || 18u < significant - 20u)
	{
		return {};
	}
	__uint128_t coefficient;
	::std::int_least32_t real_exponent;
	if (significant - 20u < 14u)
	{
		auto const converted{::fast_io::details::
			compute_binary64_common_significant_precision(
				mantissa, exponent, significant)};
		if (!converted.success)
		{
			return {};
		}
		coefficient = converted.significand;
		real_exponent = converted.exponent;
	}
	else if (significant == 34u)
	{
		auto const carrier{::fast_io::details::
			compute_binary64_p34_precision_carrier(mantissa, exponent)};
		if (!carrier)
		{
			return {};
		}
		coefficient = carrier & binary64_p34_precision_coefficient_mask;
		real_exponent =
			static_cast<::std::int_least32_t>(carrier >> 113u) - 512;
	}
	else
	{
		coefficient = ::fast_io::details::
			compute_binary64_p35_p38_precision_carrier(
				mantissa, exponent, significant, real_exponent);
		if (!coefficient)
		{
			return {};
		}
	}
	// P20-P38 uses the same presentation-independent u128 coefficient as
	// emission.  P34 retains its packed exponent only at the arithmetic ABI;
	// P35-P38 return the exponent through the shared scalar output because their
	// coefficients occupy as many as 127 bits.  Every ambiguity rejection above
	// falls through to exact sizing before presentation observes metadata.
	// Materialization removes trailing zeroes while preserving the real exponent;
	// preserve modes reconstruct their virtual width from `significant`, so every
	// layout consumes the same metadata without writing a character.
	auto const decimal{::fast_io::details::materialize_binary64_common_significant_precision(
		coefficient, real_exponent, significant)};
	return {floating_precise_make_decimal_metadata<true>(decimal), true};
}

template <typename flt>
[[nodiscard]] inline constexpr floating_precise_decimal_metadata_result
floating_precise_try_exact_identity_metadata(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::int_least32_t keep,
	::std::int_least32_t &real_exponent, bool exponent_may_cross_decade) noexcept
{
	static_assert(::std::same_as<flt, float> || ::std::same_as<flt, double>);
	using trait = ::fast_io::details::iec559_traits<flt>;
	constexpr ::std::int_least32_t bias{
		(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
	auto binary_mantissa{static_cast<::std::uint_least64_t>(mantissa)};
	::std::int_least32_t binary_exponent{};
	if (exponent)
	{
		binary_mantissa |= static_cast<::std::uint_least64_t>(1ULL) << trait::mbits;
		binary_exponent = static_cast<::std::int_least32_t>(exponent) - bias -
						  static_cast<::std::int_least32_t>(trait::mbits);
	}
	else
	{
		binary_exponent = 1 - bias - static_cast<::std::int_least32_t>(trait::mbits);
	}

	if (exponent_may_cross_decade)
	{
		// A shortest coefficient which is not a power of ten lies strictly inside
		// a decimal decade, so its real exponent is exact.  Only a decimal-power
		// coefficient can have a round-trip interval crossing that boundary; one
		// exact leading digit resolves precisely that exceptional case.
		auto const prefix{
			::fast_io::details::exact_precision_compact_window_from_binary<flt>(
				mantissa, exponent, 1u, real_exponent)};
		if (!prefix.success)
		{
			return {};
		}
		real_exponent = prefix.real_exponent;
	}

	if (binary_exponent < 0)
	{
		auto const exponent_magnitude{static_cast<::std::uint_least32_t>(-binary_exponent)};
		auto const factors_two{static_cast<::std::uint_least32_t>(
			::std::countr_zero(binary_mantissa))};
		if (factors_two < exponent_magnitude)
		{
			/*
			After cancelling t=v2(M) binary factors, x=(M/2^t)*5^k*10^-k
			with k=-e-t>0.  The remaining M/2^t is odd, hence its product with
			5^k is not divisible by ten.  The exact canonical decimal therefore
			has exponent -k and L=floor(log10(x))+k+1 coefficient digits.  If
			keep>=L, every rounding policy is the identity; presentation can use
			this metadata without observing any coefficient digit.
			*/
			auto const k{static_cast<::std::int_least32_t>(exponent_magnitude - factors_two)};
			auto const exact_size64{static_cast<::std::int_least64_t>(real_exponent) +
									static_cast<::std::int_least64_t>(k) + 1};
			if (exact_size64 <= 0 || keep < exact_size64)
			{
				return {};
			}
			return {{{1u}, static_cast<::std::size_t>(exact_size64), -k}, true};
		}
		// The binary value is already integral.  Divide away the negative
		// exponent before applying the integer factorization below; the residual
		// significand can still contain both factors two and five.
		binary_mantissa >>= exponent_magnitude;
		binary_exponent = 0;
	}
	if (real_exponent < 0)
	{
		return {};
	}
	auto const integer_digits{static_cast<::std::size_t>(real_exponent) + 1u};
	auto factor_five_source{binary_mantissa};
	::std::size_t factors_five{};
	for (; factor_five_source % 5u == 0u; factor_five_source /= 5u)
	{
		++factors_five;
	}
	auto const factors_two{static_cast<::std::size_t>(::std::countr_zero(binary_mantissa)) +
						   static_cast<::std::size_t>(binary_exponent)};
	auto const trailing_zeroes{
		factors_two < factors_five ? factors_two : factors_five};
	auto const exact_size{integer_digits - trailing_zeroes};
	if (keep < static_cast<::std::int_least64_t>(exact_size))
	{
		return {};
	}
	return {{{1u}, exact_size, static_cast<::std::int_least32_t>(trailing_zeroes)}, true};
}

// The optional noinline spelling enforces the measured COMDAT sharing boundary
// only; compilers without either attribute evaluate identical arithmetic and
// retain the complete fallback on every failure.
template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
[[nodiscard]] inline constexpr floating_precise_rounded_metadata_result
floating_precise_prepare_exact_window_metadata(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t precision,
	::std::int_least32_t fixed_keep, bool fractional_grid, bool negative,
	::std::int_least32_t initial_real_exponent,
	bool initial_exponent_may_cross_decade) noexcept
{
	static_assert(::std::same_as<flt, float> || ::std::same_as<flt, double>);
	auto real_exponent{initial_real_exponent};
	for (unsigned attempt{}; attempt != 2u; ++attempt)
	{
		::std::int_least32_t keep{fixed_keep};
		constexpr auto int32_max{
			(::std::numeric_limits<::std::int_least32_t>::max)()};
		bool precision_exceeds_int32{};
		if (fractional_grid)
		{
			precision_exceeds_int32 =
				static_cast<::std::size_t>(int32_max) < precision;
			if (precision_exceeds_int32)
			{
				// Every finite binary32/binary64 exact coefficient is far shorter
				// than the maximum int_least32_t digit count.  Saturating only this
				// comparison operand therefore preserves the identity proof; the
				// original size_t precision remains available to layout overflow checks.
				keep = int32_max;
			}
			else
			{
				auto const requested_keep{static_cast<::std::int_least64_t>(real_exponent) +
										  static_cast<::std::int_least64_t>(precision) + 1};
				if (requested_keep < 0 && initial_exponent_may_cross_decade)
				{
					/*
					The initial shortest exponent may differ by one at a decimal decade.
					A one-digit exact window resolves that boundary before the below-half
					shortcut decides between zero and one directed quantum.
					*/
					auto const prefix{
						::fast_io::details::exact_precision_compact_window_from_binary<flt>(
							mantissa, exponent, 1u, real_exponent)};
					if (!prefix.success)
					{
						return {};
					}
					real_exponent = prefix.real_exponent;
					initial_exponent_may_cross_decade = false;
					auto const corrected_keep{static_cast<::std::int_least64_t>(real_exponent) +
											  static_cast<::std::int_least64_t>(precision) + 1};
					if (corrected_keep < 0)
					{
						// requested_keep < 0 means |x| < 10^(-P-1), strictly below
						// every nearest halfway boundary for the 10^-P quantum.
						// Nearest policies therefore produce zero.  A directed policy
						// produces either zero or exactly one quantum according to its
						// sign rule.  Those two one-digit decimals are the complete
						// rounded result, so neither a guard digit nor a prefix window is
						// needed.  This branch has already proved that -P is representable.
						::fast_io::details::exact_precision_compact_window_decimal rounded{};
						if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
						{
							rounded.digits[0] = 0u;
						}
						else
						{
							rounded.digits[0] = static_cast<unsigned char>(
								::fast_io::details::floating_rounding_directed_round_up<rounding>(
									negative));
						}
						rounded.size = 1u;
						rounded.exponent = -static_cast<::std::int_least32_t>(precision);
						return {floating_precise_make_rounded_metadata(rounded), true};
					}
					keep = int32_max < corrected_keep
							   ? int32_max
							   : static_cast<::std::int_least32_t>(corrected_keep);
				}
				else
				{
					keep = int32_max < requested_keep
							   ? int32_max
							   : static_cast<::std::int_least32_t>(requested_keep);
				}
			}
		}

		/*
		Before rejecting an oversized materialization window, test whether the exact
		binary value already fits entirely inside the retained coefficient.  The
		identity helper materializes one leading digit only and proves both the
		integer and genuinely fractional domains.  Consequently large integers and
		fine P grids remain O(1) even when `keep` exceeds the bounded window capacity;
		an identity miss still retains the ordinary prefix/guard/sticky path below.
		*/
		if (static_cast<::std::int_least32_t>((::std::numeric_limits<flt>::max_digits10)) <= keep)
		{
			auto identity{floating_precise_try_exact_identity_metadata<flt>(
				mantissa, exponent, keep, real_exponent,
				initial_exponent_may_cross_decade)};
			initial_exponent_may_cross_decade = false;
			if (identity.success)
			{
				return {{identity.decimal, identity.decimal}, true};
			}
			if (fractional_grid)
			{
				if (precision_exceeds_int32)
				{
					return {};
				}
				auto const corrected_keep{static_cast<::std::int_least64_t>(real_exponent) +
										  static_cast<::std::int_least64_t>(precision) + 1};
				if (corrected_keep < 0 ||
					::fast_io::details::exact_precision_window_digit_capacity <=
						static_cast<::std::uint_least64_t>(corrected_keep))
				{
					return {};
				}
				keep = static_cast<::std::int_least32_t>(corrected_keep);
			}
		}
		if (keep < 0 || ::fast_io::details::exact_precision_window_digit_capacity <=
							static_cast<::std::size_t>(keep))
		{
			return {};
		}

		// One additional digit is sufficient for every rounding policy: it is
		// the guard, while the materializer's tail_nonzero bit is the sticky.
		// The pair determines the unique rounded coefficient; no later layout
		// predicate can observe any discarded digit individually.
		auto const requested_digits{static_cast<::std::size_t>(keep) + 1u};
		if (requested_digits <=
			::fast_io::details::exact_precision_compact_window_digit_capacity)
		{
			auto generated{::fast_io::details::exact_precision_compact_window_from_binary<flt>(
				mantissa, exponent, requested_digits, real_exponent)};
			if (generated.success)
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
				return {fractional_grid
					? floating_precise_make_fractional_rounded_metadata(
						generated.decimal, precision, rounded)
					: floating_precise_make_rounded_metadata(generated.decimal), true};
			}
		}
		auto generated{::fast_io::details::exact_precision_window_from_binary<flt>(
			mantissa, exponent, requested_digits, real_exponent)};
		if (!generated.success)
		{
			return {};
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
		return {fractional_grid
			? floating_precise_make_fractional_rounded_metadata(
				generated.decimal, precision, rounded)
			: floating_precise_make_rounded_metadata(generated.decimal), true};
	}
	return {};
}
#endif

/*
The complete expansion is a correctness fallback, not a presentation policy.
Outlining it behind `<flt, rounding>` gives all four formats, both precision
interpretations and both trailing-zero policies one COMDAT arithmetic body.  The caller supplies
only whether P denotes a fixed decimal grid; punctuation and JSON decoration
remain in the thin layout function after this helper returns.
The optional noinline attribute controls placement and COMDAT reuse only; it is
not an arithmetic capability requirement.
*/
template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
[[nodiscard]] inline constexpr floating_precise_rounded_metadata
floating_precise_prepare_full_exact_metadata(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t precision,
	::std::int_least32_t fixed_keep, bool fractional_grid, bool negative) noexcept
{
	auto decimal{::fast_io::details::exact_precision_from_binary<flt>(mantissa, exponent)};
	auto keep{fixed_keep};
	if (fractional_grid)
	{
		constexpr auto int32_max{(::std::numeric_limits<::std::int_least32_t>::max)()};
		if (static_cast<::std::size_t>(int32_max) < precision)
		{
			keep = int32_max;
		}
		else
		{
			auto const real_exponent{
				decimal.exponent + static_cast<::std::int_least32_t>(decimal.size) - 1};
			auto const requested_keep{static_cast<::std::int_least64_t>(real_exponent) +
									  static_cast<::std::int_least64_t>(precision) + 1};
			keep = int32_max < requested_keep
					   ? int32_max
					   : static_cast<::std::int_least32_t>(requested_keep);
		}
	}
	auto const rounded{static_cast<::std::int_least32_t>(decimal.size) > keep};
	::fast_io::details::exact_precision_round<rounding>(decimal, keep, negative);
	return fractional_grid
		? floating_precise_make_fractional_rounded_metadata(
			decimal, precision, rounded)
		: floating_precise_make_rounded_metadata(decimal);
}

template <typename flt>
[[nodiscard]] inline constexpr floating_precise_rounded_metadata
floating_precise_make_carrier_metadata(
	::fast_io::details::dragonbox_decimal_mantissa_type<flt> mantissa,
	::std::int_least32_t exponent) noexcept
{
	auto const raw_size{static_cast<::std::size_t>(
		::fast_io::details::chars_len<10u, true>(mantissa))};
	floating_precise_decimal_metadata raw{
		{static_cast<unsigned char>(mantissa != 0u)}, raw_size, exponent};
	auto trimmed{raw};
	for (; mantissa && mantissa % 10u == 0u; mantissa /= 10u)
	{
		--trimmed.size;
		++trimmed.exponent;
	}
	if (!mantissa)
	{
		trimmed.exponent = 0;
	}
	return {raw, trimmed};
}

/*
All decimal arithmetic preceding presentation is shared by target kind at
runtime.  `significant` describes significant/scientific requests, while
`fractional_grid` asks for the 10^-P fixed grid.  Neither general/fixed/decimal
selection nor JSON punctuation enters this body.  Returning both raw and
trimmed metadata preserves the emitter's carrier/window distinction: a
rounded window may retain a significant terminal zero, whereas a
non-preserving caller observes its canonical quotient.
*/
template <typename flt, ::fast_io::manipulators::floating_rounding rounding>
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#elif __has_cpp_attribute(msvc::noinline)
[[msvc::noinline]]
#endif
[[nodiscard]] inline constexpr floating_precise_rounded_metadata
floating_precise_prepare_precision_metadata(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t precision,
	::std::size_t significant, ::std::int_least32_t fixed_keep,
	bool fractional_grid, bool require_fractional_rounding_state,
	bool negative) noexcept
{
	auto carrier{[&]() constexpr noexcept {
		if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
		{
			return ::fast_io::details::dragonbox_impl<flt, rounding>(
				mantissa, static_cast<::std::int_least32_t>(exponent), negative);
		}
		else
		{
			return ::fast_io::details::dragonbox_impl<flt,
													  ::fast_io::manipulators::floating_rounding::nearest_to_even>(
				mantissa, static_cast<::std::int_least32_t>(exponent), negative);
		}
	}()};
	auto const length{static_cast<::std::size_t>(
		::fast_io::details::chars_len<10u, true>(carrier.m10))};
	auto const real_exponent{
		carrier.e10 + static_cast<::std::int_least32_t>(length) - 1};

	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		::std::size_t requested{significant};
		if (fractional_grid)
		{
			if (0 <= real_exponent)
			{
				requested = ::fast_io::details::exact_precision_saturating_add(
					::fast_io::details::exact_precision_saturating_add(
						static_cast<::std::size_t>(real_exponent), precision),
					1u);
			}
			else
			{
				auto const leading_zeroes{static_cast<::std::size_t>(-real_exponent)};
				requested = leading_zeroes <= precision
								? precision - leading_zeroes + 1u
								: 0u;
			}
		}
		bool carrier_is_exact_enough{};
		if (requested && requested < length)
		{
			auto const cut{length - requested};
			if (cut < 20u)
			{
				auto const divisor{
					::fast_io::details::print_rsv_fp_pow10_0_to_19_table[cut]};
				auto const remainder{static_cast<::std::uint_least64_t>(
					carrier.m10 % divisor)};
				auto const twice_remainder{remainder << 1u};
				auto const distance{twice_remainder < divisor
										? divisor - twice_remainder
										: twice_remainder - divisor};
				carrier_is_exact_enough = 1u < distance;
			}
		}
		else if (exponent && length <= requested &&
				 requested <= static_cast<::std::size_t>((::std::numeric_limits<flt>::digits10)))
		{
			/*
			The shortest carrier contains no sticky bit.  Ordinarily that is enough:
			for a normal value and at most digits10 requested digits, padding cannot
			cross a nearest boundary.  General fractional-preserve has one additional
			observable, however: it synthesizes the 10^-P suffix only when rounding
			actually discarded a nonzero exact tail.  When the requested width exceeds
			the carrier width, only the exact window can distinguish that event from a
			dyadic value already on the grid.  Equality needs no suffix and remains a
			safe carrier result.  This is the size-only counterpart of the emitter's
			general/fractional-preserve carrier gate; other formats must retain their
			established padding path.
			*/
			carrier_is_exact_enough =
				!require_fractional_rounding_state || length == requested;
		}
		if (carrier_is_exact_enough)
		{
			if (fractional_grid)
			{
				::fast_io::details::print_rsv_fp_round_to_fractional<flt, rounding>(
					carrier.m10, carrier.e10, precision, negative);
			}
			else
			{
				::fast_io::details::print_rsv_fp_round_to_significant<flt, rounding, false>(
					carrier.m10, carrier.e10, significant, negative);
			}
			return floating_precise_make_carrier_metadata<flt>(carrier.m10, carrier.e10);
		}

		if (fractional_grid)
		{
			constexpr auto int32_max{
				(::std::numeric_limits<::std::int_least32_t>::max)()};
			if (precision <= static_cast<::std::size_t>(int32_max) &&
				static_cast<::std::int_least64_t>(real_exponent) +
						static_cast<::std::int_least64_t>(precision) <=
					-2)
			{
				/*
				A leading digit at least two decimal places below the 10^-P
				quantum is strictly below its halfway boundary.  Every nearest
				policy therefore rounds to zero.  This is the same sufficient
				condition used by the emitter after its shortest-carrier miss;
				returning both the requested raw quantum and canonical zero lets
				the presentation layer reproduce preserving and trimming modes.
				*/
				floating_precise_decimal_metadata raw{
					{0u}, 1u, -static_cast<::std::int_least32_t>(precision)};
				floating_precise_decimal_metadata trimmed{{0u}, 1u, 0};
				return {raw, trimmed};
			}
		}

		// P16--P17 on MSVC x64 shares the proved outlined carrier used by emission;
		// native-u128 targets additionally support P18--P38.  Every miss is merely
		// an optimization rejection and falls through to exact materialization.
#if defined(__SIZEOF_INT128__) || \
	(defined(_MSC_VER) && defined(_M_X64) && !defined(__clang__) && \
	 !(defined(__arm64ec__) || defined(_M_ARM64EC)))
		if constexpr (::fast_io::details::dragonbox_uses_binary64_core<flt>)
		{
			if (!fractional_grid)
			{
				auto const narrow_da{floating_precise_try_binary64_narrow_da_metadata(
					static_cast<::std::uint_least64_t>(mantissa), exponent, significant)};
				if (narrow_da.success)
				{
					return {narrow_da.decimal, narrow_da.decimal};
				}
				// The P20--P38 coefficient type is native u128; MSVC must not
				// name or parse this helper after its independent P16--P17 probe.
#if defined(__SIZEOF_INT128__)
				auto const wide_da{floating_precise_try_binary64_wide_da_metadata(
					static_cast<::std::uint_least64_t>(mantissa), exponent, significant)};
				if (wide_da.success)
				{
					return {wide_da.decimal, wide_da.decimal};
				}
#endif
			}
		}
#endif
	}
	else if (fractional_grid)
	{
		constexpr auto int32_max{
			(::std::numeric_limits<::std::int_least32_t>::max)()};
		if (precision <= static_cast<::std::size_t>(int32_max) &&
			static_cast<::std::int_least64_t>(real_exponent) +
					static_cast<::std::int_least64_t>(precision) <=
				-2)
		{
			/*
			The strict half-quantum proof above is independent of rounding
			direction.  A directed policy selects either canonical zero or one
			exact 10^-P quantum solely from the value's sign and its direction;
			no coefficient digit can affect that choice.  This reproduces the
			emitter's directed tiny branch without opening an exact expansion.
			*/
			auto const digit{static_cast<unsigned char>(
				::fast_io::details::floating_rounding_directed_round_up<rounding>(
					negative))};
			floating_precise_decimal_metadata raw{
				{digit}, 1u, -static_cast<::std::int_least32_t>(precision)};
			auto trimmed{raw};
			if (!digit)
			{
				trimmed.exponent = 0;
			}
			return {raw, trimmed};
		}
	}

	// The exact window is shared by all presentations and all trailing-zero
	// policies.  Its miss is an arithmetic capacity rejection only.
#if defined(__SIZEOF_INT128__)
	if constexpr (::std::same_as<flt, float> || ::std::same_as<flt, double>)
	{
		auto const window{floating_precise_prepare_exact_window_metadata<flt, rounding>(
			mantissa, exponent, precision, fixed_keep, fractional_grid, negative,
			real_exponent,
			carrier.m10 == ::fast_io::details::print_rsv_fp_pow10_0_to_19_table[length - 1u])};
		if (window.success)
		{
			return window.metadata;
		}
	}
#endif
	return floating_precise_prepare_full_exact_metadata<flt, rounding>(
		mantissa, exponent, precision, fixed_keep, fractional_grid, negative);
}

template <typename flt, ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  ::fast_io::manipulators::floating_rounding rounding, bool json_float>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_exact_precision_nonzero_size(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t precision, bool negative) noexcept
{
	if constexpr (rounding != ::fast_io::manipulators::floating_rounding::current_environment)
	{
		constexpr bool fractional{
			::fast_io::details::floating_precision_is_fractional<precision_mode>};
		constexpr bool preserve{
			::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>};
		constexpr bool fractional_grid{fractional &&
									   format != ::fast_io::manipulators::floating_format::scientific};
		constexpr auto int32_max{(::std::numeric_limits<::std::int_least32_t>::max)()};
		::std::size_t significant{};
		if constexpr (!fractional_grid)
		{
			if constexpr (fractional)
			{
				significant = ::fast_io::details::exact_precision_saturating_add(precision, 1u);
			}
			else
			{
				significant = precision ? precision : 1u;
			}
		}
		auto const fixed_keep{significant > static_cast<::std::size_t>(int32_max)
								  ? int32_max
								  : static_cast<::std::int_least32_t>(significant)};
		if constexpr (::fast_io::details::exact_precision_is_wide_binary<flt>)
		{
			/*
			Wide types intentionally bypass the binary32/binary64 Schubfach
			carrier in `floating_precise_prepare_precision_metadata`: its cache
			and endpoint arithmetic are proved only for the narrow exponent
			domain.  A bounded window success returns exactly the decimal used by
			emission for normal and subnormal values.  A capacity, grid or
			equal-endpoint rejection retains the complete exact expansion, so the
			optimization is a pure fast-path refinement with an authoritative
			fallback.  Frontends without a native scalar uint128 do not name the
			window helpers and enter that fallback directly.
			*/
#if defined(__SIZEOF_INT128__)
			if (precision <= 128u)
			{
				auto const window{floating_precise_wide_window_size<flt, json_float>(
					mantissa, exponent, precision, negative, format,
					precision_mode, rounding)};
				if (window.success)
				{
					return window.size;
				}
			}
#endif
			auto const prepared{floating_precise_prepare_full_exact_metadata<flt, rounding>(
				mantissa, exponent, precision, fixed_keep, fractional_grid, negative)};
			auto const decimal{preserve ? prepared.raw : prepared.trimmed};
			return floating_precise_rounded_precision_size<flt, format, precision_mode,
				json_float>(decimal, precision, significant);
		}
		else
		{
			constexpr bool require_fractional_rounding_state{
				format == ::fast_io::manipulators::floating_format::general &&
				precision_mode == ::fast_io::manipulators::floating_precision::
					fractional_preserve_trailing_zero};
			auto const prepared{floating_precise_prepare_precision_metadata<flt, rounding>(
				mantissa, exponent, precision, significant, fixed_keep,
				fractional_grid, require_fractional_rounding_state, negative)};
			auto const decimal{preserve ? prepared.raw : prepared.trimmed};
			return floating_precise_rounded_precision_size<flt, format, precision_mode,
				json_float>(decimal, precision, significant);
		}
	}
	else if constexpr (rounding == ::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return floating_precise_exact_precision_nonzero_size<flt, format, precision_mode,
																 ::fast_io::manipulators::floating_rounding::toward_plus_infinity, json_float>(
				mantissa, exponent, precision, negative);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return floating_precise_exact_precision_nonzero_size<flt, format, precision_mode,
																 ::fast_io::manipulators::floating_rounding::toward_minus_infinity, json_float>(
				mantissa, exponent, precision, negative);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return floating_precise_exact_precision_nonzero_size<flt, format, precision_mode,
																 ::fast_io::manipulators::floating_rounding::toward_zero, json_float>(
				mantissa, exponent, precision, negative);
		default:
			return floating_precise_exact_precision_nonzero_size<flt, format, precision_mode,
																 ::fast_io::manipulators::floating_rounding::nearest_to_even, json_float>(
				mantissa, exponent, precision, negative);
		}
	}
}

template <typename flt, ::fast_io::manipulators::floating_precision precision_mode,
		  bool json_float>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_precision_zero_body_size(
	::fast_io::manipulators::floating_format format, ::std::size_t precision) noexcept
{
	constexpr bool significant{
		::fast_io::details::floating_precision_is_significant<precision_mode>};
	constexpr bool fractional{
		::fast_io::details::floating_precision_is_fractional<precision_mode>};
	constexpr bool preserve{
		::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>};
	if (format == ::fast_io::manipulators::floating_format::scientific)
	{
		auto fractional_digits{precision};
		if constexpr (significant)
		{
			fractional_digits = precision ? precision - 1u : 0u;
		}
		::std::size_t size{1u};
		if constexpr (preserve)
		{
			if (fractional_digits)
			{
				size = floating_precise_add(size, floating_precise_add(1u, fractional_digits));
			}
		}
		return floating_precise_add(size, floating_precise_decimal_exponent_size<flt>(0));
	}
	if constexpr (fractional)
	{
		if constexpr (json_float)
		{
			if (!precision || !preserve)
			{
				return 3u;
			}
		}
		if constexpr (preserve)
		{
			return precision ? floating_precise_add(2u, precision) : 1u;
		}
		return 1u;
	}
	if constexpr (significant && preserve)
	{
		if constexpr (json_float)
		{
			if (precision <= 1u)
			{
				return 3u;
			}
		}
		return 1u < precision ? floating_precise_add(1u, precision) : 1u;
	}
	return 1u + (json_float ? 2u : 0u);
}

template <typename flt, ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_precision precision_mode, bool json_float>
[[nodiscard]] inline constexpr ::std::size_t
floating_precise_fractional_tiny_zero_body_size(::std::size_t precision) noexcept
{
	static_assert(::fast_io::details::floating_precision_is_fractional<precision_mode>);
	static_assert(format != ::fast_io::manipulators::floating_format::scientific);
	constexpr bool preserve{
		::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>};
	if constexpr (format == ::fast_io::manipulators::floating_format::general)
	{
		if constexpr (!preserve)
		{
			return 1u + (json_float ? 2u : 0u);
		}
		if (!precision)
		{
			return 1u + (json_float ? 2u : 0u);
		}
		if (precision < 5u)
		{
			return floating_precise_add(2u, precision);
		}
		return floating_precise_add(1u,
									floating_precise_decimal_exponent_size<flt>(
										-static_cast<::std::int_least32_t>(precision)));
	}
	else
	{
		if constexpr (json_float)
		{
			if (!precision || !preserve)
			{
				return 3u;
			}
		}
		if constexpr (preserve)
		{
			return precision ? floating_precise_add(2u, precision) : 1u;
		}
		return 1u;
	}
}

template <bool showpos, bool nan_show_sign, bool nan_show_type,
		  ::fast_io::manipulators::floating_format format,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  ::fast_io::manipulators::floating_rounding rounding, bool json_float, typename flt>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_precision_size(
	flt value, ::std::size_t precision) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr mantissa_type exponent_mask{
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u};
	auto const [mantissa, exponent, negative]{::fast_io::details::get_punned_result(value)};
	if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
	{
		return floating_precise_special_size<showpos, nan_show_sign, nan_show_type,
											 trait::mbits>(mantissa, negative);
	}
	if constexpr (::fast_io::details::dragonbox_uses_binary32_core<flt> &&
				  sizeof(flt) < sizeof(float))
	{
		// Build binary32 from the already classified integer fields.  Besides being
		// exact for every finite binary16/bfloat16 value, this prevents a target
		// FCVT from being speculated across the special-value check and quieting an
		// sNaN or raising FE_INVALID on the size-only path.
		auto const widened{
			::fast_io::details::dragonbox_narrow_float_from_fields<flt>(
				mantissa, exponent, negative)};
		return floating_precise_precision_size<showpos, nan_show_sign, nan_show_type,
											   format, precision_mode, rounding, json_float>(widened, precision);
	}
	auto const sign_size{floating_precise_sign_size<showpos>(negative)};
	if (!mantissa && !exponent)
	{
		return floating_precise_add(sign_size,
									floating_precise_precision_zero_body_size<flt, precision_mode, json_float>(
										format, precision));
	}
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding> &&
				  ::fast_io::details::floating_precision_is_fractional<precision_mode> &&
				  format != ::fast_io::manipulators::floating_format::scientific &&
				  (::std::same_as<flt, float> || ::std::same_as<flt, double>))
	{
		// Reuse emission's proved binary upper bound before either decimal
		// carrier.  When the magnitude is strictly below half a 10^-P quantum,
		// all nearest policies produce zero.  Counting its canonical/preserved
		// grammar directly also covers general's P=5 scientific transition.
		constexpr ::std::uint_least32_t half_exponent{
			(static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 2u};
		bool tiny{!precision && exponent < half_exponent};
		if (precision)
		{
			constexpr auto subnormal_zero_precision{static_cast<::std::size_t>(
				-(::std::numeric_limits<flt>::min_exponent10))};
			constexpr ::std::uint_least32_t exponent_bias{half_exponent + 1u};
			if (exponent + 32u < exponent_bias)
			{
				tiny = (exponent <= 1u && precision <= subnormal_zero_precision) ||
					   ::fast_io::details::exact_precision_fractional_tiny_binary_bound<flt>(
						   exponent, precision);
			}
		}
		if (tiny)
		{
			return floating_precise_add(sign_size,
										floating_precise_fractional_tiny_zero_body_size<flt, format,
																						precision_mode, json_float>(precision));
		}
	}
	return floating_precise_add(sign_size,
								floating_precise_exact_precision_nonzero_size<flt, format, precision_mode,
																			  rounding, json_float>(mantissa, exponent, precision, negative));
}

/// @brief Computes the exact shortest-hex length through the native floating ABI.
/// @details The scalar intentionally remains by value and therefore mirrors the low-level writer's floating-register
/// convention.  The upper precise CPO field-normalizes the exceptional Clang x86 __bf16 domain.
template <bool showbase, bool showpos, bool nan_show_sign, bool nan_show_type, typename flt>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_hex_shortest_size(flt value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr mantissa_type exponent_mask{
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u};
	constexpr ::std::uint_least32_t bias{
		(static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u};
	auto const [mantissa, exponent, negative]{::fast_io::details::get_punned_result(value)};
	if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
	{
		return floating_precise_special_size<showpos, nan_show_sign, nan_show_type,
											 trait::mbits>(mantissa, negative);
	}
	auto size{floating_precise_sign_size<showpos>(negative) + (showbase ? 2u : 0u)};
	if (!mantissa && !exponent)
	{
		return size + 4u; // 0p+0
	}
	auto binary_exponent{static_cast<::std::int_least32_t>(exponent) -
						 static_cast<::std::int_least32_t>(bias)};
	if (mantissa)
	{
		constexpr ::std::uint_least32_t makeup_bits{
			((trait::mbits / 4u + 1u) * 4u - trait::mbits) % 4u};
		auto const trailing_bits{static_cast<::std::uint_least32_t>(
									 ::fast_io::details::my_countr_zero_unchecked(mantissa)) +
								 makeup_bits};
		constexpr ::std::uint_least32_t maximum_fraction_digits{
			static_cast<::std::uint_least32_t>((trait::mbits + makeup_bits) >> 2u)};
		auto const mantissa_length{maximum_fraction_digits - (trailing_bits >> 2u)};
		if (!exponent)
		{
			++binary_exponent;
		}
		size += static_cast<::std::size_t>(mantissa_length) + 2u;
	}
	else
	{
		size += 1u;
	}
	return size + floating_precise_hex_exponent_size(binary_exponent);
}

/// @brief Computes an exact precision-controlled hex length through the native floating ABI.
/// @details The scalar intentionally remains by value; the upper CPO handles exceptional object-field extraction so
/// this general low-level API does not impose a reference convention on every supported IEC 60559 type.
template <bool showbase, bool showpos, bool nan_show_sign, bool nan_show_type,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  ::fast_io::manipulators::floating_rounding rounding, typename flt>
[[nodiscard]] inline constexpr ::std::size_t floating_precise_hex_precision_size(
	flt value, ::std::size_t precision) noexcept
{
	if constexpr (rounding == ::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return floating_precise_hex_precision_size<showbase, showpos, nan_show_sign,
													   nan_show_type, precision_mode,
													   ::fast_io::manipulators::floating_rounding::toward_plus_infinity>(value, precision);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return floating_precise_hex_precision_size<showbase, showpos, nan_show_sign,
													   nan_show_type, precision_mode,
													   ::fast_io::manipulators::floating_rounding::toward_minus_infinity>(value, precision);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return floating_precise_hex_precision_size<showbase, showpos, nan_show_sign,
													   nan_show_type, precision_mode,
													   ::fast_io::manipulators::floating_rounding::toward_zero>(value, precision);
		default:
			return floating_precise_hex_precision_size<showbase, showpos, nan_show_sign,
													   nan_show_type, precision_mode,
													   ::fast_io::manipulators::floating_rounding::nearest_to_even>(value, precision);
		}
	}
	else
	{
		using trait = ::fast_io::details::iec559_traits<flt>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr mantissa_type exponent_mask{
			(static_cast<mantissa_type>(1u) << trait::ebits) - 1u};
		constexpr ::std::uint_least32_t bias{
			(static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u};
		constexpr ::std::uint_least32_t makeup_bits{
			((trait::mbits / 4u + 1u) * 4u - trait::mbits) % 4u};
		constexpr ::std::size_t fractional_hex_digits{(trait::mbits + makeup_bits) >> 2u};
		constexpr ::std::size_t total_hex_digits{fractional_hex_digits + 1u};
		auto const [mantissa, exponent, negative]{::fast_io::details::get_punned_result(value)};
		if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return floating_precise_special_size<showpos, nan_show_sign, nan_show_type,
												 trait::mbits>(mantissa, negative);
		}
		constexpr bool fractional_precision{
			::fast_io::details::floating_precision_is_fractional<precision_mode>};
		constexpr bool preserve{
			::fast_io::details::floating_precision_preserves_trailing_zero<precision_mode>};
		auto total_precision{precision};
		if constexpr (fractional_precision)
		{
			constexpr auto size_max{(::std::numeric_limits<::std::size_t>::max)()};
			if (total_precision != size_max)
			{
				++total_precision;
			}
		}
		else if (!total_precision)
		{
			total_precision = 1u;
		}
		auto size{floating_precise_sign_size<showpos>(negative) + (showbase ? 2u : 0u)};
		if (!mantissa && !exponent)
		{
			auto const digits_to_print{preserve ? total_precision : 1u};
			size = floating_precise_add(size, digits_to_print);
			if (1u < digits_to_print)
			{
				size = floating_precise_add(size, 1u);
			}
			return floating_precise_add(size, floating_precise_hex_exponent_size(0));
		}

		auto binary_exponent{static_cast<::std::int_least32_t>(exponent) -
							 static_cast<::std::int_least32_t>(bias)};
		if (!exponent)
		{
			++binary_exponent;
		}
		auto const aligned_mantissa{static_cast<mantissa_type>(mantissa << makeup_bits)};
		// Init-capture transports the structured-binding scalar while preserving the exact sizing observation.
		auto const hex_digit_at = [aligned_mantissa, exponent_value = exponent](::std::size_t index) constexpr noexcept {
			if (!index)
			{
				return static_cast<::std::uint_least32_t>(exponent_value ? 1u : 0u);
			}
			if (fractional_hex_digits < index)
			{
				return ::std::uint_least32_t{};
			}
			auto const shift{static_cast<::std::uint_least32_t>(
				(fractional_hex_digits - index) << 2u)};
			return static_cast<::std::uint_least32_t>(
				(aligned_mantissa >> shift) & static_cast<mantissa_type>(0xFu));
		};
		unsigned char digits[total_hex_digits + 1u]{};
		auto const retained_digits{
			total_precision < total_hex_digits ? total_precision : total_hex_digits};
		for (::std::size_t index{}; index != retained_digits; ++index)
		{
			digits[index] = static_cast<unsigned char>(hex_digit_at(index));
		}
		if (total_precision < total_hex_digits)
		{
			auto const next_digit{hex_digit_at(total_precision)};
			bool tail_nonzero{};
			for (auto index{total_precision + 1u}; index != total_hex_digits; ++index)
			{
				if (hex_digit_at(index))
				{
					tail_nonzero = true;
					break;
				}
			}
			if (::fast_io::details::print_rsvhexfloat_round_up<rounding>(
					negative, digits[total_precision - 1u], next_digit, tail_nonzero))
			{
				for (auto index{total_precision}; index; --index)
				{
					auto &digit{digits[index - 1u]};
					if (digit != 0xFu)
					{
						++digit;
						break;
					}
					digit = 0u;
				}
				if constexpr (
					precision_mode !=
					::fast_io::manipulators::floating_precision::
						charconv_hex_fractional)
				{
					/*
					After carry, 2*2^E and 1*2^(E+1) are mathematically
					identical.  Native hexadecimal presentation selects the
					normalized latter form, so its exponent-width contribution
					must be measured after the increment.  Charconv fractional
					precision instead retains 2p+E; excluding it here keeps the
					size proof identical to the emitter and therefore prevents
					an exact-fit buffer from being rejected without granting
					the writer any additional storage.
					*/
					if (exponent != 0u && digits[0] == 2u)
					{
						digits[0] = 1u;
						for (::std::size_t index{1u};
							 index != retained_digits; ++index)
						{
							digits[index] = 0u;
						}
						++binary_exponent;
					}
				}
			}
		}
		auto digits_to_print{retained_digits};
		if constexpr (preserve)
		{
			digits_to_print = total_precision;
		}
		else
		{
			for (; 1u < digits_to_print && !digits[digits_to_print - 1u]; --digits_to_print)
			{
			}
		}
		size = floating_precise_add(size, digits_to_print);
		if (1u < digits_to_print)
		{
			size = floating_precise_add(size, 1u);
		}
		return floating_precise_add(
			size, floating_precise_hex_exponent_size(binary_exponent));
	}
}

} // namespace fast_io::details

namespace fast_io
{

namespace details
{

// A precise CPO must be callable for every type admitted by its concept;
// leaving an unsupported decimal type to a function-body static_assert would
// make requires-expressions report a false capability.  Hexadecimal emission
// works directly from every available IEC 60559 field decomposition.  Scalar
// decimal emission remains limited to the proved shortest domains.  Runtime-
// precision decimal emission additionally admits the strict binary80/binary128
// representation gate, where both size and output use the same exact decimal
// expansion and never narrow the value to binary64.
//
// A precise reservation is a two-call protocol: size is observed before the
// ordinary writer is invoked.  `current_environment` would independently read
// the process rounding mode in those two calls, so an intervening fesetround
// could shorten the output or overrun an exact allocation.  It is therefore
// excluded whenever rounding participates.  Shortest hexadecimal conversion
// does not consult rounding and remains safe with that otherwise inert flag.
template <::fast_io::manipulators::scalar_flags flags, typename flt>
inline constexpr bool floating_precise_scalar_supported{
	::fast_io::details::print_floating_scalar_supported<flags, flt> &&
	(flags.floating == ::fast_io::manipulators::floating_format::hexfloat ||
	 flags.rounding != ::fast_io::manipulators::floating_rounding::current_environment)};

template <::fast_io::manipulators::scalar_flags flags, typename flt>
inline constexpr bool floating_precise_precision_supported{
	::fast_io::details::print_floating_precision_supported<flags, flt> &&
	::fast_io::details::print_floating_precision_valid<flags.precision> &&
	flags.rounding != ::fast_io::manipulators::floating_rounding::current_environment};

template <::fast_io::manipulators::scalar_flags flags, typename flt>
inline constexpr bool floating_precise_range_supported{
	::fast_io::details::print_floating_scalar_supported<flags, flt> &&
	flags.floating != ::fast_io::manipulators::floating_format::hexfloat &&
	flags.rounding != ::fast_io::manipulators::floating_rounding::current_environment};

/*
The x86 Clang bfloat16 workaround must cover the public CPO boundary itself.
Without AVX512-BF16, copying the owning manipulator by value can rematerialize
its stored raw-bit-one subnormal before the callee can extract integer fields;
once lost, the later field transport cannot recover it.  Borrow only that
legacy x86 domain.  With AVX512-BF16 the CPO instead receives a compact integer
field carrier by value.  Its converting constructor captures the caller's
object representation before a native bfloat copy can be introduced, while the
CPO and all lower formatters retain register-class by-value transport.  This is
also robust against an optimizer-inlined nested copy quieting an sNaN.  Every
other floating type keeps the established by-value owning manipulator and its
measured code shape; AArch64 never enters either x86-only workaround.
*/
template <typename manipulator, typename flt>
using floating_precise_parameter_t = ::std::conditional_t<
	::fast_io::details::print_floating_decimal_requires_integer_transport<flt>,
	manipulator const &,
	::std::conditional_t<
		::fast_io::details::print_floating_requires_object_field_capture<flt>,
		::fast_io::details::floating_precise_field_parameter<manipulator, flt>,
		manipulator>>;

template <typename manipulator, typename flt>
using floating_precise_range_parameter_t = ::std::conditional_t<
	::fast_io::details::print_floating_requires_object_field_capture<flt>,
	::fast_io::details::floating_precise_range_field_parameter<manipulator, flt>,
	manipulator>;

template <typename flt, typename parameter>
[[nodiscard]] inline constexpr auto
floating_precise_captured_fields(parameter const &value) noexcept
{
	if constexpr (requires { value.fields; })
	{
		return value.fields;
	}
	else
	{
		return ::fast_io::details::compiler_constant_floating_capture_fields<
			::std::remove_cvref_t<flt>>(
			value.reference);
	}
}

#if defined(__SIZEOF_INT128__)

/*
Exact-decimal size metadata
===========================

For a nonzero finite binary value x=M*2^E, let

    K = floor(log2(x)),  a = floor(K*log10(2)).

The binade [2^K,2^(K+1)) is narrower than one decade.  Consequently
floor(log10(x)) is either a or a+1, and deciding which one needs exactly one
comparison with 10^(a+1).  The 249-bit wide-Ryu power cache makes that
comparison authoritative: its power-of-five prefix encloses the omitted tail;
only an unresolved prefix equality falls back to the complete exact backend.

After cancelling binary factors from M*2^E, its canonical decimal exponent q
is also cheap.  E<0 gives q=E because the remaining M is odd and M*5^-E has no
decimal trailing zero.  E>=0 gives q=min(v2(M)+E,v5(M)).  If d is the canonical
coefficient length and r=floor(log10(x)), then r=q+d-1, hence d=r-q+1.

This is a genuine metadata path: no coefficient limb array and no digit buffer
is constructed in the normal run-time case.  Constant evaluation deliberately
retains the independently tested exact-limb implementation, so compiler limits
and run-time cache policy cannot become part of the constexpr result.
*/

[[nodiscard]] inline constexpr unsigned
floating_precise_exact_bit_width(__uint128_t value) noexcept
{
	auto const high{static_cast<::std::uint_least64_t>(value >> 64u)};
	if (high)
	{
		return 128u - static_cast<unsigned>(::std::countl_zero(high));
	}
	return 64u - static_cast<unsigned>(::std::countl_zero(
		static_cast<::std::uint_least64_t>(value)));
}

[[nodiscard]] inline constexpr ::std::int_least32_t
floating_precise_exact_floor_log10_pow2(
	::std::int_least32_t exponent) noexcept
{
	/* floor(log10(2)*2^48).  On the complete binary16/bfloat16/binary32/
	binary64/binary80/binary128 K-domain, |K|<=16495.  The truncation error is
	below 5.77e-11 there, while exhaustive integer-boundary analysis gives a
	minimum distance above 2.76e-5 (at K=-13301). */
	constexpr ::std::int_least64_t multiplier{INT64_C(84732411018727)};
	constexpr ::std::int_least64_t denominator{INT64_C(1) << 48u};
	auto const product{static_cast<::std::int_least64_t>(exponent) * multiplier};
	if (0 <= product)
	{
		return static_cast<::std::int_least32_t>(product / denominator);
	}
	return static_cast<::std::int_least32_t>(
		-((-product + denominator - 1) / denominator));
}

[[nodiscard]] inline unsigned floating_precise_exact_cache_bit_width(
	::std::uint_least64_t const *power) noexcept
{
	for (unsigned index{4u}; index; --index)
	{
		auto const value{power[index - 1u]};
		if (value)
		{
			return (index - 1u) * 64u +
				64u - static_cast<unsigned>(::std::countl_zero(value));
		}
	}
	return 0u;
}

/* Compare M*2^binary_exponent with P*2^power_exponent.  P is the normalized
249-bit little-endian cache integer, while M has at most 113 bits. */
[[nodiscard]] inline int floating_precise_exact_compare_dyadics(
	__uint128_t mantissa, ::std::int_least32_t binary_exponent,
	::std::uint_least64_t const *power,
	::std::int_least32_t power_exponent) noexcept
{
	auto const mantissa_bits{
		::fast_io::details::floating_precise_exact_bit_width(mantissa)};
	auto const power_bits{
		::fast_io::details::floating_precise_exact_cache_bit_width(power)};
	auto const left_top{static_cast<::std::int_least32_t>(mantissa_bits - 1u) +
		binary_exponent};
	auto const right_top{static_cast<::std::int_least32_t>(power_bits - 1u) +
		power_exponent};
	if (left_top != right_top)
	{
		return left_top < right_top ? -1 : 1;
	}

	/* Equal top exponents imply shift=power_bits-mantissa_bits>=0 because the
	cache has 249 bits and every admitted significand has at most 113. */
	auto const shift{static_cast<unsigned>(binary_exponent - power_exponent)};
	auto const word_shift{shift / 64u};
	auto const bit_shift{shift % 64u};
	::std::uint_least64_t left[4u]{};
	::std::uint_least64_t const words[2u]{
		static_cast<::std::uint_least64_t>(mantissa),
		static_cast<::std::uint_least64_t>(mantissa >> 64u)};
	for (unsigned index{}; index != 2u; ++index)
	{
		auto const destination{word_shift + index};
		if (destination < 4u)
		{
			left[destination] |= words[index] << bit_shift;
		}
		if (bit_shift && destination + 1u < 4u)
		{
			left[destination + 1u] |= words[index] >> (64u - bit_shift);
		}
	}
	for (unsigned index{4u}; index; --index)
	{
		if (left[index - 1u] != power[index - 1u])
		{
			return left[index - 1u] < power[index - 1u] ? -1 : 1;
		}
	}
	return 0;
}

[[nodiscard]] inline unsigned floating_precise_exact_product_bit_width(
	__uint128_t mantissa, ::std::uint_least64_t const *power) noexcept
{
	::std::uint_least64_t const words[2u]{
		static_cast<::std::uint_least64_t>(mantissa),
		static_cast<::std::uint_least64_t>(mantissa >> 64u)};
	::std::uint_least64_t product[6u]{};
	for (unsigned left{}; left != 2u; ++left)
	{
		__uint128_t carry{};
		for (unsigned right{}; right != 4u; ++right)
		{
			auto const output{left + right};
			auto const value{static_cast<__uint128_t>(words[left]) *
				power[right] + product[output] + carry};
			product[output] = static_cast<::std::uint_least64_t>(value);
			carry = value >> 64u;
		}
		product[left + 4u] = static_cast<::std::uint_least64_t>(carry);
	}
	for (unsigned index{6u}; index; --index)
	{
		auto const value{product[index - 1u]};
		if (value)
		{
			return (index - 1u) * 64u +
				64u - static_cast<unsigned>(::std::countl_zero(value));
		}
	}
	return 0u;
}

enum class floating_precise_exact_power10_comparison : unsigned char
{
	below,
	at_least,
	ambiguous
};

[[nodiscard]] inline floating_precise_exact_power10_comparison
floating_precise_exact_compare_power10(
	__uint128_t mantissa, ::std::int_least32_t binary_exponent,
	::std::int_least32_t decimal_exponent) noexcept
{
	using namespace ::fast_io::details::wide_ryu;
	if (0 <= decimal_exponent)
	{
		auto const count{static_cast<::std::uint_least32_t>(decimal_exponent)};
		::std::uint_least64_t power[4u];
		generic_computePow5(count, power);
		// `count` is the nonnegative decimal exponent required by the unsigned power-cache contract.
		auto const cache_exponent{static_cast<::std::int_least32_t>(
			pow5bits(count)) -
			float_128_pow5_bitcount};
		/* M*2^E >= 10^n iff M*2^(E-n) >= 5^n. */
		auto const lower_comparison{
			::fast_io::details::floating_precise_exact_compare_dyadics(
				mantissa, binary_exponent - decimal_exponent, power,
				cache_exponent)};
		if (cache_exponent <= 0)
		{
			return lower_comparison < 0
				? floating_precise_exact_power10_comparison::below
				: floating_precise_exact_power10_comparison::at_least;
		}
		/* P*2^s <= 5^n < (P+1)*2^s. */
		if (lower_comparison <= 0)
		{
			return floating_precise_exact_power10_comparison::below;
		}
		for (unsigned index{}; index != 4u; ++index)
		{
			if (++power[index])
			{
				break;
			}
		}
		auto const upper_comparison{
			::fast_io::details::floating_precise_exact_compare_dyadics(
				mantissa, binary_exponent - decimal_exponent, power,
				cache_exponent)};
		return 0 <= upper_comparison
			? floating_precise_exact_power10_comparison::at_least
			: floating_precise_exact_power10_comparison::ambiguous;
	}

	auto const count{static_cast<::std::uint_least32_t>(-decimal_exponent)};
	::std::uint_least64_t power[4u];
	generic_computePow5(count, power);
	// Negation is performed before conversion, so `count` is again the cache's proved nonnegative exponent domain.
	auto const cache_exponent{static_cast<::std::int_least32_t>(
		pow5bits(count)) -
		float_128_pow5_bitcount};
	/* M*2^E >= 10^-n iff M*5^n*2^(E+n) >= 1. */
	auto const scaled_exponent{cache_exponent + binary_exponent - decimal_exponent};
	auto const lower_top{static_cast<::std::int_least32_t>(
		::fast_io::details::floating_precise_exact_product_bit_width(
			mantissa, power) - 1u) + scaled_exponent};
	if (0 <= lower_top)
	{
		return floating_precise_exact_power10_comparison::at_least;
	}
	if (cache_exponent <= 0)
	{
		return floating_precise_exact_power10_comparison::below;
	}
	for (unsigned index{}; index != 4u; ++index)
	{
		if (++power[index])
		{
			break;
		}
	}
	auto const upper_top{static_cast<::std::int_least32_t>(
		::fast_io::details::floating_precise_exact_product_bit_width(
			mantissa, power) - 1u) + scaled_exponent};
	return upper_top < 0
		? floating_precise_exact_power10_comparison::below
		: floating_precise_exact_power10_comparison::ambiguous;
}

template <typename flt>
[[nodiscard]] inline constexpr ::fast_io::details::exact_decimal_layout
floating_precise_exact_decimal_layout_from_binary(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent) noexcept
{
	if (::std::is_constant_evaluated())
	{
		return ::fast_io::details::exact_decimal_layout_from_binary<flt>(
			mantissa, exponent);
	}
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	/* binary16's complete coefficient is so small that its established limb
	path is measurably faster than reconstructing a cached boundary power. */
	if constexpr (trait::mbits == 10u && trait::ebits == 5u)
	{
		return ::fast_io::details::exact_decimal_layout_from_binary<flt>(
			mantissa, exponent);
	}
	auto const original_mantissa{mantissa};
	constexpr ::std::int_least32_t bias{
		(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
	::std::int_least32_t binary_exponent{};
	if (exponent)
	{
		mantissa |= static_cast<mantissa_type>(
			static_cast<mantissa_type>(1u) << trait::mbits);
		binary_exponent = static_cast<::std::int_least32_t>(exponent) - bias -
			static_cast<::std::int_least32_t>(trait::mbits);
	}
	else
	{
		binary_exponent = 1 - bias -
			static_cast<::std::int_least32_t>(trait::mbits);
	}
	auto const wide_mantissa{static_cast<__uint128_t>(mantissa)};
	auto const binary_floor{binary_exponent +
		static_cast<::std::int_least32_t>(
			::fast_io::details::floating_precise_exact_bit_width(wide_mantissa) - 1u)};
	auto real_exponent{
		::fast_io::details::floating_precise_exact_floor_log10_pow2(binary_floor)};
	auto const boundary{real_exponent + 1};
	auto const comparison{
		::fast_io::details::floating_precise_exact_compare_power10(
			wide_mantissa, binary_exponent, boundary)};
	if (comparison == floating_precise_exact_power10_comparison::ambiguous)
	{
		return ::fast_io::details::exact_decimal_layout_from_binary<flt>(
			original_mantissa, exponent);
	}
	real_exponent += static_cast<::std::int_least32_t>(
		comparison == floating_precise_exact_power10_comparison::at_least);

	auto reduced{wide_mantissa};
	auto canonical_binary_exponent{binary_exponent};
	for (; canonical_binary_exponent < 0 && (reduced & 1u) == 0u;
		 ++canonical_binary_exponent)
	{
		reduced >>= 1u;
	}
	::std::int_least32_t decimal_exponent{};
	if (canonical_binary_exponent < 0)
	{
		decimal_exponent = canonical_binary_exponent;
	}
	else
	{
		auto twos{canonical_binary_exponent};
		auto factor{reduced};
		for (; (factor & 1u) == 0u; factor >>= 1u)
		{
			++twos;
		}
		::std::int_least32_t fives{};
		for (factor = reduced; factor % 5u == 0u; factor /= 5u)
		{
			++fives;
		}
		decimal_exponent = twos < fives ? twos : fives;
	}
	auto const size{real_exponent - decimal_exponent + 1};
	return {static_cast<::std::size_t>(size), decimal_exponent};
}

#endif

template <::fast_io::manipulators::scalar_flags flags, typename flt>
[[nodiscard]] inline constexpr ::std::size_t
floating_precise_exact_decimal_fields_size(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u)};
	if (exponent == exponent_mask)
	{
		return ::fast_io::details::floating_precise_special_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type,
			trait::mbits>(mantissa, negative);
	}
	auto const sign_size{
		::fast_io::details::floating_precise_sign_size<flags.showpos>(negative)};
	if (mantissa == 0u && exponent == 0u)
	{
		constexpr ::std::size_t zero_size{
			flags.floating ==
					::fast_io::manipulators::floating_format::scientific
				? 5u
				: 1u + (flags.json_float ? 2u : 0u)};
		return sign_size + zero_size;
	}
	auto const decimal{
#if defined(__SIZEOF_INT128__)
		::fast_io::details::floating_precise_exact_decimal_layout_from_binary<flt>(
			mantissa, exponent)
#else
		::fast_io::details::exact_decimal_layout_from_binary<flt>(
			mantissa, exponent)
#endif
	};
	auto const real_exponent{decimal.exponent +
							 static_cast<::std::int_least32_t>(decimal.size) - 1};
	auto const fixed_size{[&]() constexpr noexcept {
		auto const point{real_exponent + 1};
		if (point <= 0)
		{
			return decimal.size + static_cast<::std::size_t>(1 - real_exponent);
		}
		auto const integer_digits{static_cast<::std::size_t>(point)};
		if (integer_digits < decimal.size)
		{
			return decimal.size + 1u;
		}
		return integer_digits + (flags.json_float ? 2u : 0u);
	}()};
	auto const scientific_size{decimal.size +
							   static_cast<::std::size_t>(decimal.size != 1u) +
							   ::fast_io::details::floating_precise_decimal_exponent_size<flt>(
								   real_exponent)};
	::std::size_t magnitude_size{};
	if constexpr (flags.floating ==
				  ::fast_io::manipulators::floating_format::fixed)
	{
		magnitude_size = fixed_size;
	}
	else if constexpr (flags.floating ==
					   ::fast_io::manipulators::floating_format::scientific)
	{
		magnitude_size = scientific_size;
	}
	else if constexpr (flags.floating ==
					   ::fast_io::manipulators::floating_format::general)
	{
		magnitude_size = -4 <= real_exponent && real_exponent < 6
							 ? fixed_size
							 : scientific_size;
	}
	else
	{
		/* decimal chooses its layout before JSON's fixed-only `.0` suffix is
		added.  Comparing the post-JSON fixed size would incorrectly switch an
		integer such as 9000 from the writer's `9000.0` to a five-byte
		scientific size. */
		auto fixed_selection_size{fixed_size};
		if constexpr (flags.json_float)
		{
			auto const point{real_exponent + 1};
			if (0 < point &&
				decimal.size <= static_cast<::std::size_t>(point))
			{
				fixed_selection_size -= 2u;
			}
		}
		/* decimal selects fixed on a length tie, matching the writer. */
		magnitude_size = fixed_selection_size <= scientific_size
								 ? fixed_size
								 : scientific_size;
	}
	return sign_size + magnitude_size;
}

} // namespace details

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires(::fast_io::details::
				 print_floating_exact_decimal_supported<flags, flt>)
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	io_reserve_type_t<char_type,
					  ::fast_io::manipulators::exact_decimal_manip_t<flags, flt>>,
	::fast_io::manipulators::exact_decimal_manip_t<flags, flt> const &value) noexcept
{
	(void)sizeof(char_type);
	using floating_type = ::std::remove_cvref_t<flt>;
	auto const fields{::fast_io::details::get_punned_result(value.reference)};
	return ::fast_io::details::floating_precise_exact_decimal_fields_size<
		flags, floating_type>(
		fields.mantissa, fields.exponent, fields.sign);
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires(::fast_io::details::
				 print_floating_exact_decimal_supported<flags, flt>)
inline constexpr char_type *print_reserve_precise_define(
	io_reserve_type_t<char_type,
					  ::fast_io::manipulators::exact_decimal_manip_t<flags, flt>>,
	char_type *iter, ::std::size_t precise_size,
	::fast_io::manipulators::exact_decimal_manip_t<flags, flt> const &value) noexcept
{
	(void)precise_size;
	using floating_type = ::std::remove_cvref_t<flt>;
	auto const fields{::fast_io::details::get_punned_result(value.reference)};
	return ::fast_io::details::print_floating_exact_decimal_fields_define<
		flags, floating_type>(
		iter, fields.mantissa, fields.exponent, fields.sign);
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_exact_decimal_supported<flags, flt> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	io_reserve_type_t<char_type,
					  ::fast_io::manipulators::exact_decimal_field_manip_t<flags, flt>>,
	::fast_io::manipulators::exact_decimal_field_manip_t<flags, flt> value) noexcept
{
	(void)sizeof(char_type);
	using floating_type = ::std::remove_cvref_t<flt>;
	auto const fields{
		::fast_io::details::floating_scalar_proxy_fields<floating_type>(
			value.representation)};
	return ::fast_io::details::floating_precise_exact_decimal_fields_size<
		flags, floating_type>(
		fields.mantissa, fields.exponent, fields.sign);
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_exact_decimal_supported<flags, flt> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr char_type *print_reserve_precise_define(
	io_reserve_type_t<char_type,
					  ::fast_io::manipulators::exact_decimal_field_manip_t<flags, flt>>,
	char_type *iter, ::std::size_t precise_size,
	::fast_io::manipulators::exact_decimal_field_manip_t<flags, flt> value) noexcept
{
	(void)precise_size;
	using floating_type = ::std::remove_cvref_t<flt>;
	auto const fields{
		::fast_io::details::floating_scalar_proxy_fields<floating_type>(
			value.representation)};
	return ::fast_io::details::print_floating_exact_decimal_fields_define<
		flags, floating_type>(
		iter, fields.mantissa, fields.exponent, fields.sign);
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires ::fast_io::details::floating_precise_scalar_supported<flags, flt>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, flt>>,
	::fast_io::details::floating_precise_parameter_t<
		::fast_io::manipulators::scalar_manip_t<flags, flt>, flt> value) noexcept
{
	static_assert(flags.floating == ::fast_io::manipulators::floating_format::general ||
				  flags.floating == ::fast_io::manipulators::floating_format::scientific ||
				  flags.floating == ::fast_io::manipulators::floating_format::fixed ||
				  flags.floating == ::fast_io::manipulators::floating_format::decimal ||
				  flags.floating == ::fast_io::manipulators::floating_format::hexfloat);
	(void)sizeof(char_type); // Encoding changes code-unit values, never this grammar's length.
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::hexfloat)
	{
		if constexpr (::fast_io::details::
			print_floating_requires_object_field_capture<flt>)
		{
			auto const fields{
				::fast_io::details::floating_precise_captured_fields<flt>(value)};
			return ::fast_io::details::compiler_constant_hex_scalar_fields_size<
				flags>(fields);
		}
		else
		{
			return ::fast_io::details::floating_precise_hex_shortest_size<
				flags.showbase, flags.showpos, flags.nan_show_sign,
				flags.nan_show_type>(value.reference);
		}
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
					   sizeof(flt) == sizeof(double))
	{
		// On the MSVC long-double ABI, both the ordinary writer and this counter
		// normalize to binary64.  The cast is exact because the representations
		// have identical precision and exponent ranges.
		return ::fast_io::details::floating_precise_shortest_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type, flags.floating,
			flags.rounding, flags.json_float>(static_cast<double>(value.reference));
	}
	else if constexpr (::fast_io::details::
		print_floating_requires_object_field_capture<flt>)
	{
		using floating_type = ::std::remove_cvref_t<flt>;
		auto const [mantissa, exponent, sign]{
			::fast_io::details::floating_precise_captured_fields<flt>(value)};
		return ::fast_io::details::floating_precise_shortest_fields_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type,
			flags.floating, flags.rounding, flags.json_float, floating_type>(
				mantissa, exponent, sign);
	}
	else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
	{
		using floating_type = ::std::remove_cvref_t<flt>;
		using trait = ::fast_io::details::iec559_traits<floating_type>;
		auto const [mantissa, exponent, sign]{
			::fast_io::details::get_punned_result(value.reference)};
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::floating_precise_special_size<
				flags.showpos, flags.nan_show_sign, flags.nan_show_type,
				trait::mbits>(mantissa, sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_narrow_float_from_fields<floating_type>(
				mantissa, exponent, sign)};
		return ::fast_io::details::floating_precise_shortest_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type, flags.floating,
			flags.rounding, flags.json_float>(widened);
	}
	else
	{
		static_assert(
			::fast_io::details::print_floating_decimal_direct_supported<flt> ||
				::fast_io::details::print_floating_decimal_exact_supported<flt>,
			"decimal shortest sizing requires a direct narrow domain or the strict exact wide domain");
		return ::fast_io::details::floating_precise_shortest_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type, flags.floating,
			flags.rounding, flags.json_float>(value.reference);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires ::fast_io::details::floating_precise_scalar_supported<flags, flt>
inline constexpr char_type *print_reserve_precise_define(
	io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, flt>> tag,
	char_type *iter, ::std::size_t precise_size,
	::fast_io::details::floating_precise_parameter_t<
		::fast_io::manipulators::scalar_manip_t<flags, flt>, flt> value) noexcept
{
	/*
	The measured length proves the logical cursor but does not grant storage after
	that cursor.  Ordinary reserve writers may exploit their advertised maximum
	capacity for a fixed-width ASCII staging store; a precise allocation may not.
	Decimal binary16/bfloat16/binary32/binary64 values therefore use the private
	exact-bounds renderer, which preserves the same punned fields, rounded carrier
	and presentation grammar while selecting only exact-store leaves.  The ABI
	normalization order below is intentionally identical to `print_reserve_define`:
	MSVC long double is binary64, a narrow exact type widens to binary32, and the
	x86 Clang bfloat16 workaround transports integer fields without a lossy native
	argument.  Hexadecimal scalar rendering already emits each returned code unit
	individually or through a fixed-width write wholly inside the final spelling,
	so it retains its ordinary semantic entry.  Wide decimal scalar shortest is
	not an advertised precise capability; wide runtime-precision formatting is a
	separate protocol audited below.
	*/
	(void)precise_size;
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		if constexpr (::fast_io::details::
			print_floating_requires_object_field_capture<flt>)
		{
			// Keep the AVX512-BF16 public CPO by value, but do not pass its
			// native scalar through a second by-value manipulator boundary.
			// Clang may quiet an sNaN while rematerializing that nested copy.
			// Integer fields captured from this register-backed owning object
			// preserve every payload bit and feed the same hex grammar directly.
			auto const fields{
				::fast_io::details::floating_precise_captured_fields<flt>(value)};
			return ::fast_io::details::
				compiler_constant_hex_scalar_fields_define<flags>(iter, fields);
		}
		else
		{
			return print_reserve_define(tag, iter, value);
		}
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
		sizeof(flt) == sizeof(double))
	{
		return ::fast_io::details::print_rsvflt_exact_bounds_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float>(
				iter, static_cast<double>(value.reference));
	}
	else if constexpr (::fast_io::details::
		print_floating_requires_object_field_capture<flt>)
	{
		using floating_type = ::std::remove_cvref_t<flt>;
		auto const [mantissa, exponent, sign]{
			::fast_io::details::floating_precise_captured_fields<flt>(value)};
		return ::fast_io::details::print_rsvflt_fields_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float, floating_type, true>(
				iter, mantissa, exponent, sign);
	}
	else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
	{
		using floating_type = ::std::remove_cvref_t<flt>;
		using trait = ::fast_io::details::iec559_traits<floating_type>;
		auto const [mantissa, exponent, sign]{
			::fast_io::details::get_punned_result(value.reference)};
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::prsv_fp_nan_impl<
				flags.showpos, flags.uppercase, flags.nan_show_sign,
				flags.nan_show_type, trait::mbits>(iter, mantissa, sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_narrow_float_from_fields<floating_type>(
				mantissa, exponent, sign)};
		return ::fast_io::details::print_rsvflt_exact_bounds_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float>(
			iter, widened);
	}
#if defined(__SIZEOF_INT128__)
	else if constexpr (
		::fast_io::details::fp_floating_point_is_ibm_double_double<flt>)
	{
		return ::fast_io::details::
			print_floating_ibm_double_double_scalar_define<flags>(
				iter, value.reference);
	}
	else if constexpr (
		::fast_io::details::print_floating_decimal_exact_supported<flt>)
	{
		using floating_type = ::std::remove_cvref_t<flt>;
		auto const [mantissa, exponent, negative]{
			::fast_io::details::get_punned_result(value.reference)};
		return ::fast_io::details::
			print_floating_wide_scalar_fields_define<flags, floating_type>(
				iter, mantissa, exponent, negative);
	}
	#endif
	else
	{
		static_assert(::fast_io::details::
			print_floating_decimal_direct_supported<flt>);
		return ::fast_io::details::print_rsvflt_exact_bounds_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float>(iter, value.reference);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires ::fast_io::details::floating_precise_precision_supported<flags, flt>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_precision_t<flags, flt>>,
	::fast_io::details::floating_precise_parameter_t<
		::fast_io::manipulators::scalar_manip_precision_t<flags, flt>, flt> value) noexcept
{
	static_assert(flags.floating == ::fast_io::manipulators::floating_format::general ||
				  flags.floating == ::fast_io::manipulators::floating_format::scientific ||
				  flags.floating == ::fast_io::manipulators::floating_format::fixed ||
				  flags.floating == ::fast_io::manipulators::floating_format::decimal ||
				  flags.floating == ::fast_io::manipulators::floating_format::hexfloat);
	(void)sizeof(char_type);
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::hexfloat)
	{
		static_assert(::fast_io::details::floating_precision_is_significant<flags.precision> ||
					  ::fast_io::details::floating_precision_is_fractional<flags.precision>);
		if constexpr (::fast_io::details::
			print_floating_requires_object_field_capture<flt>)
		{
			auto const fields{
				::fast_io::details::floating_precise_captured_fields<flt>(value)};
			return ::fast_io::details::
				compiler_constant_hex_precision_fields_runtime_size<flags>(
					fields, value.precision);
		}
		else
		{
			return ::fast_io::details::floating_precise_hex_precision_size<
				flags.showbase, flags.showpos, flags.nan_show_sign,
				flags.nan_show_type, flags.precision, flags.rounding>(
					value.reference, value.precision);
		}
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
					   sizeof(flt) == sizeof(double))
	{
		return ::fast_io::details::floating_precise_precision_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type, flags.floating,
			flags.precision, flags.rounding, flags.json_float>(
			static_cast<double>(value.reference), value.precision);
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<flt>, float>)
	{
		using trait = ::fast_io::details::iec559_traits<float>;
		auto const [mantissa, exponent, sign]{
			::fast_io::details::get_punned_result(value.reference)};
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::floating_precise_special_size<
				flags.showpos, flags.nan_show_sign, flags.nan_show_type,
				trait::mbits>(mantissa, sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_binary32_finite_fields_to_binary64(
				static_cast<::std::uint_least32_t>(mantissa), exponent, sign)};
		return ::fast_io::details::floating_precise_precision_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type,
			flags.floating, flags.precision, flags.rounding,
			flags.json_float>(widened, value.precision);
	}
	else if constexpr (::fast_io::details::
		print_floating_requires_object_field_capture<flt>)
	{
		using floating_type = ::std::remove_cvref_t<flt>;
		using trait = ::fast_io::details::iec559_traits<floating_type>;
		auto const [mantissa, exponent, sign]{
			::fast_io::details::floating_precise_captured_fields<flt>(value)};
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::floating_precise_special_size<
				flags.showpos, flags.nan_show_sign, flags.nan_show_type,
				trait::mbits>(mantissa, sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_narrow_float_from_fields<floating_type>(
				mantissa, exponent, sign)};
		return ::fast_io::details::floating_precise_precision_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type,
			flags.floating, flags.precision, flags.rounding,
			flags.json_float>(widened, value.precision);
	}
	else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
	{
		using floating_type = ::std::remove_cvref_t<flt>;
		using trait = ::fast_io::details::iec559_traits<floating_type>;
		auto const [mantissa, exponent, sign]{
			::fast_io::details::get_punned_result(value.reference)};
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::floating_precise_special_size<
				flags.showpos, flags.nan_show_sign, flags.nan_show_type,
				trait::mbits>(mantissa, sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_narrow_float_from_fields<floating_type>(
				mantissa, exponent, sign)};
		return ::fast_io::details::floating_precise_precision_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type, flags.floating,
			flags.precision, flags.rounding, flags.json_float>(
			widened, value.precision);
	}
	else
	{
		static_assert(
			::fast_io::details::print_floating_decimal_direct_supported<flt> ||
				::fast_io::details::print_floating_decimal_exact_supported<flt>,
			"decimal precision sizing requires a direct narrow domain or the strict exact wide domain");
		return ::fast_io::details::floating_precise_precision_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type, flags.floating,
			flags.precision, flags.rounding, flags.json_float>(value.reference, value.precision);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires ::fast_io::details::floating_precise_precision_supported<flags, flt>
inline constexpr char_type *print_reserve_precise_define(
	io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_precision_t<flags, flt>> tag,
	char_type *iter, ::std::size_t precise_size,
	::fast_io::details::floating_precise_parameter_t<
		::fast_io::manipulators::scalar_manip_precision_t<flags, flt>, flt> value) noexcept
{
	// The proof is identical to the scalar overload above.  Runtime precision
	// changes the rounded carrier and padding count, both of which are explicit
	// inputs to the independent length function; it does not create a second
	// emission grammar.
	(void)precise_size;
	if constexpr (::fast_io::details::
		print_floating_requires_object_field_capture<flt>)
	{
		using floating_type = ::std::remove_cvref_t<flt>;
		auto const fields{
			::fast_io::details::floating_precise_captured_fields<flt>(value)};
		if constexpr (flags.floating ==
			::fast_io::manipulators::floating_format::hexfloat)
		{
			return ::fast_io::details::
				compiler_constant_hex_precision_fields_runtime_define<flags>(
					iter, fields, value.precision);
		}
		else
		{
			using trait = ::fast_io::details::iec559_traits<floating_type>;
			constexpr auto exponent_mask{
				(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
			if (fields.exponent ==
				static_cast<::std::uint_least32_t>(exponent_mask))
			{
				return ::fast_io::details::prsv_fp_nan_impl<
					flags.showpos, flags.uppercase, flags.nan_show_sign,
					flags.nan_show_type, trait::mbits>(
						iter, fields.mantissa, fields.sign);
			}
			auto const widened{
				::fast_io::details::dragonbox_narrow_float_from_fields<
					floating_type>(fields.mantissa, fields.exponent, fields.sign)};
			return ::fast_io::details::print_rsvflt_precision_define_impl<
				flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
				flags.floating, flags.precision, flags.rounding,
				flags.nan_show_sign, flags.nan_show_type, flags.json_float>(
					iter, widened, value.precision);
		}
	}
	else
	{
		return print_reserve_define(tag, iter, value);
	}
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags,
	::fast_io::details::my_floating_point flt>
	requires(
		::fast_io::details::floating_precise_scalar_supported<flags, flt> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	io_reserve_type_t<char_type,
		::fast_io::manipulators::floating_scalar_field_manip_t<flags, flt>>,
	::fast_io::manipulators::floating_scalar_field_manip_t<flags, flt> value) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	auto const fields{
		::fast_io::details::floating_scalar_proxy_fields<floating_type>(
			value.representation)};
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		return ::fast_io::details::compiler_constant_hex_scalar_fields_size<
			flags>(fields);
	}
	else
	{
		return ::fast_io::details::floating_precise_shortest_fields_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type,
			flags.floating, flags.rounding, flags.json_float, floating_type>(
				fields.mantissa, fields.exponent, fields.sign);
	}
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags,
	::fast_io::details::my_floating_point flt>
	requires(
		::fast_io::details::floating_precise_scalar_supported<flags, flt> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr char_type *print_reserve_precise_define(
	io_reserve_type_t<char_type,
		::fast_io::manipulators::floating_scalar_field_manip_t<flags, flt>>,
	char_type *iter, ::std::size_t precise_size,
	::fast_io::manipulators::floating_scalar_field_manip_t<flags, flt> value) noexcept
{
	(void)precise_size;
	using floating_type = ::std::remove_cvref_t<flt>;
	auto const fields{
		::fast_io::details::floating_scalar_proxy_fields<floating_type>(
			value.representation)};
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		return ::fast_io::details::compiler_constant_hex_scalar_fields_define<
			flags>(iter, fields);
	}
	else
	{
		return ::fast_io::details::print_rsvflt_fields_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float, floating_type, true>(
				iter, fields.mantissa, fields.exponent, fields.sign);
	}
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags,
	::fast_io::details::my_floating_point flt>
	requires(
		::fast_io::details::floating_precise_precision_supported<flags, flt> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	io_reserve_type_t<char_type,
		::fast_io::manipulators::floating_scalar_field_manip_precision_t<
			flags, flt>>,
	::fast_io::manipulators::floating_scalar_field_manip_precision_t<
		flags, flt> value) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	auto const fields{
		::fast_io::details::floating_scalar_proxy_fields<floating_type>(
			value.representation)};
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		return ::fast_io::details::
			compiler_constant_hex_precision_fields_runtime_size<flags>(
				fields, value.precision);
	}
	else
	{
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		if (fields.exponent ==
			static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::floating_precise_special_size<
				flags.showpos, flags.nan_show_sign, flags.nan_show_type,
				trait::mbits>(fields.mantissa, fields.sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_narrow_float_from_fields<
				floating_type>(fields.mantissa, fields.exponent, fields.sign)};
		return ::fast_io::details::floating_precise_precision_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type,
			flags.floating, flags.precision, flags.rounding, flags.json_float>(
				widened, value.precision);
	}
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags,
	::fast_io::details::my_floating_point flt>
	requires(
		::fast_io::details::floating_precise_precision_supported<flags, flt> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr char_type *print_reserve_precise_define(
	io_reserve_type_t<char_type,
		::fast_io::manipulators::floating_scalar_field_manip_precision_t<
			flags, flt>>,
	char_type *iter, ::std::size_t precise_size,
	::fast_io::manipulators::floating_scalar_field_manip_precision_t<
		flags, flt> value) noexcept
{
	(void)precise_size;
	using floating_type = ::std::remove_cvref_t<flt>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	auto const fields{
		::fast_io::details::floating_scalar_proxy_fields<floating_type>(
			value.representation)};
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		return ::fast_io::details::
			compiler_constant_hex_precision_fields_runtime_define<flags>(
				iter, fields, value.precision);
	}
	else
	{
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		if (fields.exponent ==
			static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::prsv_fp_nan_impl<
				flags.showpos, flags.uppercase, flags.nan_show_sign,
				flags.nan_show_type, trait::mbits>(
					iter, fields.mantissa, fields.sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_narrow_float_from_fields<
				floating_type>(fields.mantissa, fields.exponent, fields.sign)};
		return ::fast_io::details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.precision, flags.rounding,
			flags.nan_show_sign, flags.nan_show_type, flags.json_float>(
				iter, widened, value.precision);
	}
}

namespace details
{

template <::fast_io::manipulators::scalar_flags flags, typename flt>
[[nodiscard]] inline constexpr ::std::size_t
floating_precise_narrow_range_fields_size(
	::fast_io::details::punning_result<::std::remove_cvref_t<flt>> fields,
	::std::size_t minimum_precision,
	::std::size_t maximum_precision) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	(void)::fast_io::details::normalize_floating_precision_range(
		minimum_precision, maximum_precision);
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
	if (fields.exponent == exponent_mask)
	{
		return ::fast_io::details::floating_precise_special_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type,
			trait::mbits>(fields.mantissa, fields.sign);
	}
	::fast_io::details::floating_precision_range_plan plan{};
	if (fields.mantissa == 0u && fields.exponent == 0u)
	{
		plan = ::fast_io::details::make_floating_precision_range_plan(
			1u, minimum_precision, maximum_precision);
	}
	else
	{
		auto const decimal{
			::fast_io::details::print_floating_shortest_decimal_fields<
				flags.rounding, floating_type>(
				fields.mantissa, fields.exponent, fields.sign)};
		plan = ::fast_io::details::make_floating_precision_range_plan(
			static_cast<::std::size_t>(
				::fast_io::details::chars_len<10u, true>(decimal.m10)),
			minimum_precision, maximum_precision);
		if (plan.shortest)
		{
			return ::fast_io::details::floating_precise_sign_size<flags.showpos>(
					   fields.sign) +
				   ::fast_io::details::floating_precise_decimal_layout_size<
					   floating_type, flags.floating, flags.json_float>(
					   decimal.m10, decimal.e10);
		}
	}
	if (plan.shortest)
	{
		return ::fast_io::details::floating_precise_shortest_fields_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type,
			flags.floating, flags.rounding, flags.json_float, floating_type>(
			fields.mantissa, fields.exponent, fields.sign);
	}
	auto const widened{
		::fast_io::details::dragonbox_narrow_float_from_fields<floating_type>(
			fields.mantissa, fields.exponent, fields.sign)};
	if (plan.preserve)
	{
		return ::fast_io::details::floating_precise_precision_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type,
			flags.floating,
			::fast_io::manipulators::floating_precision::
				significant_preserve_trailing_zero,
			flags.rounding, flags.json_float>(widened, plan.precision);
	}
	return ::fast_io::details::floating_precise_precision_size<
		flags.showpos, flags.nan_show_sign, flags.nan_show_type,
		flags.floating,
		::fast_io::manipulators::floating_precision::significant,
		flags.rounding, flags.json_float>(widened, plan.precision);
}

template <::fast_io::manipulators::scalar_flags flags, typename flt,
		  ::std::integral char_type>
inline constexpr char_type *floating_precise_narrow_range_fields_define(
	char_type *iter,
	::fast_io::details::punning_result<::std::remove_cvref_t<flt>> fields,
	::std::size_t minimum_precision,
	::std::size_t maximum_precision) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	(void)::fast_io::details::normalize_floating_precision_range(
		minimum_precision, maximum_precision);
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
	if (fields.exponent == exponent_mask)
	{
		return ::fast_io::details::prsv_fp_nan_impl<
			flags.showpos, flags.uppercase, flags.nan_show_sign,
			flags.nan_show_type, trait::mbits>(
			iter, fields.mantissa, fields.sign);
	}
	::fast_io::details::floating_precision_range_plan plan{};
	if (fields.mantissa == 0u && fields.exponent == 0u)
	{
		plan = ::fast_io::details::make_floating_precision_range_plan(
			1u, minimum_precision, maximum_precision);
	}
	else
	{
		auto const decimal{
			::fast_io::details::print_floating_shortest_decimal_fields<
				flags.rounding, floating_type>(
				fields.mantissa, fields.exponent, fields.sign)};
		plan = ::fast_io::details::make_floating_precision_range_plan(
			static_cast<::std::size_t>(
				::fast_io::details::chars_len<10u, true>(decimal.m10)),
			minimum_precision, maximum_precision);
		if (plan.shortest)
		{
			iter = ::fast_io::details::print_rsv_fp_sign_impl<flags.showpos>(
				iter, fields.sign);
			return ::fast_io::details::print_rsvflt_decimal_define_impl<
				floating_type, flags.comma, flags.uppercase_e, flags.floating,
				flags.json_float>(iter, decimal.m10, decimal.e10);
		}
	}
	if (plan.shortest)
	{
		return ::fast_io::details::print_rsvflt_fields_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float, floating_type, true>(
			iter, fields.mantissa, fields.exponent, fields.sign);
	}
	if constexpr (!::fast_io::details::is_ascii<char_type>)
	{
		if (fields.mantissa != 0u || fields.exponent != 0u)
		{
			if (plan.preserve)
			{
				return ::fast_io::details::
					print_floating_precision_range_non_ascii_exact<
						flags,
						::fast_io::manipulators::floating_precision::
							significant_preserve_trailing_zero,
						floating_type>(iter, fields.mantissa, fields.exponent,
									   plan.precision, fields.sign);
			}
			return ::fast_io::details::
				print_floating_precision_range_non_ascii_exact<
					flags,
					::fast_io::manipulators::floating_precision::significant,
					floating_type>(iter, fields.mantissa, fields.exponent,
								   plan.precision, fields.sign);
		}
	}
	auto const widened{
		::fast_io::details::dragonbox_narrow_float_from_fields<floating_type>(
			fields.mantissa, fields.exponent, fields.sign)};
	if (plan.preserve)
	{
		return ::fast_io::details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating,
			::fast_io::manipulators::floating_precision::
				significant_preserve_trailing_zero,
			flags.rounding, flags.nan_show_sign, flags.nan_show_type,
			flags.json_float>(iter, widened, plan.precision);
	}
	return ::fast_io::details::print_rsvflt_precision_define_impl<
		flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
		flags.floating,
		::fast_io::manipulators::floating_precision::significant,
		flags.rounding, flags.nan_show_sign, flags.nan_show_type,
		flags.json_float>(iter, widened, plan.precision);
}

} // namespace details

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires(
		::fast_io::details::floating_precise_range_supported<flags, flt> &&
		!::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	io_reserve_type_t<char_type,
					  ::fast_io::manipulators::floating_scalar_precision_range_manip_t<
						  flags, flt>>,
	::fast_io::details::floating_precise_range_parameter_t<
		::fast_io::manipulators::floating_scalar_precision_range_manip_t<
			flags, flt>,
		flt>
		value) noexcept
{
	(void)sizeof(char_type);
	using floating_type = ::std::remove_cvref_t<flt>;
	if constexpr (requires { value.fields; })
	{
		return ::fast_io::details::floating_precise_narrow_range_fields_size<
			flags, floating_type>(value.fields, value.minimum_precision,
								  value.maximum_precision);
	}
	else
	{
		using trait = ::fast_io::details::iec559_traits<floating_type>;
		(void)::fast_io::details::normalize_floating_precision_range(
			value.minimum_precision, value.maximum_precision);
		auto const fields{
			::fast_io::details::get_punned_result(value.reference)};
		constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) -
			1u)};
		if (fields.exponent == exponent_mask)
		{
			using scalar_type =
				::fast_io::manipulators::scalar_manip_t<flags, flt>;
			return ::fast_io::print_reserve_precise_size(
				::fast_io::io_reserve_type<char_type, scalar_type>,
				scalar_type{value.reference});
		}
		::fast_io::details::floating_precision_range_plan plan{};
		if (fields.mantissa == 0u && fields.exponent == 0u)
		{
			plan = ::fast_io::details::make_floating_precision_range_plan(
				1u, value.minimum_precision, value.maximum_precision);
		}
		else
		{
#if defined(__SIZEOF_INT128__)
			if constexpr (::fast_io::details::
							  fp_floating_point_is_ibm_double_double<floating_type>)
			{
				auto const decimal{::fast_io::details::
									   print_floating_ibm_double_double_shortest_decimal<
										   flags.rounding>(value.reference)};
				plan = ::fast_io::details::make_floating_precision_range_plan(
					static_cast<::std::size_t>(
						::fast_io::details::chars_len<10u, true>(decimal.m10)),
					value.minimum_precision, value.maximum_precision);
				if (plan.shortest)
				{
					return ::fast_io::details::floating_precise_sign_size<
							   flags.showpos>(fields.sign) +
						   ::fast_io::details::floating_precise_decimal_layout_size<
							   floating_type, flags.floating, flags.json_float>(
							   decimal.m10, decimal.e10);
				}
			}
			else
#endif
			{
				auto const decimal{::fast_io::details::
									   print_floating_shortest_decimal_fields<
										   flags.rounding, floating_type>(
										   fields.mantissa, fields.exponent, fields.sign)};
				plan = ::fast_io::details::make_floating_precision_range_plan(
					static_cast<::std::size_t>(
						::fast_io::details::chars_len<10u, true>(decimal.m10)),
					value.minimum_precision, value.maximum_precision);
				if (plan.shortest)
				{
					return ::fast_io::details::floating_precise_sign_size<
							   flags.showpos>(fields.sign) +
						   ::fast_io::details::floating_precise_decimal_layout_size<
							   floating_type, flags.floating, flags.json_float>(
							   decimal.m10, decimal.e10);
				}
			}
		}
		if (plan.shortest)
		{
			using scalar_type =
				::fast_io::manipulators::scalar_manip_t<flags, flt>;
			return ::fast_io::print_reserve_precise_size(
				::fast_io::io_reserve_type<char_type, scalar_type>,
				scalar_type{value.reference});
		}
		if (plan.preserve)
		{
			constexpr auto precision_flags{
				::fast_io::details::floating_precision_mani_flags_cache<
					flags, ::fast_io::manipulators::floating_precision::
							   significant_preserve_trailing_zero>};
			using precision_type =
				::fast_io::manipulators::scalar_manip_precision_t<
					precision_flags, flt>;
			return ::fast_io::print_reserve_precise_size(
				::fast_io::io_reserve_type<char_type, precision_type>,
				precision_type{value.reference, plan.precision});
		}
		constexpr auto precision_flags{
			::fast_io::details::floating_precision_mani_flags_cache<
				flags,
				::fast_io::manipulators::floating_precision::significant>};
		using precision_type =
			::fast_io::manipulators::scalar_manip_precision_t<
				precision_flags, flt>;
		return ::fast_io::print_reserve_precise_size(
			::fast_io::io_reserve_type<char_type, precision_type>,
			precision_type{value.reference, plan.precision});
	}
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires(
		::fast_io::details::floating_precise_range_supported<flags, flt> &&
		!::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr char_type *print_reserve_precise_define(
	io_reserve_type_t<char_type,
					  ::fast_io::manipulators::floating_scalar_precision_range_manip_t<
						  flags, flt>>,
	char_type *iter, ::std::size_t precise_size,
	::fast_io::details::floating_precise_range_parameter_t<
		::fast_io::manipulators::floating_scalar_precision_range_manip_t<
			flags, flt>,
		flt>
		value) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	if constexpr (!::fast_io::details::is_ascii<char_type> &&
				  !requires { value.fields; })
	{
		(void)precise_size;
		using range_type = ::fast_io::manipulators::
			floating_scalar_precision_range_manip_t<flags, flt>;
		return ::fast_io::print_reserve_define(
			::fast_io::io_reserve_type<char_type, range_type>, iter,
			range_type{value.reference, value.minimum_precision,
					   value.maximum_precision});
	}
	if constexpr (requires { value.fields; })
	{
		(void)precise_size;
		return ::fast_io::details::floating_precise_narrow_range_fields_define<
			flags, floating_type>(iter, value.fields, value.minimum_precision,
								  value.maximum_precision);
	}
	else
	{
		using trait = ::fast_io::details::iec559_traits<floating_type>;
		(void)::fast_io::details::normalize_floating_precision_range(
			value.minimum_precision, value.maximum_precision);
		auto const fields{
			::fast_io::details::get_punned_result(value.reference)};
		constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) -
			1u)};
		if (fields.exponent == exponent_mask)
		{
			using scalar_type =
				::fast_io::manipulators::scalar_manip_t<flags, flt>;
			return ::fast_io::print_reserve_precise_define(
				::fast_io::io_reserve_type<char_type, scalar_type>, iter,
				precise_size, scalar_type{value.reference});
		}
		::fast_io::details::floating_precision_range_plan plan{};
		if (fields.mantissa == 0u && fields.exponent == 0u)
		{
			plan = ::fast_io::details::make_floating_precision_range_plan(
				1u, value.minimum_precision, value.maximum_precision);
		}
		else
		{
#if defined(__SIZEOF_INT128__)
			if constexpr (::fast_io::details::
							  fp_floating_point_is_ibm_double_double<floating_type>)
			{
				auto const decimal{::fast_io::details::
									   print_floating_ibm_double_double_shortest_decimal<
										   flags.rounding>(value.reference)};
				plan = ::fast_io::details::make_floating_precision_range_plan(
					static_cast<::std::size_t>(
						::fast_io::details::chars_len<10u, true>(decimal.m10)),
					value.minimum_precision, value.maximum_precision);
				if (plan.shortest)
				{
					iter = ::fast_io::details::print_rsv_fp_sign_impl<
						flags.showpos>(iter, fields.sign);
					return ::fast_io::details::
						print_rsvflt_decimal_define_impl<
							floating_type, flags.comma, flags.uppercase_e,
							flags.floating, flags.json_float>(
							iter, decimal.m10, decimal.e10);
				}
			}
			else
#endif
			{
				auto const decimal{::fast_io::details::
									   print_floating_shortest_decimal_fields<
										   flags.rounding, floating_type>(
										   fields.mantissa, fields.exponent, fields.sign)};
				plan = ::fast_io::details::make_floating_precision_range_plan(
					static_cast<::std::size_t>(
						::fast_io::details::chars_len<10u, true>(decimal.m10)),
					value.minimum_precision, value.maximum_precision);
				if (plan.shortest)
				{
					iter = ::fast_io::details::print_rsv_fp_sign_impl<
						flags.showpos>(iter, fields.sign);
					return ::fast_io::details::
						print_rsvflt_decimal_define_impl<
							floating_type, flags.comma, flags.uppercase_e,
							flags.floating, flags.json_float>(
							iter, decimal.m10, decimal.e10);
				}
			}
		}
		if (plan.shortest)
		{
			using scalar_type =
				::fast_io::manipulators::scalar_manip_t<flags, flt>;
			return ::fast_io::print_reserve_precise_define(
				::fast_io::io_reserve_type<char_type, scalar_type>, iter,
				precise_size, scalar_type{value.reference});
		}
		if (plan.preserve)
		{
			constexpr auto precision_flags{
				::fast_io::details::floating_precision_mani_flags_cache<
					flags, ::fast_io::manipulators::floating_precision::
							   significant_preserve_trailing_zero>};
			using precision_type =
				::fast_io::manipulators::scalar_manip_precision_t<
					precision_flags, flt>;
			return ::fast_io::print_reserve_precise_define(
				::fast_io::io_reserve_type<char_type, precision_type>, iter,
				precise_size,
				precision_type{value.reference, plan.precision});
		}
		constexpr auto precision_flags{
			::fast_io::details::floating_precision_mani_flags_cache<
				flags,
				::fast_io::manipulators::floating_precision::significant>};
		using precision_type =
			::fast_io::manipulators::scalar_manip_precision_t<
				precision_flags, flt>;
		return ::fast_io::print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, precision_type>, iter,
			precise_size,
			precision_type{value.reference, plan.precision});
	}
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires(
		::fast_io::details::floating_precise_range_supported<flags, flt> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	io_reserve_type_t<char_type,
					  ::fast_io::manipulators::
						  floating_scalar_field_precision_range_manip_t<flags, flt>>,
	::fast_io::manipulators::floating_scalar_field_precision_range_manip_t<
		flags, flt>
		value) noexcept
{
	(void)sizeof(char_type);
	using floating_type = ::std::remove_cvref_t<flt>;
	auto const fields{
		::fast_io::details::floating_scalar_proxy_fields<floating_type>(
			value.representation)};
	return ::fast_io::details::floating_precise_narrow_range_fields_size<
		flags, floating_type>(fields, value.minimum_precision,
							  value.maximum_precision);
}

template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires(
		::fast_io::details::floating_precise_range_supported<flags, flt> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr char_type *print_reserve_precise_define(
	io_reserve_type_t<char_type,
					  ::fast_io::manipulators::
						  floating_scalar_field_precision_range_manip_t<flags, flt>>,
	char_type *iter, ::std::size_t precise_size,
	::fast_io::manipulators::floating_scalar_field_precision_range_manip_t<
		flags, flt>
		value) noexcept
{
	(void)precise_size;
	if constexpr (!::fast_io::details::is_ascii<char_type>)
	{
		using range_type = ::fast_io::manipulators::
			floating_scalar_field_precision_range_manip_t<flags, flt>;
		return ::fast_io::print_reserve_define(
			::fast_io::io_reserve_type<char_type, range_type>, iter, value);
	}
	using floating_type = ::std::remove_cvref_t<flt>;
	auto const fields{
		::fast_io::details::floating_scalar_proxy_fields<floating_type>(
			value.representation)};
	return ::fast_io::details::floating_precise_narrow_range_fields_define<
		flags, floating_type>(iter, fields, value.minimum_precision,
							  value.maximum_precision);
}

} // namespace fast_io

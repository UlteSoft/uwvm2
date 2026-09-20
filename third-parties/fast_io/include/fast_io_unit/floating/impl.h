#pragma once

#include "punning.h"
#include "hexfloat.h"
#include "compiler_constant_hex.h"
#include "decfloat.h"
#include "roundtrip.h"
#include "wide_shortest.h"

namespace fast_io
{

namespace details
{

template <typename T>
concept print_floating_has_iec559_traits = requires {
	typename ::fast_io::details::iec559_traits<::std::remove_cvref_t<T>>::mantissa_type;
};

template <typename T, bool = ::fast_io::details::print_floating_has_iec559_traits<T>>
struct print_floating_decimal_direct_supported_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct print_floating_decimal_direct_supported_impl<T, true>
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	inline static constexpr bool value{
		(trait::mbits <= ::fast_io::details::iec559_traits<float>::mbits &&
		 trait::ebits <= ::fast_io::details::iec559_traits<float>::ebits &&
		 sizeof(no_cvref_t) <= sizeof(float)) ||
		::std::same_as<no_cvref_t, double>
#ifdef __STDCPP_FLOAT32_T__
		|| ::std::same_as<no_cvref_t, _Float32>
#endif
#if defined(FAST_IO_HAS_FLOAT64_TYPE)
		|| ::std::same_as<no_cvref_t, _Float64>
#endif
	};
};

template <typename T>
inline constexpr bool print_floating_decimal_direct_supported{
	::fast_io::details::print_floating_decimal_direct_supported_impl<T>::value};

template <typename T, bool = ::fast_io::details::print_floating_has_iec559_traits<T>>
struct print_floating_decimal_via_float_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct print_floating_decimal_via_float_impl<T, true>
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	inline static constexpr bool value{
		!::fast_io::details::print_floating_decimal_direct_supported<no_cvref_t> &&
		trait::mbits <= ::fast_io::details::iec559_traits<float>::mbits &&
		trait::ebits <= ::fast_io::details::iec559_traits<float>::ebits};
};

template <typename T>
inline constexpr bool print_floating_decimal_via_float{
	::fast_io::details::print_floating_decimal_via_float_impl<T>::value};

/*
Binary80, binary128, and IBM double-double precision formatting is implemented by the exact
decimal-expansion backend rather than the binary32/binary64 Dragonbox cache.
Admit only representations for which get_punned_result and the exact backend
have matching field models:

* x87 binary80 has a 63-bit stored fraction and a 15-bit exponent.  Its object
  representation is decoded by the explicitly proved little-endian x87
  overload in punning.h.
* IEEE binary128 has a 112-bit stored fraction and a 15-bit exponent in a
  same-size unsigned carrier.  The same-size condition excludes IBM double-
  double long double from this *field-layout* case.
* IBM double-double is admitted by its separate representation predicate.  Its
  two binary64 components are reduced to one exact dyadic before reaching the
  decimal backend; adjacent-value logic never treats the synthetic carrier as
  an IEEE field.

This is a representation capability, not an ISA selection.  In particular it
does not make a wide type enter dragonbox_main, whose runtime power cache and
endpoint arithmetic are intentionally limited to binary32 and binary64.
*/
template <typename T, bool = ::fast_io::details::print_floating_has_iec559_traits<T>>
struct print_floating_decimal_exact_supported_impl
{
	inline static constexpr bool value{};
};

template <typename T>
struct print_floating_decimal_exact_supported_impl<T, true>
{
	using no_cvref_t = ::std::remove_cvref_t<T>;
	using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
	using mantissa_type = typename trait::mantissa_type;
	inline static constexpr bool has_uint128{
#if defined(__SIZEOF_INT128__) && __SIZEOF_INT128__ == 16
		true
#else
		false
#endif
	};
	inline static constexpr bool binary80{
		has_uint128 &&
		::fast_io::details::fp_floating_point_is_float80<no_cvref_t> &&
		::std::endian::native == ::std::endian::little && trait::mbits == 63u &&
		trait::ebits == 15u &&
		sizeof(mantissa_type) == sizeof(::std::uint_least64_t)};
	inline static constexpr bool binary128{
		has_uint128 && trait::mbits == 112u && trait::ebits == 15u &&
		sizeof(no_cvref_t) == sizeof(mantissa_type) &&
		::std::numeric_limits<mantissa_type>::digits >= 128u};
	inline static constexpr bool ibm_double_double{
		has_uint128 && ::fast_io::details::fp_floating_point_is_ibm_double_double<no_cvref_t>};
	inline static constexpr bool value{binary80 || binary128 || ibm_double_double};
};

template <typename T>
inline constexpr bool print_floating_decimal_exact_supported{
	::fast_io::details::print_floating_decimal_exact_supported_impl<T>::value};

// Clang's x86 bfloat16 lowering can rematerialize a nested scalar copy through
// __truncsfbf2 (or a narrowing instruction) and thereby destroy subnormal and
// NaN payload bits.  The upper scalar CPO therefore captures integer IEC 60559
// fields from its own manipulator object before entering a lower formatter.
// With AVX512-BF16 the owning aggregate itself has a bit-exact by-value ABI, so
// only this local field capture is needed.  Without it, even the owning CPO
// parameter must be borrowed.  Every ordinary low-level floating entry remains
// by value.  The capability macro prevents merely naming __bf16 on targets
// where Clang rejects the type.
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__) && \
	(defined(__x86_64__) || defined(_M_X64)) &&                       \
	!(defined(__arm64ec__) || defined(_M_ARM64EC))
template <typename T>
inline constexpr bool print_floating_requires_object_field_capture{
	::std::same_as<::std::remove_cvref_t<T>, __bf16>};

template <typename T>
inline constexpr bool print_floating_decimal_requires_integer_transport{
	::fast_io::details::print_floating_requires_object_field_capture<T>
#if defined(__AVX512BF16__)
	&& false
#endif
};
#else
template <typename T>
inline constexpr bool print_floating_requires_object_field_capture{};

template <typename T>
inline constexpr bool print_floating_decimal_requires_integer_transport{};
#endif

/*
The representation-sensitive x86 Clang bfloat16 domain must become integer
fields before an owning manipulator crosses a second formatter boundary.
Without AVX512-BF16 the public scalar CPO remains borrowed. With AVX512-BF16
the public CPO receives this compact owning carrier by value: its converting
constructor reads the caller's object bits directly, while the lower formatter
never rematerializes a signaling NaN through __truncsfbf2/VCVTNEPS2BF16.
Every other floating domain keeps its native by-value manipulator ABI.
*/
template <typename manipulator, typename flt>
struct floating_precise_field_parameter
{
	::fast_io::details::punning_result<::std::remove_cvref_t<flt>> fields{};
	::std::size_t precision{};

	inline constexpr floating_precise_field_parameter(manipulator const &value) noexcept
		: fields{::fast_io::details::compiler_constant_floating_capture_fields<
			  ::std::remove_cvref_t<flt>>(value.reference)}
	{
		if constexpr (requires { value.precision; })
		{
			precision = value.precision;
		}
	}
};

/* A precision-range precise query needs the captured floating fields and both
interval endpoints.  Keep this carrier separate from the existing precision
carrier so no ordinary scalar/precision ABI or object layout changes. */
template <typename manipulator, typename flt>
struct floating_precise_range_field_parameter
{
	::fast_io::details::punning_result<::std::remove_cvref_t<flt>> fields{};
	::std::size_t minimum_precision{};
	::std::size_t maximum_precision{};

	inline constexpr floating_precise_range_field_parameter(manipulator const &value) noexcept
		: fields{::fast_io::details::compiler_constant_floating_capture_fields<
			  ::std::remove_cvref_t<flt>>(value.reference)},
		  minimum_precision{value.minimum_precision},
		  maximum_precision{value.maximum_precision}
	{}
};

template <typename manipulator, typename flt>
using floating_value_or_field_parameter_t = ::std::conditional_t<
	::fast_io::details::print_floating_requires_object_field_capture<flt>,
	::fast_io::details::floating_precise_field_parameter<manipulator, flt>,
	manipulator>;

/// Decodes the integer-owned narrow floating proxy without ever reconstructing
/// a native bfloat16 object.  The representation layout is the same IEC 60559
/// layout already described by `iec559_traits`.
template <typename floating_type>
[[nodiscard]] inline constexpr auto
floating_scalar_proxy_fields(::std::uint_least16_t representation) noexcept
{
	using clean_type = ::std::remove_cvref_t<floating_type>;
	using trait = ::fast_io::details::iec559_traits<clean_type>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto mantissa_mask{
		(static_cast<::std::uint_least16_t>(1u) << trait::mbits) - 1u};
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least16_t>(1u) << trait::ebits) - 1u};
	return ::fast_io::details::punning_result<clean_type>{
		static_cast<mantissa_type>(representation & mantissa_mask),
		static_cast<::std::uint_least32_t>(
			(representation >> trait::mbits) & exponent_mask),
		static_cast<bool>(representation >> (trait::mbits + trait::ebits))};
}

template <::fast_io::manipulators::floating_format format>
inline constexpr bool print_floating_format_valid{
	format == ::fast_io::manipulators::floating_format::general ||
	format == ::fast_io::manipulators::floating_format::scientific ||
	format == ::fast_io::manipulators::floating_format::fixed ||
	format == ::fast_io::manipulators::floating_format::decimal ||
	format == ::fast_io::manipulators::floating_format::hexfloat};

template <::fast_io::manipulators::floating_precision precision>
inline constexpr bool print_floating_precision_valid{
	precision == ::fast_io::manipulators::floating_precision::significant ||
	precision == ::fast_io::manipulators::floating_precision::fractional ||
	precision == ::fast_io::manipulators::floating_precision::significant_preserve_trailing_zero ||
	precision == ::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero ||
	precision == ::fast_io::manipulators::floating_precision::charconv_significant ||
	precision == ::fast_io::manipulators::floating_precision::charconv_hex_fractional};

template <::fast_io::manipulators::floating_rounding rounding>
inline constexpr bool print_floating_rounding_valid{
	rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_to_odd ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_plus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_minus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_toward_zero ||
	rounding == ::fast_io::manipulators::floating_rounding::nearest_away_from_zero ||
	rounding == ::fast_io::manipulators::floating_rounding::toward_plus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::toward_minus_infinity ||
	rounding == ::fast_io::manipulators::floating_rounding::toward_zero ||
	rounding == ::fast_io::manipulators::floating_rounding::away_from_zero ||
	rounding == ::fast_io::manipulators::floating_rounding::current_environment};

/*
Reserve customization is a public capability query, so unsupported floating
representations and forged enum values must be removed by constraints rather
than diagnosed from a function-body static_assert.  Hexadecimal formatting
requires a punned IEC 60559 field decomposition.  Scalar decimal formatting
further requires the exact binary64 ABI normalization, an exact binary32
widening, or a directly implemented binary32/binary64 shortest domain.
*/
template <::fast_io::manipulators::scalar_flags flags, typename flt>
inline constexpr bool print_floating_ordinary_supported{
	flags.base == 10u &&
	::fast_io::details::print_floating_format_valid<flags.floating> &&
	::fast_io::details::print_floating_rounding_valid<flags.rounding> &&
	::fast_io::details::print_floating_has_iec559_traits<flt> &&
	::fast_io::details::scan_decfloat_layout_supported<flt> &&
	(flags.floating == ::fast_io::manipulators::floating_format::hexfloat ||
	 (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
	  sizeof(::std::remove_cvref_t<flt>) == sizeof(double)) ||
	 ::fast_io::details::print_floating_decimal_via_float<flt> ||
	 ::fast_io::details::print_floating_decimal_direct_supported<flt> ||
	 ::fast_io::details::print_floating_decimal_exact_supported<flt>)};

/*
Scalar decimal formatting promises the shortest round-tripping spelling, while
runtime precision promises rounding on an explicitly requested decimal grid.
The binary80/binary128 scalar branch uses the exact-interval carrier in
wide_shortest.h; it never enters the binary32/binary64 Dragonbox cache.  Keeping
that algorithm boundary explicit lets the public scalar and precision CPOs
advertise the same representation set without conflating their conversion
engines.
*/
template <::fast_io::manipulators::scalar_flags flags, typename flt>
inline constexpr bool print_floating_scalar_supported{
	::fast_io::details::print_floating_ordinary_supported<flags, flt>};

template <::fast_io::manipulators::scalar_flags flags, typename flt>
inline constexpr bool print_floating_precision_supported{
	flags.base == 10u &&
	::fast_io::details::print_floating_format_valid<flags.floating> &&
	::fast_io::details::print_floating_rounding_valid<flags.rounding> &&
	::fast_io::details::print_floating_has_iec559_traits<flt> &&
	::fast_io::details::scan_decfloat_layout_supported<flt> &&
	(flags.floating == ::fast_io::manipulators::floating_format::hexfloat ||
	 ::fast_io::details::print_floating_ordinary_supported<flags, flt> ||
	 ::fast_io::details::print_floating_decimal_exact_supported<flt>)};

/* Exact decimal consumes the source representation directly, including the
binary16 and bfloat16 fields that ordinary shortest output may widen to
binary32.  It deliberately does not inspect flags.rounding. */
template <::fast_io::manipulators::scalar_flags flags, typename flt>
inline constexpr bool print_floating_exact_decimal_supported{
	flags.base == 10u &&
	flags.floating != ::fast_io::manipulators::floating_format::hexfloat &&
	::fast_io::details::print_floating_format_valid<flags.floating> &&
	::fast_io::details::print_floating_has_iec559_traits<flt> &&
	::fast_io::details::scan_decfloat_layout_supported<flt>};


template <::fast_io::manipulators::scalar_flags flags, typename flt>
concept print_floating_staged_supported =
	::fast_io::details::my_floating_point<flt> &&
	(::std::same_as<::std::remove_cvref_t<flt>, float> ||
	 ::std::same_as<::std::remove_cvref_t<flt>, double>) &&
	::fast_io::details::da::staged_supported<::std::remove_cvref_t<flt>> &&
	::fast_io::details::print_floating_ordinary_supported<flags, flt> &&
	flags.floating != ::fast_io::manipulators::floating_format::hexfloat &&
	flags.rounding == ::fast_io::manipulators::floating_rounding::nearest_to_even;

template <::fast_io::manipulators::scalar_flags flags, typename flt,
		  ::std::integral char_type>
	requires(::fast_io::details::print_floating_exact_decimal_supported<flags, flt>)
inline constexpr char_type *print_floating_exact_decimal_fields_define(
	char_type *iter,
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u)};
	if (exponent == exponent_mask)
	{
		return ::fast_io::details::prsv_fp_nan_impl<
			flags.showpos, flags.uppercase, flags.nan_show_sign,
			flags.nan_show_type, trait::mbits>(iter, mantissa, negative);
	}
	iter = ::fast_io::details::print_rsv_fp_sign_impl<flags.showpos>(
		iter, negative);
	if (mantissa == 0u && exponent == 0u)
	{
		if constexpr (flags.floating ==
					  ::fast_io::manipulators::floating_format::scientific)
		{
			return ::fast_io::details::prsv_fp_dece0<flags.uppercase>(iter);
		}
		*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
		if constexpr (flags.json_float)
		{
			return ::fast_io::details::
				print_rsv_fp_append_json_float_zero<flags.comma>(iter);
		}
		return iter;
	}
	auto const decimal{
		::fast_io::details::exact_decimal_from_binary<flt>(mantissa, exponent)};
	/* The materializer has already removed decimal trailing zeroes.  Supplying
	its complete coefficient as the significant precision selects only the
	common layout writer; no rounding routine is entered. */
	return ::fast_io::details::print_rsvflt_rounded_precision_define_impl<
		flt, flags.comma, flags.uppercase_e, flags.floating,
		::fast_io::manipulators::floating_precision::significant,
		flags.json_float>(iter, decimal, decimal.size, decimal.size);
}

template <::fast_io::manipulators::floating_rounding rounding, typename flt>
/* current_environment is sampled by the owning range dispatcher before
this carrier-producing helper is entered. */
	requires(rounding !=
			 ::fast_io::manipulators::floating_rounding::current_environment)
[[nodiscard]] inline constexpr auto print_floating_shortest_decimal_fields(
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
#if defined(__SIZEOF_INT128__)
	if constexpr (::fast_io::details::
					  print_floating_decimal_exact_supported<flt> &&
				  !::fast_io::details::fp_floating_point_is_ibm_double_double<flt>)
	{
		auto const decimal{
			::fast_io::details::wide_shortest_from_binary<flt, rounding>(
				mantissa, exponent, negative)};
		if (!decimal.success)
		{
			::fast_io::fast_terminate();
		}
		return decimal;
	}
	else
#endif
	{
		if constexpr (::fast_io::details::
						  dragonbox_uses_binary32_core<flt> &&
					  sizeof(flt) < sizeof(float))
		{
			return ::fast_io::details::
				dragonbox_impl_narrow_hybrid<flt, rounding>(
					mantissa, static_cast<::std::int_least32_t>(exponent),
					negative);
		}
		else
		{
			return ::fast_io::details::dragonbox_impl<flt, rounding>(
				mantissa, static_cast<::std::int_least32_t>(exponent),
				negative);
		}
	}
}

#if defined(__SIZEOF_INT128__)
template <::fast_io::manipulators::floating_rounding rounding, typename flt>
	requires(::fast_io::details::fp_floating_point_is_ibm_double_double<flt>)
[[nodiscard]] inline constexpr auto
print_floating_ibm_double_double_shortest_decimal(flt value) noexcept
{
	static_assert(rounding !=
				  ::fast_io::manipulators::floating_rounding::current_environment);
	auto const decimal{
		::fast_io::details::wide_shortest_from_ibm_double_double<rounding>(
			value)};
	if (!decimal.success)
	{
		::fast_io::fast_terminate();
	}
	return decimal;
}

#endif

#if defined(__SIZEOF_INT128__)
/*
Binary80/binary128 scalar decimal output is deliberately kept outside
print_rsvflt_fields_define_impl: that established entry owns the binary32/64
Dragonbox and DA hot paths.  This representation-gated adapter performs the
same special/sign/zero grammar, obtains the independently proved exact-interval
shortest carrier, and then rejoins the common decimal presentation writer.
Thus enabling the public wide CPO cannot perturb any narrow dynamic path.
*/
template <::fast_io::manipulators::scalar_flags flags, typename flt,
		  ::std::integral char_type>
	requires(::fast_io::details::print_floating_decimal_exact_supported<flt> &&
			 flags.floating !=
				 ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr char_type *print_floating_wide_scalar_fields_define(
	char_type *iter,
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
	if constexpr (flags.rounding ==
				  ::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return ::fast_io::details::print_floating_wide_scalar_fields_define<
				::fast_io::details::floating_rounding_mani_flags_cache<
					flags, ::fast_io::manipulators::floating_rounding::
							   toward_plus_infinity>,
				flt>(
				iter, mantissa, exponent, negative);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return ::fast_io::details::print_floating_wide_scalar_fields_define<
				::fast_io::details::floating_rounding_mani_flags_cache<
					flags, ::fast_io::manipulators::floating_rounding::
							   toward_minus_infinity>,
				flt>(
				iter, mantissa, exponent, negative);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return ::fast_io::details::print_floating_wide_scalar_fields_define<
				::fast_io::details::floating_rounding_mani_flags_cache<
					flags, ::fast_io::manipulators::floating_rounding::toward_zero>,
				flt>(iter, mantissa, exponent, negative);
		default:
			return ::fast_io::details::print_floating_wide_scalar_fields_define<
				::fast_io::details::floating_rounding_mani_flags_cache<
					flags, ::fast_io::manipulators::floating_rounding::nearest_to_even>,
				flt>(iter, mantissa, exponent, negative);
		}
	}
	else
	{
		using trait = ::fast_io::details::iec559_traits<flt>;
		using mantissa_type = typename trait::mantissa_type;
		constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
			(static_cast<mantissa_type>(1u) << trait::ebits) - 1u)};
		if (exponent == exponent_mask)
		{
			return ::fast_io::details::prsv_fp_nan_impl<
				flags.showpos, flags.uppercase, flags.nan_show_sign,
				flags.nan_show_type, trait::mbits>(
				iter, mantissa, negative);
		}
		iter = ::fast_io::details::print_rsv_fp_sign_impl<flags.showpos>(
			iter, negative);
		if (mantissa == 0u && exponent == 0u)
		{
			if constexpr (flags.floating ==
						  ::fast_io::manipulators::floating_format::scientific)
			{
				return ::fast_io::details::prsv_fp_dece0<flags.uppercase>(iter);
			}
			*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
			if constexpr (flags.json_float)
			{
				return ::fast_io::details::
					print_rsv_fp_append_json_float_zero<flags.comma>(iter);
			}
			return iter;
		}
		auto const decimal{
			::fast_io::details::wide_shortest_from_binary<flt, flags.rounding>(
				mantissa, exponent, negative)};
		if (!decimal.success)
		{
			::fast_io::fast_terminate();
		}
		return ::fast_io::details::print_rsvflt_decimal_define_impl<
			flt, flags.comma, flags.uppercase_e, flags.floating,
			flags.json_float>(iter, decimal.m10, decimal.e10);
	}
}

/*
IBM double-double rejoins the common decimal writer only after constructing
its interval from real ABI neighbors.  Special values still come from the
leading binary64 field, while the finite carrier is the exact sum of both
components.  Separating this adapter from the IEEE field entry is the proof
that enabling the format cannot reinterpret its second component as exponent
or payload bits.
*/
template <::fast_io::manipulators::scalar_flags flags,
		  ::std::integral char_type, typename flt>
	requires(::fast_io::details::fp_floating_point_is_ibm_double_double<flt> &&
			 flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr char_type *print_floating_ibm_double_double_scalar_define(
	char_type *iter, flt value) noexcept
{
	if constexpr (flags.rounding ==
				  ::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return ::fast_io::details::print_floating_ibm_double_double_scalar_define<
				::fast_io::details::floating_rounding_mani_flags_cache<flags,
																	   ::fast_io::manipulators::floating_rounding::toward_plus_infinity>>(
				iter, value);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return ::fast_io::details::print_floating_ibm_double_double_scalar_define<
				::fast_io::details::floating_rounding_mani_flags_cache<flags,
																	   ::fast_io::manipulators::floating_rounding::toward_minus_infinity>>(
				iter, value);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return ::fast_io::details::print_floating_ibm_double_double_scalar_define<
				::fast_io::details::floating_rounding_mani_flags_cache<flags,
																	   ::fast_io::manipulators::floating_rounding::toward_zero>>(
				iter, value);
		default:
			return ::fast_io::details::print_floating_ibm_double_double_scalar_define<
				::fast_io::details::floating_rounding_mani_flags_cache<flags,
																	   ::fast_io::manipulators::floating_rounding::nearest_to_even>>(
				iter, value);
		}
	}
	else
	{
		using floating_type = ::std::remove_cvref_t<flt>;
		using trait = ::fast_io::details::iec559_traits<floating_type>;
		auto const [mantissa, exponent, negative]{
			::fast_io::details::get_punned_result(value)};
		constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
		if (exponent == exponent_mask)
		{
			return ::fast_io::details::prsv_fp_nan_impl<
				flags.showpos, flags.uppercase, flags.nan_show_sign,
				flags.nan_show_type, trait::mbits>(iter, mantissa, negative);
		}
		iter = ::fast_io::details::print_rsv_fp_sign_impl<flags.showpos>(
			iter, negative);
		if (!mantissa && !exponent)
		{
			if constexpr (flags.floating ==
						  ::fast_io::manipulators::floating_format::scientific)
			{
				return ::fast_io::details::prsv_fp_dece0<flags.uppercase>(iter);
			}
			*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
			if constexpr (flags.json_float)
			{
				return ::fast_io::details::
					print_rsv_fp_append_json_float_zero<flags.comma>(iter);
			}
			return iter;
		}
		auto const decimal{
			::fast_io::details::wide_shortest_from_ibm_double_double<flags.rounding>(
				static_cast<long double>(value))};
		if (!decimal.success)
		{
			::fast_io::fast_terminate();
		}
		return ::fast_io::details::print_rsvflt_decimal_define_impl<
			floating_type, flags.comma, flags.uppercase_e, flags.floating,
			flags.json_float>(iter, decimal.m10, decimal.e10);
	}
}
#endif

} // namespace details

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_staged_supported<flags, flt>
inline constexpr auto print_staged_type(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	return ::fast_io::io_type_t<::fast_io::details::da::staged_conversion_result<floating_type>>{};
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_staged_supported<flags, flt>
inline constexpr ::std::size_t print_staged_width(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>) noexcept
{
	return ::fast_io::details::da::staged_width<::std::remove_cvref_t<flt>>();
}

/// @brief Caps floating conversion staging at the measured scheduler/register envelope.
/// @details `staged_printable` proves that independent values may be prepared before emission; it does not prove that
///          an arbitrarily long prepared-state array is profitable.  Let `L(T)` be the live state needed after the
///          cached-power product for one value of type `T`.  Before the first emission, a width-W schedule necessarily
///          keeps at least W*L(T) live.  Binary64 retains a 64-bit significand, a decimal exponent, an optional final
///          digit, and the renderer's layout state; its pressure is therefore strictly greater than binary32's.  The
///          complete Apple-M prepare-all/emit-all measurements reach their optimum at W=8 for binary32 and W=6 for
///          binary64.  Admitting binary64 widths seven or eight cannot expose a new multiply chain after all six
///          profitable chains already cover the measured UMULH latency, but it can create spills before serialization.
///
///          This branch is a scheduling theorem, not a numeric one: every admitted width computes the same independent
///          DA carrier and emits the same bytes in source order.  Values beyond the cap retain the ordinary bounded
///          formatter, so changing the cap can affect only instruction scheduling and frame size.  The type-dependent
///          expression is constant-evaluated; the binary32 and binary64 machine functions contain no dynamic test.
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_staged_supported<flags, flt>
inline constexpr ::std::size_t print_staged_max_count(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	return ::std::same_as<floating_type, float> ? 8u : 6u;
}

/// @brief Selects the audited staged fallback placement for one floating formatter type.
/// @details The in-caller path is limited to ASCII `char` shortest-decimal format because that is the complete caller
///          measured by the target policy. Wider characters, EBCDIC execution character sets, and other presentation
///          formats retain the conservative cold fallback until separately audited. This customization changes only
///          placement of the ordinary scalar formatter after eligibility fails; it cannot change emitted characters.
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_staged_supported<flags, flt>
inline constexpr bool print_staged_fallback_inline(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>) noexcept
{
	return ::std::same_as<char_type, char> && ::fast_io::details::is_ascii<char_type> &&
		   flags.floating == ::fast_io::manipulators::floating_format::decimal &&
		   ::fast_io::details::da::staged_inline_fallback_supported<::std::remove_cvref_t<flt>>;
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_staged_supported<flags, flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool print_staged_eligible(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>,
	manipulators::scalar_manip_t<flags, flt> const &value) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr auto exponent_mask{
		(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
	auto [mantissa, exponent, sign]{
		::fast_io::details::get_punned_result(static_cast<floating_type>(value.reference))};
	(void)sign;
	return ::fast_io::details::da::staged_eligible<floating_type>(
		mantissa, exponent, exponent_mask);
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_staged_supported<flags, flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::fast_io::details::da::staged_conversion_result<::std::remove_cvref_t<flt>>
print_staged_prepare(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>,
	manipulators::scalar_manip_t<flags, flt> const &value) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	auto const [mantissa, exponent, sign]{
		::fast_io::details::get_punned_result(static_cast<floating_type>(value.reference))};
	(void)sign;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	auto const significand{static_cast<::std::uint_least64_t>(mantissa) |
						   (static_cast<::std::uint_least64_t>(1u) << trait::mbits)};
	::fast_io::details::da::conversion_result converted;
	if constexpr (sizeof(floating_type) <= sizeof(float))
	{
		converted = ::fast_io::details::da::compute_binary32_staged(
			static_cast<::std::uint_least32_t>(significand),
			static_cast<::std::uint_least32_t>(exponent));
	}
	else
	{
		converted = ::fast_io::details::da::compute_binary64(
			significand, static_cast<::std::uint_least32_t>(exponent));
	}
	if constexpr (::fast_io::details::da::staged_prepares_sign<floating_type>)
	{
		return {converted.significand, converted.exponent, converted.last_digit,
				converted.has_last_digit, static_cast<bool>(sign)};
	}
	else
	{
		return converted;
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_staged_supported<flags, flt>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_staged_define(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>, char_type *iter,
	manipulators::scalar_manip_t<flags, flt> const &value,
	::fast_io::details::da::staged_conversion_result<::std::remove_cvref_t<flt>> const &prepared) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	::fast_io::details::da::conversion_result const converted{
		prepared.significand, prepared.exponent, prepared.last_digit, prepared.has_last_digit};
	bool negative;
	if constexpr (::fast_io::details::da::staged_prepares_sign<floating_type>)
	{
		(void)value;
		negative = prepared.negative;
	}
	else
	{
		auto const [mantissa, exponent, sign]{
			::fast_io::details::get_punned_result(static_cast<floating_type>(value.reference))};
		(void)mantissa;
		(void)exponent;
		negative = sign;
	}
	if constexpr (::std::same_as<char_type, char> && ::fast_io::details::is_ascii<char_type>)
	{
		if (!::std::is_constant_evaluated())
		{
			if constexpr (flags.showpos)
			{
				*iter = static_cast<char>(negative ? u8'-' : u8'+');
				++iter;
			}
			else
			{
				*iter = static_cast<char>(u8'-');
				iter += static_cast<::std::size_t>(negative);
			}
			auto const result{
				::fast_io::details::da::print_ascii_shortest<floating_type, flags, true>(iter, converted)};
			if (result != nullptr)
			{
				return result;
			}
			auto const finalized{::fast_io::details::da::trim_trailing_zeros(
				::fast_io::details::da::finalize<floating_type>(converted))};
			return ::fast_io::details::print_rsvflt_decimal_define_impl<
				floating_type, flags.comma, flags.uppercase_e, flags.floating, flags.json_float>(
				iter, finalized.m10, finalized.e10);
		}
	}
	iter = ::fast_io::details::print_rsv_fp_sign_impl<flags.showpos>(iter, negative);
	auto const finalized{::fast_io::details::da::trim_trailing_zeros(
		::fast_io::details::da::finalize<floating_type>(converted))};
	return ::fast_io::details::print_rsvflt_decimal_define_impl<
		floating_type, flags.comma, flags.uppercase_e, flags.floating, flags.json_float>(
		iter, finalized.m10, finalized.e10);
}

/*
Specialized contiguous binary32/binary64 nearest-even batch
===========================================================

This is the primary float_DA schedule, not a second conversion algorithm.
For each regular normal source x_i, `compute_binary32_staged` or
`compute_binary64` returns the same Żmij/DA interval carrier C_i used by the
scalar formatter.  The carrier is a deterministic function of x_i's integer
IEC 60559 fields and reads no neighboring source.  Hence, for a group of W
values,

	C_0 = prepare(x_0), ..., C_(W-1) = prepare(x_(W-1))

are mutually independent.  Emission E(C_i, sign(x_i)) is ordered and writes
only at the cursor returned by E(C_(i-1), sign(x_(i-1))).  Moving every prepare
before every emit therefore preserves the dependency partial order:

	prepare_0 ... prepare_(W-1); emit_0 ... emit_(W-1)

is a topological reordering of the scalar schedule and emits identical bytes.
On Apple M, W=7 for binary32 and W=6 for binary64 expose enough independent
UMULH chains to cover multiply latency without spilling the larger binary64
state.  Binary32 originally preferred eight while its carrier occupied 24
bytes; compressing that carrier to 16 bytes moved a fresh W=5..8 Apple optimum
to seven, while the eighth lane again increased register pressure.  On the
measured x86-64 P-core, GCC16 instead prefers W=6 and Clang23 W=8; this is a
compiler scheduling boundary, not a numerical one.  Every width is a
compile-time constant, so neither type selection nor width selection exists in
the generated loop.

The raw contiguous source is intentional.  Reconstructing a generic print
proxy and calling the generic staged CPO for classification, preparation, and
emission in every lane was measured to lose approximately one nanosecond per
value on M4.  Keeping the field decode and the DA leaves here lets the compiler
hoist cache bases, interleave independent high products, and then schedule the
ASCII renderer as one batch kernel.
*/
template <::fast_io::details::my_floating_point flt>
[[nodiscard]] inline consteval ::std::size_t
print_contiguous_staged_range_optimal_extent() noexcept
{
	using trait = ::fast_io::details::iec559_traits<
		::std::remove_cvref_t<flt>>;
	if constexpr (trait::mbits == 52u)
	{
		// The larger binary64 carrier reaches the measured pressure limit at six.
		return 6u;
	}
	else
	{
		/*
		All returned values are schedules of the same seven mathematical fields;
		the preprocessor chooses only an audited compiler/ISA register envelope.
		AArch64 uses the physical-M4 W=7 result.  x86 Clang 23 benefits from eight
		independent chains, while x86 GCC16 spills at seven and uses six.  Other
		compilers inherit the smaller code/state choice until measured.
		*/
#if (defined(__aarch64__) || defined(__arm64__)) && !defined(__AARCH64EB__)
		return 7u;
#elif (defined(__x86_64__) || defined(_M_X64)) && defined(__clang__) && \
	!defined(__arm64ec__) && !defined(_M_ARM64EC)
		return 8u;
#else
		return 6u;
#endif
	}
}

template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires(
		((::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 23u &&
		  ::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::ebits == 8u) ||
		 (::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 52u &&
		  ::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::ebits == 11u)) &&
		flags.base == 10u && !flags.showpos && !flags.uppercase &&
		!flags.uppercase_e && !flags.comma && !flags.json_float &&
		flags.floating ==
			::fast_io::manipulators::floating_format::decimal &&
		flags.rounding ==
			::fast_io::manipulators::floating_rounding::nearest_to_even)
inline constexpr ::std::size_t print_contiguous_staged_range_width(
	::fast_io::io_reserve_type_t<
		char, ::fast_io::manipulators::scalar_manip_t<flags, flt>>) noexcept
{
	/*
	This is the minimum count that pays for the source-aware phase boundary,
	and exactly the number of independent carriers retained by one iteration.
	It is keyed by the proved IEC representation rather than an exact C++ type:
	a `double` source and GCC's `_Float64` print tag have identical 52/11 field
	partitions, so the classification, cached products, and renderer state are
	the same W=6 computation.  Binary32 uses the compiler/ISA compact-carrier
	schedule documented above.  Returning a compile-time value makes the public
	gate contain no run-time type branch.
	*/
	return ::fast_io::
		print_contiguous_staged_range_optimal_extent<flt>();
}

/*
`conversion_result` is the general scalar carrier and deliberately keeps its
Boolean as an independently addressable field.  A binary32 staged batch never
takes field addresses: it stores W independent results and later consumes them
by value.  Its fine-grid digit belongs to {0,...,9}; therefore

	packed = digit | (has_digit << 4)

is injective, because the digit occupies bits [0,3] and the Boolean occupies
bit 4.  The inverse is `(packed & 15, (packed >> 4) & 1)`, so the staged
carrier is mathematically identical to `conversion_result`.  On ordinary
64/32-bit ABIs it uses 16 rather than 24 bytes, removing 56 bytes from a W=7
array.  The compact form is intentionally private to binary32: M4 measurements
show that binary64's longer renderer pays more for the packed-field extraction
than it saves in carrier traffic.
*/
struct staged_binary32_conversion_result
{
	::std::uint_least64_t significand;
	::std::int_least32_t exponent;
	::std::uint_least32_t digit_and_presence;
};

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr staged_binary32_conversion_result pack_staged_binary32_conversion(
	::fast_io::details::da::conversion_result value) noexcept
{
	return {value.significand, value.exponent,
			value.last_digit |
				(static_cast<::std::uint_least32_t>(
					 value.has_last_digit)
				 << 4u)};
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::uint_least32_t staged_last_digit(
	staged_binary32_conversion_result value) noexcept
{
	return value.digit_and_presence & 15u;
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool
staged_has_last_digit(
	staged_binary32_conversion_result value) noexcept
{
	/*
	`pack_staged_binary32_conversion` writes no bit above bit four, so the shift
	is already exactly zero or one.  A redundant `&1` is mathematically inert,
	but Apple Clang 23 changes the complete W=7 loop schedule when it is present;
	the audited spelling below preserves the faster 384-byte-frame pipeline.
	*/
	return static_cast<bool>(value.digit_and_presence >> 4u);
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::uint_least32_t staged_last_digit(
	::fast_io::details::da::conversion_result value) noexcept
{
	return value.last_digit;
}

[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool
staged_has_last_digit(
	::fast_io::details::da::conversion_result value) noexcept
{
	return value.has_last_digit;
}

template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt, typename source_flt>
	requires(
		::fast_io::details::my_floating_point<
			::std::remove_cv_t<source_flt>> &&
		sizeof(::std::remove_cv_t<source_flt>) ==
			sizeof(::std::remove_cvref_t<flt>) &&
		::fast_io::details::iec559_traits<
			::std::remove_cv_t<source_flt>>::mbits ==
			::fast_io::details::iec559_traits<
				::std::remove_cvref_t<flt>>::mbits &&
		::fast_io::details::iec559_traits<
			::std::remove_cv_t<source_flt>>::ebits ==
			::fast_io::details::iec559_traits<
				::std::remove_cvref_t<flt>>::ebits &&
		((::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 23u &&
		  ::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::ebits == 8u) ||
		 (::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 52u &&
		  ::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::ebits == 11u)) &&
		flags.base == 10u && !flags.showpos && !flags.uppercase &&
		!flags.uppercase_e && !flags.comma && !flags.json_float &&
		flags.floating ==
			::fast_io::manipulators::floating_format::decimal &&
		flags.rounding ==
			::fast_io::manipulators::floating_rounding::nearest_to_even)
[[nodiscard]] inline char *
print_contiguous_staged_range_define_small_separator_impl(
	::fast_io::io_reserve_type_t<
		char, ::fast_io::manipulators::scalar_manip_t<flags, flt>>,
	char *destination, source_flt *source, ::std::size_t count,
	::fast_io::basic_io_scatter_t<char> separator) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr mantissa_type exponent_mask{
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u};
	constexpr ::std::size_t extent{
		::fast_io::
			print_contiguous_staged_range_optimal_extent<flt>()};
	constexpr auto implicit_bit{
		static_cast<::std::uint_least64_t>(1u) << trait::mbits};
	/*
	The caller proves separator.len<=1.  Reading its byte is performed once and
	only when it exists, so an empty scatter may legally carry a null base.
	`separator_advance` is then exactly zero or one.
	*/
	char separator_character{};
	if (separator.len != 0u)
	{
		separator_character = *separator.base;
	}
	auto const separator_advance{separator.len};
	::std::size_t index{};

	/*
	For a one-byte separator the ordered grammar

		value_0, sep, value_1, ..., sep, value_(n-1)

	is byte-identical to writing `value_i, sep` for every i and returning one
	byte before the physical cursor.  For the empty separator, the same program
	writes one scratch byte at the unchanged cursor and returns that cursor.
	Thus one store plus `destination += separator.len` implements both cases
	without a first-element branch in the serial emission phase.

	The final physical separator is inside the staged range's reserve bound.
	Binary32 and binary64 shortest decimal spellings occupy at most 16 and 25
	bytes respectively including sign, whereas their element reserve is at least
	65 bytes.  Hence even n=1 leaves over forty writable scratch bytes; for n>1
	the unused per-element reserves only increase that margin.  This is the same
	physical-store contract used by the fixed-width 8/16-byte digit writers: the
	returned pointer excludes scratch, while the caller supplies the complete
	range reserve rather than an exact logical-size buffer.
	*/
	auto emit_separator_after = [&]() noexcept {
		*destination = separator_character;
		destination += separator_advance;
	};

	for (; index + extent <= count; index += extent)
	{
		bool all_regular{true};
		for (::std::size_t lane{}; lane != extent; ++lane)
		{
			auto const fields{
				::fast_io::details::get_punned_result(
					source[index + lane])};
			/*
			For a finite normal non-power-of-two value, both adjacent binary
			gaps equal one ULP, so the nearest-even preimage is the symmetric
			midpoint interval consumed by the regular DA formula.  Exponent zero
			denotes a subnormal, exponent_mask denotes Inf/NaN, and mantissa zero
			at a normal exponent denotes an exact power of two whose predecessor
			gap is half as large.  The predicate rejects exactly those three
			irregular classes.  Bitwise conjunction evaluates every lane and
			places no short-circuit edge between independent field decodes.
			*/
			all_regular &= ::fast_io::details::da::
				staged_eligible<floating_type>(
					fields.mantissa, fields.exponent, exponent_mask);
		}

		if (all_regular) [[likely]]
		{
			using staged_result = ::std::conditional_t<
				trait::mbits == 23u,
				::fast_io::staged_binary32_conversion_result,
				::fast_io::details::da::conversion_result>;
			staged_result prepared[extent];
			for (::std::size_t lane{}; lane != extent; ++lane)
			{
				auto const fields{
					::fast_io::details::get_punned_result(
						source[index + lane])};
				auto const significand{
					static_cast<::std::uint_least64_t>(fields.mantissa) |
					implicit_bit};
				if constexpr (trait::mbits == 23u)
				{
					prepared[lane] =
						::fast_io::
							pack_staged_binary32_conversion(
								::fast_io::details::da::
									compute_binary32_staged(
										static_cast<
											::std::uint_least32_t>(
											significand),
										fields.exponent));
				}
				else
				{
					prepared[lane] =
						::fast_io::details::da::compute_binary64(
							significand, fields.exponent);
				}
			}

			/*
			No emission is interleaved with the loop above.  This phase boundary
			is the performance invariant: every cached-power multiply becomes
			available to the scheduler before the first serial output cursor is
			introduced.
			*/
			for (::std::size_t lane{}; lane != extent; ++lane)
			{
				auto const fields{
					::fast_io::details::get_punned_result(
						source[index + lane])};
				*destination = static_cast<char>(u8'-');
				destination += static_cast<::std::size_t>(fields.sign);
				char *direct;
				if constexpr (trait::mbits == 23u)
				{
					direct =
						::fast_io::details::da::
							print_ascii_shortest_fields<
								floating_type, flags, true>(
								destination,
								prepared[lane].significand,
								prepared[lane].exponent,
								::fast_io::staged_last_digit(
									prepared[lane]),
								::fast_io::staged_has_last_digit(
									prepared[lane]));
				}
				else
				{
					/*
					Binary64 deliberately retains the ordinary unpacked carrier.
					Besides avoiding bit extraction, passing that exact object
					preserves the smaller audited M4 caller frame.
					*/
					direct =
						::fast_io::details::da::
							print_ascii_shortest<
								floating_type, flags, true>(
								destination, prepared[lane]);
				}
				if (direct != nullptr) [[likely]]
				{
					destination = direct;
				}
				else
				{
					/*
					The decimal presentation normally selects either the compact
					or extended ASCII layout.  If a future layout policy rejects
					a carrier, finalizing that identical carrier and entering the
					character-generic writer preserves the numeric and notation
					contract; it does not recompute the interval.
					*/
					auto const finalized{[&]() noexcept {
						if constexpr (trait::mbits == 23u)
						{
							return ::fast_io::details::da::
								trim_trailing_zeros(
									::fast_io::details::da::
										finalize<floating_type>({prepared[lane].significand,
																 prepared[lane].exponent,
																 ::fast_io::
																	 staged_last_digit(
																		 prepared[lane]),
																 ::fast_io::
																	 staged_has_last_digit(
																		 prepared[lane])}));
						}
						else
						{
							return ::fast_io::details::da::
								trim_trailing_zeros(
									::fast_io::details::da::
										finalize<floating_type>(
											prepared[lane]));
						}
					}()};
					destination =
						::fast_io::details::
							print_rsvflt_decimal_define_impl<
								floating_type, flags.comma,
								flags.uppercase_e, flags.floating,
								flags.json_float>(
								destination, finalized.m10,
								finalized.e10);
				}
				emit_separator_after();
			}
			continue;
		}

		/*
		No output has been produced for this group when classification fails.
		The complete group may therefore use the scalar field entry without
		rollback.  That entry owns subnormal normalization, the asymmetric
		power-of-two interval, signed zero, infinities, and NaN payload grammar.
		The fold remains inline because measurement showed that outlining it
		enlarged the linked image and added a hot conditional call while saving
		too little frame space.  Every lane still executes in source order, and
		later groups are independent and may re-enter the regular batch.
		*/
		for (::std::size_t lane{}; lane != extent; ++lane)
		{
			auto const fields{
				::fast_io::details::get_punned_result(
					source[index + lane])};
			destination =
				::fast_io::details::print_rsvflt_fields_define_impl<
					flags.showpos, flags.uppercase,
					flags.uppercase_e, flags.comma, flags.floating,
					flags.rounding, flags.nan_show_sign,
					flags.nan_show_type, flags.json_float,
					floating_type>(
					destination, fields.mantissa,
					fields.exponent, fields.sign);
			emit_separator_after();
		}
	}

	/*
	The suffix contains fewer than W values.  Preparing it cannot realize the
	measured latency-hiding width, so scalar emission avoids an additional
	partially live state array.  This is exactly the untouched suffix of the
	original ordered fold.
	*/
	for (; index != count; ++index)
	{
		auto const fields{
			::fast_io::details::get_punned_result(source[index])};
		destination =
			::fast_io::details::print_rsvflt_fields_define_impl<
				flags.showpos, flags.uppercase, flags.uppercase_e,
				flags.comma, flags.floating, flags.rounding,
				flags.nan_show_sign, flags.nan_show_type,
				flags.json_float, floating_type>(
				destination, fields.mantissa, fields.exponent,
				fields.sign);
		emit_separator_after();
	}
	return count == 0u ? destination
					   : destination - separator_advance;
}

template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt, typename source_flt>
	requires(
		::fast_io::details::my_floating_point<
			::std::remove_cv_t<source_flt>> &&
		sizeof(::std::remove_cv_t<source_flt>) ==
			sizeof(::std::remove_cvref_t<flt>) &&
		::fast_io::details::iec559_traits<
			::std::remove_cv_t<source_flt>>::mbits ==
			::fast_io::details::iec559_traits<
				::std::remove_cvref_t<flt>>::mbits &&
		::fast_io::details::iec559_traits<
			::std::remove_cv_t<source_flt>>::ebits ==
			::fast_io::details::iec559_traits<
				::std::remove_cvref_t<flt>>::ebits &&
		((::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 23u &&
		  ::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::ebits == 8u) ||
		 (::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 52u &&
		  ::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::ebits == 11u)) &&
		flags.base == 10u && !flags.showpos && !flags.uppercase &&
		!flags.uppercase_e && !flags.comma && !flags.json_float &&
		flags.floating ==
			::fast_io::manipulators::floating_format::decimal &&
		flags.rounding ==
			::fast_io::manipulators::floating_rounding::nearest_to_even)
[[nodiscard]] inline char *print_contiguous_staged_range_define(
	::fast_io::io_reserve_type_t<
		char, ::fast_io::manipulators::scalar_manip_t<flags, flt>>
		tag,
	char *destination, source_flt *source, ::std::size_t count,
	::fast_io::basic_io_scatter_t<char> separator) noexcept
{
	/*
	Separator shape is loop-invariant.  Empty and one-code-unit separators share
	one branchless inner kernel, avoiding the roughly 36 KiB text increase
	measured when three complete f32/f64 kernels were instantiated.  A longer
	scatter is uncommon in numeric batch output and retains the compact scalar
	fold below.  Both choices use the same scalar formatting leaf for every
	irregular value and preserve element/separator ordering.
	*/
	if (separator.len <= 1u)
	{
		return ::fast_io::
			print_contiguous_staged_range_define_small_separator_impl(
				tag, destination, source, count, separator);
	}
	using floating_type = ::std::remove_cvref_t<flt>;
	for (::std::size_t index{}; index != count; ++index)
	{
		if (index != 0u)
		{
			destination =
				::fast_io::details::decay::small_scatter_copy_n(
					separator.base, separator.len, destination);
		}
		auto const fields{
			::fast_io::details::get_punned_result(source[index])};
		destination =
			::fast_io::details::print_rsvflt_fields_define_impl<
				flags.showpos, flags.uppercase, flags.uppercase_e,
				flags.comma, flags.floating, flags.rounding,
				flags.nan_show_sign, flags.nan_show_type,
				flags.json_float, floating_type>(
				destination, fields.mantissa, fields.exponent,
				fields.sign);
	}
	return destination;
}

/*
Specialized contiguous binary16/bfloat16 nearest-even batch
===========================================================

For either narrow IEC representation, remove the sign bit and call the
remaining 15-bit word r.  The compact table stores the presentation-independent
nearest-even carrier

	C(r) = (m(r), e(r), digits(m(r)))

in three bytes; dragonbox/impl.h proves the 22-bit encoding and the exact
3*2^15 = 96-KiB size.  No pre-rendered character table exists.  Consequently
one carrier table serves `char`, every wide character type, ASCII, and EBCDIC,
and one binary16/bfloat16 instantiation can never exceed the 100-KiB table
budget.

For W independent source words, the addresses &C(r_i) do not depend on the
output cursor or on another lane.  Reconstructing the W carriers before the
first presentation is a topological reordering of the scalar loop: it exposes
3W independent byte loads, then preserves
sign/value order while the common renderer advances the serial cursor.  The
renderer is deliberately outside the table; its integer digit leaves are small
enough to remain hot and avoid multiplying 6--10 presentation bytes by every
one of 32,768 encodings.  No native narrow arithmetic or conversion occurs,
which also avoids x86 bfloat16 ABI paths that may rematerialize or quiet a value.

Zero and the special exponent are excluded from the carrier phase.  Signed zero
has a grammar-specific one-byte magnitude, while Inf/NaN presentation may
depend on payload and flags.  Since classification finishes before any byte of
the group is written, an excluded lane permits the complete group to enter the
existing scalar field formatter without rollback.  Subnormals are admitted:
their compact carriers were generated from the same exact narrow field model
and need no normalization at run time.
*/
template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires(
		((::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 10u &&
		  ::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::ebits == 5u) ||
		 (::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 7u &&
		  ::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::ebits == 8u)) &&
		sizeof(::std::remove_cvref_t<flt>) == 2u &&
		flags.base == 10u && !flags.showpos && !flags.uppercase &&
		!flags.uppercase_e && !flags.comma && !flags.json_float &&
		flags.floating ==
			::fast_io::manipulators::floating_format::decimal &&
		flags.rounding ==
			::fast_io::manipulators::floating_rounding::nearest_to_even)
inline constexpr ::std::size_t print_contiguous_staged_range_width(
	::fast_io::io_reserve_type_t<
		char, ::fast_io::manipulators::scalar_manip_t<flags, flt>>) noexcept
{
	/*
	The width is part of the representation-specific code-generation contract,
	not a run-time architecture dispatch.  With the 24-bit carrier, paired M4
	public-range measurements select W=2 for binary16 and W=4 for bfloat16.
	Binary16's longer coefficients make four reconstructed layouts compete with
	the renderer; bfloat16's at-most-four-digit coefficient leaves enough
	register capacity for four table reads.  Both widths preserve the same
	topological proof above and bound fallback duplication.
	*/
	using trait = ::fast_io::details::iec559_traits<
		::std::remove_cvref_t<flt>>;
	return trait::mbits == 7u ? 4u : 2u;
}

template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt, typename source_flt>
	requires(
		::fast_io::details::my_floating_point<
			::std::remove_cv_t<source_flt>> &&
		sizeof(::std::remove_cv_t<source_flt>) == 2u &&
		sizeof(::std::remove_cvref_t<flt>) == 2u &&
		::fast_io::details::iec559_traits<
			::std::remove_cv_t<source_flt>>::mbits ==
			::fast_io::details::iec559_traits<
				::std::remove_cvref_t<flt>>::mbits &&
		::fast_io::details::iec559_traits<
			::std::remove_cv_t<source_flt>>::ebits ==
			::fast_io::details::iec559_traits<
				::std::remove_cvref_t<flt>>::ebits &&
		((::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 10u &&
		  ::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::ebits == 5u) ||
		 (::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 7u &&
		  ::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::ebits == 8u)) &&
		flags.base == 10u && !flags.showpos && !flags.uppercase &&
		!flags.uppercase_e && !flags.comma && !flags.json_float &&
		flags.floating ==
			::fast_io::manipulators::floating_format::decimal &&
		flags.rounding ==
			::fast_io::manipulators::floating_rounding::nearest_to_even)
[[nodiscard]] inline char *
print_contiguous_staged_narrow_range_define_small_separator_impl(
	::fast_io::io_reserve_type_t<
		char, ::fast_io::manipulators::scalar_manip_t<flags, flt>>,
	char *destination, source_flt *source, ::std::size_t count,
	::fast_io::basic_io_scatter_t<char> separator) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	using carrier_type =
		::fast_io::details::dragonbox_narrow_shortest_result<floating_type>;
	constexpr ::std::size_t extent{
		trait::mbits == 7u ? 4u : 2u};
	constexpr auto mantissa_mask{
		(static_cast<::std::uint_least16_t>(1u) << trait::mbits) - 1u};
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least16_t>(1u) << trait::ebits) - 1u};
	constexpr auto magnitude_mask{
		(static_cast<::std::uint_least16_t>(1u)
		 << (trait::mbits + trait::ebits)) -
		1u};
	char separator_character{};
	if (separator.len != 0u)
	{
		/*
		The admission predicate in the public overload proves len<=1.  Reading is
		guarded so an empty scatter may legally carry a null base.
		*/
		separator_character = *separator.base;
	}
	auto const separator_advance{separator.len};
	bool has_preceding_element{};
	auto emit_separator = [&]() noexcept {
		if (has_preceding_element && separator_advance != 0u)
		{
			*destination = separator_character;
			++destination;
		}
		has_preceding_element = true;
	};

	auto const raw_representation =
		[](source_flt const &value) noexcept {
			static_assert(sizeof(value) ==
						  sizeof(::std::uint_least16_t));
#if FAST_IO_HAS_BUILTIN(__builtin_bit_cast)
			return __builtin_bit_cast(
				::std::uint_least16_t, value);
#else
			return ::fast_io::bit_cast<
				::std::uint_least16_t>(value);
#endif
		};

	::std::size_t index{};
	for (; index + extent <= count; index += extent)
	{
		carrier_type converted[extent];
		bool signs[extent];
		bool all_ordinary{true};
		for (::std::size_t lane{}; lane != extent; ++lane)
		{
			auto const raw{
				raw_representation(source[index + lane])};
			auto const magnitude{
				static_cast<::std::uint_least16_t>(
					raw & magnitude_mask)};
			auto const exponent{
				static_cast<::std::uint_least16_t>(
					magnitude >> trait::mbits)};
			signs[lane] = static_cast<bool>(
				raw >> (trait::mbits + trait::ebits));
			auto const mantissa{
				static_cast<mantissa_type>(
					magnitude & mantissa_mask)};
			converted[lane] =
				::fast_io::details::
					dragonbox_narrow_shortest_lookup<
						floating_type>(
						mantissa,
						static_cast<::std::int_least32_t>(
							exponent));
			/*
			magnitude zero is ±0.  exponent_mask is Inf/NaN.  Every other
			magnitude, including exponent-zero subnormals, owns a nonzero compact
			carrier.  Bitwise conjunction exposes all four independent lookups
			before the emission phase.
			*/
			all_ordinary &= magnitude != 0u &&
							exponent != exponent_mask;
		}

		if (all_ordinary) [[likely]]
		{
			for (::std::size_t lane{}; lane != extent; ++lane)
			{
				emit_separator();
				*destination = static_cast<char>(u8'-');
				destination +=
					static_cast<::std::size_t>(signs[lane]);
				/*
				The coefficient, exponent, and derived length denote exactly the
				same carrier as the scalar narrow path.  The shared renderer is a
				pure presentation function, hence moving conversion before the
				group's first output byte cannot change either spelling or order.
				*/
				auto const &decimal{converted[lane]};
				destination =
					::fast_io::details::
						print_rsvflt_narrow_ascii_decimal<
							floating_type>(
							destination, decimal);
			}
			continue;
		}

		for (::std::size_t lane{}; lane != extent; ++lane)
		{
			emit_separator();
			auto const raw{
				raw_representation(source[index + lane])};
			auto const mantissa{
				static_cast<mantissa_type>(
					raw & mantissa_mask)};
			auto const exponent{
				static_cast<::std::uint_least32_t>(
					(raw >> trait::mbits) & exponent_mask)};
			auto const sign{static_cast<bool>(
				raw >> (trait::mbits + trait::ebits))};
			destination =
				::fast_io::details::
					print_rsvflt_fields_define_impl<
						false, false, false, false,
						::fast_io::manipulators::
							floating_format::decimal,
						::fast_io::manipulators::
							floating_rounding::nearest_to_even,
						true, false, false, floating_type>(
						destination, mantissa, exponent, sign);
		}
	}

	for (; index != count; ++index)
	{
		emit_separator();
		auto const raw{raw_representation(source[index])};
		auto const mantissa{
			static_cast<mantissa_type>(raw & mantissa_mask)};
		auto const exponent{
			static_cast<::std::uint_least32_t>(
				(raw >> trait::mbits) & exponent_mask)};
		auto const sign{static_cast<bool>(
			raw >> (trait::mbits + trait::ebits))};
		destination =
			::fast_io::details::print_rsvflt_fields_define_impl<
				false, false, false, false,
				::fast_io::manipulators::floating_format::decimal,
				::fast_io::manipulators::floating_rounding::nearest_to_even,
				true, false, false, floating_type>(
				destination, mantissa, exponent, sign);
	}
	return destination;
}

template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt, typename source_flt>
	requires(
		::fast_io::details::my_floating_point<
			::std::remove_cv_t<source_flt>> &&
		sizeof(::std::remove_cv_t<source_flt>) == 2u &&
		sizeof(::std::remove_cvref_t<flt>) == 2u &&
		::fast_io::details::iec559_traits<
			::std::remove_cv_t<source_flt>>::mbits ==
			::fast_io::details::iec559_traits<
				::std::remove_cvref_t<flt>>::mbits &&
		::fast_io::details::iec559_traits<
			::std::remove_cv_t<source_flt>>::ebits ==
			::fast_io::details::iec559_traits<
				::std::remove_cvref_t<flt>>::ebits &&
		((::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 10u &&
		  ::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::ebits == 5u) ||
		 (::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 7u &&
		  ::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::ebits == 8u)) &&
		flags.base == 10u && !flags.showpos && !flags.uppercase &&
		!flags.uppercase_e && !flags.comma && !flags.json_float &&
		flags.floating ==
			::fast_io::manipulators::floating_format::decimal &&
		flags.rounding ==
			::fast_io::manipulators::floating_rounding::nearest_to_even)
[[nodiscard]] inline char *print_contiguous_staged_range_define(
	::fast_io::io_reserve_type_t<
		char, ::fast_io::manipulators::scalar_manip_t<flags, flt>>
		tag,
	char *destination, source_flt *source, ::std::size_t count,
	::fast_io::basic_io_scatter_t<char> separator) noexcept
{
	if (separator.len <= 1u)
	{
		return ::fast_io::
			print_contiguous_staged_narrow_range_define_small_separator_impl(
				tag, destination, source, count, separator);
	}
	using floating_type = ::std::remove_cvref_t<flt>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr auto mantissa_mask{
		(static_cast<::std::uint_least16_t>(1u) << trait::mbits) - 1u};
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least16_t>(1u) << trait::ebits) - 1u};
	for (::std::size_t index{}; index != count; ++index)
	{
		if (index != 0u)
		{
			destination =
				::fast_io::details::decay::small_scatter_copy_n(
					separator.base, separator.len, destination);
		}
		auto const raw{
#if FAST_IO_HAS_BUILTIN(__builtin_bit_cast)
			__builtin_bit_cast(
				::std::uint_least16_t, source[index])
#else
			::fast_io::bit_cast<::std::uint_least16_t>(
				source[index])
#endif
		};
		auto const mantissa{
			static_cast<typename trait::mantissa_type>(
				raw & mantissa_mask)};
		auto const exponent{
			static_cast<::std::uint_least32_t>(
				(raw >> trait::mbits) & exponent_mask)};
		auto const sign{static_cast<bool>(
			raw >> (trait::mbits + trait::ebits))};
		destination =
			::fast_io::details::print_rsvflt_fields_define_impl<
				false, false, false, false,
				::fast_io::manipulators::floating_format::decimal,
				::fast_io::manipulators::floating_rounding::nearest_to_even,
				true, false, false, floating_type>(
				destination, mantissa, exponent, sign);
	}
	return destination;
}

#if (defined(__clang__) ||                                           \
	 (defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 13)) && \
	defined(__SIZEOF_INT128__)
/*
Compiler-selected binary80/binary128 nearest-even range schedule
=================================================================

One wide conversion performs several independent 128x256 cached-power
products, followed by a serial decimal-removal loop and presentation.  For two
source values A and B, conversion(A) and conversion(B) have no data dependence;
only emit(A) must precede emit(B).  The legal topological order is therefore

	convert(A), convert(B), emit(A), emit(B).

Clang 17--23 on the measured x86-64 P-core reduce this order by up to roughly
three nanoseconds per value.  GCC 11 and 12 also expose the independent
multiply chains and gain approximately three to five nanoseconds in the
conversion leaf.  Four lanes extend six u128 interval values per lane across
too much live state and lose the gain.  GCC 13--16 instead schedule the scalar
form better and regress with W=2.  The preprocessor fence is consequently a
measured compiler code-generation policy, not a numeric or ISA capability:
Clang remains enabled, GCC is enabled only before major 13, and every supported
major remains in the compile matrix so a measured counterexample can narrow the
fence without changing the algorithm.

The group is converted only when both values are finite and nonzero.  Since no
byte is emitted before classification completes, a zero, Inf, or NaN sends the
whole group through the scalar grammar without rollback.  Ordinary powers of
two and subnormals remain eligible: wide_ryu_nearest_even proves both interval
forms and needs no floating arithmetic.  Separators are emitted only in the
second phase, preserving the exact ordered concatenation of the scalar fold.
*/
template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires(
		((::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 63u) ||
		 (::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 112u)) &&
		::fast_io::details::iec559_traits<
			::std::remove_cvref_t<flt>>::ebits == 15u &&
		flags.base == 10u && !flags.showpos && !flags.uppercase &&
		!flags.uppercase_e && !flags.comma && !flags.json_float &&
		flags.floating ==
			::fast_io::manipulators::floating_format::decimal &&
		flags.rounding ==
			::fast_io::manipulators::floating_rounding::nearest_to_even)
inline constexpr ::std::size_t print_contiguous_staged_range_width(
	::fast_io::io_reserve_type_t<
		char, ::fast_io::manipulators::scalar_manip_t<
				  flags, flt>>) noexcept
{
	return 2u;
}

template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt, typename source_flt>
	requires(
		::fast_io::details::my_floating_point<
			::std::remove_cv_t<source_flt>> &&
		::fast_io::details::iec559_traits<
			::std::remove_cv_t<source_flt>>::mbits ==
			::fast_io::details::iec559_traits<
				::std::remove_cvref_t<flt>>::mbits &&
		::fast_io::details::iec559_traits<
			::std::remove_cv_t<source_flt>>::ebits ==
			::fast_io::details::iec559_traits<
				::std::remove_cvref_t<flt>>::ebits &&
		((::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 63u) ||
		 (::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 112u)) &&
		::fast_io::details::iec559_traits<
			::std::remove_cvref_t<flt>>::ebits == 15u &&
		flags.base == 10u && !flags.showpos && !flags.uppercase &&
		!flags.uppercase_e && !flags.comma && !flags.json_float &&
		flags.floating ==
			::fast_io::manipulators::floating_format::decimal &&
		flags.rounding ==
			::fast_io::manipulators::floating_rounding::nearest_to_even)
[[nodiscard]] inline char *
print_contiguous_staged_wide_range_define_small_separator_impl(
	::fast_io::io_reserve_type_t<
		char, ::fast_io::manipulators::scalar_manip_t<flags, flt>>,
	char *destination, source_flt *source, ::std::size_t count,
	::fast_io::basic_io_scatter_t<char> separator) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr ::std::size_t extent{2u};
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) -
		1u};
	char separator_character{};
	if (separator.len != 0u)
	{
		separator_character = *separator.base;
	}
	auto const separator_advance{separator.len};
	bool has_preceding_element{};
	auto emit_separator = [&]() noexcept {
		if (has_preceding_element && separator_advance != 0u)
		{
			*destination++ = separator_character;
		}
		has_preceding_element = true;
	};

	::std::size_t index{};
	for (; index + extent <= count; index += extent)
	{
		::fast_io::details::wide_ryu_result converted[extent];
		bool signs[extent];
		bool all_ordinary{true};
		for (::std::size_t lane{}; lane != extent; ++lane)
		{
			auto const fields{
				::fast_io::details::get_punned_result(
					source[index + lane])};
			signs[lane] = fields.sign;
			converted[lane] =
				::fast_io::details::
					wide_ryu_nearest_even<floating_type>(
						fields.mantissa, fields.exponent);
			all_ordinary &=
				(fields.mantissa != 0u ||
				 fields.exponent != 0u) &&
				fields.exponent != exponent_mask;
		}
		if (all_ordinary) [[likely]]
		{
			for (::std::size_t lane{}; lane != extent; ++lane)
			{
				emit_separator();
				*destination = static_cast<char>(u8'-');
				destination += static_cast<::std::size_t>(
					signs[lane]);
				destination =
					::fast_io::details::
						print_rsvflt_decimal_define_impl<
							floating_type, false, false,
							::fast_io::manipulators::
								floating_format::decimal,
							false>(
							destination,
							converted[lane].m10,
							converted[lane].e10);
			}
			continue;
		}
		for (::std::size_t lane{}; lane != extent; ++lane)
		{
			emit_separator();
			auto const fields{
				::fast_io::details::get_punned_result(
					source[index + lane])};
			destination =
				::fast_io::details::
					print_floating_wide_scalar_fields_define<
						flags, floating_type>(
						destination, fields.mantissa,
						fields.exponent, fields.sign);
		}
	}
	for (; index != count; ++index)
	{
		emit_separator();
		auto const fields{
			::fast_io::details::get_punned_result(source[index])};
		destination =
			::fast_io::details::
				print_floating_wide_scalar_fields_define<
					flags, floating_type>(
					destination, fields.mantissa,
					fields.exponent, fields.sign);
	}
	return destination;
}

template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt, typename source_flt>
	requires(
		::fast_io::details::my_floating_point<
			::std::remove_cv_t<source_flt>> &&
		::fast_io::details::iec559_traits<
			::std::remove_cv_t<source_flt>>::mbits ==
			::fast_io::details::iec559_traits<
				::std::remove_cvref_t<flt>>::mbits &&
		::fast_io::details::iec559_traits<
			::std::remove_cv_t<source_flt>>::ebits ==
			::fast_io::details::iec559_traits<
				::std::remove_cvref_t<flt>>::ebits &&
		((::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 63u) ||
		 (::fast_io::details::iec559_traits<
			  ::std::remove_cvref_t<flt>>::mbits == 112u)) &&
		::fast_io::details::iec559_traits<
			::std::remove_cvref_t<flt>>::ebits == 15u &&
		flags.base == 10u && !flags.showpos && !flags.uppercase &&
		!flags.uppercase_e && !flags.comma && !flags.json_float &&
		flags.floating ==
			::fast_io::manipulators::floating_format::decimal &&
		flags.rounding ==
			::fast_io::manipulators::floating_rounding::nearest_to_even)
[[nodiscard]] inline char *print_contiguous_staged_range_define(
	::fast_io::io_reserve_type_t<
		char, ::fast_io::manipulators::scalar_manip_t<flags, flt>>
		tag,
	char *destination, source_flt *source, ::std::size_t count,
	::fast_io::basic_io_scatter_t<char> separator) noexcept
{
	if (separator.len <= 1u)
	{
		return ::fast_io::
			print_contiguous_staged_wide_range_define_small_separator_impl(
				tag, destination, source, count, separator);
	}
	using floating_type = ::std::remove_cvref_t<flt>;
	for (::std::size_t index{}; index != count; ++index)
	{
		if (index != 0u)
		{
			destination =
				::fast_io::details::decay::small_scatter_copy_n(
					separator.base, separator.len, destination);
		}
		auto const fields{
			::fast_io::details::get_punned_result(source[index])};
		destination =
			::fast_io::details::
				print_floating_wide_scalar_fields_define<
					flags, floating_type>(
					destination, fields.mantissa,
					fields.exponent, fields.sign);
	}
	return destination;
}
#endif

/// @feature concept:runtime_precise_size
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_scalar_supported<flags, flt>
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating ||
				  manipulators::floating_format::hexfloat == flags.floating);
	using trait = ::fast_io::details::iec559_traits<flt>;
	if constexpr (flags.floating == manipulators::floating_format::hexfloat)
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
					  || ::std::same_as<::std::remove_cvref_t<flt>, __float128>
#endif
		)
		{
			if constexpr (::fast_io::details::fp_floating_point_is_float80<::std::remove_cvref_t<flt>>)
			{
				return details::print_rsv_fp_size_with_special_cache<
					details::print_rsvhexfloat_size_cache<flags.showbase,
														  typename details::iec559_traits<::std::remove_cvref_t<flt>>::mantissa_type>,
					flags.nan_show_type>;
			}
			else
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
				if constexpr (sizeof(flt) > sizeof(double))
			{
				return details::print_rsv_fp_size_with_special_cache<
					details::print_rsvhexfloat_size_cache<flags.showbase, __uint128_t>, flags.nan_show_type>;
			}
			else
#endif
				return details::print_rsv_fp_size_with_special_cache<
					details::print_rsvhexfloat_size_cache<flags.showbase,
														  typename details::iec559_traits<double>::mantissa_type>,
					flags.nan_show_type>;
		}
		else
		{
			return details::print_rsv_fp_size_with_special_cache<
				details::print_rsvhexfloat_size_cache<flags.showbase, typename trait::mantissa_type>,
				flags.nan_show_type>;
		}
	}
	else
	{
		constexpr ::std::size_t decimal_extra{
			((flags.floating == manipulators::floating_format::general) ? 3u : 0u) +
			((flags.json_float && flags.floating != manipulators::floating_format::scientific) ? 2u : 0u)};
		/*
		The narrow ASCII DA renderer deliberately uses fixed-width stores.  Its
		binary32 scientific leaf can physically reach

		  sign + one leading-digit offset + nine carrier bytes + eight exponent bytes = 19,

		while the logical binary32 cache is 15.  The corresponding binary64 bound
		is 1 + 1 + 17 + 8 = 27 versus a logical cache of 24.  Decimal and
		scientific therefore need four and three additional writable bytes,
		respectively; general already contributes three logical-policy bytes but
		binary32 still needs one more.  Fixed has a much larger exponent-derived
		bound, and non-char/EBCDIC renderers do not enter this ASCII store contract.

		Both explicit nearest-even and current-environment formatting receive this
		capacity: the latter may dispatch to the former after reading the run-time
		rounding mode.  This value is reserve capacity, not reported output length.
		The precise-reserve CPO continues to use its independent exact size and
		exact-store renderer.  Taking the maximum rather than adding both allowances
		is valid because JSON's two-byte suffix is emitted only for zero, which never
		enters the staged finite-carrier writer.
		*/
		constexpr bool uses_da_ascii_staging{
			::std::same_as<char_type, char> &&
			::fast_io::details::is_ascii<char_type> &&
			(flags.rounding == manipulators::floating_rounding::nearest_to_even ||
			 flags.rounding == manipulators::floating_rounding::current_environment) &&
			flags.floating != manipulators::floating_format::fixed};
		constexpr ::std::size_t da_ascii_staging_extra{
			uses_da_ascii_staging
				? (::fast_io::details::print_floating_decimal_via_float<flt>
					   ? 4u
					   : (trait::mbits <= 23u && trait::ebits <= 8u
							  ? 4u
							  : (trait::mbits == 52u && trait::ebits == 11u ? 3u : 0u)))
				: 0u};
		constexpr ::std::size_t decimal_capacity_extra{
			decimal_extra < da_ascii_staging_extra ? da_ascii_staging_extra : decimal_extra};
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
					  sizeof(flt) == sizeof(double)) // this is the case on xxx-windows-msvc
		{
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				details::print_rsv_fp_size_with_special_cache<details::print_rsv_cache<double, flags.floating>,
															  flags.nan_show_type>,
				decimal_capacity_extra);
		}
		else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
		{
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				details::print_rsv_fp_size_with_special_cache<details::print_rsv_cache<float, flags.floating>,
															  flags.nan_show_type>,
				decimal_capacity_extra);
		}
		else if constexpr (
			::fast_io::details::print_floating_decimal_exact_supported<flt>)
		{
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				details::print_rsv_fp_size_with_special_cache<
					details::print_rsv_cache<::std::remove_cvref_t<flt>,
											 flags.floating>,
					flags.nan_show_type>,
				decimal_capacity_extra);
		}
		else
		{
			static_assert(::fast_io::details::print_floating_decimal_direct_supported<flt>,
						  "currently only support iec559 float32 and float64 decimal output; narrower IEC559 "
						  "formats are printed through float");
			return ::fast_io::details::intrinsics::add_or_overflow_die(
				details::print_rsv_fp_size_with_special_cache<
					details::print_rsv_cache<::std::remove_cvref_t<flt>, flags.floating>, flags.nan_show_type>,
				decimal_capacity_extra);
		}
	}
}

namespace details::decay
{

/// The narrow ASCII shortest formatter intentionally uses fixed-width stores inside its advertised reserve bound.
/// A fixed external view therefore stages this exact source category before publishing the logical cursor, while
/// ordinary buffered destinations retain the faster in-place reserve writer.
template <::std::integral char_type, manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
struct print_fixed_external_overstore_traits<
	char_type, manipulators::scalar_manip_t<flags, flt>>
{
	using trait = ::fast_io::details::iec559_traits<::std::remove_cvref_t<flt>>;
	inline static constexpr bool available{
		::std::same_as<char_type, char> && ::fast_io::details::is_ascii<char_type> &&
		::fast_io::details::print_floating_scalar_supported<flags, flt> &&
		(flags.rounding == manipulators::floating_rounding::nearest_to_even ||
		 flags.rounding == manipulators::floating_rounding::current_environment) &&
		(::fast_io::details::print_floating_decimal_via_float<flt> ||
		 (trait::mbits <= 23u && trait::ebits <= 8u) ||
		 (trait::mbits == 52u && trait::ebits == 11u))};
};

} // namespace details::decay

/// Floating scalar spellings are composed of sign characters, digits, radix/exponent punctuation, and the fixed
/// alphabetic special values (`inf`/`nan` plus an optional payload).  None of those grammars emits C whitespace.  This
/// source-side proof is intentionally attached to the exact supported scalar formatter, so custom floating wrappers or
/// unrelated text providers cannot select the token-direct `inplace_to` path by merely being reserve-printable.
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires ::fast_io::details::print_floating_scalar_supported<flags, flt>
inline constexpr ::std::true_type print_fragment_c_space_free(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>) noexcept
{
	return {};
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(::fast_io::details::print_floating_scalar_supported<flags, flt> &&
			 !::fast_io::details::print_floating_decimal_requires_integer_transport<flt>)
inline constexpr char_type *print_reserve_define(io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>,
											 char_type *iter,
											 ::fast_io::details::floating_value_or_field_parameter_t<
													 manipulators::scalar_manip_t<flags, flt>, flt>
													 f) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating ||
				  manipulators::floating_format::hexfloat == flags.floating);
	using floating_type = ::std::remove_cvref_t<flt>;
	if constexpr (::fast_io::details::print_floating_requires_object_field_capture<flt>)
	{
		auto const fields{f.fields};
		if constexpr (flags.floating ==
					  manipulators::floating_format::hexfloat)
		{
			return ::fast_io::details::compiler_constant_hex_scalar_fields_define<flags>(
				iter, fields);
		}
		else
		{
			return details::print_rsvflt_fields_define_impl<
				flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
				flags.floating, flags.rounding, flags.nan_show_sign,
				flags.nan_show_type, flags.json_float, floating_type>(
				iter, fields.mantissa, fields.exponent, fields.sign);
		}
	}
	else if constexpr (flags.floating == manipulators::floating_format::hexfloat)
	{
		if constexpr (::fast_io::details::fp_floating_point_is_ibm_double_double<
						  floating_type>)
		{
			/* The synthetic p=106 fields denote the exact component sum.  Hexadecimal
			   output is exact, so no adjacent-value property is needed here. */
			auto const fields{::fast_io::details::get_punned_result(f.reference)};
			return ::fast_io::details::compiler_constant_hex_scalar_fields_define<flags>(
				iter, fields);
		}
		else if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
						   || ::std::same_as<::std::remove_cvref_t<flt>, __float128>
#endif
		)
		{
			if constexpr (::fast_io::details::fp_floating_point_is_float80<::std::remove_cvref_t<flt>>)
			{
				return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
															  flags.uppercase, flags.uppercase_e, flags.comma,
															  flags.nan_show_sign, flags.nan_show_type>(
					iter, f.reference);
			}
			else
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
				if constexpr (sizeof(flt) > sizeof(double))
			{
				return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
															  flags.uppercase, flags.uppercase_e, flags.comma,
															  flags.nan_show_sign, flags.nan_show_type>(
					iter, static_cast<__float128>(f.reference));
			}
			else
#endif
				return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
															  flags.uppercase, flags.uppercase_e, flags.comma,
															  flags.nan_show_sign, flags.nan_show_type>(
					iter, static_cast<double>(f.reference));
		}
		else
		{
			return details::print_rsvhexfloat_define_impl<flags.showbase, flags.uppercase_showbase, flags.showpos,
														  flags.uppercase, flags.uppercase_e, flags.comma,
														  flags.nan_show_sign, flags.nan_show_type>(iter,
																									f.reference);
		}
	}
	else
	{
		if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double> &&
					  sizeof(flt) == sizeof(double)) // this is the case on xxx-windows-msvc
		{
			return details::print_rsvflt_define_impl<flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
													 flags.floating, flags.rounding, flags.nan_show_sign,
													 flags.nan_show_type, flags.json_float>(
				iter, static_cast<double>(f.reference));
		}
		else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
		{
			using trait = ::fast_io::details::iec559_traits<floating_type>;
			auto const [mantissa, exponent, sign]{
				::fast_io::details::get_punned_result(f.reference)};
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
			return details::print_rsvflt_define_impl<flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
													 flags.floating, flags.rounding, flags.nan_show_sign,
													 flags.nan_show_type, flags.json_float>(
				iter, widened);
		}
#if defined(__SIZEOF_INT128__)
		else if constexpr (
			::fast_io::details::fp_floating_point_is_ibm_double_double<
				floating_type>)
		{
			return ::fast_io::details::
				print_floating_ibm_double_double_scalar_define<flags>(
					iter, f.reference);
		}
		else if constexpr (
			::fast_io::details::print_floating_decimal_exact_supported<flt>)
		{
			auto const [mantissa, exponent, negative]{
				::fast_io::details::get_punned_result(f.reference)};
			return ::fast_io::details::
				print_floating_wide_scalar_fields_define<flags,
														 ::std::remove_cvref_t<flt>>(
					iter, mantissa, exponent, negative);
		}
#endif
		else
		{
			// this is the case for every other platform, including xxx-windows-gnu
			static_assert(::fast_io::details::print_floating_decimal_direct_supported<flt>,
						  "currently only support iec559 float32 and float64 decimal output; narrower IEC559 "
						  "formats are printed through float");
			return details::print_rsvflt_define_impl<
				flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
				flags.floating, flags.rounding, flags.nan_show_sign,
				flags.nan_show_type, flags.json_float>(iter, f.reference);
		}
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
	requires(::fast_io::details::print_floating_ordinary_supported<flags, flt> &&
			 ::fast_io::details::print_floating_decimal_requires_integer_transport<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type, manipulators::scalar_manip_t<flags, flt>>,
	char_type *iter, manipulators::scalar_manip_t<flags, flt> const &f) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	if constexpr (flags.floating ==
				  ::fast_io::manipulators::floating_format::hexfloat)
	{
		auto const fields{
			::fast_io::details::compiler_constant_floating_capture_fields<
				floating_type>(
				f.reference)};
		return ::fast_io::details::compiler_constant_hex_scalar_fields_define<flags>(
			iter, fields);
	}
	else
	{
		auto const [mantissa, exponent, sign]{
			::fast_io::details::compiler_constant_floating_capture_fields<
				floating_type>(
				f.reference)};
		return details::print_rsvflt_fields_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float, floating_type>(
			iter, mantissa, exponent, sign);
	}
}

template <::std::integral char_type,
		  manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_scalar_supported<flags, flt> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr ::std::size_t print_reserve_size(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_field_manip_t<flags, flt>>) noexcept
{
	return print_reserve_size(
		::fast_io::io_reserve_type<char_type,
								   manipulators::scalar_manip_t<flags, flt>>);
}

/// The proxy is already integer-owned, so this lower leaf receives it by value
/// and feeds the established field formatter directly.  No native bfloat16
/// aggregate or floating conversion is formed at this boundary.
template <::std::integral char_type,
		  manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_scalar_supported<flags, flt> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_field_manip_t<flags, flt>>,
	char_type *iter,
	manipulators::floating_scalar_field_manip_t<flags, flt> value) noexcept
{
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
			flags.nan_show_type, flags.json_float, floating_type>(
			iter, fields.mantissa, fields.exponent, fields.sign);
	}
}

namespace details
{

template <typename flt,
		  ::fast_io::manipulators::floating_format format, bool json_float>
[[nodiscard]] inline constexpr ::std::size_t
print_floating_exact_decimal_reserve_size() noexcept
{
	using trait = ::fast_io::details::iec559_traits<flt>;
	constexpr ::std::size_t bias{
		(static_cast<::std::size_t>(1u) << (trait::ebits - 1u)) - 1u};
	/* The smallest subnormal is 2^-(bias+mbits-1).  Its fixed spelling has
	exactly denominator_power leading/fractional positions plus `0.`. */
	constexpr ::std::size_t denominator_power{bias + trait::mbits - 1u};
	constexpr ::std::size_t fixed_capacity{
		denominator_power + 2u + (json_float ? 2u : 0u)};
	/* One radix point plus `e-` and a conservative decimal exponent width.
	The digit capacity is the proved coefficient bound used by the materializer. */
	constexpr ::std::size_t scientific_capacity{
		::fast_io::details::exact_precision_digit_capacity<flt> + 16u};
	if constexpr (format ==
				  ::fast_io::manipulators::floating_format::fixed)
	{
		return fixed_capacity + 1u;
	}
	else if constexpr (format ==
					   ::fast_io::manipulators::floating_format::scientific)
	{
		return scientific_capacity + 1u;
	}
	else
	{
		return (fixed_capacity < scientific_capacity
					? scientific_capacity
					: fixed_capacity) +
			   1u;
	}
}

} // namespace details

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
	requires(::fast_io::details::print_floating_exact_decimal_supported<flags, flt>)
inline constexpr ::std::size_t print_reserve_size(
	io_reserve_type_t<char_type,
					  manipulators::exact_decimal_manip_t<flags, flt>>) noexcept
{
	return ::fast_io::details::print_floating_exact_decimal_reserve_size<
		::std::remove_cvref_t<flt>, flags.floating, flags.json_float>();
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
	requires(::fast_io::details::print_floating_exact_decimal_supported<flags, flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type,
					  manipulators::exact_decimal_manip_t<flags, flt>>,
	char_type *iter,
	manipulators::exact_decimal_manip_t<flags, flt> value) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	auto const fields{::fast_io::details::get_punned_result(value.reference)};
	return ::fast_io::details::print_floating_exact_decimal_fields_define<
		flags, floating_type>(iter, fields.mantissa, fields.exponent, fields.sign);
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_exact_decimal_supported<flags, flt> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr ::std::size_t print_reserve_size(
	io_reserve_type_t<char_type,
					  manipulators::exact_decimal_field_manip_t<flags, flt>>) noexcept
{
	return ::fast_io::details::print_floating_exact_decimal_reserve_size<
		::std::remove_cvref_t<flt>, flags.floating, flags.json_float>();
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_exact_decimal_supported<flags, flt> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type,
					  manipulators::exact_decimal_field_manip_t<flags, flt>>,
	char_type *iter,
	manipulators::exact_decimal_field_manip_t<flags, flt> value) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	auto const fields{
		::fast_io::details::floating_scalar_proxy_fields<floating_type>(
			value.representation)};
	return ::fast_io::details::print_floating_exact_decimal_fields_define<
		flags, floating_type>(iter, fields.mantissa, fields.exponent, fields.sign);
}

/* Exact sizing necessarily performs the same full expansion as emission.
Bounded print/concat destinations should therefore materialize once using the
proved ordinary bound; exact-buffer APIs can still use the precise CPO below. */
template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	io_reserve_type_t<char_type,
					  manipulators::exact_decimal_manip_t<flags, flt>>) noexcept
{
	return {};
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
inline constexpr ::std::true_type print_single_pass_bounded_direct_put_area_safe(
	io_reserve_type_t<char_type,
					  manipulators::exact_decimal_manip_t<flags, flt>>) noexcept
{
	return {};
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	io_reserve_type_t<char_type,
					  manipulators::exact_decimal_manip_t<flags, flt>>,
	manipulators::exact_decimal_manip_t<flags, flt> const &,
	::std::size_t maximum_size) noexcept
{
	auto const bound{
		::fast_io::details::print_floating_exact_decimal_reserve_size<
			::std::remove_cvref_t<flt>, flags.floating, flags.json_float>()};
	return bound <= maximum_size ? bound : SIZE_MAX;
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	io_reserve_type_t<char_type,
					  manipulators::exact_decimal_field_manip_t<flags, flt>>) noexcept
{
	return {};
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
inline constexpr ::std::true_type print_single_pass_bounded_direct_put_area_safe(
	io_reserve_type_t<char_type,
					  manipulators::exact_decimal_field_manip_t<flags, flt>>) noexcept
{
	return {};
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	io_reserve_type_t<char_type,
					  manipulators::exact_decimal_field_manip_t<flags, flt>>,
	manipulators::exact_decimal_field_manip_t<flags, flt>,
	::std::size_t maximum_size) noexcept
{
	auto const bound{
		::fast_io::details::print_floating_exact_decimal_reserve_size<
			::std::remove_cvref_t<flt>, flags.floating, flags.json_float>()};
	return bound <= maximum_size ? bound : SIZE_MAX;
}

namespace details
{

/// @brief Returns the representation-specific base of a runtime-precision floating reserve bound.
/// @details The requested precision and the grammar's final fixed allowance are deliberately excluded. Keeping this
///          constant separate lets ordinary reserve sizing retain its exact historical `(base + precision) + suffix`
///          overflow sequence while speculative concat sizing can perform the same additions non-fatally.
template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
	requires(::fast_io::details::print_floating_precision_supported<flags, flt> &&
			 ::fast_io::details::print_floating_precision_valid<flags.precision>)
inline constexpr ::std::size_t print_floating_precision_reserve_base_size() noexcept
{
	using no_cvref_t = ::std::remove_cvref_t<flt>;
	::std::size_t base_size{};
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::hexfloat)
	{
		static_assert(
			::fast_io::details::floating_precision_is_significant<flags.precision> ||
				::fast_io::details::floating_precision_is_fractional<flags.precision>,
			"fast_io hexfloat precision supports significant and fractional hexadecimal digit precision");
		using trait = ::fast_io::details::iec559_traits<flt>;
		if constexpr (::std::same_as<no_cvref_t, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
					  || ::std::same_as<no_cvref_t, __float128>
#endif
		)
		{
			if constexpr (::fast_io::details::fp_floating_point_is_float80<no_cvref_t>)
			{
				base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
					::fast_io::details::print_rsvhexfloat_size_cache<
						flags.showbase,
						typename ::fast_io::details::iec559_traits<no_cvref_t>::mantissa_type>,
					flags.nan_show_type>;
			}
			else
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
				if constexpr (sizeof(flt) > sizeof(double))
			{
				base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
					::fast_io::details::print_rsvhexfloat_size_cache<flags.showbase, __uint128_t>,
					flags.nan_show_type>;
			}
			else
#endif
				base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
					::fast_io::details::print_rsvhexfloat_size_cache<
						flags.showbase,
						typename ::fast_io::details::iec559_traits<double>::mantissa_type>,
					flags.nan_show_type>;
		}
		else
		{
			base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
				::fast_io::details::print_rsvhexfloat_size_cache<
					flags.showbase, typename trait::mantissa_type>,
				flags.nan_show_type>;
		}
		return base_size;
	}
	else
	{
		static_assert(
			::fast_io::manipulators::floating_format::general == flags.floating ||
			::fast_io::manipulators::floating_format::scientific == flags.floating ||
			::fast_io::manipulators::floating_format::fixed == flags.floating ||
			::fast_io::manipulators::floating_format::decimal == flags.floating);
		/*
		Fractional general formatting can retain the complete integral part before
		rounding on the 10^-P grid. At P=0 a max-finite binary80 value, for example,
		is a 4933-digit integer even though ordinary general formatting would choose
		scientific notation. Therefore both general and decimal fractional modes use
		the fixed reserve bound. Significant general/decimal use their native cache.
		*/
		constexpr auto reserve_floating{
			(::fast_io::details::floating_precision_is_fractional<flags.precision> &&
			 (flags.floating == ::fast_io::manipulators::floating_format::decimal ||
			  flags.floating == ::fast_io::manipulators::floating_format::general))
				? ::fast_io::manipulators::floating_format::fixed
				: flags.floating};
		if constexpr (::std::same_as<no_cvref_t, long double> &&
					  sizeof(flt) == sizeof(double))
		{
			base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
				::fast_io::details::print_rsv_cache<double, reserve_floating>,
				flags.nan_show_type>;
		}
		else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
		{
			base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
				::fast_io::details::print_rsv_cache<float, reserve_floating>,
				flags.nan_show_type>;
		}
		else if constexpr (::fast_io::details::print_floating_decimal_exact_supported<flt>)
		{
			base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
				::fast_io::details::print_rsv_cache<no_cvref_t, reserve_floating>,
				flags.nan_show_type>;
		}
		else
		{
			static_assert(::fast_io::details::print_floating_decimal_direct_supported<flt>,
						  "currently only support iec559 float32 and float64 decimal output; narrower IEC559 "
						  "formats are printed through float");
			base_size = ::fast_io::details::print_rsv_fp_size_with_special_cache<
				::fast_io::details::print_rsv_cache<no_cvref_t, reserve_floating>,
				flags.nan_show_type>;
		}
		return base_size;
	}
}

template <::fast_io::manipulators::scalar_flags flags>
inline consteval ::std::size_t print_floating_precision_reserve_suffix_size() noexcept
{
	if constexpr (flags.floating == ::fast_io::manipulators::floating_format::hexfloat &&
				  ::fast_io::details::floating_precision_is_fractional<flags.precision>)
	{
		return 9u;
	}
	else
	{
		return 8u;
	}
}

} // namespace details

/// @brief Marks a run-time precision floating scalar as profitable for bounded one-pass materialization.
/// @details Computing this scalar's exact size performs the floating conversion which emission would otherwise repeat.
///          The marker is a source capability only: print and string construction independently decide whether their
///          destination can profit from a contiguous temporary or put area.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point value_type>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>>) noexcept
{
	return {};
}

/// @brief Authorizes direct put-area emission for a run-time precision floating scalar.
/// @details A cheap bounded-size query alone says nothing about exceptions or observable output state. Print therefore
///          requires this independent opt-in together with its structural noexcept proof and the destination's
///          deferred-cursor contract.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point value_type>
inline constexpr ::std::true_type print_single_pass_bounded_direct_put_area_safe(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>>) noexcept
{
	return {};
}

/// @brief Returns the scalar's non-fatal upper bound when it fits `maximum_size`, or `SIZE_MAX` otherwise.
/// @details This definition lives with the floating formatter because it depends on representation-specific fixed
///          overhead. It never invokes the ordinary reserve-size CPO, whose overflow policy is termination.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point value_type>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>>,
	::fast_io::manipulators::scalar_manip_precision_t<flags, value_type> value,
	::std::size_t maximum_size) noexcept
{
	(void)sizeof(char_type);
	auto const base_size{
		::fast_io::details::print_floating_precision_reserve_base_size<flags, value_type>()};
	if (maximum_size < base_size || maximum_size - base_size < value.precision)
	{
		return SIZE_MAX;
	}
	auto const precision_size{base_size + value.precision};
	constexpr auto suffix_size{
		::fast_io::details::print_floating_precision_reserve_suffix_size<flags>()};
	if (maximum_size - precision_size < suffix_size)
	{
		return SIZE_MAX;
	}
	return precision_size + suffix_size;
}

/// @feature concept:runtime_precise_size
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(::fast_io::details::print_floating_precision_supported<flags, flt> &&
			 flags.floating == manipulators::floating_format::hexfloat &&
			 ::fast_io::details::print_floating_precision_valid<flags.precision>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>,
				   ::fast_io::details::floating_value_or_field_parameter_t<
					   manipulators::scalar_manip_precision_t<flags, flt>, flt>
					   f) noexcept
{
	auto const base_size{
		::fast_io::details::print_floating_precision_reserve_base_size<flags, flt>()};
	return ::fast_io::details::intrinsics::add_or_overflow_die(
		::fast_io::details::intrinsics::add_or_overflow_die(base_size, f.precision),
		::fast_io::details::print_floating_precision_reserve_suffix_size<flags>());
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(::fast_io::details::print_floating_precision_supported<flags, flt> &&
			 flags.floating == manipulators::floating_format::hexfloat &&
			 ::fast_io::details::print_floating_precision_valid<flags.precision> &&
			 !::fast_io::details::print_floating_decimal_requires_integer_transport<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>,
	char_type *iter,
	::fast_io::details::floating_value_or_field_parameter_t<
		manipulators::scalar_manip_precision_t<flags, flt>, flt>
		f) noexcept
{
	static_assert(::fast_io::details::floating_precision_is_significant<flags.precision> ||
					  ::fast_io::details::floating_precision_is_fractional<flags.precision>,
				  "fast_io hexfloat precision supports significant and fractional hexadecimal digit precision");
	if constexpr (::fast_io::details::fp_floating_point_is_ibm_double_double<
					  ::std::remove_cvref_t<flt>>)
	{
		/*
		The p=106 carrier returned by get_punned_result is the exact sum of the
		two binary64 components.  Precision rounding depends only on that dyadic
		value, so the integer-field hexadecimal planner is authoritative.  Casting
		to double would erase the low component; casting to IEEE binary128 could
		quiet an IBM signaling NaN and change its payload or floating environment.
		Using the same fields consumed by floating_precise_hex_precision_size also
		proves exact-capacity sizing and emission select one identical spelling.
		*/
		auto const fields{
			::fast_io::details::get_punned_result(f.reference)};
		return ::fast_io::details::
			compiler_constant_hex_precision_fields_runtime_define<flags>(
				iter, fields, f.precision);
	}
	else if constexpr (::fast_io::details::print_floating_requires_object_field_capture<flt>)
	{
		auto const fields{f.fields};
		return ::fast_io::details::compiler_constant_hex_precision_fields_runtime_define<
			flags>(iter, fields, f.precision);
	}
	else if constexpr (::std::same_as<::std::remove_cvref_t<flt>, long double>
#if defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)
					   || ::std::same_as<::std::remove_cvref_t<flt>, __float128>
#endif
	)
	{
		if constexpr (::fast_io::details::fp_floating_point_is_float80<::std::remove_cvref_t<flt>>)
		{
			return details::print_rsvhexfloat_precision_define_impl<
				flags.showbase, flags.uppercase_showbase, flags.showpos, flags.uppercase, flags.uppercase_e,
				flags.comma, flags.rounding, flags.precision, flags.nan_show_sign, flags.nan_show_type>(
				iter, f.reference, f.precision);
		}
		else
#if (defined(__SIZEOF_FLOAT128__) || defined(__FLOAT128__)) && defined(__SIZEOF_INT128__)
			if constexpr (sizeof(flt) > sizeof(double))
		{
			return details::print_rsvhexfloat_precision_define_impl<
				flags.showbase, flags.uppercase_showbase, flags.showpos, flags.uppercase, flags.uppercase_e,
				flags.comma, flags.rounding, flags.precision, flags.nan_show_sign, flags.nan_show_type>(
				iter, static_cast<__float128>(f.reference), f.precision);
		}
		else
#endif
			return details::print_rsvhexfloat_precision_define_impl<
				flags.showbase, flags.uppercase_showbase, flags.showpos, flags.uppercase, flags.uppercase_e,
				flags.comma, flags.rounding, flags.precision, flags.nan_show_sign, flags.nan_show_type>(
				iter, static_cast<double>(f.reference), f.precision);
	}
	else
	{
		return details::print_rsvhexfloat_precision_define_impl<
			flags.showbase, flags.uppercase_showbase, flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.rounding, flags.precision, flags.nan_show_sign, flags.nan_show_type>(iter, f.reference, f.precision);
	}
}

/// @feature concept:runtime_precise_size
template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(::fast_io::details::print_floating_precision_supported<flags, flt> &&
			 flags.floating != manipulators::floating_format::hexfloat &&
			 ::fast_io::details::print_floating_precision_valid<flags.precision>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>,
				   ::fast_io::details::floating_value_or_field_parameter_t<
					   manipulators::scalar_manip_precision_t<flags, flt>, flt>
					   f) noexcept
{
	auto const base_size{
		::fast_io::details::print_floating_precision_reserve_base_size<flags, flt>()};
	return ::fast_io::details::intrinsics::add_or_overflow_die(
		::fast_io::details::intrinsics::add_or_overflow_die(base_size, f.precision),
		::fast_io::details::print_floating_precision_reserve_suffix_size<flags>());
}

template <::std::integral char_type, manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(::fast_io::details::print_floating_precision_supported<flags, flt> &&
			 flags.floating != manipulators::floating_format::hexfloat &&
			 ::fast_io::details::print_floating_precision_valid<flags.precision> &&
			 !::fast_io::details::print_floating_decimal_requires_integer_transport<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type, manipulators::scalar_manip_precision_t<flags, flt>>,
	char_type *iter,
	::fast_io::details::floating_value_or_field_parameter_t<
		manipulators::scalar_manip_precision_t<flags, flt>, flt>
		f) noexcept
{
	static_assert(manipulators::floating_format::general == flags.floating ||
				  manipulators::floating_format::scientific == flags.floating ||
				  manipulators::floating_format::fixed == flags.floating ||
				  manipulators::floating_format::decimal == flags.floating);
	using no_cvref_t = ::std::remove_cvref_t<flt>;
	if constexpr (::fast_io::details::print_floating_requires_object_field_capture<flt>)
	{
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		auto const [mantissa, exponent, sign]{f.fields};
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::prsv_fp_nan_impl<
				flags.showpos, flags.uppercase, flags.nan_show_sign,
				flags.nan_show_type, trait::mbits>(iter, mantissa, sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_narrow_float_from_fields<no_cvref_t>(
				mantissa, exponent, sign)};
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.precision, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float>(iter, widened, f.precision);
	}
	else if constexpr (::std::same_as<no_cvref_t, long double> &&
					   sizeof(flt) == sizeof(double))
	{
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma, flags.floating, flags.precision,
			flags.rounding, flags.nan_show_sign, flags.nan_show_type, flags.json_float>(
			iter, static_cast<double>(f.reference), f.precision);
	}
	else if constexpr (::std::same_as<no_cvref_t, float>)
	{
		using trait = ::fast_io::details::iec559_traits<float>;
		auto const [mantissa, exponent, sign]{
			::fast_io::details::get_punned_result(f.reference)};
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		// Classify from the original binary32 fields.  In particular, an sNaN
		// returns here and is never passed through a floating conversion.
		if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::prsv_fp_nan_impl<
				flags.showpos, flags.uppercase, flags.nan_show_sign,
				flags.nan_show_type, trait::mbits>(iter, mantissa, sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_binary32_finite_fields_to_binary64(
				static_cast<::std::uint_least32_t>(mantissa), exponent, sign)};
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.precision, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float>(iter, widened, f.precision);
	}
	else if constexpr (::fast_io::details::print_floating_decimal_via_float<flt>)
	{
		using trait = ::fast_io::details::iec559_traits<no_cvref_t>;
		auto const [mantissa, exponent, sign]{
			::fast_io::details::get_punned_result(f.reference)};
		constexpr auto exponent_mask{
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u};
		if (exponent == static_cast<::std::uint_least32_t>(exponent_mask))
		{
			return ::fast_io::details::prsv_fp_nan_impl<
				flags.showpos, flags.uppercase, flags.nan_show_sign,
				flags.nan_show_type, trait::mbits>(iter, mantissa, sign);
		}
		auto const widened{
			::fast_io::details::dragonbox_narrow_float_from_fields<no_cvref_t>(
				mantissa, exponent, sign)};
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma, flags.floating, flags.precision,
			flags.rounding, flags.nan_show_sign, flags.nan_show_type, flags.json_float>(
			iter, widened, f.precision);
	}
	else if constexpr (::fast_io::details::print_floating_decimal_exact_supported<flt>)
	{
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.precision, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float>(iter, f.reference, f.precision);
	}
	else
	{
		static_assert(::fast_io::details::print_floating_decimal_direct_supported<flt>,
					  "currently only support iec559 float32 and float64 decimal output; narrower IEC559 "
					  "formats are printed through float");
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.precision, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float>(iter, f.reference, f.precision);
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
	requires(::fast_io::details::print_floating_precision_supported<flags, flt> &&
			 ::fast_io::details::print_floating_precision_valid<flags.precision> &&
			 ::fast_io::details::print_floating_decimal_requires_integer_transport<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type,
					  manipulators::scalar_manip_precision_t<flags, flt>>,
	char_type *iter,
	manipulators::scalar_manip_precision_t<flags, flt> const &f) noexcept
{
	using floating_type = ::std::remove_cvref_t<flt>;
	if constexpr (flags.floating ==
				  ::fast_io::manipulators::floating_format::hexfloat)
	{
		auto const fields{
			::fast_io::details::compiler_constant_floating_capture_fields<
				floating_type>(
				f.reference)};
		return ::fast_io::details::compiler_constant_hex_precision_fields_runtime_define<
			flags>(iter, fields, f.precision);
	}
	else
	{
		using trait = ::fast_io::details::iec559_traits<floating_type>;
		auto const [mantissa, exponent, sign]{
			::fast_io::details::compiler_constant_floating_capture_fields<
				floating_type>(
				f.reference)};
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
		return details::print_rsvflt_precision_define_impl<
			flags.showpos, flags.uppercase, flags.uppercase_e, flags.comma,
			flags.floating, flags.precision, flags.rounding, flags.nan_show_sign,
			flags.nan_show_type, flags.json_float>(iter, widened, f.precision);
	}
}

template <::std::integral char_type,
		  manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_precision_supported<flags, flt> &&
		::fast_io::details::print_floating_precision_valid<flags.precision> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr ::std::size_t print_reserve_size(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_field_manip_precision_t<flags, flt>>,
	manipulators::floating_scalar_field_manip_precision_t<flags, flt> value) noexcept
{
	auto const base_size{
		::fast_io::details::print_floating_precision_reserve_base_size<
			flags, flt>()};
	return ::fast_io::details::intrinsics::add_or_overflow_die(
		::fast_io::details::intrinsics::add_or_overflow_die(
			base_size, value.precision),
		::fast_io::details::print_floating_precision_reserve_suffix_size<flags>());
}

template <::std::integral char_type,
		  manipulators::scalar_flags flags, details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_precision_supported<flags, flt> &&
		::fast_io::details::print_floating_precision_valid<flags.precision> &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_field_manip_precision_t<flags, flt>>,
	char_type *iter,
	manipulators::floating_scalar_field_manip_precision_t<flags, flt> value) noexcept
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

template <::std::integral char_type,
		  manipulators::scalar_flags flags, details::my_floating_point flt>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_field_manip_precision_t<flags, flt>>) noexcept
{
	return {};
}

template <::std::integral char_type,
		  manipulators::scalar_flags flags, details::my_floating_point flt>
inline constexpr ::std::true_type print_single_pass_bounded_direct_put_area_safe(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_field_manip_precision_t<flags, flt>>) noexcept
{
	return {};
}

template <::std::integral char_type,
		  manipulators::scalar_flags flags, details::my_floating_point flt>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_field_manip_precision_t<flags, flt>>,
	manipulators::floating_scalar_field_manip_precision_t<flags, flt> value,
	::std::size_t maximum_size) noexcept
{
	(void)sizeof(char_type);
	auto const base_size{
		::fast_io::details::print_floating_precision_reserve_base_size<
			flags, flt>()};
	if (maximum_size < base_size ||
		maximum_size - base_size < value.precision)
	{
		return SIZE_MAX;
	}
	auto const precision_size{base_size + value.precision};
	constexpr auto suffix_size{
		::fast_io::details::print_floating_precision_reserve_suffix_size<flags>()};
	if (maximum_size - precision_size < suffix_size)
	{
		return SIZE_MAX;
	}
	return precision_size + suffix_size;
}

namespace details
{

struct floating_precision_range_plan
{
	::std::size_t precision{};
	bool shortest{};
	bool preserve{};
};

[[nodiscard]] inline constexpr ::std::size_t
normalize_floating_precision_range(
	::std::size_t minimum_precision,
	::std::size_t maximum_precision) noexcept
{
	if (!maximum_precision || maximum_precision < minimum_precision)
	{
		::fast_io::fast_terminate();
	}
	return minimum_precision ? minimum_precision : 1u;
}

[[nodiscard]] inline constexpr floating_precision_range_plan
make_floating_precision_range_plan(
	::std::size_t shortest_precision, ::std::size_t minimum_precision,
	::std::size_t maximum_precision) noexcept
{
	minimum_precision =
		::fast_io::details::normalize_floating_precision_range(
			minimum_precision, maximum_precision);
	if (shortest_precision < minimum_precision)
	{
		return {minimum_precision, false, true};
	}
	if (maximum_precision < shortest_precision)
	{
		return {maximum_precision, false, false};
	}
	return {shortest_precision, true, false};
}

template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::manipulators::floating_precision precision_mode,
		  ::fast_io::details::my_floating_point flt,
		  ::std::integral char_type>
inline constexpr char_type *
print_floating_precision_range_non_ascii_exact(
	char_type *iter,
	typename ::fast_io::details::iec559_traits<flt>::mantissa_type mantissa,
	::std::uint_least32_t exponent, ::std::size_t precision,
	bool negative) noexcept
{
	static_assert(flags.rounding !=
				  ::fast_io::manipulators::floating_rounding::current_environment);
	iter = ::fast_io::details::print_rsv_fp_sign_impl<flags.showpos>(
		iter, negative);
	return ::fast_io::details::print_rsvflt_exact_precision_define_impl<
		flt, flags.comma, flags.uppercase_e, flags.floating,
		precision_mode, flags.rounding, flags.json_float>(
		iter, mantissa, exponent, precision, negative);
}

template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
[[nodiscard]] inline constexpr ::std::size_t
print_floating_precision_range_reserve_size(
	::std::size_t minimum_precision,
	::std::size_t maximum_precision) noexcept
{
	(void)::fast_io::details::normalize_floating_precision_range(
		minimum_precision, maximum_precision);
	constexpr auto precision_flags{
		::fast_io::details::floating_precision_mani_flags_cache<
			flags,
			::fast_io::manipulators::floating_precision::
				significant_preserve_trailing_zero>};
	auto const base_size{
		::fast_io::details::print_floating_precision_reserve_base_size<
			precision_flags, flt>()};
	return ::fast_io::details::intrinsics::add_or_overflow_die(
		::fast_io::details::intrinsics::add_or_overflow_die(
			base_size, maximum_precision),
		::fast_io::details::
			print_floating_precision_reserve_suffix_size<precision_flags>());
}

template <::fast_io::manipulators::scalar_flags flags,
		  ::fast_io::details::my_floating_point flt>
[[nodiscard]] inline constexpr ::std::size_t
print_floating_precision_range_bounded_materialization_size(
	::std::size_t minimum_precision, ::std::size_t maximum_precision,
	::std::size_t maximum_size) noexcept
{
	(void)::fast_io::details::normalize_floating_precision_range(
		minimum_precision, maximum_precision);
	constexpr auto precision_flags{
		::fast_io::details::floating_precision_mani_flags_cache<
			flags, ::fast_io::manipulators::floating_precision::
					   significant_preserve_trailing_zero>};
	auto const base_size{
		::fast_io::details::print_floating_precision_reserve_base_size<
			precision_flags, flt>()};
	if (maximum_size < base_size ||
		maximum_size - base_size < maximum_precision)
	{
		return SIZE_MAX;
	}
	auto const precision_size{base_size + maximum_precision};
	constexpr auto suffix_size{
		::fast_io::details::print_floating_precision_reserve_suffix_size<
			precision_flags>()};
	if (maximum_size - precision_size < suffix_size)
	{
		return SIZE_MAX;
	}
	return precision_size + suffix_size;
}

} // namespace details

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_scalar_supported<flags, flt> &&
		flags.floating != manipulators::floating_format::hexfloat &&
		!::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr ::std::size_t print_reserve_size(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_precision_range_manip_t<flags, flt>>,
	manipulators::floating_scalar_precision_range_manip_t<flags, flt>
		value) noexcept
{
	return ::fast_io::details::print_floating_precision_range_reserve_size<
		flags, flt>(value.minimum_precision, value.maximum_precision);
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_scalar_supported<flags, flt> &&
		flags.floating != manipulators::floating_format::hexfloat &&
		!::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_precision_range_manip_t<flags, flt>>,
	char_type *iter,
	manipulators::floating_scalar_precision_range_manip_t<flags, flt>
		value) noexcept
{
	if constexpr (flags.rounding ==
				  manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case manipulators::floating_rounding::toward_plus_infinity:
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type,
										   decltype(manipulators::rounding<
													manipulators::floating_rounding::toward_plus_infinity>(
											   value))>,
				iter, manipulators::rounding<manipulators::floating_rounding::toward_plus_infinity>(value));
		case manipulators::floating_rounding::toward_minus_infinity:
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type,
										   decltype(manipulators::rounding<
													manipulators::floating_rounding::toward_minus_infinity>(
											   value))>,
				iter, manipulators::rounding<manipulators::floating_rounding::toward_minus_infinity>(value));
		case manipulators::floating_rounding::toward_zero:
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type,
										   decltype(manipulators::rounding<
													manipulators::floating_rounding::toward_zero>(value))>,
				iter, manipulators::rounding<manipulators::floating_rounding::toward_zero>(value));
		default:
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type,
										   decltype(manipulators::rounding<
													manipulators::floating_rounding::nearest_to_even>(value))>,
				iter, manipulators::rounding<manipulators::floating_rounding::nearest_to_even>(value));
		}
	}
	else
	{
		using floating_type = ::std::remove_cvref_t<flt>;
		using trait = ::fast_io::details::iec559_traits<floating_type>;
		(void)::fast_io::details::normalize_floating_precision_range(
			value.minimum_precision, value.maximum_precision);
		auto const fields{::fast_io::details::get_punned_result(value.reference)};
		constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
		if (fields.exponent == exponent_mask)
		{
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type,
										   manipulators::scalar_manip_t<flags, flt>>,
				iter, manipulators::scalar_manip_t<flags, flt>{value.reference});
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
				auto const decimal{
					::fast_io::details::print_floating_shortest_decimal_fields<
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
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type,
										   manipulators::scalar_manip_t<flags, flt>>,
				iter, manipulators::scalar_manip_t<flags, flt>{value.reference});
		}
		if constexpr (!::fast_io::details::is_ascii<char_type>)
		{
			/* The ordinary precision writer retains execution-charset-specific
			fast paths.  A precision range is a separate opt-in proxy, so use its
			already-required exact fallback for non-ASCII char/wchar destinations.
			This keeps every ordinary rounding instantiation and branch unchanged. */
			if (fields.mantissa != 0u || fields.exponent != 0u)
			{
				if (plan.preserve)
				{
					return ::fast_io::details::
						print_floating_precision_range_non_ascii_exact<
							flags,
							manipulators::floating_precision::
								significant_preserve_trailing_zero,
							floating_type>(iter, fields.mantissa,
										   fields.exponent, plan.precision, fields.sign);
				}
				return ::fast_io::details::
					print_floating_precision_range_non_ascii_exact<
						flags, manipulators::floating_precision::significant,
						floating_type>(iter, fields.mantissa, fields.exponent,
									   plan.precision, fields.sign);
			}
		}
		if (plan.preserve)
		{
			constexpr auto precision_flags{
				::fast_io::details::floating_precision_mani_flags_cache<
					flags, manipulators::floating_precision::
							   significant_preserve_trailing_zero>};
			using precision_type =
				manipulators::scalar_manip_precision_t<precision_flags, flt>;
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type, precision_type>, iter,
				precision_type{value.reference, plan.precision});
		}
		else
		{
			constexpr auto precision_flags{
				::fast_io::details::floating_precision_mani_flags_cache<
					flags, manipulators::floating_precision::significant>};
			using precision_type =
				manipulators::scalar_manip_precision_t<precision_flags, flt>;
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type, precision_type>, iter,
				precision_type{value.reference, plan.precision});
		}
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_scalar_supported<flags, flt> &&
		flags.floating != manipulators::floating_format::hexfloat &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr ::std::size_t print_reserve_size(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_field_precision_range_manip_t<flags, flt>>,
	manipulators::floating_scalar_field_precision_range_manip_t<flags, flt>
		value) noexcept
{
	return ::fast_io::details::print_floating_precision_range_reserve_size<
		flags, flt>(value.minimum_precision, value.maximum_precision);
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
	requires(
		::fast_io::details::print_floating_scalar_supported<flags, flt> &&
		flags.floating != manipulators::floating_format::hexfloat &&
		::fast_io::details::floating_scalar_requires_integer_proxy<flt>)
inline constexpr char_type *print_reserve_define(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_field_precision_range_manip_t<flags, flt>>,
	char_type *iter,
	manipulators::floating_scalar_field_precision_range_manip_t<flags, flt>
		value) noexcept
{
	if constexpr (flags.rounding ==
				  manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case manipulators::floating_rounding::toward_plus_infinity:
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type,
										   decltype(manipulators::rounding<
													manipulators::floating_rounding::toward_plus_infinity>(
											   value))>,
				iter, manipulators::rounding<manipulators::floating_rounding::toward_plus_infinity>(value));
		case manipulators::floating_rounding::toward_minus_infinity:
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type,
										   decltype(manipulators::rounding<
													manipulators::floating_rounding::toward_minus_infinity>(
											   value))>,
				iter, manipulators::rounding<manipulators::floating_rounding::toward_minus_infinity>(value));
		case manipulators::floating_rounding::toward_zero:
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type,
										   decltype(manipulators::rounding<
													manipulators::floating_rounding::toward_zero>(value))>,
				iter, manipulators::rounding<manipulators::floating_rounding::toward_zero>(value));
		default:
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type,
										   decltype(manipulators::rounding<
													manipulators::floating_rounding::nearest_to_even>(value))>,
				iter, manipulators::rounding<manipulators::floating_rounding::nearest_to_even>(value));
		}
	}
	else
	{
		using floating_type = ::std::remove_cvref_t<flt>;
		using trait = ::fast_io::details::iec559_traits<floating_type>;
		(void)::fast_io::details::normalize_floating_precision_range(
			value.minimum_precision, value.maximum_precision);
		auto const fields{
			::fast_io::details::floating_scalar_proxy_fields<floating_type>(
				value.representation)};
		constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
		if (fields.exponent == exponent_mask)
		{
			using scalar_type =
				manipulators::floating_scalar_field_manip_t<flags, flt>;
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type, scalar_type>, iter,
				scalar_type{value.representation});
		}
		::fast_io::details::floating_precision_range_plan plan{};
		if (fields.mantissa == 0u && fields.exponent == 0u)
		{
			plan = ::fast_io::details::make_floating_precision_range_plan(
				1u, value.minimum_precision, value.maximum_precision);
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
				value.minimum_precision, value.maximum_precision);
			if (plan.shortest)
			{
				iter = ::fast_io::details::print_rsv_fp_sign_impl<flags.showpos>(
					iter, fields.sign);
				return ::fast_io::details::print_rsvflt_decimal_define_impl<
					floating_type, flags.comma, flags.uppercase_e,
					flags.floating, flags.json_float>(
					iter, decimal.m10, decimal.e10);
			}
		}
		if (plan.shortest)
		{
			using scalar_type =
				manipulators::floating_scalar_field_manip_t<flags, flt>;
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type, scalar_type>, iter,
				scalar_type{value.representation});
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
							manipulators::floating_precision::
								significant_preserve_trailing_zero,
							floating_type>(iter, fields.mantissa,
										   fields.exponent, plan.precision, fields.sign);
				}
				return ::fast_io::details::
					print_floating_precision_range_non_ascii_exact<
						flags, manipulators::floating_precision::significant,
						floating_type>(iter, fields.mantissa, fields.exponent,
									   plan.precision, fields.sign);
			}
		}
		if (plan.preserve)
		{
			constexpr auto precision_flags{
				::fast_io::details::floating_precision_mani_flags_cache<
					flags, manipulators::floating_precision::
							   significant_preserve_trailing_zero>};
			using precision_type =
				manipulators::floating_scalar_field_manip_precision_t<
					precision_flags, flt>;
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type, precision_type>, iter,
				precision_type{value.representation, plan.precision});
		}
		else
		{
			constexpr auto precision_flags{
				::fast_io::details::floating_precision_mani_flags_cache<
					flags, manipulators::floating_precision::significant>};
			using precision_type =
				manipulators::floating_scalar_field_manip_precision_t<
					precision_flags, flt>;
			return ::fast_io::print_reserve_define(
				::fast_io::io_reserve_type<char_type, precision_type>, iter,
				precision_type{value.representation, plan.precision});
		}
	}
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_precision_range_manip_t<flags, flt>>) noexcept
{
	return {};
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
inline constexpr ::std::true_type
print_single_pass_bounded_direct_put_area_safe(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_precision_range_manip_t<flags, flt>>) noexcept
{
	return {};
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_precision_range_manip_t<flags, flt>>,
	manipulators::floating_scalar_precision_range_manip_t<flags, flt> value,
	::std::size_t maximum_size) noexcept
{
	(void)sizeof(char_type);
	return ::fast_io::details::
		print_floating_precision_range_bounded_materialization_size<flags, flt>(
			value.minimum_precision, value.maximum_precision, maximum_size);
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_field_precision_range_manip_t<flags,
																				  flt>>) noexcept
{
	return {};
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
inline constexpr ::std::true_type
print_single_pass_bounded_direct_put_area_safe(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_field_precision_range_manip_t<flags,
																				  flt>>) noexcept
{
	return {};
}

template <::std::integral char_type, manipulators::scalar_flags flags,
		  details::my_floating_point flt>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	io_reserve_type_t<char_type,
					  manipulators::floating_scalar_field_precision_range_manip_t<flags,
																				  flt>>,
	manipulators::floating_scalar_field_precision_range_manip_t<flags, flt>
		value,
	::std::size_t maximum_size) noexcept
{
	(void)sizeof(char_type);
	return ::fast_io::details::
		print_floating_precision_range_bounded_materialization_size<flags, flt>(
			value.minimum_precision, value.maximum_precision, maximum_size);
}
} // namespace fast_io

#include "precise_size.h"
#include "compiler_constant.h"
#include "../../fast_io_core_impl/charconv/float_from_chars.h"
#include "../../fast_io_core_impl/charconv/float_to_chars.h"

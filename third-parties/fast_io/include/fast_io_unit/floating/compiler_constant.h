#pragma once

namespace fast_io
{

namespace manipulators
{

/// @brief Print/concat-owned proxy for an optimizer-proven constant floating scalar.
/// @details Its formatter is deliberately independent of the ordinary floating reserve writer. The value remains in
///          the proxy so GCC/Clang can propagate it into a compact bit-to-text routine only in the proven constant arm;
///          an unknown run-time scalar never constructs this type or enters this algorithm.
template <typename char_type, scalar_flags flags, typename value_type>
struct compiler_constant_floating_scalar_manip_t
{
	using manip_tag = manip_tag_t;
	using clean_type = ::std::remove_cv_t<value_type>;
	using binary_mantissa_type =
		typename ::fast_io::details::iec559_traits<clean_type>::mantissa_type;
	using decimal_mantissa_type =
		::fast_io::details::dragonbox_decimal_mantissa_type<clean_type>;

	binary_mantissa_type binary_mantissa{};
	decimal_mantissa_type decimal_mantissa{};
	::std::uint_least32_t binary_exponent{};
	::std::int_least32_t decimal_exponent{};
	bool negative{};
};

/// @brief Print/concat-owned integer carrier for a proven constant floating
///        value and precision.
/// @details Keeping the native scalar out of this proxy preserves every
///          bfloat16 subnormal and NaN payload.  Decimal formats additionally
///          retain a proved shortest carrier when one is sufficient for the
///          requested rounding grid; an exact-fields fallback remains available
///          for every other constant admitted by the bounded protocol.
template <typename char_type, scalar_flags flags, typename value_type>
struct compiler_constant_floating_precision_manip_t
{
	using manip_tag = manip_tag_t;
	using output_char_type = char_type;
	using clean_type = ::std::remove_cv_t<value_type>;
	using floating_trait = ::fast_io::details::iec559_traits<clean_type>;
	using dragonbox_mantissa_type =
		::fast_io::details::dragonbox_decimal_mantissa_type<clean_type>;
	// A narrow shortest carrier uses uint32_t, but an exact binary16 dyadic
	// coefficient can require 17 decimal digits.  The precision-only proxy may
	// retain such a coefficient in uint64_t without changing the scalar path.
	using decimal_mantissa_type = ::std::conditional_t<
		(floating_trait::mbits <= 10u &&
		 sizeof(dragonbox_mantissa_type) < sizeof(::std::uint_least64_t)),
		::std::uint_least64_t, dragonbox_mantissa_type>;
	::fast_io::details::punning_result<clean_type> fields{};
	::std::size_t precision;
	decimal_mantissa_type decimal_mantissa{};
	::std::int_least32_t decimal_exponent{};
	bool decimal_carrier_available{};
	bool decimal_rounding_discarded{};
	// Cached while all source fields are still optimizer-local.  Prepared
	// unbuffered print reads this scalar before deciding whether the proxy must
	// escape to its large immutable-fragment fallback; that ordering lets Clang
	// eliminate the fallback and scalar-replace short constant records.
	::std::size_t materialized_size{};
};

} // namespace manipulators

namespace details
{

// Scientific, general and decimal shortest spellings are bounded by 32 code
// units.  Fixed is different: the smallest binary128 subnormal has a shortest
// fixed spelling just over five thousand code units long.  The complete
// supported IEC 60559 set (binary16, bfloat16, binary32, binary64, binary80
// and binary128) is bounded by binary128's ordinary fixed reserve cache:
//
// * sign + radix grammar + e10max + m10digits = 5,006 code units.
//
// The fixed emitter maps its long zero runs to this immutable storage, so this
// is a reserve/payload bound rather than a per-call character buffer.
inline constexpr ::std::size_t compiler_constant_floating_scalar_capacity{5006u};

template <typename floating_type>
inline constexpr bool compiler_constant_floating_type_supported =
	#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	(!::fast_io::details::fp_floating_point_is_ibm_double_double<
		 ::std::remove_cv_t<floating_type>> &&
	 (::fast_io::details::print_floating_decimal_direct_supported<
		 ::std::remove_cv_t<floating_type>> ||
	 ::fast_io::details::print_floating_decimal_exact_supported<
		 ::std::remove_cv_t<floating_type>>))
	#else
	false
#endif
	;

template <typename floating_type>
inline constexpr bool compiler_constant_floating_hex_type_supported =
	#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	(!::fast_io::details::fp_floating_point_is_ibm_double_double<
		 ::std::remove_cv_t<floating_type>> &&
	 ::fast_io::details::compiler_constant_hex_has_iec559_traits<
		 ::std::remove_cv_t<floating_type>>)
#else
	false
#endif
	;

// A constant precision request is still a run-time object as far as the C++
// type system is concerned: __builtin_constant_p cannot turn its value into a
// non-type template argument.  Inspect precision through 1024 so a compile-time
// request can be admitted when its *result* is short.  The character-specific
// proxy reserve contract is exactly the shared 256-byte materialization budget;
// a longer spelling stays on the established formatter.
inline constexpr ::std::size_t compiler_constant_decimal_precision_limit{1024u};
template <::std::integral char_type>
inline constexpr ::std::size_t compiler_constant_decimal_precision_capacity{
	::fast_io::details::compiler_constant_materialization_max_bytes /
	sizeof(char_type)};

template <::fast_io::manipulators::scalar_flags flags, typename floating_type>
inline constexpr bool compiler_constant_floating_precision_supported{
	::fast_io::details::print_floating_precision_supported<flags, floating_type> &&
	::fast_io::details::print_floating_precision_valid<flags.precision> &&
	/*
	The precision proxy captures one IEC field by bit-casting the complete object
	to `iec559_traits<T>::mantissa_type`.  IBM double-double exposes those traits
	only as a synthetic exact-value carrier: its object is the ordered pair
	(high,low).  Concatenating that pair would recover neither their exact sum nor
	the component-dependent hexadecimal normalization.  Excluding IBM here (and
	not in compiler_constant_hex_scalar_supported, whose integer-field helpers
	are also used after the native IBM adapter) proves that constant-value and
	dynamic precision calls both enter the same native-object path.
	*/
	!::fast_io::details::fp_floating_point_is_ibm_double_double<
		::std::remove_cv_t<floating_type>> &&
	flags.percentage == ::fast_io::manipulators::percentage_flag::none &&
	flags.rounding !=
		::fast_io::manipulators::floating_rounding::current_environment &&
	(flags.floating == ::fast_io::manipulators::floating_format::hexfloat
		 ? ::fast_io::details::compiler_constant_hex_precision_supported<
			   flags, floating_type>
		 : ::fast_io::details::compiler_constant_floating_type_supported<
			   floating_type>)};

template <::fast_io::manipulators::scalar_flags flags>
inline constexpr ::std::size_t compiler_constant_floating_precision_limit{
	flags.floating == ::fast_io::manipulators::floating_format::hexfloat
		? ::fast_io::details::compiler_constant_hex_precision_limit
		: ::fast_io::details::compiler_constant_decimal_precision_limit};

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
		 requires(::fast_io::details::compiler_constant_floating_precision_supported<
			 flags, floating_type>)
[[nodiscard]] inline consteval ::std::size_t
compiler_constant_floating_precision_capacity() noexcept
{
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		return ::fast_io::details::compiler_constant_hex_capacity;
	}
	else
	{
		return ::fast_io::details::
			compiler_constant_decimal_precision_capacity<char_type>;
	}
}

/// @brief Rebuilds a native scalar from already captured IEC 60559 fields.
/// @details This is a bit-copy adapter used only inside the proven-constant
///          branch.  The proxy itself continues to cross print/concat layers as
///          integers, so no native floating reference ABI is introduced.
template <typename floating_type>
[[nodiscard]] inline constexpr floating_type
compiler_constant_floating_value_from_fields(
	::fast_io::details::punning_result<floating_type> fields) noexcept
{
	using clean_type = ::std::remove_cv_t<floating_type>;
	using trait = ::fast_io::details::iec559_traits<clean_type>;
	using mantissa_type = typename trait::mantissa_type;
#if defined(__SIZEOF_FLOAT80__) ||                              \
	(defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
	 __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384)
	if constexpr (
		::fast_io::details::fp_floating_point_is_float80<clean_type> &&
		::std::endian::native == ::std::endian::little)
	{
		using storage_type = ::fast_io::details::float80_storage<
			sizeof(clean_type) - sizeof(::std::uint_least64_t) -
			sizeof(::std::uint_least16_t)>;
		storage_type storage{};
		constexpr ::std::uint_least64_t explicit_integer_bit{
			::std::uint_least64_t{1u} << 63u};
		storage.mantissa = static_cast<::std::uint_least64_t>(fields.mantissa);
		if (fields.exponent != 0u)
		{
			storage.mantissa |= explicit_integer_bit;
		}
		storage.exponent = static_cast<::std::uint_least16_t>(
			fields.exponent |
			(static_cast<::std::uint_least32_t>(static_cast<bool>(fields.sign))
			 << 15u));
		return ::fast_io::bit_cast<clean_type>(storage);
	}
	else
#endif
	{
		static_assert(sizeof(clean_type) == sizeof(mantissa_type));
		auto const bits{static_cast<mantissa_type>(
			fields.mantissa |
			(static_cast<mantissa_type>(fields.exponent) << trait::mbits) |
			(static_cast<mantissa_type>(static_cast<bool>(fields.sign))
			 << (trait::mbits + trait::ebits)))};
		return ::fast_io::bit_cast<clean_type>(bits);
	}
}

template <::fast_io::manipulators::scalar_flags flags, typename floating_type,
	::std::integral char_type>
		 requires(
			flags.floating != ::fast_io::manipulators::floating_format::hexfloat &&
			::fast_io::details::compiler_constant_floating_precision_supported<
				flags, floating_type>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
compiler_constant_floating_decimal_precision_fields_define(
	char_type *iter,
	::fast_io::details::punning_result<floating_type> fields,
	::std::size_t precision) noexcept
{
	using clean_type = ::std::remove_cv_t<floating_type>;
	using trait = ::fast_io::details::iec559_traits<clean_type>;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
	if (fields.exponent == exponent_mask)
	{
		return ::fast_io::details::prsv_fp_nan_impl<
			flags.showpos, flags.uppercase, flags.nan_show_sign,
			flags.nan_show_type, trait::mbits>(
				iter, fields.mantissa, static_cast<bool>(fields.sign));
	}
	iter = ::fast_io::details::print_rsv_fp_sign_impl<flags.showpos>(
		iter, static_cast<bool>(fields.sign));
	if (fields.mantissa == 0u && fields.exponent == 0u)
	{
		*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
		if constexpr (flags.floating ==
			::fast_io::manipulators::floating_format::scientific)
		{
			auto fractional_digits{precision};
			if constexpr (::fast_io::details::
				floating_precision_is_significant<flags.precision>)
			{
				fractional_digits = precision ? precision - 1u : 0u;
			}
			if constexpr (::fast_io::details::
				floating_precision_preserves_trailing_zero<flags.precision>)
			{
				if (fractional_digits)
				{
					*iter++ = ::fast_io::char_literal_v<
						(flags.comma ? u8',' : u8'.'), char_type>;
					iter = ::fast_io::details::fill_zeros_impl(
						iter, fractional_digits);
				}
			}
			return ::fast_io::details::print_rsv_fp_e_impl<
				clean_type, flags.uppercase_e>(iter, 0);
		}
		else if constexpr (::fast_io::details::
			floating_precision_is_fractional<flags.precision>)
		{
			if constexpr (flags.json_float)
			{
				if (!precision || !::fast_io::details::
					floating_precision_preserves_trailing_zero<flags.precision>)
				{
					return ::fast_io::details::
						print_rsv_fp_append_json_float_zero<flags.comma>(iter);
				}
			}
			if constexpr (::fast_io::details::
				floating_precision_preserves_trailing_zero<flags.precision>)
			{
				return ::fast_io::details::print_rsv_fp_append_point_zeros<
					flags.comma>(iter, precision);
			}
			return iter;
		}
		else if constexpr (flags.precision ==
			::fast_io::manipulators::floating_precision::
				significant_preserve_trailing_zero)
		{
			if constexpr (flags.json_float)
			{
				if (precision <= 1u)
				{
					return ::fast_io::details::
						print_rsv_fp_append_json_float_zero<flags.comma>(iter);
				}
			}
			return ::fast_io::details::print_rsv_fp_append_point_zeros<
				flags.comma>(iter, precision ? precision - 1u : 0u);
		}
		else
		{
			if constexpr (flags.json_float)
			{
				return ::fast_io::details::
					print_rsv_fp_append_json_float_zero<flags.comma>(iter);
			}
			return iter;
		}
	}
	return ::fast_io::details::print_rsvflt_exact_precision_body_impl<
		clean_type, flags.comma, flags.uppercase_e, flags.floating,
		flags.precision, flags.rounding, flags.json_float>(
			iter, fields.mantissa, fields.exponent, precision,
			static_cast<bool>(fields.sign));
}

template <::fast_io::manipulators::scalar_flags flags, typename floating_type>
		 requires(
			flags.floating != ::fast_io::manipulators::floating_format::hexfloat &&
			::fast_io::details::compiler_constant_floating_precision_supported<
				flags, floating_type>)
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::size_t
compiler_constant_floating_decimal_precision_fields_size(
	::fast_io::details::punning_result<floating_type> fields,
	::std::size_t precision) noexcept
{
	using clean_type = ::std::remove_cv_t<floating_type>;
	using trait = ::fast_io::details::iec559_traits<clean_type>;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
	if (fields.exponent == exponent_mask)
	{
		return ::fast_io::details::floating_precise_special_size<
			flags.showpos, flags.nan_show_sign, flags.nan_show_type,
			trait::mbits>(fields.mantissa, static_cast<bool>(fields.sign));
	}
	auto const sign_size{
		::fast_io::details::floating_precise_sign_size<flags.showpos>(
			static_cast<bool>(fields.sign))};
	if (fields.mantissa == 0u && fields.exponent == 0u)
	{
		return ::fast_io::details::floating_precise_add(
			sign_size,
			::fast_io::details::floating_precise_precision_zero_body_size<
				clean_type, flags.precision, flags.json_float>(
					flags.floating, precision));
	}
	constexpr bool fractional{
		::fast_io::details::floating_precision_is_fractional<flags.precision>};
	constexpr bool preserve{
		::fast_io::details::floating_precision_preserves_trailing_zero<
			flags.precision>};
	constexpr auto int32_max{
		(::std::numeric_limits<::std::int_least32_t>::max)()};
	auto decimal{::fast_io::details::exact_precision_from_binary<clean_type>(
		fields.mantissa, fields.exponent)};
	auto const real_exponent{
		decimal.exponent + static_cast<::std::int_least32_t>(decimal.size) - 1};
	::std::size_t significant{};
	::std::int_least32_t keep{};
	if constexpr (fractional)
	{
		if constexpr (flags.floating ==
			::fast_io::manipulators::floating_format::scientific)
		{
			significant = ::fast_io::details::exact_precision_saturating_add(
				precision, 1u);
			keep = significant > static_cast<::std::size_t>(int32_max)
				? int32_max
				: static_cast<::std::int_least32_t>(significant);
		}
		else if (precision > static_cast<::std::size_t>(int32_max))
		{
			keep = int32_max;
			significant = static_cast<::std::size_t>(keep);
		}
		else
		{
			auto const requested_keep{
				static_cast<::std::int_least64_t>(real_exponent) + 1 +
				static_cast<::std::int_least64_t>(precision)};
			keep = int32_max < requested_keep
				? int32_max
				: static_cast<::std::int_least32_t>(requested_keep);
			significant = keep < 0 ? 0u : static_cast<::std::size_t>(keep);
		}
	}
	else
	{
		significant = precision ? precision : 1u;
		keep = significant > static_cast<::std::size_t>(int32_max)
			? int32_max
			: static_cast<::std::int_least32_t>(significant);
	}
	auto const rounded{
		static_cast<::std::int_least32_t>(decimal.size) > keep};
	::fast_io::details::exact_precision_round<flags.rounding>(
		decimal, keep, static_cast<bool>(fields.sign));
	if constexpr (!preserve)
	{
		::fast_io::details::exact_precision_trim(decimal);
	}
	if constexpr (fractional && preserve &&
		flags.floating == ::fast_io::manipulators::floating_format::general)
	{
		significant = rounded
			? ::fast_io::details::
				exact_precision_fractional_general_rounded_virtual_size(
					decimal, precision)
			: decimal.size;
	}
	auto const body_size{
		::fast_io::details::floating_precise_rounded_precision_size<
			clean_type, flags.floating, flags.precision, flags.json_float>(
				decimal, precision, significant)};
	return ::fast_io::details::floating_precise_add(sign_size, body_size);
}

[[nodiscard]] inline constexpr ::std::size_t
compiler_constant_floating_unsigned_digits(::std::uint_least32_t value) noexcept
{
	::std::size_t digits{1u};
	while (10u <= value)
	{
		value /= 10u;
		++digits;
	}
	return digits;
}

template <::std::integral char_type>
inline constexpr char_type *
compiler_constant_floating_write_unsigned(
	char_type *iter, ::std::uint_least32_t value,
	::std::size_t minimum_digits = 1u) noexcept
{
	auto size{
		::fast_io::details::compiler_constant_floating_unsigned_digits(value)};
	if (size < minimum_digits)
	{
		size = minimum_digits;
	}
	// Constant floating proxies overwhelmingly emit one- and two-digit
	// exponents.  Spell those cases forward so GCC can turn an optimizer-proven
	// value into direct stores instead of retaining a one-iteration backwards
	// loop.  `char_literal_add` preserves non-ASCII execution character sets.
	if (size == 1u)
	{
		*iter++ = ::fast_io::char_literal_add<char_type>(value % 10u);
		return iter;
	}
	if (size == 2u)
	{
		*iter++ = ::fast_io::char_literal_add<char_type>((value / 10u) % 10u);
		*iter++ = ::fast_io::char_literal_add<char_type>(value % 10u);
		return iter;
	}
	auto *const end{iter + size};
	for (auto *current{end}; current != iter;)
	{
		*--current = ::fast_io::char_literal_add<char_type>(value % 10u);
		value /= 10u;
	}
	return end;
}

template <typename unsigned_type>
[[nodiscard]] inline constexpr ::std::size_t
compiler_constant_floating_decimal_digits(unsigned_type value) noexcept;

// Constant-materialization digit leaf. A pinned 32-callsite deletion A/B made
// forced placement 336/288 text bytes smaller on GCC 11/12 with unchanged
// callers. GCC 13 reversed that result, and GCC 14--16 grew by 22--25 KiB by
// cloning this digit loop instead of sharing it. Clang 17 and 21--23 were
// identical; Clang 18--20 also favored ordinary placement by 11--12 KiB. Every
// unknown-value wrapper was instruction-identical. The closed GNU upper bound
// therefore records a measured GCC 13 reversal, not an assumed future policy.
// GCC 11 is the oldest tested GNU endpoint; no older frontend inherits this
// placement rule without a new measurement.
template <::std::integral char_type, typename unsigned_type>
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__ && __GNUC__ < 13
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr char_type *
compiler_constant_floating_write_decimal_digits(
	char_type *iter, unsigned_type value) noexcept
{
	auto const size{
		::fast_io::details::compiler_constant_floating_decimal_digits(value)};
	if (size == 1u)
	{
		*iter++ = ::fast_io::char_literal_add<char_type>(
			static_cast<::std::uint_least32_t>(value));
		return iter;
	}
	if (size == 2u)
	{
		auto const quotient{static_cast<unsigned_type>(value / 10u)};
		*iter++ = ::fast_io::char_literal_add<char_type>(
			static_cast<::std::uint_least32_t>(quotient));
		*iter++ = ::fast_io::char_literal_add<char_type>(
			static_cast<::std::uint_least32_t>(
				value - quotient * 10u));
		return iter;
	}
	auto *const end{iter + size};
	for (auto *current{end}; current != iter;)
	{
		auto const quotient{static_cast<unsigned_type>(value / 10u)};
		auto const remainder{static_cast<::std::uint_least32_t>(
			value - quotient * 10u)};
		*--current = ::fast_io::char_literal_add<char_type>(remainder);
		value = quotient;
	}
	return end;
}

// Constant-proxy digit leaf. Removing this placement together with the two
// length leaves below makes GCC 15/Clang 23 retain decimal helper calls in the
// `i=3.2` literal path (0x3f/0x4d-byte callers become 0x210/0x26b-class
// dispatchers). No run-time integer or floating formatter calls this helper.
template <::std::integral char_type, typename unsigned_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
compiler_constant_floating_write_decimal_digits_exact(
	char_type *iter, unsigned_type value, ::std::size_t size) noexcept

{
	if (size == 1u)
	{
		*iter++ = ::fast_io::char_literal_add<char_type>(
			static_cast<::std::uint_least32_t>(value % 10u));
		return iter;
	}
	if (size == 2u)
	{
		auto const quotient{static_cast<unsigned_type>(value / 10u)};
		*iter++ = ::fast_io::char_literal_add<char_type>(
			static_cast<::std::uint_least32_t>(quotient % 10u));
		*iter++ = ::fast_io::char_literal_add<char_type>(
			static_cast<::std::uint_least32_t>(
				value - quotient * 10u));
		return iter;
	}
	auto *const end{iter + size};
	for (auto *current{end}; current != iter;)
	{
		auto const quotient{static_cast<unsigned_type>(value / 10u)};
		auto const remainder{static_cast<::std::uint_least32_t>(
			value - quotient * 10u)};
		*--current = ::fast_io::char_literal_add<char_type>(remainder);
		value = quotient;
	}
	return end;
}

// The u64 threshold tree is a constant-carrier leaf. At -O3 an ordinary
// `inline` leaves an outlined length query in both tested compilers; forcing
// only this bounded tree is required for the digit chain above to fold.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::size_t
compiler_constant_floating_decimal_digits_u64(
	::std::uint_least64_t input) noexcept
{
	// Keep this detector local to the compiler-constant carrier. Calling the
	// ordinary integer-length CPO here made Clang outline that otherwise tiny
	// boundary, while the old division loop prevented GCC from proving the
	// compact-output capacity. The converter-shaped tree folds immediately for
	// a known carrier and remains bounded if a proxy is inspected at run time.
	if (input < UINT64_C(100000000))
	{
		if (input < UINT64_C(10000))
		{
			if (input < UINT64_C(100))
			{
				return 1u + static_cast<::std::size_t>(input >= UINT64_C(10));
			}
			return 3u + static_cast<::std::size_t>(input >= UINT64_C(1000));
		}
		if (input < UINT64_C(1000000))
		{
			return 5u + static_cast<::std::size_t>(input >= UINT64_C(100000));
		}
		return 7u + static_cast<::std::size_t>(input >= UINT64_C(10000000));
	}
	if (input < UINT64_C(1000000000000))
	{
		if (input < UINT64_C(10000000000))
		{
			return 9u + static_cast<::std::size_t>(input >= UINT64_C(1000000000));
		}
		return 11u + static_cast<::std::size_t>(input >= UINT64_C(100000000000));
	}
	if (input < UINT64_C(10000000000000000))
	{
		if (input < UINT64_C(100000000000000))
		{
			return 13u + static_cast<::std::size_t>(input >= UINT64_C(10000000000000));
		}
		return 15u + static_cast<::std::size_t>(input >= UINT64_C(1000000000000000));
	}
	if (input < UINT64_C(1000000000000000000))
	{
		return 17u + static_cast<::std::size_t>(input >= UINT64_C(100000000000000000));
	}
	return 19u + static_cast<::std::size_t>(input >= UINT64_C(10000000000000000000));
}

// This adapter must expose the u64/u128 split to the constant-proxy caller.
// A/B removal, with the writers still forced, retains the same helper call and
// prevents the literal scalar from reaching its 0x3f/0x4d-byte terminal form.
template <typename unsigned_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::size_t
compiler_constant_floating_decimal_digits(unsigned_type value) noexcept
{
	if constexpr (sizeof(unsigned_type) <= sizeof(::std::uint_least64_t))
	{
		return ::fast_io::details::compiler_constant_floating_decimal_digits_u64(
			static_cast<::std::uint_least64_t>(value));
	}
#if defined(__SIZEOF_INT128__)
	else if constexpr (sizeof(unsigned_type) <= sizeof(__uint128_t))
	{
		constexpr __uint128_t ten_to_19{UINT64_C(10000000000000000000)};
		constexpr __uint128_t ten_to_20{ten_to_19 * 10u};
		if (value < ten_to_19)
		{
			return ::fast_io::details::compiler_constant_floating_decimal_digits_u64(
				static_cast<::std::uint_least64_t>(value));
		}
		if (value < ten_to_20)
		{
			return 20u;
		}
		return 20u +
			::fast_io::details::compiler_constant_floating_decimal_digits_u64(
				static_cast<::std::uint_least64_t>(
					static_cast<__uint128_t>(value) / ten_to_20));
	}
#endif
	else
	{
		::std::size_t size{1u};
		while (10u <= value)
		{
			value = static_cast<unsigned_type>(value / 10u);
			++size;
		}
		return size;
	}
}

template <typename unsigned_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr unsigned_type
compiler_constant_floating_power_of_ten(::std::size_t exponent) noexcept
{
	// Keep the binary32/binary64 carrier range branch-shaped.  GCC 13 can
	// propagate the constant decimal carrier through the proxy but does not
	// collapse the counted multiply loop below; the surviving loop in turn
	// prevents its following quotient/remainder stores from becoming immediate
	// bytes.  A switch has identical constexpr semantics and lets that older
	// optimizer select the divisor before lowering the digit writer.  Wider
	// carriers and deliberately out-of-range precision plans retain the generic
	// overflow-equivalent loop.  In A/B assembly, adding the cases reduced the
	// GCC 13 field-only probe from 0xdd to 0x5d bytes; this local always-inline
	// placement is also required because Clang otherwise outlines the enlarged
	// switch.  Neither helper is reachable from the ordinary run-time ftoa path.
	switch (exponent)
	{
	case 0u: return static_cast<unsigned_type>(UINT64_C(1));
	case 1u: return static_cast<unsigned_type>(UINT64_C(10));
	case 2u: return static_cast<unsigned_type>(UINT64_C(100));
	case 3u: return static_cast<unsigned_type>(UINT64_C(1000));
	case 4u: return static_cast<unsigned_type>(UINT64_C(10000));
	case 5u: return static_cast<unsigned_type>(UINT64_C(100000));
	case 6u: return static_cast<unsigned_type>(UINT64_C(1000000));
	case 7u: return static_cast<unsigned_type>(UINT64_C(10000000));
	case 8u: return static_cast<unsigned_type>(UINT64_C(100000000));
	case 9u: return static_cast<unsigned_type>(UINT64_C(1000000000));
	case 10u: return static_cast<unsigned_type>(UINT64_C(10000000000));
	case 11u: return static_cast<unsigned_type>(UINT64_C(100000000000));
	case 12u: return static_cast<unsigned_type>(UINT64_C(1000000000000));
	case 13u: return static_cast<unsigned_type>(UINT64_C(10000000000000));
	case 14u: return static_cast<unsigned_type>(UINT64_C(100000000000000));
	case 15u: return static_cast<unsigned_type>(UINT64_C(1000000000000000));
	case 16u: return static_cast<unsigned_type>(UINT64_C(10000000000000000));
	case 17u: return static_cast<unsigned_type>(UINT64_C(100000000000000000));
	case 18u: return static_cast<unsigned_type>(UINT64_C(1000000000000000000));
	case 19u: return static_cast<unsigned_type>(UINT64_C(10000000000000000000));
	default: break;
	}
	unsigned_type value{1u};
	while (exponent != 0u)
	{
		value = static_cast<unsigned_type>(value * 10u);
		--exponent;
	}
	return value;
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type>
inline constexpr char_type *
compiler_constant_floating_write_sign(
	char_type *iter, bool negative) noexcept
{
	if (negative)
	{
		*iter++ = ::fast_io::char_literal_v<u8'-', char_type>;
	}
	else if constexpr (flags.showpos)
	{
		*iter++ = ::fast_io::char_literal_v<u8'+', char_type>;
	}
	return iter;
}

template <::fast_io::manipulators::scalar_flags flags>
[[nodiscard]] inline constexpr ::std::size_t
compiler_constant_floating_sign_size(bool negative) noexcept
{
	return static_cast<::std::size_t>(negative || flags.showpos);
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::size_t mantissa_bits, ::std::integral char_type,
	typename mantissa_type>
inline constexpr char_type *
compiler_constant_floating_write_special(
	char_type *iter, mantissa_type mantissa, bool negative) noexcept
{
	return ::fast_io::details::prsv_fp_nan_impl<
		flags.showpos, flags.uppercase, flags.nan_show_sign,
		flags.nan_show_type, mantissa_bits>(iter, mantissa, negative);
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::size_t mantissa_bits, typename mantissa_type>
[[nodiscard]] inline constexpr ::std::size_t
compiler_constant_floating_special_size(
	mantissa_type mantissa, bool negative) noexcept
{
	if (mantissa == 0u)
	{
		return ::fast_io::details::compiler_constant_floating_sign_size<flags>(
			negative) + 3u;
	}
	::std::size_t size{3u};
	if constexpr (flags.nan_show_sign)
	{
		size += ::fast_io::details::compiler_constant_floating_sign_size<flags>(
			negative);
	}
	if constexpr (flags.nan_show_type)
	{
		constexpr mantissa_type quiet_bit{
			::fast_io::details::fp_quiet_nan_mantissa_mask<
				mantissa_type, mantissa_bits>()};
		if (negative && mantissa == quiet_bit)
		{
			size += 5u;
		}
		else if (::fast_io::details::fp_nan_is_signaling<
				mantissa_type, mantissa_bits>(mantissa))
		{
			size += 6u;
		}
	}
	return size;
}

template <::std::size_t minimum_digits, bool hexadecimal, bool uppercase,
	::std::integral char_type>
inline constexpr char_type *
compiler_constant_floating_write_exponent(
	char_type *iter, ::std::int_least32_t exponent) noexcept
{
	if constexpr (hexadecimal)
	{
		*iter++ = uppercase ? ::fast_io::char_literal_v<u8'P', char_type>
							 : ::fast_io::char_literal_v<u8'p', char_type>;
	}
	else
	{
		*iter++ = uppercase ? ::fast_io::char_literal_v<u8'E', char_type>
							 : ::fast_io::char_literal_v<u8'e', char_type>;
	}
	bool const negative{exponent < 0};
	*iter++ = negative ? ::fast_io::char_literal_v<u8'-', char_type>
					   : ::fast_io::char_literal_v<u8'+', char_type>;
	auto const magnitude{static_cast<::std::uint_least32_t>(
		negative ? -static_cast<::std::int_least64_t>(exponent) : exponent)};
	return ::fast_io::details::compiler_constant_floating_write_unsigned(
		iter, magnitude, minimum_digits);
}

template <bool uppercase, ::std::integral char_type>
[[nodiscard]] inline constexpr char_type
compiler_constant_floating_hex_digit(::std::uint_least32_t nibble) noexcept
{
	if (nibble < 10u)
	{
		return ::fast_io::char_literal_add<char_type>(nibble);
	}
	switch (nibble)
	{
	case 10u:
		return uppercase ? ::fast_io::char_literal_v<u8'A', char_type>
						 : ::fast_io::char_literal_v<u8'a', char_type>;
	case 11u:
		return uppercase ? ::fast_io::char_literal_v<u8'B', char_type>
						 : ::fast_io::char_literal_v<u8'b', char_type>;
	case 12u:
		return uppercase ? ::fast_io::char_literal_v<u8'C', char_type>
						 : ::fast_io::char_literal_v<u8'c', char_type>;
	case 13u:
		return uppercase ? ::fast_io::char_literal_v<u8'D', char_type>
						 : ::fast_io::char_literal_v<u8'd', char_type>;
	case 14u:
		return uppercase ? ::fast_io::char_literal_v<u8'E', char_type>
						 : ::fast_io::char_literal_v<u8'e', char_type>;
	default:
		return uppercase ? ::fast_io::char_literal_v<u8'F', char_type>
						 : ::fast_io::char_literal_v<u8'f', char_type>;
	}
}

[[nodiscard]] inline constexpr ::std::size_t
compiler_constant_floating_exponent_size(
	::std::int_least32_t exponent, ::std::size_t minimum_digits) noexcept
{
	auto const magnitude{static_cast<::std::uint_least32_t>(
		exponent < 0 ? -static_cast<::std::int_least64_t>(exponent) : exponent)};
	auto digits{
		::fast_io::details::compiler_constant_floating_unsigned_digits(magnitude)};
	if (digits < minimum_digits)
	{
		digits = minimum_digits;
	}
	return 2u + digits;
}

template <::fast_io::manipulators::scalar_flags flags, typename unsigned_type>
// GCC 13 outlines this exact-size leaf after the scalar carrier has already
// been materialized.  The resulting call hides the five-byte `i=3.2` extent
// from the geometric whole-record selector and leaves four scratch tiers in
// the caller (0x261 bytes versus 0x67 after forcing this leaf in the GCC 13
// probe).  This helper is reachable only from compiler-constant proxy sizing;
// ordinary ftoa has a disjoint size path.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::size_t
compiler_constant_floating_fixed_size(
	unsigned_type mantissa, ::std::int_least32_t exponent) noexcept
{
	auto const digits{static_cast<::std::int_least32_t>(
		::fast_io::details::compiler_constant_floating_decimal_digits(mantissa))};
	auto const real_exponent{static_cast<::std::int_least32_t>(
		exponent + digits - 1)};
	::std::size_t size;
	if (digits <= real_exponent)
	{
		size = static_cast<::std::size_t>(real_exponent + 1);
		if constexpr (flags.json_float)
		{
			size += 2u;
		}
	}
	else if (0 <= real_exponent)
	{
		size = static_cast<::std::size_t>(digits + 1);
		if (digits == real_exponent + 1)
		{
			--size;
			if constexpr (flags.json_float)
			{
				size += 2u;
			}
		}
	}
	else
	{
		size = static_cast<::std::size_t>(-real_exponent) +
			static_cast<::std::size_t>(digits) + 1u;
	}
	return size;
}

template <::fast_io::manipulators::scalar_flags flags, typename floating_type,
	typename unsigned_type>
[[nodiscard]] inline constexpr ::std::size_t
compiler_constant_floating_scientific_size(
	unsigned_type mantissa, ::std::int_least32_t exponent) noexcept
{
	auto const digits{static_cast<::std::int_least32_t>(
		::fast_io::details::compiler_constant_floating_decimal_digits(mantissa))};
	auto const real_exponent{static_cast<::std::int_least32_t>(
		exponent + digits - 1)};
	return static_cast<::std::size_t>(digits == 1 ? 1 : digits + 1) +
		::fast_io::details::compiler_constant_floating_exponent_size(
			real_exponent, 2u);
}

// Constant-proxy notation selector. GCC 15 and Clang 23 otherwise outline the
// decision and block complete folding of an ordinary literal scalar; the
// dynamic ftoa notation selector is a different function and ABI.
template <::fast_io::manipulators::scalar_flags flags, typename unsigned_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool
compiler_constant_floating_uses_fixed(
	unsigned_type mantissa, ::std::int_least32_t exponent) noexcept
{
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::fixed)
	{
		return true;
	}
	else if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::scientific)
	{
		return false;
	}
	else if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::general)
	{
		/*
		The constant proxy stores the same canonical (M,e) carrier as runtime
		Dragonbox.  If M has L digits, both paths therefore have the identical
		scientific exponent X=e+L-1.  Applying -4<=X<6 here proves that constant
		materialization cannot select a different notation merely because the
		optimizer exposed M and e at compile time.
		*/
		auto const digits{static_cast<::std::int_least32_t>(
			::fast_io::details::
				compiler_constant_floating_decimal_digits(mantissa))};
		auto const scientific_exponent{
			static_cast<::std::int_least32_t>(
				exponent + digits - 1)};
		return -4 <= scientific_exponent &&
			scientific_exponent < 6;
	}
	else
	{
		auto const digits{
			::fast_io::details::compiler_constant_floating_decimal_digits(mantissa)};
		auto const real_exponent{static_cast<::std::int_least32_t>(
			exponent + static_cast<::std::int_least32_t>(digits) - 1)};
		::std::size_t fixed_length{};
		if (static_cast<::std::int_least32_t>(digits) <= real_exponent)
		{
			fixed_length = static_cast<::std::size_t>(real_exponent + 1);
		}
		else if (0 <= real_exponent)
		{
			fixed_length = digits + 2u;
			if (static_cast<::std::int_least32_t>(digits) == real_exponent + 1)
			{
				--fixed_length;
			}
		}
		else
		{
			fixed_length = static_cast<::std::size_t>(-real_exponent) +
				digits + 1u;
		}
		auto const scientific_length{
			::fast_io::details::print_rsv_fp_scientific_length(
				real_exponent, digits)};
		return scientific_length >= fixed_length;
	}
}

// Constant-proxy fixed writer. Removing this three-writer placement group
// changes the GCC 15 literal caller from 0x3f bytes to 0xf1 bytes and the
// Clang 23 caller from 0x4d to 0x87, both with residual calls.
template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type, typename unsigned_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
compiler_constant_floating_write_fixed(
	char_type *iter, unsigned_type mantissa,
	::std::int_least32_t exponent) noexcept
{
	auto const size{static_cast<::std::int_least32_t>(
		::fast_io::details::compiler_constant_floating_decimal_digits(mantissa))};
	auto const real_exponent{static_cast<::std::int_least32_t>(
		exponent + size - 1)};
	if (size <= real_exponent)
	{
		iter = ::fast_io::details::
			compiler_constant_floating_write_decimal_digits(iter, mantissa);
		for (auto count{static_cast<::std::size_t>(real_exponent + 1 - size)};
			 count != 0u; --count)
		{
			*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
		}
		if constexpr (flags.json_float)
		{
			*iter++ = ::fast_io::char_literal_v<(flags.comma ? u8',' : u8'.'), char_type>;
			*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
		}
	}
	else if (0 <= real_exponent)
	{
		auto const integral_size{static_cast<::std::size_t>(real_exponent + 1)};
		auto const fractional_size{
			static_cast<::std::size_t>(size) - integral_size};
		if (fractional_size != 0u)
		{
			auto const divisor{
				::fast_io::details::compiler_constant_floating_power_of_ten<
					unsigned_type>(fractional_size)};
			iter = ::fast_io::details::
				compiler_constant_floating_write_decimal_digits_exact(
					iter, static_cast<unsigned_type>(mantissa / divisor),
					integral_size);
			*iter++ = ::fast_io::char_literal_v<(flags.comma ? u8',' : u8'.'), char_type>;
			iter = ::fast_io::details::
				compiler_constant_floating_write_decimal_digits_exact(
					iter, static_cast<unsigned_type>(mantissa % divisor),
					fractional_size);
		}
		else
		{
			iter = ::fast_io::details::
				compiler_constant_floating_write_decimal_digits_exact(
					iter, mantissa, integral_size);
			if constexpr (flags.json_float)
			{
				*iter++ = ::fast_io::char_literal_v<(flags.comma ? u8',' : u8'.'), char_type>;
				*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
			}
		}
	}
	else
	{
		*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
		*iter++ = ::fast_io::char_literal_v<(flags.comma ? u8',' : u8'.'), char_type>;
		for (auto count{static_cast<::std::size_t>(-real_exponent - 1)};
			 count != 0u; --count)
		{
			*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
		}
		iter = ::fast_io::details::
			compiler_constant_floating_write_decimal_digits(iter, mantissa);
	}
	return iter;
}

// Constant-proxy scientific writer; it shares the measured three-writer A/B
// above and never changes placement of the ordinary scientific ftoa writer.
template <::fast_io::manipulators::scalar_flags flags, typename floating_type,
	::std::integral char_type, typename unsigned_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
compiler_constant_floating_write_scientific(
	char_type *iter, unsigned_type mantissa,
	::std::int_least32_t exponent) noexcept
{
	auto const size{
		::fast_io::details::compiler_constant_floating_decimal_digits(mantissa)};
	auto const divisor{
		::fast_io::details::compiler_constant_floating_power_of_ten<unsigned_type>(
			size - 1u)};
	*iter++ = ::fast_io::char_literal_add<char_type>(
		static_cast<::std::uint_least32_t>(mantissa / divisor));
	if (1u < size)
	{
		*iter++ = ::fast_io::char_literal_v<(flags.comma ? u8',' : u8'.'), char_type>;
		iter = ::fast_io::details::
			compiler_constant_floating_write_decimal_digits_exact(
				iter, static_cast<unsigned_type>(mantissa % divisor), size - 1u);
	}
	auto const real_exponent{static_cast<::std::int_least32_t>(
		exponent + static_cast<::std::int_least32_t>(size) - 1)};
	return ::fast_io::details::compiler_constant_floating_write_exponent<
		2u, false,
		flags.uppercase_e>(iter, real_exponent);
}

template <::fast_io::manipulators::scalar_flags flags,
	typename floating_type>
[[nodiscard]] inline constexpr ::std::size_t
compiler_constant_floating_hex_size(
	typename ::fast_io::details::iec559_traits<floating_type>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr ::std::uint_least32_t exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	if (exponent == exponent_mask)
	{
		return ::fast_io::details::compiler_constant_floating_special_size<
			flags, trait::mbits>(mantissa, negative);
	}
	constexpr ::std::size_t nibble_count{(trait::mbits + 3u) / 4u};
	constexpr ::std::size_t padding_bits{nibble_count * 4u - trait::mbits};
	auto aligned{static_cast<typename trait::mantissa_type>(mantissa << padding_bits)};
	::std::size_t fractional_digits{nibble_count};
	while (fractional_digits != 0u && (aligned & 0x0fu) == 0u)
	{
		aligned = static_cast<typename trait::mantissa_type>(aligned >> 4u);
		--fractional_digits;
	}
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u)};
	auto const binary_exponent{exponent == 0u && mantissa == 0u ? 0
		: exponent == 0u ? 1 - bias
		: static_cast<::std::int_least32_t>(exponent) - bias};
	return ::fast_io::details::compiler_constant_floating_sign_size<flags>(negative) +
		static_cast<::std::size_t>(flags.showbase ? 2u : 0u) + 1u +
		static_cast<::std::size_t>(fractional_digits != 0u) + fractional_digits +
		::fast_io::details::compiler_constant_floating_exponent_size(
			binary_exponent, 1u);
}

// Constant-proxy hexadecimal writer. Clang 23 specifically changes the direct
// literal hex symbol from 0x50 to 0x68 bytes and emits an out-of-line call when
// this attribute alone is removed; the native hexfloat ABI stays by value.
template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type, typename floating_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
compiler_constant_floating_write_hex(
	char_type *iter,
	typename ::fast_io::details::iec559_traits<floating_type>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr ::std::uint_least32_t exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	if (exponent == exponent_mask)
	{
		return ::fast_io::details::compiler_constant_floating_write_special<
			flags, trait::mbits>(iter, mantissa, negative);
	}
	iter = ::fast_io::details::compiler_constant_floating_write_sign<flags>(
		iter, negative);
	if constexpr (flags.showbase)
	{
		*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
		*iter++ = flags.uppercase_showbase
			? ::fast_io::char_literal_v<u8'X', char_type>
			: ::fast_io::char_literal_v<u8'x', char_type>;
	}
	*iter++ = ::fast_io::char_literal_add<char_type>(exponent == 0u ? 0u : 1u);
	constexpr ::std::size_t nibble_count{(trait::mbits + 3u) / 4u};
	constexpr ::std::size_t padding_bits{nibble_count * 4u - trait::mbits};
	auto aligned{static_cast<typename trait::mantissa_type>(mantissa << padding_bits)};
	::std::size_t fractional_digits{nibble_count};
	while (fractional_digits != 0u && (aligned & 0x0fu) == 0u)
	{
		aligned = static_cast<typename trait::mantissa_type>(aligned >> 4u);
		--fractional_digits;
	}
	if (fractional_digits != 0u)
	{
		*iter++ = ::fast_io::char_literal_v<(flags.comma ? u8',' : u8'.'), char_type>;
		for (::std::size_t index{fractional_digits}; index != 0u; --index)
		{
			auto const nibble{static_cast<::std::uint_least32_t>(
				(aligned >> ((index - 1u) * 4u)) & 0x0fu)};
			*iter++ = ::fast_io::details::compiler_constant_floating_hex_digit<
				flags.uppercase, char_type>(nibble);
		}
	}
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u)};
	auto const binary_exponent{exponent == 0u && mantissa == 0u ? 0
		: exponent == 0u ? 1 - bias
		: static_cast<::std::int_least32_t>(exponent) - bias};
	if (mantissa == 0u && exponent == 0u)
	{
		return ::fast_io::details::compiler_constant_floating_write_exponent<
			1u, true, flags.uppercase>(iter, binary_exponent);
	}
	return ::fast_io::details::compiler_constant_floating_write_exponent<
		1u, true, flags.uppercase_e>(iter, binary_exponent);
}

/// @brief Constant-arm copy of Dragonbox's public policy composition.
/// @details The arithmetic kernels remain the ordinary library implementation;
///          only their call sites are forced into this compiler-constant arm.
///          This prevents the optimizer from outlining a conversion after the
///          value-level gate has already proved the source constant, without
///          changing any ordinary run-time floating specialization.
template <typename decimal_type>
inline constexpr auto
compiler_constant_floating_trim_decimal(
	::fast_io::details::m10_result<decimal_type> value) noexcept
{
	// A failed wide-shortest probe is represented by a zero coefficient.  Keep
	// that sentinel canonical instead of entering the trailing-zero loop; all
	// successful carriers still remove every representation-only decimal zero.
	while (value.m10 != 0u && value.m10 % 10u == 0u)
	{
		value.m10 = static_cast<decimal_type>(value.m10 / 10u);
		++value.e10;
	}
	return value;
}

// Integer-field conversion boundary for the constant proxy. Without forced
// inlining GCC 15 leaves a 0x1cb-byte literal dispatcher and Clang 23 expands
// the same caller beyond 13 KiB. Ordinary runtime ftoa calls its established
// DA/Dragonbox entries directly and cannot enter this boundary.
template <typename floating_type,
	::fast_io::manipulators::floating_rounding rounding>
#if __has_cpp_attribute(__gnu__::__const__)
[[__gnu__::__const__]]
#endif
[[nodiscard]] inline constexpr auto
compiler_constant_floating_narrow_to_decimal(
	typename ::fast_io::details::iec559_traits<floating_type>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
	using decimal_type =
		::fast_io::details::dragonbox_decimal_mantissa_type<floating_type>;
	using result_type = ::fast_io::details::m10_result<decimal_type>;
	if constexpr (rounding ==
		::fast_io::manipulators::floating_rounding::nearest_to_even)
	{
		auto const decimal{
			::fast_io::details::dragonbox_narrow_shortest_lookup<floating_type>(
				mantissa, static_cast<::std::int_least32_t>(exponent))};
		return result_type{decimal.m10, decimal.e10};
	}
	else
	{
		constexpr bool bfloat16{
			::fast_io::details::iec559_traits<floating_type>::mbits == 7u &&
			::fast_io::details::iec559_traits<floating_type>::ebits == 8u};
		if constexpr (!bfloat16)
		{
			auto direct{::fast_io::details::dragonbox_impl<
				floating_type, rounding>(mantissa,
					static_cast<::std::int_least32_t>(exponent), negative)};
			if (direct.m10 &&
				::fast_io::details::dragonbox_decimal_printable_roundtrips_to<
					floating_type, rounding>(direct.m10, direct.e10, mantissa,
						static_cast<::std::int_least32_t>(exponent), negative))
			{
				return direct;
			}
		}
		else if (exponent < static_cast<::std::uint_least32_t>(
			::fast_io::details::dragonbox_bfloat16_high_fallback_min_exponent))
		{
			auto direct{::fast_io::details::dragonbox_impl<
				floating_type, rounding>(mantissa,
					static_cast<::std::int_least32_t>(exponent), negative)};
			if (direct.m10 &&
				::fast_io::details::dragonbox_decimal_printable_roundtrips_to<
					floating_type, rounding>(direct.m10, direct.e10, mantissa,
						static_cast<::std::int_least32_t>(exponent), negative))
			{
				return direct;
			}
		}

		// This is the body of the ordinary narrow fallback, kept in the
		// compiler-constant layer so its deliberately cold/noinline run-time
		// entry does not survive after all fields are optimizer constants.
		auto const widened{
			::fast_io::details::dragonbox_narrow_float_punned<floating_type>(
				mantissa, static_cast<::std::int_least32_t>(exponent), negative)};
		auto converted{::fast_io::details::dragonbox_impl<float, rounding>(
			widened.mantissa,
			static_cast<::std::int_least32_t>(widened.exponent), widened.sign)};
		::fast_io::details::dragonbox_shorten_decimal_to_target<
			floating_type, rounding>(converted.m10, converted.e10, mantissa,
				static_cast<::std::int_least32_t>(exponent), negative);
		return result_type{converted.m10, converted.e10};
	}
}

template <typename floating_type,
	::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr auto
compiler_constant_floating_dragonbox_main(
	typename ::fast_io::details::iec559_traits<floating_type>::mantissa_type mantissa,
	::std::int_least32_t exponent, bool negative) noexcept
{
	if constexpr (::fast_io::details::floating_rounding_is_nearest<rounding>)
	{
		return ::fast_io::details::dragonbox_main_nearest_policy<
			floating_type, rounding>(mantissa, exponent, negative);
	}
	else if constexpr (rounding ==
		::fast_io::manipulators::floating_rounding::toward_zero)
	{
		return ::fast_io::details::dragonbox_main_directed<floating_type, false>(
			mantissa, exponent);
	}
	else if constexpr (rounding ==
		::fast_io::manipulators::floating_rounding::away_from_zero)
	{
		return ::fast_io::details::dragonbox_main_directed<floating_type, true>(
			mantissa, exponent);
	}
	else
	{
		constexpr bool positive_rounds_up{
			rounding == ::fast_io::manipulators::floating_rounding::toward_plus_infinity};
		bool const right_closed{negative != positive_rounds_up};
		if (right_closed)
		{
			return ::fast_io::details::dragonbox_main_directed<floating_type, true>(
				mantissa, exponent);
		}
		return ::fast_io::details::dragonbox_main_directed<floating_type, false>(
			mantissa, exponent);
	}
}

template <typename floating_type,
	::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool
compiler_constant_floating_decimal_roundtrips_to(
	::fast_io::details::dragonbox_decimal_mantissa_type<floating_type> decimal_mantissa,
	::std::int_least32_t decimal_exponent,
	typename ::fast_io::details::iec559_traits<floating_type>::mantissa_type binary_mantissa,
	::std::int_least32_t binary_exponent, bool negative) noexcept
{
	::fast_io::details::dragonbox_decimal_adjusted_mantissa adjusted;
	bool const converted{[&]() constexpr noexcept {
		return ::fast_io::details::dragonbox_decimal_compute_adjusted<
			floating_type, rounding>(decimal_exponent,
				static_cast<::std::uint_least64_t>(decimal_mantissa), negative,
				adjusted);
	}()};
	return converted && adjusted.mantissa == binary_mantissa &&
		adjusted.power2 == binary_exponent;
}

template <typename floating_type,
	::fast_io::manipulators::floating_rounding rounding>
[[nodiscard]] inline constexpr bool
compiler_constant_floating_decimal_printable_roundtrips_to(
	::fast_io::details::dragonbox_decimal_mantissa_type<floating_type> decimal_mantissa,
	::std::int_least32_t decimal_exponent,
	typename ::fast_io::details::iec559_traits<floating_type>::mantissa_type binary_mantissa,
	::std::int_least32_t binary_exponent, bool negative) noexcept
{
	::std::uint_least64_t decimal_mantissa_limit{1u};
	for (::std::uint_least32_t index{};
		 index != ::fast_io::details::iec559_traits<floating_type>::m10digits;
		 ++index)
	{
		decimal_mantissa_limit *= 10u;
	}
	return decimal_mantissa != 0u &&
		static_cast<::std::uint_least64_t>(decimal_mantissa) <
			decimal_mantissa_limit &&
		::fast_io::details::
			compiler_constant_floating_decimal_roundtrips_to<
				floating_type, rounding>(decimal_mantissa, decimal_exponent,
					binary_mantissa, binary_exponent, negative);
}

template <typename floating_type,
	::fast_io::manipulators::floating_rounding rounding>
inline constexpr bool
compiler_constant_floating_correct_extend(
	::fast_io::details::dragonbox_decimal_mantissa_type<floating_type> base,
	::std::int_least32_t exponent,
	::fast_io::details::dragonbox_decimal_mantissa_type<floating_type> &mantissa,
	::std::int_least32_t &decimal_exponent,
	typename ::fast_io::details::iec559_traits<floating_type>::mantissa_type binary_mantissa,
	::std::int_least32_t binary_exponent, bool negative) noexcept
{
	using decimal_type =
		::fast_io::details::dragonbox_decimal_mantissa_type<floating_type>;
	decimal_type add_limit{1u};
	for (::std::uint_least32_t extension{}; extension != 3u; ++extension)
	{
		auto const next_base{static_cast<decimal_type>(base * 10u)};
		if (next_base / 10u != base)
		{
			break;
		}
		base = next_base;
		add_limit = static_cast<decimal_type>(add_limit * 10u);
		--exponent;
		for (decimal_type add{}; add != add_limit; ++add)
		{
			auto const candidate{static_cast<decimal_type>(base + add)};
			if (::fast_io::details::
				compiler_constant_floating_decimal_printable_roundtrips_to<
					floating_type, rounding>(candidate, exponent, binary_mantissa,
						binary_exponent, negative))
			{
				mantissa = candidate;
				decimal_exponent = exponent;
				return true;
			}
		}
		for (decimal_type sub{1u}; sub != add_limit && sub <= base; ++sub)
		{
			auto const candidate{static_cast<decimal_type>(base - sub)};
			if (::fast_io::details::
				compiler_constant_floating_decimal_printable_roundtrips_to<
					floating_type, rounding>(candidate, exponent, binary_mantissa,
						binary_exponent, negative))
			{
				mantissa = candidate;
				decimal_exponent = exponent;
				return true;
			}
		}
	}
	return false;
}

template <typename floating_type,
	::fast_io::manipulators::floating_rounding rounding>
inline constexpr void
compiler_constant_floating_correct(
	::fast_io::details::dragonbox_decimal_mantissa_type<floating_type> &mantissa,
	::std::int_least32_t &decimal_exponent,
	typename ::fast_io::details::iec559_traits<floating_type>::mantissa_type binary_mantissa,
	::std::int_least32_t binary_exponent, bool negative) noexcept
{
	using decimal_type =
		::fast_io::details::dragonbox_decimal_mantissa_type<floating_type>;
	auto roundtrips = [&](decimal_type candidate,
		::std::int_least32_t exponent) constexpr noexcept {
		return ::fast_io::details::
			compiler_constant_floating_decimal_printable_roundtrips_to<
				floating_type, rounding>(candidate, exponent, binary_mantissa,
					binary_exponent, negative);
	};
	if (roundtrips(mantissa, decimal_exponent))
	{
		return;
	}
	auto const next{static_cast<decimal_type>(mantissa + 1u)};
	if (roundtrips(next, decimal_exponent))
	{
		mantissa = next;
		return;
	}
	if (mantissa != 0u)
	{
		auto const previous{static_cast<decimal_type>(mantissa - 1u)};
		if (roundtrips(previous, decimal_exponent))
		{
			mantissa = previous;
			return;
		}
	}
	auto const nearest_untrimmed{[&]() constexpr noexcept {
		return ::fast_io::details::dragonbox_main<floating_type>(
			binary_mantissa, binary_exponent);
	}()};
	auto const nearest{
		::fast_io::details::compiler_constant_floating_trim_decimal(
			nearest_untrimmed)};
	if (roundtrips(nearest.m10, nearest.e10))
	{
		mantissa = nearest.m10;
		decimal_exponent = nearest.e10;
		return;
	}
	auto const nearest_next{static_cast<decimal_type>(nearest.m10 + 1u)};
	if (roundtrips(nearest_next, nearest.e10))
	{
		mantissa = nearest_next;
		decimal_exponent = nearest.e10;
		return;
	}
	if (nearest.m10 != 0u)
	{
		auto const nearest_previous{
			static_cast<decimal_type>(nearest.m10 - 1u)};
		if (roundtrips(nearest_previous, nearest.e10))
		{
			mantissa = nearest_previous;
			decimal_exponent = nearest.e10;
			return;
		}
	}
	if (::fast_io::details::compiler_constant_floating_correct_extend<
			floating_type, rounding>(nearest.m10, nearest.e10, mantissa,
				decimal_exponent, binary_mantissa, binary_exponent, negative))
	{
		return;
	}
	(void)::fast_io::details::compiler_constant_floating_correct_extend<
		floating_type, rounding>(mantissa, decimal_exponent, mantissa,
			decimal_exponent, binary_mantissa, binary_exponent, negative);
}

template <typename floating_type,
	::fast_io::manipulators::floating_rounding rounding>
#if __has_cpp_attribute(__gnu__::__const__)
[[__gnu__::__const__]]
#endif
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr auto
compiler_constant_floating_to_decimal(
	typename ::fast_io::details::iec559_traits<floating_type>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
	if constexpr (
		::fast_io::details::dragonbox_uses_binary32_core<floating_type> &&
		sizeof(floating_type) < sizeof(float))
	{
		return ::fast_io::details::compiler_constant_floating_narrow_to_decimal<
			floating_type, rounding>(mantissa, exponent, negative);
	}
	else if constexpr (rounding ==
			::fast_io::manipulators::floating_rounding::nearest_to_even &&
		((::fast_io::details::iec559_traits<floating_type>::mbits == 23u &&
		  ::fast_io::details::iec559_traits<floating_type>::ebits == 8u) ||
		 (::fast_io::details::iec559_traits<floating_type>::mbits == 52u &&
		  ::fast_io::details::iec559_traits<floating_type>::ebits == 11u)))
	{
		// Preserve the ordinary DA carrier exactly.  Trailing zero removal is
		// performed by the small local loop in the materializer below instead
		// of DA's run-time-tuned rtz helper, which need not inline in a literal
		// fixed-format call even though every operand is optimizer-constant. This branch admits only binary32/binary64,
		// whose raw exponent fields fit exactly in DA's signed exponent parameter.
		return ::fast_io::details::da::to_decimal<floating_type>(
			mantissa, static_cast<::std::int_least32_t>(exponent));
	}
	else
	{
		auto converted{
			::fast_io::details::compiler_constant_floating_dragonbox_main<
				floating_type, rounding>(mantissa,
					static_cast<::std::int_least32_t>(exponent), negative)};
		auto trimmed{
			::fast_io::details::compiler_constant_floating_trim_decimal(converted)};
		::fast_io::details::compiler_constant_floating_correct<
			floating_type, rounding>(trimmed.m10, trimmed.e10, mantissa,
				static_cast<::std::int_least32_t>(exponent), negative);
		return ::fast_io::details::compiler_constant_floating_trim_decimal(trimmed);
	}
}

// Owns constant-proxy construction only. A/B removal grows the GCC 15 literal
// caller from 0x3f to 0x1cb bytes and Clang 23 from 0x4d to more than 14 KiB;
// dynamic values fail the upper builtin-constant gate before this call exists.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr auto
compiler_constant_floating_scalar_materialize(floating_type const &value) noexcept
{
	// This routine reads through `value`.  GNU `const` would incorrectly claim
	// that the result is independent of pointed-to storage (the reference is
	// transported as a pointer at the ABI boundary), allowing calls across a
	// store to be commoned.  Keep the optimizer-visible implementation inline;
	// do not attach a pointer-insensitive function attribute here.
	using clean_type = ::std::remove_cv_t<floating_type>;
	using trait = ::fast_io::details::iec559_traits<clean_type>;
	using result_type =
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			char_type, flags, clean_type>;
	auto const [mantissa, exponent, negative]{
		::fast_io::details::compiler_constant_floating_capture_fields<clean_type>(
			value)};
	result_type result{
		.binary_mantissa = mantissa,
		.binary_exponent = exponent,
		.negative = static_cast<bool>(negative)};
	if constexpr (flags.floating !=
		::fast_io::manipulators::floating_format::hexfloat)
	{
		constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
		if (exponent != exponent_mask && (mantissa != 0u || exponent != 0u))
		{
#if defined(__SIZEOF_INT128__)
			if constexpr (
				::fast_io::details::print_floating_decimal_exact_supported<clean_type>)
			{
				auto decimal{
					::fast_io::details::wide_shortest_from_binary<
						clean_type, flags.rounding>(
							mantissa, exponent, negative)};
				if (decimal.success)
				{
					result.decimal_mantissa = decimal.m10;
					result.decimal_exponent = decimal.e10;
				}
			}
			else
#endif
			{
				auto decimal{
					::fast_io::details::compiler_constant_floating_to_decimal<
						clean_type, flags.rounding>(mantissa, exponent, negative)};
				while (decimal.m10 % 10u == 0u)
				{
					decimal.m10 = static_cast<decltype(decimal.m10)>(
						decimal.m10 / 10u);
					++decimal.e10;
				}
				result.decimal_mantissa = decimal.m10;
				result.decimal_exponent = decimal.e10;
			}
		}
	}
	return result;
}

template <typename floating_type>
using compiler_constant_floating_precision_mantissa_type =
	::std::conditional_t<
		(::fast_io::details::iec559_traits<floating_type>::mbits <= 10u &&
		 sizeof(::fast_io::details::dragonbox_decimal_mantissa_type<
			 floating_type>) < sizeof(::std::uint_least64_t)),
		::std::uint_least64_t,
		::fast_io::details::dragonbox_decimal_mantissa_type<floating_type>>;

template <typename floating_type>
struct compiler_constant_floating_decimal_precision_plan
{
	using decimal_mantissa_type =
		::fast_io::details::
			compiler_constant_floating_precision_mantissa_type<floating_type>;
	decimal_mantissa_type mantissa{};
	::std::int_least32_t exponent{};
	bool success{};
	bool rounding_discarded{};
};

/// @brief Selects the standard numeric-limits owner for a representation-equivalent IEC 559 scalar.
/// @details GCC may normalize ordinary `float`/`double` manipulators to the distinct `_Float32`/`_Float64` language
///          types. In C++20, libstdc++ reports `numeric_limits<_Float64>::digits10 == 0` even though the IEC layout is
///          binary64, which made the constant precision planner reject a valid carrier on GCC 13. Only the two proved
///          binary32/binary64 layouts borrow the canonical limits metadata; all other formats retain their own type.
template <typename floating_type>
using compiler_constant_floating_limits_type = ::std::conditional_t<
	(::fast_io::details::iec559_traits<floating_type>::mbits == 23u &&
	 ::fast_io::details::iec559_traits<floating_type>::ebits == 8u),
	float,
	::std::conditional_t<
		(::fast_io::details::iec559_traits<floating_type>::mbits == 52u &&
		 ::fast_io::details::iec559_traits<floating_type>::ebits == 11u),
		double, floating_type>>;

/// @brief Stores every power of five representable by the constant carrier proof.
/// @details `5^27` is the largest power representable by uint64_t and `5^55`
///          is the largest one representable by uint128.  Constructing the table
///          in constant evaluation removes a value-dependent multiply/divide
///          loop from the optimizer query.  An out-of-domain exponent is rejected
///          conservatively by the caller, so no overflowing entry is formed.
template <typename unsigned_type>
struct compiler_constant_floating_power5_table
{
	inline static constexpr ::std::size_t maximum_exponent{
		sizeof(unsigned_type) <= sizeof(::std::uint_least64_t) ? 27u : 55u};
	unsigned_type values[maximum_exponent + 1u]{};

	inline constexpr compiler_constant_floating_power5_table() noexcept
	{
		values[0] = static_cast<unsigned_type>(1u);
		for (::std::size_t index{1u}; index != maximum_exponent + 1u; ++index)
		{
			values[index] = static_cast<unsigned_type>(
				values[index - 1u] * static_cast<unsigned_type>(5u));
		}
	}

	[[nodiscard]] inline constexpr unsigned_type operator[](
		::std::size_t index) const noexcept
	{
		return values[index];
	}
};

template <typename unsigned_type>
inline constexpr compiler_constant_floating_power5_table<unsigned_type>
	compiler_constant_floating_power5_values{};

/// @brief Proves that a shortest decimal carrier is the exact binary value.
/// @details The run-time predicate intentionally stops at uint64_t because its
///          callers need only binary32/binary64.  Constant precision also owns
///          binary80/binary128, so this integer-only counterpart widens the
///          same factorization proof to native uint128 when available.  It may
///          conservatively reject on a power-of-five overflow.
template <typename floating_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool
compiler_constant_floating_decimal_carrier_is_binary_exact(
	typename ::fast_io::details::iec559_traits<floating_type>::mantissa_type
		binary_mantissa,
	::std::uint_least32_t raw_exponent,
	::fast_io::details::compiler_constant_floating_precision_mantissa_type<
		floating_type>
		decimal_mantissa,
	::std::int_least32_t decimal_exponent) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using binary_mantissa_type = typename trait::mantissa_type;
	using decimal_mantissa_type =
		::fast_io::details::compiler_constant_floating_precision_mantissa_type<
			floating_type>;
#if defined(__SIZEOF_INT128__)
	using work_type = ::std::conditional_t<
		(sizeof(binary_mantissa_type) > sizeof(::std::uint_least64_t) ||
		 sizeof(decimal_mantissa_type) > sizeof(::std::uint_least64_t)),
		__uint128_t, ::std::uint_least64_t>;
#else
	using work_type = ::std::uint_least64_t;
	if constexpr (sizeof(binary_mantissa_type) > sizeof(work_type) ||
				  sizeof(decimal_mantissa_type) > sizeof(work_type))
	{
		return false;
	}
#endif
	constexpr ::std::int_least32_t bias{
		(static_cast<::std::int_least32_t>(1u) << (trait::ebits - 1u)) - 1};
	work_type binary{static_cast<work_type>(binary_mantissa)};
	::std::int_least32_t binary_exponent{};
	if (raw_exponent)
	{
		binary |= static_cast<work_type>(1u) << trait::mbits;
		binary_exponent = static_cast<::std::int_least32_t>(raw_exponent) -
			bias - static_cast<::std::int_least32_t>(trait::mbits);
	}
	else
	{
		binary_exponent = 1 - bias -
			static_cast<::std::int_least32_t>(trait::mbits);
	}
	if (!binary || !decimal_mantissa)
	{
		return binary == 0u && decimal_mantissa == 0u;
	}

	work_type decimal{static_cast<work_type>(decimal_mantissa)};
	auto decimal_binary_exponent{decimal_exponent};
	constexpr auto power5_maximum{
		::fast_io::details::compiler_constant_floating_power5_table<
			work_type>::maximum_exponent};
	::std::size_t power5_exponent{};
	if (decimal_exponent < 0)
	{
		// Check the signed range before negation, including INT_MIN.  A larger
		// reciprocal power cannot be represented by the proof's work integer and
		// is therefore a conservative miss rather than an approximate decision.
		if (decimal_exponent <
			-static_cast<::std::int_least32_t>(power5_maximum))
		{
			return false;
		}
		power5_exponent = static_cast<::std::size_t>(-decimal_exponent);
		auto const divisor{
			::fast_io::details::compiler_constant_floating_power5_values<
				work_type>[power5_exponent]};
		if (decimal % divisor)
		{
			return false;
		}
		decimal = static_cast<work_type>(decimal / divisor);
	}
	else
	{
		if (static_cast<::std::uint_least32_t>(decimal_exponent) >
			power5_maximum)
		{
			return false;
		}
		power5_exponent = static_cast<::std::size_t>(decimal_exponent);
		auto const multiplier{
			::fast_io::details::compiler_constant_floating_power5_values<
				work_type>[power5_exponent]};
		constexpr work_type maximum{static_cast<work_type>(~work_type{})};
		if (maximum / multiplier < decimal)
		{
			return false;
		}
		decimal = static_cast<work_type>(decimal * multiplier);
	}
	auto const decimal_binary_zeroes{static_cast<::std::int_least32_t>(
		::fast_io::details::my_countr_zero_unchecked(decimal))};
	decimal = static_cast<work_type>(decimal >> decimal_binary_zeroes);
	decimal_binary_exponent += decimal_binary_zeroes;
	auto const binary_zeroes{static_cast<::std::int_least32_t>(
		::fast_io::details::my_countr_zero_unchecked(binary))};
	binary = static_cast<work_type>(binary >> binary_zeroes);
	binary_exponent += binary_zeroes;
	return decimal == binary && decimal_binary_exponent == binary_exponent;
}

/// @brief Produces the canonical nearest decimal carrier used by the
///        compiler-constant precision planner.
/// @details This leaf is not part of runtime ftoa. An earlier aggregate deletion A/B was
///          neutral on GCC 11/12, while GCC 13--16 saved 1.2--14 KiB and avoided
///          GCC 15/16 width-wrapper expansions from 44/54 to 2,909/2,489
///          instructions. Clang 17--20 saved 62--64 KiB and Clang 21--23 saved
///          about 5.2 KiB. The stricter constant/runtime symbol audit then showed
///          that ordinary placement leaves this call in GCC 11/12's literal
///          precision function; expose it there as well. Every unknown-value
///          wrapper is rejected before this constant-only leaf.
template <typename floating_type>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 17 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr auto
compiler_constant_floating_nearest_decimal_from_fields(
	::fast_io::details::punning_result<floating_type> fields) noexcept
{
	using decimal_type =
		::fast_io::details::dragonbox_decimal_mantissa_type<floating_type>;
	using result_type = ::fast_io::details::m10_result<decimal_type>;
#if defined(__SIZEOF_INT128__)
	if constexpr (
		::fast_io::details::print_floating_decimal_exact_supported<floating_type>)
	{
		auto const wide{::fast_io::details::wide_shortest_from_binary<
			floating_type,
			::fast_io::manipulators::floating_rounding::nearest_to_even>(
				fields.mantissa, fields.exponent,
				static_cast<bool>(fields.sign))};
		return ::fast_io::details::compiler_constant_floating_trim_decimal(
			result_type{wide.success ? static_cast<decimal_type>(wide.m10)
									  : decimal_type{},
				wide.success ? wide.e10 : 0});
	}
	else
#endif
	{
		auto const decimal{
			::fast_io::details::compiler_constant_floating_to_decimal<
				floating_type,
				::fast_io::manipulators::floating_rounding::nearest_to_even>(
					fields.mantissa, fields.exponent,
					static_cast<bool>(fields.sign))};
		// DA deliberately returns a fixed-width carrier with trailing zeroes.
		// Runtime ftoa trims it before precision dispatch; the integer-field
		// constant protocol must establish the same canonical invariant before
		// deciding whether an exact carrier needs guard digits.
		return ::fast_io::details::compiler_constant_floating_trim_decimal(
			result_type{decimal.m10, decimal.e10});
	}
}

/// @brief Checks whether a decimal carrier covers the requested rounding grid.
/// @details This predicate is used only by compiler-constant precision
///          planning. In a pinned 16-callsite deletion A/B, forcing this leaf
///          reduced GCC 13--16 text by 304/320/320/768 bytes by eliminating
///          out-of-line grid/exact-carrier clones. The earlier aggregate probe
///          let GCC 11/12 grow by 64/96 bytes because their caller still failed
///          the complete constant gate; after fixing that gate, a strict symbol
///          audit identifies this remaining call as the next opaque boundary.
///          Clang 21--23 also need this first
///          constant-only leaf exposed before the companion carrier-size leaf
///          can eliminate unavailable precision branches. Unknown values are
///          rejected before the planner is formed, so this marker cannot expand
///          the native runtime ftoa arm.
template <typename floating_type>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr bool
compiler_constant_floating_carrier_grid_supported(
	::fast_io::details::compiler_constant_floating_precision_mantissa_type<
		floating_type> mantissa,
	::std::int_least32_t exponent, ::std::size_t requested,
	::std::size_t precision, bool fractional_grid) noexcept
{
	auto const length{::fast_io::details::
		compiler_constant_floating_decimal_digits(mantissa)};
	if (!fractional_grid)
	{
		return requested >= length || length - requested < 20u;
	}
	if (0 <= exponent ||
		precision >= static_cast<::std::size_t>(
			-static_cast<::std::int_least64_t>(exponent)))
	{
		return true;
	}
	auto const cut{static_cast<::std::int_least64_t>(
		-static_cast<::std::int_least64_t>(precision)) - exponent};
	return cut < 20;
}

#if defined(__clang__)
/// @brief Tries the normal-value decimal-grid proof used by Clang's literal
///        precision gate.
/// @details This is only the `length <= requested <= digits10` sufficient
///          condition; ambiguous, subnormal, directed-rounding and long-
///          precision values return an empty plan to the complete planner.
///          Keeping the precheck separate avoids cloning any exact-window
///          algorithm into the caller.  The precheck deliberately stops at
///          binary64: evaluating a second wide-shortest carrier for binary80
///          or binary128 can exhaust Clang's constexpr step budget, while the
///          complete planner already owns the wide-format fallback. In a pinned
///          Clang 17--23 deletion matrix this marker saved 83--85 KiB on 17--20
///          and about 5.2 KiB on 21--23; unknown wrappers were instruction-
///          identical. The positive policy remains future-open.
template <::fast_io::manipulators::scalar_flags flags, typename floating_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr auto
compiler_constant_floating_try_normal_decimal_grid_plan(
	::fast_io::details::punning_result<floating_type> fields,
	::std::size_t precision) noexcept
{
	using result_type =
		::fast_io::details::compiler_constant_floating_decimal_precision_plan<
			floating_type>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	if constexpr (!::fast_io::details::floating_rounding_is_nearest<
			flags.rounding> ||
		11u < trait::ebits || 52u < trait::mbits)
	{
		return result_type{};
	}
	else
	{
		constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
		if (!fields.exponent || fields.exponent == exponent_mask)
		{
			return result_type{};
		}
		auto const carrier{::fast_io::details::
			compiler_constant_floating_nearest_decimal_from_fields(fields)};
		if (!carrier.m10)
		{
			return result_type{};
		}
		auto const length{::fast_io::details::
			compiler_constant_floating_decimal_digits(carrier.m10)};
		auto const real_exponent{
			carrier.e10 + static_cast<::std::int_least32_t>(length) - 1};
		constexpr bool fractional{
			::fast_io::details::floating_precision_is_fractional<flags.precision>};
		constexpr bool fractional_grid{
			fractional && flags.floating !=
				::fast_io::manipulators::floating_format::scientific};
		::std::size_t requested{};
		if constexpr (fractional)
		{
			if constexpr (flags.floating ==
				::fast_io::manipulators::floating_format::scientific)
			{
				requested = ::fast_io::details::exact_precision_saturating_add(
					precision, 1u);
			}
			else if (0 <= real_exponent)
			{
				requested = ::fast_io::details::exact_precision_saturating_add(
					::fast_io::details::exact_precision_saturating_add(
						static_cast<::std::size_t>(real_exponent), precision),
					1u);
			}
			else
			{
				auto const leading_zeroes{static_cast<::std::size_t>(
					-static_cast<::std::int_least64_t>(real_exponent))};
				requested = leading_zeroes <= precision
					? precision - leading_zeroes + 1u
					: 0u;
			}
		}
		else
		{
			requested = precision ? precision : 1u;
		}
		if (requested < length ||
			static_cast<::std::size_t>((::std::numeric_limits<
				::fast_io::details::compiler_constant_floating_limits_type<
					floating_type>>::digits10)) < requested ||
			!::fast_io::details::
				compiler_constant_floating_carrier_grid_supported<floating_type>(
					carrier.m10, carrier.e10, requested, precision,
					fractional_grid))
		{
			return result_type{};
		}
		if constexpr (flags.floating ==
				::fast_io::manipulators::floating_format::general &&
			flags.precision == ::fast_io::manipulators::floating_precision::
				fractional_preserve_trailing_zero)
		{
			if (length != requested)
			{
				return result_type{};
			}
		}
		auto const rounding_discarded{[&]() constexpr noexcept {
			if constexpr (fractional_grid)
			{
				return carrier.e10 < 0 &&
					precision < static_cast<::std::size_t>(
						-static_cast<::std::int_least64_t>(carrier.e10));
			}
			else
			{
				return requested < length;
			}
		}()};
		return result_type{carrier.m10, carrier.e10, true,
			rounding_discarded};
	}
}
#endif

/// @brief Selects the compiler-friendly decimal precision carrier.
/// @details Success is a numeric proof, not merely a shortest-format
///          heuristic.  Exact dyadics are complete carriers.  Other nearest
///          requests use the same strict distance-from-half and normal-value
///          separation proofs as the ordinary precision formatter.  Directed
///          requests accept only exact carriers or a value strictly below the
///          requested fractional quantum.  Every miss retains the exact binary
///          expansion fallback.
template <::fast_io::manipulators::scalar_flags flags, typename floating_type>
// GCC 11--16 at -O3 otherwise outline this constant-proxy planning leaf. In a
// pinned 16-callsite width+precision A/B, forced placement reduced aggregate
// wrapper instructions from 10,688/8,848/6,368/6,816/18,543/18,927 to
// 320/336/800/752/487/670 respectively; GCC 13--16 also saved 29--88 KiB of
// text. Every unknown-value wrapper remained instruction-identical. Clang is
// intentionally excluded: it already makes the better placement decision and
// forcing this body grows its literal caller. This function is reachable only
// from the compiler-constant precision protocol and cannot alter runtime ftoa.
// GCC 11 is the oldest tested GNU endpoint; no older compiler is extrapolated.
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr auto
compiler_constant_floating_make_decimal_precision_plan(
	::fast_io::details::punning_result<floating_type> fields,
	::std::size_t precision) noexcept
{
	using result_type =
		::fast_io::details::compiler_constant_floating_decimal_precision_plan<
			floating_type>;
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
	if (fields.exponent == exponent_mask ||
		(fields.mantissa == 0u && fields.exponent == 0u))
	{
		return result_type{};
	}
	constexpr bool fractional{
		::fast_io::details::floating_precision_is_fractional<flags.precision>};
	constexpr bool fractional_grid{
		fractional && flags.floating !=
			::fast_io::manipulators::floating_format::scientific};
#if defined(__SIZEOF_INT128__)
	/*
	 The complete binary80/binary128 coefficient at the minimum exponent has
	 more than eleven thousand digits.  Asking wide-shortest to build that
	 object merely to retain P+guard digits exceeds Clang's default constexpr
	 step budget.  Raw exponent 0 and 1 share the same binary exponent; restore
	 the normal's implicit bit and reuse the proved 512-bit subnormal window.
	 Its equal-endpoint acceptance supplies an exact prefix plus sticky state,
	 so this is a compile-time complexity reduction, not an approximation.
	 */
	if constexpr ((trait::mbits == 63u || trait::mbits == 112u) &&
		trait::ebits == 15u)
	{
		auto significand{static_cast<__uint128_t>(fields.mantissa)};
		constexpr ::std::int_least32_t bias{16383};
		::std::int_least32_t binary_exponent{
			1 - bias - static_cast<::std::int_least32_t>(trait::mbits)};
		if (fields.exponent)
		{
			significand |= static_cast<__uint128_t>(1u) << trait::mbits;
			binary_exponent = static_cast<::std::int_least32_t>(fields.exponent) -
				bias - static_cast<::std::int_least32_t>(trait::mbits);
		}
		auto const exponent_probe{::fast_io::details::
			exact_precision_wide_window_from_significand(
				significand, trait::mbits, binary_exponent, 1u)};
		if (exponent_probe.success)
		{
			::std::size_t requested{};
			if constexpr (fractional)
			{
				if constexpr (flags.floating ==
					::fast_io::manipulators::floating_format::scientific)
				{
					requested = ::fast_io::details::
						exact_precision_saturating_add(precision, 1u);
				}
				else if (0 <= exponent_probe.real_exponent)
				{
					requested = ::fast_io::details::
						exact_precision_saturating_add(
							::fast_io::details::
								exact_precision_saturating_add(
									static_cast<::std::size_t>(
										exponent_probe.real_exponent),
									precision),
							1u);
				}
				else
				{
					auto const leading_zeroes{static_cast<::std::size_t>(
						-exponent_probe.real_exponent)};
					requested = leading_zeroes <= precision
						? precision - leading_zeroes + 1u
						: 0u;
				}
			}
			else
			{
				requested = precision ? precision : 1u;
			}
			constexpr auto carrier_digits{
				::std::numeric_limits<__uint128_t>::digits10};
			if (requested < static_cast<::std::size_t>(carrier_digits))
			{
				auto const window{::fast_io::details::
					exact_precision_wide_window_from_significand(
						significand, trait::mbits, binary_exponent,
						requested + 1u)};
				if (window.success)
				{
					__uint128_t encoded{};
					for (::std::size_t index{};
						 index != window.decimal.size; ++index)
					{
						auto digit{static_cast<unsigned>(
							window.decimal.digits[index])};
						if (index + 1u == window.decimal.size &&
							window.tail_nonzero &&
							(digit == 0u || digit == 5u))
						{
							++digit;
						}
						encoded = encoded * 10u + digit;
					}
					auto const guard{window.decimal.digits[requested]};
					return result_type{encoded, window.decimal.exponent,
						true, guard != 0u || window.tail_nonzero};
				}
			}
		}

		/*
		 For the remaining wide exponents, precision formatting needs only the
		 requested decimal grid plus one guard digit.  Building the shortest
		 round-trip interval first would construct three full endpoint expansions;
		 at binary80/binary128 maximum finite values that needlessly exceeds
		 Clang's default constexpr step budget.  One exact expansion is sufficient:
		 its canonical final digit is nonzero, so any omitted suffix is a proved
		 sticky tail.  The compact uint128 carrier below therefore preserves every
		 nearest tie and directed-rounding decision without consulting ftoa again.
		 */
		if (1u < fields.exponent)
		{
			auto const exact{
				::fast_io::details::exact_precision_from_binary<floating_type>(
					fields.mantissa, fields.exponent)};
			auto const real_exponent{static_cast<::std::int_least32_t>(
				exact.exponent + static_cast<::std::int_least32_t>(exact.size) - 1)};
			::std::size_t requested{};
			if constexpr (fractional)
			{
				if constexpr (flags.floating ==
					::fast_io::manipulators::floating_format::scientific)
				{
					requested = ::fast_io::details::exact_precision_saturating_add(
						precision, 1u);
				}
				else if (0 <= real_exponent)
				{
					requested = ::fast_io::details::exact_precision_saturating_add(
						::fast_io::details::exact_precision_saturating_add(
							static_cast<::std::size_t>(real_exponent), precision),
						1u);
				}
				else
				{
					auto const leading_zeroes{static_cast<::std::size_t>(
						-real_exponent)};
					requested = leading_zeroes <= precision
						? precision - leading_zeroes + 1u
						: 0u;
				}
			}
			else
			{
				requested = precision ? precision : 1u;
			}

			constexpr auto carrier_digits{static_cast<::std::size_t>(
				::std::numeric_limits<__uint128_t>::digits10)};
			if (exact.size <= carrier_digits && exact.size <= requested)
			{
				__uint128_t coefficient{};
				for (::std::size_t index{}; index != exact.size; ++index)
				{
					coefficient = coefficient * 10u + exact.digits[index];
				}
				return result_type{coefficient, exact.exponent, true, false};
			}
			if (requested < exact.size && requested < carrier_digits)
			{
				auto const encoded_size{requested + 1u};
				__uint128_t encoded{};
				for (::std::size_t index{}; index != encoded_size; ++index)
				{
					auto digit{static_cast<unsigned>(exact.digits[index])};
					if (index + 1u == encoded_size && encoded_size < exact.size &&
						(digit == 0u || digit == 5u))
					{
						++digit;
					}
					encoded = encoded * 10u + digit;
				}
				auto const encoded_exponent{static_cast<::std::int_least32_t>(
					exact.exponent + static_cast<::std::int_least32_t>(
						exact.size - encoded_size))};
				return result_type{encoded, encoded_exponent, true, true};
			}
		}
	}
#endif
	auto const carrier{
		::fast_io::details::compiler_constant_floating_nearest_decimal_from_fields(
			fields)};
	if (!carrier.m10)
	{
		return result_type{};
	}
	auto const length{::fast_io::details::
		compiler_constant_floating_decimal_digits(carrier.m10)};
	auto const real_exponent{
		carrier.e10 + static_cast<::std::int_least32_t>(length) - 1};
	auto const requested_for_real_exponent{
		[precision](::std::int_least32_t candidate_real_exponent)
			constexpr noexcept -> ::std::size_t {
			if constexpr (fractional)
			{
				if constexpr (flags.floating ==
					::fast_io::manipulators::floating_format::scientific)
				{
					return ::fast_io::details::exact_precision_saturating_add(
						precision, 1u);
				}
				else if (0 <= candidate_real_exponent)
				{
					return ::fast_io::details::exact_precision_saturating_add(
						::fast_io::details::exact_precision_saturating_add(
							static_cast<::std::size_t>(candidate_real_exponent),
							precision),
						1u);
				}
				else
				{
					auto const leading_zeroes{static_cast<::std::size_t>(
						-static_cast<::std::int_least64_t>(
							candidate_real_exponent))};
					return leading_zeroes <= precision
						? precision - leading_zeroes + 1u
						: 0u;
				}
			}
			else
			{
				return precision ? precision : 1u;
			}
		}};
	auto const requested{requested_for_real_exponent(real_exponent)};

	auto const rounding_discarded{[&]() constexpr noexcept {
		if constexpr (fractional_grid)
		{
			return carrier.e10 < 0 &&
				precision < static_cast<::std::size_t>(
					-static_cast<::std::int_least64_t>(carrier.e10));
		}
		else
		{
			return requested < length;
		}
	}()};
	auto const grid_supported{
		::fast_io::details::compiler_constant_floating_carrier_grid_supported<
			floating_type>(carrier.m10, carrier.e10, requested, precision,
				fractional_grid)};

	/*
	 A nearest shortest carrier that is strictly separated from the decimal
	 rounding midpoint already determines the requested result.  This is the
	 same sufficient proof formerly performed after the exact-expansion
	 fallbacks; testing it here avoids instantiating those much larger paths for
	 ordinary constants without broadening the accepted numeric domain.
	 */
	if constexpr (::fast_io::details::floating_rounding_is_nearest<
		flags.rounding>)
	{
		bool exact_enough{};
		if (requested && requested < length)
		{
			auto const cut{length - requested};
			if (cut < 20u)
			{
				auto const divisor{
					::fast_io::details::print_rsv_fp_pow10_0_to_19_table[cut]};
				auto const remainder{static_cast<::std::uint_least64_t>(
					carrier.m10 % divisor)};
				auto const half{divisor / 2u};
				auto const distance{remainder <= half
					? divisor - remainder * 2u
					: (remainder - half) * 2u};
				exact_enough = 1u < distance;
			}
		}
		else if (fields.exponent && length <= requested &&
			requested <= static_cast<::std::size_t>((::std::numeric_limits<
				::fast_io::details::compiler_constant_floating_limits_type<
					floating_type>>::digits10)))
		{
			if constexpr (flags.floating ==
					::fast_io::manipulators::floating_format::general &&
				flags.precision == ::fast_io::manipulators::floating_precision::
					fractional_preserve_trailing_zero)
			{
				exact_enough = length == requested;
			}
			else
			{
				exact_enough = true;
			}
		}
		if (exact_enough && grid_supported)
		{
			return result_type{carrier.m10, carrier.e10, true,
				rounding_discarded};
		}
	}
	if (grid_supported &&
		::fast_io::details::
			compiler_constant_floating_decimal_carrier_is_binary_exact<
				floating_type>(fields.mantissa, fields.exponent, carrier.m10,
					carrier.e10))
	{
		return result_type{carrier.m10, carrier.e10, true,
			rounding_discarded};
	}

	/*
	 Binary32/binary64 precision cannot in general be reconstructed from the
	 shortest round-trip carrier: the minimum binary64 subnormal at P=6
	 scientific is the canonical counterexample.  Reuse the established exact
	 prefix window instead of constructing its complete 114/768-digit dyadic
	 expansion.  Only requested digits, one guard and a sticky bit escape.  A
	 sticky guard 0/5 is moved to 1/6, respectively, which encodes exactly the
	 below/equal/above-half distinction needed by all six nearest policies while
	 also preserving the nonzero-discard fact required by the four directed
	 policies.  This constant-only consumer does not alter the run-time ftoa
	 carrier or its by-value floating ABI.
	 */
#if defined(__SIZEOF_INT128__)
	if constexpr (::std::same_as<::std::remove_cv_t<floating_type>, float> ||
		::std::same_as<::std::remove_cv_t<floating_type>, double>)
	{
		using clean_type = ::std::remove_cv_t<floating_type>;
		constexpr auto carrier_digits{static_cast<::std::size_t>(
			::std::numeric_limits<
				::fast_io::details::
					compiler_constant_floating_precision_mantissa_type<
						clean_type>>::digits10)};
		if (requested < carrier_digits)
		{
			auto window_real_exponent{real_exponent};
			auto window_requested{requested};
			for (unsigned attempt{}; attempt != 2u; ++attempt)
			{
				auto const window{::fast_io::details::
					exact_precision_compact_window_from_binary<clean_type>(
						fields.mantissa, fields.exponent,
						window_requested + 1u, window_real_exponent)};
				if (!window.success)
				{
					break;
				}
				if (window.real_exponent != window_real_exponent)
				{
					window_real_exponent = window.real_exponent;
					window_requested = requested_for_real_exponent(
						window_real_exponent);
					if (carrier_digits <= window_requested)
					{
						break;
					}
					continue;
				}
				using precision_mantissa_type = ::fast_io::details::
					compiler_constant_floating_precision_mantissa_type<
						clean_type>;
				precision_mantissa_type encoded{};
				for (::std::size_t index{};
					 index != window.decimal.size; ++index)
				{
					auto digit{static_cast<precision_mantissa_type>(
						window.decimal.digits[index])};
					if (index + 1u == window.decimal.size &&
						window.tail_nonzero && (digit == 0u || digit == 5u))
					{
						++digit;
					}
					encoded = static_cast<precision_mantissa_type>(
						encoded * 10u + digit);
				}
				return result_type{encoded, window.decimal.exponent, true,
					window.tail_nonzero};
			}
		}
	}
#endif

	/*
	 A binary16/bfloat16 value can likewise need more coefficient digits than
	 its shortest carrier (for example 1.53125).  Its complete exact expansion
	 is small, so keeping that simpler uint64 path avoids routing narrow formats
	 through a binary32-specific window merely because Dragonbox shares a core.
	 */
	if constexpr (trait::mbits <= 10u && trait::ebits <= 8u)
	{
		auto const exact{
			::fast_io::details::exact_precision_from_binary<floating_type>(
				fields.mantissa, fields.exponent)};
		constexpr auto carrier_digits{
			::std::numeric_limits<::std::uint_least64_t>::digits10};
		if (exact.size <= static_cast<::std::size_t>(carrier_digits))
		{
			using precision_mantissa_type = ::fast_io::details::
				compiler_constant_floating_precision_mantissa_type<floating_type>;
			precision_mantissa_type exact_mantissa{};
			bool carrier_fits{true};
			constexpr auto carrier_maximum{
				(::std::numeric_limits<precision_mantissa_type>::max)()};
			for (::std::size_t index{}; index != exact.size; ++index)
			{
				auto const digit{
					static_cast<precision_mantissa_type>(exact.digits[index])};
				if ((carrier_maximum - digit) / 10u < exact_mantissa)
				{
					carrier_fits = false;
					break;
				}
				exact_mantissa = static_cast<precision_mantissa_type>(
					exact_mantissa * 10u + digit);
			}
			auto const exact_real_exponent{
				exact.exponent +
				static_cast<::std::int_least32_t>(exact.size) - 1};
			::std::size_t exact_requested{};
			if constexpr (fractional)
			{
				if constexpr (flags.floating ==
					::fast_io::manipulators::floating_format::scientific)
				{
					exact_requested =
						::fast_io::details::exact_precision_saturating_add(
							precision, 1u);
				}
				else if (0 <= exact_real_exponent)
				{
					exact_requested =
						::fast_io::details::exact_precision_saturating_add(
							::fast_io::details::
								exact_precision_saturating_add(
									static_cast<::std::size_t>(
										exact_real_exponent),
									precision),
							1u);
				}
				else
				{
					auto const leading_zeroes{static_cast<::std::size_t>(
						-exact_real_exponent)};
					exact_requested = leading_zeroes <= precision
						? precision - leading_zeroes + 1u
						: 0u;
				}
			}
			else
			{
				exact_requested = precision ? precision : 1u;
			}
			auto const exact_rounding_discarded{[&]() constexpr noexcept {
				if constexpr (fractional_grid)
				{
					return exact.exponent < 0 &&
						precision < static_cast<::std::size_t>(
							-static_cast<::std::int_least64_t>(
								exact.exponent));
				}
				else
				{
					return exact_requested < exact.size;
				}
			}()};
			if (carrier_fits && ::fast_io::details::
					compiler_constant_floating_carrier_grid_supported<
						floating_type>(exact_mantissa, exact.exponent,
							exact_requested, precision, fractional_grid))
			{
				return result_type{exact_mantissa, exact.exponent, true,
					exact_rounding_discarded};
			}
		}

		/*
		 When the complete coefficient is longer than uint64_t, retain one
		 decimal guard digit beyond the requested grid.  A nonzero tail turns
		 guard 0 into 1 and guard 5 into 6; those are the only cases where the
		 discarded tail changes any of the ten deterministic policies.  The
		 ordinary carrier rounder can then make the final decision exactly from
		 at most nineteen digits, with no floating arithmetic.
		 */
		auto const exact_real_exponent{
			exact.exponent + static_cast<::std::int_least32_t>(exact.size) - 1};
		::std::size_t exact_requested{};
		if constexpr (fractional)
		{
			if constexpr (flags.floating ==
				::fast_io::manipulators::floating_format::scientific)
			{
				exact_requested =
					::fast_io::details::exact_precision_saturating_add(
						precision, 1u);
			}
			else if (0 <= exact_real_exponent)
			{
				exact_requested =
					::fast_io::details::exact_precision_saturating_add(
						::fast_io::details::exact_precision_saturating_add(
							static_cast<::std::size_t>(exact_real_exponent),
							precision),
						1u);
			}
			else
			{
				auto const leading_zeroes{
					static_cast<::std::size_t>(-exact_real_exponent)};
				exact_requested = leading_zeroes <= precision
					? precision - leading_zeroes + 1u
					: 0u;
			}
		}
		else
		{
			exact_requested = precision ? precision : 1u;
		}
		if (exact_requested < exact.size &&
			exact_requested < static_cast<::std::size_t>(carrier_digits))
		{
			using precision_mantissa_type = ::fast_io::details::
				compiler_constant_floating_precision_mantissa_type<floating_type>;
			auto const encoded_size{exact_requested + 1u};
			precision_mantissa_type encoded_mantissa{};
			bool carrier_fits{true};
			constexpr auto carrier_maximum{
				(::std::numeric_limits<precision_mantissa_type>::max)()};
			for (::std::size_t index{}; index != encoded_size; ++index)
			{
				auto digit{
					static_cast<precision_mantissa_type>(exact.digits[index])};
				if (index + 1u == encoded_size && encoded_size < exact.size &&
					(digit == 0u || digit == 5u))
				{
					++digit;
				}
				if ((carrier_maximum - digit) / 10u < encoded_mantissa)
				{
					carrier_fits = false;
					break;
				}
				encoded_mantissa = static_cast<precision_mantissa_type>(
					encoded_mantissa * 10u + digit);
			}
			auto const encoded_exponent{static_cast<::std::int_least32_t>(
				exact.exponent + static_cast<::std::int_least32_t>(
					exact.size - encoded_size))};
			if (carrier_fits && ::fast_io::details::
					compiler_constant_floating_carrier_grid_supported<
						floating_type>(encoded_mantissa, encoded_exponent,
							exact_requested, precision, fractional_grid))
			{
				return result_type{encoded_mantissa, encoded_exponent, true,
					true};
			}
		}
	}

	if constexpr (fractional_grid)
	{
		constexpr auto int32_max{
			(::std::numeric_limits<::std::int_least32_t>::max)()};
		if (precision <= static_cast<::std::size_t>(int32_max) &&
			static_cast<::std::int_least64_t>(real_exponent) +
					static_cast<::std::int_least64_t>(precision) <=
				-2)
		{
			return result_type{carrier.m10, carrier.e10, true, true};
		}
	}
	return result_type{};
}

inline constexpr ::std::size_t
	compiler_constant_floating_compact_decimal_capacity{40u};

struct compiler_constant_floating_compact_decimal
{
	unsigned char
		digits[compiler_constant_floating_compact_decimal_capacity]{};
	::std::size_t size{1u};
	::std::int_least32_t exponent{};
};

template <::fast_io::manipulators::scalar_flags flags, typename floating_type>
struct compiler_constant_floating_rounded_carrier
{
	compiler_constant_floating_compact_decimal decimal{};
	::std::size_t significant{};
};

/// @brief Tests whether a constant precision carrier still satisfies the
///        ordinary Dragonbox/DA coefficient-width invariant.
/// @details The native rounding and digit writers use `chars_len<10,true>`;
///          that Ryu-mode query intentionally omits decimal widths which a
///          shortest binary32/binary64 carrier can never reach.  The constant
///          exact-window planner may append a guard digit or retain an exact
///          dyadic coefficient beyond that bound.  Such carriers must stay in
///          the constant protocol's full-width integer routines or their
///          length is undercounted before rounding and emission.
// Constant native-emitter placement audit: this forced leaf and the two named
// define leaves below (`compiler_constant_floating_native_precision_carrier_define`
// and `compiler_constant_floating_decimal_precision_carrier_define`) form one
// constant-only unit. With carrier sizing and width forwarding already exposed,
// the focused GCC 15 O3 caller moved from 0x9a/one carrier call to 0x6f/zero
// calls; forcing the outer define without this predicate instead produced
// 0x103/four calls. GCC 13/15 and Clang 23 unknown-value bodies retained the
// same normalized instruction hashes. No native run-time ftoa CPO uses this unit.
template <typename floating_type, typename mantissa_type>
#if defined(__GNUC__) && !defined(__clang__) && 15 <= __GNUC__
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr bool
compiler_constant_floating_is_native_decimal_carrier(
	mantissa_type mantissa) noexcept
{
	using native_mantissa_type =
		::fast_io::details::dragonbox_decimal_mantissa_type<floating_type>;
	if constexpr (::std::same_as<mantissa_type, native_mantissa_type> &&
		sizeof(mantissa_type) <= sizeof(::std::uint_least64_t))
	{
		return ::fast_io::details::compiler_constant_floating_decimal_digits(
			mantissa) <= static_cast<::std::size_t>(
				::fast_io::details::iec559_traits<floating_type>::m10digits);
	}
	else
	{
		(void)mantissa;
		return false;
	}
}

template <typename floating_type,
	::fast_io::manipulators::floating_rounding rounding,
	typename mantissa_type>
inline constexpr void
compiler_constant_floating_round_to_significant(
	mantissa_type &mantissa, ::std::int_least32_t &exponent,
	::std::size_t precision, bool negative) noexcept
{
	using native_mantissa_type =
		::fast_io::details::dragonbox_decimal_mantissa_type<floating_type>;
	if constexpr (::std::same_as<mantissa_type, native_mantissa_type> &&
		sizeof(mantissa_type) <= sizeof(::std::uint_least64_t))
	{
		if (::fast_io::details::
			compiler_constant_floating_is_native_decimal_carrier<floating_type>(
				mantissa))
		{
			::fast_io::details::print_rsv_fp_round_to_significant<
				floating_type, rounding, false>(
					mantissa, exponent, precision, negative);
			return;
		}
	}
	if (!mantissa)
	{
		return;
	}
	if (!precision)
	{
		precision = 1u;
	}
	auto const length{::fast_io::details::
		compiler_constant_floating_decimal_digits(mantissa)};
	if (precision < length)
	{
		auto const cut{length - precision};
		mantissa_type divisor{1u};
		for (::std::size_t index{}; index != cut; ++index)
		{
			divisor *= 10u;
		}
		auto quotient{static_cast<mantissa_type>(mantissa / divisor)};
		auto const remainder{static_cast<mantissa_type>(
			mantissa - quotient * divisor)};
		bool round_up{};
		if (remainder)
		{
			if constexpr (::fast_io::details::floating_rounding_is_nearest<
				rounding>)
			{
				auto const half{static_cast<mantissa_type>(divisor >> 1u)};
				if (half < remainder)
				{
					round_up = true;
				}
				else if (remainder == half)
				{
					round_up = ::fast_io::details::
						print_rsv_fp_decimal_tie_round_up<rounding>(
							negative,
							static_cast<::std::uint_least64_t>(quotient));
				}
			}
			else
			{
				round_up = ::fast_io::details::
					floating_rounding_directed_round_up<rounding>(negative);
			}
		}
		if (round_up)
		{
			++quotient;
		}
		mantissa = quotient;
		exponent += static_cast<::std::int_least32_t>(cut);
		if (precision < static_cast<::std::size_t>(
				::std::numeric_limits<mantissa_type>::digits10) + 1u &&
			::fast_io::details::compiler_constant_floating_decimal_digits(
				mantissa) > precision)
		{
			mantissa /= 10u;
			++exponent;
		}
	}
}

/// @brief Rounds a decimal carrier without re-entering the native run-time ftoa leaf.
/// @details Compiler-constant fixed formatting needs an integer-only expression graph that the optimizer can fold all
///          the way to character stores. The native shortcut remains in the ordinary wrapper below; this helper is
///          selected only after the compiler-constant protocol has proved both the value and precision constant. A
///          strict per-symbol audit extends the required range to GCC 11/12: leaving this helper outlined retains two
///          rounding calls and the complete mutually-exclusive presentation graph in a literal fixed-precision caller.
///          The unknown-value arm rejects before this helper is instantiated. A recursive `%.*f` deletion matrix also
///          requires this exact portable rounding edge on Clang 21--23: without it the successful constant root retains
///          the proxy helper, while a run-time precision root can reach the same proxy implementation. Clang 16--20
///          fail the complete six-edge candidate and therefore retain ordinary placement.
template <typename floating_type,
	::fast_io::manipulators::floating_rounding rounding,
	typename mantissa_type>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr void
compiler_constant_floating_round_to_fractional_portable(
	mantissa_type &mantissa, ::std::int_least32_t &exponent,
	::std::size_t precision, bool negative) noexcept
{
	if (!mantissa || 0 <= exponent ||
		precision >= static_cast<::std::size_t>(
			-static_cast<::std::int_least64_t>(exponent)))
	{
		return;
	}
	auto const target_exponent{-static_cast<::std::int_least32_t>(precision)};
	auto const cut{static_cast<::std::uint_least32_t>(
		target_exponent - exponent)};
	if (static_cast<::std::uint_least32_t>(
			::std::numeric_limits<mantissa_type>::digits10) < cut)
	{
		if constexpr (::fast_io::details::floating_rounding_is_nearest<
			rounding>)
		{
			mantissa = 0u;
		}
		else
		{
			mantissa = static_cast<mantissa_type>(
				::fast_io::details::floating_rounding_directed_round_up<
					rounding>(negative));
		}
		exponent = target_exponent;
		return;
	}
	mantissa_type divisor{1u};
	for (::std::uint_least32_t index{}; index != cut; ++index)
	{
		divisor *= 10u;
	}
	auto quotient{static_cast<mantissa_type>(mantissa / divisor)};
	auto const remainder{static_cast<mantissa_type>(
		mantissa - quotient * divisor)};
	bool round_up{};
	if (remainder)
	{
		if constexpr (::fast_io::details::floating_rounding_is_nearest<
			rounding>)
		{
			auto const half{static_cast<mantissa_type>(divisor >> 1u)};
			if (half < remainder)
			{
				round_up = true;
			}
			else if (remainder == half)
			{
				round_up = ::fast_io::details::
					print_rsv_fp_decimal_tie_round_up<rounding>(
						negative,
						static_cast<::std::uint_least64_t>(quotient));
			}
		}
		else
		{
			round_up = ::fast_io::details::
				floating_rounding_directed_round_up<rounding>(negative);
		}
	}
	if (round_up)
	{
		++quotient;
	}
	mantissa = quotient;
	exponent = target_exponent;
}

template <typename floating_type,
	::fast_io::manipulators::floating_rounding rounding,
	typename mantissa_type>
inline constexpr void
compiler_constant_floating_round_to_fractional(
	mantissa_type &mantissa, ::std::int_least32_t &exponent,
	::std::size_t precision, bool negative) noexcept
{
	using native_mantissa_type =
		::fast_io::details::dragonbox_decimal_mantissa_type<floating_type>;
	if constexpr (::std::same_as<mantissa_type, native_mantissa_type> &&
		sizeof(mantissa_type) <= sizeof(::std::uint_least64_t))
	{
		if (::fast_io::details::
			compiler_constant_floating_is_native_decimal_carrier<floating_type>(
				mantissa))
		{
			::fast_io::details::print_rsv_fp_round_to_fractional<
				floating_type, rounding>(
					mantissa, exponent, precision, negative);
			return;
		}
	}
	::fast_io::details::compiler_constant_floating_round_to_fractional_portable<
		floating_type, rounding>(mantissa, exponent, precision, negative);
}

template <::fast_io::manipulators::scalar_flags flags, typename floating_type>
[[nodiscard]] inline constexpr auto
compiler_constant_floating_round_decimal_carrier(
	::fast_io::details::compiler_constant_floating_precision_mantissa_type<
		floating_type> mantissa,
	::std::int_least32_t exponent, ::std::size_t precision, bool negative,
	bool rounding_discarded) noexcept
{
	using result_type =
		::fast_io::details::compiler_constant_floating_rounded_carrier<
			flags, floating_type>;
	constexpr bool fractional{
		::fast_io::details::floating_precision_is_fractional<flags.precision>};
	constexpr bool preserve{
		::fast_io::details::floating_precision_preserves_trailing_zero<
			flags.precision>};
	::std::size_t significant{};
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::scientific)
	{
		significant = fractional
			? ::fast_io::details::exact_precision_saturating_add(precision, 1u)
			: (precision ? precision : 1u);
		::fast_io::details::compiler_constant_floating_round_to_significant<
			floating_type, flags.rounding>(
				mantissa, exponent, significant, negative);
	}
	else if constexpr (fractional)
	{
		::fast_io::details::compiler_constant_floating_round_to_fractional<
			floating_type, flags.rounding>(
				mantissa, exponent, precision, negative);
	}
	else
	{
		significant = precision ? precision : 1u;
		::fast_io::details::compiler_constant_floating_round_to_significant<
			floating_type, flags.rounding>(
				mantissa, exponent, significant, negative);
	}
	if constexpr (!preserve)
	{
		::fast_io::details::print_rsv_fp_trim_trailing_zero(
			mantissa, exponent);
	}

	result_type result{};
	result.decimal.exponent = exponent;
	if (!mantissa)
	{
		result.decimal.digits[0] = 0u;
		result.decimal.size = 1u;
	}
	else
	{
		auto const size{
			::fast_io::details::compiler_constant_floating_decimal_digits(
				mantissa)};
		if (compiler_constant_floating_compact_decimal_capacity < size)
		{
			return result_type{};
		}
		result.decimal.size = size;
		for (auto index{size}; index; mantissa /= 10u)
		{
			result.decimal.digits[--index] =
				static_cast<unsigned char>(mantissa % 10u);
		}
	}
	if constexpr (fractional && preserve &&
		flags.floating == ::fast_io::manipulators::floating_format::general)
	{
		significant = rounding_discarded
			? ::fast_io::details::
				exact_precision_fractional_general_rounded_virtual_size(
					result.decimal, precision)
			: result.decimal.size;
	}
	result.significant = significant;
	return result;
}

// Carrier-size placement audit: changing only this compiler-constant size leaf reduced the focused GCC 15
// width+precision caller from 0x457/10 calls to 0x13a/one call and `.text` from 31,767 to 29,562 bytes. Together with
// the grid predicate above, pinned Clang 21--23 reduced a fixed-precision constant caller from 560 instructions and 43
// calls to 340 and 17, and a 16-callsite executable from about 215 KiB to 45 KiB without a material compile-time or
// peak-RSS increase. Unknown values cannot reach either constant-only leaf: the dedicated run-time ftoa translation
// units were byte-identical before and after the change on all three Clang versions. Both positive policies remain
// future-open; the ordinary fields-size fallback, exact planner and native by-value ftoa path remain unforced.
template <::fast_io::manipulators::scalar_flags flags, typename floating_type>
#if (defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::size_t
compiler_constant_floating_decimal_precision_carrier_size(
	::fast_io::details::compiler_constant_floating_precision_mantissa_type<
		floating_type> mantissa,
	::std::int_least32_t exponent, ::std::size_t precision, bool negative,
	bool rounding_discarded) noexcept
{
	if constexpr (
		flags.floating == ::fast_io::manipulators::floating_format::fixed &&
		::fast_io::details::floating_precision_is_fractional<flags.precision> &&
		::fast_io::details::floating_precision_preserves_trailing_zero<
			flags.precision>)
	{
		::fast_io::details::
			compiler_constant_floating_round_to_fractional_portable<
				floating_type, flags.rounding>(
					mantissa, exponent, precision, negative);
		auto const digits{
			::fast_io::details::compiler_constant_floating_decimal_digits(
				mantissa)};
		auto const real_exponent{static_cast<::std::int_least64_t>(exponent) +
			static_cast<::std::int_least64_t>(digits) - 1};
		auto const integral_size{real_exponent < 0
			? 1u
			: static_cast<::std::size_t>(real_exponent) + 1u};
		return integral_size +
			(precision
				? precision + 1u
				: static_cast<::std::size_t>(flags.json_float) * 2u);
	}
	constexpr bool preserve{
		::fast_io::details::floating_precision_preserves_trailing_zero<
			flags.precision>};
	constexpr bool significant{
		::fast_io::details::floating_precision_is_significant<flags.precision>};
	auto const direct{[&]() constexpr noexcept {
		if constexpr (!preserve || !significant ||
			flags.floating ==
				::fast_io::manipulators::floating_format::scientific)
		{
			return true;
		}
		else
		{
			auto const requested{precision ? precision : 1u};
			auto const length{::fast_io::details::
				compiler_constant_floating_decimal_digits(mantissa)};
			if (requested <= length)
			{
				return true;
			}
			auto const padding{requested - length};
			if (static_cast<::std::size_t>(
					::fast_io::details::iec559_traits<floating_type>::m10digits) <
					requested ||
				20u <= padding)
			{
				return false;
			}
			auto const multiplier{
				::fast_io::details::print_rsv_fp_pow10_0_to_19_table[padding]};
			using mantissa_type = ::fast_io::details::
				compiler_constant_floating_precision_mantissa_type<floating_type>;
			auto const next{static_cast<mantissa_type>(mantissa * multiplier)};
			return next / multiplier == mantissa;
		}
	}()};
	using precision_mantissa_type = ::fast_io::details::
		compiler_constant_floating_precision_mantissa_type<floating_type>;
	using native_mantissa_type =
		::fast_io::details::dragonbox_decimal_mantissa_type<floating_type>;
	if constexpr (::std::same_as<precision_mantissa_type, native_mantissa_type> &&
		sizeof(precision_mantissa_type) <= sizeof(::std::uint_least64_t))
	{
		if (direct && ::fast_io::details::
			compiler_constant_floating_is_native_decimal_carrier<floating_type>(
				mantissa))
		{
			return ::fast_io::details::floating_precise_carrier_precision_size<
				floating_type, flags.floating, flags.precision, flags.rounding,
				flags.json_float>(
					mantissa, exponent, precision, negative);
		}
	}
	auto const rounded{
		::fast_io::details::compiler_constant_floating_round_decimal_carrier<
			flags, floating_type>(mantissa, exponent, precision, negative,
				rounding_discarded)};
	return ::fast_io::details::floating_precise_rounded_precision_size<
		floating_type, flags.floating, flags.precision, flags.json_float>(
			rounded.decimal, precision, rounded.significant);
}

/// @brief Keeps the fixed fractional native-carrier decision inside a compiler-constant proxy.
/// @details This is exactly the fixed/preserving arm of `print_rsv_fp_precision_decision_impl`: round to the requested
///          decimal quantum, then render that carrier with the constant-digit terminal. All other modes retain the
///          shared decision implementation. The strategy flag changes only integer digit placement; punctuation,
///          rounding, JSON spelling and the returned endpoint remain owned by the same fixed-precision function.
// Forced placement is the middle leaf of the constant native-emitter audit documented at
// `compiler_constant_floating_is_native_decimal_carrier`. Tested GCC 13--16 benefit, so the policy remains open until
// a newer compiler measures a reversal.
template <::fast_io::manipulators::scalar_flags flags, typename floating_type,
	::std::integral char_type>
#if defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr char_type *
compiler_constant_floating_native_precision_carrier_define(
	char_type *iter,
	::fast_io::details::dragonbox_decimal_mantissa_type<floating_type> mantissa,
	::std::int_least32_t exponent, ::std::size_t precision,
	bool negative) noexcept
{
	if constexpr (
		flags.floating == ::fast_io::manipulators::floating_format::fixed &&
		::fast_io::details::floating_precision_is_fractional<flags.precision> &&
		::fast_io::details::floating_precision_preserves_trailing_zero<
			flags.precision>)
	{
		::fast_io::details::print_rsv_fp_round_to_fractional<
			floating_type, flags.rounding>(
				mantissa, exponent, precision, negative);
		return ::fast_io::details::print_rsv_fp_fixed_precision_impl<
			floating_type, flags.comma, flags.json_float,
			::fast_io::details::floating_fixed_precision_digit_writer::positional>(
				iter, mantissa, exponent, precision);
	}
	else
	{
		return ::fast_io::details::print_rsv_fp_precision_decision_impl<
			floating_type, flags.comma, flags.uppercase_e, flags.floating,
			flags.precision, flags.rounding, flags.json_float>(
				iter, mantissa, exponent, precision, negative);
	}
}

// Forced placement is the outer leaf of the constant native-emitter audit
// documented at `compiler_constant_floating_is_native_decimal_carrier`.
// Clang 21--23 additionally require this constant-only edge after prepared
// output has selected its bounded contiguous representation; leaving it
// outlined retains the complete carrier fallback in an otherwise short record.
template <::fast_io::manipulators::scalar_flags flags, typename floating_type,
	::std::integral char_type>
#if defined(__GNUC__) && !defined(__clang__) && 15 <= __GNUC__
FAST_IO_GNU_ALWAYS_INLINE
#elif defined(__clang__) && 21 <= __clang_major__
FAST_IO_GNU_ALWAYS_INLINE
#endif
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
compiler_constant_floating_decimal_precision_carrier_define(
	char_type *iter,
	::fast_io::details::compiler_constant_floating_precision_mantissa_type<
		floating_type> mantissa,
	::std::int_least32_t exponent, ::std::size_t precision, bool negative,
	bool rounding_discarded) noexcept
{
	if constexpr (
		flags.floating == ::fast_io::manipulators::floating_format::fixed &&
		::fast_io::details::floating_precision_is_fractional<flags.precision> &&
		::fast_io::details::floating_precision_preserves_trailing_zero<
			flags.precision>)
	{
		::fast_io::details::
			compiler_constant_floating_round_to_fractional_portable<
				floating_type, flags.rounding>(
					mantissa, exponent, precision, negative);
		constexpr auto spelling_flags{[]() consteval {
			auto value{flags};
			value.json_float = false;
			return value;
		}()};
		iter = ::fast_io::details::compiler_constant_floating_write_fixed<
			spelling_flags>(iter, mantissa, exponent);
		auto const existing_fractional{exponent < 0
			? static_cast<::std::size_t>(
				-static_cast<::std::int_least64_t>(exponent))
			: 0u};
		if (precision)
		{
			if (!existing_fractional)
			{
				*iter++ = ::fast_io::char_literal_v<
					(flags.comma ? u8',' : u8'.'), char_type>;
			}
			for (auto count{precision - existing_fractional}; count; --count)
			{
				*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
			}
		}
		else if constexpr (flags.json_float)
		{
			*iter++ = ::fast_io::char_literal_v<
				(flags.comma ? u8',' : u8'.'), char_type>;
			*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
		}
		return iter;
	}
	constexpr bool preserve{
		::fast_io::details::floating_precision_preserves_trailing_zero<
			flags.precision>};
	constexpr bool significant{
		::fast_io::details::floating_precision_is_significant<flags.precision>};
	auto const direct{[&]() constexpr noexcept {
		if constexpr (!preserve || !significant ||
			flags.floating ==
				::fast_io::manipulators::floating_format::scientific)
		{
			return true;
		}
		else
		{
			auto const requested{precision ? precision : 1u};
			auto const length{::fast_io::details::
				compiler_constant_floating_decimal_digits(mantissa)};
			if (requested <= length)
			{
				return true;
			}
			auto const padding{requested - length};
			if (static_cast<::std::size_t>(
					::fast_io::details::iec559_traits<floating_type>::m10digits) <
					requested ||
				20u <= padding)
			{
				return false;
			}
			auto const multiplier{
				::fast_io::details::print_rsv_fp_pow10_0_to_19_table[padding]};
			using mantissa_type = ::fast_io::details::
				compiler_constant_floating_precision_mantissa_type<floating_type>;
			auto const next{static_cast<mantissa_type>(mantissa * multiplier)};
			return next / multiplier == mantissa;
		}
	}()};
	using precision_mantissa_type = ::fast_io::details::
		compiler_constant_floating_precision_mantissa_type<floating_type>;
	using native_mantissa_type =
		::fast_io::details::dragonbox_decimal_mantissa_type<floating_type>;
	if constexpr (::std::same_as<precision_mantissa_type, native_mantissa_type> &&
		sizeof(precision_mantissa_type) <= sizeof(::std::uint_least64_t))
	{
		if (direct && ::fast_io::details::
			compiler_constant_floating_is_native_decimal_carrier<floating_type>(
				mantissa))
		{
			return ::fast_io::details::
				compiler_constant_floating_native_precision_carrier_define<
					flags, floating_type>(
					iter, mantissa, exponent, precision, negative);
		}
	}
	auto const rounded{
		::fast_io::details::compiler_constant_floating_round_decimal_carrier<
			flags, floating_type>(mantissa, exponent, precision, negative,
				rounding_discarded)};
	return ::fast_io::details::print_rsvflt_rounded_precision_define_impl<
		floating_type, flags.comma, flags.uppercase_e, flags.floating,
		flags.precision, flags.json_float>(
			iter, rounded.decimal, precision, rounded.significant);
}

// Exact-size leaf for an already materialized constant proxy. Removing the
// size/define pair leaves an out-of-line proxy call (GCC 15 main 0x8c; Clang 23
// main 0x5be). It is not the native floating precise-size implementation.
template <::fast_io::manipulators::scalar_flags flags,
	::std::integral proxy_char_type, typename floating_type>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::size_t
compiler_constant_floating_scalar_materialized_output_size(
	::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
		proxy_char_type, flags, floating_type> const &value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		return ::fast_io::details::compiler_constant_floating_hex_size<
			flags, floating_type>(value.binary_mantissa, value.binary_exponent,
			value.negative);
	}
	else if (value.binary_exponent == exponent_mask)
	{
		return ::fast_io::details::compiler_constant_floating_special_size<
			flags, trait::mbits>(value.binary_mantissa, value.negative);
	}
	else if (value.binary_mantissa == 0u && value.binary_exponent == 0u)
	{
		auto size{
			::fast_io::details::compiler_constant_floating_sign_size<flags>(
				value.negative) + 1u};
		if constexpr (flags.floating ==
			::fast_io::manipulators::floating_format::scientific)
		{
			size += 3u;
		}
		else if constexpr (flags.json_float)
		{
			size += 2u;
		}
		return size;
	}
	else
	{
		auto size{
			::fast_io::details::compiler_constant_floating_sign_size<flags>(
				value.negative)};
		if (::fast_io::details::compiler_constant_floating_uses_fixed<flags>(
				value.decimal_mantissa, value.decimal_exponent))
		{
			size += ::fast_io::details::compiler_constant_floating_fixed_size<flags>(
				value.decimal_mantissa, value.decimal_exponent);
		}
		else
		{
			size += ::fast_io::details::compiler_constant_floating_scientific_size<
				flags, floating_type>(value.decimal_mantissa,
				value.decimal_exponent);
		}
		return size;
	}
}

template <::fast_io::manipulators::scalar_flags flags, typename floating_type>
#if __has_cpp_attribute(__gnu__::__const__)
[[__gnu__::__const__]]
#endif
[[nodiscard]] inline constexpr ::std::size_t
compiler_constant_floating_scalar_output_size(floating_type value) noexcept
{
	auto const materialized{
		::fast_io::details::compiler_constant_floating_scalar_materialize<
			char, flags>(value)};
	return ::fast_io::details::
		compiler_constant_floating_scalar_materialized_output_size<flags>(
			materialized);
}

// Emission leaf paired with the proxy size proof above. Its parameters contain
// integer fields only; forcing it cannot change native floating register-class
// transport in the ordinary print/concat path.
template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type, ::std::integral proxy_char_type,
	typename floating_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
compiler_constant_floating_scalar_define(
	char_type *iter,
	::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
		proxy_char_type, flags, floating_type> const &value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		return ::fast_io::details::compiler_constant_floating_write_hex<
			flags, char_type, floating_type>(iter, value.binary_mantissa,
			value.binary_exponent, value.negative);
	}
	else if (value.binary_exponent == exponent_mask)
	{
		return ::fast_io::details::compiler_constant_floating_write_special<
			flags, trait::mbits>(iter, value.binary_mantissa, value.negative);
	}
	iter = ::fast_io::details::compiler_constant_floating_write_sign<flags>(
		iter, value.negative);
	if (value.binary_mantissa == 0u && value.binary_exponent == 0u)
	{
		*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
		if constexpr (flags.floating ==
			::fast_io::manipulators::floating_format::scientific)
		{
			*iter++ = flags.uppercase
				? ::fast_io::char_literal_v<u8'E', char_type>
				: ::fast_io::char_literal_v<u8'e', char_type>;
			*iter++ = ::fast_io::char_literal_v<u8'+', char_type>;
			*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
		}
		else if constexpr (flags.json_float)
		{
			*iter++ = ::fast_io::char_literal_v<
				(flags.comma ? u8',' : u8'.'), char_type>;
			*iter++ = ::fast_io::char_literal_v<u8'0', char_type>;
		}
		return iter;
	}
	if (::fast_io::details::compiler_constant_floating_uses_fixed<flags>(
			value.decimal_mantissa, value.decimal_exponent))
	{
		return ::fast_io::details::compiler_constant_floating_write_fixed<flags>(
			iter, value.decimal_mantissa, value.decimal_exponent);
	}
	return ::fast_io::details::compiler_constant_floating_write_scientific<
		flags, floating_type>(iter, value.decimal_mantissa,
		value.decimal_exponent);
}

/*
Immutable character storage for the unbuffered compiler-constant path.  Every
entry is constructed through char_literal_v/char_literal_add, rather than by
assuming the execution character set is ASCII.  Consequently char and wchar_t
remain correct on EBCDIC targets, while char8_t/char16_t/char32_t each retain
their own correctly typed storage.  Descriptors may point into these inline
objects for the duration of the program; no descriptor ever points into a
floating proxy or a local formatting buffer.
*/
template <::std::integral char_type>
struct compiler_constant_floating_fragment_storage
{
	inline static constexpr auto decimal_digits{[]() constexpr {
		::fast_io::freestanding::array<char_type, 10u> result{};
		for (::std::size_t index{}; index != result.size(); ++index)
		{
			result[index] = ::fast_io::char_literal_add<char_type>(index);
		}
		return result;
	}()};

	inline static constexpr auto decimal_digit_pairs{[]() constexpr {
		::fast_io::freestanding::array<char_type, 200u> result{};
		for (::std::size_t value{}; value != 100u; ++value)
		{
			result[value * 2u] =
				::fast_io::char_literal_add<char_type>(value / 10u);
			result[value * 2u + 1u] =
				::fast_io::char_literal_add<char_type>(value % 10u);
		}
		return result;
	}()};

	template <bool uppercase>
	inline static constexpr auto hexadecimal_digits{[]() constexpr {
		::fast_io::freestanding::array<char_type, 16u> result{};
		for (::std::size_t index{}; index != result.size(); ++index)
		{
			result[index] = ::fast_io::details::compiler_constant_floating_hex_digit<
				uppercase, char_type>(static_cast<::std::uint_least32_t>(index));
		}
		return result;
	}()};

	template <bool uppercase>
	inline static constexpr auto hexadecimal_digit_pairs{[]() constexpr {
		::fast_io::freestanding::array<char_type, 512u> result{};
		for (::std::size_t value{}; value != 256u; ++value)
		{
			result[value * 2u] =
				::fast_io::details::compiler_constant_floating_hex_digit<
					uppercase, char_type>(
						static_cast<::std::uint_least32_t>(value >> 4u));
			result[value * 2u + 1u] =
				::fast_io::details::compiler_constant_floating_hex_digit<
					uppercase, char_type>(
						static_cast<::std::uint_least32_t>(value & 0x0fu));
		}
		return result;
	}()};

	inline static constexpr auto zeroes{[]() constexpr {
		::fast_io::freestanding::array<
			char_type, compiler_constant_floating_scalar_capacity> result{};
		for (auto &element : result)
		{
			element = ::fast_io::char_literal_v<u8'0', char_type>;
		}
		return result;
	}()};

	inline static constexpr ::fast_io::freestanding::array<char_type, 2u>
		signs{::fast_io::char_literal_v<u8'+', char_type>,
			  ::fast_io::char_literal_v<u8'-', char_type>};
	inline static constexpr ::fast_io::freestanding::array<char_type, 2u>
		decimal_points{::fast_io::char_literal_v<u8'.', char_type>,
					   ::fast_io::char_literal_v<u8',', char_type>};
	inline static constexpr ::fast_io::freestanding::array<char_type, 2u>
		lower_hex_prefix{::fast_io::char_literal_v<u8'0', char_type>,
						 ::fast_io::char_literal_v<u8'x', char_type>};
	inline static constexpr ::fast_io::freestanding::array<char_type, 2u>
		upper_hex_prefix{::fast_io::char_literal_v<u8'0', char_type>,
						 ::fast_io::char_literal_v<u8'X', char_type>};

	template <bool hexadecimal, bool uppercase, bool negative>
	inline static constexpr auto exponent_prefix{[]() constexpr {
		::fast_io::freestanding::array<char_type, 2u> result{};
		if constexpr (hexadecimal)
		{
			result[0u] = ::fast_io::char_literal_v<
				uppercase ? u8'P' : u8'p', char_type>;
		}
		else
		{
			result[0u] = ::fast_io::char_literal_v<
				uppercase ? u8'E' : u8'e', char_type>;
		}
		result[1u] = ::fast_io::char_literal_v<
			negative ? u8'-' : u8'+', char_type>;
		return result;
	}()};

	template <bool uppercase>
	inline static constexpr auto infinity{[]() constexpr {
		::fast_io::freestanding::array<char_type, 3u> result{};
		result[0u] = ::fast_io::char_literal_v<uppercase ? u8'I' : u8'i', char_type>;
		result[1u] = ::fast_io::char_literal_v<uppercase ? u8'N' : u8'n', char_type>;
		result[2u] = ::fast_io::char_literal_v<uppercase ? u8'F' : u8'f', char_type>;
		return result;
	}()};

	template <bool uppercase>
	inline static constexpr auto nan{[]() constexpr {
		::fast_io::freestanding::array<char_type, 3u> result{};
		result[0u] = ::fast_io::char_literal_v<uppercase ? u8'N' : u8'n', char_type>;
		result[1u] = ::fast_io::char_literal_v<uppercase ? u8'A' : u8'a', char_type>;
		result[2u] = ::fast_io::char_literal_v<uppercase ? u8'N' : u8'n', char_type>;
		return result;
	}()};

	template <bool uppercase>
	inline static constexpr auto indeterminate_suffix{[]() constexpr {
		::fast_io::freestanding::array<char_type, 5u> result{};
		result[0u] = ::fast_io::char_literal_v<u8'(', char_type>;
		result[1u] = ::fast_io::char_literal_v<uppercase ? u8'I' : u8'i', char_type>;
		result[2u] = ::fast_io::char_literal_v<uppercase ? u8'N' : u8'n', char_type>;
		result[3u] = ::fast_io::char_literal_v<uppercase ? u8'D' : u8'd', char_type>;
		result[4u] = ::fast_io::char_literal_v<u8')', char_type>;
		return result;
	}()};

	template <bool uppercase>
	inline static constexpr auto signaling_suffix{[]() constexpr {
		::fast_io::freestanding::array<char_type, 6u> result{};
		result[0u] = ::fast_io::char_literal_v<u8'(', char_type>;
		result[1u] = ::fast_io::char_literal_v<uppercase ? u8'S' : u8's', char_type>;
		result[2u] = ::fast_io::char_literal_v<uppercase ? u8'N' : u8'n', char_type>;
		result[3u] = ::fast_io::char_literal_v<uppercase ? u8'A' : u8'a', char_type>;
		result[4u] = ::fast_io::char_literal_v<uppercase ? u8'N' : u8'n', char_type>;
		result[5u] = ::fast_io::char_literal_v<u8')', char_type>;
		return result;
	}()};
};

template <::std::integral char_type>
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_fragment_append(
	::fast_io::basic_io_scatter_t<char_type> *current,
	char_type const *base, ::std::size_t size) noexcept
{
	if (size != 0u)
	{
		*current++ = {base, size};
	}
	return current;
}

template <::std::integral char_type, typename unsigned_type>
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_fragment_decimal_exact(
	::fast_io::basic_io_scatter_t<char_type> *current,
	unsigned_type value, ::std::size_t digits) noexcept
{
	using storage =
		::fast_io::details::compiler_constant_floating_fragment_storage<char_type>;
	if ((digits & 1u) != 0u)
	{
		auto const divisor{
			::fast_io::details::compiler_constant_floating_power_of_ten<
				unsigned_type>(digits - 1u)};
		auto const digit{static_cast<::std::size_t>(value / divisor)};
		current = ::fast_io::details::compiler_constant_floating_fragment_append(
			current, storage::decimal_digits.data() + digit, 1u);
		value = static_cast<unsigned_type>(value % divisor);
		--digits;
	}
	if (digits != 0u)
	{
		auto divisor{
			::fast_io::details::compiler_constant_floating_power_of_ten<
				unsigned_type>(digits - 2u)};
		for (;;)
		{
			auto const pair{static_cast<::std::size_t>(value / divisor)};
			current = ::fast_io::details::compiler_constant_floating_fragment_append(
				current, storage::decimal_digit_pairs.data() + pair * 2u, 2u);
			value = static_cast<unsigned_type>(value % divisor);
			if (digits == 2u)
			{
				break;
			}
			digits -= 2u;
			divisor = static_cast<unsigned_type>(divisor / 100u);
		}
	}
	return current;
}

template <::std::integral char_type>
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_fragment_zeroes(
	::fast_io::basic_io_scatter_t<char_type> *current,
	::std::size_t count) noexcept
{
	using storage =
		::fast_io::details::compiler_constant_floating_fragment_storage<char_type>;
	return ::fast_io::details::compiler_constant_floating_fragment_append(
		current, storage::zeroes.data(), count);
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type>
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_fragment_sign(
	::fast_io::basic_io_scatter_t<char_type> *current, bool negative) noexcept
{
	if (negative || flags.showpos)
	{
		using storage = ::fast_io::details::
			compiler_constant_floating_fragment_storage<char_type>;
		current = ::fast_io::details::compiler_constant_floating_fragment_append(
			current, storage::signs.data() + static_cast<::std::size_t>(negative),
			1u);
	}
	return current;
}

template <bool hexadecimal, bool uppercase, ::std::integral char_type>
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_fragment_exponent(
	::fast_io::basic_io_scatter_t<char_type> *current,
	::std::int_least32_t exponent, ::std::size_t minimum_digits) noexcept
{
	using storage =
		::fast_io::details::compiler_constant_floating_fragment_storage<char_type>;
	bool const negative{exponent < 0};
	if (negative)
	{
		auto const &prefix{storage::template exponent_prefix<
			hexadecimal, uppercase, true>};
		current = ::fast_io::details::compiler_constant_floating_fragment_append(
			current, prefix.data(), prefix.size());
	}
	else
	{
		auto const &prefix{storage::template exponent_prefix<
			hexadecimal, uppercase, false>};
		current = ::fast_io::details::compiler_constant_floating_fragment_append(
			current, prefix.data(), prefix.size());
	}
	auto const magnitude{static_cast<::std::uint_least32_t>(
		negative ? -static_cast<::std::int_least64_t>(exponent) : exponent)};
	auto digits{
		::fast_io::details::compiler_constant_floating_unsigned_digits(magnitude)};
	if (digits < minimum_digits)
	{
		digits = minimum_digits;
	}
	return ::fast_io::details::compiler_constant_floating_fragment_decimal_exact(
		current, magnitude, digits);
}

template <::std::integral char_type, typename unsigned_type>
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_fragment_decimal_slice(
	::fast_io::basic_io_scatter_t<char_type> *current,
	unsigned_type value, ::std::size_t total_digits, ::std::size_t first,
	::std::size_t last) noexcept
{
	if (first == last)
	{
		return current;
	}
	auto const trailing{total_digits - last};
	if (trailing)
	{
		value = static_cast<unsigned_type>(
			value / ::fast_io::details::compiler_constant_floating_power_of_ten<
				unsigned_type>(trailing));
	}
	auto const digits{last - first};
	if (first)
	{
		value = static_cast<unsigned_type>(
			value % ::fast_io::details::compiler_constant_floating_power_of_ten<
				unsigned_type>(digits));
	}
	return ::fast_io::details::compiler_constant_floating_fragment_decimal_exact(
		current, value, digits);
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type, typename unsigned_type>
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_fragment_rounded_fixed(
	::fast_io::basic_io_scatter_t<char_type> *current,
	unsigned_type mantissa, ::std::int_least32_t exponent,
	::std::size_t virtual_size, bool force_fractional,
	::std::size_t fractional_precision) noexcept
{
	using storage =
		::fast_io::details::compiler_constant_floating_fragment_storage<char_type>;
	auto const size{
		::fast_io::details::compiler_constant_floating_decimal_digits(mantissa)};
	auto const point{exponent + static_cast<::std::int_least32_t>(size)};
	auto append_point = [&](auto *iter) constexpr noexcept {
		return ::fast_io::details::compiler_constant_floating_fragment_append(
			iter,
			storage::decimal_points.data() +
				static_cast<::std::size_t>(flags.comma),
			1u);
	};
	if (force_fractional && fractional_precision && 0 < point)
	{
		auto const integer_digits{static_cast<::std::size_t>(point)};
		if (integer_digits <= size && virtual_size == size)
		{
			current = ::fast_io::details::
				compiler_constant_floating_fragment_decimal_slice(
					current, mantissa, size, 0u, integer_digits);
			current = append_point(current);
			current = ::fast_io::details::
				compiler_constant_floating_fragment_decimal_slice(
					current, mantissa, size, integer_digits, size);
			auto const present{size - integer_digits};
			return ::fast_io::details::
				compiler_constant_floating_fragment_zeroes(
					current, present < fractional_precision
						? fractional_precision - present
						: 0u);
		}
	}
	bool wrote_point{};
	if (point <= 0)
	{
		current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
			current, 1u);
		if (mantissa != 0u || force_fractional)
		{
			current = append_point(current);
			wrote_point = true;
			current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
				current, static_cast<::std::size_t>(-point));
			current = ::fast_io::details::
				compiler_constant_floating_fragment_decimal_exact(
					current, mantissa, size);
			current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
				current, virtual_size - size);
		}
	}
	else
	{
		auto const integer_digits{static_cast<::std::size_t>(point)};
		if (integer_digits < virtual_size)
		{
			auto const from_mantissa{
				integer_digits < size ? integer_digits : size};
			current = ::fast_io::details::
				compiler_constant_floating_fragment_decimal_slice(
					current, mantissa, size, 0u, from_mantissa);
			current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
				current, from_mantissa < integer_digits
					? integer_digits - from_mantissa
					: 0u);
			current = append_point(current);
			wrote_point = true;
			if (integer_digits < size)
			{
				current = ::fast_io::details::
					compiler_constant_floating_fragment_decimal_slice(
						current, mantissa, size, integer_digits, size);
			}
			if (size < virtual_size)
			{
				auto const already{
					size < integer_digits ? integer_digits : size};
				current = ::fast_io::details::
					compiler_constant_floating_fragment_zeroes(
						current, virtual_size - already);
			}
		}
		else
		{
			current = ::fast_io::details::
				compiler_constant_floating_fragment_decimal_exact(
					current, mantissa, size);
			current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
				current, integer_digits - size);
		}
	}
	if (force_fractional)
	{
		auto const present{point <= 0
			? static_cast<::std::size_t>(-point) + virtual_size
			: point < static_cast<::std::int_least32_t>(virtual_size)
				? virtual_size - static_cast<::std::size_t>(point)
				: 0u};
		if (!wrote_point && fractional_precision)
		{
			current = append_point(current);
			wrote_point = true;
		}
		if (present < fractional_precision)
		{
			current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
				current, fractional_precision - present);
		}
	}
	if constexpr (flags.json_float)
	{
		if (!wrote_point)
		{
			current = append_point(current);
			current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
				current, 1u);
		}
	}
	return current;
}

template <::fast_io::manipulators::scalar_flags flags, typename floating_type,
	::std::integral char_type, typename unsigned_type>
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_fragment_rounded_scientific(
	::fast_io::basic_io_scatter_t<char_type> *current,
	unsigned_type mantissa, ::std::int_least32_t exponent,
	::std::size_t fractional_precision, bool preserve) noexcept
{
	using storage =
		::fast_io::details::compiler_constant_floating_fragment_storage<char_type>;
	auto const size{
		::fast_io::details::compiler_constant_floating_decimal_digits(mantissa)};
	auto const real_exponent{
		exponent + static_cast<::std::int_least32_t>(size) - 1};
	current = ::fast_io::details::
		compiler_constant_floating_fragment_decimal_slice(
			current, mantissa, size, 0u, 1u);
	auto const available{size - 1u};
	auto const used{
		available < fractional_precision ? available : fractional_precision};
	if (used || (preserve && fractional_precision))
	{
		current = ::fast_io::details::compiler_constant_floating_fragment_append(
			current,
			storage::decimal_points.data() +
				static_cast<::std::size_t>(flags.comma),
			1u);
		current = ::fast_io::details::
			compiler_constant_floating_fragment_decimal_slice(
				current, mantissa, size, 1u, used + 1u);
		if (preserve && used < fractional_precision)
		{
			current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
				current, fractional_precision - used);
		}
	}
	return ::fast_io::details::compiler_constant_floating_fragment_exponent<
		false, flags.uppercase_e>(current, real_exponent, 2u);
}

template <::fast_io::manipulators::scalar_flags flags, typename floating_type,
	::std::integral char_type>
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_fragment_decimal_precision_carrier(
	::fast_io::basic_io_scatter_t<char_type> *current,
	::fast_io::details::compiler_constant_floating_precision_mantissa_type<
		floating_type> mantissa,
	::std::int_least32_t exponent, ::std::size_t precision, bool negative,
	bool rounding_discarded) noexcept
{
	constexpr bool fractional{
		::fast_io::details::floating_precision_is_fractional<flags.precision>};
	constexpr bool preserve{
		::fast_io::details::floating_precision_preserves_trailing_zero<
			flags.precision>};
	::std::size_t significant{};
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::scientific)
	{
		significant = fractional
			? ::fast_io::details::exact_precision_saturating_add(precision, 1u)
			: (precision ? precision : 1u);
		::fast_io::details::compiler_constant_floating_round_to_significant<
			floating_type, flags.rounding>(
				mantissa, exponent, significant, negative);
	}
	else if constexpr (fractional)
	{
		::fast_io::details::compiler_constant_floating_round_to_fractional<
			floating_type, flags.rounding>(
				mantissa, exponent, precision, negative);
	}
	else
	{
		significant = precision ? precision : 1u;
		::fast_io::details::compiler_constant_floating_round_to_significant<
			floating_type, flags.rounding>(
				mantissa, exponent, significant, negative);
	}
	if constexpr (!preserve)
	{
		::fast_io::details::print_rsv_fp_trim_trailing_zero(mantissa, exponent);
	}
	auto const size{
		::fast_io::details::compiler_constant_floating_decimal_digits(mantissa)};
	if constexpr (fractional && preserve &&
		flags.floating == ::fast_io::manipulators::floating_format::general)
	{
		if (rounding_discarded)
		{
			::std::size_t padding{};
			if (0 <= exponent)
			{
				padding = ::fast_io::details::exact_precision_saturating_add(
					precision, static_cast<::std::size_t>(exponent));
			}
			else
			{
				auto const magnitude{static_cast<::std::size_t>(
					-static_cast<::std::int_least64_t>(exponent))};
				if (magnitude < precision)
				{
					padding = precision - magnitude;
				}
			}
			significant = ::fast_io::details::exact_precision_saturating_add(
				size, padding);
		}
		else
		{
			significant = size;
		}
	}
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::scientific)
	{
		auto const fractional_digits{
			fractional ? precision : significant - 1u};
		return ::fast_io::details::
			compiler_constant_floating_fragment_rounded_scientific<
				flags, floating_type>(current, mantissa, exponent,
					fractional_digits, preserve);
	}
	auto virtual_size{size};
	if constexpr (preserve &&
		(!fractional || flags.floating ==
			::fast_io::manipulators::floating_format::general))
	{
		if (virtual_size < significant)
		{
			virtual_size = significant;
		}
	}
	if constexpr (flags.floating ==
			::fast_io::manipulators::floating_format::fixed ||
		(fractional && flags.floating ==
			::fast_io::manipulators::floating_format::decimal))
	{
		return ::fast_io::details::
			compiler_constant_floating_fragment_rounded_fixed<flags>(
				current, mantissa, exponent, virtual_size,
				fractional && preserve, precision);
	}
	auto const virtual_padding{virtual_size - size};
	bool fixed{};
	constexpr auto int32_max{
		(::std::numeric_limits<::std::int_least32_t>::max)()};
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::general)
	{
		auto const rounded_exponent{
			exponent + static_cast<::std::int_least32_t>(size) - 1};
		if constexpr (flags.precision ==
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
					static_cast<::std::int_least64_t>(exponent) -
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
			static_cast<::std::int_least64_t>(exponent) -
			static_cast<::std::int_least64_t>(virtual_padding)};
		fixed = -5 < virtual_exponent && virtual_exponent < 7;
	}
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::decimal)
	{
		auto const rounded_exponent{
			exponent + static_cast<::std::int_least32_t>(size) - 1};
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
				virtual_size,
				static_cast<::std::size_t>(-rounded_exponent) + 1u);
		}
		auto const scientific_length{
			::fast_io::details::print_rsv_fp_scientific_length(
				rounded_exponent, virtual_size)};
		fixed = scientific_length >= fixed_length;
	}
	if (fixed)
	{
		return ::fast_io::details::
			compiler_constant_floating_fragment_rounded_fixed<flags>(
				current, mantissa, exponent, virtual_size,
				fractional && preserve, precision);
	}
	return ::fast_io::details::
		compiler_constant_floating_fragment_rounded_scientific<
			flags, floating_type>(current, mantissa, exponent,
				virtual_size - 1u, preserve);
}

template <::fast_io::manipulators::scalar_flags flags, typename floating_type,
	::std::integral char_type>
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_fragment_decimal_precision_zero(
	::fast_io::basic_io_scatter_t<char_type> *current,
	::std::size_t precision) noexcept
{
	using storage =
		::fast_io::details::compiler_constant_floating_fragment_storage<char_type>;
	current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
		current, 1u);
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::scientific)
	{
		auto fractional_digits{precision};
		if constexpr (::fast_io::details::
			floating_precision_is_significant<flags.precision>)
		{
			fractional_digits = precision ? precision - 1u : 0u;
		}
		if constexpr (::fast_io::details::
			floating_precision_preserves_trailing_zero<flags.precision>)
		{
			if (fractional_digits)
			{
				current = ::fast_io::details::
					compiler_constant_floating_fragment_append(
						current,
						storage::decimal_points.data() +
							static_cast<::std::size_t>(flags.comma),
						1u);
				current = ::fast_io::details::
					compiler_constant_floating_fragment_zeroes(
						current, fractional_digits);
			}
		}
		return ::fast_io::details::compiler_constant_floating_fragment_exponent<
			false, flags.uppercase_e>(current, 0, 2u);
	}
	constexpr bool fractional{
		::fast_io::details::floating_precision_is_fractional<flags.precision>};
	constexpr bool preserve{
		::fast_io::details::floating_precision_preserves_trailing_zero<
			flags.precision>};
	::std::size_t zeroes{};
	if constexpr (fractional && preserve)
	{
		zeroes = precision;
	}
	else if constexpr (!fractional && preserve)
	{
		zeroes = precision ? precision - 1u : 0u;
	}
	else if constexpr (flags.json_float)
	{
		zeroes = 1u;
	}
	if (zeroes)
	{
		current = ::fast_io::details::compiler_constant_floating_fragment_append(
			current,
			storage::decimal_points.data() +
				static_cast<::std::size_t>(flags.comma),
			1u);
		current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
			current, zeroes);
	}
	return current;
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::size_t mantissa_bits, ::std::integral char_type,
	typename mantissa_type>
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_fragment_special(
	::fast_io::basic_io_scatter_t<char_type> *current,
	mantissa_type mantissa, bool negative) noexcept
{
	using storage =
		::fast_io::details::compiler_constant_floating_fragment_storage<char_type>;
	if (mantissa == 0u)
	{
		current = ::fast_io::details::compiler_constant_floating_fragment_sign<flags>(
			current, negative);
		auto const &text{storage::template infinity<flags.uppercase>};
		return ::fast_io::details::compiler_constant_floating_fragment_append(
			current, text.data(), text.size());
	}
	if constexpr (flags.nan_show_sign)
	{
		current = ::fast_io::details::compiler_constant_floating_fragment_sign<flags>(
			current, negative);
	}
	auto const &text{storage::template nan<flags.uppercase>};
	current = ::fast_io::details::compiler_constant_floating_fragment_append(
		current, text.data(), text.size());
	if constexpr (flags.nan_show_type)
	{
		constexpr mantissa_type quiet_bit{
			::fast_io::details::fp_quiet_nan_mantissa_mask<
				mantissa_type, mantissa_bits>()};
		if (negative && mantissa == quiet_bit)
		{
			auto const &suffix{
				storage::template indeterminate_suffix<flags.uppercase>};
			return ::fast_io::details::compiler_constant_floating_fragment_append(
				current, suffix.data(), suffix.size());
		}
		if (::fast_io::details::fp_nan_is_signaling<
				mantissa_type, mantissa_bits>(mantissa))
		{
			auto const &suffix{
				storage::template signaling_suffix<flags.uppercase>};
			return ::fast_io::details::compiler_constant_floating_fragment_append(
				current, suffix.data(), suffix.size());
		}
	}
	return current;
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type, typename unsigned_type>
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
// Required third edge of the GCC expanded-fragment deletion chain. Without it the successful `to<double>` root still
// calls this fixed-notation planner and retains the complete 32-slot frame; it accepts only integer proxy fields.
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_fragment_fixed(
	::fast_io::basic_io_scatter_t<char_type> *current,
	unsigned_type mantissa, ::std::int_least32_t exponent) noexcept
{
	using storage =
		::fast_io::details::compiler_constant_floating_fragment_storage<char_type>;
	auto const digits{static_cast<::std::int_least32_t>(
		::fast_io::details::compiler_constant_floating_decimal_digits(mantissa))};
	auto const real_exponent{
		static_cast<::std::int_least32_t>(exponent + digits - 1)};
	if (digits <= real_exponent)
	{
		current = ::fast_io::details::compiler_constant_floating_fragment_decimal_exact(
			current, mantissa, static_cast<::std::size_t>(digits));
		current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
			current, static_cast<::std::size_t>(real_exponent + 1 - digits));
		if constexpr (flags.json_float)
		{
			current = ::fast_io::details::compiler_constant_floating_fragment_append(
				current,
				storage::decimal_points.data() + static_cast<::std::size_t>(flags.comma),
				1u);
			current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
				current, 1u);
		}
	}
	else if (0 <= real_exponent)
	{
		auto const integral_size{static_cast<::std::size_t>(real_exponent + 1)};
		auto const fractional_size{
			static_cast<::std::size_t>(digits) - integral_size};
		if (fractional_size != 0u)
		{
			auto const divisor{
				::fast_io::details::compiler_constant_floating_power_of_ten<
					unsigned_type>(fractional_size)};
			current = ::fast_io::details::compiler_constant_floating_fragment_decimal_exact(
				current, static_cast<unsigned_type>(mantissa / divisor), integral_size);
			current = ::fast_io::details::compiler_constant_floating_fragment_append(
				current,
				storage::decimal_points.data() + static_cast<::std::size_t>(flags.comma),
				1u);
			current = ::fast_io::details::compiler_constant_floating_fragment_decimal_exact(
				current, static_cast<unsigned_type>(mantissa % divisor), fractional_size);
		}
		else
		{
			current = ::fast_io::details::compiler_constant_floating_fragment_decimal_exact(
				current, mantissa, integral_size);
			if constexpr (flags.json_float)
			{
				current = ::fast_io::details::compiler_constant_floating_fragment_append(
					current,
					storage::decimal_points.data() +
						static_cast<::std::size_t>(flags.comma),
					1u);
				current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
					current, 1u);
			}
		}
	}
	else
	{
		current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
			current, 1u);
		current = ::fast_io::details::compiler_constant_floating_fragment_append(
			current,
			storage::decimal_points.data() + static_cast<::std::size_t>(flags.comma),
			1u);
		current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
			current, static_cast<::std::size_t>(-real_exponent - 1));
		current = ::fast_io::details::compiler_constant_floating_fragment_decimal_exact(
			current, mantissa, static_cast<::std::size_t>(digits));
	}
	return current;
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type, typename unsigned_type>
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_fragment_scientific(
	::fast_io::basic_io_scatter_t<char_type> *current,
	unsigned_type mantissa, ::std::int_least32_t exponent) noexcept
{
	using storage =
		::fast_io::details::compiler_constant_floating_fragment_storage<char_type>;
	auto const digits{
		::fast_io::details::compiler_constant_floating_decimal_digits(mantissa)};
	auto const divisor{
		::fast_io::details::compiler_constant_floating_power_of_ten<unsigned_type>(
			digits - 1u)};
	current = ::fast_io::details::compiler_constant_floating_fragment_decimal_exact(
		current, static_cast<unsigned_type>(mantissa / divisor), 1u);
	if (1u < digits)
	{
		current = ::fast_io::details::compiler_constant_floating_fragment_append(
			current,
			storage::decimal_points.data() + static_cast<::std::size_t>(flags.comma),
			1u);
		current = ::fast_io::details::compiler_constant_floating_fragment_decimal_exact(
			current, static_cast<unsigned_type>(mantissa % divisor), digits - 1u);
	}
	auto const real_exponent{static_cast<::std::int_least32_t>(
		exponent + static_cast<::std::int_least32_t>(digits) - 1)};
	return ::fast_io::details::compiler_constant_floating_fragment_exponent<
		false, flags.uppercase_e>(current, real_exponent, 2u);
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type, typename floating_type>
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_fragment_hex(
	::fast_io::basic_io_scatter_t<char_type> *current,
	typename ::fast_io::details::iec559_traits<floating_type>::mantissa_type mantissa,
	::std::uint_least32_t exponent, bool negative) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	using storage =
		::fast_io::details::compiler_constant_floating_fragment_storage<char_type>;
	constexpr ::std::uint_least32_t exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	if (exponent == exponent_mask)
	{
		return ::fast_io::details::compiler_constant_floating_fragment_special<
			flags, trait::mbits>(current, mantissa, negative);
	}
	current = ::fast_io::details::compiler_constant_floating_fragment_sign<flags>(
		current, negative);
	if constexpr (flags.showbase)
	{
		auto const &prefix{flags.uppercase_showbase ? storage::upper_hex_prefix
											 : storage::lower_hex_prefix};
		current = ::fast_io::details::compiler_constant_floating_fragment_append(
			current, prefix.data(), prefix.size());
	}
	auto const &hex_digits{storage::template hexadecimal_digits<flags.uppercase>};
	current = ::fast_io::details::compiler_constant_floating_fragment_append(
		current, hex_digits.data() + static_cast<::std::size_t>(exponent != 0u), 1u);
	constexpr ::std::size_t nibble_count{(trait::mbits + 3u) / 4u};
	constexpr ::std::size_t padding_bits{nibble_count * 4u - trait::mbits};
	auto aligned{static_cast<mantissa_type>(mantissa << padding_bits)};
	::std::size_t fractional_digits{nibble_count};
	while (fractional_digits != 0u && (aligned & 0x0fu) == 0u)
	{
		aligned = static_cast<mantissa_type>(aligned >> 4u);
		--fractional_digits;
	}
	if (fractional_digits != 0u)
	{
		current = ::fast_io::details::compiler_constant_floating_fragment_append(
			current,
			storage::decimal_points.data() + static_cast<::std::size_t>(flags.comma),
			1u);
		if ((fractional_digits & 1u) != 0u)
		{
			auto const shift{(fractional_digits - 1u) * 4u};
			auto const digit{static_cast<::std::size_t>(
				(aligned >> shift) & static_cast<mantissa_type>(0x0fu))};
			current = ::fast_io::details::compiler_constant_floating_fragment_append(
				current, hex_digits.data() + digit, 1u);
			--fractional_digits;
		}
		auto const &pairs{
			storage::template hexadecimal_digit_pairs<flags.uppercase>};
		while (fractional_digits != 0u)
		{
			auto const shift{(fractional_digits - 2u) * 4u};
			auto const pair{static_cast<::std::size_t>(
				(aligned >> shift) & static_cast<mantissa_type>(0xffu))};
			current = ::fast_io::details::compiler_constant_floating_fragment_append(
				current, pairs.data() + pair * 2u, 2u);
			fractional_digits -= 2u;
		}
	}
	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u)};
	auto const binary_exponent{exponent == 0u && mantissa == 0u ? 0
		: exponent == 0u ? 1 - bias
		: static_cast<::std::int_least32_t>(exponent) - bias};
	if (mantissa == 0u && exponent == 0u)
	{
		return ::fast_io::details::compiler_constant_floating_fragment_exponent<
			true, flags.uppercase>(current, binary_exponent, 1u);
	}
	return ::fast_io::details::compiler_constant_floating_fragment_exponent<
		true, flags.uppercase_e>(current, binary_exponent, 1u);
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type, ::std::integral proxy_char_type,
	typename floating_type>
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
// This is the algorithm leaf behind the proxy-only CPO. The complete `to` deletion test requires both edges: forcing
// only the public CPO still leaves this helper outlined and preserves the 32-slot descriptor frame. Unknown native
// floats cannot name this integer-field proxy overload.
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_scalar_static_fragments_define(
	::fast_io::basic_io_scatter_t<char_type> *current,
	::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
		proxy_char_type, flags, floating_type> const &value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		return ::fast_io::details::compiler_constant_floating_fragment_hex<
			flags, char_type, floating_type>(current, value.binary_mantissa,
			value.binary_exponent, value.negative);
	}
	else if (value.binary_exponent == exponent_mask)
	{
		return ::fast_io::details::compiler_constant_floating_fragment_special<
			flags, trait::mbits>(current, value.binary_mantissa, value.negative);
	}
	current = ::fast_io::details::compiler_constant_floating_fragment_sign<flags>(
		current, value.negative);
	if (value.binary_mantissa == 0u && value.binary_exponent == 0u)
	{
		current = ::fast_io::details::compiler_constant_floating_fragment_zeroes(
			current, 1u);
		if constexpr (flags.floating ==
			::fast_io::manipulators::floating_format::scientific)
		{
			return ::fast_io::details::compiler_constant_floating_fragment_exponent<
				false, flags.uppercase>(current, 0, 1u);
		}
		else if constexpr (flags.json_float)
		{
			using storage = ::fast_io::details::
				compiler_constant_floating_fragment_storage<char_type>;
			current = ::fast_io::details::compiler_constant_floating_fragment_append(
				current,
				storage::decimal_points.data() + static_cast<::std::size_t>(flags.comma),
				1u);
			return ::fast_io::details::compiler_constant_floating_fragment_zeroes(
				current, 1u);
		}
		return current;
	}
	if (::fast_io::details::compiler_constant_floating_uses_fixed<flags>(
			value.decimal_mantissa, value.decimal_exponent))
	{
		return ::fast_io::details::compiler_constant_floating_fragment_fixed<flags>(
			current, value.decimal_mantissa, value.decimal_exponent);
	}
	return ::fast_io::details::compiler_constant_floating_fragment_scientific<flags>(
		current, value.decimal_mantissa, value.decimal_exponent);
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type, ::std::integral proxy_char_type,
	typename floating_type>
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
compiler_constant_floating_precision_static_fragments_define(
	::fast_io::basic_io_scatter_t<char_type> *current,
	::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
		proxy_char_type, flags, floating_type> const &value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using storage =
		::fast_io::details::compiler_constant_floating_fragment_storage<char_type>;
	auto const [mantissa, exponent, negative]{value.fields};
	constexpr ::std::uint_least32_t exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	if constexpr (flags.floating !=
		::fast_io::manipulators::floating_format::hexfloat)
	{
		if (exponent == exponent_mask)
		{
			return ::fast_io::details::compiler_constant_floating_fragment_special<
				flags, trait::mbits>(current, mantissa, negative);
		}
		current = ::fast_io::details::compiler_constant_floating_fragment_sign<flags>(
			current, negative);
		if (mantissa == 0u && exponent == 0u)
		{
			return ::fast_io::details::
				compiler_constant_floating_fragment_decimal_precision_zero<
					flags, floating_type>(current, value.precision);
		}
		if (!value.decimal_carrier_available)
		{
			// This proxy is created only after the source eligibility proof has
			// selected either a finite carrier or a special/zero spelling.  Keep
			// the invariant defensive for manually forged internal proxies.
			return current;
		}
		return ::fast_io::details::
			compiler_constant_floating_fragment_decimal_precision_carrier<
				flags, floating_type>(
					current, value.decimal_mantissa, value.decimal_exponent,
					value.precision, negative,
					value.decimal_rounding_discarded);
	}
	else
	{
		if (exponent == exponent_mask)
		{
			return ::fast_io::details::compiler_constant_floating_fragment_special<
				flags, trait::mbits>(current, mantissa, negative);
		}
		current = ::fast_io::details::compiler_constant_floating_fragment_sign<flags>(
			current, negative);
		if constexpr (flags.showbase)
		{
			auto const &prefix{flags.uppercase_showbase
				? storage::upper_hex_prefix
				: storage::lower_hex_prefix};
			current = ::fast_io::details::compiler_constant_floating_fragment_append(
				current, prefix.data(), prefix.size());
		}
		auto const plan{
			::fast_io::details::compiler_constant_hex_make_precision_plan<flags>(
				value.fields, value.precision)};
		auto const &digits{
			storage::template hexadecimal_digits<flags.uppercase>};
		current = ::fast_io::details::compiler_constant_floating_fragment_append(
			current,
			digits.data() + ::fast_io::details::
				compiler_constant_hex_precision_plan_digit(plan, 0u),
			1u);
		if (1u < plan.digits_to_print)
		{
			current = ::fast_io::details::compiler_constant_floating_fragment_append(
				current,
				storage::decimal_points.data() +
					static_cast<::std::size_t>(flags.comma),
				1u);
			auto const digit_limit{plan.digits_to_print < plan.retained_digits
				? plan.digits_to_print
				: plan.retained_digits};
			::std::size_t index{1u};
			if (((digit_limit - index) & 1u) != 0u)
			{
				current = ::fast_io::details::
					compiler_constant_floating_fragment_append(
						current,
						digits.data() + ::fast_io::details::
							compiler_constant_hex_precision_plan_digit(
								plan, index),
						1u);
				++index;
			}
			auto const &pairs{
				storage::template hexadecimal_digit_pairs<flags.uppercase>};
			for (; index != digit_limit; index += 2u)
			{
				auto const pair{static_cast<::std::size_t>(
					(::fast_io::details::
						 compiler_constant_hex_precision_plan_digit(plan, index)
					 << 4u) |
					::fast_io::details::
						compiler_constant_hex_precision_plan_digit(
							plan, index + 1u))};
				current = ::fast_io::details::
					compiler_constant_floating_fragment_append(
						current, pairs.data() + pair * 2u, 2u);
			}
			if (digit_limit < plan.digits_to_print)
			{
				current = ::fast_io::details::
					compiler_constant_floating_fragment_zeroes(
						current, plan.digits_to_print - digit_limit);
			}
		}
		return ::fast_io::details::compiler_constant_floating_fragment_exponent<
			true, flags.uppercase_e>(
				current, plan.binary_exponent, 1u);
	}
}
} // namespace details

/// @brief Exposes a raw default-format floating source to print's pre-alias compiler-constant gate.
template <::std::integral char_type, typename floating_type>
	requires(
		::fast_io::details::my_floating_point<floating_type> &&
		::fast_io::details::compiler_constant_floating_type_supported<
			::fast_io::details::float_alias_type<floating_type>>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	::fast_io::io_reserve_type_t<char_type, floating_type>) noexcept
{
	return {};
}

template <::std::integral char_type, typename floating_type>
	requires(
		::fast_io::details::my_floating_point<floating_type> &&
		::fast_io::details::compiler_constant_floating_type_supported<
			::fast_io::details::float_alias_type<floating_type>>)
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char_type, floating_type>,
	floating_type const &value) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	return __builtin_constant_p(value);
#else
	(void)value;
	return false;
#endif
}

template <::std::integral char_type, typename floating_type>
	requires(
		::fast_io::details::my_floating_point<floating_type> &&
		::fast_io::details::compiler_constant_floating_type_supported<
			::fast_io::details::float_alias_type<floating_type>>)
// True-arm materializer only. Removing this attribute makes GCC 15 retain a
// 0x1cb-byte call path and makes Clang 23 expand the literal caller beyond
// 14 KiB; a dynamic source takes the unchanged false arm before construction.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr auto
print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char_type, floating_type>,
	floating_type const &value) noexcept
{
	using alias_type = ::fast_io::details::float_alias_type<floating_type>;
	if constexpr (
		::fast_io::details::floating_scalar_requires_integer_proxy<alias_type>)
	{
		// The source reference is already the alias type.  Re-spelling this as
		// `static_cast<__bf16>` makes Clang rematerialize a native narrowing
		// operation even though the following materializer consumes only fields.
		return ::fast_io::details::compiler_constant_floating_scalar_materialize<
			char_type,
			::fast_io::manipulators::floating_point_default_scalar_flags>(value);
	}
	else
	{
		return ::fast_io::details::compiler_constant_floating_scalar_materialize<
			char_type,
			::fast_io::manipulators::floating_point_default_scalar_flags>(
				static_cast<alias_type>(value));
	}
}

/// @brief Forwards a proved raw floating constant into its integer-field proxy.
/// @details The generic proved-gate forwarding CPO deliberately does not force GCC 13, 14, or 16 because doing so at
///          the type-agnostic boundary perturbs unrelated run-time wrappers. This overload is narrower: core can call
///          it only after this floating source's side-effect-free `__builtin_constant_p` query has succeeded. Tested GCC
///          13--16 otherwise outline the forwarding edge and lose the constant graph before compact emission. Tested
///          Clang 21--23 must retain the placement of the generic CPO which this more-specialized overload supersedes;
///          without it they reintroduce the same materializer call. The positive policy remains open for newer
///          frontends until a measured reversal. The ordinary false arm retains the native floating formatter and ABI.
template <::std::integral char_type, typename floating_type>
	requires(
		::fast_io::compiler_constant_printable<char_type, floating_type> &&
		::fast_io::details::my_floating_point<floating_type> &&
		::fast_io::details::compiler_constant_floating_type_supported<
			::fast_io::details::float_alias_type<floating_type>>)
#if (defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize_gate_proven(
	::fast_io::io_reserve_type_t<char_type, floating_type> tag,
	floating_type const &value) noexcept
{
	return print_compiler_constant_materialize(tag, value);
}

template <::std::integral char_type, typename floating_type>
	requires(
		::fast_io::details::my_floating_point<floating_type> &&
		::fast_io::details::compiler_constant_floating_type_supported<
			::fast_io::details::float_alias_type<floating_type>>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	::fast_io::io_reserve_type_t<char_type, floating_type>) noexcept
{
	return {};
}

/// @brief Records the permanent scalar-query and replacement-graph classification for raw floating sources.
/// @details The matrix distinguishes literal values from opaque values and consumer-specific fragment/precise writers;
///          this type-only marker itself grants no destination permission.
template <::std::integral char_type, typename floating_type>
	requires(
		::fast_io::details::my_floating_point<floating_type> &&
		::fast_io::details::compiler_constant_floating_type_supported<
			::fast_io::details::float_alias_type<floating_type>>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	::fast_io::io_reserve_type_t<char_type, floating_type>) noexcept
{
	return {};
}

/// @brief Classifies a raw floating source whose replacement prefers the bounded fragment spelling.
/// @details The materializer below produces the same scalar proxy that owns the expanded-fragment marker. Consumers
///          use this source-side classification only to reject an unproved compiler/destination pair before forming
///          that proxy or evaluating `__builtin_constant_p`; it does not select an output strategy by itself.
template <::std::integral char_type, typename floating_type>
	requires(
		::fast_io::details::my_floating_point<floating_type> &&
		::fast_io::details::compiler_constant_floating_type_supported<
			::fast_io::details::float_alias_type<floating_type>>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_source_prefer_expanded_fragments(
	::fast_io::io_reserve_type_t<char_type, floating_type>) noexcept
{
	return {};
}

/// @brief Classifies a raw floating value as one flat compiler-constant scalar source.
/// @details The query observes only the source scalar and the successful materializer produces one integer-field proxy;
///          dynamic precision and semantic formatting wrappers have separate source types and cannot satisfy this CPO.
template <::std::integral char_type, typename floating_type>
	requires(
		::fast_io::details::my_floating_point<floating_type> &&
		::fast_io::details::compiler_constant_floating_type_supported<
			::fast_io::details::float_alias_type<floating_type>>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_simple_scalar_source(
	::fast_io::io_reserve_type_t<char_type, floating_type>) noexcept
{
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<floating_type> &&
		::fast_io::details::print_floating_scalar_supported<flags, floating_type> &&
		flags.percentage == ::fast_io::manipulators::percentage_flag::none &&
		flags.rounding != ::fast_io::manipulators::floating_rounding::current_environment)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_t<flags, floating_type>>) noexcept
{
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<floating_type> &&
		::fast_io::details::print_floating_scalar_supported<flags, floating_type> &&
		flags.percentage == ::fast_io::manipulators::percentage_flag::none &&
		flags.rounding != ::fast_io::manipulators::floating_rounding::current_environment)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_t<flags, floating_type>>) noexcept
{
	return {};
}

/// @brief Records the permanent classification for a type-flagged floating scalar graph.
/// @details Formatting flags are type-owned, so the paired query roots vary only `reference`; consumers still prove
///          whether their selected exact or expanded-fragment writer is completely erased.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<floating_type> &&
		::fast_io::details::print_floating_scalar_supported<flags, floating_type> &&
		flags.percentage == ::fast_io::manipulators::percentage_flag::none &&
		flags.rounding != ::fast_io::manipulators::floating_rounding::current_environment)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_t<flags, floating_type>>) noexcept
{
	return {};
}

/// @brief Classifies an explicit floating scalar source before its replacement type is formed.
/// @details The flags are preserved verbatim by materialization, so the resulting scalar proxy satisfies the same
///          expanded-fragment preference as a raw floating source. Precision manipulators intentionally use a separate
///          replacement protocol and do not satisfy this source marker.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<floating_type> &&
		::fast_io::details::print_floating_scalar_supported<flags, floating_type> &&
		flags.percentage == ::fast_io::manipulators::percentage_flag::none &&
		flags.rounding != ::fast_io::manipulators::floating_rounding::current_environment)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_source_prefer_expanded_fragments(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_t<flags, floating_type>>) noexcept
{
	return {};
}

/// @brief Classifies a type-flagged floating scalar without admitting dynamic precision state.
/// @details All formatting flags are compile-time members of the type, while the query reads only `reference` and the
///          materializer returns one scalar proxy. The separate precision manipulator deliberately has no such marker.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<floating_type> &&
		::fast_io::details::print_floating_scalar_supported<flags, floating_type> &&
		flags.percentage == ::fast_io::manipulators::percentage_flag::none &&
		flags.rounding != ::fast_io::manipulators::floating_rounding::current_environment)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_simple_scalar_source(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_t<flags, floating_type>>) noexcept
{
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<floating_type> &&
		::fast_io::details::print_floating_scalar_supported<flags, floating_type> &&
		flags.percentage == ::fast_io::manipulators::percentage_flag::none &&
		flags.rounding != ::fast_io::manipulators::floating_rounding::current_environment)
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_t<flags, floating_type>>,
	::fast_io::manipulators::scalar_manip_t<flags, floating_type> const &value) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	return __builtin_constant_p(value.reference);
#else
	(void)value;
	return false;
#endif
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<floating_type> &&
		::fast_io::details::print_floating_scalar_supported<flags, floating_type> &&
		flags.percentage == ::fast_io::manipulators::percentage_flag::none &&
		flags.rounding != ::fast_io::manipulators::floating_rounding::current_environment)
// Manipulator true-arm counterpart of the raw materializer above. The same A/B
// leaves residual calls for literal scalar manipulators, while runtime values
// preserve their original scalar_manip_t and native by-value formatter.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr auto
print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_t<flags, floating_type>>,
	::fast_io::manipulators::scalar_manip_t<flags, floating_type> const &value) noexcept
{
	return ::fast_io::details::
		compiler_constant_floating_scalar_materialize<char_type, flags>(
			value.reference);
}

/// @brief Preserves a proved constant scalar-manipulator graph through GCC's forwarding boundary.
/// @details This source-specific overload is reachable only from the already-taken compiler-constant arm. Keeping the
///          attribute here, instead of widening the generic CPO, prevents unknown scalar values from inheriting any
///          extra inlining or code-size policy while allowing tested GCC 13--16 and Clang 21--23 to fold flags and
///          captured fields completely. The positive policy remains open for newer frontends until a measured reversal.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<floating_type> &&
		::fast_io::details::print_floating_scalar_supported<flags, floating_type> &&
		flags.percentage == ::fast_io::manipulators::percentage_flag::none &&
		flags.rounding != ::fast_io::manipulators::floating_rounding::current_environment)
#if (defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize_gate_proven(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_t<flags, floating_type>> tag,
	::fast_io::manipulators::scalar_manip_t<flags, floating_type> const &value) noexcept
{
	return print_compiler_constant_materialize(tag, value);
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<floating_type>)
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			char_type, flags, floating_type>>) noexcept
{
	// Reuse the native scalar formatter's proved type/format bound instead of
	// reporting binary128 fixed's library-wide 5,006-character maximum for every
	// proxy.  The proxy emitter has the same sign, special-value and notation
	// grammar, so this remains a reserve contract; it merely lets the print layer
	// prove that decimal/general/scientific/hex records cannot reach its large
	// immutable-fragment continuation.  Fixed binary80/binary128 retain their
	// full exponent-derived bound and therefore keep that continuation.
	return print_reserve_size(
		::fast_io::io_reserve_type<char_type,
			::fast_io::manipulators::scalar_manip_t<flags, floating_type>>);
}

/// @brief Emits an already materialized constant-float proxy into caller-owned storage.
/// @details This compiler-constant leaf is never runtime ftoa. Forced placement
///          saved 5.8--6.4 KiB on every tested GCC 11--16. Clang 17--20 instead
///          grew by about 66 KiB and expanded 16 default-field callers from 544
///          to more than 17,000 instructions; Clang 21--23 reversed again,
///          saving about 5.2 KiB and eliminating 16 residual calls. Every
///          unknown-value wrapper was instruction-identical. GNU remains
///          future-open; Clang resumes at 21 and remains open until a reversal.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<floating_type>)
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			char_type, flags, floating_type>>,
	char_type *iter,
	::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
		char_type, flags, floating_type> const &value) noexcept
{
	return ::fast_io::details::compiler_constant_floating_scalar_define<flags>(
		iter, value);
}

/// @brief Reports the exact spelling length carried by a proven constant float.
/// @details The scalar proxy stores only binary/decimal fields; even a binary128
///          fixed spelling can therefore expose its exact (possibly large)
///          destination extent without allocating or materializing a character
///          array.  Concat uses this protocol to resize its final destination
///          once instead of treating the 5,006-code-unit fixed upper bound as a
///          per-call temporary-buffer requirement.
// Placement evidence: dropping this leaf alone grows GCC 15 main from 0x3f to
// 0x113 bytes and Clang 23 from 0x4d to more than 13 KiB. Only an integer-field
// constant proxy satisfies this overload.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<floating_type>)
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::size_t
print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			char_type, flags, floating_type>>,
	::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
		char_type, flags, floating_type> const &value) noexcept
{
	return ::fast_io::details::
		compiler_constant_floating_scalar_materialized_output_size<flags>(value);
}

/// @brief Writes a proven constant float into its exact destination slice.
/// @details `precise_size` is the result of the companion size CPO.  The writer
///          deliberately reuses the same field emitter as reserve output; no
///          native floating arithmetic or second conversion is introduced.
// Placement evidence: dropping this leaf leaves a proxy define call (GCC 15
// main 0x8c, Clang 23 main 0x86 versus 0x3f/0x4d). Native ftoa is disjoint.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<floating_type>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			char_type, flags, floating_type>>,
	char_type *iter, ::std::size_t precise_size,
	::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
		char_type, flags, floating_type> const &value) noexcept
{
	(void)precise_size;
	return ::fast_io::details::compiler_constant_floating_scalar_define<flags>(
		iter, value);
}

/// @brief Returns the complete scalar spelling when it is one provider-owned immutable slice.
/// @details This is deliberately narrower than the general static-fragment protocol.  It lets direct print preserve
///          the zero-copy rodata path for values such as `2.0` before considering exact compact materialization, without
///          constructing the scalar proxy's conservative 32-descriptor graph merely to discover its actual count.
///          A zero-length result means that punctuation, sign, exponent, padding, or multiple digit-table slices are
///          required; floating scalar spellings themselves are never empty.
// Clang 23 otherwise outlines this lookup and turns the 0x4d-byte ordinary
// literal print into a 0x308f-byte fragment dispatcher. GCC already inlines it,
// but the attribute is shared because this is a constant-proxy-only CPO.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags,
	::std::integral proxy_char_type, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<
		floating_type> &&
		::std::same_as<char_type, proxy_char_type>)
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr
	::fast_io::basic_io_scatter_t<char_type>
print_compiler_constant_single_static_fragment(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			proxy_char_type, flags, floating_type>>,
	::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
		proxy_char_type, flags, floating_type> const &value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	using storage =
		::fast_io::details::compiler_constant_floating_fragment_storage<char_type>;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u)};
	if (value.binary_exponent == exponent_mask)
	{
		if (value.binary_mantissa == 0u)
		{
			if (value.negative || flags.showpos)
			{
				return {};
			}
			auto const &text{storage::template infinity<flags.uppercase>};
			return {text.data(), text.size()};
		}
		if constexpr (flags.nan_show_sign)
		{
			if (value.negative || flags.showpos)
			{
				return {};
			}
		}
		if constexpr (flags.nan_show_type)
		{
			constexpr auto quiet_bit{
				::fast_io::details::fp_quiet_nan_mantissa_mask<
					mantissa_type, trait::mbits>()};
			if ((value.negative && value.binary_mantissa == quiet_bit) ||
				::fast_io::details::fp_nan_is_signaling<
					mantissa_type, trait::mbits>(value.binary_mantissa))
			{
				return {};
			}
		}
		auto const &text{storage::template nan<flags.uppercase>};
		return {text.data(), text.size()};
	}
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		return {};
	}
	if (value.negative || flags.showpos)
	{
		return {};
	}
	if (value.binary_mantissa == 0u && value.binary_exponent == 0u)
	{
		if constexpr (flags.floating ==
				::fast_io::manipulators::floating_format::scientific ||
			flags.json_float)
		{
			return {};
		}
		return {storage::zeroes.data(), 1u};
	}
	if (!::fast_io::details::compiler_constant_floating_uses_fixed<flags>(
			value.decimal_mantissa, value.decimal_exponent) ||
		value.decimal_exponent != 0 || flags.json_float)
	{
		return {};
	}
	auto const digits{
		::fast_io::details::compiler_constant_floating_decimal_digits(
			value.decimal_mantissa)};
	if (digits == 1u)
	{
		return {storage::decimal_digits.data() +
			static_cast<::std::size_t>(value.decimal_mantissa), 1u};
	}
	if (digits == 2u)
	{
		return {storage::decimal_digit_pairs.data() +
			static_cast<::std::size_t>(value.decimal_mantissa) * 2u, 2u};
	}
	return {};
}

/// @brief Allows exact compact materialization for a scalar constant after the single-fragment rodata probe.
/// @details The scalar proxy's 5,006-code-unit ordinary reserve bound covers binary128 fixed output and is not a
///          profitability estimate.  Its precise protocol instead reports the selected value's actual spelling and
///          writes exactly that range, enabling buffered print and concat to admit short constants without a large
///          temporary.  Unknown source values never construct this proxy.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags,
	::std::integral proxy_char_type, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<
			floating_type> &&
		::std::same_as<char_type, proxy_char_type>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_prefer_precise_compact(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			proxy_char_type, flags, floating_type>>) noexcept
{
	return {};
}

/// @brief Selects expanded immutable fragments when an IO consumer must materialize a GCC constant float contiguously.
/// @details GCC 11/12 retain the scalar proxy's decimal digit loop when its precise writer feeds `to`, even after the
///          builtin query succeeds. Expanding the provider-declared fragment slots instead reduces `3.125` to five
///          immediate stores on every tested GCC 11--17. The consumer remains responsible for a type-level reserve
///          bound and an aggregate descriptor bound, so large fixed spellings cannot enter this strategy. Clang does
///          not use this marker: its independently proved 21+ path selects the precise writer.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags,
	::std::integral proxy_char_type, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_type_supported<
			floating_type> &&
		::std::same_as<char_type, proxy_char_type>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_prefer_expanded_fragments(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			proxy_char_type, flags, floating_type>>) noexcept
{
	return {};
}

/// @brief Maximum immutable-fragment descriptor count for one constant float.
/// @details Decimal and hexadecimal coefficient digits are emitted in pairs;
///          sign, radix point, base prefix, exponent prefix and special text
///          each consume at most one descriptor.  The conservative bound also
///          covers a binary128 coefficient; a complete fixed-format zero run
///          is one descriptor into the bounded immutable zero table.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags,
	::std::integral proxy_char_type, typename floating_type>
		requires(
			::fast_io::details::compiler_constant_floating_type_supported<
				floating_type>)
inline constexpr ::std::size_t print_compiler_constant_static_fragments_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			proxy_char_type, flags, floating_type>>) noexcept
{
	return 32u;
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags,
	::std::integral proxy_char_type, typename floating_type>
		requires(
			::fast_io::details::compiler_constant_floating_type_supported<
				floating_type>)
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
// GCC outlines this proxy-only bridge after `to` expands the bounded fragment slots, retaining a 1,048-byte frame,
// 540 instructions, and the complete fragment formatter in the literal caller. Forced placement is confined to the
// already-materialized proxy CPO; unknown native floats cannot select this overload.
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
print_compiler_constant_static_fragments_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			proxy_char_type, flags, floating_type>>,
	::fast_io::basic_io_scatter_t<char_type> *first,
	::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
		proxy_char_type, flags, floating_type> const &value) noexcept
{
	return ::fast_io::details::
		compiler_constant_floating_scalar_static_fragments_define<flags>(
			first, value);
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_precision_supported<
			flags, floating_type>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_precision_t<
			flags, floating_type>>) noexcept
{
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_precision_supported<
			flags, floating_type>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_precision_t<
			flags, floating_type>>) noexcept
{
	return {};
}

/// @brief Records the field-complete classification for dynamic-precision floating sources.
/// @details Value and precision have independent negative roots. Most consumers deliberately FCO this shape because a
///          successful query still leaves the planner; only an explicitly measured compiler/destination pair may admit
///          it. Clang 13--23 retain 146--292 instructions and planner calls even in the direct literal query root, so
///          no Clang frontend receives this provider proof and every semantic wrapper around it remains query-free.
///          GCC 11 through trunk reduce the same literal and independently unknown roots to immediate one/zero results.
#if defined(__GNUC__) && !defined(__clang__)
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_precision_supported<
			flags, floating_type>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_precision_t<
			flags, floating_type>>) noexcept
{
	return {};
}
#endif

/// @brief Classifies a source-level dynamic-precision floating leaf without forming its replacement type.
/// @details This type-only CPO carries no formatting or output policy. An active-condition consumer can use it before
///          the value query to apply a compiler code-generation proof to the complete condition record. Keeping the
///          classification on the source type is essential: after condition selection, the omitted-precision arm is an
///          ordinary floating scalar and no longer records that it shares a dynamic-star source with this leaf.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_precision_supported<
			flags, floating_type>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_dynamic_precision_floating_leaf(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_precision_t<
			flags, floating_type>>) noexcept
{
	return {};
}

/// @brief Proves that precision-float eligibility already bounded the exact replacement.
/// @details The value-dependent eligibility query computes the complete output
///          size and accepts only when it fits the compiler-constant byte
///          budget for `char_type`. Width and other semantic wrappers may
///          therefore reuse a true result without materializing the numeric
///          carrier merely to repeat its precise-size query.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_precision_supported<
			flags, floating_type>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_eligible_implies_compact_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_precision_t<
			flags, floating_type>>) noexcept
{
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_precision_supported<
			flags, floating_type>)
// This is the optimizer query promised by
// `print_compiler_constant_materialization_query_inline_safe`.  GCC 15
// otherwise outlines it in a lowered static-prefix + precision-float run; an
// outlined builtin query necessarily observes an opaque parameter and returns
// false, leaving the 27-byte prefix and four-byte scalar as two buffered
// operations.  Forced placement is confined to this query: for an unknown
// value either builtin predicate folds to false before the precision planner is
// formed, so the established run-time ftoa path gains no instructions.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_precision_t<flags, floating_type>>,
	::fast_io::manipulators::scalar_manip_precision_t<
		flags, floating_type> const &value) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	if (!__builtin_constant_p(value.reference) ||
		!__builtin_constant_p(value.precision) ||
		::fast_io::details::compiler_constant_floating_precision_limit<flags> <
			value.precision)
	{
		return false;
	}
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		return true;
	}
	else
	{
		auto const fields{
			::fast_io::details::compiler_constant_floating_capture_fields<
				floating_type>(
				value.reference)};
		using clean_type = ::std::remove_cv_t<floating_type>;
		using trait = ::fast_io::details::iec559_traits<clean_type>;
		constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
			(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) -
			1u)};
		::std::size_t output_size{};
		if (fields.exponent != exponent_mask &&
			(fields.mantissa != 0u || fields.exponent != 0u))
		{
#if defined(__clang__)
			auto plan{::fast_io::details::
				compiler_constant_floating_try_normal_decimal_grid_plan<
					flags, clean_type>(fields, value.precision)};
			if (!plan.success)
			{
				plan = ::fast_io::details::
					compiler_constant_floating_make_decimal_precision_plan<
						flags, clean_type>(fields, value.precision);
			}
#else
			auto const plan{::fast_io::details::
				compiler_constant_floating_make_decimal_precision_plan<
					flags, clean_type>(fields, value.precision)};
#endif
			if (plan.success)
			{
				output_size = ::fast_io::details::floating_precise_sign_size<
					flags.showpos>(static_cast<bool>(fields.sign));
				output_size = ::fast_io::details::floating_precise_add(
					output_size,
					::fast_io::details::
						compiler_constant_floating_decimal_precision_carrier_size<
							flags, clean_type>(plan.mantissa, plan.exponent,
								value.precision, static_cast<bool>(fields.sign),
								plan.rounding_discarded));
			}
			else
			{
				// A finite decimal precision replacement must own a proved compact
				// carrier.  This invariant lets the unbuffered branch describe every
				// payload byte through immutable digit/pair/zero tables; an ambiguous
				// value remains on the established exact formatter instead of
				// constructing either a 256-byte reserve frame or a local exact array.
				return false;
			}
		}
		if (!output_size)
		{
			output_size = ::fast_io::details::
				compiler_constant_floating_decimal_precision_fields_size<flags>(
					fields, value.precision);
		}
		return output_size <= ::fast_io::details::
			compiler_constant_decimal_precision_capacity<char_type>;
	}
#else
	(void)value;
	return false;
#endif
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_precision_supported<
			flags, floating_type>)
// Precision true-arm materializer. Without this placement Clang 23 expands the
// direct constant-hex caller from 0x50 to 0x1fe8 bytes; GCC 15 also increases
// object text. Dynamic precision/value controls remain byte-identical.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr auto
print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_precision_t<flags, floating_type>>,
	::fast_io::manipulators::scalar_manip_precision_t<
		flags, floating_type> const &value) noexcept
{
	using clean_type = ::std::remove_cv_t<floating_type>;
	using result_type =
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			char_type, flags, clean_type>;
	auto const fields{
		::fast_io::details::compiler_constant_floating_capture_fields<clean_type>(
			value.reference)};
	result_type result{.fields = fields, .precision = value.precision};
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		result.materialized_size = ::fast_io::details::
			compiler_constant_hex_precision_fields_size_impl<flags>(
				fields, value.precision);
	}
	else
	{
#if defined(__clang__)
		auto plan{::fast_io::details::
			compiler_constant_floating_try_normal_decimal_grid_plan<
				flags, clean_type>(fields, value.precision)};
		if (!plan.success)
		{
			plan = ::fast_io::details::
				compiler_constant_floating_make_decimal_precision_plan<
					flags, clean_type>(fields, value.precision);
		}
#else
		auto const plan{::fast_io::details::
			compiler_constant_floating_make_decimal_precision_plan<
				flags, clean_type>(fields, value.precision)};
#endif
		result.decimal_mantissa = plan.mantissa;
		result.decimal_exponent = plan.exponent;
		result.decimal_carrier_available = plan.success;
		result.decimal_rounding_discarded = plan.rounding_discarded;
		if (plan.success)
		{
			auto size{::fast_io::details::floating_precise_sign_size<
				flags.showpos>(static_cast<bool>(fields.sign))};
			result.materialized_size = ::fast_io::details::floating_precise_add(
				size,
				::fast_io::details::
					compiler_constant_floating_decimal_precision_carrier_size<
						flags, clean_type>(
							plan.mantissa, plan.exponent, value.precision,
							static_cast<bool>(fields.sign),
							plan.rounding_discarded));
		}
		else
		{
			result.materialized_size = ::fast_io::details::
				compiler_constant_floating_decimal_precision_fields_size<flags>(
					fields, value.precision);
		}
	}
	return result;
}

/// @brief Preserves a proved constant precision-float graph through GCC's forwarding boundary.
/// @details Precision materialization has a larger integer-only planning graph than the raw scalar form. A strict
///          paired assembly audit extends the required GCC interval down to 11: without this marker GCC 11/12 retain
///          the complete precision planner and its runtime fallback after the builtin query has already succeeded.
///          GCC 13--16 and Clang 21--23 likewise need this final forwarding edge to expose the completed proxy to compact
///          print/concat. The overload cannot be selected by the native runtime formatter because core invokes it only
///          after this exact source object's compiler-constant eligibility query returned true.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_precision_supported<
			flags, floating_type>)
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize_gate_proven(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::scalar_manip_precision_t<flags, floating_type>> tag,
	::fast_io::manipulators::scalar_manip_precision_t<
		flags, floating_type> const &value) noexcept
{
	return print_compiler_constant_materialize(tag, value);
}

template <::std::integral char_type, ::std::integral proxy_char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_precision_supported<
			flags, floating_type> &&
		::std::same_as<char_type, proxy_char_type>)
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			proxy_char_type, flags, floating_type>>) noexcept
{
	return ::fast_io::details::compiler_constant_floating_precision_capacity<
		char_type, flags, floating_type>();
}

template <::std::integral char_type, ::std::integral proxy_char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_precision_supported<
			flags, floating_type> &&
		::std::same_as<char_type, proxy_char_type>)
// Constant-proxy boundary only. Without forced inlining at -O3, GCC 15 grows
// the formatted literal caller from 0x72 to 0x177 bytes and Clang 23 grows it
// from 0x59 to 0xa4 bytes, both retaining an out-of-line define call. Dynamic
// precision floats use the native by-value formatter and never enter this CPO.
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			proxy_char_type, flags, floating_type>>,
	char_type *iter,
	::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
		proxy_char_type, flags, floating_type> const &value) noexcept
{
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		return ::fast_io::details::compiler_constant_hex_precision_fields_define<
			flags>(iter, value.fields, value.precision);
	}
	else
	{
		if (value.decimal_carrier_available)
		{
			iter = ::fast_io::details::print_rsv_fp_sign_impl<flags.showpos>(
				iter, static_cast<bool>(value.fields.sign));
			return ::fast_io::details::
				compiler_constant_floating_decimal_precision_carrier_define<
					flags, floating_type>(
						iter, value.decimal_mantissa, value.decimal_exponent,
						value.precision, static_cast<bool>(value.fields.sign),
						value.decimal_rounding_discarded);
		}
		return ::fast_io::details::
			compiler_constant_floating_decimal_precision_fields_define<flags>(
				iter, value.fields, value.precision);
	}
}

/// @brief Returns the exact destination extent for a constant precision float.
/// @details The decimal branch delegates to the same independently proved
///          fields/precision sizing algorithm as ordinary precise reserve.  It
///          does not format into scratch storage and therefore lets concat
///          resize exactly once.
// A/B removal grows GCC 15's formatted constant-hex caller from 0x72 to 0x178
// and Clang 23 from 0x59 to 0x3ab, with size calls left behind. The overload is
// selected only for the integer-field precision proxy.
template <::std::integral char_type, ::std::integral proxy_char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_precision_supported<
			flags, floating_type> &&
		::std::same_as<char_type, proxy_char_type>)
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::size_t
print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			proxy_char_type, flags, floating_type>>,
	::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
		proxy_char_type, flags, floating_type> const &value) noexcept
{
	if (value.materialized_size != 0u)
	{
		return value.materialized_size;
	}
	if constexpr (flags.floating ==
		::fast_io::manipulators::floating_format::hexfloat)
	{
		return ::fast_io::details::
			compiler_constant_hex_precision_fields_size_impl<flags>(
				value.fields, value.precision);
	}
	else
	{
		if (value.decimal_carrier_available)
		{
			auto size{::fast_io::details::floating_precise_sign_size<
				flags.showpos>(static_cast<bool>(value.fields.sign))};
			return ::fast_io::details::floating_precise_add(
				size,
				::fast_io::details::
					compiler_constant_floating_decimal_precision_carrier_size<
						flags, floating_type>(
							value.decimal_mantissa, value.decimal_exponent,
							value.precision, static_cast<bool>(value.fields.sign),
							value.decimal_rounding_discarded));
		}
		return ::fast_io::details::
			compiler_constant_floating_decimal_precision_fields_size<flags>(
				value.fields, value.precision);
	}
}

/// @brief Emits a constant precision float into its exact destination slice.
/// @details Native floating values never cross this proxy CPO by reference;
///          the integer fields are reconstructed only inside the selected
///          algorithm and every lower native floating formatter remains
///          register-class by value.
// A/B removal grows the same formatted literal to 0x177/0xa4 on GCC 15/Clang
// 23 and retains an out-of-line define call. Runtime precision CPOs are separate.
template <::std::integral char_type, ::std::integral proxy_char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_precision_supported<
			flags, floating_type> &&
		::std::same_as<char_type, proxy_char_type>)
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			proxy_char_type, flags, floating_type>> tag,
	char_type *iter, ::std::size_t precise_size,
	::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
		proxy_char_type, flags, floating_type> const &value) noexcept
{
	(void)tag;
	(void)precise_size;
	return print_reserve_define(
		::fast_io::io_reserve_type<char_type,
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				proxy_char_type, flags, floating_type>>,
		iter, value);
}

/// @brief Selects exact compact materialization before an expensive immutable-fragment graph is built.
/// @details A precision-float proxy advertises a conservative 32-descriptor immutable spelling so long preserving
///          fields can remain zero-copy.  For a short spelling, constructing that worst-case descriptor array first is
///          counterproductive.  This type-only strategy marker promises that the companion precise-size query is
///          pure, repeatable and non-throwing, and that its non-throwing pointer-returning precise writer emits exactly
///          the measured extent.  Direct unbuffered print may therefore measure first and, when its generic compact
///          threshold admits the value, materialize once into the selected small tier without touching descriptors.
template <::std::integral char_type, ::std::integral proxy_char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_floating_precision_supported<
			flags, floating_type> &&
		::std::same_as<char_type, proxy_char_type>)
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_prefer_precise_compact(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			proxy_char_type, flags, floating_type>>) noexcept
{
	return {};
}

/// @brief Immutable-fragment capacity for a bounded constant precision float.
/// @details Hexadecimal nibbles and decimal carrier digits are emitted in
///          pairs.  Decimal padding is one slice of the typed immutable zero
///          table, so even a 256-byte preserving spelling needs no proportional
///          descriptor run or character scratch array.
template <::std::integral char_type, ::std::integral proxy_char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
		requires(
			::fast_io::details::compiler_constant_floating_precision_supported<
				flags, floating_type>)
inline constexpr ::std::size_t print_compiler_constant_static_fragments_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			proxy_char_type, flags, floating_type>>) noexcept
{
	return 32u;
}

template <::std::integral char_type, ::std::integral proxy_char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type>
		requires(
			::fast_io::details::compiler_constant_floating_precision_supported<
				flags, floating_type>)
inline constexpr
	::fast_io::basic_io_scatter_t<char_type> *
print_compiler_constant_static_fragments_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			proxy_char_type, flags, floating_type>>,
	::fast_io::basic_io_scatter_t<char_type> *first,
	::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
		proxy_char_type, flags, floating_type> const &value) noexcept
{
	if (::fast_io::details::compiler_constant_floating_precision_limit<flags> <
		value.precision)
	{
		return first;
	}
	return ::fast_io::details::
		compiler_constant_floating_precision_static_fragments_define<flags>(
			first, value);
}

} // namespace fast_io

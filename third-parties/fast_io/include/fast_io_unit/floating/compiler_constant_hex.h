#pragma once

namespace fast_io::details
{

/*
Clang's x86 bfloat16 lowering without AVX512-BF16 is the only audited scalar
domain whose nested by-value copy can rematerialize through a narrowing
operation. Borrowing that object until its bits become an integer carrier
preserves subnormals and signaling-NaN payloads. AVX512-BF16, AArch64, and the
ordinary binary16/binary32/binary64/binary80/binary128 domains use their native
by-value ABI instead.
*/
template <typename floating_type>
inline constexpr bool compiler_constant_floating_capture_requires_object_reference{
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__) && \
	(defined(__x86_64__) || defined(_M_X64)) &&                     \
	!(defined(__arm64ec__) || defined(_M_ARM64EC)) &&               \
	!defined(__AVX512BF16__)
	::std::same_as<::std::remove_cvref_t<floating_type>, __bf16>
#else
	false
#endif
};

template <typename floating_type>
using compiler_constant_floating_capture_parameter_t = ::std::conditional_t<
	::fast_io::details::
		compiler_constant_floating_capture_requires_object_reference<floating_type>,
	::std::remove_cvref_t<floating_type> const &,
	::std::remove_cvref_t<floating_type>>;

/// @brief Captures IEC 60559 fields at a target-specific native scalar ABI boundary.
/// @details The explicit template argument makes the parameter policy visible at
///          every call site. Register-safe scalars are intentionally passed by
///          value, so float, double, native half/bfloat, binary80, and binary128
///          remain in their target floating register class. Only the simulated
///          Clang/x86 bfloat16 domain above is borrowed until its raw bits have
///          been converted to the integer field carrier.
template <typename floating_type>
[[nodiscard]] inline constexpr
	::fast_io::details::punning_result<
	::std::remove_cvref_t<floating_type>>
compiler_constant_floating_capture_fields(
	::fast_io::details::compiler_constant_floating_capture_parameter_t<
		floating_type> value) noexcept
{
	using clean_type = ::std::remove_cvref_t<floating_type>;
	using trait = ::fast_io::details::iec559_traits<clean_type>;
	using mantissa_type = typename trait::mantissa_type;
#if defined(__SIZEOF_FLOAT80__) ||                              \
	(defined(__LDBL_MANT_DIG__) && defined(__LDBL_MAX_EXP__) && \
	 __LDBL_MANT_DIG__ == 64 && __LDBL_MAX_EXP__ == 16384)
	if constexpr (::fast_io::details::fp_floating_point_is_float80<clean_type> &&
				  ::std::endian::native == ::std::endian::little)
	{
		using storage_type = ::fast_io::details::float80_storage<
			sizeof(clean_type) - sizeof(::std::uint_least64_t) -
			sizeof(::std::uint_least16_t)>;
		auto const unwrap{::fast_io::bit_cast<storage_type>(value)};
		constexpr ::std::uint_least64_t explicit_integer_bit{
			::std::uint_least64_t{1u} << 63u};
		return {
			unwrap.mantissa & static_cast<::std::uint_least64_t>(
								  explicit_integer_bit - 1u),
			static_cast<::std::uint_least32_t>(unwrap.exponent & 0x7fffu),
			static_cast<bool>((unwrap.exponent >> 15u) & 1u)};
	}
	else
#endif
	{
		static_assert(sizeof(clean_type) == sizeof(mantissa_type));
		auto const unwrap{::fast_io::bit_cast<mantissa_type>(value)};
		constexpr auto mantissa_mask{
			(static_cast<mantissa_type>(1u) << trait::mbits) - 1u};
		constexpr auto exponent_mask{
			(static_cast<mantissa_type>(1u) << trait::ebits) - 1u};
		return {
			static_cast<mantissa_type>(unwrap & mantissa_mask),
			static_cast<::std::uint_least32_t>(
				(unwrap >> trait::mbits) & exponent_mask),
			static_cast<bool>(
				(unwrap >> (trait::mbits + trait::ebits)) & 1u)};
	}
}

// The uniform limit keeps the largest spelling below 52 code units:
// sign + base prefix + 41 significant digits (fractional P=40) + radix point
// + `p` + exponent sign + five binary-exponent digits.  It therefore covers
// binary16/bfloat16/binary32/binary64/binary80/binary128 and all five public
// character domains with one auditable reserve contract.
inline constexpr ::std::size_t compiler_constant_hex_precision_limit{40u};
inline constexpr ::std::size_t compiler_constant_hex_capacity{52u};

template <typename floating_type>
concept compiler_constant_hex_has_iec559_traits = requires {
	typename ::fast_io::details::iec559_traits<
		::std::remove_cv_t<floating_type>>::mantissa_type;
};

template <::fast_io::manipulators::floating_precision precision>
inline constexpr bool compiler_constant_hex_precision_mode_valid{
	precision == ::fast_io::manipulators::floating_precision::significant ||
	precision == ::fast_io::manipulators::floating_precision::fractional ||
	precision == ::fast_io::manipulators::floating_precision::significant_preserve_trailing_zero ||
	precision == ::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero ||
	precision == ::fast_io::manipulators::floating_precision::charconv_hex_fractional};

template <::fast_io::manipulators::floating_rounding rounding>
inline constexpr bool compiler_constant_hex_rounding_valid{
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

template <::fast_io::manipulators::scalar_flags flags, typename floating_type>
inline constexpr bool compiler_constant_hex_scalar_supported{
	::fast_io::details::compiler_constant_hex_has_iec559_traits<floating_type> &&
	flags.base == 10u &&
	flags.floating == ::fast_io::manipulators::floating_format::hexfloat &&
	flags.percentage == ::fast_io::manipulators::percentage_flag::none &&
	!flags.json_float};

template <::fast_io::manipulators::scalar_flags flags, typename floating_type>
inline constexpr bool compiler_constant_hex_precision_supported{
	::fast_io::details::compiler_constant_hex_scalar_supported<flags, floating_type> &&
	::fast_io::details::compiler_constant_hex_precision_mode_valid<flags.precision> &&
	::fast_io::details::compiler_constant_hex_rounding_valid<flags.rounding> &&
	flags.rounding !=
		::fast_io::manipulators::floating_rounding::current_environment};

template <::fast_io::manipulators::scalar_flags flags,
	::fast_io::manipulators::floating_rounding rounding>
inline constexpr auto compiler_constant_hex_replace_rounding{[]() constexpr noexcept {
	auto adjusted{flags};
	adjusted.rounding = rounding;
	return adjusted;
}()};

[[nodiscard]] inline constexpr ::std::size_t
compiler_constant_hex_size_add(::std::size_t left, ::std::size_t right) noexcept
{
	constexpr auto maximum{(::std::numeric_limits<::std::size_t>::max)()};
	return maximum - left < right ? maximum : left + right;
}

[[nodiscard]] inline constexpr ::std::size_t
compiler_constant_hex_exponent_size(::std::int_least32_t exponent) noexcept
{
	auto magnitude{static_cast<::std::uint_least32_t>(
		exponent < 0 ? -static_cast<::std::int_least64_t>(exponent) : exponent)};
	::std::size_t digits{1u};
	for (; 10u <= magnitude; magnitude /= 10u)
	{
		++digits;
	}
	return 2u + digits; // exponent marker and mandatory sign
}

/// @brief Emits the signed binary exponent without entering the run-time hex-float formatter.
/// @details The compiler-constant plan has already reduced the exponent to a
///          bounded integer. Keeping this small decimal writer in the constant
///          protocol lets Clang fold literal hex floats while the ordinary
///          by-value floating formatter retains its independent register ABI.
// A/B: without this leaf placement Clang 23 emits a call and stack frame in the
// formatted literal (0x59 -> 0x64 bytes); GCC 15 is unchanged. The function
// accepts integer exponent state only and is unreachable from native hexfloat.
template <::std::integral char_type>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
compiler_constant_hex_write_exponent(
	char_type *iter, ::std::int_least32_t exponent) noexcept
{
	bool const negative{exponent < 0};
	*iter++ = negative ? ::fast_io::char_literal_v<u8'-', char_type>
					   : ::fast_io::char_literal_v<u8'+', char_type>;
	auto magnitude{static_cast<::std::uint_least32_t>(
		negative ? -static_cast<::std::int_least64_t>(exponent) : exponent)};
	::std::size_t digits{1u};
	for (auto value{magnitude}; 10u <= value; value /= 10u)
	{
		++digits;
	}
	auto *const end{iter + digits};
	for (auto *current{end}; current != iter; magnitude /= 10u)
	{
		*--current = ::fast_io::char_literal_add<char_type>(magnitude % 10u);
	}
	return end;
}

/// @brief Integer-only rounded hexadecimal digit plan shared by contiguous and
///        immutable-fragment compiler-constant emitters.
/// @details The plan deliberately contains nibble values, not encoded output
///          characters.  It is therefore safe to keep as transient optimizer
///          state while both emitters obtain their actual payload from their
///          own character-domain storage.  Rounding is delegated to the same
///          established print_rsvhexfloat_round_up policy used by the ordinary
///          formatter; this layer does not introduce a second rounding rule.
template <typename floating_type>
struct compiler_constant_hex_precision_plan
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	inline static constexpr ::std::size_t fractional_hex_digits{
		(trait::mbits + 3u) / 4u};
	inline static constexpr ::std::size_t total_hex_digits{
		fractional_hex_digits + 1u};
	// Keep the fractional coefficient as one aligned integer.  The former
	// per-nibble local array defeated Clang's constant propagation even after
	// forced inlining, leaving a loop and a large frame in a proven-constant
	// print.  Integer extraction is both representation-exact and a much smaller
	// optimizer graph for binary16 through binary128.
	mantissa_type fractional{};
	::std::size_t retained_digits{};
	::std::size_t digits_to_print{};
	::std::int_least32_t binary_exponent{};
	::std::uint_least8_t leading_digit{};
};

template <typename floating_type>
[[nodiscard]] inline constexpr
	::std::uint_least32_t
compiler_constant_hex_precision_plan_digit(
	::fast_io::details::compiler_constant_hex_precision_plan<floating_type> const &
		plan,
	::std::size_t index) noexcept
{
	using plan_type =
		::fast_io::details::compiler_constant_hex_precision_plan<floating_type>;
	using mantissa_type = typename plan_type::mantissa_type;
	if (index == 0u)
	{
		return plan.leading_digit;
	}
	auto const shift{static_cast<::std::uint_least32_t>(
		(plan_type::fractional_hex_digits - index) * 4u)};
	return static_cast<::std::uint_least32_t>(
		(plan.fractional >> shift) & static_cast<mantissa_type>(0x0fu));
}

template <::fast_io::manipulators::scalar_flags flags, typename floating_type>
		requires(::fast_io::details::compiler_constant_hex_precision_supported<
			flags, floating_type>)
// Constant-proxy planning leaf. An actual-emitter O3 A/B matrix keeps forced
// inlining for GCC 11--16: total text improved or stayed identical in every
// release, with GCC 16 saving 177 bytes, so later GCC versions inherit that
// latest positive result. Clang 18--22 instead grew by 16--67 bytes; Clang 23
// reversed the result and saved 366 bytes, so only Clang 23 and later inherit
// the new policy. Clang 17 was byte-identical and needs no forced placement.
// No native scalar crosses this integer-field planning API. GCC 11 and Clang
// 17 are the oldest tested endpoints; older frontends are not extrapolated.
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 23 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr auto
compiler_constant_hex_make_precision_plan(
	::fast_io::details::punning_result<floating_type> fields,
	::std::size_t precision) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	using plan_type =
		::fast_io::details::compiler_constant_hex_precision_plan<floating_type>;
	auto const [mantissa, exponent, negative]{fields};
	constexpr bool fractional_precision{
		::fast_io::details::floating_precision_is_fractional<flags.precision>};
	constexpr bool preserve_trailing_zero{
		::fast_io::details::floating_precision_preserves_trailing_zero<
			flags.precision>};
	auto total_precision{precision};
	if constexpr (fractional_precision)
	{
		constexpr auto maximum{(::std::numeric_limits<::std::size_t>::max)()};
		if (total_precision != maximum)
		{
			++total_precision;
		}
	}
	else if (total_precision == 0u)
	{
		total_precision = 1u;
	}

	plan_type plan{};
	if (mantissa == 0u && exponent == 0u)
	{
		plan.retained_digits = 1u;
		plan.digits_to_print = preserve_trailing_zero ? total_precision : 1u;
		return plan;
	}

	constexpr auto bias{static_cast<::std::int_least32_t>(
		(static_cast<::std::uint_least32_t>(1u) << (trait::ebits - 1u)) - 1u)};
	plan.binary_exponent = exponent == 0u
		? static_cast<::std::int_least32_t>(1 - bias)
		: static_cast<::std::int_least32_t>(exponent) - bias;
	constexpr ::std::size_t fractional_hex_digits{
		plan_type::fractional_hex_digits};
	constexpr ::std::size_t total_hex_digits{plan_type::total_hex_digits};
	constexpr ::std::size_t padding_bits{
		fractional_hex_digits * 4u - trait::mbits};
	auto const aligned_mantissa{
		static_cast<mantissa_type>(mantissa << padding_bits)};
	// Init-capture keeps this C++20 path usable on frontends which cannot directly capture a structured-binding name.
	auto const source_digit_at = [aligned_mantissa, exponent_value = exponent](
		::std::size_t index) constexpr noexcept -> ::std::uint_least32_t {
		if (index == 0u)
		{
			return exponent_value == 0u ? 0u : 1u;
		}
		if (fractional_hex_digits < index)
		{
			return 0u;
		}
		auto const shift{static_cast<::std::uint_least32_t>(
			(fractional_hex_digits - index) * 4u)};
		return static_cast<::std::uint_least32_t>(
			(aligned_mantissa >> shift) & static_cast<mantissa_type>(0x0fu));
	};
	plan.retained_digits =
		total_precision < total_hex_digits ? total_precision : total_hex_digits;
	plan.leading_digit = static_cast<::std::uint_least8_t>(
		exponent == 0u ? 0u : 1u);
	auto const retained_fractional_digits{
		plan.retained_digits - 1u};
	if (retained_fractional_digits == fractional_hex_digits)
	{
		plan.fractional = aligned_mantissa;
	}
	else if (retained_fractional_digits != 0u)
	{
		auto const discarded_bits{static_cast<::std::uint_least32_t>(
			(fractional_hex_digits - retained_fractional_digits) * 4u)};
		plan.fractional = static_cast<mantissa_type>(
			(aligned_mantissa >> discarded_bits) << discarded_bits);
	}
	if (total_precision < total_hex_digits)
	{
		auto const next_digit{source_digit_at(total_precision)};
		auto const remaining_nibbles{
			fractional_hex_digits - total_precision};
		bool tail_nonzero{};
		if (remaining_nibbles != 0u)
		{
			auto const remaining_bits{static_cast<::std::uint_least32_t>(
				remaining_nibbles * 4u)};
			auto const tail_mask{static_cast<mantissa_type>(
				(static_cast<mantissa_type>(1u) << remaining_bits) - 1u)};
			tail_nonzero = (aligned_mantissa & tail_mask) != 0u;
		}
		if (::fast_io::details::print_rsvhexfloat_round_up<flags.rounding>(
				negative,
				::fast_io::details::compiler_constant_hex_precision_plan_digit(
					plan, total_precision - 1u),
				next_digit,
				tail_nonzero))
		{
			if (retained_fractional_digits == 0u)
			{
				++plan.leading_digit;
			}
			else
			{
				auto const increment_shift{static_cast<::std::uint_least32_t>(
					(fractional_hex_digits - retained_fractional_digits) * 4u)};
				auto const increment{static_cast<mantissa_type>(
					static_cast<mantissa_type>(1u) << increment_shift)};
				constexpr auto fractional_bits{fractional_hex_digits * 4u};
				constexpr auto fractional_maximum{[]() constexpr noexcept {
					if constexpr (fractional_bits == static_cast<::std::size_t>(
						::std::numeric_limits<mantissa_type>::digits))
					{
						return static_cast<mantissa_type>(
							~static_cast<mantissa_type>(0u));
					}
					else
					{
						return static_cast<mantissa_type>(
							(static_cast<mantissa_type>(1u) << fractional_bits) - 1u);
					}
				}()};
				// The aligned hexadecimal fraction often occupies fewer bits than
				// its uint16/32/128 carrier.  Overflow must therefore be measured
				// against the fractional field, not the C++ integer maximum; bits
				// above that field are ignored by digit extraction and previously
				// lost the carry which normalizes 0xffff... to 1p(E+1).
				if (fractional_maximum - plan.fractional < increment)
				{
					plan.fractional = 0u;
					++plan.leading_digit;
				}
				else
				{
					plan.fractional = static_cast<mantissa_type>(
						plan.fractional + increment);
				}
			}
			/*
			Keep the constant proxy observationally equivalent to the runtime
			hexadecimal precision emitter.  Both carriers denote the same real
			number after a leading carry; only charconv's printf-a spelling keeps
			2p+E instead of normalizing to 1p+(E+1).
			*/
			if constexpr (
				flags.precision !=
				::fast_io::manipulators::floating_precision::
					charconv_hex_fractional)
			{
				if (exponent != 0u && plan.leading_digit == 2u)
				{
					plan.leading_digit = 1u;
					plan.fractional = 0u;
					++plan.binary_exponent;
				}
			}
		}
	}
	plan.digits_to_print = plan.retained_digits;
	if constexpr (preserve_trailing_zero)
	{
		plan.digits_to_print = total_precision;
	}
	else
	{
		while (1u < plan.digits_to_print &&
			::fast_io::details::compiler_constant_hex_precision_plan_digit(
				plan, plan.digits_to_print - 1u) == 0u)
		{
			--plan.digits_to_print;
		}
	}
	return plan;
}

template <::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(::fast_io::details::compiler_constant_hex_precision_supported<
		flags, floating_type>)
// Exact-size leaf for integer-field precision proxies. Without forced inlining
// Clang 23 grows direct literal output from 0x50 to 0x244 bytes and retains a
// size call; dynamic controls are byte-identical.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::size_t
compiler_constant_hex_precision_fields_size_impl(
	::fast_io::details::punning_result<floating_type> fields,
	::std::size_t precision) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	using mantissa_type = typename trait::mantissa_type;
	auto const [mantissa, exponent, negative]{fields};
	constexpr auto exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	if (exponent == exponent_mask)
	{
		if (mantissa == 0u)
		{
			return static_cast<::std::size_t>(flags.showpos || negative) + 3u;
		}
		::std::size_t size{3u};
		if constexpr (flags.nan_show_sign)
		{
			size += static_cast<::std::size_t>(flags.showpos || negative);
		}
		if constexpr (flags.nan_show_type)
		{
			constexpr auto quiet_bit{
				::fast_io::details::fp_quiet_nan_mantissa_mask<
					mantissa_type, trait::mbits>()};
			if (negative && mantissa == quiet_bit)
			{
				size += 5u;
			}
			else if (::fast_io::details::fp_nan_is_signaling<
				mantissa_type, trait::mbits>(mantissa))
			{
				size += 6u;
			}
		}
		return size;
	}

	auto size{static_cast<::std::size_t>(flags.showpos || negative) +
		static_cast<::std::size_t>(flags.showbase) * 2u};
	auto const plan{
		::fast_io::details::compiler_constant_hex_make_precision_plan<flags>(
			fields, precision)};
	size = ::fast_io::details::compiler_constant_hex_size_add(
		size, plan.digits_to_print);
	if (1u < plan.digits_to_print)
	{
		size = ::fast_io::details::compiler_constant_hex_size_add(size, 1u);
	}
	return ::fast_io::details::compiler_constant_hex_size_add(
		size, ::fast_io::details::compiler_constant_hex_exponent_size(
			plan.binary_exponent));
}

template <::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_hex_scalar_supported<flags, floating_type> &&
		::fast_io::details::compiler_constant_hex_precision_mode_valid<flags.precision> &&
		::fast_io::details::compiler_constant_hex_rounding_valid<flags.rounding>)
[[nodiscard]] inline constexpr ::std::size_t
compiler_constant_hex_precision_fields_runtime_size(
	::fast_io::details::punning_result<floating_type> fields,
	::std::size_t precision) noexcept
{
	if constexpr (flags.rounding ==
		::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return ::fast_io::details::compiler_constant_hex_precision_fields_size_impl<
				::fast_io::details::compiler_constant_hex_replace_rounding<
					flags, ::fast_io::manipulators::floating_rounding::toward_plus_infinity>>(
					fields, precision);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return ::fast_io::details::compiler_constant_hex_precision_fields_size_impl<
				::fast_io::details::compiler_constant_hex_replace_rounding<
					flags, ::fast_io::manipulators::floating_rounding::toward_minus_infinity>>(
					fields, precision);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return ::fast_io::details::compiler_constant_hex_precision_fields_size_impl<
				::fast_io::details::compiler_constant_hex_replace_rounding<
					flags, ::fast_io::manipulators::floating_rounding::toward_zero>>(
					fields, precision);
		default:
			return ::fast_io::details::compiler_constant_hex_precision_fields_size_impl<
				::fast_io::details::compiler_constant_hex_replace_rounding<
					flags, ::fast_io::manipulators::floating_rounding::nearest_to_even>>(
					fields, precision);
		}
	}
	else
	{
		return ::fast_io::details::compiler_constant_hex_precision_fields_size_impl<
			flags>(fields, precision);
	}
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type, typename floating_type>
	requires(::fast_io::details::compiler_constant_hex_precision_supported<
		flags, floating_type>)
// Integer-field emitter leaf. Removing this attribute grows GCC 15 direct and
// formatted literal symbols from 0x82/0x72 to 0x16d/0x366. Native hexfloat
// formatting still uses its independent by-value implementation.
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *compiler_constant_hex_precision_fields_define_impl(
	char_type *iter,
	::fast_io::details::punning_result<floating_type> fields,
	::std::size_t precision) noexcept
{
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	auto const [mantissa, exponent, negative]{fields};
	constexpr ::std::uint_least32_t exponent_mask{
		(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
	if (exponent == exponent_mask)
	{
		return ::fast_io::details::prsv_fp_nan_impl<
			flags.showpos, flags.uppercase, flags.nan_show_sign,
			flags.nan_show_type, trait::mbits>(iter, mantissa, negative);
	}

	iter = ::fast_io::details::print_rsv_fp_sign_impl<flags.showpos>(
		iter, negative);
	if constexpr (flags.showbase)
	{
		iter = ::fast_io::details::print_reserve_show_base_impl<
			16u, flags.uppercase_showbase, false>(iter);
	}
	auto const plan{
		::fast_io::details::compiler_constant_hex_make_precision_plan<flags>(
			fields, precision)};
	iter = ::fast_io::details::print_rsvhexfloat_digit_impl<flags.uppercase>(
		iter,
		::fast_io::details::compiler_constant_hex_precision_plan_digit(
			plan, 0u));
	if (1u < plan.digits_to_print)
	{
		*iter++ = ::fast_io::char_literal_v<
			(flags.comma ? u8',' : u8'.'), char_type>;
		auto const digit_limit{
			plan.digits_to_print < plan.retained_digits
				? plan.digits_to_print
				: plan.retained_digits};
		for (::std::size_t index{1u}; index != digit_limit; ++index)
		{
			iter = ::fast_io::details::print_rsvhexfloat_digit_impl<flags.uppercase>(
				iter,
				::fast_io::details::compiler_constant_hex_precision_plan_digit(
					plan, index));
		}
		if (digit_limit < plan.digits_to_print)
		{
			iter = ::fast_io::details::my_fill_n(
				iter, plan.digits_to_print - digit_limit,
				::fast_io::char_literal_v<u8'0', char_type>);
		}
	}
	*iter++ = ::fast_io::char_literal_v<
		(flags.uppercase_e ? u8'P' : u8'p'), char_type>;
	return ::fast_io::details::compiler_constant_hex_write_exponent(
		iter, plan.binary_exponent);
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type, typename floating_type>
	requires(::fast_io::details::compiler_constant_hex_precision_supported<
		flags, floating_type>)
// Checked constant-proxy adapter. With only this placement removed, GCC 15
// leaves an outer call and grows direct/formatted literals to 0x16d/0x1b7;
// Clang 23 grows them to 0x78/0x84. Runtime controls do not use this adapter.
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *compiler_constant_hex_precision_fields_define(
	char_type *iter, ::fast_io::details::punning_result<floating_type> fields,
	::std::size_t precision) noexcept
{
	if (::fast_io::details::compiler_constant_hex_precision_limit < precision)
	{
		return iter;
	}
	return ::fast_io::details::compiler_constant_hex_precision_fields_define_impl<
		flags>(iter, fields, precision);
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type, typename floating_type>
	requires(
		::fast_io::details::compiler_constant_hex_scalar_supported<flags, floating_type> &&
		::fast_io::details::compiler_constant_hex_precision_mode_valid<flags.precision> &&
		::fast_io::details::compiler_constant_hex_rounding_valid<flags.rounding>)
inline constexpr char_type *compiler_constant_hex_precision_fields_runtime_define(
	char_type *iter, ::fast_io::details::punning_result<floating_type> fields,
	::std::size_t precision) noexcept
{
	if constexpr (flags.rounding ==
		::fast_io::manipulators::floating_rounding::current_environment)
	{
		switch (::fast_io::details::current_floating_rounding())
		{
		case ::fast_io::manipulators::floating_rounding::toward_plus_infinity:
			return ::fast_io::details::compiler_constant_hex_precision_fields_define_impl<
				::fast_io::details::compiler_constant_hex_replace_rounding<
					flags, ::fast_io::manipulators::floating_rounding::toward_plus_infinity>>(
					iter, fields, precision);
		case ::fast_io::manipulators::floating_rounding::toward_minus_infinity:
			return ::fast_io::details::compiler_constant_hex_precision_fields_define_impl<
				::fast_io::details::compiler_constant_hex_replace_rounding<
					flags, ::fast_io::manipulators::floating_rounding::toward_minus_infinity>>(
					iter, fields, precision);
		case ::fast_io::manipulators::floating_rounding::toward_zero:
			return ::fast_io::details::compiler_constant_hex_precision_fields_define_impl<
				::fast_io::details::compiler_constant_hex_replace_rounding<
					flags, ::fast_io::manipulators::floating_rounding::toward_zero>>(
					iter, fields, precision);
		default:
			return ::fast_io::details::compiler_constant_hex_precision_fields_define_impl<
				::fast_io::details::compiler_constant_hex_replace_rounding<
					flags, ::fast_io::manipulators::floating_rounding::nearest_to_even>>(
					iter, fields, precision);
		}
	}
	else
	{
		return ::fast_io::details::compiler_constant_hex_precision_fields_define_impl<
			flags>(iter, fields, precision);
	}
}

template <::fast_io::manipulators::scalar_flags flags,
	::std::integral char_type, typename floating_type>
	requires(::fast_io::details::compiler_constant_hex_scalar_supported<
		flags, floating_type>)
inline constexpr char_type *compiler_constant_hex_scalar_fields_define(
	char_type *iter,
	::fast_io::details::punning_result<floating_type> fields) noexcept
{
	constexpr auto precision_flags{[]() constexpr noexcept {
		auto adjusted{flags};
		adjusted.rounding =
			::fast_io::manipulators::floating_rounding::nearest_to_even;
		adjusted.precision =
			::fast_io::manipulators::floating_precision::significant;
		return adjusted;
	}()};
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr ::std::size_t exact_digits{(trait::mbits + 3u) / 4u + 1u};
	return ::fast_io::details::compiler_constant_hex_precision_fields_define<
		precision_flags>(iter, fields, exact_digits);
}

template <::fast_io::manipulators::scalar_flags flags, typename floating_type>
	requires(::fast_io::details::compiler_constant_hex_scalar_supported<
		flags, floating_type>)
[[nodiscard]] inline constexpr ::std::size_t
compiler_constant_hex_scalar_fields_size(
	::fast_io::details::punning_result<floating_type> fields) noexcept
{
	constexpr auto precision_flags{[]() constexpr noexcept {
		auto adjusted{flags};
		adjusted.rounding =
			::fast_io::manipulators::floating_rounding::nearest_to_even;
		adjusted.precision =
			::fast_io::manipulators::floating_precision::significant;
		return adjusted;
	}()};
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	constexpr ::std::size_t exact_digits{(trait::mbits + 3u) / 4u + 1u};
	return ::fast_io::details::compiler_constant_hex_precision_fields_size_impl<
		precision_flags>(fields, exact_digits);
}

} // namespace fast_io::details

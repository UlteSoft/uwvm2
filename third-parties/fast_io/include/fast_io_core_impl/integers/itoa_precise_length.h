#pragma once

namespace fast_io::details
{

/// @brief Identifies the prefix spelling selected for a formatted integer.
enum class itoa_prefix_kind : unsigned char
{
	none,
	binary_lower,
	binary_upper,
	ternary_lower,
	ternary_upper,
	octal_zero,
	octal_c2y_lower,
	octal_c2y_upper,
	hex_lower,
	hex_upper,
	generic_single_digit_base,
	generic_double_digit_base
};

/// @brief Describes every length component of a formatted integer.
struct itoa_precise_layout
{
	char sign_character{};
	itoa_prefix_kind prefix{};
	::std::size_t sign_size{};
	::std::size_t prefix_size{};
	::std::size_t digits_size{};

	/// @return The exact number of output characters in the complete formatted integer.
	inline constexpr ::std::size_t total_size() const noexcept
	{
		return sign_size + prefix_size + digits_size;
	}
};

/// @brief Extracts decimal digit selection from the 32-bit JEAIII conversion ranges.
/// @details The branch structure matches jeaiii_first_two and the short forms of
/// jeaiii_range4, jeaiii_range6, jeaiii_range8, and jeaiii_range10.
inline constexpr ::std::size_t itoa_jeaiii_decimal_digits_u32(::std::uint_least32_t value) noexcept
{
	if (value < 10000u)
	{
		// The first JEAIII range handles one through four decimal digits.
		if (value < 100u)
		{
			return 1u + static_cast<::std::size_t>(value >= 10u);
		}
		return 3u + static_cast<::std::size_t>(value >= 1000u);
	}
	if (value < 1000000u)
	{
		// range6 distinguishes five- and six-digit values with one comparison.
		return 5u + static_cast<::std::size_t>(value >= 100000u);
	}
	if (value < 100000000u)
	{
		// range8 distinguishes seven- and eight-digit values with one comparison.
		return 7u + static_cast<::std::size_t>(value >= 10000000u);
	}
	// range10 covers the remaining nine- and ten-digit uint32 values.
	return 9u + static_cast<::std::size_t>(value >= 1000000000u);
}

/// @brief Extracts the non-recursive 64-bit JEAIII decimal length control flow.
/// @details Large values retain the converter's eight- and sixteen-digit decomposition. This is usually faster than a
/// threshold-only tree for ordinary small values, counters, and uniformly distributed machine integers.
inline constexpr ::std::size_t itoa_jeaiii_decimal_digits_u64(::std::uint_least64_t value) noexcept
{
	constexpr ::std::uint_least64_t divisor8{100000000u};
	if (value < divisor8)
	{
		// Values below 10^8 use the exact 32-bit JEAIII range tree.
		return ::fast_io::details::itoa_jeaiii_decimal_digits_u32(
			static_cast<::std::uint_least32_t>(value));
	}
	if (value < static_cast<::std::uint_least64_t>(1000000000u))
	{
		// JEAIII handles this interval as a fixed nine-digit range on every supported architecture.
		return 9u;
	}
	::std::uint_least64_t const high{value / divisor8};
	if (high < divisor8)
	{
		// The low group contributes exactly eight digits; only the leading group needs detection.
		return ::fast_io::details::itoa_jeaiii_decimal_digits_u32(
				   static_cast<::std::uint_least32_t>(high)) +
			   8u;
	}
	constexpr ::std::uint_least64_t divisor16{divisor8 * divisor8};
	// Two fixed eight-digit groups leave at most four leading digits for uint64.
	return ::fast_io::details::itoa_jeaiii_decimal_digits_u32(
			   static_cast<::std::uint_least32_t>(value / divisor16)) +
		   16u;
}

/// @brief Provides a division-free decimal threshold tree for strategy experiments.
/// @details This alternative is useful when decimal magnitudes are approximately uniform. The default precise-length
/// path uses the converter-matching JEAIII decomposition above.
inline constexpr ::std::size_t itoa_jeaiii_decimal_digits_u64_tree(::std::uint_least64_t value) noexcept
{
	if (value < static_cast<::std::uint_least64_t>(100000000u))
	{
		return ::fast_io::details::itoa_jeaiii_decimal_digits_u32(
			static_cast<::std::uint_least32_t>(value));
	}
	if (value < static_cast<::std::uint_least64_t>(1000000000000u))
	{
		if (value < static_cast<::std::uint_least64_t>(10000000000u))
		{
			return 9u + static_cast<::std::size_t>(value >= static_cast<::std::uint_least64_t>(1000000000u));
		}
		return 11u + static_cast<::std::size_t>(value >= static_cast<::std::uint_least64_t>(100000000000u));
	}
	if (value < static_cast<::std::uint_least64_t>(10000000000000000u))
	{
		if (value < static_cast<::std::uint_least64_t>(100000000000000u))
		{
			return 13u + static_cast<::std::size_t>(value >= static_cast<::std::uint_least64_t>(10000000000000u));
		}
		return 15u + static_cast<::std::size_t>(value >= static_cast<::std::uint_least64_t>(1000000000000000u));
	}
	if (value < static_cast<::std::uint_least64_t>(1000000000000000000u))
	{
		return 17u + static_cast<::std::size_t>(value >= static_cast<::std::uint_least64_t>(100000000000000000u));
	}
	return 19u +
		   static_cast<::std::size_t>(value >= static_cast<::std::uint_least64_t>(10000000000000000000ull));
}

/// @brief Computes the exact digit count for an unsigned integer in the requested base.
/// @details Decimal integers up to 64 bits use the extracted JEAIII detector. Wider decimal integers and non-decimal
/// bases reuse chars_len so their established uint128, power-of-two, and base^4 algorithms remain unchanged.
template <::std::size_t base, ::fast_io::details::my_unsigned_integral U>
	requires(2u <= base && base <= 36u)
inline constexpr ::std::size_t itoa_precise_digit_count(U value) noexcept
{
	if constexpr (base == 10u && sizeof(U) <= sizeof(::std::uint_least32_t))
	{
		return ::fast_io::details::itoa_jeaiii_decimal_digits_u32(
			static_cast<::std::uint_least32_t>(value));
	}
	else if constexpr (base == 10u && sizeof(U) <= sizeof(::std::uint_least64_t))
	{
		return ::fast_io::details::itoa_jeaiii_decimal_digits_u64(
			static_cast<::std::uint_least64_t>(value));
	}
	else
	{
		return ::fast_io::details::chars_len<base, false>(value);
	}
}

/// @brief Converts a signed or unsigned integer to its unsigned magnitude without signed overflow.
template <::fast_io::details::my_integral T>
	requires(!::std::same_as<::std::remove_cvref_t<T>, bool>)
inline constexpr auto itoa_unsigned_magnitude(T value) noexcept
{
	using unsigned_type = ::fast_io::details::my_make_unsigned_t<T>;
	unsigned_type magnitude{static_cast<unsigned_type>(value)};
	if constexpr (::fast_io::details::my_signed_integral<T>)
	{
		if (value < 0)
		{
			// Unsigned subtraction represents abs(value) correctly even for the minimum signed value.
			magnitude = static_cast<unsigned_type>(unsigned_type{} - magnitude);
		}
	}
	return magnitude;
}

/// @brief Selects the prefix spelling for an integer format at compile time.
template <::std::size_t base, bool showbase, bool uppercase_showbase, bool oct_c2y>
	requires(2u <= base && base <= 36u)
inline consteval itoa_prefix_kind itoa_select_prefix() noexcept
{
	if constexpr (!showbase || base == 10u)
	{
		return itoa_prefix_kind::none;
	}
	else if constexpr (base == 2u)
	{
		return uppercase_showbase ? itoa_prefix_kind::binary_upper : itoa_prefix_kind::binary_lower;
	}
	else if constexpr (base == 3u)
	{
		return uppercase_showbase ? itoa_prefix_kind::ternary_upper : itoa_prefix_kind::ternary_lower;
	}
	else if constexpr (base == 8u)
	{
		if constexpr (oct_c2y)
		{
			return uppercase_showbase ? itoa_prefix_kind::octal_c2y_upper : itoa_prefix_kind::octal_c2y_lower;
		}
		else
		{
			return itoa_prefix_kind::octal_zero;
		}
	}
	else if constexpr (base == 16u)
	{
		return uppercase_showbase ? itoa_prefix_kind::hex_upper : itoa_prefix_kind::hex_lower;
	}
	else if constexpr (base < 10u)
	{
		return itoa_prefix_kind::generic_single_digit_base;
	}
	else
	{
		return itoa_prefix_kind::generic_double_digit_base;
	}
}

/// @brief Returns the character length of a selected integer prefix.
inline constexpr ::std::size_t itoa_prefix_size(itoa_prefix_kind prefix) noexcept
{
	switch (prefix)
	{
	case itoa_prefix_kind::none:
		return 0u;
	case itoa_prefix_kind::octal_zero:
		return 1u;
	case itoa_prefix_kind::generic_single_digit_base:
		return 4u;
	case itoa_prefix_kind::generic_double_digit_base:
		return 5u;
	default:
		return 2u;
	}
}

/// @brief Predicts the exact sign, prefix, digit, and total lengths of a formatted integer.
template <::std::size_t base, bool showbase = false, bool uppercase_showbase = false, bool showpos = false,
		  bool oct_c2y = false, ::fast_io::details::my_integral T>
	requires(2u <= base && base <= 36u && !::std::same_as<::std::remove_cvref_t<T>, bool>)
inline constexpr itoa_precise_layout itoa_predict_precise_layout(T value) noexcept
{
	itoa_precise_layout layout;
	if constexpr (showpos)
	{
		// showpos always emits exactly one sign, using '-' for negative signed values and '+' otherwise.
		layout.sign_character = ::fast_io::details::my_signed_integral<T> && value < 0 ? '-' : '+';
		layout.sign_size = 1u;
	}
	else if constexpr (::fast_io::details::my_signed_integral<T>)
	{
		if (value < 0)
		{
			layout.sign_character = '-';
			layout.sign_size = 1u;
		}
	}
	layout.prefix = ::fast_io::details::itoa_select_prefix<base, showbase, uppercase_showbase, oct_c2y>();
	layout.prefix_size = ::fast_io::details::itoa_prefix_size(layout.prefix);
	layout.digits_size = ::fast_io::details::itoa_precise_digit_count<base>(
		::fast_io::details::itoa_unsigned_magnitude(value));
	return layout;
}

/// @brief Returns the exact total output length for a non-full integer format.
template <::std::size_t base, bool showbase = false, bool showpos = false, bool oct_c2y = false,
		  ::fast_io::details::my_integral T>
	requires(2u <= base && base <= 36u && !::std::same_as<::std::remove_cvref_t<T>, bool>)
inline constexpr ::std::size_t itoa_precise_length(T value) noexcept
{
	return ::fast_io::details::itoa_predict_precise_layout<base, showbase, false, showpos, oct_c2y>(value)
		.total_size();
}

static_assert(::fast_io::details::itoa_jeaiii_decimal_digits_u32(0u) == 1u);
static_assert(::fast_io::details::itoa_jeaiii_decimal_digits_u32(4294967295u) == 10u);
static_assert(::fast_io::details::itoa_jeaiii_decimal_digits_u64(10000000000000000000ull) == 20u);
static_assert(::fast_io::details::itoa_precise_length<10>(-9223372036854775807ll - 1ll) == 20u);

} // namespace fast_io::details

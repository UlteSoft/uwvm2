#pragma once

/*
 * General floating-presentation adapter (FMT level).
 *
 * The format-language `g`/`G` decision, alternate-form behavior, and
 * fixed-versus-scientific wrapper are expressed here as printable objects.
 * Numeric conversion remains delegated to existing fast_io floating protocols,
 * and final width/scatter/buffer/device strategy remains an IO-level decision.
 */

#include "semantic.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace fast_io::manipulators
{

/// @brief Selects the C/Python/printf general floating presentation after precision rounding.
/// @details For significant precision `P`, the scientific child is selected exactly when the rounded decimal exponent
///          is below `-4` or at least `P`; otherwise the fixed child is emitted. `alternate_form` records `#` behavior,
///          so both children must carry the matching trailing-zero policy. The object owns both typed alternatives to
///          preserve their ordinary reserve-print protocols and selects exactly one at materialization time.
template <typename fixed_type, typename scientific_type, bool alternate_form>
struct general_float_t
{
	using manip_tag = ::fast_io::manip_tag_t;

	[[no_unique_address]] fixed_type fixed;
	[[no_unique_address]] scientific_type scientific;
	::std::size_t precision;

	inline static constexpr bool preserves_trailing_zero{alternate_form};
};

} // namespace fast_io::manipulators

namespace fast_io::fmt::details
{

template <typename...>
inline constexpr bool general_float_dependent_false_v{};

/**
 * Obtains the transported floating value without imposing a second ABI choice.
 *
 * `make_brace_floating` has already decayed the source according to fast_io's
 * ABI policy and stores that carrier in `scalar_manip_precision_t::reference`.
 * Peeling semantic wrappers, instead of accepting the original user type again,
 * guarantees that the layout hint and both actual conversions observe the same
 * rounded input representation on SysV, MS, AAPCS and the less common ABIs.
 */
template <typename leaf_type>
[[nodiscard]] inline constexpr decltype(auto)
general_float_scalar_reference(leaf_type const &leaf) noexcept
{
	if constexpr (requires { leaf.scalar; })
	{
		return general_float_scalar_reference(leaf.scalar);
	}
	else if constexpr (requires { leaf.reference; })
	{
		return (leaf.reference);
	}
	else if constexpr (requires { leaf.representation; })
	{
		// The only field proxy is IEC 60559 bfloat16.  Widen its preserved
		// representation into the high half of binary32 by integer bit-cast;
		// this is exact and cannot quiet a signaling NaN.
		return ::fast_io::bit_cast<float>(
			static_cast<::std::uint_least32_t>(leaf.representation) << 16u);
	}
	else
	{
		static_assert(general_float_dependent_false_v<leaf_type>,
			"fast_io format: a general-float child must be a fast_io scalar leaf");
	}
}

template <typename leaf_type>
inline constexpr bool general_float_has_field_representation = []() constexpr {
	if constexpr (requires(leaf_type const &leaf) { leaf.scalar; })
	{
		return general_float_has_field_representation<
			::std::remove_cvref_t<decltype(::std::declval<leaf_type const &>().scalar)>>;
	}
	else
	{
		return requires(leaf_type const &leaf) { leaf.representation; };
	}
}();

template <typename leaf_type>
	requires general_float_has_field_representation<leaf_type>
[[nodiscard]] inline constexpr ::std::uint_least16_t
general_float_field_representation(leaf_type const &leaf) noexcept
{
	if constexpr (requires { leaf.scalar; })
	{
		return general_float_field_representation(leaf.scalar);
	}
	else
	{
		return leaf.representation;
	}
}

template <::std::integral char_type, typename leaf_type>
[[nodiscard]] inline constexpr ::std::size_t
general_float_leaf_reserve_size(leaf_type const &leaf)
{
	if constexpr (::fast_io::dynamic_reserve_printable<char_type, leaf_type>)
	{
		return print_reserve_size(
			::fast_io::io_reserve_type<char_type, leaf_type>, leaf);
	}
	else
	{
		static_assert(::fast_io::reserve_printable<char_type, leaf_type>);
		return print_reserve_size(
			::fast_io::io_reserve_type<char_type, leaf_type>);
	}
}

template <::std::integral char_type>
[[nodiscard]] inline constexpr int general_float_digit_value(
	char_type character) noexcept
{
	// Enumerating the ten code units is intentional.  An arithmetic range test
	// would silently assume ASCII and fail for the supported EBCDIC executions.
	if (character == ::fast_io::char_literal_v<u8'0', char_type>) return 0;
	if (character == ::fast_io::char_literal_v<u8'1', char_type>) return 1;
	if (character == ::fast_io::char_literal_v<u8'2', char_type>) return 2;
	if (character == ::fast_io::char_literal_v<u8'3', char_type>) return 3;
	if (character == ::fast_io::char_literal_v<u8'4', char_type>) return 4;
	if (character == ::fast_io::char_literal_v<u8'5', char_type>) return 5;
	if (character == ::fast_io::char_literal_v<u8'6', char_type>) return 6;
	if (character == ::fast_io::char_literal_v<u8'7', char_type>) return 7;
	if (character == ::fast_io::char_literal_v<u8'8', char_type>) return 8;
	if (character == ::fast_io::char_literal_v<u8'9', char_type>) return 9;
	return -1;
}

/** Ensures that `#` retains a radix point when P has no fractional digit. */
template <::std::integral char_type>
[[nodiscard]] inline constexpr char_type *general_float_apply_alternate(
	char_type *first, char_type *last) noexcept
{
	auto insertion{last};
	bool numeric{};
	for (auto iter{first}; iter != last; ++iter)
	{
		if (*iter == ::fast_io::char_literal_v<u8'.', char_type>) return last;
		if (*iter == ::fast_io::char_literal_v<u8'e', char_type> ||
			*iter == ::fast_io::char_literal_v<u8'E', char_type>)
			insertion = iter;
		if (general_float_digit_value(*iter) >= 0) numeric = true;
	}
	if (!numeric) return last;
	for (auto iter{last}; iter != insertion; --iter) *iter = *(iter - 1);
	*insertion = ::fast_io::char_literal_v<u8'.', char_type>;
	return last + 1;
}

struct general_float_exponent_result
{
	::std::int_least64_t exponent{};
	bool numeric{};
};

/**
 * Recovers the exponent from an already rounded child spelling.
 *
 * This textual proof is independent of the binary-to-decimal algorithm.  A
 * scientific child exposes its exponent directly.  For a fixed child, the
 * first nonzero digit and the radix position define the same exponent.  Zero
 * is canonicalized to exponent zero.  NaN and infinity have no decimal digit
 * and are reported as nonnumeric; their fixed/scientific child spellings are
 * identical, so no second conversion is needed.
 */
template <::std::integral char_type>
[[nodiscard]] inline constexpr general_float_exponent_result
general_float_rounded_exponent(char_type const *first,
	char_type const *last) noexcept
{
	auto exponent_marker{last};
	for (auto iter{first}; iter != last; ++iter)
	{
		if (*iter == ::fast_io::char_literal_v<u8'e', char_type> ||
			*iter == ::fast_io::char_literal_v<u8'E', char_type>)
		{
			exponent_marker = iter;
			break;
		}
	}

	if (exponent_marker != last)
	{
		auto iter{exponent_marker + 1};
		bool negative{};
		if (iter != last &&
			(*iter == ::fast_io::char_literal_v<u8'+', char_type> ||
			 *iter == ::fast_io::char_literal_v<u8'-', char_type>))
		{
			negative = *iter == ::fast_io::char_literal_v<u8'-', char_type>;
			++iter;
		}
		::std::int_least64_t exponent{};
		bool has_digit{};
		for (; iter != last; ++iter)
		{
			auto const digit{general_float_digit_value(*iter)};
			if (digit < 0) break;
			has_digit = true;
			// Supported floating representations have decimal exponents below
			// 5000.  Saturation nevertheless makes this parser total for a custom
			// scalar leaf without admitting signed overflow.
			constexpr auto saturation{INT_LEAST64_MAX / 10};
			if (exponent > saturation)
				exponent = INT_LEAST64_MAX;
			else
				exponent = exponent * 10 + digit;
		}
		if (!has_digit) return {};
		return {negative ? -exponent : exponent, true};
	}

	auto radix{last};
	for (auto iter{first}; iter != last; ++iter)
	{
		if (*iter == ::fast_io::char_literal_v<u8'.', char_type>)
		{
			radix = iter;
			break;
		}
	}
	auto const integral_end{radix == last ? last : radix};
	::std::size_t integral_digits{};
	for (auto iter{first}; iter != integral_end; ++iter)
	{
		if (general_float_digit_value(*iter) >= 0) ++integral_digits;
	}
	::std::size_t digit_index{};
	for (auto iter{first}; iter != integral_end; ++iter)
	{
		auto const digit{general_float_digit_value(*iter)};
		if (digit < 0) continue;
		if (digit != 0)
		{
			return {static_cast<::std::int_least64_t>(
				integral_digits - digit_index - 1u), true};
		}
		++digit_index;
	}
	if (radix != last)
	{
		::std::size_t fractional_index{};
		for (auto iter{radix + 1}; iter != last; ++iter)
		{
			auto const digit{general_float_digit_value(*iter)};
			if (digit < 0) continue;
			++fractional_index;
			if (digit != 0)
			{
				return {-static_cast<::std::int_least64_t>(fractional_index), true};
			}
		}
	}
	// At least one decimal zero distinguishes finite zero from inf/nan.
	return {0, integral_digits != 0u};
}

[[nodiscard]] inline constexpr bool general_float_uses_scientific(
	general_float_exponent_result result, ::std::size_t precision) noexcept
{
	if (!result.numeric) return false;
	if (result.exponent < -4) return true;
	return result.exponent >= 0 &&
		static_cast<::std::uint_least64_t>(result.exponent) >= precision;
}

template <typename floating_type>
[[nodiscard]] inline constexpr floating_type general_float_pow10(
	::std::size_t exponent) noexcept
{
	floating_type result{1};
	floating_type factor{10};
	while (exponent != 0u)
	{
		if ((exponent & 1u) != 0u) result *= factor;
		exponent >>= 1u;
		if (exponent != 0u) factor *= factor;
	}
	return result;
}

struct general_float_selection_hint
{
	bool scientific{};
	bool requires_validation{};
};

/**
 * Chooses the likely one-pass child without participating in correctness.
 *
 * Values in [1e-4, 10^P) normally use fixed notation.  Only rounding near
 * either endpoint can change that answer; the emitted-spelling proof below
 * detects those carries and overwrites with the other child.  The hint uses
 * exponentiation by squaring, so an attacker-controlled dynamic precision
 * costs O(log P), and overflow merely produces infinity and selects fixed as
 * the tentative child.  No libm logarithm, locale, or format parsing is used.
 *
 * Most values are also far enough from either notation boundary that decimal
 * rounding cannot change the initial answer.  The widest possible directed
 * significant-digit adjustment is less than one unit in the last retained
 * place.  Values below half of 1e-4 therefore cannot round up to 1e-4, and
 * fixed candidates below half of 10^P cannot round up to 10^P.  Only the two
 * remaining half-open boundary neighborhoods need the emitted-text proof.
 */
template <typename fixed_type>
[[nodiscard]] inline constexpr general_float_selection_hint
general_float_initial_selection(
	fixed_type const &fixed, ::std::size_t precision) noexcept
{
	if constexpr (general_float_has_field_representation<fixed_type>)
	{
		auto const representation{general_float_field_representation(fixed)};
		// Classify NaN and infinity before forming a binary32 value.  In
		// particular, never compare a signaling-NaN carrier in floating
		// arithmetic merely to choose a formatting child.
		if (((representation >> 7u) & 0xffu) == 0xffu)
		{
			return {};
		}
		auto const transported{::fast_io::bit_cast<float>(
			static_cast<::std::uint_least32_t>(representation) << 16u)};
		auto const magnitude{transported < 0.0f ? -transported : transported};
		if (magnitude == 0.0f) return {};
		constexpr float lower_boundary{1.0f / 10000.0f};
		if (magnitude < lower_boundary)
		{
			return {true, !(magnitude < lower_boundary / 2.0f)};
		}
		auto const upper{general_float_pow10<float>(precision)};
		if (magnitude >= upper) return {true, false};
		return {false, !(magnitude < upper / 2.0f)};
	}
	else
	{
	decltype(auto) transported{general_float_scalar_reference(fixed)};
	using floating_type = ::std::remove_cvref_t<decltype(transported)>;
	static_assert(::fast_io::details::my_floating_point<floating_type>,
		"fast_io format: general-float children must transport a floating value");
	using trait = ::fast_io::details::iec559_traits<floating_type>;
	auto const fields{::fast_io::details::get_punned_result(transported)};
	constexpr auto exponent_mask{static_cast<decltype(fields.exponent)>(
		(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
	// Classify both NaN and infinity from the representation before the first
	// floating comparison.  A signaling NaN must be printable without raising
	// FE_INVALID merely because the frontend needs a tentative g/G child.
	if (fields.exponent == exponent_mask) return {};
	auto const magnitude{transported < floating_type{} ? -transported : transported};
	if (magnitude == floating_type{}) return {};
	constexpr auto lower_boundary{
		floating_type{1} / floating_type{10000}};
	if (magnitude < lower_boundary)
	{
		return {true, !(magnitude < lower_boundary / floating_type{2})};
	}
	auto const upper{general_float_pow10<floating_type>(precision)};
	if (magnitude >= upper) return {true, false};
	return {false, !(magnitude < upper / floating_type{2})};
	}
}

template <typename fixed_type>
[[nodiscard]] inline constexpr bool general_float_initial_scientific(
	fixed_type const &fixed, ::std::size_t precision) noexcept
{
	return general_float_initial_selection(fixed, precision).scientific;
}

template <bool alternate_form, typename fixed_type, typename scientific_type>
[[nodiscard]] inline constexpr auto make_general_float(
	fixed_type &&fixed, scientific_type &&scientific,
	::std::size_t precision) noexcept
{
	using fixed_storage = ::std::remove_cvref_t<fixed_type>;
	using scientific_storage = ::std::remove_cvref_t<scientific_type>;
	return ::fast_io::manipulators::general_float_t<
		fixed_storage, scientific_storage, alternate_form>{
		::std::forward<fixed_type>(fixed),
		::std::forward<scientific_type>(scientific),
		precision == 0u ? 1u : precision};
}

template <typename fixed_type, typename scientific_type, bool alternate_form>
[[nodiscard]] inline constexpr auto make_general_float(
	fixed_type &&fixed, scientific_type &&scientific,
	::std::size_t precision,
	::std::bool_constant<alternate_form>) noexcept
{
	return make_general_float<alternate_form>(
		::std::forward<fixed_type>(fixed),
		::std::forward<scientific_type>(scientific), precision);
}

} // namespace fast_io::fmt::details

namespace fast_io
{

template <::std::integral char_type, typename fixed_type,
	  typename scientific_type, bool alternate_form>
	requires((::fast_io::reserve_printable<char_type, fixed_type> ||
			  ::fast_io::dynamic_reserve_printable<char_type, fixed_type>) &&
			 (::fast_io::reserve_printable<char_type, scientific_type> ||
			  ::fast_io::dynamic_reserve_printable<char_type, scientific_type>))
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::general_float_t<
			fixed_type, scientific_type, alternate_form>>,
	::fast_io::manipulators::general_float_t<
		fixed_type, scientific_type, alternate_form> const &value)
	noexcept(noexcept(::fast_io::fmt::details::general_float_leaf_reserve_size<
		char_type>(value.fixed)) &&
		noexcept(::fast_io::fmt::details::general_float_leaf_reserve_size<
			char_type>(value.scientific)))
{
	auto const fixed_size{
		::fast_io::fmt::details::general_float_leaf_reserve_size<char_type>(
			value.fixed)};
	auto const scientific_size{
		::fast_io::fmt::details::general_float_leaf_reserve_size<char_type>(
			value.scientific)};
	auto const child_size{fixed_size < scientific_size ? scientific_size : fixed_size};
	return child_size + static_cast<::std::size_t>(alternate_form);
}

template <::std::integral char_type, typename fixed_type,
	  typename scientific_type, bool alternate_form>
	requires((::fast_io::reserve_printable<char_type, fixed_type> ||
			  ::fast_io::dynamic_reserve_printable<char_type, fixed_type>) &&
			 (::fast_io::reserve_printable<char_type, scientific_type> ||
			  ::fast_io::dynamic_reserve_printable<char_type, scientific_type>))
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::general_float_t<
			fixed_type, scientific_type, alternate_form>>,
	char_type *iter,
	::fast_io::manipulators::general_float_t<
		fixed_type, scientific_type, alternate_form> const &value)
	noexcept(noexcept(print_reserve_define(
		::fast_io::io_reserve_type<char_type, fixed_type>, iter, value.fixed)) &&
		noexcept(print_reserve_define(
			::fast_io::io_reserve_type<char_type, scientific_type>,
			iter, value.scientific)))
{
	auto const initial_selection{
		::fast_io::fmt::details::general_float_initial_selection(
			value.fixed, value.precision)};
	if (initial_selection.scientific)
	{
		auto end{print_reserve_define(
			::fast_io::io_reserve_type<char_type, scientific_type>,
			iter, value.scientific)};
		if constexpr (alternate_form)
			end = ::fast_io::fmt::details::general_float_apply_alternate(iter, end);
		if (!initial_selection.requires_validation) return end;
		auto const exponent{
			::fast_io::fmt::details::general_float_rounded_exponent(iter, end)};
		if (::fast_io::fmt::details::general_float_uses_scientific(
			exponent, value.precision))
			return end;
		auto selected_end{print_reserve_define(
			::fast_io::io_reserve_type<char_type, fixed_type>, iter, value.fixed)};
		if constexpr (alternate_form)
			selected_end = ::fast_io::fmt::details::general_float_apply_alternate(
				iter, selected_end);
		return selected_end;
	}
	else
	{
		auto end{print_reserve_define(
			::fast_io::io_reserve_type<char_type, fixed_type>, iter, value.fixed)};
		if constexpr (alternate_form)
			end = ::fast_io::fmt::details::general_float_apply_alternate(iter, end);
		if (!initial_selection.requires_validation) return end;
		auto const exponent{
			::fast_io::fmt::details::general_float_rounded_exponent(iter, end)};
		if (!::fast_io::fmt::details::general_float_uses_scientific(
			exponent, value.precision))
			return end;
		auto selected_end{print_reserve_define(
			::fast_io::io_reserve_type<char_type, scientific_type>,
			iter, value.scientific)};
		if constexpr (alternate_form)
			selected_end = ::fast_io::fmt::details::general_float_apply_alternate(
				iter, selected_end);
		return selected_end;
	}
}

/**
 * Internal zero padding needs only the sign shift.  Fixed and scientific
 * children are constructed from the same flags/value, so forwarding either
 * proof is sufficient and avoids making the selection leaf formatting-aware.
 */
template <::std::integral char_type, typename fixed_type,
	  typename scientific_type, bool alternate_form>
	requires requires(fixed_type const &fixed) {
		{
			print_define_internal_shift(
				::fast_io::io_reserve_type<char_type, fixed_type>, fixed)
		} -> ::std::same_as<::std::size_t>;
	}
[[nodiscard]] inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::general_float_t<
			fixed_type, scientific_type, alternate_form>>,
	::fast_io::manipulators::general_float_t<
		fixed_type, scientific_type, alternate_form> const &value)
	noexcept(noexcept(print_define_internal_shift(
		::fast_io::io_reserve_type<char_type, fixed_type>, value.fixed)))
{
	return print_define_internal_shift(
		::fast_io::io_reserve_type<char_type, fixed_type>, value.fixed);
}

} // namespace fast_io

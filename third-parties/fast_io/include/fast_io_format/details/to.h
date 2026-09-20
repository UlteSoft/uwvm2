#pragma once

/*
 * Format-to-conversion continuation (FMT-to-IO boundary).
 *
 * A format program is an output description.  Once lowered, its components
 * are ordinary fast_io printable objects and can therefore be fed directly to
 * the existing `to`/`inplace_to` conversion machinery.  This file deliberately
 * contains no scanner or formatter implementation of its own; it only bridges
 * the format lowering callback to the IO-level conversion front doors.
 */

#include "lower.h"

#include <concepts>
#include <type_traits>
#include <utility>

namespace fast_io::fmt::details
{

/** Converts one lowered format component pack into a newly constructed value. */
template <typename result_type, ::std::integral char_type>
struct to_lowered_components
{
	template <typename... component_types>
	[[nodiscard]] inline constexpr result_type operator()(
		component_types &&...components) const
	{
		return ::fast_io::basic_to<char_type, result_type>(
			::std::forward<component_types>(components)...);
	}
};

/** Applies one grammar rule and sends its lowered components to `basic_to`. */
template <typename result_type, ::std::integral char_type,
		  ::fast_io::fmt::basic_fixed_string format_literal,
		  ::fast_io::fmt::format_grammar grammar_type,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type to_with_rule(
	[[maybe_unused]] grammar_type grammar, argument_types &&...arguments)
{
	// Grammar objects are stateless type tags; lowering consumes their type so no run-time read or ABI-visible copy is
	// required after the by-value front-door parameter has been formed.
	using rule_type = ::std::remove_cvref_t<grammar_type>;
	return ::fast_io::fmt::details::lower_format_program<
		format_literal, rule_type>(
		to_lowered_components<result_type, char_type>{},
		arguments...);
}

/** Sends one lowered format component pack to an existing scan target. */
template <::std::integral char_type, typename target_type>
struct inplace_to_lowered_components
{
	target_type &&target;

	template <typename... component_types>
	inline constexpr void operator()(component_types &&...components) const
	{
		if constexpr (sizeof...(component_types) == 0u)
		{
			// An empty format emits no source characters, so the existing target
			// remains unchanged just as an empty print run is a no-op.
			return;
		}
		else
		{
		// A named target is intentionally passed as an lvalue, matching the
		// ordinary `inplace_to(target, ...)` facade and preserving its lifetime
		// throughout the complete lowering operation.
		::fast_io::basic_inplace_to<char_type>(
			target, ::std::forward<component_types>(components)...);
		}
	}
};

/** Applies one grammar rule and sends its lowered components to `basic_inplace_to`. */
template <::std::integral char_type,
		  ::fast_io::fmt::basic_fixed_string format_literal,
		  ::fast_io::fmt::format_grammar grammar_type,
		  typename target_type, typename... argument_types>
inline constexpr void inplace_to_with_rule(
	[[maybe_unused]] grammar_type grammar, target_type &&target,
	argument_types &&...arguments)
{
	// As above, the grammar's type selects the rule while the object intentionally carries no run-time state.
	using rule_type = ::std::remove_cvref_t<grammar_type>;
	::fast_io::fmt::details::lower_format_program<format_literal, rule_type>(
		inplace_to_lowered_components<char_type, target_type>{
			::std::forward<target_type>(target)},
		arguments...);
}

/** Built-in character-domain bridge for value-returning conversion. */
template <typename expected_char_type, typename result_type,
		  ::fast_io::fmt::basic_fixed_string format_literal,
		  ::fast_io::fmt::format_grammar grammar_type,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type to_builtin_with_rule(
	grammar_type grammar, argument_types &&...arguments)
{
	using literal_char_type = typename decltype(format_literal)::value_type;
	static_assert(::std::same_as<literal_char_type, expected_char_type>,
		"fast_io format: the format literal character type does not match the selected to function");
	return ::fast_io::fmt::details::to_with_rule<
		result_type, expected_char_type, format_literal>(
		grammar, ::std::forward<argument_types>(arguments)...);
}

/** Built-in character-domain bridge for an existing conversion target. */
template <typename expected_char_type,
		  ::fast_io::fmt::basic_fixed_string format_literal,
		  ::fast_io::fmt::format_grammar grammar_type,
		  typename target_type, typename... argument_types>
inline constexpr void inplace_to_builtin_with_rule(
	grammar_type grammar, target_type &&target,
	argument_types &&...arguments)
{
	using literal_char_type = typename decltype(format_literal)::value_type;
	static_assert(::std::same_as<literal_char_type, expected_char_type>,
		"fast_io format: the format literal character type does not match the selected inplace_to function");
	::fast_io::fmt::details::inplace_to_with_rule<
		expected_char_type, format_literal>(
		grammar, ::std::forward<target_type>(target),
		::std::forward<argument_types>(arguments)...);
}

} // namespace fast_io::fmt::details

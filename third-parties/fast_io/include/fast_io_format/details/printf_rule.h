#pragma once

/*
 * Built-in type-safe printf grammar provider (FMT level).
 *
 * This file connects percent-format parsing to the open compiler and
 * replacement-rule protocols, including exact validation of value, dynamic
 * width, and dynamic precision references. It translates accepted directives
 * to the same typed IO object vocabulary used by brace formatting; no varargs,
 * stream dispatch, allocation, or primitive writes occur here.
 */

#include "../types.h"
#include "printf_parser.h"
#include "builtin_diagnostics.h"
#include "compile.h"
#include "replacement_rules.h"
#include "lower.h"

namespace fast_io::fmt::details
{

enum class printf_argument_list_error : unsigned char
{
	none,
	index_out_of_range,
	unreferenced_argument
};

struct printf_argument_list_validation
{
	printf_argument_list_error error{printf_argument_list_error::none};
	::std::size_t argument_index{};
	::std::size_t source_position{};
};

/** Proves that printf's complete value/width/precision argument domain is exact. */
template <::fast_io::fmt::basic_fixed_string format_literal,
		  ::std::size_t argument_count>
[[nodiscard]] inline consteval printf_argument_list_validation
validate_printf_argument_list() noexcept
{
	constexpr auto const &program{
		::fast_io::fmt::details::checked_program<
			format_literal, ::fast_io::fmt::printf_fmt_t>};
	bool referenced[argument_count == 0u ? 1u : argument_count]{};
	printf_argument_list_validation result{};
	auto mark_reference = [&](argument_reference reference,
							  ::std::size_t source_position) consteval {
		if (reference.index >= argument_count)
		{
			result.error = printf_argument_list_error::index_out_of_range;
			result.argument_index = reference.index;
			result.source_position = source_position;
			return false;
		}
		referenced[reference.index] = true;
		return true;
	};

	for (::std::size_t field_index{};
		 field_index != program.field_count; ++field_index)
	{
		auto const &field{program.fields[field_index]};
		if (field.specification.width.kind == format_parameter_kind::argument &&
			!mark_reference(field.specification.width.argument,
							field.source.offset))
		{
			return result;
		}
		if (field.specification.precision.kind == format_parameter_kind::argument &&
			!mark_reference(field.specification.precision.argument,
							field.source.offset))
		{
			return result;
		}
		if (!mark_reference(field.argument, field.source.offset))
		{
			return result;
		}
	}

	for (::std::size_t argument_index{};
		 argument_index != argument_count; ++argument_index)
	{
		if (!referenced[argument_index])
		{
			result.error = printf_argument_list_error::unreferenced_argument;
			result.argument_index = argument_index;
			return result;
		}
	}
	return result;
}

} // namespace fast_io::fmt::details

namespace fast_io::fmt
{

/**
 * Registers percent conversions as an independent compile-time grammar rule.
 *
 * A percent sign is syntax only while this rule is selected.  Brace rules and
 * third-party rules therefore see it as ordinary data unless their own CPO
 * chooses otherwise, and the common emitter never tests for it.
 */
template <basic_fixed_string format_literal>
[[nodiscard]] consteval auto compile_format_program(printf_fmt_t) noexcept
{
	constexpr auto result{
		::fast_io::fmt::details::parse_printf_format<format_literal>()};
	if constexpr (result.error !=
				  ::fast_io::fmt::details::format_parse_error::none)
	{
		::fast_io::fmt::details::diagnose_parse_error<
			result.error, result.error_position>();
	}
	return result.program;
}

/** Public proof that the percent rule owns a compile-time program for a literal. */
template <basic_fixed_string format_literal>
concept percent_format_rule = format_rule_for<format_literal, printf_fmt_t>;

/** Enforces printf's exact argument domain before syntax-neutral lowering. */
template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr bool validate_format_argument_list(
	printf_fmt_t) noexcept
{
	constexpr auto validation{
		::fast_io::fmt::details::validate_printf_argument_list<
			format_literal, sizeof...(argument_types)>()};
	// Indexed replacement lowering already owns the missing-argument
	// diagnostic. This rule-level contract supplies the complementary complete
	// domain proof. Its exact bool result remains a valid expression when false,
	// allowing the generic consumer to reject it explicitly on every frontend.
	return validation.error ==
		   ::fast_io::fmt::details::printf_argument_list_error::none;
}

/** Registers percent argument selection and value-rule lowering through ADL. */
template <auto format_literal, auto field, typename argument_pack>
[[nodiscard]] inline constexpr decltype(auto) lower_format_replacement_define(
	printf_fmt_t,
	::fast_io::fmt::details::compile_time_value<format_literal>,
	::fast_io::fmt::details::compiled_replacement_t<field>,
	argument_pack &arguments)
{
	return ::fast_io::fmt::details::make_rule_replacement<
		printf_fmt_t, format_literal, field>(arguments);
}

} // namespace fast_io::fmt

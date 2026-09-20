#pragma once

/*
 * Built-in brace grammar provider (FMT level).
 *
 * This file binds the brace parser to the open grammar-compilation CPO,
 * validates ordinary and nested argument references against the typed call-site
 * pack, and exposes each replacement to the common rule protocol. It owns
 * brace-language meaning, not formatting strategy: accepted fields are
 * translated into IO-level printable or semantic objects.
 */

#include "../types.h"
#include "brace_parser.h"
#include "builtin_diagnostics.h"
#include "compile.h"
#include "replacement_rules.h"
#include "lower.h"

namespace fast_io::fmt::details
{

enum class brace_argument_list_error : unsigned char
{
	none,
	reference_resolution,
	unreferenced_argument
};

struct brace_argument_list_validation
{
	brace_argument_list_error error{brace_argument_list_error::none};
	argument_resolution_error resolution_error{
		argument_resolution_error::none};
	::std::size_t argument_index{};
	::std::size_t source_position{};
};

/** Marks one resolved brace reference without routing an immediate call through a local lambda.
 *
 * GCC 13 incorrectly diagnoses a call to an immediate function made from a
 * consteval generic/local lambda as taking that function's address. Keeping
 * the parameterized operation in an ordinary constexpr function lets the
 * outer immediate validator supply its values and retains whole-program
 * validation on every supported C++20 frontend.
 */
template <::fast_io::fmt::basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] inline constexpr bool mark_brace_argument_reference(
	argument_reference reference, ::std::size_t source_position,
	bool *referenced, brace_argument_list_validation &result) noexcept
{
	auto const resolution{
		resolve_argument_reference_value<format_literal,
									 argument_types...>(reference)};
	if (resolution.error != argument_resolution_error::none)
	{
		result.error = brace_argument_list_error::reference_resolution;
		result.resolution_error = resolution.error;
		result.argument_index = resolution.index;
		result.source_position = source_position;
		return false;
	}
	referenced[resolution.index] = true;
	return true;
}

/** Resolves every ordinary and nested brace reference into one exact argument domain. */
template <::fast_io::fmt::basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] inline constexpr brace_argument_list_validation
validate_brace_argument_list() noexcept
{
	constexpr auto const &program{
		::fast_io::fmt::details::checked_program<
			format_literal, ::fast_io::fmt::brace_fmt_t>};
	constexpr ::std::size_t argument_count{sizeof...(argument_types)};
	bool referenced[argument_count == 0u ? 1u : argument_count]{};
	brace_argument_list_validation result{};

	for (::std::size_t field_index{};
		 field_index != program.field_count; ++field_index)
	{
		auto const &field{program.fields[field_index]};
		if (!mark_brace_argument_reference<format_literal, argument_types...>(
				field.argument, field.source.offset, referenced, result))
		{
			return result;
		}
		if (field.specification.width.kind == format_parameter_kind::argument &&
			!mark_brace_argument_reference<format_literal, argument_types...>(
				field.specification.width.argument, field.source.offset,
				referenced, result))
		{
			return result;
		}
		if (field.specification.precision.kind == format_parameter_kind::argument &&
			!mark_brace_argument_reference<format_literal, argument_types...>(
				field.specification.precision.argument, field.source.offset,
				referenced, result))
		{
			return result;
		}

		auto const tail{field.specification.format_tail};
		if (tail.size != 0u)
		{
			using char_type = typename decltype(format_literal)::value_type;
			using scratch_program = basic_format_program<char_type, 0u>;
			format_parse_result<scratch_program> parse_result{};
			auto indexing_state{
				field.specification.format_tail_indexing_state};
			auto next_automatic_index{
				field.specification.format_tail_next_automatic_index};
			::std::size_t cursor{tail.offset};
			::std::size_t const tail_end{tail.offset + tail.size};
			while (cursor != tail_end)
			{
				if (is_syntax_character<u8'{'>(format_literal[cursor]))
				{
					::std::size_t const opening_position{cursor};
					format_parameter nested_parameter{};
					if (!parse_brace_dynamic_parameter<format_literal>(
							parse_result, cursor, nested_parameter,
							indexing_state, next_automatic_index))
					{
						result.error =
							brace_argument_list_error::reference_resolution;
						result.resolution_error =
							argument_resolution_error::name_not_found;
						result.source_position = opening_position;
						return result;
					}
					if (!mark_brace_argument_reference<
							format_literal, argument_types...>(
							nested_parameter.argument, opening_position,
							referenced, result))
					{
						return result;
					}
				}
				else
				{
					++cursor;
				}
			}
		}
	}

	for (::std::size_t argument_index{};
		 argument_index != argument_count; ++argument_index)
	{
		if (!referenced[argument_index])
		{
			result.error = brace_argument_list_error::unreferenced_argument;
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
 * Registers the brace syntax as one compile-time grammar rule.
 *
 * This overload, rather than the generic print/concat implementation, owns the
 * meaning of left and right braces.  The rule returns the syntax-neutral flat
 * program consumed through the grammar CPO protocol; no parser state survives
 * the immediate invocation.
 */
template <basic_fixed_string format_literal>
[[nodiscard]] consteval auto compile_format_program(brace_fmt_t) noexcept
{
	constexpr auto result{
		::fast_io::fmt::details::parse_brace_format<format_literal>()};
	if constexpr (result.error !=
				  ::fast_io::fmt::details::format_parse_error::none)
	{
		::fast_io::fmt::details::diagnose_parse_error<
			result.error, result.error_position>();
	}
	return result.program;
}

/** Public proof that the brace rule owns a compile-time program for a literal. */
template <basic_fixed_string format_literal>
concept brace_format_rule = format_rule_for<format_literal, brace_fmt_t>;

/** Enforces the exact brace argument domain before syntax-neutral lowering. */
template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr bool validate_format_argument_list(
	brace_fmt_t) noexcept
{
	constexpr auto validation{
		::fast_io::fmt::details::validate_brace_argument_list<
			format_literal, argument_types...>()};
	// Per-reference lowering retains its established diagnostics for missing,
	// ambiguous, and out-of-range references. Only this whole-program pass can
	// observe a supplied argument which no field selected. Returning an exact
	// bool keeps the CPO well-formed even when the proof is false; the generic
	// consumer must then assert the value instead of treating a failed immediate
	// diagnostic as if this optional protocol did not exist on early Clang.
	return validation.error ==
		   ::fast_io::fmt::details::brace_argument_list_error::none;
}

/** Registers brace argument selection and value-rule lowering through ADL. */
template <auto format_literal, auto field, typename argument_pack>
[[nodiscard]] inline constexpr decltype(auto) lower_format_replacement_define(
	brace_fmt_t,
	::fast_io::fmt::details::compile_time_value<format_literal>,
	::fast_io::fmt::details::compiled_replacement_t<field>,
	argument_pack &arguments)
{
	return ::fast_io::fmt::details::make_rule_replacement<
		brace_fmt_t, format_literal, field>(arguments);
}

} // namespace fast_io::fmt

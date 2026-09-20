#pragma once

/*
 * Compile-time diagnostics for built-in format grammars (FMT level).
 *
 * Structural parser error codes and source offsets are mapped here to focused
 * template diagnostics. This is a reporting leaf of syntax compilation; it
 * has no run-time representation and is unrelated to IO status protocols such
 * as `status_print_define` or scan parse results.
 */

#include "program.h"

#include <cstddef>

namespace fast_io::fmt::details
{

/** Emits precise diagnostics for the two built-in syntax-rule error domain. */
template <format_parse_error error, ::std::size_t source_position>
inline consteval void diagnose_parse_error()
{
	if constexpr (error == format_parse_error::capacity_exceeded)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: internal flat-program capacity proof failed");
	}
	else if constexpr (error == format_parse_error::unmatched_left_brace)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: unmatched '{' in brace format string");
	}
	else if constexpr (error == format_parse_error::unmatched_right_brace)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: unmatched '}' in brace format string");
	}
	else if constexpr (error == format_parse_error::invalid_argument_identifier)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: invalid brace argument identifier");
	}
	else if constexpr (error == format_parse_error::argument_index_overflow)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: argument index is too large");
	}
	else if constexpr (error == format_parse_error::mixed_automatic_and_manual_indexing)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: automatic and manual brace indexing cannot be mixed");
	}
	else if constexpr (error == format_parse_error::expected_closing_brace)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: expected a closing brace");
	}
	else if constexpr (error == format_parse_error::invalid_fill_character)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: invalid fill code point");
	}
	else if constexpr (error == format_parse_error::invalid_format_specification)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: invalid brace format specification");
	}
	else if constexpr (error == format_parse_error::width_overflow)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: field width is too large");
	}
	else if constexpr (error == format_parse_error::precision_overflow)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: field precision is too large");
	}
	else if constexpr (error == format_parse_error::invalid_dynamic_parameter)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: invalid dynamic width or precision argument");
	}
	else if constexpr (error == format_parse_error::invalid_presentation_type)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: invalid brace presentation type");
	}
	else if constexpr (error == format_parse_error::dangling_percent)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: dangling '%' in printf format string");
	}
	else if constexpr (error == format_parse_error::invalid_printf_position)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: invalid one-based printf argument position");
	}
	else if constexpr (error == format_parse_error::mixed_printf_positional_and_sequential_indexing)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: positional and sequential printf arguments cannot be mixed");
	}
	else if constexpr (error == format_parse_error::invalid_printf_length)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: invalid printf length modifier");
	}
	else if constexpr (error == format_parse_error::invalid_printf_conversion)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: invalid printf conversion");
	}
	else if constexpr (error == format_parse_error::forbidden_printf_n)
	{
		static_assert(error == format_parse_error::none,
					  "fast_io format: %n is intentionally forbidden because formatting has no output-count side effect");
	}
	else
	{
		static_assert(error == format_parse_error::none);
	}

	// `source_position` is a template argument so compiler instantiation traces
	// identify the exact code-unit offset without storing source state at runtime.
	(void)source_position;
}

} // namespace fast_io::fmt::details

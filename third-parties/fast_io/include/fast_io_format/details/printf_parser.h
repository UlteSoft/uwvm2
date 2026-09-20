#pragma once

/*
 * Parser for the type-safe printf-format source language (FMT level).
 *
 * This constant-evaluation stage resolves percent directives, flags, length
 * syntax, and sequential or positional references into the shared structural
 * program. It does not implement C varargs and does not format values. The
 * printf rule validates the typed argument domain and common lowering converts
 * accepted fields into ordinary fast_io IO objects.
 */

#include "program.h"

#include <cstddef>
#include <type_traits>

namespace fast_io::fmt::details
{

enum class printf_indexing_state : unsigned char
{
	undetermined,
	sequential,
	positional
};

template <::fast_io::fmt::basic_fixed_string format_string, typename result_type>
consteval bool printf_select_sequential_argument(result_type &result,
	argument_reference &reference, printf_indexing_state &indexing_state,
	::std::size_t &next_sequential_index, ::std::size_t position) noexcept
{
	if (indexing_state == printf_indexing_state::positional)
	{
		return set_parse_error(result,
			format_parse_error::mixed_printf_positional_and_sequential_indexing, position);
	}
	indexing_state = printf_indexing_state::sequential;
	reference.kind = argument_reference_kind::automatic;
	reference.index = next_sequential_index;
	if (next_sequential_index == (::std::numeric_limits<::std::size_t>::max)())
	{
		return set_parse_error(result, format_parse_error::argument_index_overflow, position);
	}
	++next_sequential_index;
	return true;
}

template <::fast_io::fmt::basic_fixed_string format_string, typename result_type>
consteval bool printf_select_positional_argument(result_type &result,
	argument_reference &reference, printf_indexing_state &indexing_state,
	::std::size_t one_based_index, ::std::size_t position) noexcept
{
	if (one_based_index == 0u)
	{
		return set_parse_error(result, format_parse_error::invalid_printf_position, position);
	}
	if (indexing_state == printf_indexing_state::sequential)
	{
		return set_parse_error(result,
			format_parse_error::mixed_printf_positional_and_sequential_indexing, position);
	}
	indexing_state = printf_indexing_state::positional;
	reference.kind = argument_reference_kind::index;
	reference.index = one_based_index - 1u;
	return true;
}

struct printf_decimal_scan
{
	::std::size_t value{};
	::std::size_t end{};
	bool overflow{};
};

template <::fast_io::fmt::basic_fixed_string format_string>
[[nodiscard]] constexpr printf_decimal_scan scan_printf_decimal(::std::size_t cursor) noexcept
{
	// This helper is evaluated only by the enclosing consteval parser.  Keeping
	// the leaf constexpr avoids the pre-DR20 Clang 13--15 rule that rejects a
	// mutable parser cursor as an argument to a nested immediate invocation;
	// the outer parser still proves that no runtime parsing path can exist.
	constexpr ::std::size_t size{format_string.size()};
	printf_decimal_scan result{};
	result.end = cursor;
	while (result.end != size)
	{
		unsigned const digit{syntax_digit_value(format_string[result.end])};
		if (digit == 10u)
		{
			break;
		}
		if (!result.overflow && !checked_decimal_accumulate(result.value, digit))
		{
			result.overflow = true;
		}
		++result.end;
	}
	return result;
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] consteval bool printf_presentation(char_type value, presentation_type &result) noexcept
{
	if (is_syntax_character<u8'c'>(value)) result = presentation_type::character;
	else if (is_syntax_character<u8'd'>(value) || is_syntax_character<u8'i'>(value))
		result = presentation_type::decimal;
	else if (is_syntax_character<u8'u'>(value)) result = presentation_type::unsigned_decimal;
	else if (is_syntax_character<u8'o'>(value)) result = presentation_type::octal;
	else if (is_syntax_character<u8'x'>(value)) result = presentation_type::hex_lower;
	else if (is_syntax_character<u8'X'>(value)) result = presentation_type::hex_upper;
	else if (is_syntax_character<u8'a'>(value)) result = presentation_type::hexfloat_lower;
	else if (is_syntax_character<u8'A'>(value)) result = presentation_type::hexfloat_upper;
	else if (is_syntax_character<u8'e'>(value)) result = presentation_type::scientific_lower;
	else if (is_syntax_character<u8'E'>(value)) result = presentation_type::scientific_upper;
	else if (is_syntax_character<u8'f'>(value)) result = presentation_type::fixed_lower;
	else if (is_syntax_character<u8'F'>(value)) result = presentation_type::fixed_upper;
	else if (is_syntax_character<u8'g'>(value)) result = presentation_type::general_lower;
	else if (is_syntax_character<u8'G'>(value)) result = presentation_type::general_upper;
	else if (is_syntax_character<u8's'>(value)) result = presentation_type::string;
	else if (is_syntax_character<u8'p'>(value)) result = presentation_type::pointer;
	else return false;
	return true;
}

template <::fast_io::fmt::basic_fixed_string format_string, typename result_type>
consteval bool parse_printf_star_parameter(result_type &result, ::std::size_t &cursor,
	format_parameter &parameter, printf_indexing_state &indexing_state,
	::std::size_t &next_sequential_index) noexcept
{
	constexpr ::std::size_t size{format_string.size()};
	::std::size_t const star_position{cursor++};
	parameter.kind = format_parameter_kind::argument;
	if (cursor != size && syntax_digit_value(format_string[cursor]) != 10u)
	{
		auto const position{scan_printf_decimal<format_string>(cursor)};
		if (position.end == size || !is_syntax_character<u8'$'>(format_string[position.end]) ||
			position.overflow || position.value == 0u)
		{
			return set_parse_error(result, format_parse_error::invalid_printf_position, cursor);
		}
		cursor = position.end + 1u;
		return printf_select_positional_argument<format_string>(result, parameter.argument,
			indexing_state, position.value, star_position);
	}
	return printf_select_sequential_argument<format_string>(result, parameter.argument,
		indexing_state, next_sequential_index, star_position);
}

/**
 * Compile a printf format string into the same flat program used by the brace
 * frontend.  A sequential `*` width and precision consume arguments before the
 * converted value, exactly as printf does.  Positional N$ references are
 * resolved to zero-based indices during this pass; mixing them with sequential
 * references is rejected before lowering.
 */
template <::fast_io::fmt::basic_fixed_string format_string>
[[nodiscard]] consteval auto parse_printf_format() noexcept
{
	using fixed_string_type = ::std::remove_cv_t<decltype(format_string)>;
	using char_type = typename fixed_string_type::value_type;
	constexpr ::std::size_t size{format_string.size()};
	// Every printf replacement consumes at least a percent sign and conversion.
	using program_type = basic_format_program<char_type, size, 2u>;
	format_parse_result<program_type> result{};
	printf_indexing_state indexing_state{printf_indexing_state::undetermined};
	::std::size_t next_sequential_index{};
	::std::size_t cursor{};
	while (cursor != size)
	{
		if (!is_syntax_character<u8'%'>(format_string[cursor]))
		{
			if (!result.program.append_literal(format_string[cursor]))
			{
				set_parse_error(result, format_parse_error::capacity_exceeded, cursor);
				return result;
			}
			++cursor;
			continue;
		}

		::std::size_t const opening_position{cursor++};
		if (cursor == size)
		{
			set_parse_error(result, format_parse_error::dangling_percent, opening_position);
			return result;
		}
		if (is_syntax_character<u8'%'>(format_string[cursor]))
		{
			// The source code unit is already in the format literal's execution
			// representation, including GCC's wide-EBCDIC storage endian.
			if (!result.program.append_literal(format_string[cursor]))
			{
				set_parse_error(result, format_parse_error::capacity_exceeded, opening_position);
				return result;
			}
			++cursor;
			continue;
		}

		replacement_field<char_type> field{};
		field.source.offset = opening_position;
		bool value_is_positional{};
		::std::size_t value_position{};
		::std::size_t const possible_position_begin{cursor};
		if (syntax_digit_value(format_string[cursor]) != 10u)
		{
			auto const possible_position{scan_printf_decimal<format_string>(cursor)};
			if (possible_position.end != size &&
				is_syntax_character<u8'$'>(format_string[possible_position.end]))
			{
				if (possible_position.overflow || possible_position.value == 0u)
				{
					set_parse_error(result, format_parse_error::invalid_printf_position,
						possible_position_begin);
					return result;
				}
				value_is_positional = true;
				value_position = possible_position.value;
				cursor = possible_position.end + 1u;
			}
		}

		// Flags are accepted in any order and may repeat, as required by printf.
		for (;;)
		{
			if (cursor != size && is_syntax_character<u8'-'>(format_string[cursor]))
			{
				field.specification.alignment = format_alignment::left;
				++cursor;
			}
			else if (cursor != size && is_syntax_character<u8'+'>(format_string[cursor]))
			{
				field.specification.sign = format_sign::plus;
				++cursor;
			}
			else if (cursor != size && is_syntax_character<u8' '>(format_string[cursor]))
			{
				if (field.specification.sign != format_sign::plus)
				{
					field.specification.sign = format_sign::space;
				}
				++cursor;
			}
			else if (cursor != size && is_syntax_character<u8'#'>(format_string[cursor]))
			{
				field.specification.alternate_form = true;
				++cursor;
			}
			else if (cursor != size && is_syntax_character<u8'0'>(format_string[cursor]))
			{
				field.specification.zero_padding = true;
				++cursor;
			}
			else if (cursor != size && is_syntax_character<u8'\''>(format_string[cursor]))
			{
				field.specification.locale_specific = true;
				++cursor;
			}
			else
			{
				break;
			}
		}

		if (cursor != size && is_syntax_character<u8'*'>(format_string[cursor]))
		{
			if (!parse_printf_star_parameter<format_string>(result, cursor, field.specification.width,
				indexing_state, next_sequential_index))
			{
				return result;
			}
		}
		else if (cursor != size && syntax_digit_value(format_string[cursor]) != 10u)
		{
			auto const width{scan_printf_decimal<format_string>(cursor)};
			if (width.overflow ||
				static_cast<::std::size_t>((::std::numeric_limits<int>::max)()) < width.value)
			{
				set_parse_error(result, format_parse_error::width_overflow, cursor);
				return result;
			}
			field.specification.width.kind = format_parameter_kind::literal;
			field.specification.width.value = width.value;
			cursor = width.end;
		}

		if (cursor != size && is_syntax_character<u8'.'>(format_string[cursor]))
		{
			++cursor;
			if (cursor != size && is_syntax_character<u8'*'>(format_string[cursor]))
			{
				if (!parse_printf_star_parameter<format_string>(result, cursor,
					field.specification.precision, indexing_state, next_sequential_index))
				{
					return result;
				}
			}
			else if (cursor != size && syntax_digit_value(format_string[cursor]) != 10u)
			{
				auto const precision{scan_printf_decimal<format_string>(cursor)};
				if (precision.overflow ||
					static_cast<::std::size_t>((::std::numeric_limits<int>::max)()) < precision.value)
				{
					set_parse_error(result, format_parse_error::precision_overflow, cursor);
					return result;
				}
				field.specification.precision.kind = format_parameter_kind::literal;
				field.specification.precision.value = precision.value;
				cursor = precision.end;
			}
			else
			{
				// In printf, a bare dot is precision zero rather than a syntax error.
				field.specification.precision.kind = format_parameter_kind::literal;
				field.specification.precision.value = 0u;
			}
		}

		if (cursor != size && is_syntax_character<u8'h'>(format_string[cursor]))
		{
			++cursor;
			if (cursor != size && is_syntax_character<u8'h'>(format_string[cursor]))
			{
				field.printf_length = printf_length_modifier::hh;
				++cursor;
			}
			else field.printf_length = printf_length_modifier::h;
		}
		else if (cursor != size && is_syntax_character<u8'l'>(format_string[cursor]))
		{
			++cursor;
			if (cursor != size && is_syntax_character<u8'l'>(format_string[cursor]))
			{
				field.printf_length = printf_length_modifier::ll;
				++cursor;
			}
			else field.printf_length = printf_length_modifier::l;
		}
		else if (cursor != size && is_syntax_character<u8'j'>(format_string[cursor]))
		{
			field.printf_length = printf_length_modifier::j;
			++cursor;
		}
		else if (cursor != size && is_syntax_character<u8'z'>(format_string[cursor]))
		{
			field.printf_length = printf_length_modifier::z;
			++cursor;
		}
		else if (cursor != size && is_syntax_character<u8't'>(format_string[cursor]))
		{
			field.printf_length = printf_length_modifier::t;
			++cursor;
		}
		else if (cursor != size && is_syntax_character<u8'L'>(format_string[cursor]))
		{
			field.printf_length = printf_length_modifier::long_double;
			++cursor;
		}

		if (cursor == size)
		{
			set_parse_error(result,
				field.printf_length == printf_length_modifier::none
					? format_parse_error::invalid_printf_conversion
					: format_parse_error::invalid_printf_length,
				cursor);
			return result;
		}
		if (is_syntax_character<u8'n'>(format_string[cursor]))
		{
			set_parse_error(result, format_parse_error::forbidden_printf_n, cursor);
			return result;
		}
		if (!printf_presentation(format_string[cursor], field.specification.presentation))
		{
			set_parse_error(result, format_parse_error::invalid_printf_conversion, cursor);
			return result;
		}
		++cursor;

		if (value_is_positional)
		{
			if (!printf_select_positional_argument<format_string>(result, field.argument,
				indexing_state, value_position, possible_position_begin))
			{
				return result;
			}
		}
		else if (!printf_select_sequential_argument<format_string>(result, field.argument,
			indexing_state, next_sequential_index, opening_position))
		{
			return result;
		}

		field.source.size = cursor - opening_position;
		if (!result.program.append_replacement(field))
		{
			set_parse_error(result, format_parse_error::capacity_exceeded, opening_position);
			return result;
		}
	}
	return result;
}

} // namespace fast_io::fmt::details

#pragma once

/*
 * Parser for the brace-format source language (FMT level).
 *
 * Constant evaluation decodes literal runs, replacement fields, fill
 * characters, alignment, width, precision, and nested references into the
 * grammar-neutral program model. This stage validates source syntax only.
 * Argument-domain validation and typed replacement lowering are performed by
 * the brace rule and common lowering protocol, respectively.
 */

#include "program.h"

#include <climits>
#include <cstddef>
#include <limits>
#include <type_traits>

namespace fast_io::fmt::details
{

// `parse_brace_format` is the single immediate boundary. Its parameterized helpers are constexpr so early C++20
// frontends do not reject nested immediate calls whose values become known only when that boundary is invoked.
template <::fast_io::fmt::format_character char_type>
[[nodiscard]] constexpr bool is_brace_alignment(char_type value) noexcept
{
	return is_syntax_character<u8'<'>(value) || is_syntax_character<u8'>'>(value) ||
		   is_syntax_character<u8'^'>(value);
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] constexpr format_alignment brace_alignment(char_type value) noexcept
{
	if (is_syntax_character<u8'<'>(value))
	{
		return format_alignment::left;
	}
	if (is_syntax_character<u8'>'>(value))
	{
		return format_alignment::right;
	}
	return format_alignment::center;
}

struct decoded_fill_scalar
{
	::std::size_t code_units{};
	bool valid{};
};

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] constexpr ::std::uint_least32_t unicode_code_unit_value(char_type value) noexcept
{
	using unsigned_type = ::std::make_unsigned_t<char_type>;
	auto unsigned_value{static_cast<unsigned_type>(value)};
	if constexpr (::std::same_as<char_type, wchar_t> &&
				  ::fast_io::details::wide_is_none_utf_endian)
	{
		unsigned_value = ::fast_io::byte_swap(unsigned_value);
	}
	return static_cast<::std::uint_least32_t>(unsigned_value);
}

template <::fast_io::fmt::basic_fixed_string format_string>
[[nodiscard]] constexpr decoded_fill_scalar decode_fill_scalar(::std::size_t cursor) noexcept
{
	using fixed_string_type = ::std::remove_cv_t<decltype(format_string)>;
	using char_type = typename fixed_string_type::value_type;
	constexpr ::std::size_t size{format_string.size()};
	if (cursor == size)
	{
		return {};
	}

	if constexpr (!::fast_io::details::is_unicode_execution_charset<char_type>)
	{
		// The execution character is already one char_type object.  Applying UTF
		// code-unit tests to an implementation-defined SBCS format string would
		// reject valid fills and contradict the execution character-set model.
		return {1u, true};
	}
	else if constexpr (::std::same_as<char_type, char> || ::std::same_as<char_type, char8_t>)
	{
		auto const first{unicode_code_unit_value(format_string[cursor])};
		auto continuation = [](::std::uint_least32_t value) constexpr noexcept {
			return 0x80u <= value && value <= 0xbfu;
		};
		if (first <= 0x7fu)
		{
			return {1u, true};
		}
		if (0xc2u <= first && first <= 0xdfu)
		{
			return {2u, cursor + 1u < size &&
							continuation(unicode_code_unit_value(format_string[cursor + 1u]))};
		}
		if (0xe0u <= first && first <= 0xefu)
		{
			if (cursor + 2u >= size)
			{
				return {3u, false};
			}
			auto const second{unicode_code_unit_value(format_string[cursor + 1u])};
			auto const third{unicode_code_unit_value(format_string[cursor + 2u])};
			bool const restricted_second{
				(first != 0xe0u || 0xa0u <= second) &&
				(first != 0xedu || second <= 0x9fu)};
			return {3u, restricted_second && continuation(second) && continuation(third)};
		}
		if (0xf0u <= first && first <= 0xf4u)
		{
			if (cursor + 3u >= size)
			{
				return {4u, false};
			}
			auto const second{unicode_code_unit_value(format_string[cursor + 1u])};
			auto const third{unicode_code_unit_value(format_string[cursor + 2u])};
			auto const fourth{unicode_code_unit_value(format_string[cursor + 3u])};
			bool const restricted_second{
				(first != 0xf0u || 0x90u <= second) &&
				(first != 0xf4u || second <= 0x8fu)};
			return {4u, restricted_second && continuation(second) && continuation(third) &&
							continuation(fourth)};
		}
		return {1u, false};
	}
	else if constexpr (::std::same_as<char_type, char16_t> ||
					   (::std::same_as<char_type, wchar_t> && sizeof(wchar_t) == sizeof(char16_t)))
	{
		auto const first{unicode_code_unit_value(format_string[cursor])};
		if (0xd800u <= first && first <= 0xdbffu)
		{
			if (cursor + 1u == size)
			{
				return {2u, false};
			}
			auto const second{unicode_code_unit_value(format_string[cursor + 1u])};
			return {2u, 0xdc00u <= second && second <= 0xdfffu};
		}
		return {1u, !(0xdc00u <= first && first <= 0xdfffu)};
	}
	else
	{
		auto const scalar{unicode_code_unit_value(format_string[cursor])};
		return {1u, scalar <= 0x10ffffu && !(0xd800u <= scalar && scalar <= 0xdfffu)};
	}
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] constexpr bool brace_presentation(char_type value, presentation_type &result) noexcept
{
	if (is_syntax_character<u8'a'>(value))
	{
		result = presentation_type::hexfloat_lower;
	}
	else if (is_syntax_character<u8'A'>(value))
	{
		result = presentation_type::hexfloat_upper;
	}
	else if (is_syntax_character<u8'b'>(value))
	{
		result = presentation_type::binary_lower;
	}
	else if (is_syntax_character<u8'B'>(value))
	{
		result = presentation_type::binary_upper;
	}
	else if (is_syntax_character<u8'c'>(value))
	{
		result = presentation_type::character;
	}
	else if (is_syntax_character<u8'd'>(value))
	{
		result = presentation_type::decimal;
	}
	else if (is_syntax_character<u8'e'>(value))
	{
		result = presentation_type::scientific_lower;
	}
	else if (is_syntax_character<u8'E'>(value))
	{
		result = presentation_type::scientific_upper;
	}
	else if (is_syntax_character<u8'f'>(value))
	{
		result = presentation_type::fixed_lower;
	}
	else if (is_syntax_character<u8'F'>(value))
	{
		result = presentation_type::fixed_upper;
	}
	else if (is_syntax_character<u8'g'>(value))
	{
		result = presentation_type::general_lower;
	}
	else if (is_syntax_character<u8'G'>(value))
	{
		result = presentation_type::general_upper;
	}
	else if (is_syntax_character<u8'o'>(value))
	{
		result = presentation_type::octal;
	}
	else if (is_syntax_character<u8'p'>(value))
	{
		result = presentation_type::pointer;
	}
	else if (is_syntax_character<u8's'>(value))
	{
		result = presentation_type::string;
	}
	else if (is_syntax_character<u8'x'>(value))
	{
		result = presentation_type::hex_lower;
	}
	else if (is_syntax_character<u8'X'>(value))
	{
		result = presentation_type::hex_upper;
	}
	else if (is_syntax_character<u8'?'>(value))
	{
		result = presentation_type::debug;
	}
	else
	{
		return false;
	}
	return true;
}

template <::fast_io::fmt::basic_fixed_string format_string, typename result_type>
constexpr bool brace_select_automatic_argument(result_type &result,
											   argument_reference &reference, brace_indexing_state &indexing_state,
											   ::std::size_t &next_automatic_index, ::std::size_t position) noexcept
{
	if (indexing_state == brace_indexing_state::manual)
	{
		return set_parse_error(result, format_parse_error::mixed_automatic_and_manual_indexing, position);
	}
	indexing_state = brace_indexing_state::automatic;
	reference.kind = argument_reference_kind::automatic;
	reference.index = next_automatic_index;
	if (next_automatic_index == (::std::numeric_limits<::std::size_t>::max)())
	{
		return set_parse_error(result, format_parse_error::argument_index_overflow, position);
	}
	++next_automatic_index;
	return true;
}

template <::fast_io::fmt::basic_fixed_string format_string, typename result_type>
constexpr bool brace_select_manual_argument(result_type &result,
											argument_reference &reference, brace_indexing_state &indexing_state,
											::std::size_t index, ::std::size_t position) noexcept
{
	if (indexing_state == brace_indexing_state::automatic)
	{
		return set_parse_error(result, format_parse_error::mixed_automatic_and_manual_indexing, position);
	}
	indexing_state = brace_indexing_state::manual;
	reference.kind = argument_reference_kind::index;
	reference.index = index;
	return true;
}

/**
 * Parse an argument reference without consuming its delimiter.  A normal
 * replacement field permits ':' or '}', while a nested width/precision field
 * permits only '}'.  Named references remain slices into `format_string`;
 * neither hashing nor character-set-dependent identifier normalization occurs.
 */
template <::fast_io::fmt::basic_fixed_string format_string, typename result_type>
constexpr bool parse_brace_argument_reference(result_type &result,
											  ::std::size_t &cursor, bool allow_colon, argument_reference &reference,
											  brace_indexing_state &indexing_state, ::std::size_t &next_automatic_index) noexcept
{
	constexpr ::std::size_t size{format_string.size()};
	auto is_delimiter = [allow_colon](auto value) constexpr noexcept {
		return is_syntax_character<u8'}'>(value) ||
			   (allow_colon && is_syntax_character<u8':'>(value));
	};

	if (cursor == size)
	{
		return set_parse_error(result, format_parse_error::expected_closing_brace, cursor);
	}
	::std::size_t const reference_position{cursor};
	if (is_delimiter(format_string[cursor]))
	{
		return brace_select_automatic_argument<format_string>(result, reference, indexing_state,
															  next_automatic_index, reference_position);
	}

	if (syntax_digit_value(format_string[cursor]) != 10u)
	{
		if (is_syntax_character<u8'0'>(format_string[cursor]) &&
			cursor + 1u != size &&
			syntax_digit_value(format_string[cursor + 1u]) != 10u)
		{
			// fmt's manual argument grammar admits the single identifier `0`,
			// but not a decimal spelling with a leading zero such as `{00}`.
			return set_parse_error(result,
								   format_parse_error::invalid_argument_identifier, cursor + 1u);
		}
		::std::size_t index{};
		do
		{
			unsigned const digit{syntax_digit_value(format_string[cursor])};
			if (!checked_decimal_accumulate(index, digit))
			{
				return set_parse_error(result, format_parse_error::argument_index_overflow, cursor);
			}
			++cursor;
		} while (cursor != size && syntax_digit_value(format_string[cursor]) != 10u);
		if (static_cast<::std::size_t>(INT_MAX) < index)
		{
			return set_parse_error(result,
								   format_parse_error::argument_index_overflow, reference_position);
		}

		if (cursor == size || !is_delimiter(format_string[cursor]))
		{
			return set_parse_error(result, format_parse_error::invalid_argument_identifier, cursor);
		}
		return brace_select_manual_argument<format_string>(result, reference, indexing_state, index,
														   reference_position);
	}

	if (!is_identifier_initial(format_string[cursor]))
	{
		return set_parse_error(result, format_parse_error::invalid_argument_identifier, cursor);
	}
	::std::size_t const name_begin{cursor++};
	while (cursor != size && is_identifier_continuation(format_string[cursor]))
	{
		++cursor;
	}
	if (cursor == size || !is_delimiter(format_string[cursor]))
	{
		return set_parse_error(result, format_parse_error::invalid_argument_identifier, cursor);
	}
	reference.kind = argument_reference_kind::name;
	reference.name = {name_begin, cursor - name_begin};
	return true;
}

template <::fast_io::fmt::basic_fixed_string format_string, typename result_type>
constexpr bool parse_brace_dynamic_parameter(result_type &result,
											 ::std::size_t &cursor, format_parameter &parameter,
											 brace_indexing_state &indexing_state, ::std::size_t &next_automatic_index) noexcept
{
	constexpr ::std::size_t size{format_string.size()};
	::std::size_t const opening_position{cursor};
	++cursor; // consume '{'
	parameter.kind = format_parameter_kind::argument;
	if (!parse_brace_argument_reference<format_string>(result, cursor, false, parameter.argument,
													   indexing_state, next_automatic_index))
	{
		return false;
	}
	if (cursor == size || !is_syntax_character<u8'}'>(format_string[cursor]))
	{
		return set_parse_error(result, format_parse_error::invalid_dynamic_parameter, opening_position);
	}
	++cursor;
	return true;
}

template <::fast_io::fmt::basic_fixed_string format_string, typename result_type>
constexpr bool parse_brace_unsigned(result_type &result, ::std::size_t &cursor,
									::std::size_t &value, format_parse_error overflow_error) noexcept
{
	constexpr ::std::size_t size{format_string.size()};
	value = 0u;
	do
	{
		unsigned const digit{syntax_digit_value(format_string[cursor])};
		if (!checked_decimal_accumulate(value, digit))
		{
			return set_parse_error(result, overflow_error, cursor);
		}
		++cursor;
	} while (cursor != size && syntax_digit_value(format_string[cursor]) != 10u);
	return true;
}

template <::fast_io::fmt::basic_fixed_string format_string, typename result_type>
constexpr bool parse_brace_specification(result_type &result, ::std::size_t &cursor,
										 format_specification<typename decltype(format_string)::value_type> &specification,
										 brace_indexing_state &indexing_state, ::std::size_t &next_automatic_index) noexcept
{
	constexpr ::std::size_t size{format_string.size()};
	auto const specification_begin{cursor};
	specification.raw_format_specification.offset = specification_begin;
	specification.raw_format_indexing_state = indexing_state;
	specification.raw_format_next_automatic_index = next_automatic_index;
	if (cursor == size)
	{
		return set_parse_error(result, format_parse_error::expected_closing_brace, cursor);
	}

	// A fill is one Unicode scalar, not one code unit.  Decode at most four code
	// units, then require an alignment token immediately after that scalar.  The
	// bounded lookahead keeps the total consteval work O(format-string length).
	auto const decoded_fill{decode_fill_scalar<format_string>(cursor)};
	bool const has_fill_alignment{
		decoded_fill.valid && cursor + decoded_fill.code_units < size &&
		is_brace_alignment(format_string[cursor + decoded_fill.code_units])};
	if (!decoded_fill.valid)
	{
		// If an alignment token occurs where the end of this malformed scalar
		// could have been, diagnose the prefix as a bad fill rather than reporting
		// the less useful "bad presentation type" at its first code unit.
		auto const lookahead_limit{
			decoded_fill.code_units < 4u ? decoded_fill.code_units : 4u};
		for (::std::size_t i{1u}; i <= lookahead_limit && cursor + i < size; ++i)
		{
			if (is_brace_alignment(format_string[cursor + i]))
			{
				return set_parse_error(result, format_parse_error::invalid_fill_character, cursor);
			}
		}
	}
	if (has_fill_alignment)
	{
		if (is_syntax_character<u8'{'>(format_string[cursor]) ||
			is_syntax_character<u8'}'>(format_string[cursor]))
		{
			return set_parse_error(result, format_parse_error::invalid_fill_character, cursor);
		}
		for (::std::size_t i{}; i != decoded_fill.code_units; ++i)
		{
			specification.fill[i] = format_string[cursor + i];
		}
		specification.fill_size = decoded_fill.code_units;
		specification.has_fill = true;
		specification.alignment = brace_alignment(format_string[cursor + decoded_fill.code_units]);
		cursor += decoded_fill.code_units + 1u;
	}
	else if (cursor != size && is_brace_alignment(format_string[cursor]))
	{
		specification.alignment = brace_alignment(format_string[cursor]);
		++cursor;
	}

	if (cursor != size && is_syntax_character<u8'+'>(format_string[cursor]))
	{
		specification.sign = format_sign::plus;
		++cursor;
	}
	else if (cursor != size && is_syntax_character<u8'-'>(format_string[cursor]))
	{
		specification.sign = format_sign::minus;
		++cursor;
	}
	else if (cursor != size && is_syntax_character<u8' '>(format_string[cursor]))
	{
		specification.sign = format_sign::space;
		++cursor;
	}

	if (cursor != size && is_syntax_character<u8'#'>(format_string[cursor]))
	{
		specification.alternate_form = true;
		++cursor;
	}
	if (cursor != size && is_syntax_character<u8'0'>(format_string[cursor]))
	{
		specification.zero_padding = true;
		++cursor;
		if (cursor != size && is_syntax_character<u8'0'>(format_string[cursor]))
		{
			return set_parse_error(result,
								   format_parse_error::invalid_format_specification, cursor);
		}
	}

	if (cursor != size && syntax_digit_value(format_string[cursor]) != 10u)
	{
		::std::size_t const width_position{cursor};
		specification.width.kind = format_parameter_kind::literal;
		if (!parse_brace_unsigned<format_string>(result, cursor, specification.width.value,
												 format_parse_error::width_overflow))
		{
			return false;
		}
		if (static_cast<::std::size_t>((::std::numeric_limits<int>::max)()) <
			specification.width.value)
		{
			return set_parse_error(result, format_parse_error::width_overflow, width_position);
		}
	}
	else if (cursor != size && is_syntax_character<u8'{'>(format_string[cursor]))
	{
		if (!parse_brace_dynamic_parameter<format_string>(result, cursor, specification.width,
														  indexing_state, next_automatic_index))
		{
			return false;
		}
	}

	if (cursor != size && is_syntax_character<u8'.'>(format_string[cursor]))
	{
		::std::size_t const dot_position{cursor++};
		if (cursor != size && syntax_digit_value(format_string[cursor]) != 10u)
		{
			::std::size_t const precision_position{cursor};
			specification.precision.kind = format_parameter_kind::literal;
			if (!parse_brace_unsigned<format_string>(result, cursor, specification.precision.value,
													 format_parse_error::precision_overflow))
			{
				return false;
			}
			if (static_cast<::std::size_t>((::std::numeric_limits<int>::max)()) <
				specification.precision.value)
			{
				return set_parse_error(result, format_parse_error::precision_overflow,
									   precision_position);
			}
		}
		else if (cursor != size && is_syntax_character<u8'{'>(format_string[cursor]))
		{
			if (!parse_brace_dynamic_parameter<format_string>(result, cursor, specification.precision,
															  indexing_state, next_automatic_index))
			{
				return false;
			}
		}
		else
		{
			return set_parse_error(result, format_parse_error::invalid_format_specification, dot_position);
		}
	}

	if (cursor != size && is_syntax_character<u8'L'>(format_string[cursor]))
	{
		specification.locale_specific = true;
		++cursor;
	}

	if (cursor == size)
	{
		return set_parse_error(result, format_parse_error::expected_closing_brace, cursor);
	}
	auto const type_directed_begin{cursor};
	if (!is_syntax_character<u8'}'>(format_string[cursor]))
	{
		if (is_syntax_character<u8'?'>(format_string[cursor]) && cursor + 1u < size &&
			is_syntax_character<u8's'>(format_string[cursor + 1u]))
		{
			specification.presentation = presentation_type::debug_string;
			cursor += 2u;
		}
		else
		{
			if (brace_presentation(format_string[cursor], specification.presentation))
			{
				++cursor;
			}
		}
		if (cursor != size && !is_syntax_character<u8'}'>(format_string[cursor]))
		{
			// The common grammar has ended. Preserve the remaining characters for a
			// type-directed consteval parser (ranges, chrono, or an ADL formatter).
			// Nested fields are validated here because they belong to the one global
			// indexing domain even though their width/precision meaning is decided by
			// the type-directed parser. This is flat compile-time IR data, never a
			// runtime parser fallback.
			auto const tail_begin{cursor};
			specification.format_tail_indexing_state = indexing_state;
			specification.format_tail_next_automatic_index = next_automatic_index;
			while (cursor != size && !is_syntax_character<u8'}'>(format_string[cursor]))
			{
				if (is_syntax_character<u8'{'>(format_string[cursor]))
				{
					format_parameter nested_parameter{};
					if (!parse_brace_dynamic_parameter<format_string>(result, cursor,
																	  nested_parameter, indexing_state, next_automatic_index))
					{
						return false;
					}
				}
				else
				{
					++cursor;
				}
			}
			specification.format_tail = {tail_begin, cursor - tail_begin};
		}
	}
	if (cursor == size || !is_syntax_character<u8'}'>(format_string[cursor]))
	{
		return set_parse_error(result, format_parse_error::invalid_format_specification, cursor);
	}
	specification.raw_format_specification.size = cursor - specification_begin;
	specification.type_directed_specification = {
		type_directed_begin, cursor - type_directed_begin};
	return true;
}

/**
 * Compile a brace format string into a flat program in one imperative
 * consteval pass.  Escapes are decoded as characters are appended, so adjacent
 * ordinary and escaped literals coalesce into one operation.  No runtime parse
 * entry point exists: malformed syntax is represented only in the consteval
 * result and must be diagnosed by the lowering layer.
 */
template <::fast_io::fmt::basic_fixed_string format_string>
[[nodiscard]] consteval auto parse_brace_format() noexcept
{
	using fixed_string_type = ::std::remove_cv_t<decltype(format_string)>;
	using char_type = typename fixed_string_type::value_type;
	constexpr ::std::size_t size{format_string.size()};
	// Every brace replacement consumes at least "{}".  Supplying the proven
	// width keeps the shared, one-code-unit-capable IR compact for this grammar.
	using program_type = basic_format_program<char_type, size, 2u>;
	format_parse_result<program_type> result{};
	brace_indexing_state indexing_state{brace_indexing_state::undetermined};
	::std::size_t next_automatic_index{};
	::std::size_t cursor{};
	while (cursor != size)
	{
		if (is_syntax_character<u8'{'>(format_string[cursor]))
		{
			::std::size_t const opening_position{cursor};
			if (cursor + 1u < size && is_syntax_character<u8'{'>(format_string[cursor + 1u]))
			{
				// Reuse the source code unit.  Besides avoiding a redundant character-set
				// conversion, this preserves GCC's special wide-EBCDIC storage endian.
				if (!result.program.append_literal(format_string[cursor]))
				{
					set_parse_error(result, format_parse_error::capacity_exceeded, cursor);
					return result;
				}
				cursor += 2u;
				continue;
			}

			++cursor;
			replacement_field<char_type> field{};
			field.source.offset = opening_position;
			if (!parse_brace_argument_reference<format_string>(result, cursor, true, field.argument,
															   indexing_state, next_automatic_index))
			{
				return result;
			}
			if (cursor != size && is_syntax_character<u8':'>(format_string[cursor]))
			{
				++cursor;
				if (!parse_brace_specification<format_string>(result, cursor, field.specification,
															  indexing_state, next_automatic_index))
				{
					return result;
				}
			}
			if (cursor == size || !is_syntax_character<u8'}'>(format_string[cursor]))
			{
				set_parse_error(result, format_parse_error::unmatched_left_brace, opening_position);
				return result;
			}
			++cursor;
			field.source.size = cursor - opening_position;
			if (!result.program.append_replacement(field))
			{
				set_parse_error(result, format_parse_error::capacity_exceeded, opening_position);
				return result;
			}
			continue;
		}

		if (is_syntax_character<u8'}'>(format_string[cursor]))
		{
			if (cursor + 1u == size || !is_syntax_character<u8'}'>(format_string[cursor + 1u]))
			{
				set_parse_error(result, format_parse_error::unmatched_right_brace, cursor);
				return result;
			}
			if (!result.program.append_literal(format_string[cursor]))
			{
				set_parse_error(result, format_parse_error::capacity_exceeded, cursor);
				return result;
			}
			cursor += 2u;
			continue;
		}

		if (!result.program.append_literal(format_string[cursor]))
		{
			set_parse_error(result, format_parse_error::capacity_exceeded, cursor);
			return result;
		}
		++cursor;
	}
	return result;
}

} // namespace fast_io::fmt::details

#pragma once

/*
 * Grammar-neutral compiled-program representation (FMT level).
 *
 * Parsers record literals, argument references, replacement specifications,
 * and source diagnostics in the structural types defined here. The program is
 * compile-time syntax IR: grammar providers produce it and `lower.h` consumes
 * it. It never owns run-time arguments, output state, or device operations.
 */

#include "fixed_string.h"
#include "../../fast_io_core.h"

#include <cstddef>
#include <limits>

namespace fast_io::fmt::details
{

enum class format_parse_error : unsigned char
{
	none,
	capacity_exceeded,
	unmatched_left_brace,
	unmatched_right_brace,
	invalid_argument_identifier,
	argument_index_overflow,
	mixed_automatic_and_manual_indexing,
	expected_closing_brace,
	invalid_fill_character,
	invalid_format_specification,
	width_overflow,
	precision_overflow,
	invalid_dynamic_parameter,
	invalid_presentation_type,
	dangling_percent,
	invalid_printf_position,
	mixed_printf_positional_and_sequential_indexing,
	invalid_printf_length,
	invalid_printf_conversion,
	forbidden_printf_n
};

struct source_slice
{
	::std::size_t offset{};
	::std::size_t size{};
};

/** Transports a structural constant through a CPO without runtime storage. */
template <auto constant_value>
struct compile_time_value
{
	static inline constexpr auto value{constant_value};
};

enum class argument_reference_kind : unsigned char
{
	automatic,
	index,
	name
};

/**
 * An automatic reference already carries its resolved zero-based index.  This
 * keeps lowering non-stateful and, more importantly, prevents it from
 * rebuilding an O(number-of-fields) indexing state in each template
 * instantiation.  A named reference points into the original NTTP string.
 * Keeping that stable source slice also leaves the C++26 lowering free to use
 * static reflection for member lookup without changing the parser or its IR.
 */
struct argument_reference
{
	argument_reference_kind kind{argument_reference_kind::automatic};
	::std::size_t index{};
	source_slice name{};
};

enum class format_parameter_kind : unsigned char
{
	none,
	literal,
	argument
};

struct format_parameter
{
	format_parameter_kind kind{format_parameter_kind::none};
	::std::size_t value{};
	argument_reference argument{};
};

enum class format_alignment : unsigned char
{
	none,
	left,
	right,
	center
};

enum class format_sign : unsigned char
{
	default_sign,
	plus,
	minus,
	space
};

enum class presentation_type : unsigned char
{
	none,
	binary_lower,
	binary_upper,
	character,
	decimal,
	unsigned_decimal,
	octal,
	hex_lower,
	hex_upper,
	hexfloat_lower,
	hexfloat_upper,
	scientific_lower,
	scientific_upper,
	fixed_lower,
	fixed_upper,
	general_lower,
	general_upper,
	string,
	pointer,
	debug,
	debug_string
};

enum class printf_length_modifier : unsigned char
{
	none,
	hh,
	h,
	l,
	ll,
	j,
	z,
	t,
	long_double
};

/** Tracks the one indexing domain shared by a brace replacement and its nested fields. */
enum class brace_indexing_state : unsigned char
{
	undetermined,
	automatic,
	manual
};

template <::fast_io::fmt::format_character char_type>
struct format_specification
{
	// One Unicode scalar may occupy four UTF-8 or two UTF-16 code units.  Keeping
	// the original code units is necessary: width counts a repeated fill pattern,
	// not a transcoded character chosen by the formatting frontend.
	char_type fill[4u]{};
	::std::size_t fill_size{};
	bool has_fill{};
	format_alignment alignment{format_alignment::none};
	format_sign sign{format_sign::default_sign};
	bool alternate_form{};
	bool zero_padding{};
	bool locale_specific{};
	format_parameter width{};
	format_parameter precision{};
	presentation_type presentation{presentation_type::none};
	// The complete text following the replacement field's delimiter colon. A
	// range/custom formatter owns a grammar that can be ambiguous with the common
	// scalar prefix (`{::>5}` is the canonical example), so type-directed
	// consteval compilation must be able to revisit the original NTTP slice.
	source_slice raw_format_specification{};
	brace_indexing_state raw_format_indexing_state{
		brace_indexing_state::undetermined};
	::std::size_t raw_format_next_automatic_index{};
	// Chrono and similar formatters share the generic fill/alignment/width/
	// precision prefix but own every code unit after it.  This slice begins
	// before the scalar presentation probe, so a leading chrono literal such as
	// `date %Y` cannot lose its first `d` merely because `d` is a scalar token.
	source_slice type_directed_specification{};
	// A suffix outside the common scalar/text grammar remains a slice of the
	// original NTTP. Range, chrono, and ADL formatters parse it in their own
	// consteval layer; no cursor or format text survives in runtime objects.
	source_slice format_tail{};
	// Nested replacement fields inside an opaque, type-directed suffix still
	// participate in the outer format's indexing domain. Saving the state at the
	// beginning of the suffix lets a range/custom consteval parser reproduce the
	// globally resolved references rather than accidentally restarting at zero.
	brace_indexing_state format_tail_indexing_state{
		brace_indexing_state::undetermined};
	::std::size_t format_tail_next_automatic_index{};
};

template <::fast_io::fmt::format_character char_type>
struct replacement_field
{
	argument_reference argument{};
	format_specification<char_type> specification{};
	printf_length_modifier printf_length{printf_length_modifier::none};
	source_slice source{};
};

enum class format_operation_kind : unsigned char
{
	literal,
	replacement
};

struct format_operation
{
	format_operation_kind kind{format_operation_kind::literal};
	::std::size_t payload_index{};
};

template <typename element_type, ::std::size_t capacity>
struct fixed_capacity_array
{
	element_type elements[capacity == 0u ? 1u : capacity]{};

	[[nodiscard]] inline constexpr element_type &operator[](::std::size_t index) noexcept
	{
		return elements[index];
	}

	[[nodiscard]] inline constexpr element_type const &operator[](::std::size_t index) const noexcept
	{
		return elements[index];
	}
};

/**
 * A flat, fixed-capacity formatting program.
 *
 * Capacity is bounded by the number of source code units: every replacement
 * operation consumes at least two units and every decoded literal consumes at
 * least one.  The parsers consequently need neither allocation nor a recursive
 * template AST.  They instantiate one parser regardless of field count and
 * execute one imperative consteval pass, avoiding the super-linear template
 * growth seen in recursively typed compile-format representations.
 *
 * Operations contain only a kind and an index into one of two dense payload
 * arrays.  Embedding `replacement_field` in every operation would make each
 * literal slot carry two argument references, two parameters, a fill pattern,
 * and all formatting flags.  For a large literal-heavy format that avoidable
 * padding is much larger than the source and is repeatedly materialized in
 * compiler constant-evaluation and template-argument storage.  The compact SoA
 * representation makes a literal operation two machine words while retaining
 * constant-time indexed lowering.
 *
 * Deliberately absent is a "dialect" discriminator.  The program is the common
 * result type of a syntax-rule CPO; once that CPO has decoded its spelling,
 * neither the emitter nor the optimizer needs to know whether a replacement
 * was introduced by braces, a percent conversion, or a third-party grammar.
 * Keeping that fact in the grammar type (instead of every IR object) both
 * proves syntax-neutral lowering and removes dead constant state from compiler
 * evaluation and NTTP materialization.
 */
template <::fast_io::fmt::format_character char_type, ::std::size_t capacity,
		  ::std::size_t minimum_replacement_code_units = 1u>
struct basic_format_program
{
	using value_type = char_type;
	static inline constexpr ::std::size_t maximum_capacity{capacity};
	static_assert(minimum_replacement_code_units != 0u,
				  "fast_io format: a grammar replacement must consume at least one code unit");
	// The generic IR cannot assume that a replacement is spelled "{}" or "%x";
	// a third-party rule may legitimately use one code unit.  Each syntax compiler
	// therefore states its proven minimum source width as a type argument.  The
	// built-in parsers use two and retain their compact arrays, while the default
	// of one is the conservative bound required by an open grammar protocol.
	static inline constexpr ::std::size_t maximum_replacement_count{
		capacity / minimum_replacement_code_units};
	// Two literal runs must be separated by a replacement.  If the grammar's
	// minimum replacement width is m, r runs consume at least r + (r-1)m code
	// units, hence r <= (capacity+m)/(m+1).  This algebraic capacity proof avoids
	// both heuristic rejection and over-allocation for the built-in grammars.
	static inline constexpr ::std::size_t maximum_literal_run_count{
		capacity == 0u ? 0u : (capacity + minimum_replacement_code_units) / (minimum_replacement_code_units + 1u)};
	static inline constexpr ::std::size_t maximum_operation_count{
		maximum_replacement_count + maximum_literal_run_count};

	fixed_capacity_array<char_type, capacity> literal_storage{};
	::std::size_t literal_size{};
	// Literal slices address literal_storage rather than the source format
	// string, so escaped braces and percent signs are already decoded.
	fixed_capacity_array<source_slice, maximum_literal_run_count> literal_runs{};
	::std::size_t literal_run_count{};
	fixed_capacity_array<replacement_field<char_type>, maximum_replacement_count> fields{};
	::std::size_t field_count{};
	fixed_capacity_array<format_operation, maximum_operation_count> operations{};
	::std::size_t operation_count{};

	inline constexpr bool append_literal(char_type value) noexcept
	{
		if (literal_size == capacity)
		{
			return false;
		}
		if (operation_count == 0u || operations[operation_count - 1u].kind != format_operation_kind::literal)
		{
			if (operation_count == maximum_operation_count ||
				literal_run_count == maximum_literal_run_count)
			{
				return false;
			}
			auto &operation{operations[operation_count++]};
			operation.kind = format_operation_kind::literal;
			operation.payload_index = literal_run_count;
			literal_runs[literal_run_count++] = {literal_size, 0u};
		}
		literal_storage[literal_size++] = value;
		++literal_runs[operations[operation_count - 1u].payload_index].size;
		return true;
	}

	inline constexpr bool append_replacement(replacement_field<char_type> const &field) noexcept
	{
		if (operation_count == maximum_operation_count || field_count == maximum_replacement_count)
		{
			return false;
		}
		auto &operation{operations[operation_count++]};
		operation.kind = format_operation_kind::replacement;
		operation.payload_index = field_count;
		fields[field_count++] = field;
		return true;
	}
};

template <typename program_type>
struct format_parse_result
{
	program_type program{};
	format_parse_error error{format_parse_error::none};
	::std::size_t error_position{};

	[[nodiscard]] inline constexpr explicit operator bool() const noexcept
	{
		return error == format_parse_error::none;
	}
};

template <typename result_type>
inline constexpr bool set_parse_error(result_type &result, format_parse_error error, ::std::size_t position) noexcept
{
	result.error = error;
	result.error_position = position;
	return false;
}

template <char8_t token, ::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr bool is_syntax_character(char_type value) noexcept
{
	// `arithmetic_char_literal_v` is the representation used by an actual string
	// code unit.  This differs from `char_literal_v` for the GCC wide-EBCDIC
	// "none-native-endian" mode: a wide literal contains 0x000000C0 for an IBM037
	// left brace while the ordinary promoted character literal is 0xC0000000.
	// Raw ASCII comparisons are wrong on every EBCDIC target, and the non-
	// arithmetic helper is additionally wrong for that wide execution ABI.
	return value == ::fast_io::arithmetic_char_literal_v<token, char_type>;
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr unsigned syntax_digit_value(char_type value) noexcept
{
	// Do not use `value - '0'`: although decimal digits happen to be contiguous
	// in the common EBCDIC code pages, the format grammar must not rely on that
	// encoding property.  The sentinel 10 denotes "not a grammar digit".
	if (is_syntax_character<u8'0'>(value))
	{
		return 0u;
	}
	if (is_syntax_character<u8'1'>(value))
	{
		return 1u;
	}
	if (is_syntax_character<u8'2'>(value))
	{
		return 2u;
	}
	if (is_syntax_character<u8'3'>(value))
	{
		return 3u;
	}
	if (is_syntax_character<u8'4'>(value))
	{
		return 4u;
	}
	if (is_syntax_character<u8'5'>(value))
	{
		return 5u;
	}
	if (is_syntax_character<u8'6'>(value))
	{
		return 6u;
	}
	if (is_syntax_character<u8'7'>(value))
	{
		return 7u;
	}
	if (is_syntax_character<u8'8'>(value))
	{
		return 8u;
	}
	if (is_syntax_character<u8'9'>(value))
	{
		return 9u;
	}
	return 10u;
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr bool is_identifier_initial(char_type value) noexcept
{
	// EBCDIC letters occupy several disjoint ranges.  Enumerating grammar
	// letters is deliberate: range tests such as A <= c && c <= Z are not
	// portable evidence of an ASCII identifier.
	return is_syntax_character<u8'_'>(value) ||
		   is_syntax_character<u8'a'>(value) || is_syntax_character<u8'b'>(value) ||
		   is_syntax_character<u8'c'>(value) || is_syntax_character<u8'd'>(value) ||
		   is_syntax_character<u8'e'>(value) || is_syntax_character<u8'f'>(value) ||
		   is_syntax_character<u8'g'>(value) || is_syntax_character<u8'h'>(value) ||
		   is_syntax_character<u8'i'>(value) || is_syntax_character<u8'j'>(value) ||
		   is_syntax_character<u8'k'>(value) || is_syntax_character<u8'l'>(value) ||
		   is_syntax_character<u8'm'>(value) || is_syntax_character<u8'n'>(value) ||
		   is_syntax_character<u8'o'>(value) || is_syntax_character<u8'p'>(value) ||
		   is_syntax_character<u8'q'>(value) || is_syntax_character<u8'r'>(value) ||
		   is_syntax_character<u8's'>(value) || is_syntax_character<u8't'>(value) ||
		   is_syntax_character<u8'u'>(value) || is_syntax_character<u8'v'>(value) ||
		   is_syntax_character<u8'w'>(value) || is_syntax_character<u8'x'>(value) ||
		   is_syntax_character<u8'y'>(value) || is_syntax_character<u8'z'>(value) ||
		   is_syntax_character<u8'A'>(value) || is_syntax_character<u8'B'>(value) ||
		   is_syntax_character<u8'C'>(value) || is_syntax_character<u8'D'>(value) ||
		   is_syntax_character<u8'E'>(value) || is_syntax_character<u8'F'>(value) ||
		   is_syntax_character<u8'G'>(value) || is_syntax_character<u8'H'>(value) ||
		   is_syntax_character<u8'I'>(value) || is_syntax_character<u8'J'>(value) ||
		   is_syntax_character<u8'K'>(value) || is_syntax_character<u8'L'>(value) ||
		   is_syntax_character<u8'M'>(value) || is_syntax_character<u8'N'>(value) ||
		   is_syntax_character<u8'O'>(value) || is_syntax_character<u8'P'>(value) ||
		   is_syntax_character<u8'Q'>(value) || is_syntax_character<u8'R'>(value) ||
		   is_syntax_character<u8'S'>(value) || is_syntax_character<u8'T'>(value) ||
		   is_syntax_character<u8'U'>(value) || is_syntax_character<u8'V'>(value) ||
		   is_syntax_character<u8'W'>(value) || is_syntax_character<u8'X'>(value) ||
		   is_syntax_character<u8'Y'>(value) || is_syntax_character<u8'Z'>(value);
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr bool is_identifier_continuation(char_type value) noexcept
{
	return is_identifier_initial(value) || syntax_digit_value(value) != 10u;
}

[[nodiscard]] inline constexpr bool checked_decimal_accumulate(::std::size_t &value, unsigned digit) noexcept
{
	constexpr auto maximum{(::std::numeric_limits<::std::size_t>::max)()};
	if (value > (maximum - digit) / 10u)
	{
		return false;
	}
	value = value * 10u + digit;
	return true;
}

} // namespace fast_io::fmt::details

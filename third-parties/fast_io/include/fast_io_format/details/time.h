#pragma once

/*
 * Chrono-format parser and semantic model (FMT level).
 *
 * Calendar fields, time-zone state, subsecond precision, padding, and chrono
 * conversion specifications are compiled into structural state and typed
 * printable adapters. The file owns chrono source-language meaning only.
 * Numeric/text emission and all output-device behavior are delegated to
 * existing IO printable protocols.
 */

#include "program.h"
#include "../types.h"
// Time lowering consumes only the freestanding print CPOs.  Pulling hosted
// filesystem and legacy-stream adapters into a string-only format operation
// would make that operation depend on facilities it can never call.
#include "../../fast_io_freestanding.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace fast_io::fmt::details
{

enum class chrono_padding : unsigned char;

struct chrono_calendar_fields
{
	::std::int_least64_t year{};
	unsigned month{};
	unsigned month_day{};
	unsigned weekday{};
	unsigned year_day{};
	unsigned hour{};
	unsigned minute{};
	unsigned second{};
};

template <bool has_utc_offset_value, bool has_time_zone_name_value>
struct basic_chrono_calendar_state
{
	static inline constexpr bool has_utc_offset{has_utc_offset_value};
	static inline constexpr bool has_time_zone_name{has_time_zone_name_value};

	chrono_calendar_fields value{};
	::std::uint_least64_t fractional_second{};
	unsigned fractional_precision{};
	::std::int_least32_t utc_offset{};
};

struct chrono_iso_week_fields
{
	::std::int_least64_t year{};
	unsigned week{};
};

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr char_type chrono_basic_latin(char8_t value) noexcept;

template <::fast_io::fmt::format_character char_type, ::std::size_t extent>
inline constexpr char_type *write_chrono_ascii(
	char_type *output, char8_t const (&text)[extent]) noexcept;

[[nodiscard]] inline constexpr ::std::int_least64_t chrono_tm_year(chrono_calendar_fields const &value) noexcept;
[[nodiscard]] inline constexpr unsigned chrono_tm_month(chrono_calendar_fields const &value) noexcept;
[[nodiscard]] inline constexpr unsigned chrono_tm_month_day(chrono_calendar_fields const &value) noexcept;
[[nodiscard]] inline constexpr unsigned chrono_tm_weekday(chrono_calendar_fields const &value) noexcept;
[[nodiscard]] inline constexpr unsigned chrono_tm_year_day(chrono_calendar_fields const &value) noexcept;
[[nodiscard]] inline constexpr unsigned chrono_tm_hour(chrono_calendar_fields const &value) noexcept;
[[nodiscard]] inline constexpr unsigned chrono_tm_minute(chrono_calendar_fields const &value) noexcept;
[[nodiscard]] inline constexpr unsigned chrono_tm_second(chrono_calendar_fields const &value) noexcept;
[[nodiscard]] inline constexpr chrono_iso_week_fields chrono_make_iso_week_fields(
	chrono_calendar_fields const &value) noexcept;
[[nodiscard]] inline constexpr ::std::int64_t chrono_floor_divide(
	::std::int64_t numerator, ::std::int64_t denominator) noexcept;
[[nodiscard]] inline constexpr unsigned chrono_positive_modulo(
	::std::int64_t value, unsigned modulus) noexcept;
[[nodiscard]] inline constexpr ::std::int_least64_t chrono_floor_century(::std::int_least64_t year) noexcept;
[[nodiscard]] inline constexpr unsigned chrono_short_year(::std::int_least64_t year) noexcept;

template <::fast_io::fmt::format_character char_type, typename integer_type>
inline constexpr char_type *write_chrono_integer(
	char_type *output, integer_type value) noexcept;

template <::fast_io::fmt::format_character char_type, typename integer_type>
inline constexpr char_type *write_chrono_padded_integer(
	char_type *output, integer_type value, unsigned width,
	chrono_padding padding) noexcept;

template <typename integer_type>
[[nodiscard]] inline constexpr ::std::size_t chrono_padded_integer_size(
	integer_type value, unsigned width, chrono_padding padding) noexcept;

template <typename signed_type>
[[nodiscard]] inline constexpr ::std::make_unsigned_t<signed_type>
chrono_unsigned_magnitude(signed_type value) noexcept;

[[nodiscard]] inline constexpr bool chrono_is_gregorian_leap_year(
	::std::int_least64_t year) noexcept;

template <::fast_io::fmt::format_character char_type>
inline constexpr char_type *write_chrono_separator(
	char_type *output, char8_t separator) noexcept;

enum class chrono_parse_error : unsigned char
{
	none,
	invalid_slice,
	dangling_percent,
	invalid_conversion,
	utc_offset_not_supported,
	time_zone_name_not_supported,
	locale_modifier_not_supported,
	brace_in_chrono_literal,
	capacity_exceeded
};

enum class chrono_padding : unsigned char
{
	zero,
	space,
	none
};

/**
 * A normalized subset of the chrono conversion vocabulary.
 *
 * Composite spellings (`%D`, `%F`, `%R`, `%T`, `%c`, `%x`, and `%X`) remain
 * single operations.  Expanding them in the parser would enlarge the NTTP IR
 * and compiler memory footprint, while expanding them with `if constexpr` in
 * the emitter produces the same straight-line stores.  There is no run-time
 * format-string or opcode interpretation in either case.
 */
enum class chrono_opcode : unsigned char
{
	literal,
	percent,
	newline,
	tab,
	abbreviated_weekday,
	full_weekday,
	abbreviated_month,
	full_month,
	date_time,
	century,
	day_of_month,
	us_date,
	space_day_of_month,
	iso_date,
	iso_week_based_short_year,
	iso_week_based_year,
	hour_24,
	hour_12,
	day_of_year,
	month,
	minute,
	am_pm,
	time_12,
	time_hm,
	second,
	time_hms,
	iso_weekday,
	sunday_week,
	iso_week,
	sunday_weekday,
	monday_week,
	locale_date,
	locale_time,
	short_year,
	year,
	utc_offset,
	time_zone_name,
	default_date_time
};

struct chrono_operation
{
	chrono_opcode opcode{chrono_opcode::literal};
	chrono_padding padding{chrono_padding::zero};
	source_slice literal{};
};

template <::fast_io::fmt::format_character char_type, ::std::size_t capacity>
struct basic_chrono_program
{
	using value_type = char_type;
	static inline constexpr ::std::size_t maximum_capacity{capacity};

	fixed_capacity_array<chrono_operation, capacity> operations{};
	::std::size_t operation_count{};

	inline constexpr bool append_literal(::std::size_t offset) noexcept
	{
		if (operation_count != 0u)
		{
			auto &last{operations[operation_count - 1u]};
			if (last.opcode == chrono_opcode::literal &&
				last.literal.offset + last.literal.size == offset)
			{
				++last.literal.size;
				return true;
			}
		}
		if (operation_count == capacity)
		{
			return false;
		}
		operations[operation_count++] = {
			chrono_opcode::literal, chrono_padding::zero, {offset, 1u}};
		return true;
	}

	inline constexpr bool append_conversion(
		chrono_opcode opcode, chrono_padding padding = chrono_padding::zero) noexcept
	{
		if (operation_count == capacity)
		{
			return false;
		}
		operations[operation_count++] = {opcode, padding, {}};
		return true;
	}
};

template <typename program_type>
struct chrono_parse_result
{
	program_type program{};
	chrono_parse_error error{chrono_parse_error::none};
	::std::size_t error_position{};

	[[nodiscard]] inline constexpr explicit operator bool() const noexcept
	{
		return error == chrono_parse_error::none;
	}
};

template <typename result_type>
inline constexpr bool set_chrono_error(
	result_type &result, chrono_parse_error error, ::std::size_t position) noexcept
{
	result.error = error;
	result.error_position = position;
	return false;
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr bool is_chrono_padding_modifier(char_type value) noexcept
{
	return is_syntax_character<u8'_'>(value) || is_syntax_character<u8'-'>(value) ||
		   is_syntax_character<u8'0'>(value);
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr chrono_padding decode_chrono_padding(char_type value) noexcept
{
	if (is_syntax_character<u8'_'>(value))
	{
		return chrono_padding::space;
	}
	if (is_syntax_character<u8'-'>(value))
	{
		return chrono_padding::none;
	}
	return chrono_padding::zero;
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr bool decode_chrono_opcode(
	char_type value, chrono_opcode &opcode) noexcept
{
	if (is_syntax_character<u8'a'>(value))
	{
		opcode = chrono_opcode::abbreviated_weekday;
	}
	else if (is_syntax_character<u8'A'>(value))
	{
		opcode = chrono_opcode::full_weekday;
	}
	else if (is_syntax_character<u8'b'>(value) || is_syntax_character<u8'h'>(value))
	{
		opcode = chrono_opcode::abbreviated_month;
	}
	else if (is_syntax_character<u8'B'>(value))
	{
		opcode = chrono_opcode::full_month;
	}
	else if (is_syntax_character<u8'c'>(value))
	{
		opcode = chrono_opcode::date_time;
	}
	else if (is_syntax_character<u8'C'>(value))
	{
		opcode = chrono_opcode::century;
	}
	else if (is_syntax_character<u8'd'>(value))
	{
		opcode = chrono_opcode::day_of_month;
	}
	else if (is_syntax_character<u8'D'>(value))
	{
		opcode = chrono_opcode::us_date;
	}
	else if (is_syntax_character<u8'e'>(value))
	{
		opcode = chrono_opcode::space_day_of_month;
	}
	else if (is_syntax_character<u8'F'>(value))
	{
		opcode = chrono_opcode::iso_date;
	}
	else if (is_syntax_character<u8'g'>(value))
	{
		opcode = chrono_opcode::iso_week_based_short_year;
	}
	else if (is_syntax_character<u8'G'>(value))
	{
		opcode = chrono_opcode::iso_week_based_year;
	}
	else if (is_syntax_character<u8'H'>(value))
	{
		opcode = chrono_opcode::hour_24;
	}
	else if (is_syntax_character<u8'I'>(value))
	{
		opcode = chrono_opcode::hour_12;
	}
	else if (is_syntax_character<u8'j'>(value))
	{
		opcode = chrono_opcode::day_of_year;
	}
	else if (is_syntax_character<u8'm'>(value))
	{
		opcode = chrono_opcode::month;
	}
	else if (is_syntax_character<u8'M'>(value))
	{
		opcode = chrono_opcode::minute;
	}
	else if (is_syntax_character<u8'n'>(value))
	{
		opcode = chrono_opcode::newline;
	}
	else if (is_syntax_character<u8'p'>(value))
	{
		opcode = chrono_opcode::am_pm;
	}
	else if (is_syntax_character<u8'r'>(value))
	{
		opcode = chrono_opcode::time_12;
	}
	else if (is_syntax_character<u8'R'>(value))
	{
		opcode = chrono_opcode::time_hm;
	}
	else if (is_syntax_character<u8'S'>(value))
	{
		opcode = chrono_opcode::second;
	}
	else if (is_syntax_character<u8't'>(value))
	{
		opcode = chrono_opcode::tab;
	}
	else if (is_syntax_character<u8'T'>(value))
	{
		opcode = chrono_opcode::time_hms;
	}
	else if (is_syntax_character<u8'u'>(value))
	{
		opcode = chrono_opcode::iso_weekday;
	}
	else if (is_syntax_character<u8'U'>(value))
	{
		opcode = chrono_opcode::sunday_week;
	}
	else if (is_syntax_character<u8'V'>(value))
	{
		opcode = chrono_opcode::iso_week;
	}
	else if (is_syntax_character<u8'w'>(value))
	{
		opcode = chrono_opcode::sunday_weekday;
	}
	else if (is_syntax_character<u8'W'>(value))
	{
		opcode = chrono_opcode::monday_week;
	}
	else if (is_syntax_character<u8'x'>(value))
	{
		opcode = chrono_opcode::locale_date;
	}
	else if (is_syntax_character<u8'X'>(value))
	{
		opcode = chrono_opcode::locale_time;
	}
	else if (is_syntax_character<u8'y'>(value))
	{
		opcode = chrono_opcode::short_year;
	}
	else if (is_syntax_character<u8'Y'>(value))
	{
		opcode = chrono_opcode::year;
	}
	else if (is_syntax_character<u8'z'>(value))
	{
		opcode = chrono_opcode::utc_offset;
	}
	else if (is_syntax_character<u8'Z'>(value))
	{
		opcode = chrono_opcode::time_zone_name;
	}
	else if (is_syntax_character<u8'%'>(value))
	{
		opcode = chrono_opcode::percent;
	}
	else
	{
		return false;
	}
	return true;
}

[[nodiscard]] inline constexpr bool chrono_padding_supported(chrono_opcode opcode) noexcept
{
	return opcode == chrono_opcode::hour_24 || opcode == chrono_opcode::hour_12 ||
		   opcode == chrono_opcode::minute || opcode == chrono_opcode::second ||
		   opcode == chrono_opcode::sunday_week || opcode == chrono_opcode::iso_week ||
		   opcode == chrono_opcode::monday_week || opcode == chrono_opcode::year ||
		   opcode == chrono_opcode::day_of_month || opcode == chrono_opcode::space_day_of_month ||
		   opcode == chrono_opcode::day_of_year || opcode == chrono_opcode::month;
}

/**
 * Compiles one chrono sub-specification into a compact flat program.
 *
 * `specification` addresses the original structural format literal and excludes
 * the generic brace width/precision prefix and the closing brace.  Parsing is
 * immediate-only; no non-consteval overload is provided.  The later emitter
 * expands every operation index into a distinct template instantiation, so the
 * program is compiler metadata rather than a run-time bytecode stream.
 */
template <::fast_io::fmt::basic_fixed_string format_literal, source_slice specification,
		  bool has_utc_offset = false, bool has_time_zone_name = false>
[[nodiscard]] consteval auto parse_chrono_program() noexcept
{
	using char_type = typename decltype(format_literal)::value_type;
	constexpr ::std::size_t format_size{format_literal.size()};
	using program_type = basic_chrono_program<char_type, specification.size == 0u ? 1u : specification.size>;
	chrono_parse_result<program_type> result{};

	if (specification.offset > format_size ||
		specification.size > format_size - specification.offset)
	{
		set_chrono_error(result, chrono_parse_error::invalid_slice, specification.offset);
		return result;
	}

	if constexpr (specification.size == 0u)
	{
		(void)result.program.append_conversion(chrono_opcode::default_date_time);
		return result;
	}

	constexpr ::std::size_t end{specification.offset + specification.size};
	::std::size_t cursor{specification.offset};
	while (cursor != end)
	{
		auto const value{format_literal[cursor]};
		if (!is_syntax_character<u8'%'>(value))
		{
			if (is_syntax_character<u8'{'>(value) || is_syntax_character<u8'}'>(value))
			{
				set_chrono_error(result, chrono_parse_error::brace_in_chrono_literal, cursor);
				return result;
			}
			if (!result.program.append_literal(cursor))
			{
				set_chrono_error(result, chrono_parse_error::capacity_exceeded, cursor);
				return result;
			}
			++cursor;
			continue;
		}

		::std::size_t const percent_position{cursor++};
		if (cursor == end)
		{
			set_chrono_error(result, chrono_parse_error::dangling_percent, percent_position);
			return result;
		}

		chrono_padding padding{chrono_padding::zero};
		if (is_chrono_padding_modifier(format_literal[cursor]))
		{
			padding = decode_chrono_padding(format_literal[cursor]);
			++cursor;
			if (cursor == end)
			{
				set_chrono_error(result, chrono_parse_error::dangling_percent, percent_position);
				return result;
			}
		}

		// E/O request locale-specific alternate representations.  Silently treating
		// them as ASCII would be observably wrong on a non-C locale, so the basic,
		// allocation-free backend rejects them until an explicit locale object is
		// carried by the public format front door.
		if (is_syntax_character<u8'E'>(format_literal[cursor]) ||
			is_syntax_character<u8'O'>(format_literal[cursor]))
		{
			set_chrono_error(result, chrono_parse_error::locale_modifier_not_supported, cursor);
			return result;
		}

		chrono_opcode opcode{};
		if (!decode_chrono_opcode(format_literal[cursor], opcode))
		{
			set_chrono_error(result, chrono_parse_error::invalid_conversion, cursor);
			return result;
		}
		if (opcode == chrono_opcode::utc_offset && !has_utc_offset)
		{
			set_chrono_error(result, chrono_parse_error::utc_offset_not_supported, cursor);
			return result;
		}
		if (opcode == chrono_opcode::time_zone_name && !has_time_zone_name)
		{
			set_chrono_error(result, chrono_parse_error::time_zone_name_not_supported, cursor);
			return result;
		}
		if (padding != chrono_padding::zero && !chrono_padding_supported(opcode))
		{
			set_chrono_error(result, chrono_parse_error::invalid_conversion, cursor);
			return result;
		}
		if (!result.program.append_conversion(opcode, padding))
		{
			set_chrono_error(result, chrono_parse_error::capacity_exceeded, cursor);
			return result;
		}
		++cursor;
	}
	return result;
}

template <chrono_parse_error error>
consteval void diagnose_chrono_parse_error()
{
	if constexpr (error == chrono_parse_error::invalid_slice)
	{
		static_assert(error == chrono_parse_error::none, "fast_io time: invalid source slice");
	}
	else if constexpr (error == chrono_parse_error::dangling_percent)
	{
		static_assert(error == chrono_parse_error::none, "fast_io time: dangling percent conversion");
	}
	else if constexpr (error == chrono_parse_error::invalid_conversion)
	{
		static_assert(error == chrono_parse_error::none, "fast_io time: invalid conversion specifier");
	}
	else if constexpr (error == chrono_parse_error::utc_offset_not_supported)
	{
		static_assert(error == chrono_parse_error::none,
					  "fast_io time: %z requires a value with a numeric UTC offset");
	}
	else if constexpr (error == chrono_parse_error::time_zone_name_not_supported)
	{
		static_assert(error == chrono_parse_error::none,
					  "fast_io time: %Z requires a value with a time-zone name");
	}
	else if constexpr (error == chrono_parse_error::locale_modifier_not_supported)
	{
		static_assert(error == chrono_parse_error::none,
					  "fast_io time: E/O locale modifiers require a locale-aware format overload");
	}
	else if constexpr (error == chrono_parse_error::brace_in_chrono_literal)
	{
		static_assert(error == chrono_parse_error::none,
					  "fast_io time: brace is not permitted as a time-format literal");
	}
	else if constexpr (error == chrono_parse_error::capacity_exceeded)
	{
		static_assert(error == chrono_parse_error::none, "fast_io time: internal program capacity exceeded");
	}
}

template <::fast_io::fmt::basic_fixed_string format_literal, source_slice specification,
		  bool has_utc_offset = false, bool has_time_zone_name = false>
[[nodiscard]] consteval auto make_checked_chrono_program()
{
	constexpr auto parsed{parse_chrono_program<format_literal, specification,
											   has_utc_offset, has_time_zone_name>()};
	if constexpr (parsed.error != chrono_parse_error::none)
	{
		diagnose_chrono_parse_error<parsed.error>();
	}
	return parsed.program;
}

template <::fast_io::fmt::basic_fixed_string format_literal, source_slice specification,
		  bool has_utc_offset = false, bool has_time_zone_name = false>
inline constexpr auto checked_chrono_program{
	make_checked_chrono_program<format_literal, specification,
								has_utc_offset, has_time_zone_name>()};

/**
 * Reports whether a compiled native-time program is independent of locale.
 *
 * Scan the exact checked program used by the emitter so capability diagnostics
 * and compile-time classification share one parser specialization.  Escaped
 * text such as %%c is consequently a literal and remains eligible for static
 * rendering.
 */
template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification, bool has_utc_offset,
		  bool has_time_zone_name>
[[nodiscard]] consteval bool
chrono_static_program_is_locale_free() noexcept
{
	constexpr auto const &program{
		checked_chrono_program<format_literal, specification,
						   has_utc_offset, has_time_zone_name>};
	for (::std::size_t index{}; index != program.operation_count; ++index)
	{
		auto const opcode{program.operations[index].opcode};
		if (opcode == chrono_opcode::date_time ||
			opcode == chrono_opcode::locale_date ||
			opcode == chrono_opcode::locale_time)
		{
			return false;
		}
	}
	return true;
}

} // namespace fast_io::fmt::details

namespace fast_io::fmt::details
{

template <::fast_io::fmt::format_character char_type>
inline constexpr char_type *write_chrono_weekday_name(
	char_type *output, unsigned weekday, bool full)
{
	if (weekday > 6u)
	{
		::fast_io::fast_terminate();
	}
	if (full)
	{
		switch (weekday)
		{
		case 0u:
			return write_chrono_ascii(output, u8"Sunday");
		case 1u:
			return write_chrono_ascii(output, u8"Monday");
		case 2u:
			return write_chrono_ascii(output, u8"Tuesday");
		case 3u:
			return write_chrono_ascii(output, u8"Wednesday");
		case 4u:
			return write_chrono_ascii(output, u8"Thursday");
		case 5u:
			return write_chrono_ascii(output, u8"Friday");
		default:
			return write_chrono_ascii(output, u8"Saturday");
		}
	}
	switch (weekday)
	{
	case 0u:
		return write_chrono_ascii(output, u8"Sun");
	case 1u:
		return write_chrono_ascii(output, u8"Mon");
	case 2u:
		return write_chrono_ascii(output, u8"Tue");
	case 3u:
		return write_chrono_ascii(output, u8"Wed");
	case 4u:
		return write_chrono_ascii(output, u8"Thu");
	case 5u:
		return write_chrono_ascii(output, u8"Fri");
	default:
		return write_chrono_ascii(output, u8"Sat");
	}
}

[[nodiscard]] inline constexpr ::std::size_t chrono_weekday_name_size(
	unsigned weekday, bool full)
{
	if (!full)
	{
		return 3u;
	}
	switch (weekday)
	{
	case 0u:
		return 6u;
	case 1u:
		return 6u;
	case 2u:
		return 7u;
	case 3u:
		return 9u;
	case 4u:
		return 8u;
	case 5u:
		return 6u;
	default:
		return 8u;
	}
}

template <::fast_io::fmt::format_character char_type>
inline constexpr char_type *write_chrono_month_name(
	char_type *output, unsigned month, bool full)
{
	if (month < 1u || month > 12u)
	{
		::fast_io::fast_terminate();
	}
	if (full)
	{
		switch (month)
		{
		case 1u:
			return write_chrono_ascii(output, u8"January");
		case 2u:
			return write_chrono_ascii(output, u8"February");
		case 3u:
			return write_chrono_ascii(output, u8"March");
		case 4u:
			return write_chrono_ascii(output, u8"April");
		case 5u:
			return write_chrono_ascii(output, u8"May");
		case 6u:
			return write_chrono_ascii(output, u8"June");
		case 7u:
			return write_chrono_ascii(output, u8"July");
		case 8u:
			return write_chrono_ascii(output, u8"August");
		case 9u:
			return write_chrono_ascii(output, u8"September");
		case 10u:
			return write_chrono_ascii(output, u8"October");
		case 11u:
			return write_chrono_ascii(output, u8"November");
		default:
			return write_chrono_ascii(output, u8"December");
		}
	}
	switch (month)
	{
	case 1u:
		return write_chrono_ascii(output, u8"Jan");
	case 2u:
		return write_chrono_ascii(output, u8"Feb");
	case 3u:
		return write_chrono_ascii(output, u8"Mar");
	case 4u:
		return write_chrono_ascii(output, u8"Apr");
	case 5u:
		return write_chrono_ascii(output, u8"May");
	case 6u:
		return write_chrono_ascii(output, u8"Jun");
	case 7u:
		return write_chrono_ascii(output, u8"Jul");
	case 8u:
		return write_chrono_ascii(output, u8"Aug");
	case 9u:
		return write_chrono_ascii(output, u8"Sep");
	case 10u:
		return write_chrono_ascii(output, u8"Oct");
	case 11u:
		return write_chrono_ascii(output, u8"Nov");
	default:
		return write_chrono_ascii(output, u8"Dec");
	}
}

[[nodiscard]] inline constexpr ::std::size_t chrono_month_name_size(
	unsigned month, bool full)
{
	if (!full)
	{
		return 3u;
	}
	switch (month)
	{
	case 1u:
		return 7u;
	case 2u:
		return 8u;
	case 3u:
		return 5u;
	case 4u:
		return 5u;
	case 5u:
		return 3u;
	case 6u:
		return 4u;
	case 7u:
		return 4u;
	case 8u:
		return 6u;
	case 9u:
		return 9u;
	case 10u:
		return 7u;
	case 11u:
		return 8u;
	default:
		return 8u;
	}
}

[[nodiscard]] inline constexpr ::std::int_least64_t chrono_floor_century(
	::std::int_least64_t year) noexcept
{
	return chrono_floor_divide(year, 100);
}

[[nodiscard]] inline constexpr unsigned chrono_short_year(
	::std::int_least64_t year) noexcept
{
	return chrono_positive_modulo(year, 100u);
}

template <::fast_io::fmt::format_character char_type>
inline constexpr char_type *write_chrono_separator(char_type *output, char8_t separator) noexcept
{
	*output++ = chrono_basic_latin<char_type>(separator);
	return output;
}

template <::fast_io::fmt::format_character char_type, typename state_type>
inline constexpr char_type *write_chrono_calendar_second(
	char_type *output, state_type const &state, chrono_padding padding)
{
	auto const second{chrono_tm_second(state.value)};
	auto const precision{state.fractional_precision};
	if (precision == 0u)
	{
		return write_chrono_padded_integer(output, second, 2u, padding);
	}
	if (precision > ::std::numeric_limits<::std::uint_least64_t>::digits10 ||
		state.fractional_second >= ::fast_io::uint_least64_subseconds_per_second)
	{
		::fast_io::fast_terminate();
	}
	output = write_chrono_padded_integer(output, second, 2u, padding);
	*output++ = chrono_basic_latin<char_type>(u8'.');
	auto fraction{state.fractional_second};
	for (unsigned discarded{static_cast<unsigned>(
								::std::numeric_limits<::std::uint_least64_t>::digits10) -
							precision};
		 discarded != 0u; --discarded)
	{
		fraction /= 10u;
	}
	return write_chrono_padded_integer(
		output, fraction, precision, chrono_padding::zero);
}

template <typename state_type>
[[nodiscard]] inline constexpr ::std::size_t chrono_calendar_second_capacity(
	state_type const &state, chrono_padding padding) noexcept
{
	auto const second{chrono_tm_second(state.value)};
	if (state.fractional_precision == 0u)
	{
		return chrono_padded_integer_size(second, 2u, padding);
	}
	if (state.fractional_precision >
			::std::numeric_limits<::std::uint_least64_t>::digits10 ||
		state.fractional_second >= ::fast_io::uint_least64_subseconds_per_second)
	{
		::fast_io::fast_terminate();
	}
	return chrono_padded_integer_size(second, 2u, padding) + 1u +
		   state.fractional_precision;
}

template <chrono_opcode opcode, chrono_padding padding,
		  ::fast_io::fmt::format_character char_type, typename state_type>
inline constexpr char_type *emit_chrono_calendar_operation(
	char_type *output, state_type const &state)
{
	auto const &time{state.value};
	if constexpr (opcode == chrono_opcode::abbreviated_weekday)
	{
		return write_chrono_weekday_name(output, chrono_tm_weekday(time), false);
	}
	else if constexpr (opcode == chrono_opcode::full_weekday)
	{
		return write_chrono_weekday_name(output, chrono_tm_weekday(time), true);
	}
	else if constexpr (opcode == chrono_opcode::abbreviated_month)
	{
		return write_chrono_month_name(output, chrono_tm_month(time), false);
	}
	else if constexpr (opcode == chrono_opcode::full_month)
	{
		return write_chrono_month_name(output, chrono_tm_month(time), true);
	}
	else if constexpr (opcode == chrono_opcode::century)
	{
		return write_chrono_padded_integer(output,
										   chrono_floor_century(chrono_tm_year(time)), 2u,
										   chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::day_of_month)
	{
		return write_chrono_padded_integer(
			output, chrono_tm_month_day(time), 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::space_day_of_month)
	{
		return write_chrono_padded_integer(output, chrono_tm_month_day(time), 2u,
										   padding == chrono_padding::zero ? chrono_padding::space : padding);
	}
	else if constexpr (opcode == chrono_opcode::iso_week_based_short_year)
	{
		auto const iso{chrono_make_iso_week_fields(time)};
		return write_chrono_padded_integer(
			output, chrono_short_year(iso.year), 2u, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::iso_week_based_year)
	{
		return write_chrono_padded_integer(
			output, chrono_make_iso_week_fields(time).year, 4u,
			chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::hour_24)
	{
		return write_chrono_padded_integer(
			output, chrono_tm_hour(time), 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::hour_12)
	{
		auto hour{chrono_tm_hour(time) % 12u};
		if (hour == 0u)
		{
			hour = 12u;
		}
		return write_chrono_padded_integer(output, hour, 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::day_of_year)
	{
		return write_chrono_padded_integer(
			output, chrono_tm_year_day(time) + 1u, 3u, padding);
	}
	else if constexpr (opcode == chrono_opcode::month)
	{
		return write_chrono_padded_integer(
			output, chrono_tm_month(time), 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::minute)
	{
		return write_chrono_padded_integer(
			output, chrono_tm_minute(time), 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::am_pm)
	{
		return write_chrono_ascii(
			output, chrono_tm_hour(time) < 12u ? u8"AM" : u8"PM");
	}
	else if constexpr (opcode == chrono_opcode::second)
	{
		return write_chrono_calendar_second(output, state, padding);
	}
	else if constexpr (opcode == chrono_opcode::iso_weekday)
	{
		auto const weekday{chrono_tm_weekday(time)};
		return write_chrono_integer(output, weekday == 0u ? 7u : weekday);
	}
	else if constexpr (opcode == chrono_opcode::sunday_weekday)
	{
		return write_chrono_integer(output, chrono_tm_weekday(time));
	}
	else if constexpr (opcode == chrono_opcode::sunday_week)
	{
		auto const year_day{chrono_tm_year_day(time)};
		auto const weekday{chrono_tm_weekday(time)};
		auto const week{(year_day + 7u - weekday) / 7u};
		return write_chrono_padded_integer(output, week, 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::monday_week)
	{
		auto const year_day{chrono_tm_year_day(time)};
		auto const monday_based{(chrono_tm_weekday(time) + 6u) % 7u};
		auto const week{(year_day + 7u - monday_based) / 7u};
		return write_chrono_padded_integer(output, week, 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::iso_week)
	{
		return write_chrono_padded_integer(
			output, chrono_make_iso_week_fields(time).week, 2u, padding);
	}
	else if constexpr (opcode == chrono_opcode::short_year)
	{
		return write_chrono_padded_integer(output,
										   chrono_short_year(chrono_tm_year(time)), 2u,
										   chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::year)
	{
		return write_chrono_padded_integer(
			output, chrono_tm_year(time), 4u, padding);
	}
	else if constexpr (opcode == chrono_opcode::us_date || opcode == chrono_opcode::locale_date)
	{
		output = write_chrono_padded_integer(
			output, chrono_tm_month(time), 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8'/');
		output = write_chrono_padded_integer(
			output, chrono_tm_month_day(time), 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8'/');
		return write_chrono_padded_integer(output,
										   chrono_short_year(chrono_tm_year(time)), 2u,
										   chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::iso_date)
	{
		output = write_chrono_padded_integer(
			output, chrono_tm_year(time), 4u, chrono_padding::zero);
		output = write_chrono_separator(output, u8'-');
		output = write_chrono_padded_integer(
			output, chrono_tm_month(time), 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8'-');
		return write_chrono_padded_integer(
			output, chrono_tm_month_day(time), 2u, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::time_hm)
	{
		output = write_chrono_padded_integer(
			output, chrono_tm_hour(time), 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8':');
		return write_chrono_padded_integer(
			output, chrono_tm_minute(time), 2u, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::time_hms || opcode == chrono_opcode::locale_time)
	{
		output = write_chrono_padded_integer(
			output, chrono_tm_hour(time), 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8':');
		output = write_chrono_padded_integer(
			output, chrono_tm_minute(time), 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8':');
		return write_chrono_calendar_second(output, state, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::time_12)
	{
		auto const hour_24{chrono_tm_hour(time)};
		auto hour{hour_24 % 12u};
		if (hour == 0u)
		{
			hour = 12u;
		}
		output = write_chrono_padded_integer(output, hour, 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8':');
		output = write_chrono_padded_integer(
			output, chrono_tm_minute(time), 2u, chrono_padding::zero);
		output = write_chrono_separator(output, u8':');
		output = write_chrono_calendar_second(output, state, chrono_padding::zero);
		output = write_chrono_separator(output, u8' ');
		return write_chrono_ascii(output, hour_24 < 12u ? u8"AM" : u8"PM");
	}
	else if constexpr (opcode == chrono_opcode::date_time)
	{
		output = write_chrono_weekday_name(
			output, chrono_tm_weekday(time), false);
		output = write_chrono_separator(output, u8' ');
		output = write_chrono_month_name(output, chrono_tm_month(time), false);
		output = write_chrono_separator(output, u8' ');
		output = write_chrono_padded_integer(
			output, chrono_tm_month_day(time), 2u, chrono_padding::space);
		output = write_chrono_separator(output, u8' ');
		output = emit_chrono_calendar_operation<chrono_opcode::time_hms,
												chrono_padding::zero>(output, state);
		output = write_chrono_separator(output, u8' ');
		return write_chrono_padded_integer(
			output, chrono_tm_year(time), 4u, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::default_date_time)
	{
		output = emit_chrono_calendar_operation<chrono_opcode::iso_date,
												chrono_padding::zero>(output, state);
		output = write_chrono_separator(output, u8' ');
		return emit_chrono_calendar_operation<chrono_opcode::time_hms,
											  chrono_padding::zero>(output, state);
	}
	else if constexpr (opcode == chrono_opcode::utc_offset)
	{
		static_assert(state_type::has_utc_offset,
					  "fast_io time: internal UTC-offset capability mismatch");
		auto offset{static_cast<::std::int_least64_t>(state.utc_offset)};
		*output++ = chrono_basic_latin<char_type>(offset < 0 ? u8'-' : u8'+');
		auto magnitude{chrono_unsigned_magnitude(offset)};
		auto const seconds{static_cast<unsigned>(magnitude % 60u)};
		magnitude /= 60u;
		auto const minutes{static_cast<unsigned>(magnitude % 60u)};
		magnitude /= 60u;
		output = write_chrono_padded_integer(
			output, magnitude, 2u, chrono_padding::zero);
		output = write_chrono_padded_integer(
			output, minutes, 2u, chrono_padding::zero);
		if (seconds != 0u)
		{
			output = write_chrono_padded_integer(
				output, seconds, 2u, chrono_padding::zero);
		}
		return output;
	}
	else if constexpr (opcode == chrono_opcode::time_zone_name)
	{
		static_assert(state_type::has_time_zone_name,
					  "fast_io time: internal time-zone-name capability mismatch");
		return write_chrono_ascii(output, u8"UTC");
	}
	else
	{
		static_assert(opcode == chrono_opcode::literal || opcode == chrono_opcode::percent ||
						  opcode == chrono_opcode::newline || opcode == chrono_opcode::tab,
					  "fast_io time: unhandled calendar opcode");
		return output;
	}
}

/**
 * Validate exactly the calendar fields read by one compiled conversion.
 *
 * Reserve sizing runs before emission for dynamically sized printables.  An
 * unconditional civil-date conversion here would make the nominally harmless
 * size pass stricter than the selected opcode.  This compile-time dispatch is
 * intentionally parallel to the emitter: discarded branches neither read nor
 * validate unrelated tuple members.
 */
template <chrono_opcode opcode>
inline constexpr void validate_chrono_calendar_operation_fields(
	chrono_calendar_fields const &time) noexcept
{
	if constexpr (opcode == chrono_opcode::abbreviated_weekday ||
				  opcode == chrono_opcode::full_weekday ||
				  opcode == chrono_opcode::iso_weekday ||
				  opcode == chrono_opcode::sunday_weekday)
	{
		(void)chrono_tm_weekday(time);
	}
	else if constexpr (opcode == chrono_opcode::abbreviated_month ||
					   opcode == chrono_opcode::full_month ||
					   opcode == chrono_opcode::month)
	{
		(void)chrono_tm_month(time);
	}
	else if constexpr (opcode == chrono_opcode::century ||
					   opcode == chrono_opcode::short_year ||
					   opcode == chrono_opcode::year)
	{
		(void)chrono_tm_year(time);
	}
	else if constexpr (opcode == chrono_opcode::day_of_month ||
					   opcode == chrono_opcode::space_day_of_month)
	{
		(void)chrono_tm_month_day(time);
	}
	else if constexpr (opcode == chrono_opcode::iso_week_based_short_year ||
					   opcode == chrono_opcode::iso_week_based_year ||
					   opcode == chrono_opcode::iso_week)
	{
		(void)chrono_make_iso_week_fields(time);
	}
	else if constexpr (opcode == chrono_opcode::hour_24 ||
					   opcode == chrono_opcode::hour_12 ||
					   opcode == chrono_opcode::am_pm)
	{
		(void)chrono_tm_hour(time);
	}
	else if constexpr (opcode == chrono_opcode::day_of_year)
	{
		(void)chrono_tm_year_day(time);
	}
	else if constexpr (opcode == chrono_opcode::minute)
	{
		(void)chrono_tm_minute(time);
	}
	else if constexpr (opcode == chrono_opcode::second)
	{
		(void)chrono_tm_second(time);
	}
	else if constexpr (opcode == chrono_opcode::sunday_week ||
					   opcode == chrono_opcode::monday_week)
	{
		(void)chrono_tm_year_day(time);
		(void)chrono_tm_weekday(time);
	}
	else if constexpr (opcode == chrono_opcode::us_date ||
					   opcode == chrono_opcode::locale_date ||
					   opcode == chrono_opcode::iso_date)
	{
		(void)chrono_tm_year(time);
		(void)chrono_tm_month(time);
		(void)chrono_tm_month_day(time);
	}
	else if constexpr (opcode == chrono_opcode::time_hm)
	{
		(void)chrono_tm_hour(time);
		(void)chrono_tm_minute(time);
	}
	else if constexpr (opcode == chrono_opcode::time_hms ||
					   opcode == chrono_opcode::locale_time ||
					   opcode == chrono_opcode::time_12)
	{
		(void)chrono_tm_hour(time);
		(void)chrono_tm_minute(time);
		(void)chrono_tm_second(time);
	}
	else if constexpr (opcode == chrono_opcode::date_time)
	{
		(void)chrono_tm_weekday(time);
		(void)chrono_tm_month(time);
		(void)chrono_tm_month_day(time);
		(void)chrono_tm_hour(time);
		(void)chrono_tm_minute(time);
		(void)chrono_tm_second(time);
		(void)chrono_tm_year(time);
	}
	else if constexpr (opcode == chrono_opcode::default_date_time)
	{
		(void)chrono_tm_year(time);
		(void)chrono_tm_month(time);
		(void)chrono_tm_month_day(time);
		(void)chrono_tm_hour(time);
		(void)chrono_tm_minute(time);
		(void)chrono_tm_second(time);
	}
}

template <chrono_opcode opcode, chrono_padding padding, typename state_type>
[[nodiscard]] inline constexpr ::std::size_t chrono_calendar_operation_capacity(
	state_type const &state)
{
	validate_chrono_calendar_operation_fields<opcode>(state.value);
	if constexpr (opcode == chrono_opcode::abbreviated_weekday)
	{
		return 3u;
	}
	else if constexpr (opcode == chrono_opcode::full_weekday)
	{
		return chrono_weekday_name_size(chrono_tm_weekday(state.value), true);
	}
	else if constexpr (opcode == chrono_opcode::abbreviated_month)
	{
		return 3u;
	}
	else if constexpr (opcode == chrono_opcode::full_month)
	{
		return chrono_month_name_size(chrono_tm_month(state.value), true);
	}
	else if constexpr (opcode == chrono_opcode::second)
	{
		return chrono_calendar_second_capacity(state, padding);
	}
	else if constexpr (opcode == chrono_opcode::date_time)
	{
		return 3u + 1u + 3u + 1u + 2u + 1u + 6u +
			   chrono_calendar_second_capacity(state, chrono_padding::zero) + 1u +
			   chrono_padded_integer_size(
				   chrono_tm_year(state.value), 4u, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::default_date_time)
	{
		return chrono_padded_integer_size(
				   chrono_tm_year(state.value), 4u, chrono_padding::zero) +
			   13u + chrono_calendar_second_capacity(state, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::us_date || opcode == chrono_opcode::locale_date)
	{
		return 8u;
	}
	else if constexpr (opcode == chrono_opcode::iso_date)
	{
		return chrono_padded_integer_size(
				   chrono_tm_year(state.value), 4u, chrono_padding::zero) +
			   6u;
	}
	else if constexpr (opcode == chrono_opcode::time_hm)
	{
		return 5u;
	}
	else if constexpr (opcode == chrono_opcode::time_hms || opcode == chrono_opcode::locale_time)
	{
		return 6u + chrono_calendar_second_capacity(state, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::time_12)
	{
		return 9u + chrono_calendar_second_capacity(state, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::utc_offset)
	{
		auto magnitude{chrono_unsigned_magnitude(
			static_cast<::std::int_least64_t>(state.utc_offset))};
		auto const seconds{magnitude % 60u};
		magnitude /= 3600u;
		return 1u + chrono_padded_integer_size(magnitude, 2u, chrono_padding::zero) +
			   2u + static_cast<::std::size_t>(seconds != 0u) * 2u;
	}
	else if constexpr (opcode == chrono_opcode::time_zone_name)
	{
		return 3u;
	}
	else if constexpr (opcode == chrono_opcode::year || opcode == chrono_opcode::iso_week_based_year)
	{
		auto const year{opcode == chrono_opcode::year
							? chrono_tm_year(state.value)
							: chrono_make_iso_week_fields(state.value).year};
		return chrono_padded_integer_size(year, 4u, chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::century)
	{
		return chrono_padded_integer_size(
			chrono_floor_century(chrono_tm_year(state.value)), 2u,
			chrono_padding::zero);
	}
	else if constexpr (opcode == chrono_opcode::day_of_year)
	{
		return 3u;
	}
	else
	{
		return 2u;
	}
}

} // namespace fast_io::fmt::details

namespace fast_io::fmt::details
{

template <typename T>
struct is_time_format_source : ::std::false_type
{};

template <typename T>
inline constexpr bool is_time_format_source_v{
	is_time_format_source<::std::remove_cvref_t<T>>::value};

template <typename T>
concept time_format_value = is_time_format_source_v<T>;

/** Converts an invariant UTF-8/ASCII code unit to the selected execution character type. */
template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr char_type chrono_basic_latin(char8_t value) noexcept
{
	// A cast from an ASCII integer is not an execution-character conversion on
	// EBCDIC.  The explicit table is verbose by design and is folded completely
	// for every literal/name call site.
	switch (value)
	{
	case u8'0':
		return ::fast_io::arithmetic_char_literal_v<u8'0', char_type>;
	case u8'1':
		return ::fast_io::arithmetic_char_literal_v<u8'1', char_type>;
	case u8'2':
		return ::fast_io::arithmetic_char_literal_v<u8'2', char_type>;
	case u8'3':
		return ::fast_io::arithmetic_char_literal_v<u8'3', char_type>;
	case u8'4':
		return ::fast_io::arithmetic_char_literal_v<u8'4', char_type>;
	case u8'5':
		return ::fast_io::arithmetic_char_literal_v<u8'5', char_type>;
	case u8'6':
		return ::fast_io::arithmetic_char_literal_v<u8'6', char_type>;
	case u8'7':
		return ::fast_io::arithmetic_char_literal_v<u8'7', char_type>;
	case u8'8':
		return ::fast_io::arithmetic_char_literal_v<u8'8', char_type>;
	case u8'9':
		return ::fast_io::arithmetic_char_literal_v<u8'9', char_type>;
	case u8'A':
		return ::fast_io::arithmetic_char_literal_v<u8'A', char_type>;
	case u8'B':
		return ::fast_io::arithmetic_char_literal_v<u8'B', char_type>;
	case u8'C':
		return ::fast_io::arithmetic_char_literal_v<u8'C', char_type>;
	case u8'D':
		return ::fast_io::arithmetic_char_literal_v<u8'D', char_type>;
	case u8'E':
		return ::fast_io::arithmetic_char_literal_v<u8'E', char_type>;
	case u8'F':
		return ::fast_io::arithmetic_char_literal_v<u8'F', char_type>;
	case u8'G':
		return ::fast_io::arithmetic_char_literal_v<u8'G', char_type>;
	case u8'H':
		return ::fast_io::arithmetic_char_literal_v<u8'H', char_type>;
	case u8'I':
		return ::fast_io::arithmetic_char_literal_v<u8'I', char_type>;
	case u8'J':
		return ::fast_io::arithmetic_char_literal_v<u8'J', char_type>;
	case u8'K':
		return ::fast_io::arithmetic_char_literal_v<u8'K', char_type>;
	case u8'L':
		return ::fast_io::arithmetic_char_literal_v<u8'L', char_type>;
	case u8'M':
		return ::fast_io::arithmetic_char_literal_v<u8'M', char_type>;
	case u8'N':
		return ::fast_io::arithmetic_char_literal_v<u8'N', char_type>;
	case u8'O':
		return ::fast_io::arithmetic_char_literal_v<u8'O', char_type>;
	case u8'P':
		return ::fast_io::arithmetic_char_literal_v<u8'P', char_type>;
	case u8'Q':
		return ::fast_io::arithmetic_char_literal_v<u8'Q', char_type>;
	case u8'R':
		return ::fast_io::arithmetic_char_literal_v<u8'R', char_type>;
	case u8'S':
		return ::fast_io::arithmetic_char_literal_v<u8'S', char_type>;
	case u8'T':
		return ::fast_io::arithmetic_char_literal_v<u8'T', char_type>;
	case u8'U':
		return ::fast_io::arithmetic_char_literal_v<u8'U', char_type>;
	case u8'V':
		return ::fast_io::arithmetic_char_literal_v<u8'V', char_type>;
	case u8'W':
		return ::fast_io::arithmetic_char_literal_v<u8'W', char_type>;
	case u8'X':
		return ::fast_io::arithmetic_char_literal_v<u8'X', char_type>;
	case u8'Y':
		return ::fast_io::arithmetic_char_literal_v<u8'Y', char_type>;
	case u8'Z':
		return ::fast_io::arithmetic_char_literal_v<u8'Z', char_type>;
	case u8'a':
		return ::fast_io::arithmetic_char_literal_v<u8'a', char_type>;
	case u8'b':
		return ::fast_io::arithmetic_char_literal_v<u8'b', char_type>;
	case u8'c':
		return ::fast_io::arithmetic_char_literal_v<u8'c', char_type>;
	case u8'd':
		return ::fast_io::arithmetic_char_literal_v<u8'd', char_type>;
	case u8'e':
		return ::fast_io::arithmetic_char_literal_v<u8'e', char_type>;
	case u8'f':
		return ::fast_io::arithmetic_char_literal_v<u8'f', char_type>;
	case u8'g':
		return ::fast_io::arithmetic_char_literal_v<u8'g', char_type>;
	case u8'h':
		return ::fast_io::arithmetic_char_literal_v<u8'h', char_type>;
	case u8'i':
		return ::fast_io::arithmetic_char_literal_v<u8'i', char_type>;
	case u8'j':
		return ::fast_io::arithmetic_char_literal_v<u8'j', char_type>;
	case u8'k':
		return ::fast_io::arithmetic_char_literal_v<u8'k', char_type>;
	case u8'l':
		return ::fast_io::arithmetic_char_literal_v<u8'l', char_type>;
	case u8'm':
		return ::fast_io::arithmetic_char_literal_v<u8'm', char_type>;
	case u8'n':
		return ::fast_io::arithmetic_char_literal_v<u8'n', char_type>;
	case u8'o':
		return ::fast_io::arithmetic_char_literal_v<u8'o', char_type>;
	case u8'p':
		return ::fast_io::arithmetic_char_literal_v<u8'p', char_type>;
	case u8'q':
		return ::fast_io::arithmetic_char_literal_v<u8'q', char_type>;
	case u8'r':
		return ::fast_io::arithmetic_char_literal_v<u8'r', char_type>;
	case u8's':
		return ::fast_io::arithmetic_char_literal_v<u8's', char_type>;
	case u8't':
		return ::fast_io::arithmetic_char_literal_v<u8't', char_type>;
	case u8'u':
		return ::fast_io::arithmetic_char_literal_v<u8'u', char_type>;
	case u8'v':
		return ::fast_io::arithmetic_char_literal_v<u8'v', char_type>;
	case u8'w':
		return ::fast_io::arithmetic_char_literal_v<u8'w', char_type>;
	case u8'x':
		return ::fast_io::arithmetic_char_literal_v<u8'x', char_type>;
	case u8'y':
		return ::fast_io::arithmetic_char_literal_v<u8'y', char_type>;
	case u8'z':
		return ::fast_io::arithmetic_char_literal_v<u8'z', char_type>;
	case u8' ':
		return ::fast_io::arithmetic_char_literal_v<u8' ', char_type>;
	case u8'-':
		return ::fast_io::arithmetic_char_literal_v<u8'-', char_type>;
	case u8'+':
		return ::fast_io::arithmetic_char_literal_v<u8'+', char_type>;
	case u8':':
		return ::fast_io::arithmetic_char_literal_v<u8':', char_type>;
	case u8'/':
		return ::fast_io::arithmetic_char_literal_v<u8'/', char_type>;
	case u8'[':
		return ::fast_io::arithmetic_char_literal_v<u8'[', char_type>;
	case u8']':
		return ::fast_io::arithmetic_char_literal_v<u8']', char_type>;
	case u8'.':
		return ::fast_io::arithmetic_char_literal_v<u8'.', char_type>;
	case u8'%':
		return ::fast_io::arithmetic_char_literal_v<u8'%', char_type>;
	case u8'\n':
		return ::fast_io::arithmetic_char_literal_v<u8'\n', char_type>;
	case u8'\t':
		return ::fast_io::arithmetic_char_literal_v<u8'\t', char_type>;
	default:
		return char_type{};
	}
}

template <::fast_io::fmt::format_character char_type, ::std::size_t extent>
inline constexpr char_type *write_chrono_ascii(
	char_type *output, char8_t const (&text)[extent]) noexcept
{
	for (::std::size_t index{}; index + 1u != extent; ++index)
	{
		*output++ = chrono_basic_latin<char_type>(text[index]);
	}
	return output;
}

template <::std::size_t extent>
inline constexpr ::std::size_t chrono_ascii_size(char8_t const (&)[extent]) noexcept
{
	return extent - 1u;
}

[[nodiscard]] inline constexpr ::std::int64_t chrono_floor_divide(
	::std::int64_t numerator, ::std::int64_t denominator) noexcept
{
	auto quotient{numerator / denominator};
	auto const remainder{numerator % denominator};
	if (remainder != 0 && ((remainder < 0) != (denominator < 0)))
	{
		--quotient;
	}
	return quotient;
}

[[nodiscard]] inline constexpr unsigned chrono_positive_modulo(
	::std::int64_t value, unsigned modulus) noexcept
{
	auto result{value % static_cast<::std::int64_t>(modulus)};
	if (result < 0)
	{
		result += modulus;
	}
	return static_cast<unsigned>(result);
}

/**
 * Validate one already adapted calendar field without normalizing any of its
 * neighbours.
 *
 * The C calendar structure is a tuple of independently observable fields.  In
 * particular, `%a` is specified by `tm_wday`, not by recomputing a weekday from
 * `tm_year/tm_mon/tm_mday`.  Keeping these checks separate is therefore more
 * than a performance detail: it lets a deliberately partial `tm` format `%j`
 * or `%H` without requiring an unrelated, normalized civil date.
 */
[[nodiscard]] inline constexpr ::std::int_least64_t chrono_tm_year(
	chrono_calendar_fields const &value) noexcept
{
	return value.year;
}

[[nodiscard]] inline constexpr unsigned chrono_tm_month(
	chrono_calendar_fields const &value) noexcept
{
	if (value.month < 1u || value.month > 12u)
	{
		::fast_io::fast_terminate();
	}
	return value.month;
}

[[nodiscard]] inline constexpr unsigned chrono_tm_month_day(
	chrono_calendar_fields const &value) noexcept
{
	if (value.month_day < 1u || value.month_day > 31u)
	{
		::fast_io::fast_terminate();
	}
	return value.month_day;
}

[[nodiscard]] inline constexpr unsigned chrono_tm_weekday(
	chrono_calendar_fields const &value) noexcept
{
	if (value.weekday > 6u)
	{
		::fast_io::fast_terminate();
	}
	return value.weekday;
}

[[nodiscard]] inline constexpr unsigned chrono_tm_year_day(
	chrono_calendar_fields const &value) noexcept
{
	if (value.year_day > 365u)
	{
		::fast_io::fast_terminate();
	}
	return value.year_day;
}

[[nodiscard]] inline constexpr unsigned chrono_tm_hour(
	chrono_calendar_fields const &value) noexcept
{
	if (value.hour > 23u)
	{
		::fast_io::fast_terminate();
	}
	return value.hour;
}

[[nodiscard]] inline constexpr unsigned chrono_tm_minute(
	chrono_calendar_fields const &value) noexcept
{
	if (value.minute > 59u)
	{
		::fast_io::fast_terminate();
	}
	return value.minute;
}

[[nodiscard]] inline constexpr unsigned chrono_tm_second(
	chrono_calendar_fields const &value) noexcept
{
	// C and POSIX reserve 60 for a positive leap second.
	if (value.second > 60u)
	{
		::fast_io::fast_terminate();
	}
	return value.second;
}

[[nodiscard]] inline constexpr bool chrono_is_gregorian_leap_year(
	::std::int_least64_t year) noexcept
{
	return (year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0);
}

[[nodiscard]] inline constexpr unsigned chrono_iso_weeks_in_year(
	::std::int_least64_t year, unsigned january_first_iso_weekday) noexcept
{
	// An ISO year has week 53 exactly when January 1 is Thursday, or when a
	// leap year starts on Wednesday.
	return january_first_iso_weekday == 4u ||
				   (january_first_iso_weekday == 3u &&
					chrono_is_gregorian_leap_year(year))
			   ? 53u
			   : 52u;
}

[[nodiscard]] inline constexpr chrono_iso_week_fields chrono_make_iso_week_fields(
	chrono_calendar_fields const &value) noexcept
{
	auto const year{chrono_tm_year(value)};
	auto const year_day{chrono_tm_year_day(value)};
	auto const weekday{chrono_tm_weekday(value)};
	auto const days_in_year{chrono_is_gregorian_leap_year(year) ? 366u : 365u};
	if (year_day >= days_in_year)
	{
		::fast_io::fast_terminate();
	}

	auto const iso_weekday{weekday == 0u ? 7u : weekday};
	// Moving backwards `tm_yday` days from the supplied weekday proves the
	// weekday of January 1 without consulting tm_mon or tm_mday.
	auto const january_first_sunday_weekday{chrono_positive_modulo(
		static_cast<::std::int64_t>(weekday) -
			static_cast<::std::int64_t>(year_day % 7u),
		7u)};
	auto const january_first_iso_weekday{
		january_first_sunday_weekday == 0u ? 7u : january_first_sunday_weekday};
	auto const weeks_in_year{
		chrono_iso_weeks_in_year(year, january_first_iso_weekday)};

	// For a zero-based ordinal day d and ISO weekday w, ISO 8601's Thursday
	// rule reduces to floor((d + 11 - w) / 7).  (The customary `+ 10`
	// formula uses a one-based ordinal day.)  The result can only cross the
	// adjacent ISO year at the two branches below.
	auto const candidate_week{static_cast<unsigned>(
		(static_cast<int>(year_day) + 11 - static_cast<int>(iso_weekday)) / 7)};
	if (candidate_week == 0u)
	{
		if (year == (::std::numeric_limits<::std::int_least64_t>::min)())
		{
			::fast_io::fast_terminate();
		}
		auto const previous_year{year - 1};
		auto const previous_year_shift{
			chrono_is_gregorian_leap_year(previous_year) ? 2u : 1u};
		auto const previous_january_first_sunday_weekday{
			chrono_positive_modulo(
				static_cast<::std::int64_t>(january_first_sunday_weekday) -
					static_cast<::std::int64_t>(previous_year_shift),
				7u)};
		auto const previous_january_first_iso_weekday{
			previous_january_first_sunday_weekday == 0u
				? 7u
				: previous_january_first_sunday_weekday};
		return {previous_year, chrono_iso_weeks_in_year(
								   previous_year, previous_january_first_iso_weekday)};
	}
	if (candidate_week > weeks_in_year)
	{
		if (year == (::std::numeric_limits<::std::int_least64_t>::max)())
		{
			::fast_io::fast_terminate();
		}
		return {year + 1, 1u};
	}
	return {year, candidate_week};
}

template <typename unsigned_type>
[[nodiscard]] inline constexpr ::std::size_t chrono_decimal_digits(unsigned_type value) noexcept
{
	::std::size_t digits{1u};
	while (value >= static_cast<unsigned_type>(10u))
	{
		value /= static_cast<unsigned_type>(10u);
		++digits;
	}
	return digits;
}

template <typename signed_type>
[[nodiscard]] inline constexpr ::std::make_unsigned_t<signed_type>
chrono_unsigned_magnitude(signed_type value) noexcept
{
	if constexpr (::std::is_signed_v<signed_type>)
	{
		using unsigned_type = ::std::make_unsigned_t<signed_type>;
		auto const converted{static_cast<unsigned_type>(value)};
		return value < 0 ? static_cast<unsigned_type>(0u - converted) : converted;
	}
	else
	{
		return value;
	}
}

template <::fast_io::fmt::format_character char_type, typename unsigned_type>
inline constexpr void write_chrono_integer_constant(
	char_type *output, ::std::size_t size, unsigned_type magnitude,
	bool negative) noexcept
{
	auto *cursor{output + size};
	do
	{
		*--cursor = ::fast_io::char_literal_add<char_type>(
			static_cast<unsigned>(magnitude % 10u));
		magnitude /= 10u;
	} while (magnitude != 0u);
	if (negative)
	{
		*output = chrono_basic_latin<char_type>(u8'-');
	}
}

template <::fast_io::fmt::format_character char_type, typename integer_type>
inline constexpr char_type *write_chrono_integer(
	char_type *output, integer_type value) noexcept
{
	// The ordinary integral reserve writer is allowed to use a wider final store
	// inside its fixed maximum-capacity contract.  A time field, however, first
	// computes its exact dynamic size and may place a one-digit component at the
	// very end of that allocation.  Both paths below write exactly the measured
	// character count: constant evaluation uses the scalar loop (which remains
	// portable across compiler evaluators), while runtime retains the optimized
	// precise-length integral writer.
	auto const magnitude{chrono_unsigned_magnitude(value)};
	bool const negative{::std::is_signed_v<integer_type> && value < 0};
	auto const size{chrono_decimal_digits(magnitude) +
					static_cast<::std::size_t>(negative)};
	if (::std::is_constant_evaluated())
	{
		write_chrono_integer_constant(output, size, magnitude, negative);
	}
	else
	{
		::fast_io::details::print_reserve_integral_define_precise<10u>(
			output, size, value);
	}
	return output + size;
}

template <typename integer_type>
[[nodiscard]] inline constexpr ::std::size_t chrono_integer_size(integer_type value) noexcept
{
	auto const magnitude{chrono_unsigned_magnitude(value)};
	return chrono_decimal_digits(magnitude) +
		   static_cast<::std::size_t>(::std::is_signed_v<integer_type> && value < 0);
}

template <::fast_io::fmt::format_character char_type, typename integer_type>
inline constexpr char_type *write_chrono_padded_integer(
	char_type *output, integer_type value, unsigned width, chrono_padding padding) noexcept
{
	auto const magnitude{chrono_unsigned_magnitude(value)};
	auto const digits{chrono_decimal_digits(magnitude)};
	bool const negative{::std::is_signed_v<integer_type> && value < 0};
	if (negative)
	{
		*output++ = chrono_basic_latin<char_type>(u8'-');
	}
	if (padding != chrono_padding::none && digits < width)
	{
		auto const fill{padding == chrono_padding::space ? u8' ' : u8'0'};
		for (auto count{width - static_cast<unsigned>(digits)}; count != 0u; --count)
		{
			*output++ = chrono_basic_latin<char_type>(fill);
		}
	}
	return write_chrono_integer(output, magnitude);
}

template <typename integer_type>
[[nodiscard]] inline constexpr ::std::size_t chrono_padded_integer_size(
	integer_type value, unsigned width, chrono_padding padding) noexcept
{
	auto const digits{chrono_decimal_digits(chrono_unsigned_magnitude(value))};
	return static_cast<::std::size_t>(::std::is_signed_v<integer_type> && value < 0) +
		   (padding == chrono_padding::none || digits >= width ? digits : width);
}

} // namespace fast_io::fmt::details

namespace fast_io::manipulators
{

/// @brief Carries a normalized chrono value for one compile-time brace-format time field.
/// @details `format_literal` and `specification` select the exact chrono presentation as type properties, while the
///          object stores only normalized value state. Printing therefore emits the requested calendar/time spelling
///          without retaining a run-time format pointer or token-program address in the manipulator.
template <::fast_io::fmt::basic_fixed_string format_literal,
		  ::fast_io::fmt::details::source_slice specification,
		  typename storage_type>
struct basic_chrono_field_t
{
	using manip_tag = ::fast_io::manip_tag_t;
	using value_type = storage_type;

	storage_type value;
};

} // namespace fast_io::manipulators

namespace fast_io::fmt::details
{

template <typename storage_type>
using chrono_runtime_state_t = ::std::remove_cvref_t<storage_type>;

template <typename storage_type>
[[nodiscard]] consteval bool chrono_has_utc_offset() noexcept
{
	if constexpr (requires {
					  ::std::remove_cvref_t<storage_type>::has_utc_offset;
				  })
	{
		return ::std::remove_cvref_t<storage_type>::has_utc_offset;
	}
	else
	{
		return false;
	}
}

template <typename storage_type>
inline constexpr bool chrono_has_utc_offset_v{
	chrono_has_utc_offset<storage_type>()};

template <typename storage_type>
[[nodiscard]] consteval bool chrono_has_time_zone_name() noexcept
{
	if constexpr (requires {
					  ::std::remove_cvref_t<storage_type>::has_time_zone_name;
				  })
	{
		return ::std::remove_cvref_t<storage_type>::has_time_zone_name;
	}
	else
	{
		return false;
	}
}

template <typename storage_type>
inline constexpr bool chrono_has_time_zone_name_v{
	chrono_has_time_zone_name<storage_type>()};

template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification, typename storage_type,
		  ::std::size_t operation_index, ::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr ::std::size_t chrono_operation_capacity(
	::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, storage_type> const &,
	chrono_runtime_state_t<storage_type> &state)
{
	constexpr auto const &program{
		checked_chrono_program<format_literal, specification,
							   chrono_has_utc_offset_v<storage_type>,
							   chrono_has_time_zone_name_v<storage_type>>};
	constexpr auto operation{program.operations[operation_index]};
	if constexpr (operation.opcode == chrono_opcode::literal)
	{
		return operation.literal.size;
	}
	else if constexpr (operation.opcode == chrono_opcode::percent ||
					   operation.opcode == chrono_opcode::newline ||
					   operation.opcode == chrono_opcode::tab)
	{
		return 1u;
	}
	else
	{
		return chrono_calendar_operation_capacity<operation.opcode,
												  operation.padding>(state);
	}
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification, typename storage_type,
		  ::std::size_t operation_index, ::fast_io::fmt::format_character char_type>
inline constexpr char_type *emit_chrono_operation(
	char_type *output,
	::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, storage_type> const &,
	chrono_runtime_state_t<storage_type> &state)
{
	constexpr auto const &program{
		checked_chrono_program<format_literal, specification,
							   chrono_has_utc_offset_v<storage_type>,
							   chrono_has_time_zone_name_v<storage_type>>};
	constexpr auto operation{program.operations[operation_index]};
	if constexpr (operation.opcode == chrono_opcode::literal)
	{
		for (::std::size_t index{}; index != operation.literal.size; ++index)
		{
			*output++ = format_literal[operation.literal.offset + index];
		}
		return output;
	}
	else if constexpr (operation.opcode == chrono_opcode::percent)
	{
		*output++ = chrono_basic_latin<char_type>(u8'%');
		return output;
	}
	else if constexpr (operation.opcode == chrono_opcode::newline)
	{
		*output++ = chrono_basic_latin<char_type>(u8'\n');
		return output;
	}
	else if constexpr (operation.opcode == chrono_opcode::tab)
	{
		*output++ = chrono_basic_latin<char_type>(u8'\t');
		return output;
	}
	else
	{
		return emit_chrono_calendar_operation<operation.opcode,
											  operation.padding>(output, state);
	}
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification, typename storage_type,
		  ::fast_io::fmt::format_character char_type, ::std::size_t... operation_index>
[[nodiscard]] inline constexpr ::std::size_t chrono_program_capacity_impl(
	::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, storage_type> const &field,
	::std::index_sequence<operation_index...>)
{
	auto state{field.value};
	::std::size_t result{};
	((result += chrono_operation_capacity<format_literal, specification,
										  storage_type, operation_index, char_type>(field, state)),
	 ...);
	return result;
}

template <::fast_io::fmt::basic_fixed_string format_literal,
		  source_slice specification, typename storage_type,
		  ::fast_io::fmt::format_character char_type, ::std::size_t... operation_index>
inline constexpr char_type *emit_chrono_program_impl(
	char_type *output,
	::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, storage_type> const &field,
	::std::index_sequence<operation_index...>)
{
	auto state{field.value};
	((output = emit_chrono_operation<format_literal, specification,
									 storage_type, operation_index>(output, field, state)),
	 ...);
	return output;
}

} // namespace fast_io::fmt::details

namespace fast_io
{

template <::std::integral char_type,
		  ::fast_io::fmt::basic_fixed_string format_literal,
		  ::fast_io::fmt::details::source_slice specification,
		  typename storage_type>
	requires ::std::same_as<char_type,
							typename decltype(format_literal)::value_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type,
								 ::fast_io::manipulators::basic_chrono_field_t<
									 format_literal, specification, storage_type>>,
	::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, storage_type> const &field)
{
	constexpr auto operation_count{
		::fast_io::fmt::details::checked_chrono_program<
			format_literal, specification,
			::fast_io::fmt::details::chrono_has_utc_offset_v<storage_type>,
			::fast_io::fmt::details::chrono_has_time_zone_name_v<storage_type>>
			.operation_count};
	return ::fast_io::fmt::details::chrono_program_capacity_impl<
		format_literal, specification, storage_type, char_type>(
		field, ::std::make_index_sequence<operation_count>{});
}

template <::std::integral char_type,
		  ::fast_io::fmt::basic_fixed_string format_literal,
		  ::fast_io::fmt::details::source_slice specification,
		  typename storage_type>
	requires ::std::same_as<char_type,
							typename decltype(format_literal)::value_type>
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type,
								 ::fast_io::manipulators::basic_chrono_field_t<
									 format_literal, specification, storage_type>>,
	char_type *output,
	::fast_io::manipulators::basic_chrono_field_t<
		format_literal, specification, storage_type> const &field)
{
	constexpr auto operation_count{
		::fast_io::fmt::details::checked_chrono_program<
			format_literal, specification,
			::fast_io::fmt::details::chrono_has_utc_offset_v<storage_type>,
			::fast_io::fmt::details::chrono_has_time_zone_name_v<storage_type>>
			.operation_count};
	return ::fast_io::fmt::details::emit_chrono_program_impl<
		format_literal, specification, storage_type>(
		output, field, ::std::make_index_sequence<operation_count>{});
}

} // namespace fast_io

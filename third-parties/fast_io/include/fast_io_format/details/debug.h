#pragma once

/*
 * Debug-presentation adapter (FMT level).
 *
 * Character, string, and textual range values are translated into escaped,
 * quoted printable representations according to format-language debug rules.
 * Measurement and rendering helpers expose ordinary fast_io reserve/context
 * protocols so the shared IO planner can materialize them. This file defines
 * presentation semantics only and never selects or writes to a device.
 */

#include "unicode.h"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace fast_io::fmt::details
{

enum class debug_text_kind : unsigned char
{
	string,
	character
};

enum class debug_escape_kind : unsigned char
{
	copy,
	short_escape,
	hexadecimal_2,
	hexadecimal_4,
	hexadecimal_8
};

struct debug_scalar_rendering
{
	debug_escape_kind kind{debug_escape_kind::copy};
	char8_t short_escape{};
	::std::uint_least32_t value{};
	::std::size_t source_units{1u};
	::std::size_t storage_size{1u};
	::std::size_t display_width{1u};
};

/**
 * A compact, version-stable printable predicate for debug formatting.
 *
 * fmt generates a Unicode-version-specific table which also escapes every
 * currently unassigned scalar.  Embedding that table in this strict header-only
 * frontend would make every translation unit parse roughly a kilobyte of table
 * data and would change output when the generated Unicode version changes.
 * fast_io instead rejects the stable General_Category C/Z exclusions below:
 * controls, format controls, surrogates, private-use scalars, separators, and
 * noncharacters.  Assigned graphic scalars pass through unchanged.  Invalid
 * encodings are handled independently and always escaped.
 */
[[nodiscard]] inline constexpr bool unicode_debug_printable(char32_t code_point) noexcept
{
	auto const cp{static_cast<::std::uint_least32_t>(code_point)};
	if (!unicode_scalar_value(cp) || cp < 0x20u || (0x7fu <= cp && cp <= 0x9fu))
	{
		return false;
	}
	// Unicode space/line/paragraph separators other than ordinary U+0020.
	if (cp == 0x00a0u || cp == 0x1680u || (0x2000u <= cp && cp <= 0x200au) ||
		cp == 0x2028u || cp == 0x2029u || cp == 0x202fu || cp == 0x205fu ||
		cp == 0x3000u)
	{
		return false;
	}
	// Stable format-control ranges from General_Category Cf.
	if (cp == 0x00adu || (0x0600u <= cp && cp <= 0x0605u) || cp == 0x061cu ||
		cp == 0x06ddu || cp == 0x070fu || (0x0890u <= cp && cp <= 0x0891u) ||
		cp == 0x08e2u || cp == 0x180eu || (0x200bu <= cp && cp <= 0x200fu) ||
		(0x202au <= cp && cp <= 0x202eu) || (0x2060u <= cp && cp <= 0x2064u) ||
		(0x2066u <= cp && cp <= 0x206fu) || cp == 0xfeffu ||
		(0xfff9u <= cp && cp <= 0xfffbu) || cp == 0x110bdu || cp == 0x110cdu ||
		(0x13430u <= cp && cp <= 0x1343fu) ||
		(0x1bca0u <= cp && cp <= 0x1bcafu) ||
		(0x1d173u <= cp && cp <= 0x1d17au) || cp == 0xe0001u ||
		(0xe0020u <= cp && cp <= 0xe007fu))
	{
		return false;
	}
	// Private-use ranges and Unicode noncharacters are never graphic output.
	if ((0xe000u <= cp && cp <= 0xf8ffu) ||
		(0xf0000u <= cp && cp <= 0xffffdu) ||
		(0x100000u <= cp && cp <= 0x10fffdu) ||
		(0xfdd0u <= cp && cp <= 0xfdefu) || (cp & 0xffffu) >= 0xfffeu)
	{
		return false;
	}
	return true;
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr bool debug_character_equals(
	char_type source, char8_t expected) noexcept
{
	switch (expected)
	{
	case u8'\n':
		return source == ::fast_io::char_literal_v<u8'\n', char_type>;
	case u8'\r':
		return source == ::fast_io::char_literal_v<u8'\r', char_type>;
	case u8'\t':
		return source == ::fast_io::char_literal_v<u8'\t', char_type>;
	case u8'"':
		return source == ::fast_io::char_literal_v<u8'"', char_type>;
	case u8'\'':
		return source == ::fast_io::char_literal_v<u8'\'', char_type>;
	case u8'\\':
		return source == ::fast_io::char_literal_v<u8'\\', char_type>;
	default:
		return false;
	}
}

template <debug_text_kind text_kind, ::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr debug_scalar_rendering classify_debug_scalar(
	char_type const *source, ::std::size_t remaining) noexcept
{
	auto const decoded{decode_unicode_scalar(source, remaining)};
	auto const raw_value{format_unicode_code_unit_value(*source)};
	if (debug_character_equals(*source, u8'\n'))
	{
		return {debug_escape_kind::short_escape, u8'n', raw_value,
				decoded.code_units, 2u, 2u};
	}
	if (debug_character_equals(*source, u8'\r'))
	{
		return {debug_escape_kind::short_escape, u8'r', raw_value,
				decoded.code_units, 2u, 2u};
	}
	if (debug_character_equals(*source, u8'\t'))
	{
		return {debug_escape_kind::short_escape, u8't', raw_value,
				decoded.code_units, 2u, 2u};
	}

	bool const quote{
		text_kind == debug_text_kind::string
			? debug_character_equals(*source, u8'"')
			: debug_character_equals(*source, u8'\'')};
	if (quote || debug_character_equals(*source, u8'\\'))
	{
		return {debug_escape_kind::short_escape,
				quote ? (text_kind == debug_text_kind::string ? u8'"' : u8'\'') : u8'\\',
				raw_value, decoded.code_units, 2u, 2u};
	}

	bool needs_escape{};
	if constexpr (!::fast_io::details::is_unicode_execution_charset<char_type>)
	{
		// Preserve the one-code-unit policy used by width measurement for SBCS.
		needs_escape = ::fast_io::char_category::is_c_cntrl(*source);
	}
	else
	{
		needs_escape = !decoded.valid || !unicode_debug_printable(decoded.code_point);
	}
	if (!needs_escape)
	{
		return {debug_escape_kind::copy, {}, raw_value, decoded.code_units, decoded.code_units, decoded.valid ? estimated_unicode_display_width(decoded.code_point) : 1u};
	}

	auto const escaped_value{decoded.valid
								 ? static_cast<::std::uint_least32_t>(decoded.code_point)
								 : raw_value};
	if (escaped_value < 0x100u)
	{
		return {debug_escape_kind::hexadecimal_2, {}, escaped_value, decoded.code_units, 4u, 4u};
	}
	if (escaped_value < 0x10000u)
	{
		return {debug_escape_kind::hexadecimal_4, {}, escaped_value, decoded.code_units, 6u, 6u};
	}
	if (escaped_value < 0x110000u)
	{
		return {debug_escape_kind::hexadecimal_8, {}, escaped_value, decoded.code_units, 10u, 10u};
	}
	// This case is reachable only for an invalid 32-bit code unit.  Matching
	// fmt's recovery, expose its low byte rather than manufacturing a scalar.
	return {debug_escape_kind::hexadecimal_2, {}, escaped_value & 0xffu, decoded.code_units, 4u, 4u};
}

/** Returns the initial printable ASCII run which debug emission may copy verbatim. */
template <debug_text_kind text_kind, ::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr ::std::size_t debug_ascii_copy_run(
	char_type const *source, ::std::size_t size) noexcept
{
	if constexpr (!::fast_io::details::is_unicode_execution_charset<char_type>)
	{
		return 0u;
	}
	else
	{
		::std::size_t count{};
		for (; count != size; ++count)
		{
			auto const value{format_unicode_code_unit_value(source[count])};
			if (value < 0x20u || 0x7eu < value || value == 0x5cu ||
				value == (text_kind == debug_text_kind::string ? 0x22u : 0x27u))
			{
				break;
			}
		}
		return count;
	}
}

template <::fast_io::fmt::format_character char_type>
inline constexpr char_type debug_hex_digits[16u]{
	::fast_io::char_literal_v<u8'0', char_type>,
	::fast_io::char_literal_v<u8'1', char_type>,
	::fast_io::char_literal_v<u8'2', char_type>,
	::fast_io::char_literal_v<u8'3', char_type>,
	::fast_io::char_literal_v<u8'4', char_type>,
	::fast_io::char_literal_v<u8'5', char_type>,
	::fast_io::char_literal_v<u8'6', char_type>,
	::fast_io::char_literal_v<u8'7', char_type>,
	::fast_io::char_literal_v<u8'8', char_type>,
	::fast_io::char_literal_v<u8'9', char_type>,
	::fast_io::char_literal_v<u8'a', char_type>,
	::fast_io::char_literal_v<u8'b', char_type>,
	::fast_io::char_literal_v<u8'c', char_type>,
	::fast_io::char_literal_v<u8'd', char_type>,
	::fast_io::char_literal_v<u8'e', char_type>,
	::fast_io::char_literal_v<u8'f', char_type>};

template <::fast_io::fmt::format_character char_type>
inline constexpr char_type debug_ascii_character(char8_t value) noexcept
{
	switch (value)
	{
	case u8'n':
		return ::fast_io::char_literal_v<u8'n', char_type>;
	case u8'r':
		return ::fast_io::char_literal_v<u8'r', char_type>;
	case u8't':
		return ::fast_io::char_literal_v<u8't', char_type>;
	case u8'x':
		return ::fast_io::char_literal_v<u8'x', char_type>;
	case u8'u':
		return ::fast_io::char_literal_v<u8'u', char_type>;
	case u8'U':
		return ::fast_io::char_literal_v<u8'U', char_type>;
	case u8'"':
		return ::fast_io::char_literal_v<u8'"', char_type>;
	case u8'\'':
		return ::fast_io::char_literal_v<u8'\'', char_type>;
	case u8'\\':
		return ::fast_io::char_literal_v<u8'\\', char_type>;
	default:
		return ::fast_io::char_literal_v<u8'?', char_type>;
	}
}

template <::fast_io::fmt::format_character char_type>
inline constexpr char_type *emit_debug_scalar(char_type *output,
											  char_type const *source, debug_scalar_rendering rendering,
											  ::std::size_t maximum_storage) noexcept
{
	if (rendering.kind == debug_escape_kind::copy)
	{
		auto const amount{rendering.source_units < maximum_storage
							  ? rendering.source_units
							  : maximum_storage};
		for (::std::size_t index{}; index != amount; ++index)
		{
			*output++ = source[index];
		}
		return output;
	}

	char_type buffer[10u]{};
	auto iterator{buffer};
	*iterator++ = ::fast_io::char_literal_v<u8'\\', char_type>;
	if (rendering.kind == debug_escape_kind::short_escape)
	{
		*iterator++ = debug_ascii_character<char_type>(rendering.short_escape);
	}
	else
	{
		::std::size_t digits{};
		char8_t prefix{};
		if (rendering.kind == debug_escape_kind::hexadecimal_2)
		{
			digits = 2u;
			prefix = u8'x';
		}
		else if (rendering.kind == debug_escape_kind::hexadecimal_4)
		{
			digits = 4u;
			prefix = u8'u';
		}
		else
		{
			digits = 8u;
			prefix = u8'U';
		}
		*iterator++ = debug_ascii_character<char_type>(prefix);
		for (::std::size_t shift{digits * 4u}; shift != 0u;)
		{
			shift -= 4u;
			*iterator++ = debug_hex_digits<char_type>[(rendering.value >> shift) & 0x0fu];
		}
	}
	auto const generated{static_cast<::std::size_t>(iterator - buffer)};
	auto const amount{generated < maximum_storage ? generated : maximum_storage};
	for (::std::size_t index{}; index != amount; ++index)
	{
		*output++ = buffer[index];
	}
	return output;
}

struct debug_text_payload_measurement
{
	::std::size_t storage_size{};
	::std::size_t display_width{};
};

template <debug_text_kind text_kind, ::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr debug_text_payload_measurement
measure_debug_text_payload(char_type const *data, ::std::size_t size,
						   ::std::size_t maximum_display_width) noexcept
{
	debug_text_payload_measurement result{};
	if (maximum_display_width == 0u)
	{
		return result;
	}
	// Both debug forms include their opening quote in precision and width.
	result.storage_size = 1u;
	result.display_width = 1u;
	::std::size_t consumed{};
	while (consumed != size && result.display_width != maximum_display_width)
	{
		auto const remaining{maximum_display_width - result.display_width};
#if defined(__clang__)
		auto const ascii_run{debug_ascii_copy_run<text_kind>(
			data + consumed, size - consumed)};
		if (ascii_run != 0u)
		{
			auto const amount{ascii_run < remaining ? ascii_run : remaining};
			result.storage_size += amount;
			result.display_width += amount;
			consumed += amount;
			if (amount != ascii_run)
			{
				break;
			}
			continue;
		}
#endif
		auto const rendering{classify_debug_scalar<text_kind>(data + consumed, size - consumed)};
		if (rendering.display_width <= remaining)
		{
			result.storage_size += rendering.storage_size;
			result.display_width += rendering.display_width;
			consumed += rendering.source_units;
			continue;
		}
		if (rendering.kind != debug_escape_kind::copy)
		{
			// fmt intentionally permits a precision boundary inside an ASCII
			// escape.  Recording the partial storage makes emission reproduce
			// that behavior without ever splitting an original UTF scalar.
			result.storage_size += remaining;
			result.display_width += remaining;
		}
		break;
	}
	if (consumed == size && result.display_width < maximum_display_width)
	{
		++result.storage_size;
		++result.display_width;
	}
	return result;
}

template <::fast_io::fmt::format_character char_type>
struct basic_debug_string_field
{
	using manip_tag = ::fast_io::manip_tag_t;
	::fast_io::basic_io_scatter_t<char_type> source{};
	basic_text_field_options<char_type> options{};
};

/// Models the unconditional default debug spelling used by range elements.
///
/// Keeping this semantic promise in the type removes run-time width/precision policy from the leaf and gives every
/// consumer the same branch-free one-pass define.  The type describes formatting only; destination strategy remains
/// owned by the core print/concat bounded-materialization protocol.
template <::fast_io::fmt::format_character char_type>
struct basic_default_debug_string_field
{
	using manip_tag = ::fast_io::manip_tag_t;
	::fast_io::basic_io_scatter_t<char_type> source{};
};

template <::fast_io::fmt::format_character char_type>
struct basic_debug_character_field
{
	using manip_tag = ::fast_io::manip_tag_t;
	char_type value{};
	basic_text_field_options<char_type> options{};
};

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr basic_debug_string_field<char_type>
make_debug_string_field(::fast_io::basic_io_scatter_t<char_type> source,
						basic_text_field_options<char_type> options) noexcept
{
	return {source, options};
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr basic_default_debug_string_field<char_type>
make_default_debug_string_field(
	::fast_io::basic_io_scatter_t<char_type> source) noexcept
{
	return {source};
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr basic_debug_character_field<char_type>
make_debug_character_field(char_type value,
						   basic_text_field_options<char_type> options) noexcept
{
	// Brace character formatting has no precision.  Resetting the value here
	// prevents an accidentally reused string policy from truncating an escape.
	options.maximum_display_width = SIZE_MAX;
	return {value, options};
}

struct debug_text_field_measurement
{
	debug_text_payload_measurement payload{};
	format_padding_measurement padding{};
	::std::size_t storage_size{};
};

template <debug_text_kind text_kind, ::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr debug_text_field_measurement measure_debug_text_field(
	char_type const *data, ::std::size_t size,
	basic_text_field_options<char_type> const &options) noexcept
{
	auto const payload{measure_debug_text_payload<text_kind>(
		data, size, options.maximum_display_width)};
	auto const padding{measure_format_padding(payload.display_width,
											  options.minimum_width, options.placement)};
	auto const repetitions{padding.left + padding.right};
	return {payload, padding,
			payload.storage_size + repetitions * options.fill_size};
}

template <debug_text_kind text_kind, ::fast_io::fmt::format_character char_type>
inline constexpr char_type *emit_debug_text_payload(char_type *output,
													char_type const *data, ::std::size_t size, ::std::size_t storage_size) noexcept
{
	if (storage_size == 0u)
	{
		return output;
	}
	*output++ = text_kind == debug_text_kind::string
					? ::fast_io::char_literal_v<u8'"', char_type>
					: ::fast_io::char_literal_v<u8'\'', char_type>;
	--storage_size;
	::std::size_t consumed{};
	while (consumed != size && storage_size != 0u)
	{
#if defined(__clang__)
		auto const ascii_run{debug_ascii_copy_run<text_kind>(
			data + consumed, size - consumed)};
		if (ascii_run != 0u)
		{
			auto const amount{ascii_run < storage_size ? ascii_run : storage_size};
			output = ::fast_io::details::non_overlapped_copy_n(
				data + consumed, amount, output);
			storage_size -= amount;
			if (amount != ascii_run)
			{
				return output;
			}
			consumed += amount;
			continue;
		}
#endif
		auto const rendering{classify_debug_scalar<text_kind>(data + consumed, size - consumed)};
		auto const amount{rendering.storage_size < storage_size
							  ? rendering.storage_size
							  : storage_size};
		output = emit_debug_scalar(output, data + consumed, rendering, amount);
		storage_size -= amount;
		if (amount != rendering.storage_size)
		{
			return output;
		}
		consumed += rendering.source_units;
	}
	if (storage_size != 0u)
	{
		*output++ = text_kind == debug_text_kind::string
						? ::fast_io::char_literal_v<u8'"', char_type>
						: ::fast_io::char_literal_v<u8'\'', char_type>;
	}
	return output;
}

template <debug_text_kind text_kind, ::fast_io::fmt::format_character char_type>
inline constexpr char_type *emit_debug_text_field(char_type *output,
												  char_type const *data, ::std::size_t size,
												  basic_text_field_options<char_type> const &options) noexcept
{
	auto const measurement{measure_debug_text_field<text_kind>(data, size, options)};
	output = emit_format_fill(output, options, measurement.padding.left);
	output = emit_debug_text_payload<text_kind>(output, data, size,
												measurement.payload.storage_size);
	return emit_format_fill(output, options, measurement.padding.right);
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr debug_text_field_measurement
measure_debug_string_field(basic_debug_string_field<char_type> const &field) noexcept
{
	return measure_debug_text_field<debug_text_kind::string>(
		field.source.base, field.source.len, field.options);
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr debug_text_field_measurement
measure_debug_character_field(basic_debug_character_field<char_type> const &field) noexcept
{
	return measure_debug_text_field<debug_text_kind::character>(
		__builtin_addressof(field.value), 1u, field.options);
}

} // namespace fast_io::fmt::details

namespace fast_io
{

/// @brief Opts the default debug-string semantic leaf into destination-neutral bounded materialization.
/// @details Every input code unit expands to at most `\\U0010ffff` (ten output code units), and two additional
///          code units cover the quotes.  Advertising that conservative bound lets core print and concat decide how to
///          use contiguous storage without exposing any destination type or endpoint policy in the format layer.
template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::true_type
single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<output_char_type,
		::fast_io::fmt::details::basic_default_debug_string_field<source_char_type>>) noexcept
{
	return {};
}

/// @brief Authorizes core print to emit a capacity-proven default debug string directly into a deferred put area.
/// @details The companion define is non-throwing and does not publish the destination cursor itself.  Core therefore
///          owns the single final cursor commit after every component in the admitted run has completed.
template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::true_type
print_single_pass_bounded_direct_put_area_safe(
	::fast_io::io_reserve_type_t<output_char_type,
		::fast_io::fmt::details::basic_default_debug_string_field<source_char_type>>) noexcept
{
	return {};
}

/// @brief Returns a cheap non-fatal capacity bound for the default debug-string spelling.
/// @details The overflow-safe division check proves the complete conservative extent before multiplication. The const
///          source query neither advances nor consumes the view; failure writes nothing and lets the caller resume its
///          established precise strategy.
template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::size_t
single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<output_char_type,
		::fast_io::fmt::details::basic_default_debug_string_field<source_char_type>>,
	::fast_io::fmt::details::basic_default_debug_string_field<source_char_type> const &field,
	::std::size_t maximum_size) noexcept
{
	if (maximum_size < 2u ||
		field.source.len > (maximum_size - 2u) / 10u)
	{
		return SIZE_MAX;
	}
	return 2u + field.source.len * 10u;
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<output_char_type,
		::fast_io::fmt::details::basic_default_debug_string_field<source_char_type>>,
	::fast_io::fmt::details::basic_default_debug_string_field<source_char_type> field) noexcept
{
	return ::fast_io::fmt::details::measure_debug_text_payload<
		::fast_io::fmt::details::debug_text_kind::string>(
		field.source.base, field.source.len, SIZE_MAX).storage_size;
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
inline constexpr output_char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<output_char_type,
		::fast_io::fmt::details::basic_default_debug_string_field<source_char_type>>,
	output_char_type *output,
	::fast_io::fmt::details::basic_default_debug_string_field<source_char_type> field) noexcept
{
	return ::fast_io::fmt::details::emit_debug_text_payload<
		::fast_io::fmt::details::debug_text_kind::string>(
		output, field.source.base, field.source.len, SIZE_MAX);
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<output_char_type,
		::fast_io::fmt::details::basic_default_debug_string_field<source_char_type>> tag,
	::fast_io::fmt::details::basic_default_debug_string_field<source_char_type> field) noexcept
{
	return print_reserve_size(tag, field);
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
inline constexpr output_char_type *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<output_char_type,
		::fast_io::fmt::details::basic_default_debug_string_field<source_char_type>> tag,
	output_char_type *output, ::std::size_t,
	::fast_io::fmt::details::basic_default_debug_string_field<source_char_type> field) noexcept
{
	return print_reserve_define(tag, output, field);
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::true_type
print_precise_resize_initialization_sensitive(
	::fast_io::io_reserve_type_t<output_char_type,
		::fast_io::fmt::details::basic_default_debug_string_field<source_char_type>>) noexcept
{
	return {};
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_debug_string_field<source_char_type>>,
	::fast_io::fmt::details::basic_debug_string_field<source_char_type> field) noexcept
{
	return ::fast_io::fmt::details::measure_debug_string_field(field).storage_size;
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
inline constexpr output_char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_debug_string_field<source_char_type>>,
	output_char_type *output,
	::fast_io::fmt::details::basic_debug_string_field<source_char_type> field) noexcept
{
	if (field.options.maximum_display_width == SIZE_MAX &&
		field.options.minimum_width == 0u)
	{
		// The caller has already reserved the exact measured size. Default debug
		// strings need neither padding nor truncation, so emission can classify
		// every source scalar once instead of repeating the measurement pass.
		return ::fast_io::fmt::details::emit_debug_text_payload<
			::fast_io::fmt::details::debug_text_kind::string>(
			output, field.source.base, field.source.len, SIZE_MAX);
	}
	return ::fast_io::fmt::details::emit_debug_text_field<
		::fast_io::fmt::details::debug_text_kind::string>(output,
															 field.source.base, field.source.len, field.options);
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_debug_string_field<source_char_type>>
		tag,
	::fast_io::fmt::details::basic_debug_string_field<source_char_type> field) noexcept
{
	return print_reserve_size(tag, field);
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
inline constexpr output_char_type *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_debug_string_field<source_char_type>>
		tag,
	output_char_type *output, ::std::size_t,
	::fast_io::fmt::details::basic_debug_string_field<source_char_type> field) noexcept
{
	return print_reserve_define(tag, output, field);
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::true_type
	print_precise_resize_initialization_sensitive(
		::fast_io::io_reserve_type_t<output_char_type,
									 ::fast_io::fmt::details::basic_debug_string_field<source_char_type>>) noexcept
{
	return {};
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_debug_character_field<source_char_type>>,
	::fast_io::fmt::details::basic_debug_character_field<source_char_type> field) noexcept
{
	return ::fast_io::fmt::details::measure_debug_character_field(field).storage_size;
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
inline constexpr output_char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_debug_character_field<source_char_type>>,
	output_char_type *output,
	::fast_io::fmt::details::basic_debug_character_field<source_char_type> field) noexcept
{
	return ::fast_io::fmt::details::emit_debug_text_field<
		::fast_io::fmt::details::debug_text_kind::character>(output,
																__builtin_addressof(field.value), 1u, field.options);
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_debug_character_field<source_char_type>>
		tag,
	::fast_io::fmt::details::basic_debug_character_field<source_char_type> field) noexcept
{
	return print_reserve_size(tag, field);
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
inline constexpr output_char_type *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_debug_character_field<source_char_type>>
		tag,
	output_char_type *output, ::std::size_t,
	::fast_io::fmt::details::basic_debug_character_field<source_char_type> field) noexcept
{
	return print_reserve_define(tag, output, field);
}

} // namespace fast_io

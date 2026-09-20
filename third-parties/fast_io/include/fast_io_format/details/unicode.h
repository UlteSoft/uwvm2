#pragma once

/*
 * Unicode validation and measurement support for format syntax (FMT level).
 *
 * Fill characters, precision limits, display width, escaping, and transcoded
 * literal fragments require scalar-aware traversal independent of any device.
 * These helpers provide that syntax/presentation analysis. They do not define
 * stream decorators or codecvt device CPOs and do not perform output transfer.
 */

#include "semantic.h"
#include "fixed_string.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

// The format umbrella is entered after fast_io_core restored the caller's
// macros. Re-enter the internal capability-probe scope for this header.
#include "../../fast_io_dsal/impl/misc/push_macros.h"

namespace fast_io::fmt::details
{

/**
 * The result of decoding one source character.
 *
 * `code_units` is deliberately independent of `display_width`.  UTF-8 may use
 * four storage elements for a scalar which occupies one terminal column, while
 * a CJK scalar may use one UTF-32 element and occupy two columns.  Conflating
 * these quantities either splits an encoding at a precision boundary or
 * under-allocates the final concat destination.
 */
struct decoded_unicode_scalar
{
	char32_t code_point{};
	::std::size_t code_units{1u};
	bool valid{};
};

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr ::std::uint_least32_t
format_unicode_code_unit_value(char_type value) noexcept
{
	using clean_type = ::std::remove_cv_t<char_type>;
	using unsigned_type = ::std::make_unsigned_t<clean_type>;
	auto result{static_cast<unsigned_type>(value)};
	if constexpr (::std::same_as<clean_type, wchar_t> &&
				  ::fast_io::details::wide_is_none_execution_endian)
	{
		result = ::fast_io::byte_swap(result);
	}
	return static_cast<::std::uint_least32_t>(result);
}

[[nodiscard]] inline constexpr bool unicode_scalar_value(
	::std::uint_least32_t value) noexcept
{
	return value <= 0x10ffffu && !(0xd800u <= value && value <= 0xdfffu);
}

[[nodiscard]] inline constexpr bool utf8_continuation(
	::std::uint_least32_t value) noexcept
{
	return (value & 0xc0u) == 0x80u;
}

/**
 * Decodes one scalar without reading beyond `remaining`.
 *
 * Invalid input consumes exactly one code unit.  This recovery rule is needed
 * by both truncation and debug escaping: it guarantees progress and preserves
 * every original storage element instead of silently replacing or skipping a
 * malformed suffix.
 *
 * Non-Unicode execution characters intentionally take the conservative
 * branch.  An ordinary format string value does not prove that its argument
 * uses a particular implementation-defined single-byte code page.  Treating
 * each code unit as one display column is therefore the only encoding-neutral
 * promise here.
 */
template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr decoded_unicode_scalar decode_unicode_scalar(
	char_type const *first, ::std::size_t remaining) noexcept
{
	if (remaining == 0u)
	{
		return {};
	}

	using clean_type = ::std::remove_cv_t<char_type>;
	auto const first_value{format_unicode_code_unit_value(*first)};
	if constexpr (!::fast_io::details::is_unicode_execution_charset<clean_type>)
	{
		return {static_cast<char32_t>(first_value), 1u, true};
	}
	else if constexpr (::std::same_as<clean_type, char> ||
					   ::std::same_as<clean_type, char8_t> || sizeof(clean_type) == 1u)
	{
		if (first_value <= 0x7fu)
		{
			return {static_cast<char32_t>(first_value), 1u, true};
		}
		if (0xc2u <= first_value && first_value <= 0xdfu && remaining >= 2u)
		{
			auto const second{format_unicode_code_unit_value(first[1u])};
			if (utf8_continuation(second))
			{
				return {static_cast<char32_t>(((first_value & 0x1fu) << 6u) |
											  (second & 0x3fu)),
						2u, true};
			}
		}
		else if (0xe0u <= first_value && first_value <= 0xefu && remaining >= 3u)
		{
			auto const second{format_unicode_code_unit_value(first[1u])};
			auto const third{format_unicode_code_unit_value(first[2u])};
			bool const canonical_second{
				(first_value != 0xe0u || second >= 0xa0u) &&
				(first_value != 0xedu || second <= 0x9fu)};
			if (canonical_second && utf8_continuation(second) && utf8_continuation(third))
			{
				return {static_cast<char32_t>(((first_value & 0x0fu) << 12u) |
											  ((second & 0x3fu) << 6u) | (third & 0x3fu)),
						3u, true};
			}
		}
		else if (0xf0u <= first_value && first_value <= 0xf4u && remaining >= 4u)
		{
			auto const second{format_unicode_code_unit_value(first[1u])};
			auto const third{format_unicode_code_unit_value(first[2u])};
			auto const fourth{format_unicode_code_unit_value(first[3u])};
			bool const canonical_second{
				(first_value != 0xf0u || second >= 0x90u) &&
				(first_value != 0xf4u || second <= 0x8fu)};
			if (canonical_second && utf8_continuation(second) &&
				utf8_continuation(third) && utf8_continuation(fourth))
			{
				return {static_cast<char32_t>(((first_value & 0x07u) << 18u) |
											  ((second & 0x3fu) << 12u) | ((third & 0x3fu) << 6u) |
											  (fourth & 0x3fu)),
						4u, true};
			}
		}
		return {static_cast<char32_t>(first_value), 1u, false};
	}
	else if constexpr (::std::same_as<clean_type, char16_t> || sizeof(clean_type) == 2u)
	{
		if (0xd800u <= first_value && first_value <= 0xdbffu && remaining >= 2u)
		{
			auto const second{format_unicode_code_unit_value(first[1u])};
			if (0xdc00u <= second && second <= 0xdfffu)
			{
				auto const code_point{0x10000u + ((first_value - 0xd800u) << 10u) +
									  (second - 0xdc00u)};
				return {static_cast<char32_t>(code_point), 2u, true};
			}
		}
		if (0xd800u <= first_value && first_value <= 0xdfffu)
		{
			return {static_cast<char32_t>(first_value), 1u, false};
		}
		return {static_cast<char32_t>(first_value), 1u, true};
	}
	else
	{
		return {static_cast<char32_t>(first_value), 1u,
				unicode_scalar_value(first_value)};
	}
}

/**
 * Returns the standard-library-style estimated terminal width of one scalar.
 *
 * This deliberately follows the same stable 1-or-2-column approximation used
 * by fmt and by the C++ formatting wording.  It is not a grapheme-cluster or a
 * locale-sensitive `wcwidth` implementation: those facilities would make the
 * result depend on runtime tables and terminal policy, which is inappropriate
 * for the deterministic format layer.
 */
[[nodiscard]] inline constexpr ::std::size_t estimated_unicode_display_width(
	char32_t code_point) noexcept
{
	auto const cp{static_cast<::std::uint_least32_t>(code_point)};
	return 1u + static_cast<::std::size_t>(
					cp >= 0x1100u &&
					(cp <= 0x115fu || cp == 0x2329u || cp == 0x232au ||
					 (cp >= 0x2e80u && cp <= 0xa4cfu && cp != 0x303fu) ||
					 (cp >= 0xac00u && cp <= 0xd7a3u) ||
					 (cp >= 0xf900u && cp <= 0xfaffu) ||
					 (cp >= 0xfe10u && cp <= 0xfe19u) ||
					 (cp >= 0xfe30u && cp <= 0xfe6fu) ||
					 (cp >= 0xff00u && cp <= 0xff60u) ||
					 (cp >= 0xffe0u && cp <= 0xffe6u) ||
					 (cp >= 0x20000u && cp <= 0x2fffdu) ||
					 (cp >= 0x30000u && cp <= 0x3fffdu) ||
					 (cp >= 0x1f300u && cp <= 0x1f64fu) ||
					 (cp >= 0x1f900u && cp <= 0x1f9ffu)));
}

struct unicode_prefix_measurement
{
	::std::size_t storage_size{};
	::std::size_t display_width{};
};

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr char_type const *find_ascii_run_end_scalar(
	char_type const *first, char_type const *last) noexcept
{
	using unsigned_type = ::std::make_unsigned_t<char_type>;
	while (first != last && static_cast<unsigned_type>(*first) < 0x80u)
	{
		++first;
	}
	return first;
}

/** Finds an ASCII run without imposing alignment or aliasing requirements. */
template <::fast_io::fmt::format_character char_type>
	requires(sizeof(char_type) == 1u)
[[nodiscard]] inline constexpr char_type const *find_ascii_run_end(
	char_type const *first, char_type const *last) noexcept
{
	using scan_word_type = ::std::uint_least64_t;
	if constexpr (::std::is_volatile_v<char_type> ||
				  ::std::numeric_limits<unsigned char>::digits != 8 ||
				  sizeof(scan_word_type) != 8u ||
				  ::std::numeric_limits<scan_word_type>::digits != 64)
	{
		// Volatile storage must retain one observable access per code unit.  The
		// packed mask also requires an exact 8-bit-byte, 64-value-bit load; unusual
		// character widths and integer representations retain the scalar contract.
		return find_ascii_run_end_scalar(first, last);
	}
	else
	{
#if FAST_IO_HAS_BUILTIN(__builtin_is_constant_evaluated)
		if (__builtin_is_constant_evaluated())
#else
		if (::std::is_constant_evaluated())
#endif
		{
			return find_ascii_run_end_scalar(first, last);
		}
		constexpr scan_word_type high_bits{0x8080808080808080ull};
		while (static_cast<::std::size_t>(last - first) >= sizeof(scan_word_type))
		{
			scan_word_type word{};
			::fast_io::details::my_memcpy(
				__builtin_addressof(word), first, sizeof(word));
			if ((word & high_bits) != 0u)
			{
				break;
			}
			first += sizeof(word);
		}
		return find_ascii_run_end_scalar(first, last);
	}
}

/** Finds the largest scalar-aligned prefix which fits the display-width limit. */
template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr unicode_prefix_measurement measure_unicode_prefix(
	char_type const *data, ::std::size_t size,
	::std::size_t maximum_display_width = SIZE_MAX) noexcept
{
	if constexpr (!::fast_io::details::is_unicode_execution_charset<char_type>)
	{
		auto const selected{size < maximum_display_width ? size : maximum_display_width};
		return {selected, selected};
	}
	else
	{
		if (size == 0u || maximum_display_width == 0u)
		{
			return {};
		}
		unicode_prefix_measurement result{};
		auto const *const last{data + size};
		if (maximum_display_width == SIZE_MAX)
		{
			while (data != last)
			{
				if constexpr (sizeof(char_type) == 1u)
				{
					using unsigned_type = ::std::make_unsigned_t<char_type>;
					if (static_cast<unsigned_type>(*data) < 0x80u)
					{
						auto const *const ascii_last{
							find_ascii_run_end(data, last)};
						auto const count{
							static_cast<::std::size_t>(ascii_last - data)};
						result.storage_size += count;
						result.display_width += count;
						data = ascii_last;
						continue;
					}
				}
				auto const scalar{decode_unicode_scalar(
					data, static_cast<::std::size_t>(last - data))};
				result.storage_size += scalar.code_units;
				result.display_width += scalar.valid
					? estimated_unicode_display_width(scalar.code_point)
					: 1u;
				data += scalar.code_units;
			}
			return result;
		}
		while (data != last)
		{
			auto const remaining_width{
				maximum_display_width - result.display_width};
			if (remaining_width == 0u)
			{
				break;
			}
			if constexpr (sizeof(char_type) == 1u)
			{
				using unsigned_type = ::std::make_unsigned_t<char_type>;
				if (static_cast<unsigned_type>(*data) < 0x80u)
				{
					auto const storage_left{
						static_cast<::std::size_t>(last - data)};
					auto const run_limit{remaining_width < storage_left
						? remaining_width
						: storage_left};
					auto const *const ascii_last{
						find_ascii_run_end(data, data + run_limit)};
					auto const count{
						static_cast<::std::size_t>(ascii_last - data)};
					result.storage_size += count;
					result.display_width += count;
					data = ascii_last;
					continue;
				}
			}
			auto const scalar{decode_unicode_scalar(
				data, static_cast<::std::size_t>(last - data))};
			auto const scalar_width{scalar.valid
				? estimated_unicode_display_width(scalar.code_point)
				: 1u};
			if (remaining_width < scalar_width)
			{
				break;
			}
			result.storage_size += scalar.code_units;
			result.display_width += scalar_width;
			data += scalar.code_units;
		}
		return result;
	}
}

struct format_padding_measurement
{
	::std::size_t left{};
	::std::size_t right{};
};

[[nodiscard]] inline constexpr format_padding_measurement measure_format_padding(
	::std::size_t content_width, ::std::size_t minimum_width,
	::fast_io::manipulators::scalar_placement placement) noexcept
{
	if (content_width >= minimum_width)
	{
		return {};
	}
	auto const padding{minimum_width - content_width};
	if (placement == ::fast_io::manipulators::scalar_placement::left)
	{
		return {0u, padding};
	}
	if (placement == ::fast_io::manipulators::scalar_placement::middle)
	{
		// fmt assigns the odd remainder to the right side.
		auto const left{padding / 2u};
		return {left, padding - left};
	}
	return {padding, 0u};
}

template <::fast_io::fmt::format_character char_type>
struct basic_text_field_options
{
	::std::size_t maximum_display_width{SIZE_MAX};
	::std::size_t minimum_width{};
	char_type fill[4u]{::fast_io::char_literal_v<u8' ', char_type>};
	::std::uint_least8_t fill_size{1u};
	::fast_io::manipulators::scalar_placement placement{
		::fast_io::manipulators::scalar_placement::left};
};

/**
 * Builds field policy from the already-compiled format specification.
 *
 * `fill_size` is a storage count.  One fill scalar may occupy multiple code
 * units, but the formatting grammar defines padding as a number of repetitions;
 * the allocator therefore multiplies repetitions by this count.
 */
template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr basic_text_field_options<char_type>
make_text_field_options(::std::size_t maximum_display_width,
						::std::size_t minimum_width,
						::fast_io::manipulators::scalar_placement placement,
						char_type const *fill = nullptr, ::std::size_t fill_size = 0u) noexcept
{
	basic_text_field_options<char_type> result{};
	result.maximum_display_width = maximum_display_width;
	result.minimum_width = minimum_width;
	result.placement = placement;
	if (fill_size != 0u)
	{
		// Keep the nullable run-time API while avoiding GCC sanitizer pointer
		// instrumentation in constant evaluation.  Every compiled format with a
		// nonzero fill size supplies the embedded fill array.
#if FAST_IO_HAS_BUILTIN(__builtin_is_constant_evaluated)
		if (!__builtin_is_constant_evaluated() && fill == nullptr) [[unlikely]]
#else
		if (!::std::is_constant_evaluated() && fill == nullptr) [[unlikely]]
#endif
		{
			return result;
		}
		result.fill_size = static_cast<::std::uint_least8_t>(fill_size <= 4u ? fill_size : 4u);
		for (::std::size_t index{}; index != result.fill_size; ++index)
		{
			result.fill[index] = fill[index];
		}
	}
	return result;
}

template <::fast_io::fmt::format_character char_type>
inline constexpr char_type *emit_format_fill_impl(
	char_type *output, char_type const *fill, ::std::size_t fill_size,
	::std::size_t repetitions) noexcept
{
	return emit_repeated_code_unit_pattern(
		output, fill, fill_size, repetitions);
}

template <::fast_io::fmt::format_character char_type>
inline constexpr char_type *emit_format_fill(
	char_type *output, basic_text_field_options<char_type> const &options,
	::std::size_t repetitions) noexcept
{
	return emit_format_fill_impl(
		output, options.fill, options.fill_size, repetitions);
}

template <::fast_io::fmt::format_character char_type>
struct basic_prepared_text_field_options
{
	::std::size_t storage_size{};
	::std::size_t padding_repetitions{};
	char_type fill[4u]{};
	::std::uint_least8_t fill_size{};
	::fast_io::manipulators::scalar_placement placement{
		::fast_io::manipulators::scalar_placement::left};
};

template <::fast_io::fmt::format_character char_type>
struct basic_unicode_text_field
{
	using manip_tag = ::fast_io::manip_tag_t;
	::fast_io::basic_io_scatter_t<char_type> source{};
	basic_prepared_text_field_options<char_type> options{};
};

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr basic_unicode_text_field<char_type>
make_unicode_text_field(::fast_io::basic_io_scatter_t<char_type> source,
						basic_text_field_options<char_type> options) noexcept
{
	auto const content{measure_unicode_prefix(
		source.base, source.len, options.maximum_display_width)};
	auto const padding{measure_format_padding(content.display_width,
		options.minimum_width, options.placement)};
	auto const repetitions{::fast_io::details::intrinsics::add_or_overflow_die(
		padding.left, padding.right)};
	::std::size_t padding_storage_size{};
	if (options.fill_size != 0u)
	{
		padding_storage_size = ::fast_io::details::intrinsics::mul_or_overflow_die(
			repetitions, static_cast<::std::size_t>(options.fill_size));
	}
	basic_prepared_text_field_options<char_type> prepared{
		.storage_size = ::fast_io::details::intrinsics::add_or_overflow_die(
			content.storage_size, padding_storage_size),
		.padding_repetitions = repetitions,
		.fill_size = options.fill_size,
		.placement = options.placement};
	for (::std::size_t index{}; index != 4u; ++index)
	{
		prepared.fill[index] = options.fill[index];
	}
	// This details-only value deliberately snapshots the scalar-aligned prefix
	// and padding metadata.  Both the ordinary and precise reserve protocols can
	// now emit it without rescanning the borrowed source.
	return {{source.base, content.storage_size}, prepared};
}

template <::fast_io::fmt::format_character char_type>
inline constexpr char_type *emit_unicode_text_field(char_type *output,
												basic_unicode_text_field<char_type> const &field) noexcept
{
	auto const padding{measure_format_padding(
		0u, field.options.padding_repetitions, field.options.placement)};
	output = emit_format_fill_impl(
		output, field.options.fill, field.options.fill_size, padding.left);
	// The reserve protocol supplies distinct source and destination storage,
	// matching every other scatter/string reserve definition in core.  Use the
	// constexpr-aware bulk primitive so run time lowers to memcpy while constant
	// evaluation retains the portable element-wise path.
	if constexpr (::std::is_volatile_v<char_type>)
	{
		for (::std::size_t index{}; index != field.source.len; ++index)
		{
			*output++ = field.source.base[index];
		}
	}
	else
	{
		output = ::fast_io::details::non_overlapped_copy_n(
			field.source.base, field.source.len, output);
	}
	return emit_format_fill_impl(
		output, field.options.fill, field.options.fill_size, padding.right);
}

} // namespace fast_io::fmt::details

namespace fast_io
{

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_unicode_text_field<source_char_type>>,
	::fast_io::fmt::details::basic_unicode_text_field<source_char_type> field) noexcept
{
	return field.options.storage_size;
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
inline constexpr output_char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_unicode_text_field<source_char_type>>,
	output_char_type *output,
	::fast_io::fmt::details::basic_unicode_text_field<source_char_type> field) noexcept
{
	return ::fast_io::fmt::details::emit_unicode_text_field(output, field);
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_unicode_text_field<source_char_type>>
		tag,
	::fast_io::fmt::details::basic_unicode_text_field<source_char_type> field) noexcept
{
	return print_reserve_size(tag, field);
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
inline constexpr output_char_type *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_unicode_text_field<source_char_type>>,
	output_char_type *output, ::std::size_t,
	::fast_io::fmt::details::basic_unicode_text_field<source_char_type> field) noexcept
{
	return ::fast_io::fmt::details::emit_unicode_text_field(output, field);
}

/**
 * Preparing the field already determines its exact size, and emission
 * overwrites every destination element.  The endpoint-returning, non-throwing
 * define CPO proves that an overwrite-capable concat destination need not
 * value-initialize that storage first.
 */
template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::true_type
	print_precise_resize_initialization_sensitive(
		::fast_io::io_reserve_type_t<output_char_type,
									 ::fast_io::fmt::details::basic_unicode_text_field<source_char_type>>) noexcept
{
	return {};
}

} // namespace fast_io

#include "../../fast_io_dsal/impl/misc/pop_macros.h"

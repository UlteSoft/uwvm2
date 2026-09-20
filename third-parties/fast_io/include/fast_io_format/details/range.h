#pragma once

/*
 * Range-format syntax and lowering support (FMT level).
 *
 * Range-specific presentations, separators, delimiters, element rules, and
 * nested specifications are parsed and represented here. Accepted ranges are
 * lowered compositionally into typed semantic/printable objects, so elements
 * reuse the same format-rule protocol and the final record reuses the ordinary
 * IO print or concat pipeline.
 */

#include "builtin_diagnostics.h"
#include "custom.h"
#include "debug.h"
#include "field.h"
#include "rule_protocol.h"

#include <concepts>
#include <cstddef>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

namespace fast_io::fmt::details
{

enum class range_format_presentation : unsigned char
{
	default_presentation,
	string,
	debug_string
};

enum class range_format_parse_error : unsigned char
{
	none,
	source_slice_out_of_bounds,
	invalid_range_specification,
	incompatible_range_options
};

/**
 * A structural, compile-time-only representation of fmt's range grammar.
 *
 * The carrier owns the underlying element specification instead of retaining a
 * pointer into a parser temporary.  It can therefore be used directly as a
 * class-type NTTP by the lowering layer.  `element_specification_size` is a
 * source-code-unit count; no runtime cursor or token dispatch survives in a
 * printable range leaf.
 *
 * The common brace parser stores the unconsumed type-directed suffix as a source
 * slice.  In particular, accepting only a new `n` presentation would not be
 * sufficient because `:n:f` and `::#x` must retain everything after the second
 * colon for the element formatter.  `parse_range_format_specification` below
 * consumes that slice of the original NTTP, so the hand-off neither allocates
 * nor constructs a second run-time string.
 */
template <::fast_io::fmt::format_character char_type, ::std::size_t capacity>
struct basic_range_format_specification
{
	using value_type = char_type;
	static inline constexpr ::std::size_t maximum_capacity{capacity};

	bool no_delimiters{};
	range_format_presentation presentation{
		range_format_presentation::default_presentation};
	bool has_element_specification{};
	char_type element_specification[capacity == 0u ? 1u : capacity]{};
	::std::size_t element_specification_size{};
	// Absolute source position of the first element-specification code unit.
	// The element grammar is compiled through a small synthetic literal, but
	// diagnostics must still identify the user's original outer format string.
	::std::size_t element_source_offset{};
	brace_indexing_state element_indexing_state{
		brace_indexing_state::undetermined};
	::std::size_t element_next_automatic_index{};
	range_format_parse_error error{range_format_parse_error::none};
	::std::size_t error_position{};

	[[nodiscard]] inline constexpr bool operator==(
		basic_range_format_specification const &) const noexcept = default;
};

template <range_format_parse_error error, ::std::size_t source_position>
inline consteval void diagnose_range_format_parse_error()
{
	if constexpr (error == range_format_parse_error::source_slice_out_of_bounds)
	{
		static_assert(error == range_format_parse_error::none,
					  "fast_io format: internal range-specification source slice is out of bounds");
	}
	else if constexpr (error == range_format_parse_error::invalid_range_specification)
	{
		static_assert(error == range_format_parse_error::none,
					  "fast_io format: invalid brace range format specification");
	}
	else if constexpr (error == range_format_parse_error::incompatible_range_options)
	{
		static_assert(error == range_format_parse_error::none,
					  "fast_io format: range n/underlying options are incompatible with s or ?s");
	}
	else
	{
		static_assert(error == range_format_parse_error::none);
	}
	(void)source_position;
}

/**
 * Parses `range_format_spec ::= ["n"][range_type][":" element_spec]`.
 *
 * `offset` and `count` designate only the contents after the field's first
 * colon; neither the outer braces nor the argument identifier are included.
 * Syntax characters are compared through fast_io's execution-character-set
 * mapping, so the same consteval pass works for all five supported character
 * types and for EBCDIC execution character sets.
 */
template <::fast_io::fmt::basic_fixed_string source, ::std::size_t offset = 0u,
		  ::std::size_t count = source.size() - offset>
[[nodiscard]] consteval auto parse_range_format_specification() noexcept
{
	using char_type = typename decltype(source)::value_type;
	basic_range_format_specification<char_type, count> result{};
	if constexpr (offset > source.size() || count > source.size() - offset)
	{
		result.error = range_format_parse_error::source_slice_out_of_bounds;
		result.error_position = offset;
		return result;
	}

	auto at = [](::std::size_t position) constexpr noexcept -> char_type {
		return source[offset + position];
	};
	::std::size_t cursor{};
	if (cursor != count && is_syntax_character<u8'n'>(at(cursor)))
	{
		result.no_delimiters = true;
		++cursor;
	}

	if (cursor != count && is_syntax_character<u8'?'>(at(cursor)))
	{
		if (result.no_delimiters || cursor + 1u == count ||
			!is_syntax_character<u8's'>(at(cursor + 1u)))
		{
			result.error = result.no_delimiters
							   ? range_format_parse_error::incompatible_range_options
							   : range_format_parse_error::invalid_range_specification;
			result.error_position = offset + cursor;
			return result;
		}
		cursor += 2u;
		if (cursor != count)
		{
			result.error = range_format_parse_error::incompatible_range_options;
			result.error_position = offset + cursor;
			return result;
		}
		result.presentation = range_format_presentation::debug_string;
		return result;
	}

	if (cursor != count && is_syntax_character<u8's'>(at(cursor)))
	{
		if (result.no_delimiters)
		{
			result.error = range_format_parse_error::incompatible_range_options;
			result.error_position = offset + cursor;
			return result;
		}
		++cursor;
		if (cursor != count)
		{
			result.error = range_format_parse_error::incompatible_range_options;
			result.error_position = offset + cursor;
			return result;
		}
		result.presentation = range_format_presentation::string;
		return result;
	}

	if (cursor == count)
	{
		return result;
	}
	if (!is_syntax_character<u8':'>(at(cursor)))
	{
		result.error = range_format_parse_error::invalid_range_specification;
		result.error_position = offset + cursor;
		return result;
	}
	++cursor;
	result.has_element_specification = true;
	result.element_source_offset = offset + cursor;
	for (; cursor != count; ++cursor)
	{
		result.element_specification[result.element_specification_size++] = at(cursor);
	}
	return result;
}

template <::fast_io::fmt::basic_fixed_string source, ::std::size_t offset = 0u,
		  ::std::size_t count = source.size() - offset>
[[nodiscard]] consteval auto make_checked_range_format_specification() noexcept
{
	constexpr auto result{
		parse_range_format_specification<source, offset, count>()};
	if constexpr (result.error != range_format_parse_error::none)
	{
		diagnose_range_format_parse_error<result.error, result.error_position>();
	}
	return result;
}

template <::fast_io::fmt::basic_fixed_string source, ::std::size_t offset = 0u,
		  ::std::size_t count = source.size() - offset>
inline constexpr auto checked_range_format_specification{
	make_checked_range_format_specification<source, offset, count>()};

template <::fast_io::fmt::format_character char_type>
inline constexpr auto default_range_format_specification = []() consteval {
	return basic_range_format_specification<char_type, 0u>{};
}();

/**
 * Synthetic top-level `{}` field used by the default range-element grammar.
 *
 * A range changes only two empty-field cases before ordinary brace lowering:
 * character and string elements use debug spelling, and nested ranges recurse.
 * Every remaining value must be offered to exactly the same two-phase rule
 * protocol as a real top-level `{}`.  Keeping that empty field structural lets
 * phase one select a provider without a runtime formatter object, while the
 * backend still receives the original element expression in phase two.
 */
template <::fast_io::fmt::format_character char_type>
inline constexpr replacement_field<char_type> default_range_element_field{};

/**
 * Converts the brace field's original NTTP slice into the range-specific NTTP.
 *
 * Range grammar replaces, rather than extends, the scalar grammar.  Reusing
 * the common parsed fields would misread `{::>5}`: a scalar sees ':' as a fill,
 * while a range sees the first ':' as the element-specification delimiter.
 * The raw slice resolves that ambiguity entirely during constant evaluation.
 * Its own parser also rejects `{:10}` as fmt does, instead of silently treating
 * 10 as an outer scalar width.
 */
template <::fast_io::fmt::basic_fixed_string source,
		  format_specification<typename decltype(source)::value_type> specification>
[[nodiscard]] consteval auto checked_range_specification_from_common() noexcept
{
	auto result{checked_range_format_specification<source,
												   specification.raw_format_specification.offset,
												   specification.raw_format_specification.size>};
	result.element_indexing_state =
		specification.raw_format_indexing_state;
	result.element_next_automatic_index =
		specification.raw_format_next_automatic_index;
	return result;
}

template <typename T>
inline constexpr bool map_range_source_v = requires {
	typename ::std::remove_cvref_t<T>::key_type;
	typename ::std::remove_cvref_t<T>::mapped_type;
};

template <typename T>
inline constexpr bool set_range_source_v = requires {
	typename ::std::remove_cvref_t<T>::key_type;
} && !map_range_source_v<T>;

template <typename T, typename = void>
struct tuple_format_source_trait : ::std::false_type
{};

template <typename T>
struct tuple_format_source_trait<T, ::std::void_t<decltype(::std::tuple_size<::std::remove_cvref_t<T>>::value)>> : ::std::true_type
{};

template <typename T>
concept tuple_format_source =
	(!::std::ranges::range<T>) && tuple_format_source_trait<T>::value;

template <::std::size_t index, typename tuple_type>
[[nodiscard]] inline constexpr decltype(auto) brace_tuple_get(tuple_type &&value) noexcept(noexcept(get<index>(::std::forward<tuple_type>(value))))
{
	using ::std::get;
	return get<index>(::std::forward<tuple_type>(value));
}

template <::fast_io::fmt::format_character char_type, char8_t... values>
inline constexpr char_type range_ascii_storage[]{
	::fast_io::char_literal_v<values, char_type>...};

template <::fast_io::fmt::format_character char_type, char8_t... values>
[[nodiscard]] inline constexpr ::fast_io::manipulators::static_scatter_t<
	char_type, sizeof...(values)>
range_ascii_scatter() noexcept
{
	// Range punctuation has a language-static extent and lifetime. Preserve that
	// fact in its semantic type so the IO layer may retain the immutable pointer
	// for an unbuffered scatter run; this does not classify any adjacent dynamic
	// element, or the complete range record, as static data.
	return {range_ascii_storage<char_type, values...>};
}

template <auto specification>
[[nodiscard]] consteval auto make_range_element_format_literal() noexcept
{
	using specification_type = ::std::remove_cv_t<decltype(specification)>;
	using char_type = typename specification_type::value_type;
	constexpr ::std::size_t element_size{
		specification.element_specification_size};
	// The trailing brace is the delimiter expected by
	// `parse_brace_specification`; an outer replacement field is deliberately
	// not synthesized.  A fake automatic outer field would consume argument 0
	// and would also reject an otherwise valid manually indexed dynamic width.
	char_type text[element_size + 2u]{};
	for (::std::size_t index{}; index != element_size; ++index)
	{
		text[index] = specification.element_specification[index];
	}
	text[element_size] = ::fast_io::arithmetic_char_literal_v<u8'}', char_type>;
	return ::fast_io::fmt::basic_fixed_string<char_type, element_size + 2u>{text};
}

template <auto specification>
inline constexpr auto range_element_format_literal{
	make_range_element_format_literal<specification>()};

template <auto specification>
[[nodiscard]] consteval auto make_checked_range_element_field() noexcept
{
	static_assert(specification.has_element_specification,
				  "fast_io format: internal range element parser requires an underlying specification");
	constexpr auto literal{range_element_format_literal<specification>};
	constexpr auto parsed = [=]() consteval {
		using char_type = typename decltype(literal)::value_type;
		using scratch_program = basic_format_program<char_type, literal.size(), 2u>;
		format_parse_result<scratch_program> scratch{};
		replacement_field<char_type> field{};
		brace_indexing_state indexing_state{
			specification.element_indexing_state};
		::std::size_t next_automatic_index{
			specification.element_next_automatic_index};
		::std::size_t cursor{};
		(void)parse_brace_specification<literal>(
			scratch, cursor, field.specification, indexing_state,
			next_automatic_index);
		if (scratch.error != format_parse_error::none)
		{
			// `literal` begins exactly where the user's underlying element
			// specification begins. Translate the synthetic parser position back
			// to the outer NTTP before instantiating the diagnostic.
			scratch.error_position += specification.element_source_offset;
		}
		else if (cursor == literal.size() ||
				 !is_syntax_character<u8'}'>(literal[cursor]))
		{
			set_parse_error(scratch,
							format_parse_error::invalid_format_specification,
							specification.element_source_offset + cursor);
		}
		field.source = {0u, literal.size()};
		scratch.program.fields[0u] = field;
		return scratch;
	}();
	if constexpr (parsed.error != format_parse_error::none)
	{
		diagnose_parse_error<parsed.error, parsed.error_position>();
	}
	return parsed.program.fields[0u];
}

template <auto specification>
inline constexpr auto checked_range_element_field{
	make_checked_range_element_field<specification>()};

/** A tag offered to ADL before the built-in range-element lowering. */
template <::fast_io::fmt::format_character char_type, auto specification>
struct brace_range_element_format_tag
{
	using value_type = char_type;
	static inline constexpr auto format_specification{specification};
};

void brace_range_format_element() = delete;

template <typename char_type, auto specification, typename value_type>
concept adl_brace_range_element =
	::fast_io::fmt::format_character<char_type> &&
	requires(value_type &&value) {
		brace_range_format_element(
			brace_range_element_format_tag<char_type, specification>{},
			::std::forward<value_type>(value));
	};

template <::fast_io::fmt::format_character char_type>
struct basic_range_debug_scalar
{
	using manip_tag = ::fast_io::manip_tag_t;
	char_type source[4u]{};
	debug_scalar_rendering rendering{};
};

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr basic_range_debug_scalar<char_type>
make_range_debug_scalar(char_type const *source,
						::std::size_t count) noexcept
{
	basic_range_debug_scalar<char_type> result{};
	for (::std::size_t index{}; index != count; ++index)
	{
		result.source[index] = source[index];
	}
	result.rendering = classify_debug_scalar<debug_text_kind::string>(
		result.source, count);
	return result;
}

template <::fast_io::fmt::format_character char_type>
[[nodiscard]] inline constexpr ::std::size_t
debug_scalar_target_units(char_type first) noexcept
{
	using clean_type = ::std::remove_cv_t<char_type>;
	if constexpr (!::fast_io::details::is_unicode_execution_charset<clean_type> ||
				  ::std::same_as<clean_type, char32_t> ||
				  (::std::same_as<clean_type, wchar_t> && sizeof(wchar_t) == 4u))
	{
		return 1u;
	}
	else if constexpr (::std::same_as<clean_type, char16_t> ||
					   (::std::same_as<clean_type, wchar_t> && sizeof(wchar_t) == 2u))
	{
		auto const value{format_unicode_code_unit_value(first)};
		return 0xd800u <= value && value <= 0xdbffu ? 2u : 1u;
	}
	else
	{
		auto const value{format_unicode_code_unit_value(first)};
		if (0xc2u <= value && value <= 0xdfu)
		{
			return 2u;
		}
		if (0xe0u <= value && value <= 0xefu)
		{
			return 3u;
		}
		if (0xf0u <= value && value <= 0xf4u)
		{
			return 4u;
		}
		return 1u;
	}
}

template <::fast_io::fmt::format_character char_type, auto specification,
		  typename source_type, typename argument_pack_type>
struct basic_brace_range_view
{
	using source_char_type = char_type;
	using source_value_type = source_type;
	using arguments_type = argument_pack_type;
	static inline constexpr auto format_specification{specification};

	source_type *source{};
	argument_pack_type *arguments{};
};

template <::fast_io::fmt::format_character char_type, typename source_type,
		  ::std::size_t depth = 0u>
inline consteval bool brace_range_default_staging_safe_source() noexcept;

template <::fast_io::fmt::format_character char_type, typename source_type,
		  ::std::size_t depth>
inline consteval bool brace_range_default_staging_safe_element() noexcept
{
	using clean_type = ::std::remove_cvref_t<source_type>;
	if constexpr (adl_brace_range_element<
			  char_type, default_range_format_specification<char_type>, source_type>)
	{
		// An ADL replacement may deliberately depend on the concrete endpoint.
		return false;
	}
	else if constexpr (::fast_io::details::character_integral<clean_type>)
	{
		return ::std::same_as<clean_type, char_type>;
	}
	else if constexpr (format_string_like<char_type, source_type>)
	{
		return true;
	}
	else if constexpr (::std::ranges::input_range<clean_type &> ||
					   tuple_format_source<clean_type>)
	{
		if constexpr (depth == 8u)
		{
			// Bound concept recursion for self-similar user ranges and pathological
			// nesting; deeper sources retain the ordinary correct direct path.
			return false;
		}
		else
		{
			return brace_range_default_staging_safe_source<
				char_type, clean_type, depth>();
		}
	}
	else
	{
		// These built-in scalar categories lower to core semantic nodes whose
		// spelling is independent of the eventual output observer.
		return ::fast_io::details::my_integral<clean_type> ||
			   ::fast_io::details::my_floating_point<clean_type> ||
			   ::std::is_pointer_v<clean_type> ||
			   ::std::same_as<clean_type, ::std::nullptr_t>;
	}
}

template <::fast_io::fmt::format_character char_type, typename tuple_type,
		  ::std::size_t depth, ::std::size_t... index>
inline consteval bool brace_tuple_default_staging_safe_impl(
	::std::index_sequence<index...>) noexcept
{
	return (brace_range_default_staging_safe_element<
				 char_type,
				 decltype(brace_tuple_get<index>(::std::declval<tuple_type &>())),
				 depth + 1u>() &&
			...);
}

template <::fast_io::fmt::format_character char_type, typename source_type,
		  ::std::size_t depth>
inline consteval bool brace_range_default_staging_safe_source() noexcept
{
	using clean_type = ::std::remove_cvref_t<source_type>;
	if constexpr (tuple_format_source<clean_type>)
	{
		return brace_tuple_default_staging_safe_impl<
			char_type, clean_type, depth>(
			::std::make_index_sequence<::std::tuple_size_v<clean_type>>{});
	}
	else if constexpr (::std::ranges::input_range<clean_type &>)
	{
		// Audit the expression actually observed by the formatter, not merely
		// `range_value_t`. A proxy reference may provide an ADL element formatter
		// even when its nominal value type is an otherwise admitted scalar.
		using reference_type = ::std::ranges::range_reference_t<clean_type>;
		return brace_range_default_staging_safe_element<
			char_type, reference_type, depth + 1u>();
	}
	else
	{
		return false;
	}
}

template <::fast_io::fmt::format_character char_type, auto specification,
		  typename source_type, typename argument_pack_type>
	requires(::std::ranges::input_range<source_type &> ||
			 tuple_format_source<source_type>)
[[nodiscard]] inline constexpr auto make_brace_range_view(
	source_type &source, argument_pack_type &arguments) noexcept
{
	using specification_type = ::std::remove_cv_t<decltype(specification)>;
	static_assert(::std::same_as<typename specification_type::value_type, char_type>,
				  "fast_io format: range specification and output character types must match");
	static_assert(specification.error == range_format_parse_error::none,
				  "fast_io format: unchecked range specification passed to lowering");
	if constexpr (tuple_format_source<source_type>)
	{
		static_assert(specification.presentation ==
							  range_format_presentation::default_presentation &&
						  !specification.has_element_specification,
					  "fast_io format: tuple formatting accepts only the optional n specifier");
	}
	else if constexpr (map_range_source_v<source_type>)
	{
		static_assert(specification.presentation ==
							  range_format_presentation::default_presentation &&
						  !specification.has_element_specification,
					  "fast_io format: map formatting accepts only the optional n specifier");
	}
	else if constexpr (specification.presentation !=
					   range_format_presentation::default_presentation)
	{
		using element_type = ::std::remove_cv_t<
			::std::ranges::range_value_t<source_type>>;
		static_assert(::std::same_as<element_type, char_type>,
					  "fast_io format: range s and ?s require elements matching the output character type");
	}
	return basic_brace_range_view<char_type, specification, source_type,
								  argument_pack_type>{__builtin_addressof(source),
													  __builtin_addressof(arguments)};
}

template <typename output_type, typename... argument_types>
inline constexpr void emit_range_components(
	output_type &output, argument_types &&...arguments)
{
	::fast_io::operations::decay::print_freestanding_decay_unforwarded<false>(
		output, ::std::forward<argument_types>(arguments)...);
}

/**
 * Transparent bridge from range-element selection to the core print decay boundary.
 *
 * The selected customization may return an identity-bearing lvalue proxy or an
 * xvalue into storage which remains alive for the synchronous emission.  Preserve
 * that exact value category here; only the downstream print transport is allowed
 * to choose between borrowing the proxy and materializing an ABI value.
 */
template <::fast_io::fmt::format_character char_type, auto specification,
		  typename source_type, typename argument_pack_type>
[[nodiscard]] inline constexpr decltype(auto) make_brace_range_element(
	source_type &&source, argument_pack_type &arguments);

template <::fast_io::fmt::format_character char_type, typename source_type,
		  typename argument_pack_type>
[[nodiscard]] inline constexpr decltype(auto) make_default_brace_range_element(
	source_type &&source, argument_pack_type &arguments)
{
	using clean_type = ::std::remove_cvref_t<source_type>;
	if constexpr (adl_brace_range_element<char_type,
										  default_range_format_specification<char_type>, source_type>)
	{
		return brace_range_format_element(
			brace_range_element_format_tag<char_type,
										   default_range_format_specification<char_type>>{},
			::std::forward<source_type>(source));
	}
	else if constexpr (::fast_io::details::character_integral<clean_type>)
	{
		static_assert(::std::same_as<clean_type, char_type>,
					  "fast_io format: a default range character must match the output character type");
		return make_debug_character_field(static_cast<char_type>(source), {});
	}
	else if constexpr (format_string_like<char_type, source_type>)
	{
		auto &&named_source{source};
		auto const source_scatter{static_cast<::fast_io::basic_io_scatter_t<char_type>>(
			make_string_scatter<char_type>(named_source))};
		return make_default_debug_string_field(source_scatter);
	}
	else if constexpr (::std::ranges::input_range<clean_type &> ||
					   tuple_format_source<clean_type>)
	{
		auto &&named_source{source};
		return make_brace_range_view<char_type,
									 default_range_format_specification<char_type>>(
			named_source, arguments);
	}
	else
	{
		auto &&named_source{source};
		constexpr auto literal{range_element_format_literal<
			default_range_format_specification<char_type>>};
		constexpr auto field{default_range_element_field<char_type>};
		using value_reference = decltype(named_source);

		// The range grammar has already consumed its deliberate debug/nesting
		// exceptions above.  It must not duplicate the brace provider ladder here:
		// doing so previously let the top-level and range paths disagree whenever
		// a new custom, format_as, chrono, scalar, or backend-identity concept was
		// added.  Selection and construction now use the same two-phase CPO as a
		// real `{}` replacement.  The source is passed as its exact reference type,
		// preserving proxy references and leaving ABI-aware decay to fast_io core.
		if constexpr (format_replacement_rule_for<brace_fmt_t, literal, field,
												  value_reference, argument_pack_type>)
		{
			return format_replacement_rule_adl::invoke<brace_fmt_t, literal, field>(
				named_source, arguments);
		}
		else
		{
			static_assert(format_replacement_rule_for<brace_fmt_t, literal, field,
													  value_reference, argument_pack_type>,
						  "fast_io format: no concept-defined replacement rule accepts this default range element");
			return ::fast_io::io_null;
		}
	}
}

template <::fast_io::fmt::format_character char_type, auto specification,
		  typename source_type, typename argument_pack_type>
[[nodiscard]] inline constexpr decltype(auto) make_brace_range_element(
	source_type &&source, argument_pack_type &arguments)
{
	if constexpr (adl_brace_range_element<char_type, specification, source_type>)
	{
		return brace_range_format_element(
			brace_range_element_format_tag<char_type, specification>{},
			::std::forward<source_type>(source));
	}
	else if constexpr (!specification.has_element_specification)
	{
		return make_default_brace_range_element<char_type>(
			::std::forward<source_type>(source), arguments);
	}
	else
	{
		constexpr auto field{checked_range_element_field<specification>};
		constexpr auto literal{range_element_format_literal<specification>};
		auto &&named_source{source};
		using value_reference = decltype(named_source);

		// The synthetic literal/field pair is a complete brace replacement
		// context.  Feeding it through the common protocol is what makes `{::x}`
		// obey the same provider selection as top-level `{:x}`—including user ADL
		// rules and future concepts which this range implementation cannot know.
		// Phase two still returns an ordinary fast_io semantic node, so range
		// emission performs no runtime parse, type erasure, or indirect dispatch.
		if constexpr (format_replacement_rule_for<brace_fmt_t, literal, field,
												  value_reference, argument_pack_type>)
		{
			return format_replacement_rule_adl::invoke<brace_fmt_t, literal, field>(
				named_source, arguments);
		}
		else
		{
			static_assert(format_replacement_rule_for<brace_fmt_t, literal, field,
													  value_reference, argument_pack_type>,
						  "fast_io format: no concept-defined replacement rule accepts this explicitly formatted range element");
			return ::fast_io::io_null;
		}
	}
}

template <::fast_io::fmt::format_character char_type, typename output_type,
		  typename range_type>
inline constexpr void emit_debug_character_range(
	output_type &output, range_type &range)
{
	emit_range_components(output, range_ascii_scatter<char_type, u8'\"'>());
	auto iterator{::std::ranges::begin(range)};
	auto const sentinel{::std::ranges::end(range)};
	char_type pending[4u]{};
	::std::size_t pending_size{};
	while (pending_size != 0u || iterator != sentinel)
	{
		if (pending_size == 0u)
		{
			pending[pending_size++] = static_cast<char_type>(*iterator);
			++iterator;
		}
		auto const target{debug_scalar_target_units(pending[0u])};
		while (pending_size != target && iterator != sentinel)
		{
			pending[pending_size++] = static_cast<char_type>(*iterator);
			++iterator;
		}
		auto const fragment{make_range_debug_scalar(pending, pending_size)};
		emit_range_components(output, fragment);
		auto const consumed{fragment.rendering.source_units};
		for (::std::size_t index{consumed}; index != pending_size; ++index)
		{
			pending[index - consumed] = pending[index];
		}
		pending_size -= consumed;
	}
	emit_range_components(output, range_ascii_scatter<char_type, u8'\"'>());
}

template <::fast_io::fmt::format_character char_type, auto specification,
		  typename output_type, typename tuple_type, typename argument_pack_type,
		  ::std::size_t... index>
inline constexpr void emit_brace_tuple_impl(
	output_type &output, tuple_type &tuple, argument_pack_type &arguments,
	::std::index_sequence<index...>)
{
	auto emit_one = [&]<::std::size_t current>() constexpr {
		if constexpr (current != 0u && !specification.no_delimiters)
		{
			emit_range_components(output,
								  range_ascii_scatter<char_type, u8',', u8' '>());
		}
		decltype(auto) element{brace_tuple_get<current>(tuple)};
		decltype(auto) formatted{make_default_brace_range_element<char_type>(
			element, arguments)};
		emit_range_components(output,
							  ::std::forward<decltype(formatted)>(formatted));
	};
	(emit_one.template operator()<index>(), ...);
}

template <::fast_io::fmt::format_character char_type, auto specification,
		  typename output_type, typename tuple_type, typename argument_pack_type>
inline constexpr void emit_brace_tuple(
	output_type &output, tuple_type &tuple, argument_pack_type &arguments)
{
	if constexpr (!specification.no_delimiters)
	{
		emit_range_components(output, range_ascii_scatter<char_type, u8'('>());
	}
	constexpr ::std::size_t size{
		::std::tuple_size_v<::std::remove_cvref_t<tuple_type>>};
	emit_brace_tuple_impl<char_type, specification>(output, tuple, arguments,
													::std::make_index_sequence<size>{});
	if constexpr (!specification.no_delimiters)
	{
		emit_range_components(output, range_ascii_scatter<char_type, u8')'>());
	}
}

template <::fast_io::fmt::format_character char_type, auto specification,
		  typename output_type, typename range_type, typename argument_pack_type>
inline constexpr void emit_brace_map(
	output_type &output, range_type &range, argument_pack_type &arguments)
{
	if constexpr (!specification.no_delimiters)
	{
		emit_range_components(output, range_ascii_scatter<char_type, u8'{'>());
	}
	bool first{true};
	auto iterator{::std::ranges::begin(range)};
	auto const last{::std::ranges::end(range)};
	for (; iterator != last; ++iterator)
	{
		if (!first)
		{
			emit_range_components(output,
								  range_ascii_scatter<char_type, u8',', u8' '>());
		}
		first = false;
		decltype(auto) entry{*iterator};
		decltype(auto) key{brace_tuple_get<0u>(entry)};
		decltype(auto) mapped{brace_tuple_get<1u>(entry)};
		decltype(auto) formatted_key{
			make_default_brace_range_element<char_type>(key, arguments)};
		emit_range_components(
			output, ::std::forward<decltype(formatted_key)>(formatted_key),
			range_ascii_scatter<char_type, u8':', u8' '>());
		decltype(auto) formatted_mapped{
			make_default_brace_range_element<char_type>(mapped, arguments)};
		emit_range_components(output,
							  ::std::forward<decltype(formatted_mapped)>(formatted_mapped));
	}
	if constexpr (!specification.no_delimiters)
	{
		emit_range_components(output, range_ascii_scatter<char_type, u8'}'>());
	}
}

template <::fast_io::fmt::format_character char_type, auto specification,
		  typename output_type, typename range_type, typename argument_pack_type>
inline constexpr void emit_brace_sequence(
	output_type &output, range_type &range, argument_pack_type &arguments)
{
	if constexpr (specification.presentation == range_format_presentation::string)
	{
		emit_range_components(output, range_ascii_scatter<char_type, u8'\"'>());
		auto iterator{::std::ranges::begin(range)};
		auto const last{::std::ranges::end(range)};
		for (; iterator != last; ++iterator)
		{
			emit_range_components(output,
								  ::fast_io::manipulators::chvw(static_cast<char_type>(*iterator)));
		}
		emit_range_components(output, range_ascii_scatter<char_type, u8'\"'>());
		return;
	}
	else if constexpr (specification.presentation ==
					   range_format_presentation::debug_string)
	{
		emit_debug_character_range<char_type>(output, range);
		return;
	}

	if constexpr (!specification.no_delimiters)
	{
		if constexpr (set_range_source_v<range_type>)
		{
			emit_range_components(output, range_ascii_scatter<char_type, u8'{'>());
		}
		else
		{
			emit_range_components(output, range_ascii_scatter<char_type, u8'['>());
		}
	}
	bool first{true};
	auto iterator{::std::ranges::begin(range)};
	auto const last{::std::ranges::end(range)};
	for (; iterator != last; ++iterator)
	{
		if (!first)
		{
			emit_range_components(output,
								  range_ascii_scatter<char_type, u8',', u8' '>());
		}
		first = false;
		using range_value_type = ::std::remove_cv_t<
			::std::ranges::range_value_t<range_type>>;
		if constexpr (::std::same_as<range_value_type, bool>)
		{
			// `vector<bool>` and similar packed ranges expose a proxy reference.
			// Materializing its declared value type is both the fmt semantic model
			// and the only way to avoid asking a storage proxy to be printable.
			bool element{static_cast<bool>(*iterator)};
			decltype(auto) formatted{
				make_brace_range_element<char_type, specification>(
					element, arguments)};
			emit_range_components(output,
								  ::std::forward<decltype(formatted)>(formatted));
		}
		else
		{
			decltype(auto) element{*iterator};
			decltype(auto) formatted{
				make_brace_range_element<char_type, specification>(
					element, arguments)};
			emit_range_components(output,
								  ::std::forward<decltype(formatted)>(formatted));
		}
	}
	if constexpr (!specification.no_delimiters)
	{
		if constexpr (set_range_source_v<range_type>)
		{
			emit_range_components(output, range_ascii_scatter<char_type, u8'}'>());
		}
		else
		{
			emit_range_components(output, range_ascii_scatter<char_type, u8']'>());
		}
	}
}

} // namespace fast_io::fmt::details

namespace fast_io
{

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
[[nodiscard]] inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_range_debug_scalar<source_char_type>>,
	::fast_io::fmt::details::basic_range_debug_scalar<source_char_type> value) noexcept
{
	return value.rendering.storage_size;
}

template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
inline constexpr output_char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_range_debug_scalar<source_char_type>>,
	output_char_type *output,
	::fast_io::fmt::details::basic_range_debug_scalar<source_char_type> value) noexcept
{
	return ::fast_io::fmt::details::emit_debug_scalar(
		output, value.source, value.rendering, value.rendering.storage_size);
}

/// @brief Declares that a brace range view may emit through a bounded core staging put area.
/// @details Admission is deliberately limited to recursively known built-in scalar, string, tuple, and range elements;
///          an ADL/custom element remains on the ordinary endpoint so its output-dependent behavior cannot be hidden.
///          The admitted emitter performs one forward traversal and lowers each component to the ordinary IO protocol.
///          Core may therefore coalesce a short unbuffered record without measuring or replaying an input range;
///          capacity overflow continues through the same adapter, preserving one-pass iterator and formatter effects.
template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type, auto specification,
		  typename source_type, typename argument_pack_type>
	requires(
		::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type> &&
		!specification.has_element_specification &&
		::fast_io::fmt::details::brace_range_default_staging_safe_source<
			source_char_type, source_type>())
[[nodiscard]] inline constexpr ::std::true_type
print_single_pass_staging_safe(
	::fast_io::io_reserve_type_t<output_char_type,
		::fast_io::fmt::details::basic_brace_range_view<
			source_char_type, specification, source_type, argument_pack_type>>) noexcept
{
	return {};
}

/**
 * Streams a range or tuple leaf through the ordinary fast_io print backend.
 *
 * There is intentionally no reserve-size overload for the composite.  Exact
 * sizing would consume a first traversal and is invalid for an input range;
 * a conservative bound would either be unbounded or require duplicating every
 * element formatter's policy.  The direct printable protocol visits each
 * source element exactly once and lets the destination's existing buffering
 * strategy amortize writes.  Forward/sized sources can receive a separate
 * proved reserve specialization later without weakening this correctness
 * fallback.
 */
template <::std::integral output_char_type,
		  ::fast_io::fmt::format_character source_char_type, auto specification,
		  typename source_type, typename argument_pack_type, typename output_type>
	requires ::std::same_as<::std::remove_cv_t<output_char_type>, source_char_type>
inline constexpr void print_define(
	::fast_io::io_reserve_type_t<output_char_type,
								 ::fast_io::fmt::details::basic_brace_range_view<source_char_type,
																				 specification, source_type, argument_pack_type>>,
	output_type &output,
	::fast_io::fmt::details::basic_brace_range_view<source_char_type,
												specification, source_type, argument_pack_type>
		value)
{
	if constexpr (::fast_io::fmt::details::tuple_format_source<source_type>)
	{
		::fast_io::fmt::details::emit_brace_tuple<source_char_type, specification>(
			output, *value.source, *value.arguments);
	}
	else if constexpr (::fast_io::fmt::details::map_range_source_v<source_type>)
	{
		::fast_io::fmt::details::emit_brace_map<source_char_type, specification>(
			output, *value.source, *value.arguments);
	}
	else
	{
		::fast_io::fmt::details::emit_brace_sequence<source_char_type, specification>(
			output, *value.source, *value.arguments);
	}
}

} // namespace fast_io

#pragma once

/*
 * Built-in replacement-to-IO translations (FMT level).
 *
 * This file classifies parsed brace/printf specifications and maps them to
 * identity forwarding, scalar manipulators, width/precision nodes, debug/range
 * adapters, chrono adapters, or user customization. The result is a typed
 * semantic record for the IO level. Actual reserve sizing, scatter planning,
 * context formatting, and output transfer are intentionally downstream.
 */

#include "time.h"
#include "fast_io_time.h"
#include "builtin_diagnostics.h"
#include "custom.h"
#include "field.h"
#include "range.h"
#include "rule_protocol.h"

#include <concepts>
#include <ranges>
#include <type_traits>
#include <utility>

namespace fast_io::fmt::details
{

/** Compile-time identity test for the brace grammar's empty rule. */
template <format_specification specification>
inline constexpr bool identity_brace_specification_v =
	!specification.has_fill && specification.alignment == format_alignment::none &&
	specification.sign == format_sign::default_sign && !specification.alternate_form &&
	!specification.zero_padding && !specification.locale_specific &&
	specification.width.kind == format_parameter_kind::none &&
	specification.precision.kind == format_parameter_kind::none &&
	specification.presentation == presentation_type::none &&
	specification.format_tail.size == 0u;

// Empty rule tokens are the complete result of phase-one selection.  Their
// types record semantic policy without constructing a runtime formatter,
// parser cursor, or indirect call target.
struct brace_direct_identity_format_rule
{};
struct brace_custom_format_rule
{};
struct brace_format_as_rule
{};
struct brace_chrono_format_rule
{};
struct brace_range_format_rule
{};
struct brace_invalid_tail_format_rule
{};
struct brace_unclaimed_identity_format_rule
{};
struct brace_builtin_format_rule
{};
struct printf_builtin_format_rule
{};

template <auto format_literal, auto field, typename value_type>
concept brace_direct_identity_rule =
	identity_brace_specification_v<field.specification> &&
	format_backend_identity_printable<
		typename decltype(format_literal)::value_type, value_type>;

/**
 * Preserves the direct-identity proof across provider-template deduction.
 *
 * Clang 23 can incorrectly re-normalize the named concept as false when both
 * structural NTTPs are deduced through basic_format_replacement_context. A
 * variable-template instantiation evaluates the identical proof before that
 * deduction-sensitive constraint boundary; it neither admits another value
 * category nor changes the selected rule token.
 */
template <auto format_literal, auto field, typename value_type>
inline constexpr bool brace_direct_identity_rule_constraint_v{
	brace_direct_identity_rule<format_literal, field, value_type>};

template <auto format_literal, auto field, typename value_type>
concept brace_custom_replacement_rule =
	(!brace_direct_identity_rule<format_literal, field, value_type>) &&
	custom_format_parse_expression<format_literal,
								   field.specification.raw_format_specification, value_type>;

template <auto format_literal, auto field, typename value_type>
concept brace_format_as_replacement_rule =
	(!brace_direct_identity_rule<format_literal, field, value_type>) &&
	(!brace_custom_replacement_rule<format_literal, field, value_type>) &&
	zero_brace_format_specification_v<field.specification> &&
	adl_format_as_expression<value_type>;

template <auto format_literal, auto field, typename value_type>
concept brace_chrono_replacement_rule =
	(!brace_direct_identity_rule<format_literal, field, value_type>) &&
	(!brace_custom_replacement_rule<format_literal, field, value_type>) &&
	(!brace_format_as_replacement_rule<format_literal, field, value_type>) &&
	time_format_value<::std::remove_cvref_t<value_type>>;

template <auto format_literal, auto field, typename value_type>
concept brace_range_replacement_rule =
	(!brace_direct_identity_rule<format_literal, field, value_type>) &&
	(!brace_custom_replacement_rule<format_literal, field, value_type>) &&
	(!brace_format_as_replacement_rule<format_literal, field, value_type>) &&
	(!brace_chrono_replacement_rule<format_literal, field, value_type>) &&
	(::std::ranges::input_range<::std::remove_cvref_t<value_type> &> ||
	 tuple_format_source<::std::remove_cvref_t<value_type>>) &&
	(!brace_scalar_string_source<
		typename decltype(format_literal)::value_type, value_type>);

template <auto format_literal, auto field, typename value_type>
concept brace_invalid_tail_replacement_rule =
	(!brace_direct_identity_rule<format_literal, field, value_type>) &&
	(!brace_custom_replacement_rule<format_literal, field, value_type>) &&
	(!brace_format_as_replacement_rule<format_literal, field, value_type>) &&
	(!brace_chrono_replacement_rule<format_literal, field, value_type>) &&
	(!brace_range_replacement_rule<format_literal, field, value_type>) &&
	(field.specification.format_tail.size != 0u);

/** Positive capability set implemented by the scalar/text field backend. */
template <auto format_literal, typename value_type>
concept brace_builtin_value =
	::std::same_as<::std::remove_cvref_t<value_type>, bool> ||
	::fast_io::details::character_integral<
		::std::remove_cvref_t<value_type>> ||
	format_string_like<typename decltype(format_literal)::value_type,
					   value_type> ||
	::std::same_as<::std::remove_cvref_t<value_type>, ::std::nullptr_t> ||
	(::std::is_pointer_v<::std::remove_cvref_t<value_type>> &&
	 !::std::is_function_v<::std::remove_pointer_t<
		 ::std::remove_cvref_t<value_type>>>) ||
	::fast_io::details::my_floating_point<
		::std::remove_cvref_t<value_type>> ||
	::fast_io::details::my_integral<::std::remove_cvref_t<value_type>> ||
	::std::same_as<::std::remove_cvref_t<value_type>, ::std::byte> ||
	format_backend_identity_printable<
		typename decltype(format_literal)::value_type, value_type>;

template <auto format_literal, auto field, typename value_type>
concept brace_unclaimed_identity_replacement_rule =
	(!brace_direct_identity_rule<format_literal, field, value_type>) &&
	(!brace_custom_replacement_rule<format_literal, field, value_type>) &&
	(!brace_format_as_replacement_rule<format_literal, field, value_type>) &&
	(!brace_chrono_replacement_rule<format_literal, field, value_type>) &&
	(!brace_range_replacement_rule<format_literal, field, value_type>) &&
	(!brace_invalid_tail_replacement_rule<format_literal, field, value_type>) &&
	(!brace_builtin_value<format_literal, value_type>) &&
	identity_brace_specification_v<field.specification>;

template <auto format_literal, auto field, typename value_type>
concept brace_builtin_replacement_rule =
	(!brace_direct_identity_rule<format_literal, field, value_type>) &&
	(!brace_custom_replacement_rule<format_literal, field, value_type>) &&
	(!brace_format_as_replacement_rule<format_literal, field, value_type>) &&
	(!brace_chrono_replacement_rule<format_literal, field, value_type>) &&
	(!brace_range_replacement_rule<format_literal, field, value_type>) &&
	(!brace_invalid_tail_replacement_rule<format_literal, field, value_type>) &&
	(!brace_unclaimed_identity_replacement_rule<
		format_literal, field, value_type>) &&
	brace_builtin_value<format_literal, value_type>;

/** Positive value capability for the conversion selected by a percent field. */
template <auto format_literal, auto field, typename value_type>
concept printf_builtin_replacement_rule = []() consteval {
	using clean_type = ::std::remove_cvref_t<value_type>;
	constexpr auto presentation{field.specification.presentation};
	if constexpr (presentation == presentation_type::decimal ||
				  presentation == presentation_type::unsigned_decimal ||
				  presentation == presentation_type::octal ||
				  presentation == presentation_type::hex_lower ||
				  presentation == presentation_type::hex_upper)
	{
		return ::fast_io::details::my_integral<clean_type> ||
			   ::std::same_as<clean_type, ::std::byte> ||
			   ::std::is_enum_v<clean_type>;
	}
	else if constexpr (presentation == presentation_type::character)
	{
		return ::fast_io::details::my_integral<clean_type> ||
			   ::std::same_as<clean_type, ::std::byte>;
	}
	else if constexpr (presentation == presentation_type::string)
	{
		return format_string_like<
			typename decltype(format_literal)::value_type, value_type>;
	}
	else if constexpr (presentation == presentation_type::pointer)
	{
		return ::std::same_as<clean_type, ::std::nullptr_t> ||
			   (::std::is_pointer_v<clean_type> &&
				!::std::is_function_v<::std::remove_pointer_t<clean_type>>);
	}
	else
	{
		return ::fast_io::details::my_floating_point<clean_type>;
	}
}();

} // namespace fast_io::fmt::details

namespace fast_io::fmt
{

/*
 * Phase-one built-in rule selection.
 *
 * Each overload advertises exactly one empty type through fast_io's io_type_t
 * carrier.  Mutually exclusive concepts encode priority in overload
 * viability; the phase-two emitter therefore receives an already selected
 * rule and cannot repeat or diverge from this taxonomy. `value_type` is the
 * stable named-parameter expression supplied by the protocol: it retains cv
 * and array extent while matching the lvalue expression seen by direct
 * fast_io::io::print. ABI-aware alias and transport therefore remain
 * exclusively in the final core backend.
 */
template <auto format_literal, auto field, typename value_type>
	requires ::fast_io::fmt::details::brace_direct_identity_rule_constraint_v<
		format_literal, field, value_type>
[[nodiscard]] consteval auto format_replacement_rule_type(
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<value_type>) noexcept
{
	return ::fast_io::io_type_t<
		::fast_io::fmt::details::brace_direct_identity_format_rule>{};
}

template <auto format_literal, auto field, typename value_type>
	requires ::fast_io::fmt::details::brace_custom_replacement_rule<
		format_literal, field, value_type>
[[nodiscard]] consteval auto format_replacement_rule_type(
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<value_type>) noexcept
{
	return ::fast_io::io_type_t<
		::fast_io::fmt::details::brace_custom_format_rule>{};
}

template <auto format_literal, auto field, typename value_type>
	requires ::fast_io::fmt::details::brace_format_as_replacement_rule<
		format_literal, field, value_type>
[[nodiscard]] consteval auto format_replacement_rule_type(
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<value_type>) noexcept
{
	return ::fast_io::io_type_t<
		::fast_io::fmt::details::brace_format_as_rule>{};
}

template <auto format_literal, auto field, typename value_type>
	requires ::fast_io::fmt::details::brace_chrono_replacement_rule<
		format_literal, field, value_type>
[[nodiscard]] consteval auto format_replacement_rule_type(
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<value_type>) noexcept
{
	return ::fast_io::io_type_t<
		::fast_io::fmt::details::brace_chrono_format_rule>{};
}

template <auto format_literal, auto field, typename value_type>
	requires ::fast_io::fmt::details::brace_range_replacement_rule<
		format_literal, field, value_type>
[[nodiscard]] consteval auto format_replacement_rule_type(
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<value_type>) noexcept
{
	return ::fast_io::io_type_t<
		::fast_io::fmt::details::brace_range_format_rule>{};
}

template <auto format_literal, auto field, typename value_type>
	requires ::fast_io::fmt::details::brace_invalid_tail_replacement_rule<
		format_literal, field, value_type>
[[nodiscard]] consteval auto format_replacement_rule_type(
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<value_type>) noexcept
{
	return ::fast_io::io_type_t<
		::fast_io::fmt::details::brace_invalid_tail_format_rule>{};
}

template <auto format_literal, auto field, typename value_type>
	requires ::fast_io::fmt::details::brace_unclaimed_identity_replacement_rule<
		format_literal, field, value_type>
[[nodiscard]] consteval auto format_replacement_rule_type(
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<value_type>) noexcept
{
	return ::fast_io::io_type_t<
		::fast_io::fmt::details::brace_unclaimed_identity_format_rule>{};
}

template <auto format_literal, auto field, typename value_type>
	requires ::fast_io::fmt::details::brace_builtin_replacement_rule<
		format_literal, field, value_type>
[[nodiscard]] consteval auto format_replacement_rule_type(
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<value_type>) noexcept
{
	return ::fast_io::io_type_t<
		::fast_io::fmt::details::brace_builtin_format_rule>{};
}

template <auto format_literal, auto field, typename value_type>
	requires ::fast_io::fmt::details::printf_builtin_replacement_rule<
		format_literal, field, value_type>
[[nodiscard]] consteval auto format_replacement_rule_type(
	printf_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	::fast_io::io_type_t<value_type>) noexcept
{
	return ::fast_io::io_type_t<
		::fast_io::fmt::details::printf_builtin_format_rule>{};
}

/*
 * Built-in phase-two brace providers.
 *
 * Phase one has already selected exactly one empty rule token, so these
 * overloads need no repeated taxonomy constraints.  A user provider replaces
 * a built-in policy by advertising its own token from the value type's
 * associated namespace; generic lowering then calls only that token's define
 * overload.  Selection priority and emission can therefore never disagree.
 */
template <auto format_literal, auto field, typename value_type,
		  typename argument_pack_type>
[[nodiscard]] inline constexpr decltype(auto) format_replacement_rule_define(
	::fast_io::io_type_t<
		::fast_io::fmt::details::brace_direct_identity_format_rule>,
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	value_type &value, argument_pack_type &)
{
	// Preserve the exact object.  io_print_alias/io_print_forward and any
	// target-ABI value transport still run exactly once at the selected backend.
	return (value);
}

template <auto format_literal, auto field, typename value_type,
		  typename argument_pack_type>
[[nodiscard]] inline constexpr decltype(auto) format_replacement_rule_define(
	::fast_io::io_type_t<::fast_io::fmt::details::brace_custom_format_rule>,
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	value_type &value, argument_pack_type &)
{
	return ::fast_io::fmt::details::make_custom_format_value<format_literal,
															 field.specification.raw_format_specification>(value);
}

template <auto format_literal, auto field, typename value_type,
		  typename argument_pack_type>
[[nodiscard]] inline constexpr decltype(auto) format_replacement_rule_define(
	::fast_io::io_type_t<::fast_io::fmt::details::brace_format_as_rule>,
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	value_type &value, argument_pack_type &)
{
	using char_type = typename decltype(format_literal)::value_type;
	return ::fast_io::fmt::details::make_format_as_value<char_type>(value);
}

template <auto format_literal, auto field, typename value_type,
		  typename argument_pack_type>
[[nodiscard]] inline constexpr decltype(auto) format_replacement_rule_define(
	::fast_io::io_type_t<::fast_io::fmt::details::brace_chrono_format_rule>,
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	value_type &value, argument_pack_type &arguments)
{
	using namespace ::fast_io::fmt::details;
	static_assert(field.specification.sign == format_sign::default_sign &&
					  !field.specification.alternate_form &&
					  !field.specification.zero_padding,
				  "fast_io format: time fields do not accept scalar sign, #, or zero flags");
	static_assert(!field.specification.locale_specific,
				  "fast_io format: locale-specific time formatting requires an explicit locale frontend");
	static_assert(field.specification.precision.kind == format_parameter_kind::none,
				  "fast_io format: precision is not permitted for fast_io time fields");
	auto const width{resolve_format_parameter<format_literal,
											  field.specification.width>(arguments)};
	auto formatted{make_chrono_field<format_literal,
									 field.specification.type_directed_specification>(value)};
	return apply_field_width<field.specification, false, false>(
		::std::move(formatted), width);
}

template <auto format_literal, auto field, typename value_type,
		  typename argument_pack_type>
[[nodiscard]] inline constexpr decltype(auto) format_replacement_rule_define(
	::fast_io::io_type_t<::fast_io::fmt::details::brace_range_format_rule>,
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	value_type &value, argument_pack_type &arguments)
{
	using char_type = typename decltype(format_literal)::value_type;
	constexpr auto specification{
		::fast_io::fmt::details::checked_range_specification_from_common<
			format_literal, field.specification>()};
	return ::fast_io::fmt::details::make_brace_range_view<
		char_type, specification>(value, arguments);
}

template <auto format_literal, auto field, typename value_type,
		  typename argument_pack_type>
[[nodiscard]] inline constexpr auto format_replacement_rule_define(
	::fast_io::io_type_t<
		::fast_io::fmt::details::brace_invalid_tail_format_rule>,
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	value_type &, argument_pack_type &)
{
	::fast_io::fmt::details::diagnose_parse_error<
		::fast_io::fmt::details::format_parse_error::invalid_presentation_type,
		field.specification.format_tail.offset>();
	return ::fast_io::io_null;
}

template <auto format_literal, auto field, typename value_type,
		  typename argument_pack_type>
[[nodiscard]] inline constexpr decltype(auto) format_replacement_rule_define(
	::fast_io::io_type_t<
		::fast_io::fmt::details::brace_unclaimed_identity_format_rule>,
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	value_type &value, argument_pack_type &)
{
	// Let the core report an object which no rule and no print capability owns.
	return (value);
}

template <auto format_literal, auto field, typename value_type,
		  typename argument_pack_type>
[[nodiscard]] inline constexpr decltype(auto) format_replacement_rule_define(
	::fast_io::io_type_t<::fast_io::fmt::details::brace_builtin_format_rule>,
	brace_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	value_type &value, argument_pack_type &arguments)
{
	using char_type = typename decltype(format_literal)::value_type;
	return ::fast_io::fmt::details::make_brace_value<
		char_type, field.specification>(
		value, arguments,
		::fast_io::fmt::details::compile_time_value<format_literal>{});
}

/**
 * Built-in percent-conversion provider.
 *
 * The percent grammar is registered through the same value-rule CPO as the
 * brace grammar; generic lowering never calls `make_printf_value` directly.
 * Consequently a value type's associated namespace may provide a more
 * specialized overload without changing either parser or emitter.  The field
 * is already a structural compile-time value, so this adapter retains no
 * conversion character, flags, or erased formatter at runtime.
 */
template <auto format_literal, auto field, typename value_type,
		  typename argument_pack_type>
[[nodiscard]] inline constexpr decltype(auto) format_replacement_rule_define(
	::fast_io::io_type_t<::fast_io::fmt::details::printf_builtin_format_rule>,
	printf_fmt_t,
	::fast_io::fmt::details::basic_format_replacement_context<
		format_literal, field>,
	value_type &value, argument_pack_type &arguments)
{
	using char_type = typename decltype(format_literal)::value_type;
	return ::fast_io::fmt::details::make_printf_value<char_type, field>(
		value, arguments,
		::fast_io::fmt::details::compile_time_value<format_literal>{});
}

} // namespace fast_io::fmt

#pragma once

/*
 * Typed field-presentation adapters (FMT level).
 *
 * Parsed scalar flags and precision are converted here into concrete
 * manipulator types that reuse fast_io integer, floating, character, pointer,
 * and boolean printable implementations. This is the last value-specific
 * translation step before the IO level; it does not orchestrate a complete
 * operation or call a device CPO.
 */

#include "compile.h"
#include "debug.h"
#include "dynamic.h"
#include "general_float.h"
#include "pattern_width.h"
#include "string.h"

#include <climits>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

// Printable forwarding below must expose the experimental deterministic-error effect without leaking helper macros
// into a translation unit which includes this internal header directly.
#include "../../fast_io_dsal/impl/misc/push_macros.h"

namespace fast_io::fmt::details
{

template <typename...>
inline constexpr bool dependent_false_v{};

/** Inserts printf's alternate-form radix point before an exponent, if any. */
template <::std::integral char_type, ::std::random_access_iterator iterator>
[[nodiscard]] inline constexpr iterator printf_insert_radix_point(
	iterator begin, iterator end, bool active) noexcept
{
	if (!active)
	{
		return end;
	}
	auto position{end};
	for (auto iter{begin}; iter != end; ++iter)
	{
		if (*iter == ::fast_io::char_literal_v<u8'e', char_type> ||
			*iter == ::fast_io::char_literal_v<u8'E', char_type> ||
			*iter == ::fast_io::char_literal_v<u8'p', char_type> ||
			*iter == ::fast_io::char_literal_v<u8'P', char_type>)
		{
			position = iter;
			break;
		}
	}
	for (auto iter{end}; iter != position; --iter)
	{
		*iter = *(iter - 1);
	}
	*position = ::fast_io::char_literal_v<u8'.', char_type>;
	return end + 1;
}

} // namespace fast_io::fmt::details

namespace fast_io::manipulators
{

/// @brief Adds the radix point required by printf's alternate floating spelling.
/// @details The scalar backends intentionally omit a radix point when a fractional precision of zero is requested,
///          whereas printf's `#` flag requires it. The wrapper conditionally inserts exactly one radix code unit before
///          any exponent and otherwise preserves the wrapped scalar's digits, sign, prefix, and sizing semantics.
template <typename value_type>
struct printf_force_radix_t
{
	using manip_tag = ::fast_io::manip_tag_t;
	value_type value;
	bool active{};
};

} // namespace fast_io::manipulators

namespace fast_io
{

/// @brief Propagates a destination-neutral one-pass bound through printf's conditional radix-point insertion.
template <::std::integral char_type, typename value_type>
	requires ::fast_io::single_pass_bounded_materialization_source<
		char_type, value_type>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::printf_force_radix_t<value_type>>) noexcept
{
	return {};
}

/// @brief Propagates print's direct-put-area authorization through conditional radix insertion.
template <::std::integral char_type, typename value_type>
	requires requires {
		{
			print_single_pass_bounded_direct_put_area_safe(
				::fast_io::io_reserve_type<char_type, value_type>)
		} -> ::std::same_as<::std::true_type>;
	}
inline constexpr ::std::true_type print_single_pass_bounded_direct_put_area_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::printf_force_radix_t<value_type>>) noexcept
{
	return {};
}

/// @brief Adds the active radix code unit using the caller's non-fatal remaining budget.
/// @details The bound query is read-only; the wrapped scalar remains available for the subsequent emission pass.
template <::std::integral char_type, typename value_type>
	requires ::fast_io::single_pass_bounded_materialization_source<
		char_type, value_type>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::printf_force_radix_t<value_type>>,
	::fast_io::manipulators::printf_force_radix_t<value_type> const &value,
	::std::size_t maximum_size) noexcept
{
	auto const extra_size{static_cast<::std::size_t>(value.active)};
	if (maximum_size < extra_size)
	{
		return SIZE_MAX;
	}
	auto const remaining{maximum_size - extra_size};
	auto const child_size{
		::fast_io::single_pass_bounded_materialization_size_invoke<char_type>(
			value.value, remaining)};
	if (child_size == SIZE_MAX || remaining < child_size)
	{
		return SIZE_MAX;
	}
	return child_size + extra_size;
}

template <::std::integral char_type, typename value_type>
	requires requires {
		print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>);
	}
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type,
								 ::fast_io::manipulators::printf_force_radix_t<value_type>>) noexcept
{
	// This is a type-static policy. A local constant proves evaluation instead of leaving a hidden run-time call in a
	// plain `noexcept` wrapper when the provider happens to use an experimental effect annotation.
	constexpr ::std::size_t child_size{
		print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>)};
	return child_size + 1u;
}

#if defined(__HERBCEPTIONS__)
namespace details
{

/// @brief Classifies each run-time protocol edge forwarded by `printf_force_radix_t`.
/// @details Radix insertion itself is infallible; the child invocation is therefore the complete deterministic effect
///          of each wrapper. The queried operand categories reproduce the named expressions in the corresponding body,
///          so overload resolution and effect declaration are one formal protocol edge.
template <::std::integral char_type, typename value_type>
inline constexpr bool printf_force_radix_dynamic_size_herbceptions_may_fail =
	throws((print_reserve_size(
		::fast_io::io_reserve_type<char_type, value_type>,
		::std::declval<value_type &>())));

template <::std::integral char_type, typename value_type>
inline constexpr bool printf_force_radix_define_herbceptions_may_fail =
	throws((print_reserve_define(
		::fast_io::io_reserve_type<char_type, value_type>,
		::std::declval<char_type *&>(), ::std::declval<value_type &>())));

template <::std::integral char_type, typename value_type>
inline constexpr bool printf_force_radix_precise_size_herbceptions_may_fail =
	throws((print_reserve_precise_size(
		::fast_io::io_reserve_type<char_type, value_type>,
		::std::declval<value_type &>())));

template <::std::integral char_type, ::std::random_access_iterator iterator,
		  typename value_type>
inline constexpr bool printf_force_radix_precise_define_herbceptions_may_fail =
	throws((print_reserve_precise_define(
		::fast_io::io_reserve_type<char_type, value_type>,
		// The wrapper calls the child with its named iterator and with the const local produced after removing the
		// radix-point slot. These reference categories are part of overload selection and therefore of the effect ABI.
		::std::declval<iterator &>(), ::std::declval<::std::size_t const &>(),
		::std::declval<value_type &>())));

template <::std::integral char_type, typename value_type>
inline constexpr bool printf_force_radix_internal_shift_herbceptions_may_fail =
	throws((print_define_internal_shift(
		::fast_io::io_reserve_type<char_type, value_type>,
		::std::declval<value_type &>())));

} // namespace details
#endif

template <::std::integral char_type, typename value_type>
	requires requires(value_type value) {
		print_reserve_size(::fast_io::io_reserve_type<char_type, value_type>, value);
	}
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type,
								 ::fast_io::manipulators::printf_force_radix_t<value_type>>,
	::fast_io::manipulators::printf_force_radix_t<value_type> value)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::printf_force_radix_dynamic_size_herbceptions_may_fail<
			char_type, value_type>),
		noexcept(print_reserve_size(
			::fast_io::io_reserve_type<char_type, value_type>, value.value)))
{
	// Adding the compulsory radix slot is infallible; the declaration therefore carries exactly the named child size
	// query's deterministic effect and its ordinary exception contract.
	return print_reserve_size(
			   ::fast_io::io_reserve_type<char_type, value_type>, value.value) +
		   1u;
}

template <::std::integral char_type, typename value_type>
	requires requires(value_type value, char_type *iter) {
		print_reserve_define(
			::fast_io::io_reserve_type<char_type, value_type>, iter, value);
	}
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type,
								 ::fast_io::manipulators::printf_force_radix_t<value_type>>,
	char_type *iter,
	::fast_io::manipulators::printf_force_radix_t<value_type> value)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::printf_force_radix_define_herbceptions_may_fail<
			char_type, value_type>),
		noexcept(print_reserve_define(
			::fast_io::io_reserve_type<char_type, value_type>, iter, value.value)))
{
	// The spelling adapter performs only bounded in-place movement; any deterministic failure is exactly the child's
	// named emission channel, so the wrapper declares the same formal effect without changing value transport.
	auto const end{print_reserve_define(
		::fast_io::io_reserve_type<char_type, value_type>, iter, value.value)};
	return ::fast_io::fmt::details::printf_insert_radix_point<char_type>(
		iter, end, value.active);
}

template <::std::integral char_type, typename value_type>
	requires requires {
		print_reserve_static_stack_size(
			::fast_io::io_reserve_type<char_type, value_type>);
	}
inline constexpr ::std::size_t print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<char_type,
								 ::fast_io::manipulators::printf_force_radix_t<value_type>>) noexcept
{
	// This is a type-only capacity policy. Constant evaluation proves that no run-time effectful call is hidden behind
	// the plain wrapper and keeps the result available to array-bound consumers.
	constexpr ::std::size_t child_size{print_reserve_static_stack_size(
		::fast_io::io_reserve_type<char_type, value_type>)};
	return child_size + 1u;
}

template <::std::integral char_type, typename value_type>
	requires requires(value_type value) {
		print_reserve_precise_size(
			::fast_io::io_reserve_type<char_type, value_type>, value);
	}
inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char_type,
								 ::fast_io::manipulators::printf_force_radix_t<value_type>>,
	::fast_io::manipulators::printf_force_radix_t<value_type> value)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::printf_force_radix_precise_size_herbceptions_may_fail<
			char_type, value_type>),
		noexcept(print_reserve_precise_size(
			::fast_io::io_reserve_type<char_type, value_type>, value.value)))
{
	// The active-radix adjustment is infallible and does not inspect the child, so this wrapper's two failure channels
	// are exactly those of the named precise-size query.
	return print_reserve_precise_size(
			   ::fast_io::io_reserve_type<char_type, value_type>, value.value) +
		   static_cast<::std::size_t>(value.active);
}

template <::std::integral char_type, ::std::random_access_iterator iterator,
		  typename value_type>
	requires requires(value_type value, iterator iter) {
		print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, value_type>, iter,
			::std::declval<::std::size_t const &>(), value);
	}
inline constexpr decltype(auto) print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char_type,
								 ::fast_io::manipulators::printf_force_radix_t<value_type>>,
	iterator iter, ::std::size_t size,
	::fast_io::manipulators::printf_force_radix_t<value_type> value)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::printf_force_radix_precise_define_herbceptions_may_fail<
			char_type, iterator, value_type>),
		noexcept(print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, value_type>, iter,
			::std::declval<::std::size_t const &>(), value.value)))
{
	// `inner_size` is a named const lvalue in the executed call; the constraint, ordinary exception proof, and
	// Herbception predicate all model that exact category. This prevents an rvalue-only decoy overload from changing
	// admission or the declared effect while preserving the child's `void` or endpoint result domain.
	auto const inner_size{size - static_cast<::std::size_t>(value.active)};
	using define_result = decltype(print_reserve_precise_define(
		::fast_io::io_reserve_type<char_type, value_type>, iter,
		inner_size, value.value));
	if constexpr (::std::same_as<define_result, void>)
	{
		print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, value_type>, iter,
			inner_size, value.value);
		(void)::fast_io::fmt::details::printf_insert_radix_point<char_type>(
			iter, iter + inner_size, value.active);
	}
	else
	{
		auto end{print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, value_type>, iter,
			inner_size, value.value)};
		return ::fast_io::fmt::details::printf_insert_radix_point<char_type>(
			iter, end, value.active);
	}
}

template <::std::integral char_type, typename value_type>
	requires requires(value_type value) {
		print_define_internal_shift(
			::fast_io::io_reserve_type<char_type, value_type>, value);
	}
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char_type,
								 ::fast_io::manipulators::printf_force_radix_t<value_type>>,
	::fast_io::manipulators::printf_force_radix_t<value_type> value)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::printf_force_radix_internal_shift_herbceptions_may_fail<
			char_type, value_type>),
		noexcept(print_define_internal_shift(
			::fast_io::io_reserve_type<char_type, value_type>, value.value)))
{
	// Radix insertion does not alter the child's internal-padding origin; the transparent query propagates the named
	// child expression's deterministic effect and ordinary exception property unchanged.
	return print_define_internal_shift(
		::fast_io::io_reserve_type<char_type, value_type>, value.value);
}

} // namespace fast_io

namespace fast_io::fmt::details
{

struct resolved_format_parameter
{
	::std::size_t value{};
	bool present{};
	bool negative{};
};

template <auto format_literal, format_parameter parameter,
		  ::std::size_t... index, typename... argument_types>
[[nodiscard]] inline constexpr resolved_format_parameter resolve_format_parameter(
	indexed_argument_pack<::std::index_sequence<index...>, argument_types...> &arguments)
{
	if constexpr (parameter.kind == format_parameter_kind::none)
	{
		return {};
	}
	else if constexpr (parameter.kind == format_parameter_kind::literal)
	{
		static_assert(parameter.value <= static_cast<::std::size_t>(INT_MAX),
					  "fast_io format: literal width/precision exceeds INT_MAX");
		return {parameter.value, true, false};
	}
	else
	{
		constexpr auto resolution{
			resolve_argument_reference<format_literal, parameter.argument, argument_types...>()};
		if constexpr (resolution.error != argument_resolution_error::none)
		{
			diagnose_argument_resolution<resolution.error, parameter.argument.name.offset>();
		}
		auto &holder{indexed_argument_get<resolution.index>(arguments)};
		decltype(auto) value{unwrap_static_named_argument(holder)};
		using value_type = ::std::remove_cvref_t<decltype(value)>;
		static_assert(dynamic_format_integer_type_v<value_type>,
					  "fast_io format: dynamic width/precision requires a non-character integer argument");
		auto const checked{checked_dynamic_integer(value)};
		return {checked.magnitude, true, checked.negative};
	}
}

template <format_specification specification, bool numeric, bool printf_dialect>
[[nodiscard]] inline consteval ::fast_io::manipulators::scalar_placement
static_field_placement() noexcept
{
	if constexpr (specification.alignment == format_alignment::left)
	{
		return ::fast_io::manipulators::scalar_placement::left;
	}
	else if constexpr (specification.alignment == format_alignment::right)
	{
		return ::fast_io::manipulators::scalar_placement::right;
	}
	else if constexpr (specification.alignment == format_alignment::center)
	{
		return ::fast_io::manipulators::scalar_placement::middle;
	}
	else if constexpr (specification.zero_padding && numeric)
	{
		return ::fast_io::manipulators::scalar_placement::internal;
	}
	else if (printf_dialect || numeric)
	{
		return ::fast_io::manipulators::scalar_placement::right;
	}
	else
	{
		return ::fast_io::manipulators::scalar_placement::left;
	}
}

template <format_specification specification, bool numeric, bool printf_dialect,
		  typename value_type>
[[nodiscard]] inline constexpr auto apply_field_width(
	value_type &&value, resolved_format_parameter width)
{
	if constexpr (specification.width.kind == format_parameter_kind::none)
	{
		return ::std::forward<value_type>(value);
	}
	else
	{
		constexpr auto base_placement{
			static_field_placement<specification, numeric, printf_dialect>()};
		if constexpr (specification.fill_size > 1u)
		{
			static_assert(!printf_dialect,
						  "fast_io format: printf uses a one-code-unit space or zero fill");
			if (width.negative) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			using fill_char_type =
				::std::remove_cvref_t<decltype(specification.fill[0])>;
			return make_pattern_width<specification.fill_size, fill_char_type>(
				::std::forward<value_type>(value), width.value, base_placement,
				specification.fill);
		}
		else
		{
			auto const fill_character = []() constexpr {
				using fill_char_type = ::std::remove_cvref_t<decltype(specification.fill[0])>;
				if constexpr (specification.has_fill)
				{
					return specification.fill[0];
				}
				else if constexpr (specification.zero_padding && numeric &&
								   specification.alignment == format_alignment::none)
				{
					return ::fast_io::char_literal_v<u8'0', fill_char_type>;
				}
				else
				{
					return ::fast_io::char_literal_v<u8' ', fill_char_type>;
				}
			}();

			if constexpr (printf_dialect && specification.width.kind == format_parameter_kind::argument)
			{
				auto placement{base_placement};
				if (width.negative)
				{
					placement = ::fast_io::manipulators::scalar_placement::left;
				}
				return ::fast_io::manipulators::width(
					placement, ::std::forward<value_type>(value), width.value, fill_character);
			}
			else
			{
				if (width.negative) [[unlikely]]
				{
					::fast_io::fast_terminate();
				}
				if constexpr (base_placement == ::fast_io::manipulators::scalar_placement::left)
				{
					return ::fast_io::manipulators::left(
						::std::forward<value_type>(value), width.value, fill_character);
				}
				else if constexpr (base_placement == ::fast_io::manipulators::scalar_placement::middle)
				{
					return ::fast_io::manipulators::middle(
						::std::forward<value_type>(value), width.value, fill_character);
				}
				else if constexpr (base_placement == ::fast_io::manipulators::scalar_placement::internal)
				{
					return ::fast_io::manipulators::internal(
						::std::forward<value_type>(value), width.value, fill_character);
				}
				else
				{
					return ::fast_io::manipulators::right(
						::std::forward<value_type>(value), width.value, fill_character);
				}
			}
		}
	}
}

template <format_specification specification>
[[nodiscard]] inline consteval ::fast_io::manipulators::scalar_flags integer_scalar_flags(
	bool showbase) noexcept
{
	::fast_io::manipulators::scalar_flags flags{};
	if constexpr (specification.presentation == presentation_type::binary_lower ||
				  specification.presentation == presentation_type::binary_upper)
	{
		flags.base = 2u;
	}
	else if constexpr (specification.presentation == presentation_type::octal)
	{
		flags.base = 8u;
	}
	else if constexpr (specification.presentation == presentation_type::hex_lower ||
					   specification.presentation == presentation_type::hex_upper)
	{
		flags.base = 16u;
	}
	else
	{
		flags.base = 10u;
	}

	constexpr bool uppercase{
		specification.presentation == presentation_type::binary_upper ||
		specification.presentation == presentation_type::hex_upper};
	// The caller resolves value-sensitive alternate octal before constructing this
	// flag set.  Re-reading `specification.alternate_form` here would make the
	// zero-value no-prefix arm indistinguishable and spell zero as `00`.
	flags.showbase = showbase;
	flags.showpos = specification.sign == format_sign::plus ||
					specification.sign == format_sign::space;
	flags.uppercase_showbase = uppercase;
	flags.uppercase = uppercase;
	return flags;
}

template <format_specification specification, bool showbase, typename value_type>
[[nodiscard]] inline constexpr auto make_integer_scalar(value_type value)
{
	constexpr auto flags{integer_scalar_flags<specification>(showbase)};
	auto scalar{::fast_io::details::scalar_flags_int_cache<flags>(value)};
	constexpr ::std::size_t prefix_size{
		showbase && flags.base != 10u ? (flags.base == 8u ? 1u : 2u) : 0u};
	constexpr bool internal_prefix_shift_required{
		prefix_size != 0u &&
		specification.width.kind != format_parameter_kind::none &&
		specification.zero_padding &&
		specification.alignment == format_alignment::none};
	constexpr bool space_sign_required{
		specification.sign == format_sign::space};
	if constexpr (internal_prefix_shift_required || space_sign_required)
	{
		// The wrapper exists only for semantics absent from the native scalar:
		// base-prefix-aware internal padding and replacement of an emitted plus
		// by a space.  For ordinary d/x/X/o/b fields, return the native scalar
		// type itself.  This makes lowering type-identical to mnp::dec/hex/
		// hexupper/oct/bin and leaves the core print/concat strategy to perform
		// its normal ABI decay without an avoidable format-layer proxy.
		return ::fast_io::manipulators::format_scalar_t<
			decltype(scalar), prefix_size, space_sign_required>{scalar};
	}
	else
	{
		return scalar;
	}
}

template <format_specification specification, typename value_type>
[[nodiscard]] inline constexpr auto make_brace_integer(value_type value)
{
	static_assert(specification.precision.kind == format_parameter_kind::none,
				  "fast_io format: brace integer formatting does not accept precision");
	static_assert(!specification.locale_specific,
				  "fast_io format: locale-specific integer formatting requires an explicit locale overload");

	if constexpr (specification.alternate_form &&
				  specification.presentation == presentation_type::octal)
	{
		// fmt's legacy octal alternate spelling adds one leading zero only when the
		// value itself is nonzero.  Encoding this as a semantic condition also lets
		// core width query the selected branch's exact internal shift.
		auto with_prefix{make_integer_scalar<specification, true>(value)};
		auto without_prefix{make_integer_scalar<specification, false>(value)};
		return ::fast_io::manipulators::cond(
			value != 0, ::std::move(with_prefix), ::std::move(without_prefix));
	}
	else
	{
		return make_integer_scalar<specification, specification.alternate_form>(value);
	}
}

/**
 * Implements fmt's integer-to-character domain check without an exception
 * channel.  `:c` treats the destination code-unit type as unsigned, so a
 * negative source or a value wider than that unsigned domain is invalid.  The
 * branch is value semantics, not format parsing; constant arguments remove it
 * completely and a failing runtime value follows fast_io's fail-fast policy.
 */
template <::fast_io::fmt::format_character char_type, typename value_type>
[[nodiscard]] inline constexpr bool brace_character_value_in_range(
	value_type value) noexcept
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	using output_unsigned_type = ::std::make_unsigned_t<char_type>;
	constexpr auto output_maximum{
		(::std::numeric_limits<output_unsigned_type>::max)()};
	if constexpr (::std::same_as<clean_type, ::std::byte>)
	{
		return static_cast<unsigned int>(value) <= output_maximum;
	}
	else
	{
		if constexpr (::fast_io::details::my_signed_integral<clean_type>)
		{
			if (value < 0)
			{
				return false;
			}
		}
		using input_unsigned_type =
			::fast_io::details::my_make_unsigned_t<clean_type>;
		if constexpr ((::std::numeric_limits<input_unsigned_type>::digits) <=
					  (::std::numeric_limits<output_unsigned_type>::digits))
		{
			return true;
		}
		else
		{
			return static_cast<input_unsigned_type>(value) <=
				   static_cast<input_unsigned_type>(output_maximum);
		}
	}
}

template <format_specification specification, typename pointer_type>
[[nodiscard]] inline constexpr auto make_pointer_scalar(pointer_type value)
{
	static_assert(specification.presentation == presentation_type::none ||
					  specification.presentation == presentation_type::pointer,
				  "fast_io format: invalid pointer presentation type");
	static_assert(specification.precision.kind == format_parameter_kind::none &&
					  !specification.alternate_form &&
					  (specification.sign == format_sign::default_sign || specification.sign == format_sign::minus) &&
					  !specification.locale_specific,
				  "fast_io format: invalid pointer format specification");
	constexpr ::fast_io::manipulators::scalar_flags flags{
		.base = 16u, .showbase = true};
	auto scalar{::fast_io::details::scalar_flags_int_cache<flags>(value)};
	if constexpr (specification.width.kind != format_parameter_kind::none &&
				  specification.zero_padding &&
				  specification.alignment == format_alignment::none)
	{
		// Native scalar metadata does not include a base prefix in its internal
		// shift, so the zero-padded form needs the two-code-unit proof wrapper.
		return ::fast_io::manipulators::format_scalar_t<
			decltype(scalar), 2u, false>{scalar};
	}
	else
	{
		// Unpadded pointers are exactly the native base-16/showbase manipulator.
		return scalar;
	}
}

template <format_specification specification, bool with_precision>
[[nodiscard]] inline consteval ::fast_io::manipulators::scalar_flags
floating_scalar_flags() noexcept
{
	::fast_io::manipulators::scalar_flags flags{};
	constexpr auto presentation{specification.presentation};
	if constexpr (presentation == presentation_type::hexfloat_lower ||
				  presentation == presentation_type::hexfloat_upper)
	{
		flags.floating = ::fast_io::manipulators::floating_format::hexfloat;
		// fmt's a/A spelling always includes 0x/0X; '#' controls the radix point.
		flags.showbase = true;
	}
	else if constexpr (presentation == presentation_type::scientific_lower ||
					   presentation == presentation_type::scientific_upper)
	{
		flags.floating = ::fast_io::manipulators::floating_format::scientific;
	}
	else if constexpr (presentation == presentation_type::fixed_lower ||
					   presentation == presentation_type::fixed_upper)
	{
		flags.floating = ::fast_io::manipulators::floating_format::fixed;
	}
	else if constexpr (presentation == presentation_type::general_lower ||
					   presentation == presentation_type::general_upper || with_precision)
	{
		flags.floating = ::fast_io::manipulators::floating_format::general;
	}
	else
	{
		flags.floating = ::fast_io::manipulators::floating_format::decimal;
	}

	constexpr bool uppercase{
		presentation == presentation_type::hexfloat_upper ||
		presentation == presentation_type::scientific_upper ||
		presentation == presentation_type::fixed_upper ||
		presentation == presentation_type::general_upper};
	flags.uppercase = uppercase;
	flags.uppercase_e = uppercase;
	flags.uppercase_showbase = uppercase;
	flags.showpos = specification.sign == format_sign::plus ||
					specification.sign == format_sign::space;
	if (with_precision)
	{
		constexpr bool fractional{
			presentation == presentation_type::hexfloat_lower ||
			presentation == presentation_type::hexfloat_upper ||
			presentation == presentation_type::scientific_lower ||
			presentation == presentation_type::scientific_upper ||
			presentation == presentation_type::fixed_lower ||
			presentation == presentation_type::fixed_upper};
		if constexpr (fractional)
		{
			// f/e/a precision denotes an exact number of fractional digits in both
			// brace and printf grammars.  Trimming those digits is never permitted;
			// `#` only adds the radix point when that exact count is zero.
			flags.precision =
				::fast_io::manipulators::floating_precision::fractional_preserve_trailing_zero;
		}
		else
		{
			flags.precision = specification.alternate_form
								  ? ::fast_io::manipulators::floating_precision::significant_preserve_trailing_zero
								  : ::fast_io::manipulators::floating_precision::significant;
		}
	}
	return flags;
}

template <format_specification specification, typename value_type>
[[nodiscard]] inline constexpr auto make_brace_floating(
	value_type &&value, resolved_format_parameter precision)
{
	static_assert(!specification.locale_specific,
				  "fast_io format: locale-specific floating formatting requires an explicit locale overload");
	if (precision.negative) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}

	constexpr bool presentation_has_default_six{
		specification.presentation == presentation_type::scientific_lower ||
		specification.presentation == presentation_type::scientific_upper ||
		specification.presentation == presentation_type::fixed_lower ||
		specification.presentation == presentation_type::fixed_upper ||
		specification.presentation == presentation_type::general_lower ||
		specification.presentation == presentation_type::general_upper};
	constexpr bool use_precision{
		specification.precision.kind != format_parameter_kind::none || presentation_has_default_six};
	if constexpr (use_precision)
	{
		auto count{precision.present ? precision.value : 6u};
		if constexpr (specification.presentation == presentation_type::general_lower ||
					  specification.presentation == presentation_type::general_upper)
		{
			if (count == 0u)
			{
				count = 1u;
			}
			// The core general formatter intentionally uses a type-wide notation
			// window.  The brace grammar follows printf instead: the exponent of
			// the rounded result selects scientific notation when exp < -4 or
			// exp >= precision.  Two typed scalar leaves preserve the ordinary
			// fast_io reserve path; general_float_t normally writes only the likely
			// candidate and retries solely at a rounding boundary.
			constexpr auto fixed_flags = []() consteval {
				auto result{floating_scalar_flags<specification, true>()};
				result.floating = ::fast_io::manipulators::floating_format::fixed;
				return result;
			}();
			constexpr auto scientific_flags = []() consteval {
				auto result{floating_scalar_flags<specification, true>()};
				result.floating = ::fast_io::manipulators::floating_format::scientific;
				return result;
			}();
			auto fixed_scalar{
				::fast_io::details::make_floating_scalar_manip_precision<
					fixed_flags>(value, count)};
			auto scientific_scalar{
				::fast_io::details::make_floating_scalar_manip_precision<
					scientific_flags>(value, count)};
			auto fixed{::fast_io::manipulators::format_scalar_t<
				decltype(fixed_scalar), 0u, specification.sign == format_sign::space>{
				::std::move(fixed_scalar)}};
			auto scientific{::fast_io::manipulators::format_scalar_t<
				decltype(scientific_scalar), 0u, specification.sign == format_sign::space>{
				::std::move(scientific_scalar)}};
			return make_general_float<specification.alternate_form>(
				::std::move(fixed), ::std::move(scientific), count);
		}
		else
		{
			constexpr auto flags{floating_scalar_flags<specification, true>()};
			auto scalar{
				::fast_io::details::make_floating_scalar_manip_precision<flags>(
					value, count)};
			constexpr ::std::size_t prefix_size{
				flags.floating == ::fast_io::manipulators::floating_format::hexfloat ? 2u : 0u};
			auto formatted{::fast_io::manipulators::format_scalar_t<
				decltype(scalar), prefix_size, specification.sign == format_sign::space>{scalar}};
			if constexpr (specification.alternate_form)
			{
				// The scalar's preserve policy supplies trailing zeroes, but a zero
				// fractional precision still needs the grammar-mandated radix point.
				return ::fast_io::manipulators::printf_force_radix_t<decltype(formatted)>{
					::std::move(formatted), count == 0u &&
												::fast_io::fmt::details::scalar_finite(value)};
			}
			else
			{
				return formatted;
			}
		}
	}
	else
	{
		constexpr auto flags{floating_scalar_flags<specification, false>()};
		auto scalar{
			::fast_io::details::make_floating_scalar_manip<flags>(value)};
		if constexpr (specification.presentation == presentation_type::none &&
					  specification.width.kind == format_parameter_kind::none &&
					  specification.sign == format_sign::default_sign &&
					  !specification.has_fill &&
					  specification.alignment == format_alignment::none &&
					  !specification.alternate_form && !specification.zero_padding &&
					  !specification.locale_specific)
		{
			// A plain `{}` floating field is exactly the core scalar protocol.  The
			// format wrapper can only (1) add a base-prefix contribution to internal
			// zero padding or (2) replace a leading plus by a space.  This branch has
			// neither width/zero padding nor a space sign, and the default decimal
			// presentation has a zero-size base prefix; therefore its reserve bound
			// and every emitted code unit are identical to `scalar`.  Returning the
			// native leaf also prevents a format-only forwarding CPO from becoming an
			// inlining boundary for compiler-constant floating materialization.
			return scalar;
		}
		else
		{
			constexpr ::std::size_t prefix_size{
				flags.floating == ::fast_io::manipulators::floating_format::hexfloat ? 2u : 0u};
			auto formatted{::fast_io::manipulators::format_scalar_t<
				decltype(scalar), prefix_size, specification.sign == format_sign::space>{scalar}};
			if constexpr (specification.alternate_form)
			{
				// Decimal-shortest and precision-less hexadecimal output omit an
				// otherwise redundant point; brace `#` makes it observable.
				return ::fast_io::manipulators::printf_force_radix_t<decltype(formatted)>{
					::std::move(formatted),
					::fast_io::fmt::details::scalar_finite(value)};
			}
			else
			{
				return formatted;
			}
		}
	}
}

template <typename char_type, format_specification specification, typename value_type>
[[nodiscard]] inline constexpr auto make_brace_text(
	value_type &value, resolved_format_parameter width,
	resolved_format_parameter precision)
{
	static_assert(!specification.alternate_form &&
					  specification.sign == format_sign::default_sign &&
					  !specification.zero_padding && !specification.locale_specific,
				  "fast_io format: invalid string format flag");
	static_assert(specification.presentation == presentation_type::none ||
					  specification.presentation == presentation_type::string ||
					  specification.presentation == presentation_type::debug,
				  "fast_io format: invalid string presentation type");
	if (width.negative) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	if (precision.negative) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	auto const source{make_string_scatter<char_type>(value)};
	if constexpr (specification.presentation != presentation_type::debug &&
				  specification.width.kind == format_parameter_kind::none &&
				  specification.precision.kind == format_parameter_kind::none)
	{
		// An explicit `s` with no semantic modifiers is still the identity string
		// path.  Returning the scatter directly keeps it eligible for the core's
		// scatter coalescing instead of paying a Unicode measurement pass.
		return source;
	}
	else
	{
		// The finite-array descriptor intentionally stays a scatter on the ordinary path,
		// but template argument deduction for the text-field factories does not consider
		// its conversion operator.  Project it explicitly only after we know that width,
		// precision, or debug processing needs the field wrapper.
		auto const source_scatter{
			static_cast<::fast_io::basic_io_scatter_t<char_type>>(source)};
		constexpr auto placement{
			static_field_placement<specification, false, false>()};
		auto const options{make_text_field_options<char_type>(
			precision.present ? precision.value : SIZE_MAX,
			width.present ? width.value : 0u, placement,
			specification.has_fill ? specification.fill : nullptr,
			specification.has_fill ? specification.fill_size : 0u)};
		if constexpr (specification.presentation == presentation_type::debug)
		{
			return make_debug_string_field(source_scatter, options);
		}
		else
		{
			return make_unicode_text_field(source_scatter, options);
		}
	}
}

template <typename char_type, format_specification specification,
		  ::std::size_t... index, typename... argument_types, typename value_type>
[[nodiscard]] inline constexpr auto make_brace_value(
	value_type &value,
	indexed_argument_pack<::std::index_sequence<index...>, argument_types...> &arguments,
	auto format_literal_tag)
{
	constexpr auto format_literal{decltype(format_literal_tag)::value};
	auto const width{resolve_format_parameter<format_literal, specification.width>(arguments)};
	auto const precision{resolve_format_parameter<format_literal, specification.precision>(arguments)};
	using clean_type = ::std::remove_cvref_t<value_type>;

	if constexpr (::std::same_as<clean_type, bool> &&
				  (specification.presentation == presentation_type::none ||
				   specification.presentation == presentation_type::string))
	{
		static_assert(specification.precision.kind == format_parameter_kind::none &&
						  !specification.alternate_form &&
						  specification.sign == format_sign::default_sign &&
						  !specification.zero_padding && !specification.locale_specific,
					  "fast_io format: invalid bool text format specification");
		auto formatted{::fast_io::manipulators::boolalpha(value)};
		return apply_field_width<specification, false, false>(::std::move(formatted), width);
	}
	else if constexpr (::fast_io::details::character_integral<clean_type> &&
					   (specification.presentation == presentation_type::none ||
						specification.presentation == presentation_type::character ||
						specification.presentation == presentation_type::debug))
	{
		static_assert(::std::same_as<clean_type, char_type>,
					  "fast_io format: character argument and format string must use the same character type");
		static_assert(specification.precision.kind == format_parameter_kind::none &&
						  !specification.alternate_form &&
						  specification.sign == format_sign::default_sign &&
						  !specification.zero_padding && !specification.locale_specific,
					  "fast_io format: invalid character format specification");
		if constexpr (specification.presentation == presentation_type::debug)
		{
			if (width.negative) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			constexpr auto placement{
				static_field_placement<specification, false, false>()};
			auto const options{make_text_field_options<char_type>(
				SIZE_MAX, width.present ? width.value : 0u, placement,
				specification.has_fill ? specification.fill : nullptr,
				specification.has_fill ? specification.fill_size : 0u)};
			return make_debug_character_field(value, options);
		}
		else
		{
			if constexpr (specification.width.kind != format_parameter_kind::none)
			{
				if (width.negative) [[unlikely]]
				{
					::fast_io::fast_terminate();
				}
				constexpr auto placement{
					static_field_placement<specification, false, false>()};
				auto const options{make_text_field_options<char_type>(
					SIZE_MAX, width.value, placement,
					specification.has_fill ? specification.fill : nullptr,
					specification.has_fill ? specification.fill_size : 0u)};
				return make_unicode_text_field(
					::fast_io::basic_io_scatter_t<char_type>{
						__builtin_addressof(value), 1u},
					options);
			}
			else
			{
				return ::fast_io::manipulators::chvw(value);
			}
		}
	}
	else if constexpr (format_string_like<char_type, value_type> &&
					   specification.presentation != presentation_type::pointer)
	{
		return make_brace_text<char_type, specification>(value, width, precision);
	}
	else if constexpr (::std::same_as<clean_type, ::std::nullptr_t> ||
					   (::std::is_pointer_v<clean_type> &&
						!::std::is_function_v<::std::remove_pointer_t<clean_type>>))
	{
		static_assert(specification.presentation == presentation_type::pointer ||
						  (::std::same_as<clean_type, ::std::nullptr_t> ||
						   ::std::same_as<::std::remove_cv_t<::std::remove_pointer_t<clean_type>>, void>),
					  "fast_io format: typed pointers require the explicit p presentation");
		auto formatted{make_pointer_scalar<specification>(value)};
		return apply_field_width<specification, true, false>(::std::move(formatted), width);
	}
	else if constexpr (::fast_io::details::my_floating_point<clean_type>)
	{
		static_assert(specification.presentation == presentation_type::none ||
						  specification.presentation == presentation_type::hexfloat_lower ||
						  specification.presentation == presentation_type::hexfloat_upper ||
						  specification.presentation == presentation_type::scientific_lower ||
						  specification.presentation == presentation_type::scientific_upper ||
						  specification.presentation == presentation_type::fixed_lower ||
						  specification.presentation == presentation_type::fixed_upper ||
						  specification.presentation == presentation_type::general_lower ||
						  specification.presentation == presentation_type::general_upper,
					  "fast_io format: invalid floating presentation type");
		auto formatted{make_brace_floating<specification>(value, precision)};
		return apply_field_width<specification, true, false>(::std::move(formatted), width);
	}
	else if constexpr (::fast_io::details::my_integral<clean_type> ||
					   ::std::same_as<clean_type, ::std::byte>)
	{
		if constexpr (specification.presentation == presentation_type::character)
		{
			static_assert(!::std::same_as<clean_type, bool>,
						  "fast_io format: bool does not support the character presentation");
			static_assert(!specification.alternate_form &&
							  specification.sign == format_sign::default_sign &&
							  !specification.zero_padding && specification.precision.kind == format_parameter_kind::none,
						  "fast_io format: invalid integer character format specification");
			if (!brace_character_value_in_range<char_type>(value)) [[unlikely]]
			{
				::fast_io::fast_terminate();
			}
			auto formatted{::fast_io::manipulators::chvw(static_cast<char_type>(value))};
			return apply_field_width<specification, false, false>(::std::move(formatted), width);
		}
		else
		{
			constexpr bool accepts_sign{
				::fast_io::details::my_signed_integral<clean_type> &&
				!::fast_io::details::character_integral<clean_type>};
			static_assert(accepts_sign ||
							  specification.sign == format_sign::default_sign,
						  "fast_io format: a sign option requires a signed integer argument");
			static_assert(specification.presentation == presentation_type::none ||
							  specification.presentation == presentation_type::binary_lower ||
							  specification.presentation == presentation_type::binary_upper ||
							  specification.presentation == presentation_type::decimal ||
							  specification.presentation == presentation_type::octal ||
							  specification.presentation == presentation_type::hex_lower ||
							  specification.presentation == presentation_type::hex_upper,
						  "fast_io format: invalid integer presentation type");
			if constexpr (::fast_io::details::character_integral<clean_type>)
			{
				// fmt defines numeric character formatting through the code unit's
				// unsigned representation, independent of plain-char signedness.
				using unsigned_type = ::std::make_unsigned_t<clean_type>;
				auto formatted{make_brace_integer<specification>(
					static_cast<unsigned_type>(value))};
				return apply_field_width<specification, true, false>(
					::std::move(formatted), width);
			}
			else
			{
				auto formatted{make_brace_integer<specification>(value)};
				return apply_field_width<specification, true, false>(
					::std::move(formatted), width);
			}
		}
	}
	else
	{
		static_assert(specification.presentation == presentation_type::none &&
						  specification.precision.kind == format_parameter_kind::none &&
						  !specification.alternate_form &&
						  specification.sign == format_sign::default_sign &&
						  !specification.zero_padding && !specification.locale_specific,
					  "fast_io format: custom fast_io printable accepts only default/width formatting without a format CPO");
		return apply_field_width<specification, false, false>(value, width);
	}
}

/**
 * Applies printf's field width after the conversion-specific representation is
 * known.  Integer precision is deliberately a template property here: a
 * nonnegative integer precision disables the `0` flag, whereas a negative `*`
 * precision is treated as omitted and therefore restores it.  Keeping the two
 * cases as condition arms avoids a run-time format switch while preserving the
 * exact internal sign/prefix shift supplied by the scalar backend.
 */
template <format_specification specification, bool numeric,
		  bool integer_precision_active, typename value_type>
[[nodiscard]] inline constexpr auto apply_printf_width(
	value_type &&value, resolved_format_parameter width)
{
	if constexpr (specification.width.kind == format_parameter_kind::none)
	{
		return ::std::forward<value_type>(value);
	}
	else
	{
		using fill_char_type =
			::std::remove_cvref_t<decltype(specification.fill[0])>;
		constexpr auto space{::fast_io::char_literal_v<u8' ', fill_char_type>};
		constexpr auto zero{::fast_io::char_literal_v<u8'0', fill_char_type>};
		constexpr bool left_aligned{
			specification.alignment == format_alignment::left};
		constexpr bool zero_fill{
			specification.zero_padding && numeric && !left_aligned &&
			!integer_precision_active};
		constexpr auto base_placement = left_aligned
											? ::fast_io::manipulators::scalar_placement::left
											: (zero_fill ? ::fast_io::manipulators::scalar_placement::internal
														 : ::fast_io::manipulators::scalar_placement::right);

		if constexpr (specification.width.kind == format_parameter_kind::argument)
		{
			auto placement{base_placement};
			auto fill{zero_fill ? zero : space};
			if (width.negative)
			{
				placement = ::fast_io::manipulators::scalar_placement::left;
				fill = space;
			}
			return ::fast_io::manipulators::width(
				placement, ::std::forward<value_type>(value), width.value, fill);
		}
		else if constexpr (base_placement ==
						   ::fast_io::manipulators::scalar_placement::left)
		{
			return ::fast_io::manipulators::left(
				::std::forward<value_type>(value), width.value, space);
		}
		else if constexpr (base_placement ==
						   ::fast_io::manipulators::scalar_placement::internal)
		{
			return ::fast_io::manipulators::internal(
				::std::forward<value_type>(value), width.value, zero);
		}
		else
		{
			return ::fast_io::manipulators::right(
				::std::forward<value_type>(value), width.value, space);
		}
	}
}

/**
 * Models the language-level integer promotions used at a variadic boundary.
 *
 * The ordinary character and short integer types have conversion rank no
 * greater than `int`; the distinct character code-unit types use the same
 * "first standard integer type that represents every value" rule.  Spelling
 * that rule in terms of `numeric_limits::digits` also keeps this frontend
 * correct on ABIs where `wchar_t`, `short`, or `int` have non-mainstream
 * widths.  Types whose rank is already at least that of `int` never enter this
 * trait, so this machinery cannot accidentally narrow `long`, `long long`, or
 * an implementation-provided wider integer.
 */
template <typename value_type>
inline constexpr bool printf_integer_promotion_candidate_v =
	::std::same_as<value_type, bool> ||
	::std::same_as<value_type, char> ||
	::std::same_as<value_type, signed char> ||
	::std::same_as<value_type, unsigned char> ||
	::std::same_as<value_type, short> ||
	::std::same_as<value_type, unsigned short> ||
	::std::same_as<value_type, wchar_t> ||
	::std::same_as<value_type, char8_t> ||
	::std::same_as<value_type, char16_t> ||
	::std::same_as<value_type, char32_t>;

template <typename value_type,
		  bool signed_source = ::fast_io::details::my_signed_integral<value_type>>
struct printf_integer_promoted_type;

template <typename value_type>
struct printf_integer_promoted_type<value_type, true>
{
	static constexpr int digits{::std::numeric_limits<value_type>::digits};
	using type = ::std::conditional_t<
		(digits <= ::std::numeric_limits<int>::digits), int,
		::std::conditional_t<
			(digits <= ::std::numeric_limits<long>::digits), long,
			::std::conditional_t<
				(digits <= ::std::numeric_limits<long long>::digits), long long,
				value_type>>>;
};

template <typename value_type>
struct printf_integer_promoted_type<value_type, false>
{
	static constexpr int digits{::std::numeric_limits<value_type>::digits};
	using type = ::std::conditional_t<
		(digits <= ::std::numeric_limits<int>::digits), int,
		::std::conditional_t<
			(digits <= ::std::numeric_limits<unsigned int>::digits), unsigned int,
			::std::conditional_t<
				(digits <= ::std::numeric_limits<long>::digits), long,
				::std::conditional_t<
					(digits <= ::std::numeric_limits<unsigned long>::digits), unsigned long,
					::std::conditional_t<
						(digits <= ::std::numeric_limits<long long>::digits), long long,
						::std::conditional_t<
							(digits <= ::std::numeric_limits<unsigned long long>::digits),
							unsigned long long, value_type>>>>>>;
};

template <bool signed_conversion, typename value_type>
[[nodiscard]] inline constexpr auto printf_default_integer_cast(
	value_type value) noexcept
{
	using clean_type = ::std::remove_cv_t<value_type>;
	if constexpr (printf_integer_promotion_candidate_v<clean_type>)
	{
		using promoted_type = typename printf_integer_promoted_type<clean_type>::type;
		using target_type = ::std::conditional_t<signed_conversion,
												 ::fast_io::details::my_make_signed_t<promoted_type>,
												 ::fast_io::details::my_make_unsigned_t<promoted_type>>;
		return static_cast<target_type>(static_cast<promoted_type>(value));
	}
	else
	{
		// The conversion character selects signed or unsigned interpretation, but
		// it does not select `int` as the storage width.  Preserving the source
		// rank here is what lets the resulting scalar reach fast_io's native
		// long/long-long/wide-integer backend without a lossy frontend cast.
		using target_type = ::std::conditional_t<signed_conversion,
												 ::fast_io::details::my_make_signed_t<clean_type>,
												 ::fast_io::details::my_make_unsigned_t<clean_type>>;
		return static_cast<target_type>(value);
	}
}

template <bool signed_conversion, printf_length_modifier length, typename value_type>
[[nodiscard]] inline constexpr auto printf_integer_cast(value_type value) noexcept
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	static_assert(::fast_io::details::my_integral<clean_type> ||
					  ::std::same_as<clean_type, ::std::byte> || ::std::is_enum_v<clean_type>,
				  "fast_io format: printf integer conversion requires an integer argument");
	static_assert(length != printf_length_modifier::long_double,
				  "fast_io format: L is valid only for a floating printf conversion");

	if constexpr (::std::same_as<clean_type, ::std::byte>)
	{
		return printf_integer_cast<signed_conversion, length>(
			static_cast<unsigned char>(value));
	}
	else if constexpr (::std::is_enum_v<clean_type>)
	{
		// A typed formatter has no default argument promotion boundary at which an
		// enum would otherwise be normalized.  Lowering through its ABI-visible
		// underlying integer gives it the same deterministic width policy as a
		// directly supplied integral value, including for scoped enumerations.
		// This is a type-safe semantic rule, not an attempt to reproduce the
		// undefined behavior of passing a scoped enum through C varargs.
		return printf_integer_cast<signed_conversion, length>(
			static_cast<::std::underlying_type_t<clean_type>>(value));
	}
	else if constexpr (length == printf_length_modifier::hh)
	{
		using target_type = ::std::conditional_t<signed_conversion, signed char, unsigned char>;
		return static_cast<target_type>(value);
	}
	else if constexpr (length == printf_length_modifier::h)
	{
		using target_type = ::std::conditional_t<signed_conversion, short, unsigned short>;
		return static_cast<target_type>(value);
	}
	else if constexpr (length == printf_length_modifier::l)
	{
		using target_type = ::std::conditional_t<signed_conversion, long, unsigned long>;
		return static_cast<target_type>(value);
	}
	else if constexpr (length == printf_length_modifier::ll)
	{
		using target_type = ::std::conditional_t<signed_conversion, long long, unsigned long long>;
		return static_cast<target_type>(value);
	}
	else if constexpr (length == printf_length_modifier::j)
	{
		using target_type = ::std::conditional_t<signed_conversion, ::std::intmax_t, ::std::uintmax_t>;
		return static_cast<target_type>(value);
	}
	else if constexpr (length == printf_length_modifier::z)
	{
		using target_type = ::std::conditional_t<signed_conversion,
												 ::std::make_signed_t<::std::size_t>, ::std::size_t>;
		return static_cast<target_type>(value);
	}
	else if constexpr (length == printf_length_modifier::t)
	{
		using target_type = ::std::conditional_t<signed_conversion, ::std::ptrdiff_t,
												 ::std::make_unsigned_t<::std::ptrdiff_t>>;
		return static_cast<target_type>(value);
	}
	else
	{
		return printf_default_integer_cast<signed_conversion>(value);
	}
}

template <format_specification specification, bool signed_conversion, bool showbase>
[[nodiscard]] inline consteval ::fast_io::manipulators::scalar_flags
printf_integer_scalar_flags() noexcept
{
	::fast_io::manipulators::scalar_flags flags{};
	if constexpr (specification.presentation == presentation_type::octal)
	{
		flags.base = 8u;
	}
	else if constexpr (specification.presentation == presentation_type::hex_lower ||
					   specification.presentation == presentation_type::hex_upper)
	{
		flags.base = 16u;
	}
	else
	{
		flags.base = 10u;
	}
	constexpr bool uppercase{
		specification.presentation == presentation_type::hex_upper};
	flags.showbase = showbase;
	flags.showpos = signed_conversion &&
					(specification.sign == format_sign::plus ||
					 specification.sign == format_sign::space);
	flags.uppercase_showbase = uppercase;
	flags.uppercase = uppercase;
	return flags;
}

template <format_specification specification, bool signed_conversion,
		  bool showbase, typename value_type>
[[nodiscard]] inline constexpr auto make_printf_integer_scalar(value_type value)
{
	constexpr auto flags{
		printf_integer_scalar_flags<specification, signed_conversion, showbase>()};
	auto scalar{::fast_io::details::scalar_flags_int_cache<flags>(value)};
	constexpr ::std::size_t prefix_size{showbase
											? (flags.base == 8u ? 1u : (flags.base == 16u ? 2u : 0u))
											: 0u};
	return ::fast_io::manipulators::format_scalar_t<decltype(scalar), prefix_size,
													signed_conversion && specification.sign == format_sign::space>{scalar};
}

template <format_specification specification, bool signed_conversion,
		  bool showbase, bool precision_active, typename value_type>
[[nodiscard]] inline constexpr auto make_printf_integer_path(
	value_type value, resolved_format_parameter width,
	resolved_format_parameter precision)
{
	auto scalar{make_printf_integer_scalar<specification, signed_conversion, showbase>(value)};
	if constexpr (precision_active)
	{
		constexpr auto flags{
			printf_integer_scalar_flags<specification, signed_conversion, showbase>()};
		constexpr ::std::size_t prefix_size{showbase
												? (flags.base == 8u ? 1u : (flags.base == 16u ? 2u : 0u))
												: 0u};
		auto const internal_shift{
			::fast_io::fmt::details::formatted_scalar_internal_shift<flags>(
				value, prefix_size)};
		// The legacy octal prefix is itself a precision-counted digit.  The
		// hexadecimal prefix is not, and therefore remains in the target width.
		auto const target{precision.value + internal_shift -
						  static_cast<::std::size_t>(showbase && flags.base == 8u)};
		auto padded{::fast_io::manipulators::internal(
			::std::move(scalar), target,
			::fast_io::char_literal_v<u8'0',
									  ::std::remove_cvref_t<decltype(specification.fill[0])>>)};
		bool const suppress_zero{value == 0 && precision.value == 0u &&
								 !(specification.alternate_form &&
								   specification.presentation == presentation_type::octal)};
		auto emitted{::fast_io::manipulators::cond(!suppress_zero, ::std::move(padded))};
		return apply_printf_width<specification, true, true>(
			::std::move(emitted), width);
	}
	else
	{
		return apply_printf_width<specification, true, false>(
			::std::move(scalar), width);
	}
}

template <format_specification specification, bool signed_conversion,
		  bool showbase, typename value_type>
[[nodiscard]] inline constexpr auto make_printf_integer_precision_dispatch(
	value_type value, resolved_format_parameter width,
	resolved_format_parameter precision)
{
	if constexpr (specification.precision.kind == format_parameter_kind::none)
	{
		return make_printf_integer_path<specification, signed_conversion, showbase, false>(
			value, width, precision);
	}
	else if constexpr (specification.precision.kind == format_parameter_kind::literal)
	{
		return make_printf_integer_path<specification, signed_conversion, showbase, true>(
			value, width, precision);
	}
	else
	{
		auto active{make_printf_integer_path<specification, signed_conversion, showbase, true>(
			value, width, precision)};
		auto omitted{make_printf_integer_path<specification, signed_conversion, showbase, false>(
			value, width, precision)};
		return ::fast_io::manipulators::cond(
			!precision.negative, ::std::move(active), ::std::move(omitted));
	}
}

template <format_specification specification, bool signed_conversion, typename value_type>
[[nodiscard]] inline constexpr auto make_printf_integer(
	value_type value, resolved_format_parameter width,
	resolved_format_parameter precision)
{
	static_assert(!specification.locale_specific,
				  "fast_io format: printf locale grouping requires an explicit locale overload");
	constexpr bool hexadecimal{
		specification.presentation == presentation_type::hex_lower ||
		specification.presentation == presentation_type::hex_upper};
	constexpr bool octal{specification.presentation == presentation_type::octal};
	if constexpr (specification.alternate_form && (hexadecimal || octal))
	{
		auto prefixed{make_printf_integer_precision_dispatch<
			specification, signed_conversion, true>(value, width, precision)};
		auto plain{make_printf_integer_precision_dispatch<
			specification, signed_conversion, false>(value, width, precision)};
		return ::fast_io::manipulators::cond(
			value != 0, ::std::move(prefixed), ::std::move(plain));
	}
	else
	{
		return make_printf_integer_precision_dispatch < specification,
			   signed_conversion, specification.alternate_form && octal > (value, width, precision);
	}
}

template <format_specification specification>
[[nodiscard]] inline consteval auto printf_without_precision() noexcept
{
	auto result{specification};
	result.precision = {};
	return result;
}

template <typename char_type>
inline constexpr ::std::array<char_type, 5u> printf_nil_storage{
	::fast_io::char_literal_v<u8'(', char_type>,
	::fast_io::char_literal_v<u8'n', char_type>,
	::fast_io::char_literal_v<u8'i', char_type>,
	::fast_io::char_literal_v<u8'l', char_type>,
	::fast_io::char_literal_v<u8')', char_type>};

template <typename char_type, replacement_field field,
		  ::std::size_t... index, typename... argument_types, typename value_type>
[[nodiscard]] inline constexpr auto make_printf_value(
	value_type &value,
	indexed_argument_pack<::std::index_sequence<index...>, argument_types...> &arguments,
	auto format_literal_tag)
{
	constexpr auto format_literal{decltype(format_literal_tag)::value};
	constexpr auto specification{field.specification};
	auto const width{
		resolve_format_parameter<format_literal, specification.width>(arguments)};
	auto const precision{
		resolve_format_parameter<format_literal, specification.precision>(arguments)};
	using clean_type = ::std::remove_cvref_t<value_type>;

	if constexpr (specification.presentation == presentation_type::decimal ||
				  specification.presentation == presentation_type::unsigned_decimal ||
				  specification.presentation == presentation_type::octal ||
				  specification.presentation == presentation_type::hex_lower ||
				  specification.presentation == presentation_type::hex_upper)
	{
		constexpr bool signed_conversion{
			specification.presentation == presentation_type::decimal};
		auto converted{printf_integer_cast<signed_conversion, field.printf_length>(value)};
		return make_printf_integer<specification, signed_conversion>(
			converted, width, precision);
	}
	else if constexpr (specification.presentation == presentation_type::character)
	{
		static_assert(field.printf_length == printf_length_modifier::none,
					  "fast_io format: wide printf %lc transcoding is not available in the same-character frontend");
		static_assert(::fast_io::details::my_integral<clean_type> ||
						  ::std::same_as<clean_type, ::std::byte>,
					  "fast_io format: printf %c requires an integer argument");
		static_assert(specification.precision.kind == format_parameter_kind::none &&
						  !specification.alternate_form &&
						  (specification.sign == format_sign::default_sign ||
						   specification.sign == format_sign::minus) &&
						  !specification.zero_padding && !specification.locale_specific,
					  "fast_io format: invalid printf %c flags or precision");
		auto formatted{::fast_io::manipulators::chvw(static_cast<char_type>(value))};
		return apply_printf_width<specification, false, false>(
			::std::move(formatted), width);
	}
	else if constexpr (specification.presentation == presentation_type::string)
	{
		static_assert(field.printf_length == printf_length_modifier::none,
					  "fast_io format: wide printf %ls transcoding is not available in the same-character frontend");
		static_assert(format_string_like<char_type, value_type>,
					  "fast_io format: printf %s requires a same-character string argument");
		static_assert(!specification.alternate_form &&
						  (specification.sign == format_sign::default_sign ||
						   specification.sign == format_sign::minus) &&
						  !specification.zero_padding && !specification.locale_specific,
					  "fast_io format: invalid printf %s flags");
		auto formatted{make_string_scatter<char_type>(value,
													  precision.present && !precision.negative ? precision.value : SIZE_MAX)};
		return apply_printf_width<specification, false, false>(
			::std::move(formatted), width);
	}
	else if constexpr (specification.presentation == presentation_type::pointer)
	{
		static_assert(field.printf_length == printf_length_modifier::none &&
						  specification.precision.kind == format_parameter_kind::none &&
						  !specification.alternate_form &&
						  (specification.sign == format_sign::default_sign ||
						   specification.sign == format_sign::minus) &&
						  !specification.locale_specific,
					  "fast_io format: invalid printf %p length, flags, or precision");
		static_assert(::std::same_as<clean_type, ::std::nullptr_t> ||
						  (::std::is_pointer_v<clean_type> &&
						   !::std::is_function_v<::std::remove_pointer_t<clean_type>>),
					  "fast_io format: printf %p requires an object pointer");
		auto scalar{make_pointer_scalar<specification>(value)};
		auto nonnull{apply_printf_width<specification, true, false>(
			::std::move(scalar), width)};
		auto nil{::fast_io::manipulators::static_scatter_t<char_type, 5u>{
			printf_nil_storage<char_type>.data()}};
		auto null_value{apply_printf_width<specification, false, false>(
			::std::move(nil), width)};
		return ::fast_io::manipulators::cond(
			value != nullptr, ::std::move(nonnull), ::std::move(null_value));
	}
	else
	{
		static_assert(::fast_io::details::my_floating_point<clean_type>,
					  "fast_io format: printf floating conversion requires a floating argument");
		static_assert(field.printf_length == printf_length_modifier::none ||
						  field.printf_length == printf_length_modifier::l ||
						  field.printf_length == printf_length_modifier::long_double,
					  "fast_io format: invalid printf floating length modifier");

		auto emit = [&]<typename floating_type>(floating_type converted) constexpr {
			if constexpr (specification.precision.kind == format_parameter_kind::argument)
			{
				constexpr auto omitted_specification{
					printf_without_precision<specification>()};
				auto active_precision{precision};
				// Both condition arms are materialized before the semantic predicate is
				// evaluated.  A negative star precision belongs to the omitted arm; the
				// inactive formatted arm therefore receives only its checked magnitude.
				active_precision.negative = false;
				auto active_scalar{
					make_brace_floating<specification>(converted, active_precision)};
				auto omitted_scalar{
					make_brace_floating<omitted_specification>(converted, {})};
				auto active_width{apply_printf_width<specification, true, false>(
					::std::move(active_scalar), width)};
				auto omitted_width{apply_printf_width<specification, true, false>(
					::std::move(omitted_scalar), width)};
				return ::fast_io::manipulators::cond(!precision.negative,
													 ::std::move(active_width), ::std::move(omitted_width));
			}
			else
			{
				auto scalar{
					make_brace_floating<specification>(converted, precision)};
				return apply_printf_width<specification, true, false>(
					::std::move(scalar), width);
			}
		};
		if constexpr (field.printf_length == printf_length_modifier::long_double)
		{
			return emit(static_cast<long double>(value));
		}
		else if constexpr (
			::fast_io::details::floating_scalar_requires_integer_proxy<clean_type>)
		{
			// bfloat16 participates in the scalar register ABI, but an arithmetic
			// cast of sNaN to printf's promoted double would raise FE_INVALID and
			// quiet its payload.  Its exact value is representable in binary32;
			// widen the preserved fields by bit-copy and let the existing float
			// formatter apply the identical decimal precision grammar.
			auto const representation{
				::fast_io::bit_cast<::std::uint_least16_t>(value)};
			return emit(::fast_io::bit_cast<float>(
				static_cast<::std::uint_least32_t>(representation) << 16u));
		}
		else
		{
			return emit(static_cast<double>(value));
		}
	}
}

} // namespace fast_io::fmt::details

#include "../../fast_io_dsal/impl/misc/pop_macros.h"

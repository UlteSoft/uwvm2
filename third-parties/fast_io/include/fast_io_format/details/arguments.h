#pragma once

/*
 * Run-time argument view used by format lowering (FMT level).
 *
 * The public argument pack is projected into stable, uniquely indexed lvalue
 * slots so a compiled field can select its value without recursive tuple
 * machinery or accidental copies. This file preserves identity only for the
 * lowering call; downstream alias/forward and semantic-node storage protocols
 * still decide the representation accepted by IO.
 */

#include "../types.h"
#include "program.h"

#include <cstddef>
#include <type_traits>

namespace fast_io::fmt::details
{

template <::std::size_t index, typename value_type>
struct indexed_argument_slot
{
	value_type &reference;
};

template <typename index_sequence, typename... argument_types>
struct indexed_argument_pack;

/// A non-recursive stable-lvalue view over the public argument pack.
///
/// Each argument has a uniquely indexed base.  Selecting argument I therefore uses ordinary
/// derived-to-base deduction instead of a recursive `nth_type<I, Args...>`.  This gives the
/// C++20 path the same asymptotic property that native pack indexing provides in C++26: a
/// dense N-field format does not instantiate 1 + 2 + ... + N type-selection nodes.
///
/// The view deliberately models the expressions seen inside fast_io::io::print: every forwarding
/// parameter is named before aliasing, so it is a stable lvalue for the duration of the call.
/// Array extent and cv-qualification remain intact, caller lvalues remain borrowed, and caller
/// rvalue temporaries remain alive through the complete expression. Re-forwarding a repeated
/// field such as `{0} {0}` would instead permit two moves from one object and would not match the
/// direct print boundary.
template <::std::size_t... index, typename... argument_types>
struct indexed_argument_pack<::std::index_sequence<index...>, argument_types...>
	: indexed_argument_slot<index, argument_types>...
{
	inline constexpr explicit indexed_argument_pack(argument_types &...arguments) noexcept
		: indexed_argument_slot<index, argument_types>{arguments}...
	{}
};

template <typename... argument_types>
indexed_argument_pack(::std::index_sequence_for<argument_types...>, argument_types &...)
	-> indexed_argument_pack<::std::index_sequence_for<argument_types...>, argument_types...>;

template <typename... argument_types>
[[nodiscard]] inline constexpr auto make_indexed_argument_pack(argument_types &...arguments) noexcept
{
	return indexed_argument_pack<::std::index_sequence_for<argument_types...>, argument_types...>{arguments...};
}

/// Selects an indexed base while simultaneously deducing its element type.
template <::std::size_t index, typename value_type>
[[nodiscard]] inline constexpr value_type &indexed_argument_get(
	indexed_argument_slot<index, value_type> &slot) noexcept
{
	return slot.reference;
}

template <::std::size_t index, typename value_type>
[[nodiscard]] inline constexpr value_type const &indexed_argument_get(
	indexed_argument_slot<index, value_type> const &slot) noexcept
{
	return slot.reference;
}

template <typename T>
[[nodiscard]] inline constexpr decltype(auto) unwrap_static_named_argument(T &value) noexcept
{
	if constexpr (::fast_io::fmt::is_static_named_arg_v<T>)
	{
		return unwrap_static_named_argument(value.value);
	}
	else if constexpr (::fast_io::fmt::is_static_format_arg_v<T>)
	{
		return ::std::remove_cvref_t<T>::get();
	}
	else
	{
		return (value);
	}
}

template <typename T>
[[nodiscard]] inline constexpr decltype(auto) unwrap_static_named_argument(T const &value) noexcept
{
	if constexpr (::fast_io::fmt::is_static_named_arg_v<T>)
	{
		return unwrap_static_named_argument(value.value);
	}
	else if constexpr (::fast_io::fmt::is_static_format_arg_v<T>)
	{
		return ::std::remove_cvref_t<T>::get();
	}
	else
	{
		return (value);
	}
}

enum class argument_resolution_error : unsigned char
{
	none,
	index_out_of_range,
	name_not_found,
	duplicate_name
};

struct argument_resolution
{
	::std::size_t index{};
	argument_resolution_error error{argument_resolution_error::none};
};

template <auto format_literal, source_slice name, auto candidate_name>
[[nodiscard]] inline consteval bool static_argument_name_equal() noexcept
{
	using format_char_type = typename decltype(format_literal)::value_type;
	using candidate_char_type = typename decltype(candidate_name)::value_type;
	if constexpr (!::std::same_as<format_char_type, candidate_char_type>)
	{
		return false;
	}
	else if constexpr (name.size != candidate_name.size())
	{
		return false;
	}
	else
	{
		for (::std::size_t i{}; i != name.size; ++i)
		{
			if (format_literal[name.offset + i] != candidate_name[i])
			{
				return false;
			}
		}
		return true;
	}
}

template <auto format_literal, argument_reference reference, typename... argument_types>
[[nodiscard]] inline consteval argument_resolution resolve_argument_reference() noexcept
{
	if constexpr (reference.kind != argument_reference_kind::name)
	{
		if constexpr (reference.index < sizeof...(argument_types))
		{
			return {reference.index, argument_resolution_error::none};
		}
		else
		{
			return {reference.index, argument_resolution_error::index_out_of_range};
		}
	}
	else
	{
		::std::size_t current_index{};
		::std::size_t selected_index{};
		::std::size_t match_count{};
		([&]<typename argument_type>() consteval {
			using clean_type = ::std::remove_cvref_t<argument_type>;
			if constexpr (::fast_io::fmt::is_static_named_arg_v<clean_type>)
			{
				if constexpr (static_argument_name_equal<format_literal, reference.name, clean_type::name>())
				{
					selected_index = current_index;
					++match_count;
				}
			}
			++current_index;
		}.template operator()<argument_types>(),
		 ...);

		if (match_count == 1u)
		{
			return {selected_index, argument_resolution_error::none};
		}
		if (match_count == 0u)
		{
			return {0u, argument_resolution_error::name_not_found};
		}
		return {selected_index, argument_resolution_error::duplicate_name};
	}
}

/** Resolves a reference value when a grammar-wide pass cannot name it as an NTTP.
 *
 * The grammar-wide validator is the immediate proof boundary.  This helper is
 * deliberately constexpr: its reference is a local value inside that proof,
 * and early C++20 frontends reject forwarding such a value into a nested
 * immediate invocation even though the complete evaluation is mandatory.
 */
template <auto format_literal, typename... argument_types>
[[nodiscard]] inline constexpr argument_resolution
resolve_argument_reference_value(argument_reference reference) noexcept
{
	if (reference.kind != argument_reference_kind::name)
	{
		if (reference.index < sizeof...(argument_types))
		{
			return {reference.index, argument_resolution_error::none};
		}
		return {reference.index,
				argument_resolution_error::index_out_of_range};
	}

	[[maybe_unused]] ::std::size_t current_index{};
	::std::size_t selected_index{};
	::std::size_t match_count{};
	([&]<typename argument_type>() constexpr {
		using clean_type = ::std::remove_cvref_t<argument_type>;
		if constexpr (::fast_io::fmt::is_static_named_arg_v<clean_type>)
		{
			constexpr auto candidate_name{clean_type::name};
			using format_char_type = typename decltype(format_literal)::value_type;
			using candidate_char_type = typename decltype(candidate_name)::value_type;
			if constexpr (::std::same_as<format_char_type,
										 candidate_char_type>)
			{
				bool equal{reference.name.size == candidate_name.size()};
				for (::std::size_t i{}; equal && i != reference.name.size; ++i)
				{
					equal = format_literal[reference.name.offset + i] ==
							candidate_name[i];
				}
				if (equal)
				{
					selected_index = current_index;
					++match_count;
				}
			}
		}
		++current_index;
	}.template operator()<argument_types>(),
	 ...);

	if (match_count == 1u)
	{
		return {selected_index, argument_resolution_error::none};
	}
	if (match_count == 0u)
	{
		return {0u, argument_resolution_error::name_not_found};
	}
	return {selected_index, argument_resolution_error::duplicate_name};
}

template <argument_resolution_error error, ::std::size_t source_position>
inline consteval void diagnose_argument_resolution()
{
	if constexpr (error == argument_resolution_error::index_out_of_range)
	{
		static_assert(error == argument_resolution_error::none,
					  "fast_io format: replacement-field argument index is out of range");
	}
	else if constexpr (error == argument_resolution_error::name_not_found)
	{
		static_assert(error == argument_resolution_error::none,
					  "fast_io format: static named argument was not found");
	}
	else if constexpr (error == argument_resolution_error::duplicate_name)
	{
		static_assert(error == argument_resolution_error::none,
					  "fast_io format: static named argument is ambiguous");
	}
	else
	{
		static_assert(error == argument_resolution_error::none);
	}
	(void)source_position; // The value remains visible in the diagnostic template arguments.
}

} // namespace fast_io::fmt::details

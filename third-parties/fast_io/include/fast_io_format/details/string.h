#pragma once

/*
 * String-oriented format adapters (FMT level).
 *
 * This file classifies string/scatter-like sources and provides the bounded
 * views or printable wrappers required by format specifications. Character
 * interpretation and format precision are translated here, but ownership,
 * destination growth, scatter selection, and actual transfer are delegated to
 * the ordinary IO and protocol levels.
 */

#include "semantic.h"
// The classifications below name fast_io container templates directly; include
// their declarations here instead of relying on a hosted umbrella include.
#include "../../fast_io_dsal/string.h"

#include <array>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <type_traits>

namespace fast_io::fmt::details
{

template <typename T>
struct io_scatter_traits
{
	static inline constexpr bool is_scatter{};
};

template <typename char_type>
struct io_scatter_traits<::fast_io::basic_io_scatter_t<char_type>>
{
	static inline constexpr bool is_scatter{true};
	using value_type = char_type;
};

template <typename char_type>
struct io_scatter_traits<::fast_io::basic_prfch_cacheable_io_scatter_t<char_type>>
{
	static inline constexpr bool is_scatter{true};
	using value_type = char_type;
};

template <typename char_type, typename T>
inline constexpr bool same_character_array_v = []() constexpr {
	using clean_type = ::std::remove_reference_t<T>;
	if constexpr (!::std::is_array_v<clean_type>)
	{
		return false;
	}
	else
	{
		return ::std::same_as<
			::std::remove_cv_t<::std::remove_extent_t<clean_type>>, char_type>;
	}
}();

template <typename char_type, typename T>
inline constexpr bool same_character_pointer_v = []() constexpr {
	using clean_type = ::std::remove_cvref_t<T>;
	if constexpr (!::std::is_pointer_v<clean_type>)
	{
		return false;
	}
	else
	{
		return ::std::same_as<
			::std::remove_cv_t<::std::remove_pointer_t<clean_type>>, char_type>;
	}
}();

template <typename char_type, typename T>
inline constexpr bool same_character_scatter_v = []() constexpr {
	using clean_type = ::std::remove_cvref_t<T>;
	if constexpr (!io_scatter_traits<clean_type>::is_scatter)
	{
		return false;
	}
	else
	{
		return ::std::same_as<typename io_scatter_traits<clean_type>::value_type, char_type>;
	}
}();

template <typename char_type, typename T>
concept same_character_contiguous_range =
	(!same_character_array_v<char_type, T>) &&
	(!same_character_scatter_v<char_type, T>) &&
	::std::ranges::contiguous_range<T> && ::std::ranges::sized_range<T> &&
	::std::same_as<::std::remove_cv_t<::std::ranges::range_value_t<T>>, char_type>;

template <typename char_type, typename T>
concept format_string_like =
	same_character_array_v<char_type, T> || same_character_pointer_v<char_type, T> ||
	same_character_scatter_v<char_type, T> || same_character_contiguous_range<char_type, T>;

template <typename T>
struct fast_io_string_class : ::std::false_type
{};

template <::std::integral char_type, typename allocator_type>
struct fast_io_string_class<
	::fast_io::containers::basic_string<char_type, allocator_type>> : ::std::true_type
{};

template <::std::integral char_type>
struct fast_io_string_class<
	::fast_io::containers::basic_string_view<char_type>> : ::std::true_type
{};

template <::std::integral char_type>
struct fast_io_string_class<
	::fast_io::containers::basic_cstring_view<char_type>> : ::std::true_type
{};

/**
 * Distinguishes scalar string objects from arbitrary contiguous char ranges.
 *
 * `format_string_like` is deliberately broad because an explicit `s` field may
 * consume any contiguous character source.  Default brace formatting is more
 * selective: fmt treats vector<char> and array<char, N> as ranges, while
 * std::basic_string/string_view and fast_io's string classes are scalar text.
 * Standard string classes expose `traits_type`; fast_io classes are identified
 * structurally by the specializations above.  This split prevents range
 * lowering from changing the long-established direct string/scatter path.
 */
template <typename char_type, typename T>
concept brace_scalar_string_source =
	format_string_like<char_type, T> &&
	(same_character_array_v<char_type, T> ||
	 same_character_pointer_v<char_type, T> ||
	 same_character_scatter_v<char_type, T> ||
	 requires { typename ::std::remove_cvref_t<T>::traits_type; } ||
	 fast_io_string_class<::std::remove_cvref_t<T>>::value);

template <typename char_type>
inline constexpr ::std::array<char_type, 6u> null_string_storage{
	::fast_io::char_literal_v<u8'(', char_type>,
	::fast_io::char_literal_v<u8'n', char_type>,
	::fast_io::char_literal_v<u8'u', char_type>,
	::fast_io::char_literal_v<u8'l', char_type>,
	::fast_io::char_literal_v<u8'l', char_type>,
	::fast_io::char_literal_v<u8')', char_type>};

[[nodiscard]] inline constexpr ::std::size_t bounded_length(
	::std::size_t size, ::std::size_t maximum) noexcept
{
	return size < maximum ? size : maximum;
}

/// Produces the one string representation consumed by brace `s` and printf `%s`.
///
/// Pointers are bounded before searching when precision is present, matching printf's rule
/// that no code unit beyond the precision limit need be readable.  Arrays are also bounded by
/// their object extent; unlike an untyped varargs implementation, this type-safe front end
/// never reads past a non-null-terminated character array.  Owning/view ranges preserve their
/// explicit size, including embedded nulls.
template <typename char_type, typename T>
	requires format_string_like<char_type, T>
[[nodiscard]] inline constexpr auto make_string_scatter(
	T &value, ::std::size_t maximum = SIZE_MAX) noexcept
{
	using clean_type = ::std::remove_cvref_t<T>;
	if constexpr (same_character_array_v<char_type, T>)
	{
		constexpr ::std::size_t extent{::std::extent_v<::std::remove_reference_t<T>>};
		auto const bound{bounded_length(extent, maximum)};
		// Preserve the finite readable extent without changing the ordinary scatter path.  Only print/concat's optional
		// compiler-constant protocol may turn this descriptor into a short reserve proxy.
		return ::fast_io::manipulators::bounded_cstr_scatter_t<
			char_type, extent>{
			value, ::fast_io::cstr_nlen(value, bound)};
	}
	else if constexpr (same_character_pointer_v<char_type, T>)
	{
		auto const pointer{value};
		if (pointer == nullptr)
		{
			return ::fast_io::basic_io_scatter_t<char_type>{
				null_string_storage<char_type>.data(),
				null_string_storage<char_type>.size()};
		}
		if (maximum == SIZE_MAX)
		{
			return ::fast_io::basic_io_scatter_t<char_type>{
				pointer, ::fast_io::cstr_len(pointer)};
		}
		return ::fast_io::basic_io_scatter_t<char_type>{
			pointer, ::fast_io::cstr_nlen(pointer, maximum)};
	}
	else if constexpr (same_character_scatter_v<char_type, T>)
	{
		if constexpr (::std::same_as<clean_type, ::fast_io::basic_io_scatter_t<char_type>>)
		{
			return ::fast_io::basic_io_scatter_t<char_type>{
				value.base, bounded_length(value.len, maximum)};
		}
		else
		{
			auto const scatter{value.scatter()};
			return ::fast_io::basic_io_scatter_t<char_type>{
				scatter.base, bounded_length(scatter.len, maximum)};
		}
	}
	else
	{
		return ::fast_io::basic_io_scatter_t<char_type>{
			::std::ranges::data(value),
			bounded_length(static_cast<::std::size_t>(::std::ranges::size(value)), maximum)};
	}
}

} // namespace fast_io::fmt::details

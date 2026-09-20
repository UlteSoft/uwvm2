#pragma once

/*
 * Dynamic width and precision adaptation (FMT level).
 *
 * This file validates run-time integer controls selected by a compile-time
 * field and expresses their meaning as typed formatting state. It bridges
 * format grammar semantics to IO semantic/printable objects: range checking
 * belongs here, while padding, storage, and emission strategy belong to the
 * downstream IO and CPO levels.
 */

#include "arguments.h"

#include <climits>
#include <cstddef>
#include <type_traits>

namespace fast_io::fmt::details
{

struct dynamic_format_integer
{
	::std::size_t magnitude{};
	bool negative{};
};

template <typename T>
inline constexpr bool dynamic_format_integer_type_v =
	::fast_io::details::my_integral<::std::remove_cvref_t<T>> &&
	!::std::same_as<::std::remove_cvref_t<T>, bool> &&
	!::fast_io::details::character_integral<::std::remove_cvref_t<T>>;

/// Checks a runtime width/precision value without signed overflow.
///
/// The format grammar caps these values at INT_MAX, as fmt does.  Forming the magnitude in
/// the corresponding unsigned type is required for the minimum signed value; `-value` would
/// itself be undefined before any range check could run.
template <typename T>
[[nodiscard]] inline constexpr dynamic_format_integer checked_dynamic_integer(T value) noexcept
{
	using clean_type = ::std::remove_cvref_t<T>;
	if constexpr (!dynamic_format_integer_type_v<clean_type>)
	{
		static_assert(dynamic_format_integer_type_v<clean_type>,
			"fast_io format: dynamic width/precision requires a non-character integer");
		return {};
	}
	else
	{
		using unsigned_type = ::fast_io::details::my_make_unsigned_t<clean_type>;
		bool negative{};
		unsigned_type magnitude{};
		if constexpr (::fast_io::details::my_signed_integral<clean_type>)
		{
			negative = value < 0;
			auto const bits{static_cast<unsigned_type>(value)};
			magnitude = negative ? static_cast<unsigned_type>(unsigned_type{} - bits) : bits;
		}
		else
		{
			magnitude = static_cast<unsigned_type>(value);
		}
		if (magnitude > static_cast<unsigned_type>(INT_MAX)) [[unlikely]]
		{
			::fast_io::fast_terminate();
		}
		return {static_cast<::std::size_t>(magnitude), negative};
	}
}

} // namespace fast_io::fmt::details

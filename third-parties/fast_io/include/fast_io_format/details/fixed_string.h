#pragma once

/*
 * Structural literal carrier for the format frontend (FMT level).
 *
 * This file turns a source character array into an NTTP-safe value with an
 * explicit character domain and extent. Parsers and grammar CPOs consume that
 * structural value during constant evaluation. It is format source storage,
 * not an IO scatter and not a printable-object customization.
 */

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace fast_io::fmt
{

template <typename char_type>
inline constexpr bool is_format_character_v =
	::std::same_as<::std::remove_cv_t<char_type>, char> ||
	::std::same_as<::std::remove_cv_t<char_type>, wchar_t> ||
	::std::same_as<::std::remove_cv_t<char_type>, char8_t> ||
	::std::same_as<::std::remove_cv_t<char_type>, char16_t> ||
	::std::same_as<::std::remove_cv_t<char_type>, char32_t>;

template <typename char_type>
concept format_character = is_format_character_v<char_type>;

/**
 * A structural, null-terminated string value suitable for use as a C++20 NTTP.
 *
 * The array is intentionally public: a C++20 class-type non-type template
 * argument must be structural, and private data would make the type
 * non-structural.  `size()` excludes the terminator, matching string-view
 * conventions, while `extent` includes it.
 */
template <format_character char_type, ::std::size_t extent_value>
struct basic_fixed_string
{
	static_assert(extent_value != 0u, "a format string must include a null terminator");

	using value_type = char_type;
	static inline constexpr ::std::size_t extent{extent_value};
	char_type elements[extent_value]{};

	consteval basic_fixed_string(char_type const (&source)[extent_value]) noexcept
	{
		for (::std::size_t i{}; i != extent_value; ++i)
		{
			elements[i] = source[i];
		}
	}

	[[nodiscard]] inline constexpr char_type const *data() const noexcept
	{
		return elements;
	}

	[[nodiscard]] inline static constexpr ::std::size_t size() noexcept
	{
		return extent_value - 1u;
	}

	[[nodiscard]] inline constexpr char_type const *begin() const noexcept
	{
		return elements;
	}

	[[nodiscard]] inline constexpr char_type const *end() const noexcept
	{
		return elements + size();
	}

	[[nodiscard]] inline constexpr char_type const &operator[](::std::size_t index) const noexcept
	{
		return elements[index];
	}

	[[nodiscard]] inline constexpr bool operator==(basic_fixed_string const &) const noexcept = default;
};

template <format_character char_type, ::std::size_t extent>
basic_fixed_string(char_type const (&)[extent]) -> basic_fixed_string<char_type, extent>;

} // namespace fast_io::fmt

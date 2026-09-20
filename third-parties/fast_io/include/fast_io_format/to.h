#pragma once

/* Public formatted value-to-value conversion facade. */

#include "details/brace_rule.h"
#include "details/printf_rule.h"
#include "details/to.h"

#include <cstddef>
#include <utility>

namespace fast_io::fmt
{

// Brace grammar.  The format literal is the first template argument, matching
// `print` and `concat`; the target type follows it: `fmt::to<"{}", int>(v)`.
template <basic_fixed_string format_literal, typename result_type,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type to(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		char, result_type, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

// Target-first compatibility spelling, matching the core `fast_io::to<T>`
// family: `fmt::to<int, "{}">(value)`.
template <typename result_type, basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type to(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		char, result_type, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <typename result_type, basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type wto(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		wchar_t, result_type, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename result_type,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type wto(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		wchar_t, result_type, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <typename result_type, basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type u8to(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		char8_t, result_type, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename result_type,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type u8to(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		char8_t, result_type, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <typename result_type, basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type u16to(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		char16_t, result_type, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename result_type,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type u16to(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		char16_t, result_type, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <typename result_type, basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type u32to(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		char32_t, result_type, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename result_type,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type u32to(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		char32_t, result_type, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

// Percent grammar aliases follow the existing `printf`/`concatf` naming.
template <typename result_type, basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type printf_to(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		char, result_type, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <typename result_type, basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type wprintf_to(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		wchar_t, result_type, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <typename result_type, basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type u8printf_to(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		char8_t, result_type, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <typename result_type, basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type u16printf_to(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		char16_t, result_type, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <typename result_type, basic_fixed_string format_literal,
		  typename... argument_types>
[[nodiscard]] inline constexpr result_type u32printf_to(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::to_builtin_with_rule<
		char32_t, result_type, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

// Existing target, brace grammar.  The format literal is a structural NTTP,
// just as it is for `print` and `concat`.
template <basic_fixed_string format_literal, typename target_type,
		  typename... argument_types>
inline constexpr void inplace_to(target_type &&target,
						 argument_types &&...arguments)
{
	::fast_io::fmt::details::inplace_to_builtin_with_rule<
		char, format_literal>(
		brace_fmt_t{}, ::std::forward<target_type>(target),
		::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename target_type,
		  typename... argument_types>
inline constexpr void winplace_to(target_type &&target,
						  argument_types &&...arguments)
{
	::fast_io::fmt::details::inplace_to_builtin_with_rule<
		wchar_t, format_literal>(
		brace_fmt_t{}, ::std::forward<target_type>(target),
		::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename target_type,
		  typename... argument_types>
inline constexpr void u8inplace_to(target_type &&target,
						   argument_types &&...arguments)
{
	::fast_io::fmt::details::inplace_to_builtin_with_rule<
		char8_t, format_literal>(
		brace_fmt_t{}, ::std::forward<target_type>(target),
		::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename target_type,
		  typename... argument_types>
inline constexpr void u16inplace_to(target_type &&target,
						    argument_types &&...arguments)
{
	::fast_io::fmt::details::inplace_to_builtin_with_rule<
		char16_t, format_literal>(
		brace_fmt_t{}, ::std::forward<target_type>(target),
		::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename target_type,
		  typename... argument_types>
inline constexpr void u32inplace_to(target_type &&target,
						    argument_types &&...arguments)
{
	::fast_io::fmt::details::inplace_to_builtin_with_rule<
		char32_t, format_literal>(
		brace_fmt_t{}, ::std::forward<target_type>(target),
		::std::forward<argument_types>(arguments)...);
}

// Percent grammar variants.
template <basic_fixed_string format_literal, typename target_type,
		  typename... argument_types>
inline constexpr void inplace_printf_to(target_type &&target,
							argument_types &&...arguments)
{
	::fast_io::fmt::details::inplace_to_builtin_with_rule<
		char, format_literal>(
		printf_fmt_t{}, ::std::forward<target_type>(target),
		::std::forward<argument_types>(arguments)...);
}

template <typename char_type, ::std::size_t extent, typename... argument_types>
auto inplace_to(char_type const (&)[extent], argument_types &&...) = delete;

} // namespace fast_io::fmt

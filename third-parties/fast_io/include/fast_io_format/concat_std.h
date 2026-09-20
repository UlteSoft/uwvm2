#pragma once

/*
 * Standard-string materialization facade (FMT-to-IO boundary).
 *
 * These functions differ from `concat_fast_io.h` only in their destination
 * family: the compiled format is lowered to the same typed IO component record
 * and materialized through the same concat operation, using the registered
 * `std::basic_string` strlike adapter. Parsing and field translation remain
 * FMT concerns; sizing, allocation, and leaf formatting remain IO/protocol
 * concerns.
 */

#include <string>

// fast_io_unit/string.h defines std::basic_string print adapters in terms of
// core reserve tags, so establish that dependency before opening the adapter.
#include "../fast_io_freestanding.h"
#include "../fast_io_unit/string.h"
#include "details/brace_rule.h"
#include "details/concat.h"
#include "details/printf_rule.h"

#include <cstddef>
#include <utility>

namespace fast_io::fmt
{

// Brace grammar, std::basic_string destination.
template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto concat_std(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char, ::std::string, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto wconcat_std(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		wchar_t, ::std::basic_string<wchar_t>, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto u8concat_std(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char8_t, ::std::u8string, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto u16concat_std(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char16_t, ::std::u16string, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto u32concat_std(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char32_t, ::std::u32string, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

// Percent grammar. `concatf` mirrors `printf`: the suffix selects percent
// conversions but never enables runtime parsing.
template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto concatf_std(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char, ::std::string, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto wconcatf_std(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		wchar_t, ::std::basic_string<wchar_t>, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto u8concatf_std(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char8_t, ::std::u8string, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto u16concatf_std(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char16_t, ::std::u16string, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] inline constexpr auto u32concatf_std(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char32_t, ::std::u32string, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

// Runtime arrays cannot become structural NTTPs, so every such spelling is
// deleted rather than parsed after program start.
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto concat_std(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto wconcat_std(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u8concat_std(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u16concat_std(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u32concat_std(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto concatf_std(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto wconcatf_std(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u8concatf_std(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u16concatf_std(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u32concatf_std(char_type const (&)[extent], argument_types &&...) = delete;

} // namespace fast_io::fmt

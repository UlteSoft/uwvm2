#pragma once

/*
 * fast_io-string materialization facade (FMT-to-IO boundary).
 *
 * These overloads apply the brace or printf grammar and lower the result to
 * ordinary printable components, then request materialization into a
 * `fast_io::basic_string`. Syntax interpretation belongs here; capacity
 * planning, allocation, component normalization, and printable CPO selection
 * belong to the shared IO concat engine. No stream or device protocol is
 * introduced by this adapter.
 */

// Declare the reserve-print protocol before instantiating the destination
// container; this header must remain usable without the hosted print facade.
#include "../fast_io_freestanding.h"
#include "../fast_io_dsal/string.h"
#include "details/brace_rule.h"
#include "details/concat.h"
#include "details/printf_rule.h"
#include "../fast_io_dsal/impl/misc/push_macros.h"

#include <cstddef>
#include <utility>

// These public functions are the outermost value-visible links of the format
// concat level.  Keeping the thin syntax facade in the caller is required for
// the IO-level `__builtin_constant_p` query below it; no format policy is
// introduced here, and the runtime false arm remains the checked concat path.
#pragma push_macro("FAST_IO_FMT_CONCAT_FACADE_INLINE")
#undef FAST_IO_FMT_CONCAT_FACADE_INLINE
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 13 <= __clang_major__)
#define FAST_IO_FMT_CONCAT_FACADE_INLINE FAST_IO_GNU_ALWAYS_INLINE
#else
#define FAST_IO_FMT_CONCAT_FACADE_INLINE
#endif

namespace fast_io::fmt
{

// Brace grammar, fast_io::basic_string destination.
template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] FAST_IO_FMT_CONCAT_FACADE_INLINE inline constexpr auto concat_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char, ::fast_io::string, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] FAST_IO_FMT_CONCAT_FACADE_INLINE inline constexpr auto wconcat_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		wchar_t, ::fast_io::wstring, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] FAST_IO_FMT_CONCAT_FACADE_INLINE inline constexpr auto u8concat_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char8_t, ::fast_io::u8string, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] FAST_IO_FMT_CONCAT_FACADE_INLINE inline constexpr auto u16concat_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char16_t, ::fast_io::u16string, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] FAST_IO_FMT_CONCAT_FACADE_INLINE inline constexpr auto u32concat_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char32_t, ::fast_io::u32string, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

// Percent grammar.
template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] FAST_IO_FMT_CONCAT_FACADE_INLINE inline constexpr auto concatf_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char, ::fast_io::string, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] FAST_IO_FMT_CONCAT_FACADE_INLINE inline constexpr auto wconcatf_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		wchar_t, ::fast_io::wstring, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] FAST_IO_FMT_CONCAT_FACADE_INLINE inline constexpr auto u8concatf_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char8_t, ::fast_io::u8string, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] FAST_IO_FMT_CONCAT_FACADE_INLINE inline constexpr auto u16concatf_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char16_t, ::fast_io::u16string, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
[[nodiscard]] FAST_IO_FMT_CONCAT_FACADE_INLINE inline constexpr auto u32concatf_fast_io(argument_types &&...arguments)
{
	return ::fast_io::fmt::details::concat_builtin_with_rule<
		char32_t, ::fast_io::u32string, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

// Runtime arrays are rejected for the same reason as the std destination.
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto concat_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto wconcat_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u8concat_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u16concat_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u32concat_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto concatf_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto wconcatf_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u8concatf_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u16concatf_fast_io(char_type const (&)[extent], argument_types &&...) = delete;
template <typename char_type, ::std::size_t extent, typename... argument_types>
auto u32concatf_fast_io(char_type const (&)[extent], argument_types &&...) = delete;

} // namespace fast_io::fmt

#pragma pop_macro("FAST_IO_FMT_CONCAT_FACADE_INLINE")
#include "../fast_io_dsal/impl/misc/pop_macros.h"

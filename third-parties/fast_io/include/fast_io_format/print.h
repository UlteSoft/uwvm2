#pragma once

/*
 * Public formatted-output facade (FMT-to-IO boundary).
 *
 * The functions in this file choose a format grammar, compile and lower the
 * literal, and pass the resulting typed component sequence to
 * `fast_io::io::print` or `fast_io::io::println`. Default-output overloads are
 * scenario conveniences around the same lowering path. This file does not
 * select reserve/scatter/context formatting strategies or issue writes; the IO
 * operation core makes those decisions after stream and argument
 * normalization. Value visibility is preserved across this boundary so the IO
 * level may still apply its compiler-constant policy.
 */

// The output front door is the only format component that needs hosted stream
// adapters and the default standard-output objects. Lowering itself remains
// reusable by the freestanding concat front doors.
#include "../fast_io.h"
#include "details/brace_rule.h"
#include "details/lower.h"
#include "details/printf_rule.h"

// `fast_io.h` restores public macro state. This header has a small measured
// set of constant-gate inlining boundaries, so use core's scoped portable
// attribute spelling and restore the caller's state again at the end.
#include "../fast_io_dsal/impl/misc/push_macros.h"

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

// A 16-way GCC 13--16 deletion grid proved that the four forwarding links are
// required for the explicit-output constant gate (1.97 ns with the complete
// chain versus 36.6 ns with a missing link); GCC 11/12 and Clang 17--23 kept
// those scalar explicit-output objects unchanged without a marker.  That experiment
// is not evidence for the complete default-output chain: a later strict
// standalone audit shows GCC 13--16 still outlining the two-operation lowering
// implementation before the default sink.  The default-sink boundary therefore
// remains a separate, unresolved lowering issue and is not used to justify
// these attributes. GCC 16 is the newest positive explicit-output endpoint, so
// that policy remains open until a measured reversal. MSVC retains its normal
// size model. A separate recursive `%.*f` active-condition deletion matrix
// proves that Clang 21--23 needs the four unprefixed value-visible format links
// below as part of a six-edge chain: deleting any one restores a reachable
// compiler-constant proxy or native precision formatter from the successful
// literal root. The complete candidate still fails on Clang 16--20 and grows
// text by 6.8--22.1 KiB there, so those releases must not inherit this policy.
#pragma push_macro("FAST_IO_FMT_GNU_CONSTANT_INNER_INLINE")
#undef FAST_IO_FMT_GNU_CONSTANT_INNER_INLINE
#if !defined(__clang__) && defined(__GNUC__) && __GNUC__ >= 13
#define FAST_IO_FMT_GNU_CONSTANT_INNER_INLINE FAST_IO_GNU_ALWAYS_INLINE
#else
#define FAST_IO_FMT_GNU_CONSTANT_INNER_INLINE
#endif
#pragma push_macro("FAST_IO_FMT_GCC_CALLBACK_INLINE")
#undef FAST_IO_FMT_GCC_CALLBACK_INLINE
#if (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 11) || \
	(defined(__clang__) && 21 <= __clang_major__)
#define FAST_IO_FMT_GCC_CALLBACK_INLINE FAST_IO_GNU_ALWAYS_INLINE
#else
#define FAST_IO_FMT_GCC_CALLBACK_INLINE
#endif
#pragma push_macro("FAST_IO_FMT_GCC_RULE_INLINE")
#undef FAST_IO_FMT_GCC_RULE_INLINE
#if (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 11) || \
	(defined(__clang__) && 21 <= __clang_major__)
#define FAST_IO_FMT_GCC_RULE_INLINE FAST_IO_GNU_ALWAYS_INLINE
#else
#define FAST_IO_FMT_GCC_RULE_INLINE
#endif
#pragma push_macro("FAST_IO_FMT_GCC_PRIMARY_INLINE")
#undef FAST_IO_FMT_GCC_PRIMARY_INLINE
#if (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 11) || \
	(defined(__clang__) && 21 <= __clang_major__)
#define FAST_IO_FMT_GCC_PRIMARY_INLINE FAST_IO_GNU_ALWAYS_INLINE
#else
#define FAST_IO_FMT_GCC_PRIMARY_INLINE
#endif
#pragma push_macro("FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE")
#undef FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE
#if (defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 11) || \
	(defined(__clang__) && 21 <= __clang_major__)
#define FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE FAST_IO_GNU_ALWAYS_INLINE
#else
#define FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE
#endif

namespace fast_io::fmt::details
{

/** @brief Recognizes an output stream without imposing a character domain. */
template <typename output_type>
concept format_output = requires(output_type &output) {
	typename ::std::remove_cvref_t<
		decltype(::fast_io::operations::output_stream_ref(output))>::output_char_type;
};

/** @brief Proves that an output reference consumes the requested code-unit domain. */
template <typename output_type, typename char_type>
concept format_output_for = format_output<output_type> &&
							::std::same_as<typename ::std::remove_cvref_t<decltype(::fast_io::operations::output_stream_ref(
											   ::std::declval<output_type &>()))>::output_char_type,
										   char_type>;

/**
 * @brief Sends a lowered component pack to the ordinary print operation core.
 *
 * This callback is the innermost of four value-visible links in the
 * format-to-print constant bridge. GCC's combination grid proves that removing
 * this marker makes the source opaque before IO's compiler-constant query. The
 * later active-condition audit extends only this innermost edge to GCC 11/12:
 * both releases otherwise outline this callback, so literal and unknown dynamic
 * precision records reach the same opaque helper and retain both proxy and
 * native graphs. The outer facade links remain on their independent GCC 13+
 * boundary. A callback-only Clang experiment is insufficient because the
 * other opaque links can mask its effect; deleting this edge from the complete
 * Clang 21--23 chain restores a forbidden reachable formatter. This joint
 * necessity, rather than an isolated instruction-count change, justifies the
 * Clang marker.
 */
template <typename output_type>
struct print_lowered_components
{
	output_type &&output;

	/** @brief Emits lowered components under the original output lifetime category. */
	template <typename... component_types>
	FAST_IO_FMT_GCC_CALLBACK_INLINE
	inline constexpr void operator()(component_types &&...components) const
	{
		// Formatting ends at typed components, including the empty component
		// pack. This IO boundary owns the complete-record semantics: an ordinary
		// empty run is a no-op, while an exact zero-argument status CPO may make
		// that run observable. Skipping this call in the format layer would both
		// bypass that provider contract and make `fmt::print<"">(out)` differ
		// from `io::print(out)`. The output is deliberately normalized here: the
		// named callback member preserves established lvalue strategy selection,
		// while the separate lifetime category still lets an original temporary
		// owner receive checked finish after successful emission.
		::fast_io::operations::basic_output_operation_guard<
			output_type &, output_type &&>
			guard{output};
		decltype(auto) outref{guard.ref()};
		::fast_io::operations::decay::
			print_freestanding_compiler_constant_pre_normalization<false>(
				outref, components...);
		guard.commit();
	}
};

/**
 * Emits a structural literal through one explicitly supplied grammar rule.
 *
 * This is the sole grammar-neutral output kernel. The rule participates only
 * in the compile/lower CPO lookup and is required to be an empty type token;
 * it therefore contributes no runtime parameter state. The callback receives
 * the final typed component pack and sends it through ordinary fast_io::print,
 * preserving the core alias, ABI decay, semantic-pack flattening, concat, and
 * syscall strategy selection.
 *
 * Requiring an explicit output here is a deliberate proof boundary. Built-in
 * facades perform the same device-first type classification as
 * fast_io::io::print before entering this kernel; a third-party rule frontend
 * must make that choice explicitly as well.
 *
 * This grammar-neutral bridge is the second measured GNU-frontend link.
 * The active-record call-graph audit extends this bridge to GCC 11/12: after
 * the callback is exposed, those releases otherwise outline this stateless
 * grammar hop and hide the caller's values from IO's constant query. The
 * broader facade remains on its separately measured GCC 13+ boundary. In the
 * Clang 21--23 complete-chain deletion matrix, removing this bridge alone
 * restores the mutually exclusive proxy/native graph. This bridge transports
 * values and introduces no formatting or output strategy.
 */
template <basic_fixed_string format_literal, format_grammar grammar_type,
		  typename output_type, typename... argument_types>
	requires format_output_for<output_type,
								   typename decltype(format_literal)::value_type>
FAST_IO_FMT_GCC_RULE_INLINE
inline constexpr void print_with_rule(
	grammar_type, output_type &&output, argument_types &&...arguments)
{
	using rule_type = ::std::remove_cvref_t<grammar_type>;
	::fast_io::fmt::details::lower_format_program<format_literal, rule_type>(
		::fast_io::fmt::details::print_lowered_components<output_type>{
			::std::forward<output_type>(output)},
		arguments...);
}


/** Selects the hosted default output for one explicit code-unit domain. */
template <typename char_type>
[[nodiscard]] inline constexpr auto default_format_output() noexcept
{
#if __has_include(<stdio.h>)
	// Match io::print's C-stdout boundary where fast_io exposes a portable C
	// observer. char16_t/char32_t have no such portable C stream and therefore
	// use the native descriptor observer below.
	if constexpr (::std::same_as<char_type, char>)
	{
		return ::fast_io::c_stdout();
	}
	else if constexpr (::std::same_as<char_type, wchar_t>)
	{
		return ::fast_io::wc_stdout();
	}
	else if constexpr (::std::same_as<char_type, char8_t>)
	{
		return ::fast_io::u8c_stdout();
	}
	else
#endif
		return ::fast_io::native_stdout<char_type>();
}

/**
 * Implements one built-in facade with io::print-compatible device selection.
 *
 * A stream object remains a stream even when its character domain is wrong;
 * the dedicated assertion below must diagnose that mismatch rather than
 * silently reclassifying the object as a value for default stdout.
 *
 * The value-carrying overload is the third measured GNU-frontend link for an
 * explicitly selected code-unit domain. Its zero-argument sibling has no
 * compiler-constant evidence to transport and remains ordinary inline.
 */
template <typename expected_char_type, basic_fixed_string format_literal,
		  format_grammar grammar_type, typename first_type,
		  typename... argument_types>
FAST_IO_FMT_GNU_CONSTANT_INNER_INLINE
inline constexpr void print_builtin_with_rule(
	grammar_type grammar, first_type &&first,
	argument_types &&...arguments)
{
	using literal_char_type = typename decltype(format_literal)::value_type;
	static_assert(::std::same_as<literal_char_type, expected_char_type>,
				  "fast_io format: the format literal character type does not match the selected print function");
	if constexpr (::fast_io::fmt::details::format_output<first_type>)
	{
		static_assert(
			::fast_io::fmt::details::format_output_for<first_type,
													   expected_char_type>,
			"fast_io format: output stream and format literal use different character types");
		if constexpr (::fast_io::fmt::details::format_output_for<
						  first_type, expected_char_type>)
		{
			::fast_io::fmt::details::print_with_rule<format_literal>(
				grammar, ::std::forward<first_type>(first),
				::std::forward<argument_types>(arguments)...);
		}
	}
	else
	{
		::fast_io::fmt::details::print_with_rule<format_literal>(
			grammar,
			::fast_io::fmt::details::default_format_output<expected_char_type>(),
			::std::forward<first_type>(first),
			::std::forward<argument_types>(arguments)...);
	}
}

template <typename expected_char_type, basic_fixed_string format_literal,
		  format_grammar grammar_type>
inline constexpr void print_builtin_with_rule(grammar_type grammar)
{
	using literal_char_type = typename decltype(format_literal)::value_type;
	static_assert(::std::same_as<literal_char_type, expected_char_type>,
				  "fast_io format: the format literal character type does not match the selected print function");
	::fast_io::fmt::details::print_with_rule<format_literal>(
		grammar,
		::fast_io::fmt::details::default_format_output<expected_char_type>());
}

/**
 * Implements the unprefixed io::print-compatible facade.
 *
 * An explicit first output lets the literal itself select any supported
 * character domain. Without an output, the unprefixed spelling retains
 * io::print's narrow C-stdout meaning; the prefixed family below supplies
 * default outputs for the other four domains.
 *
 * This output-first classifier is the third measured GNU-frontend link for the
 * unprefixed facade. The recursive active-record audit requires this exact
 * classifier on GCC 11/12 as well: after the grammar bridge is exposed, it is
 * the sole remaining opaque value-carrying edge. The public facade still
 * inlines naturally, so this evidence does not justify broadening its separate
 * GCC 13+ marker. Clang 21--23 also requires this classifier in the complete
 * six-edge condition-record chain; deleting it makes the selected dynamic-star
 * value opaque before the IO-level query. The zero-argument overload has no
 * value to preserve and remains ordinary inline.
 */
template <basic_fixed_string format_literal, format_grammar grammar_type,
		  typename first_type, typename... argument_types>
FAST_IO_FMT_GCC_PRIMARY_INLINE
inline constexpr void print_primary_with_rule(
	grammar_type grammar, first_type &&first,
	argument_types &&...arguments)
{
	using char_type = typename decltype(format_literal)::value_type;
	if constexpr (::fast_io::fmt::details::format_output<first_type>)
	{
		static_assert(
			::fast_io::fmt::details::format_output_for<first_type, char_type>,
			"fast_io format: output stream and format literal use different character types");
		if constexpr (::fast_io::fmt::details::format_output_for<
						  first_type, char_type>)
		{
			::fast_io::fmt::details::print_with_rule<format_literal>(
				grammar, ::std::forward<first_type>(first),
				::std::forward<argument_types>(arguments)...);
		}
	}
	else
	{
		static_assert(::std::same_as<char_type, char>,
					  "fast_io format: an unprefixed non-char format requires an explicit matching output stream");
		if constexpr (::std::same_as<char_type, char>)
		{
			::fast_io::fmt::details::print_with_rule<format_literal>(
				grammar,
				::fast_io::fmt::details::default_format_output<char>(),
				::std::forward<first_type>(first),
				::std::forward<argument_types>(arguments)...);
		}
	}
}

template <basic_fixed_string format_literal, format_grammar grammar_type>
inline constexpr void print_primary_with_rule(grammar_type grammar)
{
	using char_type = typename decltype(format_literal)::value_type;
	static_assert(::std::same_as<char_type, char>,
				  "fast_io format: an unprefixed non-char format requires an explicit matching output stream");
	if constexpr (::std::same_as<char_type, char>)
	{
		::fast_io::fmt::details::print_with_rule<format_literal>(
			grammar,
			::fast_io::fmt::details::default_format_output<char>());
	}
}

} // namespace fast_io::fmt::details

namespace fast_io::fmt
{

// Brace grammar. Function prefixes select only the code-unit domain; device
// selection remains the same output-first rule for every member of the family.
// Each value-carrying facade is the outermost measured GNU-frontend bridge
// link. GCC 11/12 require the unprefixed primary facade itself to remain in the
// source caller after the three internal links are exposed; this dedicated
// marker deliberately does not broaden the prefixed facade family without an
// equivalent call-graph proof. Clang 21--23 requires this unprefixed facade in
// the recursive `%.*f` chain, while the unmeasured prefixed facades retain their
// ordinary placement. The five code-unit domains and both grammars
// otherwise share the same implementation; no device or materialization policy
// is duplicated here.
template <basic_fixed_string format_literal, typename... argument_types>
FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE
inline constexpr void print(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_primary_with_rule<format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
FAST_IO_FMT_GNU_CONSTANT_INNER_INLINE
inline constexpr void wprint(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<wchar_t, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
FAST_IO_FMT_GNU_CONSTANT_INNER_INLINE
inline constexpr void u8print(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<char8_t, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
FAST_IO_FMT_GNU_CONSTANT_INNER_INLINE
inline constexpr void u16print(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<char16_t, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
FAST_IO_FMT_GNU_CONSTANT_INNER_INLINE
inline constexpr void u32print(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<char32_t, format_literal>(
		brace_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

// Percent grammar. The `f` suffix authorizes percent conversions; it is not a
// runtime-formatting mode. These value-carrying facades need the same bridge as
// their brace counterparts; the grammar owns no materialization policy.
template <basic_fixed_string format_literal, typename... argument_types>
FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE
inline constexpr void printf(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_primary_with_rule<format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
FAST_IO_FMT_GNU_CONSTANT_INNER_INLINE
inline constexpr void wprintf(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<wchar_t, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
FAST_IO_FMT_GNU_CONSTANT_INNER_INLINE
inline constexpr void u8printf(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<char8_t, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
FAST_IO_FMT_GNU_CONSTANT_INNER_INLINE
inline constexpr void u16printf(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<char16_t, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

template <basic_fixed_string format_literal, typename... argument_types>
FAST_IO_FMT_GNU_CONSTANT_INNER_INLINE
inline constexpr void u32printf(argument_types &&...arguments)
{
	::fast_io::fmt::details::print_builtin_with_rule<char32_t, format_literal>(
		printf_fmt_t{}, ::std::forward<argument_types>(arguments)...);
}

// Ordinary array format arguments cannot expose their contents as a template
// specialization. Every runtime-array spelling is therefore deleted instead
// of falling back to a runtime parser.
template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void print(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void print(output_type &&, char_type const (&)[extent],
				  argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void wprint(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void wprint(output_type &&, char_type const (&)[extent],
				   argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u8print(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u8print(output_type &&, char_type const (&)[extent],
					argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u16print(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u16print(output_type &&, char_type const (&)[extent],
					 argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u32print(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u32print(output_type &&, char_type const (&)[extent],
					 argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void printf(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void printf(output_type &&, char_type const (&)[extent],
				   argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void wprintf(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void wprintf(output_type &&, char_type const (&)[extent],
					argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u8printf(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u8printf(output_type &&, char_type const (&)[extent],
					 argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u16printf(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u16printf(output_type &&, char_type const (&)[extent],
					  argument_types &&...) = delete;

template <typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u32printf(char_type const (&)[extent], argument_types &&...) = delete;
template <typename output_type, typename char_type, ::std::size_t extent,
		  typename... argument_types>
inline void u32printf(output_type &&, char_type const (&)[extent],
					  argument_types &&...) = delete;

} // namespace fast_io::fmt

#pragma pop_macro("FAST_IO_FMT_GCC_CALLBACK_INLINE")
#pragma pop_macro("FAST_IO_FMT_GCC_RULE_INLINE")
#pragma pop_macro("FAST_IO_FMT_GCC_PRIMARY_INLINE")
#pragma pop_macro("FAST_IO_FMT_GCC_PRIMARY_FACADE_INLINE")
#pragma pop_macro("FAST_IO_FMT_GNU_CONSTANT_INNER_INLINE")

#include "../fast_io_dsal/impl/misc/pop_macros.h"

#pragma once

/*
 * Format-to-concat continuation (FMT-to-IO boundary).
 *
 * Common lowering supplies this file with an ordered pack of literal and
 * replacement components. The continuation performs no additional syntax work
 * and hands the complete pack to
 * `basic_general_concat_compiler_constant_checked_entry`, the IO-level
 * materialization boundary. Destination sizing, allocation, alias/forward
 * normalization, and printable CPO selection therefore remain shared with
 * non-format concat.
 */

#include "lower.h"
#include "../../fast_io_dsal/impl/misc/push_macros.h"

#include <type_traits>
#include <utility>

// Format concat owns syntax translation only, but every translated scalar must
// reach concat's IO-level builtin query while its original caller value is
// still visible. GCC 11--16 and Clang 13--23 otherwise outline at least one of
// these three stateless forwarding links. The marker selects no allocation or
// formatting strategy; the callback still delegates the complete typed record
// to the ordinary checked concat entry.
#pragma push_macro("FAST_IO_FMT_CONCAT_CONSTANT_INLINE")
#undef FAST_IO_FMT_CONCAT_CONSTANT_INLINE
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 13 <= __clang_major__)
#define FAST_IO_FMT_CONCAT_CONSTANT_INLINE FAST_IO_GNU_ALWAYS_INLINE
#else
#define FAST_IO_FMT_CONCAT_CONSTANT_INLINE
#endif

namespace fast_io::fmt::details
{

/** Named continuation that materializes a lowered component pack into the requested result string. */
template <typename result_type, ::std::integral char_type>
struct concat_lowered_components
{
	template <typename... component_types>
	[[nodiscard]] FAST_IO_FMT_CONCAT_CONSTANT_INLINE inline constexpr result_type
	operator()(component_types &&...components) const
	{
		if constexpr (sizeof...(component_types) == 0u)
		{
			return {};
		}
		else
		{
			return ::fast_io::basic_general_concat_compiler_constant_checked_entry<
				false, char_type, result_type>(
				::std::forward<component_types>(components)...);
		}
	}
};

/**
 * Materializes one compiled grammar into an explicitly selected string type.
 *
 * `result_type` is a policy chosen by the public destination facade, not a
 * syntax property. Keeping it independent from `grammar_type` prevents a
 * brace/percent cross-product in the lowering implementation. The grammar
 * object is an empty rule token used only for CPO selection, while every final
 * component is forwarded to fast_io's checked concat front door. Consequently
 * the ordinary concat concepts retain sole ownership of allocation, sizing,
 * alias normalization, and ABI transport decisions.
 *
 * The complete lowered program is always forwarded with `line == false`.
 * A terminal literal line feed is syntax data, not permission for the format
 * layer to select concat's line operation: changing that template argument can
 * select a different whole-run status CPO and is therefore observable even
 * when the emitted character sequence would be identical.
 */
template <typename result_type, basic_fixed_string format_literal,
		  format_grammar grammar_type, typename... argument_types>
[[nodiscard]] FAST_IO_FMT_CONCAT_CONSTANT_INLINE inline constexpr result_type concat_with_rule(
	grammar_type, argument_types &&...arguments)
{
	using char_type = typename decltype(format_literal)::value_type;
	using rule_type = ::std::remove_cvref_t<grammar_type>;
	return ::fast_io::fmt::details::lower_format_program<
		format_literal, rule_type>(
		::fast_io::fmt::details::concat_lowered_components<
			result_type, char_type>{},
		arguments...);
}

/** Applies one named character-domain facade to the common concat kernel. */
template <typename expected_char_type, typename result_type,
		  basic_fixed_string format_literal, format_grammar grammar_type,
		  typename... argument_types>
[[nodiscard]] FAST_IO_FMT_CONCAT_CONSTANT_INLINE inline constexpr result_type concat_builtin_with_rule(
	grammar_type grammar, argument_types &&...arguments)
{
	using literal_char_type = typename decltype(format_literal)::value_type;
	static_assert(::std::same_as<literal_char_type, expected_char_type>,
				  "fast_io format: the format literal character type does not match the selected concat function");
	return ::fast_io::fmt::details::concat_with_rule<
		result_type, format_literal>(
		grammar, ::std::forward<argument_types>(arguments)...);
}

} // namespace fast_io::fmt::details

#pragma pop_macro("FAST_IO_FMT_CONCAT_CONSTANT_INLINE")
#include "../../fast_io_dsal/impl/misc/pop_macros.h"

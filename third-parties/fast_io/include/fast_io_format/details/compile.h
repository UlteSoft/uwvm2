#pragma once

/*
 * Grammar compilation protocol and checked driver (FMT level).
 *
 * This header invokes an ADL grammar compiler for one structural literal,
 * verifies the returned program shape, and converts parser failures into
 * compile-time diagnostics. It is the open syntax-extension boundary between
 * grammar-specific parsers and grammar-independent lowering. Compilation
 * produces only structural format IR; it performs no IO.
 */

#include "../types.h"
#include "program.h"

#include <concepts>
#include <cstddef>
#include <type_traits>

namespace fast_io::fmt::details
{

namespace grammar_compile_adl
{

// This deleted declaration supplies an ordinary-lookup anchor only.  Every
// usable overload must arrive by ADL through the rule object, including the two
// built-in rules declared in brace_rule.h and printf_rule.h.  Consequently this
// protocol header has no knowledge of either token language.
template <auto, typename grammar_type>
void compile_format_program(grammar_type) = delete;

template <auto format_literal, typename grammar_type>
concept expression = requires {
	compile_format_program<format_literal>(
		::std::remove_cvref_t<grammar_type>{});
};

template <auto format_literal, typename grammar_type>
	requires expression<format_literal, grammar_type>
[[nodiscard]] consteval decltype(auto) invoke()
{
	return compile_format_program<format_literal>(
		::std::remove_cvref_t<grammar_type>{});
}

} // namespace grammar_compile_adl

/**
 * A grammar is usable only when its rule type opts into grammar overloads and
 * ADL supplies an immediate-program compiler for this exact literal domain.
 * `make_checked_program` invokes the CPO from a consteval function, so even a
 * customization spelled `constexpr` cannot move parsing to runtime.
 */
template <auto format_literal, typename grammar_type>
concept compilable_format_grammar =
	::fast_io::fmt::format_grammar<grammar_type> &&
	grammar_compile_adl::expression<format_literal, grammar_type>;

template <auto format_literal, typename grammar_tag>
[[nodiscard]] inline consteval auto make_checked_program() noexcept
{
	using grammar_type = ::std::remove_cvref_t<grammar_tag>;
	if constexpr (compilable_format_grammar<format_literal, grammar_type>)
	{
		constexpr auto program{
			grammar_compile_adl::invoke<format_literal, grammar_type>()};
		constexpr bool result_models_flat_program = requires {
			program.literal_storage;
			program.literal_runs;
			program.fields;
			program.operations;
			{ program.literal_size } -> ::std::convertible_to<::std::size_t>;
			{ program.literal_run_count } -> ::std::convertible_to<::std::size_t>;
			{ program.field_count } -> ::std::convertible_to<::std::size_t>;
			{ program.operation_count } -> ::std::convertible_to<::std::size_t>;
		};
		if constexpr (result_models_flat_program)
		{
			return program;
		}
		else
		{
			static_assert(result_models_flat_program,
						  "fast_io format: grammar compiler must diagnose its own syntax and return a flat format program");
			using char_type = typename decltype(format_literal)::value_type;
			return basic_format_program<char_type, format_literal.size()>{};
		}
	}
	else
	{
		static_assert(compilable_format_grammar<format_literal, grammar_type>,
					  "fast_io format: grammar rule has no ADL compile_format_program<literal>(rule) CPO");
		// Give the failed immediate instantiation a real return type.  Without
		// this unreachable value the variable-template cache becomes `void`, and
		// every later member access emits a distracting secondary diagnostic after
		// the useful CPO assertion above.
		using char_type = typename decltype(format_literal)::value_type;
		return basic_format_program<char_type, format_literal.size()>{};
	}
}

template <auto format_literal, typename grammar_tag>
inline constexpr auto checked_program{
	make_checked_program<format_literal, ::std::remove_cvref_t<grammar_tag>>()};

namespace format_argument_list_validation_adl
{

// An argument-list contract is grammar-owned. Source types are explicit
// template arguments rather than members of a function-argument carrier: this
// preserves arrays, cv/ref qualification, and named metadata without adding
// their namespaces to ADL. Only the grammar token selects the customization,
// so a supplied value type cannot hide mandatory brace/printf validation by
// introducing an ambiguous overload.
template <auto, typename...>
void validate_format_argument_list() = delete;

template <auto format_literal, typename grammar_type,
		  typename... argument_types>
concept expression = requires {
	{
		validate_format_argument_list<format_literal, argument_types...>(
			::std::remove_cvref_t<grammar_type>{})
	} -> ::std::same_as<bool>;
};

template <auto format_literal, typename grammar_type,
		  typename... argument_types>
	requires expression<format_literal, grammar_type, argument_types...>
[[nodiscard]] inline consteval bool invoke() noexcept(noexcept(
	validate_format_argument_list<format_literal, argument_types...>(
		::std::remove_cvref_t<grammar_type>{})))
{
	return validate_format_argument_list<format_literal, argument_types...>(
		::std::remove_cvref_t<grammar_type>{});
}

} // namespace format_argument_list_validation_adl

} // namespace fast_io::fmt::details

namespace fast_io::fmt
{

/**
 * Public proof that a rule can compile one exact literal.
 *
 * `format_grammar` is intentionally only the cheap tag gate used during
 * overload resolution.  This stronger concept requires the syntax compiler
 * CPO for the literal itself.  Frontends and third-party rules should constrain
 * on this concept so an unrelated marker type cannot accidentally reach the
 * emitter.  It says nothing about braces or percent signs: those are properties
 * of the two built-in CPO overloads, not of print/concat.
 */
template <basic_fixed_string format_literal, typename grammar_type>
concept format_rule_for =
	::fast_io::fmt::details::compilable_format_grammar<
		format_literal, grammar_type>;

} // namespace fast_io::fmt

#pragma once

/*
 * Open replacement-lowering protocol (FMT level).
 *
 * A compiled field, its structural source context, and the selected typed
 * argument are presented to ADL rule providers through this file. Providers
 * return a rule token whose lowering emits typed IO components. This is the
 * extension boundary for format semantics, not a printable CPO: the returned
 * components must ultimately satisfy the ordinary IO-level print protocols.
 */

#include <concepts>
#include <limits>
#include <type_traits>
#include <utility>

#include "../../fast_io_concept.h"

// This detail header can be entered after a public umbrella has restored user
// macros, so effect specifiers are scoped explicitly to this implementation.
#include "../../fast_io_dsal/impl/misc/push_macros.h"

namespace fast_io::fmt::details
{

/**
 * Structural context shared by every replacement-rule provider.
 *
 * The literal and field are NTTPs rather than data members.  A rule may inspect
 * the complete parsed source during constant evaluation, but the context itself
 * is empty and cannot retain a parser cursor, format pointer, or erased formatter
 * in generated code.  Defining the context with the protocol also lets nested
 * consumers (notably range elements) use the same rule set without depending on
 * a concrete brace/printf provider and creating an include cycle.
 */
template <auto format_literal, auto field>
struct basic_format_replacement_context
{
	using char_type = typename decltype(format_literal)::value_type;
	static inline constexpr auto literal{format_literal};
	static inline constexpr auto replacement{field};

	/*
	 * Keep the poison pills inside the associated context instead of ordinary
	 * namespace lookup. GCC 11 otherwise stops at an earlier zero-argument
	 * declaration and misses rule providers which ADL must add at instantiation.
	 * The conversion wrapper makes every typed provider a better overload; when
	 * no provider exists, the deleted hidden friend still closes the capability.
	 */
	struct adl_fallback_argument
	{
		template <typename value_type>
		constexpr adl_fallback_argument(value_type &&) noexcept;
	};

	friend void format_replacement_rule_type(
		adl_fallback_argument, basic_format_replacement_context,
		adl_fallback_argument) = delete;

	friend void format_replacement_rule_define(
		adl_fallback_argument, adl_fallback_argument,
		basic_format_replacement_context, adl_fallback_argument,
		adl_fallback_argument) = delete;
};

/**
 * Proves that a selected rule is a type token rather than hidden runtime state.
 *
 * Phase one advertises a rule through `io_type_t<R>`, and phase two names R
 * through another such carrier without constructing R.  Requiring R to be a
 * complete, empty, trivial, nothrow-default-constructible object makes the
 * type-token convention mechanically checkable.  In particular, a provider
 * cannot accidentally model a parser cursor, locale, allocator, or value-
 * dependent decision as rule state and then expect it to survive lowering.
 */
template <typename rule_type>
inline consteval bool format_replacement_rule_token_impl() noexcept
{
	using normalized_type = ::std::remove_cvref_t<rule_type>;
	if constexpr (!::std::same_as<rule_type, normalized_type> ||
				  !::std::is_object_v<normalized_type> ||
				  ::std::is_array_v<normalized_type> ||
				  !requires { sizeof(normalized_type); })
	{
		// Testing standard construction traits on an incomplete class is not a
		// portable false query, so completeness is established before those
		// traits are formed.
		return false;
	}
	else
	{
		return ::std::default_initializable<normalized_type> &&
			   ::std::is_empty_v<normalized_type> &&
			   ::std::is_nothrow_default_constructible_v<normalized_type> &&
#if defined(__HERBCEPTIONS__)
			   // Standard nothrow traits in the experimental frontend observe
			   // unwinding only. The orthogonal deterministic constructor effect
			   // must also be absent for this protocol token contract.
			   !::std::is_herbceptions_throws_constructible_v<normalized_type> &&
#endif
			   ::std::is_trivially_copyable_v<normalized_type> &&
			   ::std::is_trivially_destructible_v<normalized_type>;
	}
}

template <typename rule_type>
concept format_replacement_rule_token =
	format_replacement_rule_token_impl<rule_type>();

template <typename advertisement_type>
struct format_replacement_rule_advertisement_traits
{};

template <typename rule_type>
struct format_replacement_rule_advertisement_traits<
	::fast_io::io_type_t<rule_type>>
{
	using type = rule_type;
};

/**
 * Accepts precisely fast_io's existing type-advertisement carrier.
 *
 * Reusing `io_type_t` is material rather than cosmetic.  Its template argument
 * contributes the source/rule namespaces to ADL while the carrier itself has no
 * data, so both protocol phases remain extensible without passing an erased
 * callback or manufacturing a runtime registry.
 */
template <typename advertisement_type>
concept format_replacement_rule_advertisement =
	requires {
		typename format_replacement_rule_advertisement_traits<
			::std::remove_cvref_t<advertisement_type>>::type;
	} &&
	format_replacement_rule_token<
		typename format_replacement_rule_advertisement_traits<
			::std::remove_cvref_t<advertisement_type>>::type>;

namespace format_replacement_rule_type_adl
{

/**
 * The result of phase-one rule selection.
 *
 * `value_type` is the exact named-parameter expression type. It is therefore
 * an lvalue reference, but retains the source object's cv-qualification and
 * any array extent instead of applying language decay. This deliberately
 * matches fast_io::io::print, which aliases named arguments rather than the
 * caller's earlier value category. The source reaches ADL only through
 * `io_type_t<value_type>`; phase one reads no value and makes no ABI transport
 * decision.
 */
template <typename advertisement_type, bool valid>
struct selected_advertisement
{};

template <typename advertisement_type>
struct selected_advertisement<advertisement_type, true>
{
	using rule_type =
		typename format_replacement_rule_advertisement_traits<
			advertisement_type>::type;
};

template <typename advertisement_type, bool selector_is_admissible>
inline consteval bool valid_selected_advertisement_impl() noexcept
{
	// A reference result could point at provider-owned state even though its
	// referred type is `io_type_t<R>`.  Requiring the carrier itself by value
	// closes that otherwise subtle state-smuggling route.
	return selector_is_admissible &&
		   ::std::same_as<advertisement_type,
						  ::std::remove_cvref_t<advertisement_type>> &&
		   format_replacement_rule_advertisement<advertisement_type>;
}

#if defined(__HERBCEPTIONS__)
template <auto>
struct constant_selection_proof
{};

template <typename grammar_type, auto format_literal, auto field,
		  typename value_type>
concept available_selection = requires {
	format_replacement_rule_type(
		::std::remove_cvref_t<grammar_type>{},
		basic_format_replacement_context<format_literal, field>{},
		::fast_io::io_type_t<value_type>{});
};

template <typename grammar_type, auto format_literal, auto field,
		  typename value_type>
concept constant_evaluable_selection =
	available_selection<grammar_type, format_literal, field, value_type> &&
	requires {
		typename constant_selection_proof<format_replacement_rule_type(
			::std::remove_cvref_t<grammar_type>{},
			basic_format_replacement_context<format_literal, field>{},
			::fast_io::io_type_t<value_type>{})>;
	};

/**
 * Evaluates phase-one selection without introducing a run-time effect edge.
 *
 * A selector transports only an empty structural type token. A successful
 * constant evaluation is therefore the relevant proof even when its
 * declaration conservatively carries `throws`; the constexpr local consumes
 * that effect during translation, and this plain wrapper has no generated
 * error-result ABI. A selector which actually fails cannot initialize the
 * local and is removed during substitution.
 */
template <typename grammar_type, auto format_literal, auto field,
		  typename value_type>
	requires constant_evaluable_selection<
		grammar_type, format_literal, field, value_type>
[[nodiscard]] consteval auto evaluate_selection() noexcept
{
	constexpr auto result{format_replacement_rule_type(
		::std::remove_cvref_t<grammar_type>{},
		basic_format_replacement_context<format_literal, field>{},
		::fast_io::io_type_t<value_type>{})};
	return result;
}
#endif

template <typename grammar_type, auto format_literal, auto field,
		  typename value_type, typename = void>
struct selection
{};

template <typename grammar_type, auto format_literal, auto field,
		  typename value_type>
struct selection<grammar_type, format_literal, field, value_type,
#if defined(__HERBCEPTIONS__)
				 ::std::void_t<decltype(evaluate_selection<
										grammar_type, format_literal, field, value_type>())>>
#else
				 ::std::void_t<decltype(format_replacement_rule_type(
					 ::std::remove_cvref_t<grammar_type>{},
					 basic_format_replacement_context<format_literal, field>{},
					 ::fast_io::io_type_t<value_type>{}))>>
#endif
	: selected_advertisement<
		  decltype(format_replacement_rule_type(
			  ::std::remove_cvref_t<grammar_type>{},
			  basic_format_replacement_context<format_literal, field>{},
			  ::fast_io::io_type_t<value_type>{})),
		  valid_selected_advertisement_impl<decltype(format_replacement_rule_type(
												::std::remove_cvref_t<grammar_type>{},
												basic_format_replacement_context<format_literal, field>{},
												::fast_io::io_type_t<value_type>{})),
#if defined(__HERBCEPTIONS__)
											true>()>
#else
											noexcept(format_replacement_rule_type(
												::std::remove_cvref_t<grammar_type>{},
												basic_format_replacement_context<format_literal, field>{},
												::fast_io::io_type_t<value_type>{}))>()>
#endif
{};

template <typename grammar_type, auto format_literal, auto field,
		  typename value_type>
concept expression = requires {
	typename selection<grammar_type, format_literal, field,
					   value_type>::rule_type;
};

template <typename grammar_type, auto format_literal, auto field,
		  typename value_type>
using selected_rule_t =
	typename selection<grammar_type, format_literal, field,
					   value_type>::rule_type;

} // namespace format_replacement_rule_type_adl

namespace format_replacement_rule_adl
{

template <typename grammar_type, auto format_literal, auto field,
		  typename value_type, typename argument_pack_type>
concept expression =
	format_replacement_rule_type_adl::expression<
		grammar_type, format_literal, field, value_type> &&
	requires(argument_pack_type &arguments) {
		format_replacement_rule_define(
			::fast_io::io_type_t<
				format_replacement_rule_type_adl::selected_rule_t<
					grammar_type, format_literal, field, value_type>>{},
			::std::remove_cvref_t<grammar_type>{},
			basic_format_replacement_context<format_literal, field>{},
			::std::declval<value_type>(), arguments);
		requires(!::std::is_void_v<::std::remove_cv_t<
					 ::std::remove_reference_t<decltype(format_replacement_rule_define(
						 ::fast_io::io_type_t<
							 format_replacement_rule_type_adl::selected_rule_t<
								 grammar_type, format_literal, field, value_type>>{},
						 ::std::remove_cvref_t<grammar_type>{},
						 basic_format_replacement_context<format_literal, field>{},
						 ::std::declval<value_type>(), arguments))>>>);
	};

template <typename grammar_type, auto format_literal, auto field,
		  typename value_type, typename argument_pack_type>
inline constexpr bool invoke_nothrow_v = noexcept(
	format_replacement_rule_define(
		::fast_io::io_type_t<
			format_replacement_rule_type_adl::selected_rule_t<
				grammar_type, format_literal, field, value_type>>{},
		::std::remove_cvref_t<grammar_type>{},
		basic_format_replacement_context<format_literal, field>{},
		::std::declval<value_type>(),
		::std::declval<argument_pack_type &>()));

/** Classifies the deterministic effect of the exact phase-two ADL call. */
template <typename grammar_type, auto format_literal, auto field,
		  typename value_type, typename argument_pack_type>
inline constexpr bool invoke_herbceptions_throws_v =
#if defined(__HERBCEPTIONS__)
	throws((
	format_replacement_rule_define(
		::fast_io::io_type_t<
			format_replacement_rule_type_adl::selected_rule_t<
				grammar_type, format_literal, field, value_type>>{},
		::std::remove_cvref_t<grammar_type>{},
		basic_format_replacement_context<format_literal, field>{},
		::std::declval<value_type>(),
		::std::declval<argument_pack_type &>())));
#else
	false;
#endif

/**
 * Executes only the rule chosen in phase one.
 *
 * The selected rule is passed as `io_type_t<R>`, so this call contains no rule
 * object and cannot require storage. The stable named-parameter expression is
 * forwarded without cv/array decay. This is essential for fast_io: repeated
 * fields cannot consume one rvalue twice, while aliasing, status-print hooks,
 * and target-ABI value/reference transport still remain decisions of the
 * eventual core backend rather than guesses made by the format frontend.
 */
template <typename grammar_type, auto format_literal, auto field,
		  typename value_type, typename argument_pack_type>
	requires expression<grammar_type, format_literal, field,
						value_type &&, argument_pack_type>
[[nodiscard]] inline constexpr decltype(auto) invoke(
	value_type &&value, argument_pack_type &arguments)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
	(invoke_herbceptions_throws_v<
		grammar_type, format_literal, field, value_type &&,
		argument_pack_type>),
	invoke_nothrow_v<grammar_type, format_literal, field,
								 value_type &&, argument_pack_type>)
{
	return format_replacement_rule_define(
		::fast_io::io_type_t<
			format_replacement_rule_type_adl::selected_rule_t<
				grammar_type, format_literal, field, value_type &&>>{},
		::std::remove_cvref_t<grammar_type>{},
		basic_format_replacement_context<format_literal, field>{},
		::std::forward<value_type>(value), arguments);
}

} // namespace format_replacement_rule_adl

/**
 * Complete two-phase replacement capability.
 *
 * Overload resolution must first yield one valid, stateless rule type and the
 * corresponding five-argument define CPO must then accept the exact value and
 * argument-pack expressions.  This conjunction is the sole admission rule;
 * generic lowering does not need a brace/printf switch or knowledge of scalar,
 * range, chrono, or user-defined formatter categories.
 */
template <typename grammar_type, auto format_literal, auto field,
		  typename value_type, typename argument_pack_type>
concept format_replacement_rule_for =
	format_replacement_rule_adl::expression<grammar_type, format_literal,
											field, value_type, argument_pack_type>;

} // namespace fast_io::fmt::details

#include "../../fast_io_dsal/impl/misc/pop_macros.h"

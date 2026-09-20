#pragma once

/*
 * User-defined format-field extension protocol (FMT level).
 *
 * Custom parsers receive a structural view of one replacement specification
 * and return structural state that later lowers a typed value to ordinary IO
 * components. The protocol extends format-language interpretation without
 * creating a second output system: produced objects still pass through normal
 * print aliasing, semantic normalization, printable CPOs, and device transfer.
 */

#include "program.h"

// The format frontend is entered after the core umbrella has restored the
// caller's macros. Re-enter fast_io's internal effect-specifier scope locally.
#include "../../fast_io_dsal/impl/misc/push_macros.h"

#include <concepts>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace fast_io::fmt
{

/**
 * An immediate, non-owning view of the source consumed by a custom formatter.
 *
 * `format_literal` and `source` are non-type template arguments, so neither a
 * pointer to the format string nor a parse cursor is stored in a runtime
 * object.  The type-only observers are `consteval`; indexed access is
 * `constexpr` so a portable C++20 provider can be called through dependent ADL
 * on early frontends.  Core still invokes that provider only while proving its
 * structural result as an NTTP, so this does not create a runtime parse path.
 * The caller chooses the slice; the brace frontend normally supplies the
 * type-specific suffix which remains after its common field grammar.
 *
 * The interface deliberately exposes code units without interpreting them as
 * ASCII.  A customization can consequently compare against
 * `arithmetic_char_literal_v` (or its own execution-character mapping) and is
 * usable by all five standard character domains, including EBCDIC execution
 * character sets.
 *
 * A future C++26 reflection frontend may generate `format_parse_define`
 * overloads from reflected members and return the same structural state used
 * here.  Reflection therefore changes state construction, not the runtime ABI
 * or the lowering contract.  No experimental reflection syntax appears in
 * this C++20 header; such syntax must remain behind the implementation's
 * feature-test boundary in a separate frontend.
 */
template <basic_fixed_string format_literal, auto source>
struct basic_custom_format_parse_context
{
	using char_type = typename decltype(format_literal)::value_type;

	static inline constexpr auto literal{format_literal};
	static inline constexpr auto source_slice{source};

	static_assert(source.offset <= format_literal.size(),
				  "fast_io format: custom formatter source starts past the format literal");
	static_assert(source.size <= format_literal.size() - source.offset,
				  "fast_io format: custom formatter source extends past the format literal");

	[[nodiscard]] inline static consteval ::std::size_t size() noexcept
	{
		return source.size;
	}

	[[nodiscard]] inline static consteval bool empty() noexcept
	{
		return source.size == 0u;
	}

	/// Maps a formatter-local index back to the original literal for diagnostics.
	[[nodiscard]] inline static consteval ::std::size_t
	source_position(::std::size_t index) noexcept
	{
		return source.offset + index;
	}

	/**
	 * Returns one source code unit.  Bounds remain an immediate-function
	 * precondition: an invalid access is diagnosed while the CPO is evaluated
	 * and cannot become undefined behaviour in generated code.
	 */
	[[nodiscard]] inline constexpr char_type operator[](::std::size_t index) const
	{
		if (index >= source.size)
		{
			// This immediate function has no runtime path.  The fail-fast call is
			// deliberately non-constant-evaluable and therefore makes an invalid
			// parser access ill-formed without introducing an exception model.
			::fast_io::fast_terminate();
		}
		return format_literal[source.offset + index];
	}

	template <::std::size_t index>
	[[nodiscard]] inline static consteval char_type get() noexcept
	{
		static_assert(index < source.size,
					  "fast_io format: custom formatter parser read past its source slice");
		return format_literal[source.offset + index];
	}
};

/**
 * Empty runtime tag carrying a custom formatter's parsed state in its type.
 *
 * `parsed_state` has already passed the structural-NTTP proof before this tag
 * is formed.  Consequently a call to `format_alias_define(tag, value)` receives
 * no format pointer, cursor, virtual formatter, or type-erased argument.  The
 * immediate accessor is an ergonomic spelling for `if constexpr` in the ADL
 * CPO; it cannot be called as a runtime state lookup and adds no data member or
 * static state object to the tag.
 */
template <format_character char_type_, auto parsed_state>
struct basic_custom_format_state_t
{
	using char_type = char_type_;

	[[nodiscard]] inline static consteval auto state() noexcept
	{
		return parsed_state;
	}
};

/**
 * Terminal tag for a format_as customization which explicitly opts in to
 * compile-time rendering.
 *
 * The tag has no state.  In particular, it does not invoke format_as by
 * itself: the customization receives the original constant value and remains
 * responsible for producing exactly the final output code units.
 */
template <format_character char_type_>
struct basic_static_format_as_t
{
	using char_type = char_type_;
};

/** The checked value of one literal or argument-supplied format parameter. */
struct static_format_parameter_value
{
	::std::size_t value{};
	bool present{};
	bool negative{};

	constexpr bool operator==(
		static_format_parameter_value const &) const noexcept = default;
};

/**
 * Complete compile-time context for a terminal static-output customization.
 *
 * `specification` preserves the parsed common grammar verbatim, including
 * presentation, fill, alignment, sign, locale and source slices.  `width` and
 * `precision` are the already resolved parameter values; a customization
 * never has to inspect the argument pack.  `depth` is part of the type so a
 * recursive customization cannot accidentally restart its recursion budget.
 * The context is intentionally an aggregate and carries no runtime state.
 */
template <auto specification_, ::std::size_t depth_>
struct basic_static_format_context_t
{
	static inline constexpr auto specification{specification_};
	static inline constexpr ::std::size_t depth{depth_};

	using char_type = ::std::remove_cv_t<::std::remove_reference_t<
		decltype(specification.fill[0u])>>;

	static_format_parameter_value width{};
	static_format_parameter_value precision{};
};

} // namespace fast_io::fmt

namespace fast_io::fmt::details
{

/** Static custom output uses the same bounded nesting policy as aggregates. */
inline constexpr ::std::size_t static_format_recursion_limit{8u};

namespace static_format_output_adl
{

template <typename formatter_type, typename = void>
struct formatter_character_probe
{
	inline static constexpr bool valid{};
};

template <typename formatter_type>
struct formatter_character_probe<formatter_type,
								 ::std::void_t<typename ::std::remove_cvref_t<formatter_type>::char_type>>
{
	using type = typename ::std::remove_cvref_t<formatter_type>::char_type;
	inline static constexpr bool valid{
		::fast_io::fmt::format_character<type>};
};

template <typename formatter_type>
concept formatter_tag = formatter_character_probe<formatter_type>::valid;

template <typename formatter_type>
	requires formatter_tag<formatter_type>
using formatter_char_type =
	typename formatter_character_probe<formatter_type>::type;

/**
 * Strict terminal-output protocol admission.
 *
 * Both CPOs must be found by ADL, use the formatter's exact character domain,
 * return exactly size_t / Char*, and be noexcept.  Constant evaluability is
 * deliberately not tested by this shape concept: finding the correctly shaped
 * protocol is an explicit opt-in. A portable provider declares both functions
 * `constexpr`; the consteval wrappers below then prove the selected calls and
 * turn a non-constant body into a contract diagnostic instead of silently
 * selecting a dynamic formatter. No ordinary-lookup seed is declared here:
 * these are dependent calls inside an isolated namespace, so ADL is the only
 * candidate source. This also avoids GCC 11 suppressing ADL after seeing a
 * deleted zero-argument declaration in the requires-expression.
 */
template <typename context_type, typename formatter_type, typename value_type>
concept expression =
	formatter_tag<formatter_type> &&
	requires {
		typename ::std::remove_cvref_t<context_type>::char_type;
	} &&
	::std::same_as<
		typename ::std::remove_cvref_t<context_type>::char_type,
		formatter_char_type<formatter_type>> &&
	(context_type::depth < static_format_recursion_limit) &&
	requires(value_type value,
			 formatter_char_type<formatter_type> *output) {
		// Compound requirements perform dependent ADL and prove both exact result
		// type and non-throwing shape without evaluating a fabricated value.
		{
			format_static_reserve_size(
				context_type{}, formatter_type{}, value)
		} noexcept -> ::std::same_as<::std::size_t>;
		{
			format_static_reserve_define(
				context_type{}, formatter_type{}, output, value)
		} noexcept -> ::std::same_as<
			formatter_char_type<formatter_type> *>;
	};

template <typename context_type, typename formatter_type, typename value_type>
	requires expression<context_type, formatter_type, value_type &>
[[nodiscard]] inline consteval ::std::size_t size(
	context_type context, value_type &value) noexcept
{
	return format_static_reserve_size(context, formatter_type{}, value);
}

template <typename context_type, typename formatter_type, typename char_type,
		  typename value_type>
	requires expression<context_type, formatter_type, value_type &> &&
			 ::std::same_as<char_type, formatter_char_type<formatter_type>>
[[nodiscard]] inline consteval char_type *define(
	context_type context, char_type *output, value_type &value) noexcept
{
	return format_static_reserve_define(
		context, formatter_type{}, output, value);
}

} // namespace static_format_output_adl

/**
 * Delegates default-format admission to fast_io's public print concept.
 * Keeping this proof beside the format customization CPOs gives top-level,
 * nested range, and future grammar rules one definition of "identity"; none
 * of them needs to enumerate reserve/scatter/semantic printable categories.
 */
template <typename char_type, typename value_type>
concept format_backend_identity_printable =
	::fast_io::fmt::format_character<char_type> &&
	::fast_io::operations::defines::print_freestanding_params_okay<
		char_type, value_type>;

namespace custom_format_adl
{

// These ellipsis poison pills stop ordinary lookup in this namespace while
// still allowing dependent argument-dependent lookup to find the user's
// customization.  Ellipsis is intentional: GCC 13 diagnoses a zero-argument
// poison pill while forming the structural NTTP probe, whereas a forwarding-
// reference catch-all is a better match than a perfectly valid user CPO taking
// `T const&` and consequently rejects ordinary run-time custom values.
template <typename...>
void format_parse_define(...) = delete;
template <typename...>
void format_alias_define(...) = delete;
template <typename...>
void format_as(...) = delete;

template <auto format_literal, auto source, typename value_type>
concept parse_expression = requires {
	format_parse_define(
		::fast_io::io_type_t<::std::remove_cvref_t<value_type>>{},
		::fast_io::fmt::basic_custom_format_parse_context<format_literal, source>{});
};

template <auto parsed_state>
struct structural_state_proof
{};

/**
 * Forming this specialization is the relevant C++20 proof: the parser result
 * must be a constant expression of a structural type.  Merely requiring a
 * `constexpr` return type would be insufficient because it would still admit
 * a runtime call or a state containing an ineligible pointer/reference.
 */
template <bool parser_is_available, auto format_literal, auto source,
		  typename value_type>
struct structural_parse_probe : ::std::false_type
{};

/**
 * Delays the NTTP proof until ordinary constraint substitution has established
 * that ADL really found a parser.  This two-stage form is not merely a compiler
 * workaround: it prevents the poison-pill declaration from becoming part of a
 * dependent alias specialization.  Some compilers eagerly instantiate such an
 * alias while normalizing a concept conjunction, even though the preceding
 * availability constraint is false.  Selecting this specialization first
 * gives the language's class-template partial-specialization rules an explicit
 * substitution boundary and therefore keeps "no customization" a clean false
 * concept on every supported frontend.
 */
template <auto format_literal, auto source, typename value_type>
struct structural_parse_probe<true, format_literal, source, value_type>
{
	inline static constexpr bool value = requires {
		typename structural_state_proof<format_parse_define(
			::fast_io::io_type_t<::std::remove_cvref_t<value_type>>{},
			::fast_io::fmt::basic_custom_format_parse_context<format_literal, source>{})>;
	};
};

template <auto format_literal, auto source, typename value_type>
concept structural_parse_expression =
	structural_parse_probe<
		parse_expression<format_literal, source, value_type>,
		format_literal, source, value_type>::value;

template <auto format_literal, auto source, typename value_type>
	requires structural_parse_expression<format_literal, source, value_type>
[[nodiscard]] consteval auto parse_state()
{
	// Parsing is a type-only policy. Materializing the structural result as a
	// constant consumes a successful conservative Herbception effect during
	// translation and can never add an error-result edge to generated code.
	constexpr auto result{format_parse_define(
		::fast_io::io_type_t<::std::remove_cvref_t<value_type>>{},
		::fast_io::fmt::basic_custom_format_parse_context<format_literal, source>{})};
	return result;
}

template <typename state_tag, typename value_type>
concept alias_expression = requires(value_type &&value) {
	format_alias_define(state_tag{}, ::std::forward<value_type>(value));
};

/** Classifies the deterministic effect of the exact ADL alias expression. */
template <typename state_tag, typename value_type>
inline constexpr bool alias_herbceptions_throws =
#if defined(__HERBCEPTIONS__)
	throws((format_alias_define(state_tag{},
								::std::declval<value_type &&>())));
#else
	false;
#endif

template <typename state_tag, typename value_type>
	requires alias_expression<state_tag, value_type>
[[nodiscard]] inline constexpr decltype(auto) alias(
	value_type &&value) FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
	(alias_herbceptions_throws<state_tag, value_type>),
	noexcept(format_alias_define(
		state_tag{}, ::std::forward<value_type>(value))))
{
	// Preserve the exact source and result categories across the
	// deterministic-error boundary. A successful reference result remains the
	// provider's reference; the failure channel does not authorize
	// materialization or ownership.
	// In particular, do not call
	// io_print_alias/io_print_forward here: the selected print or concat backend
	// owns the ABI-specific decay decision and must still see the semantic node.
	return format_alias_define(state_tag{}, ::std::forward<value_type>(value));
}

template <typename value_type>
concept format_as_expression = requires(value_type &&value) {
	format_as(::std::forward<value_type>(value));
};

/** Classifies the deterministic effect of the exact ADL format_as expression. */
template <typename value_type>
inline constexpr bool format_as_herbceptions_throws =
#if defined(__HERBCEPTIONS__)
	throws((format_as(::std::declval<value_type &&>())));
#else
	false;
#endif

template <bool customization_is_available, typename value_type>
struct format_as_probe
{
	inline static constexpr bool nonrecursive{};
};

/**
 * As with the custom-parser probe above, result-type formation is intentionally
 * isolated behind an availability specialization.  Besides making the
 * negative concept cheap, this guarantees that a type with no `format_as` CPO
 * never attempts to instantiate the deleted ordinary-lookup poison pill.
 */
template <typename value_type>
struct format_as_probe<true, value_type>
{
	using result_type = decltype(format_as(::std::declval<value_type>()));

	inline static constexpr bool nonrecursive =
		!::std::same_as<::std::remove_cvref_t<value_type>,
						::std::remove_cvref_t<result_type>>;
};

template <typename value_type>
	requires format_as_expression<value_type>
[[nodiscard]] inline constexpr decltype(auto) as(value_type &&value)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		format_as_herbceptions_throws<value_type>,
		noexcept(format_as(::std::forward<value_type>(value))))
{
	// The return category is part of the format_as protocol. Deterministic
	// failure changes only the error path, so a successful lvalue or xvalue
	// reference is forwarded without decay or an identity-changing temporary.
	return format_as(::std::forward<value_type>(value));
}

} // namespace custom_format_adl

/** True when ADL exposes a type-directed compile-time parser CPO. */
template <auto format_literal, auto source, typename value_type>
concept custom_format_parse_expression =
	custom_format_adl::parse_expression<format_literal, source, value_type>;

/**
 * True only when the parser call can itself be encoded as an NTTP.
 *
 * C++ has no reflection facility which asks whether a function was declared
 * with the `consteval` keyword.  Requiring the result as a template argument is
 * the stronger behavioural property needed here: every accepted invocation is
 * necessarily evaluated during translation and its complete state is carried
 * by a type, irrespective of whether the user spelled the CPO `consteval` or a
 * constant-evaluable `constexpr`.
 */
template <auto format_literal, auto source, typename value_type>
concept structurally_compiled_custom_format =
	custom_format_adl::structural_parse_expression<
		format_literal, source, value_type>;

template <auto format_literal, auto source, typename value_type>
	requires structurally_compiled_custom_format<format_literal, source, value_type>
inline constexpr auto custom_format_state{
	custom_format_adl::parse_state<format_literal, source, value_type>()};

template <auto format_literal, auto source, typename value_type>
	requires structurally_compiled_custom_format<format_literal, source, value_type>
using custom_format_state_tag = ::fast_io::fmt::basic_custom_format_state_t<
	typename decltype(format_literal)::value_type,
	custom_format_state<format_literal, source, value_type>>;

template <auto format_literal, auto source, typename value_type>
concept custom_format_alias_expression =
	structurally_compiled_custom_format<format_literal, source, value_type> &&
	custom_format_adl::alias_expression<
		custom_format_state_tag<format_literal, source, value_type>, value_type>;

/**
 * Lazily classifies the run-time edge after the structural parser has won.
 *
 * The parser itself is a type-only constant-evaluation policy. Only the
 * selected `format_alias_define` invocation contributes a run-time effect and
 * therefore an ABI choice at this boundary.
 */
template <auto format_literal, auto source, typename value_type>
inline constexpr bool custom_format_value_herbceptions_throws = []() constexpr {
#if defined(__HERBCEPTIONS__)
	if constexpr (structurally_compiled_custom_format<
					  format_literal, source, value_type>)
	{
		using state_tag =
			custom_format_state_tag<format_literal, source, value_type>;
		if constexpr (custom_format_adl::alias_expression<
						  state_tag, value_type>)
		{
			return custom_format_adl::alias_herbceptions_throws<
				state_tag, value_type>;
		}
	}
#endif
	return false;
}();

/**
 * Proves that the semantic result remains consumable by the selected
 * character-domain backend after normal fast_io aliasing and ABI transport.
 * This is an unevaluated proof only; `make_custom_format_value` intentionally
 * returns the raw semantic node so decay still occurs exactly once at the
 * print/concat entry point.
 */
template <auto format_literal, auto source, typename value_type>
concept custom_format_printable =
	custom_format_alias_expression<format_literal, source, value_type> &&
	requires(value_type &&value) {
		::fast_io::io_print_forward<typename decltype(format_literal)::value_type>(
			::fast_io::io_print_alias(custom_format_adl::alias<
									  custom_format_state_tag<format_literal, source, value_type>>(
				::std::forward<value_type>(value))));
	};

/**
 * Classifies the legacy exception effect of the selected run-time alias.
 *
 * The compile-time parser contributes no run-time edge. Once its structural
 * proof and the ADL alias are available, the expression below is exactly the
 * one executed by `make_custom_format_value`. An invalid customization returns
 * true because the function body diagnoses it with a static assertion and no
 * ABI may be inferred from an unavailable CPO.
 */
template <auto format_literal, auto source, typename value_type>
inline constexpr bool custom_format_value_nothrow = []() constexpr {
	if constexpr (custom_format_alias_expression<
				  format_literal, source, value_type>)
	{
		using state_tag =
			custom_format_state_tag<format_literal, source, value_type>;
		return noexcept(custom_format_adl::alias<state_tag>(
			::std::declval<value_type &&>()));
	}
	else
	{
		return false;
	}
}();

/**
 * Parses and lowers one user-defined field without preserving frontend state.
 *
 * A parser which exists but returns non-structural/non-constant state is a
 * contract error, not permission to fall back to runtime formatting.  Likewise
 * a missing semantic CPO is diagnosed here rather than erased behind a generic
 * visitor.
 */
template <auto format_literal, auto source, typename value_type>
[[nodiscard]] inline constexpr decltype(auto) make_custom_format_value(
	value_type &&value) FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
	(custom_format_value_herbceptions_throws<
		format_literal, source, value_type>),
	custom_format_value_nothrow<format_literal, source, value_type>)
{
	static_assert(custom_format_parse_expression<format_literal, source, value_type>,
				  "fast_io format: custom type has no ADL format_parse_define(io_type_t<T>, parse_context) CPO");
	static_assert(structurally_compiled_custom_format<format_literal, source, value_type>,
				  "fast_io format: format_parse_define must produce constant structural state (prefer a consteval CPO)");

	if constexpr (structurally_compiled_custom_format<format_literal, source, value_type>)
	{
		using state_tag = custom_format_state_tag<format_literal, source, value_type>;
		static_assert(custom_format_adl::alias_expression<state_tag, value_type>,
					  "fast_io format: custom type has no ADL format_alias_define(parsed_state_tag, value) CPO");
		static_assert(custom_format_printable<format_literal, source, value_type>,
					  "fast_io format: format_alias_define result is not printable in the format character domain");
		if constexpr (custom_format_adl::alias_expression<state_tag, value_type>)
		{
			return custom_format_adl::alias<state_tag>(
				::std::forward<value_type>(value));
		}
		else
		{
			return ::fast_io::io_null;
		}
	}
	else
	{
		return ::fast_io::io_null;
	}
}

/**
 * The exact empty brace specification for which ADL `format_as(value)` is a
 * valid shortcut.  Applying it after any width, precision, alignment,
 * presentation, locale, or custom-tail token would silently discard syntax;
 * keeping this predicate complete prevents that class of fallback bug.
 */
template <auto specification>
inline constexpr bool zero_brace_format_specification_v =
	!specification.has_fill && specification.fill_size == 0u &&
	specification.alignment == decltype(specification.alignment){} &&
	specification.sign == decltype(specification.sign){} &&
	!specification.alternate_form && !specification.zero_padding &&
	!specification.locale_specific &&
	specification.width.kind == decltype(specification.width.kind){} &&
	specification.precision.kind == decltype(specification.precision.kind){} &&
	specification.presentation == decltype(specification.presentation){} &&
	specification.raw_format_specification.size == 0u &&
	specification.format_tail.size == 0u;

template <typename value_type>
concept adl_format_as_expression =
	custom_format_adl::format_as_expression<value_type>;

template <typename value_type>
concept nonrecursive_adl_format_as =
	custom_format_adl::format_as_probe<
		adl_format_as_expression<value_type>, value_type>::nonrecursive;

template <typename char_type, typename value_type>
concept printable_adl_format_as =
	::fast_io::fmt::format_character<char_type> &&
	nonrecursive_adl_format_as<value_type> && requires(value_type &&value) {
		::fast_io::io_print_forward<char_type>(::fast_io::io_print_alias(
			custom_format_adl::as(::std::forward<value_type>(value))));
	};

template <typename value_type>
inline constexpr bool format_as_value_herbceptions_throws = []() constexpr {
#if defined(__HERBCEPTIONS__)
	if constexpr (adl_format_as_expression<value_type>)
	{
		return custom_format_adl::format_as_herbceptions_throws<value_type>;
	}
#endif
	return false;
}();

/** Classifies the traditional exception edge of the exact nonrecursive `format_as` call. */
template <typename value_type>
inline constexpr bool format_as_value_nothrow = []() constexpr {
	if constexpr (nonrecursive_adl_format_as<value_type>)
	{
		return noexcept(custom_format_adl::as(
			::std::declval<value_type &&>()));
	}
	else
	{
		// The body returns `io_null` after issuing its compile-time contract diagnostic; it executes no open CPO.
		return true;
	}
}();

/**
 * Invokes the zero-specification `format_as` shortcut without pre-decaying its
 * result.  The caller must gate this function with
 * `zero_brace_format_specification_v`; keeping the specification as a separate
 * compile-time decision avoids adding it to the runtime proxy's representation.
 */
template <::fast_io::fmt::format_character char_type, typename value_type>
[[nodiscard]] inline constexpr decltype(auto) make_format_as_value(
	value_type &&value) FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
	format_as_value_herbceptions_throws<value_type>,
	format_as_value_nothrow<value_type>)
{
	static_assert(adl_format_as_expression<value_type>,
				  "fast_io format: no ADL format_as(value) customization was found");
	static_assert(nonrecursive_adl_format_as<value_type>,
				  "fast_io format: format_as(value) must not return the same semantic type");
	static_assert(printable_adl_format_as<char_type, value_type>,
				  "fast_io format: format_as(value) result is not printable in the format character domain");
	if constexpr (nonrecursive_adl_format_as<value_type>)
	{
		return custom_format_adl::as(::std::forward<value_type>(value));
	}
	else
	{
		return ::fast_io::io_null;
	}
}

} // namespace fast_io::fmt::details

#include "../../fast_io_dsal/impl/misc/pop_macros.h"

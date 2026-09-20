#pragma once

/*
 * Public type vocabulary for the format frontend (FMT level).
 *
 * Grammar identities, fixed format strings, and open grammar constraints live
 * here so syntax can be selected and checked without depending on a concrete
 * output device. These types describe source-language structure only. They
 * neither format values themselves nor participate in downstream stream
 * reference, locking, buffering, or primitive-transfer protocols.
 */

#include <concepts>
#include <type_traits>
#include <utility>

#include "../fast_io_core.h"
#include "details/fixed_string.h"

namespace fast_io::fmt
{

/// Internal grammar identity used by the compile-time parser and lowering layer.
///
/// This is a type, rather than an enum value shared with the printf grammar, so overload
/// resolution and ADL can select a parser without instantiating the other grammar.  Keeping
/// the grammars in separate overload sets is also important for compile-time diagnostics:
/// a percent sign is ordinary text in this grammar and must never be tentatively parsed by
/// the printf implementation.
struct brace_fmt_t
{
	using format_grammar_tag = void;
	explicit constexpr brace_fmt_t() noexcept = default;
};

/// Internal identity for the type-safe percent grammar.
struct printf_fmt_t
{
	using format_grammar_tag = void;
	explicit constexpr printf_fmt_t() noexcept = default;
};

/// Recognizes a stateless grammar rule without naming any concrete syntax.
///
/// This marker is only the inexpensive overload-selection gate.  The stronger
/// `compilable_format_grammar<literal, T>` concept (declared by compile.h) also
/// requires an ADL `compile_format_program<literal>(T)` CPO.  Separating those
/// checks keeps arbitrary values out of rule kernels while making the grammar
/// set open: `print_with_rule` and `concat_with_rule` do not contain a closed
/// brace-versus-percent type test.
///
/// The object is a type token, not runtime policy storage.  Parsing and lowering
/// intentionally reconstruct `T{}` inside immediate/dependent calls, so accepting
/// a stateful instance would silently discard observable state.  The empty,
/// nothrow, and trivial requirements make that ABI fact part of the concept
/// instead of relying on a convention that generic code cannot verify.
template <typename T>
concept format_grammar =
	requires { typename ::std::remove_cvref_t<T>::format_grammar_tag; } &&
	::std::default_initializable<::std::remove_cvref_t<T>> &&
	::std::is_empty_v<::std::remove_cvref_t<T>> &&
	::std::is_nothrow_default_constructible_v<::std::remove_cvref_t<T>> &&
	::std::is_trivially_copyable_v<::std::remove_cvref_t<T>> &&
	::std::is_trivially_destructible_v<::std::remove_cvref_t<T>>;

/// Owns an rvalue named argument and borrows an lvalue named argument.
///
/// The name is structural compile-time data, so name lookup cannot become the linear runtime
/// string search used by fmt's runtime named arguments. The value storage follows the usual
/// forwarding lifetime rule: an rvalue is owned and an lvalue remains borrowed. Lowering then
/// exposes the stable named member as an lvalue, exactly as an ordinary named function parameter
/// reaches fast_io::io::print; final alias and ABI transport remain backend decisions.
template <basic_fixed_string name_literal, typename storage_type>
struct static_named_arg
{
	using value_type = storage_type;
	static inline constexpr auto name{name_literal};

	storage_type value;
};

/** Recognizes the format grammar's structural fixed-string value type. */
template <typename T>
struct is_basic_fixed_string : ::std::false_type
{};

template <format_character char_type, ::std::size_t extent>
struct is_basic_fixed_string<basic_fixed_string<char_type, extent>>
	: ::std::true_type
{};

template <typename T>
inline constexpr bool is_format_character_pointer_v{
	::std::is_pointer_v<T> &&
	format_character<::std::remove_cv_t<::std::remove_pointer_t<T>>>};

template <::std::size_t index, auto value_literal>
struct static_tuple_value_slot
{
	static inline constexpr auto stored_value{value_literal};
};

template <typename index_sequence, auto... value_literals>
struct static_tuple_value_impl;

template <::std::size_t... index, auto... value_literals>
struct static_tuple_value_impl<::std::index_sequence<index...>,
							   value_literals...> : static_tuple_value_slot<index, value_literals>...
{};

template <::std::size_t index, auto value_literal>
[[nodiscard]] inline constexpr decltype(auto) static_tuple_value_slot_get(
	static_tuple_value_slot<index, value_literal> const &) noexcept
{
	if constexpr (is_basic_fixed_string<
					  ::std::remove_cv_t<decltype(static_tuple_value_slot<index,
																		  value_literal>::stored_value)>>::value ||
				  ::fast_io::manipulators::is_basic_static_string_v<
					  decltype(static_tuple_value_slot<index,
													   value_literal>::stored_value)>)
	{
		return (static_tuple_value_slot<index,
										value_literal>::stored_value.elements);
	}
	else
	{
		return (static_tuple_value_slot<index,
										value_literal>::stored_value);
	}
}

/** A tuple-like view whose heterogeneous elements are individual NTTPs. */
template <auto... value_literals>
struct static_tuple_value
	: static_tuple_value_impl<
		  ::std::index_sequence_for<decltype(value_literals)...>,
		  value_literals...>
{};

template <::std::size_t index, auto... value_literals>
	requires(index < sizeof...(value_literals))
[[nodiscard]] inline constexpr decltype(auto) get(
	static_tuple_value<value_literals...> const &value) noexcept
{
	return ::fast_io::fmt::static_tuple_value_slot_get<index>(value);
}

template <::std::size_t index, auto... value_literals>
	requires(index < sizeof...(value_literals))
[[nodiscard]] inline constexpr decltype(auto) get(
	static_tuple_value<value_literals...> &value) noexcept
{
	return ::fast_io::fmt::get<index>(
		static_cast<static_tuple_value<value_literals...> const &>(value));
}

/** Carries a heterogeneous tuple as an NTTP pack without run-time state. */
template <auto... value_literals>
struct static_tuple_format_arg
{
	static inline constexpr static_tuple_value<value_literals...> stored_value{};

	[[nodiscard]] inline static constexpr auto const &get() noexcept
	{
		return stored_value;
	}
};

template <typename T>
struct is_static_format_arg : ::std::false_type
{};

template <::fast_io::manipulators::static_argument_constant value_literal>
struct is_static_format_arg<
	::fast_io::manipulators::static_arg_t<value_literal>> : ::std::true_type
{};

template <auto... value_literals>
struct is_static_format_arg<static_tuple_format_arg<value_literals...>>
	: ::std::true_type
{};

template <typename T>
inline constexpr bool is_static_format_arg_v{
	is_static_format_arg<::std::remove_cvref_t<T>>::value};

template <typename T>
struct is_static_format_argument_holder : is_static_format_arg<::std::remove_cvref_t<T>>
{};

template <basic_fixed_string name_literal, typename storage_type>
struct is_static_format_argument_holder<static_named_arg<name_literal, storage_type>>
	: is_static_format_arg<::std::remove_cvref_t<storage_type>>
{};

template <::fast_io::manipulators::static_argument_constant name_literal,
		  ::fast_io::manipulators::static_argument_constant value_literal>
struct is_static_format_argument_holder<
	::fast_io::manipulators::static_named_arg_t<
		name_literal, value_literal>> : ::std::true_type
{};

template <typename T>
inline constexpr bool is_static_format_argument_holder_v{
	is_static_format_argument_holder<::std::remove_cvref_t<T>>::value};

template <typename T>
struct is_static_named_arg : ::std::false_type
{};

template <basic_fixed_string name_literal, typename storage_type>
struct is_static_named_arg<static_named_arg<name_literal, storage_type>> : ::std::true_type
{};

template <::fast_io::manipulators::static_argument_constant name_literal,
		  ::fast_io::manipulators::static_argument_constant value_literal>
struct is_static_named_arg<
	::fast_io::manipulators::static_named_arg_t<
		name_literal, value_literal>> : ::std::true_type
{};

template <typename T>
inline constexpr bool is_static_named_arg_v{
	is_static_named_arg<::std::remove_cvref_t<T>>::value};

/// Creates a named argument whose name participates in constant evaluation.
template <basic_fixed_string name_literal, typename T>
[[nodiscard]] inline constexpr auto arg(T &&value)
{
	using storage_type = ::std::conditional_t<
		::std::is_lvalue_reference_v<T &&>, T &&, ::std::remove_cvref_t<T>>;
	return static_named_arg<name_literal, storage_type>{::std::forward<T>(value)};
}

/** Creates an NTTP-reference carrier for a fixed non-character C array. */
template <auto &array_literal,
		  auto copied_literal =
			  ::fast_io::manipulators::basic_static_c_array_value{array_literal}>
	requires(::std::is_array_v<
				 ::std::remove_reference_t<decltype(array_literal)>> &&
			 ::std::is_const_v<::std::remove_extent_t<
				 ::std::remove_reference_t<decltype(array_literal)>>> &&
			 !format_character<::std::remove_cv_t<::std::remove_extent_t<
				 ::std::remove_reference_t<decltype(array_literal)>>>>)
[[nodiscard]] inline consteval auto static_array_arg() noexcept
{
	return ::fast_io::manipulators::static_arg<copied_literal>;
}

/** Creates a named NTTP-reference carrier for a fixed non-character C array. */
template <basic_fixed_string name_literal, auto &array_literal,
		  auto copied_literal =
			  ::fast_io::manipulators::basic_static_c_array_value{array_literal}>
	requires(::std::is_array_v<
				 ::std::remove_reference_t<decltype(array_literal)>> &&
			 ::std::is_const_v<::std::remove_extent_t<
				 ::std::remove_reference_t<decltype(array_literal)>>> &&
			 !format_character<::std::remove_cv_t<::std::remove_extent_t<
				 ::std::remove_reference_t<decltype(array_literal)>>>>)
[[nodiscard]] inline consteval auto static_named_array_arg() noexcept
{
	return static_named_arg<
		name_literal,
		decltype(::fast_io::manipulators::static_arg<copied_literal>)>{
		::fast_io::manipulators::static_arg<copied_literal>};
}

/** Creates a tuple-like argument whose elements are heterogeneous NTTPs. */
template <auto... value_literals>
[[nodiscard]] inline consteval auto static_tuple_arg() noexcept
{
	return static_tuple_format_arg<value_literals...>{};
}

/** Creates a named tuple-like argument from a heterogeneous NTTP pack. */
template <basic_fixed_string name_literal, auto... value_literals>
[[nodiscard]] inline consteval auto static_named_tuple_arg() noexcept
{
	return static_named_arg<
		name_literal, static_tuple_format_arg<value_literals...>>{};
}

} // namespace fast_io::fmt

namespace std
{

template <auto... value_literals>
struct tuple_size<::fast_io::fmt::static_tuple_value<value_literals...>>
	: ::std::integral_constant<::std::size_t, sizeof...(value_literals)>
{};

template <::std::size_t index, auto... value_literals>
struct tuple_element<
	index, ::fast_io::fmt::static_tuple_value<value_literals...>>
{
	using type = ::std::remove_reference_t<decltype(::fast_io::fmt::get<index>(::std::declval<
																			   ::fast_io::fmt::static_tuple_value<value_literals...> const &>()))>;
};

} // namespace std

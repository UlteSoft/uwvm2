#pragma once

/*
 * Width/padding semantic node for the CPO/semantic level.
 *
 * These wrappers attach minimum width, placement, and optional fill character
 * to one normalized child object. They preserve child lifetime and defer
 * measurement, internal-shift discovery, padding, and emission to the IO
 * planner and printable protocols. Width is therefore semantic structure, not
 * a device write and not a format-language parser.
 */

#include "forward.h"

namespace fast_io
{

namespace details
{

template <typename T>
using width_alias_result = decltype(::fast_io::io_print_alias(::std::declval<T>()));

/// @brief Selects the representation retained by a width semantic node.
/// @details The original source category is deliberately kept as part of this decision. Applying
///          `io_print_forward_transport` only to the alias result would lose that evidence: an alias invoked on an
///          rvalue is permitted to return an lvalue reference to one of the source's subobjects, and wrapping that
///          reference would let it escape the source temporary. Therefore every rvalue-derived result is materialized.
///          An alias reference obtained from an lvalue retains its exact cv/ref identity so noncopyable mutable proxies
///          remain usable. An ordinary ABI-small trivial lvalue is copied as the established calling-convention
///          optimization; every other ordinary lvalue is a borrowed exact reference. Consequently a width node that
///          stores an lvalue reference is a view and must not outlive that source object.
template <typename T>
using width_storage_type = ::std::conditional_t<
	::std::is_function_v<::std::remove_cvref_t<T>>,
	::std::remove_cvref_t<::fast_io::details::width_alias_result<T>>,
	::std::conditional_t<
		::std::is_lvalue_reference_v<T &&> && ::fast_io::alias_printable<T> &&
			::std::is_lvalue_reference_v<::fast_io::details::width_alias_result<T>>,
		::fast_io::details::width_alias_result<T>,
		::std::conditional_t<
			::std::is_lvalue_reference_v<T &&> && !::fast_io::alias_printable<T>,
			::std::conditional_t<::fast_io::details::io_print_forward_transport_by_value<T>,
								 ::std::remove_cvref_t<T>, T>,
			::std::remove_cvref_t<::fast_io::details::width_alias_result<T>>>>>;

/// @brief Tests the exact conversion used to initialize a width node's stored child.
/// @details Expressing the conversion as a requirement, rather than only using `is_constructible`, models the actual
///          explicit alias-to-storage conversion and cleanly rejects an rvalue alias that exposes a noncopyable borrowed
///          subobject. Guaranteed copy elision can nevertheless make an immovable owned prvalue pass that first test.
///          Such a child cannot cross the normalized semantic node's later by-value boundary, so owned storage also has
///          to be constructible from its rvalue category. Reference storage already preserves an existing identity and
///          intentionally needs no move proof. This is the same compositional contract used by condition and pack nodes.
template <typename T>
concept width_storable =
	requires {
		static_cast<::fast_io::details::width_storage_type<T>>(
			::fast_io::io_print_alias(::std::declval<T>()));
	} &&
	(::std::is_lvalue_reference_v<::fast_io::details::width_storage_type<T>> ||
	 ::std::constructible_from<
		 ::std::remove_cvref_t<::fast_io::details::width_storage_type<T>>,
		 ::std::remove_cvref_t<::fast_io::details::width_storage_type<T>> &&>);

/// @brief Computes whether width child normalization can propagate an exception.
/// @details The expression includes both the selected alias CPO and the materialization/copy required by the storage
///          policy. It is kept conditional so an unstorable adversarial alias remains a clean constraint failure rather
///          than producing a second diagnostic from a factory's exception specification.
template <typename T>
inline constexpr bool width_storage_nothrow_constructible = []() constexpr {
	if constexpr (::fast_io::details::width_storable<T>)
	{
		return noexcept(static_cast<::fast_io::details::width_storage_type<T>>(
			::fast_io::io_print_alias(::std::declval<T>())));
	}
	else
	{
		return false;
	}
}();

#if defined(__HERBCEPTIONS__)
/// @brief Classifies the deterministic-error effect of the exact width-storage expression.
/// @details The query composes alias dispatch with the selected value/reference materialization without changing the
///          existing storage type. In particular, the ABI-small decay policy remains by value and borrowed lvalues
///          retain their exact reference identity; effect support is not permission to introduce another reference.
template <typename T>
inline constexpr bool width_storage_herbceptions_throws = []() constexpr {
	if constexpr (::fast_io::details::width_storable<T>)
	{
		return throws((static_cast<::fast_io::details::width_storage_type<T>>(
			::fast_io::io_print_alias(::std::declval<T>()))));
	}
	else
	{
		return false;
	}
}();
#endif

/// @brief Normalizes one width child according to `width_storage_type`.
/// @details The explicit cast is the single construction point shared by all placement and fill-character factories;
///          this prevents those overloads from drifting into different alias, lifetime, or exception rules. Let E be
///          that exact alias-and-materialization expression, B = `throws(E)`, and N = `noexcept(E)`. The one canonical
///          signature is `throws(B)` in Herbception mode and retains the historical `noexcept(N)` contract otherwise.
template <typename T>
	requires ::fast_io::details::width_storable<T>
inline constexpr ::fast_io::details::width_storage_type<T> width_store(T &&t)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::width_storage_herbceptions_throws<T>),
		(::fast_io::details::width_storage_nothrow_constructible<T>))
{
	return static_cast<::fast_io::details::width_storage_type<T>>(
		::fast_io::io_print_alias(::std::forward<T>(t)));
}

} // namespace details

namespace manipulators
{

/// @brief Bounded width replacement owned by print/concat's compiler-constant strategy.
/// @details `placement == scalar_placement::none` denotes the run-time-placement source family; every other
///          instantiation fixes placement in the type. The child has already been replaced by its independent
///          compiler-constant proxy and `child_size` is that exact proxy spelling. Width is admitted only within the
///          common compiler-constant byte budget, so the replacement exposes one honest type-level reserve extent
///          without changing the ordinary width node or its run-time formatting algorithm.
template <scalar_placement placement, typename T, ::std::integral char_type>
struct compiler_constant_width_t
{
	using manip_tag = manip_tag_t;
	T reference;
	::std::size_t child_size{};
	::std::uint_least16_t width{};
	char_type fill{};
	scalar_placement runtime_placement{scalar_placement::right};
};

/// @brief Pads a value to a minimum width using a run-time placement and the default fill character.
/// @details `placement` selects the side/internal policy, `n` is a minimum code-unit count, and values wider than `n`
///          are emitted in full. `none` or an unrecognized enum value performs no ordinary run-time padding. The wrapped
///          value is normalized and stored immediately. Its effect is exactly the storage transaction's conditional
///          effect; safe and fallible children therefore share one canonical function type.
template <typename T>
requires ::fast_io::details::width_storable<T>
inline constexpr auto width(scalar_placement placement, T &&t, ::std::size_t n)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::width_storage_herbceptions_throws<T>),
		(::fast_io::details::width_storage_nothrow_constructible<T>))
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_runtime_t<storage_type>{
		placement, ::fast_io::details::width_store(::std::forward<T>(t)), n};
}

/// @brief Pads a value to a minimum width using run-time placement and an explicit fill code unit.
/// @details Padding uses `ch`; the value is never truncated when its formatted length exceeds `n`. `none` or an
///          unrecognized placement leaves the child unpadded in the ordinary run-time path.
template <typename T, ::std::integral char_type>
requires ::fast_io::details::width_storable<T>
inline constexpr auto width(scalar_placement placement, T &&t, ::std::size_t n, char_type ch)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::width_storage_herbceptions_throws<T>),
		(::fast_io::details::width_storage_nothrow_constructible<T>))
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_runtime_ch_t<storage_type, char_type>{
		placement, ::fast_io::details::width_store(::std::forward<T>(t)), n, ch};
}

/// @brief Left-aligns a value in a minimum-width field using the default fill character.
/// @details Padding is appended after the value; values wider than `n` are emitted unchanged.
template <typename T>
requires ::fast_io::details::width_storable<T>
inline constexpr auto left(T &&t, ::std::size_t n)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::width_storage_herbceptions_throws<T>),
		(::fast_io::details::width_storage_nothrow_constructible<T>))
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_t<scalar_placement::left, storage_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n};
}

/// @brief Applies the library's middle-placement padding to a minimum-width field with default fill.
/// @details Padding is split around the child: `floor(padding / 2)` code units precede it and the remainder follows it,
///          so an odd extra fill code unit is placed on the right. The child is never truncated.
template <typename T>
requires ::fast_io::details::width_storable<T>
inline constexpr auto middle(T &&t, ::std::size_t n)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::width_storage_herbceptions_throws<T>),
		(::fast_io::details::width_storage_nothrow_constructible<T>))
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_t<scalar_placement::middle, storage_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n};
}

/// @brief Right-aligns a value in a minimum-width field using the default fill character.
/// @details Padding precedes the value; values wider than `n` are emitted unchanged.
template <typename T>
requires ::fast_io::details::width_storable<T>
inline constexpr auto right(T &&t, ::std::size_t n)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::width_storage_herbceptions_throws<T>),
		(::fast_io::details::width_storage_nothrow_constructible<T>))
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_t<scalar_placement::right, storage_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n};
}

/// @brief Applies internal padding to a minimum-width field using the default fill character.
/// @details Padding begins after the shift reported by the child. Ordinary scalar integers report only a leading sign,
///          not a radix prefix, so direct `internal(hex0x(...))` padding can precede `0x`; the format frontend's scalar
///          wrapper reports both sign and prefix. A child without an internal-shift CPO falls back to right alignment.
template <typename T>
requires ::fast_io::details::width_storable<T>
inline constexpr auto internal(T &&t, ::std::size_t n)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::width_storage_herbceptions_throws<T>),
		(::fast_io::details::width_storage_nothrow_constructible<T>))
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_t<scalar_placement::internal, storage_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n};
}

/// @brief Left-aligns a value in a minimum-width field using explicit fill `ch`.
/// @details Padding follows the value and the child is never truncated.
template <typename T, ::std::integral char_type>
requires ::fast_io::details::width_storable<T>
inline constexpr auto left(T &&t, ::std::size_t n, char_type ch)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::width_storage_herbceptions_throws<T>),
		(::fast_io::details::width_storage_nothrow_constructible<T>))
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_ch_t<scalar_placement::left, storage_type, char_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n, ch};
}

/// @brief Applies middle-placement padding with explicit fill `ch`.
/// @details The left side receives `floor(padding / 2)` copies and the right side receives the remainder; therefore an
///          odd extra fill is on the right. `n` is a minimum field width and never a clipping limit.
template <typename T, ::std::integral char_type>
requires ::fast_io::details::width_storable<T>
inline constexpr auto middle(T &&t, ::std::size_t n, char_type ch)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::width_storage_herbceptions_throws<T>),
		(::fast_io::details::width_storage_nothrow_constructible<T>))
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_ch_t<scalar_placement::middle, storage_type, char_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n, ch};
}

/// @brief Right-aligns a value in a minimum-width field using explicit fill `ch`.
/// @details Padding precedes the value; representations wider than `n` pass through unchanged.
template <typename T, ::std::integral char_type>
requires ::fast_io::details::width_storable<T>
inline constexpr auto right(T &&t, ::std::size_t n, char_type ch)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::width_storage_herbceptions_throws<T>),
		(::fast_io::details::width_storage_nothrow_constructible<T>))
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_ch_t<scalar_placement::right, storage_type, char_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n, ch};
}

/// @brief Applies internal padding with explicit fill `ch`.
/// @details Fill begins after the child-reported internal shift; ordinary scalar integers report a sign but not their
///          radix prefix, while the format frontend wrapper reports both. Missing shift support falls back to right
///          alignment, and no truncation occurs.
template <typename T, ::std::integral char_type>
requires ::fast_io::details::width_storable<T>
inline constexpr auto internal(T &&t, ::std::size_t n, char_type ch)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::width_storage_herbceptions_throws<T>),
		(::fast_io::details::width_storage_nothrow_constructible<T>))
{
	using storage_type = ::fast_io::details::width_storage_type<T>;
	return width_ch_t<scalar_placement::internal, storage_type, char_type>{
		::fast_io::details::width_store(::std::forward<T>(t)), n, ch};
}

} // namespace manipulators

/// @brief Propagates an established read proof through each width semantic representation.
/// @details Width contributes padding but obtains its external source range exclusively from the stored child. Fixed
///          versus run-time placement and default versus explicit fill characters do not change that provenance. These
///          four overloads are kept distinct because the representation types are distinct protocol nodes; a generic
///          structural rule would accidentally certify unrelated user types with similarly named members.
template <manipulators::scalar_placement placement, typename T>
	requires prfch_cacheable_read_provenance<T>
inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	io_type_t<manipulators::width_t<placement, T>>) noexcept
{
	return {};
}

template <manipulators::scalar_placement placement, typename T, ::std::integral char_type>
	requires prfch_cacheable_read_provenance<T>
inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	io_type_t<manipulators::width_ch_t<placement, T, char_type>>) noexcept
{
	return {};
}

template <typename T>
	requires prfch_cacheable_read_provenance<T>
inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	io_type_t<manipulators::width_runtime_t<T>>) noexcept
{
	return {};
}

template <typename T, ::std::integral char_type>
	requires prfch_cacheable_read_provenance<T>
inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	io_type_t<manipulators::width_runtime_ch_t<T, char_type>>) noexcept
{
	return {};
}

namespace details
{

/// @brief Recognizes a child whose cheap non-fatal materialization bound can pass through width layout.
template <typename char_type, typename T>
concept single_pass_bounded_width_child =
	::std::integral<char_type> &&
	::fast_io::single_pass_bounded_materialization_source<char_type, T>;

/// @brief Recognizes a child explicitly authorized for print's direct bounded put-area strategy.
template <typename char_type, typename T>
concept print_single_pass_bounded_direct_put_area_width_child =
	::std::integral<char_type> && requires {
		{
			print_single_pass_bounded_direct_put_area_safe(
				::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

/// @brief Computes `max(child bound, width)` without observing the child after width already rejects the frame.
template <::std::integral char_type, typename T>
	requires single_pass_bounded_width_child<char_type, T>
[[nodiscard]] inline constexpr ::std::size_t
single_pass_bounded_width_size(
	T &child, ::std::size_t width, ::std::size_t maximum_size) noexcept
{
	if (maximum_size < width)
	{
		return SIZE_MAX;
	}
	auto const child_size{
		::fast_io::single_pass_bounded_materialization_size_invoke<char_type>(
			child, maximum_size)};
	if (child_size == SIZE_MAX || maximum_size < child_size)
	{
		return SIZE_MAX;
	}
	return child_size < width ? width : child_size;
}

/// @brief Recognizes a width child whose compiler-constant replacement has a non-throwing exact protocol.
/// @details Width must know the selected child's exact length before applying placement. Requiring the existing
///          precise-compact contract makes that length query stable and lets the width replacement retain the child's
///          mature constant materializer without allocating its conservative reserve maximum.
template <typename char_type, typename T>
concept compiler_constant_width_child = ::std::integral<char_type> &&
	::fast_io::compiler_constant_pre_normalization_safe<char_type, T> &&
	::fast_io::compiler_constant_precise_compact_preferred<
		char_type,
		::fast_io::details::compiler_constant_materialized_t<char_type, T>> &&
	requires {
		{
			print_compiler_constant_eligible_implies_compact_size(
				::fast_io::io_reserve_type<char_type,
					::std::remove_cvref_t<T>>)
		} -> ::std::same_as<::std::true_type>;
	};

template <::std::integral char_type>
inline constexpr ::std::size_t compiler_constant_width_capacity{
	::fast_io::details::compiler_constant_materialization_max_bytes /
	sizeof(char_type)};

template <::std::integral char_type, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool
compiler_constant_width_child_eligible(T const &value) noexcept
{
	// The exact child protocol owns the size computation. Its explicit marker states that a true eligibility result has
	// already compared that exact spelling with this same character-domain byte budget, so repeating materialization
	// here would only duplicate expensive floating precision work.
	return print_compiler_constant_materialization_eligible(
		::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>,
		value);
}

template <::fast_io::manipulators::scalar_placement placement,
	::std::integral char_type, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T>
// This leaf belongs only to the compiler-constant replacement protocol. A per-function -O3 audit on GCC 13/15 and
// Clang 23 showed that losing this boundary prevents all four width families from completing constant materialization
// and reconnects their constant callers to the generic formatter; every unknown-double caller stayed instruction-identical.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr auto
compiler_constant_width_materialize(T const &child, ::std::size_t width,
	char_type fill,
	::fast_io::manipulators::scalar_placement runtime_placement = placement) noexcept
{
	// Core calls this unchecked leaf only after this width node's eligibility returned true. That result proves the
	// child's eligibility and the child's compact-size marker proves its exact materialized spelling is at most the
	// common capacity. The internal gate CPO propagates that proof recursively; its default forwarding overload adds no
	// placement requirement to a floating size or formatting implementation.
	auto materialized{print_compiler_constant_materialize_gate_proven(
		::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>,
		child)};
	auto const child_size{print_reserve_precise_size(
		::fast_io::io_reserve_type<char_type,
			::std::remove_cvref_t<decltype(materialized)>>,
		materialized)};
	constexpr auto capacity{
		::fast_io::details::compiler_constant_width_capacity<char_type>};
	auto const bounded_width{width < capacity ? width : capacity};
	auto const normalized_placement{
		static_cast<::std::size_t>(runtime_placement) - 1u < 4u
			? runtime_placement
			: ::fast_io::manipulators::scalar_placement::right};
	using materialized_type = ::std::remove_cvref_t<decltype(materialized)>;
	return ::fast_io::manipulators::compiler_constant_width_t<
		placement, materialized_type, char_type>{
		::std::move(materialized), child_size,
		static_cast<::std::uint_least16_t>(bounded_width), fill,
		normalized_placement};
}

template <::fast_io::manipulators::scalar_placement placement,
	::std::integral char_type, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T>
[[nodiscard]] inline constexpr auto
compiler_constant_width_materialize_checked(T const &child,
	::std::size_t width, char_type fill,
	::fast_io::manipulators::scalar_placement runtime_placement = placement) noexcept
{
	auto materialized{print_compiler_constant_materialize(
		::fast_io::io_reserve_type<char_type, ::std::remove_cvref_t<T>>,
		child)};
	auto const child_size{print_reserve_precise_size(
		::fast_io::io_reserve_type<char_type,
			::std::remove_cvref_t<decltype(materialized)>>,
		materialized)};
	constexpr auto capacity{
		::fast_io::details::compiler_constant_width_capacity<char_type>};
	// The ordinary ADL materializer is intentionally callable without first asking eligibility. An oversized child
	// cannot fit this proxy's type-level reserve extent, so direct misuse terminates before inconsistent metadata escapes.
	if (capacity < child_size) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	auto const bounded_width{width < capacity ? width : capacity};
	auto const normalized_placement{
		static_cast<::std::size_t>(runtime_placement) - 1u < 4u
			? runtime_placement
			: ::fast_io::manipulators::scalar_placement::right};
	using materialized_type = ::std::remove_cvref_t<decltype(materialized)>;
	return ::fast_io::manipulators::compiler_constant_width_t<
		placement, materialized_type, char_type>{
		::std::move(materialized), child_size,
		static_cast<::std::uint_least16_t>(bounded_width), fill,
		normalized_placement};
}

} // namespace details

/// @brief Propagates an audited one-pass materialization bound through fixed-placement width layout.
/// @details Sizing observes the wrapper through const access so its child remains intact for emission.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
	requires ::fast_io::details::single_pass_bounded_width_child<
		char_type, T>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_t<placement, T>>) noexcept
{
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
	requires ::fast_io::details::print_single_pass_bounded_direct_put_area_width_child<
		char_type, T>
inline constexpr ::std::true_type print_single_pass_bounded_direct_put_area_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_t<placement, T>>) noexcept
{
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
	requires ::fast_io::details::single_pass_bounded_width_child<
		char_type, T>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_t<placement, T>>,
	::fast_io::manipulators::width_t<placement, T> const &value,
	::std::size_t maximum_size) noexcept
{
	return ::fast_io::details::single_pass_bounded_width_size<char_type>(
		value.reference, value.width, maximum_size);
}

/// @brief Propagates the same bound when width uses one explicit output code unit as fill.
/// @details The fill and child are queried without consuming or mutating the wrapper.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::single_pass_bounded_width_child<char_type, T>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_ch_t<placement, T, width_char_type>>) noexcept
{
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::print_single_pass_bounded_direct_put_area_width_child<
			char_type, T>
inline constexpr ::std::true_type print_single_pass_bounded_direct_put_area_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_ch_t<placement, T, width_char_type>>) noexcept
{
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::single_pass_bounded_width_child<char_type, T>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_ch_t<placement, T, width_char_type>>,
	::fast_io::manipulators::width_ch_t<placement, T, width_char_type> const &value,
	::std::size_t maximum_size) noexcept
{
	return ::fast_io::details::single_pass_bounded_width_size<char_type>(
		value.reference, value.width, maximum_size);
}

/// @brief Propagates the bound through a run-time placement selector.
/// @details Placement inspection is read-only and precedes a distinct emission operation.
template <::std::integral char_type, typename T>
	requires ::fast_io::details::single_pass_bounded_width_child<
		char_type, T>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_t<T>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename T>
	requires ::fast_io::details::print_single_pass_bounded_direct_put_area_width_child<
		char_type, T>
inline constexpr ::std::true_type print_single_pass_bounded_direct_put_area_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_t<T>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename T>
	requires ::fast_io::details::single_pass_bounded_width_child<
		char_type, T>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_t<T>>,
	::fast_io::manipulators::width_runtime_t<T> const &value,
	::std::size_t maximum_size) noexcept
{
	return ::fast_io::details::single_pass_bounded_width_size<char_type>(
		value.reference, value.width, maximum_size);
}

/// @brief Propagates the bound through run-time placement with one explicit fill code unit.
/// @details Both placement and fill are inspected through const access before the separate emission pass.
template <::std::integral char_type, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::single_pass_bounded_width_child<char_type, T>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_ch_t<T, width_char_type>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::print_single_pass_bounded_direct_put_area_width_child<
			char_type, T>
inline constexpr ::std::true_type print_single_pass_bounded_direct_put_area_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_ch_t<T, width_char_type>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::single_pass_bounded_width_child<char_type, T>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_ch_t<T, width_char_type>>,
	::fast_io::manipulators::width_runtime_ch_t<T, width_char_type> const &value,
	::std::size_t maximum_size) noexcept
{
	return ::fast_io::details::single_pass_bounded_width_size<char_type>(
		value.reference, value.width, maximum_size);
}

/// @brief Exposes a dynamic-precision descendant through fixed-placement width layout.
/// @details Width necessarily executes its stored child exactly once before applying padding. Forwarding this type-only
///          negative marker lets each IO consumer reject a compiler/version whose successful constant path retains the
///          child's precision planner. It evaluates neither width nor child and grants no materialization permission.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
	requires ::fast_io::compiler_constant_dynamic_precision_floating_source_shape<
		char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_dynamic_precision_floating_leaf(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_t<placement, T>>) noexcept
{
	return {};
}

/// @brief Exposes the same descendant when fixed placement owns an explicit fill code unit.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::compiler_constant_dynamic_precision_floating_source_shape<
			char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_dynamic_precision_floating_leaf(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_ch_t<
			placement, T, width_char_type>>) noexcept
{
	return {};
}

/// @brief Exposes a dynamic-precision descendant through run-time placement layout.
template <::std::integral char_type, typename T>
	requires ::fast_io::compiler_constant_dynamic_precision_floating_source_shape<
		char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_dynamic_precision_floating_leaf(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_t<T>>) noexcept
{
	return {};
}

/// @brief Exposes the same descendant when run-time placement owns an explicit fill code unit.
template <::std::integral char_type, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::compiler_constant_dynamic_precision_floating_source_shape<
			char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_dynamic_precision_floating_leaf(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_ch_t<
			T, width_char_type>>) noexcept
{
	return {};
}

// Width is a semantic/layout node, so its compiler-constant protocol recursively replaces only its child. The ordinary
// width types and format lowering remain algorithm-neutral; print/concat decide whether the complete bounded
// replacement is profitable for the destination.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_t<placement, T>>) noexcept
{
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_t<placement, T>>) noexcept
{
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T> &&
		::fast_io::compiler_constant_materialization_graph_proven_source_shape<
			char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_t<placement, T>>) noexcept
{
	// The child graph is independently proven; the width matrix adds the fixed placement and width fields.
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_t<placement, T>>,
	::fast_io::manipulators::width_t<placement, T> const &value) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	return __builtin_constant_p(value.width) &&
		value.width <= ::fast_io::details::compiler_constant_width_capacity<char_type> &&
		::fast_io::details::compiler_constant_width_child_eligible<char_type>(
			value.reference);
#else
	(void)value;
	return false;
#endif
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T>
// This CPO is a compiler-constant-protocol leaf. The GCC 13/15 and Clang 23 -O3 audit showed that removing only this
// boundary reconnects a constant width_t caller to the generic formatter, while its unknown-double body is identical.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr auto
print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_t<placement, T>>,
	::fast_io::manipulators::width_t<placement, T> const &value) noexcept
{
	return ::fast_io::details::compiler_constant_width_materialize_checked<placement>(
		value.reference, value.width,
		::fast_io::char_literal_v<u8' ', char_type>);
}

/// @brief Materializes fixed-placement width after core has observed the matching eligibility query as true.
/// @details The true query proves both the width capacity and the child's compact-size contract. This internal CPO is
///          discovered only by core's post-gate helper; the ordinary ADL materializer above retains its defensive check.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T>
// The four width gate-proven overloads need forced placement on tested GCC 13--16 and Clang 21--23 to preserve the
// caller's constant proof. GCC 11--12 and Clang 17--20 produce byte-identical objects with or without the attribute;
// every audited unknown-value wrapper is also unchanged. The positive policies therefore begin at the first measured
// code-generation boundary and remain open for newer frontends until a measured reversal; MSVC remains unforced.
#if (defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize_gate_proven(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_t<placement, T>>,
	::fast_io::manipulators::width_t<placement, T> const &value) noexcept
{
	return ::fast_io::details::compiler_constant_width_materialize<
		placement>(value.reference, value.width,
		::fast_io::char_literal_v<u8' ', char_type>);
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::compiler_constant_width_child<char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_ch_t<placement, T, width_char_type>>) noexcept
{
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::compiler_constant_width_child<char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_ch_t<placement, T, width_char_type>>) noexcept
{
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::compiler_constant_width_child<char_type, T> &&
		::fast_io::compiler_constant_materialization_graph_proven_source_shape<
			char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_ch_t<
			placement, T, width_char_type>>) noexcept
{
	// The explicit fill character has its own unknown-field query root in addition to the proven child and width.
	return {};
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::compiler_constant_width_child<char_type, T>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_ch_t<placement, T, width_char_type>>,
	::fast_io::manipulators::width_ch_t<placement, T, width_char_type> const &value) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	return __builtin_constant_p(value.width) && __builtin_constant_p(value.ch) &&
		value.width <= ::fast_io::details::compiler_constant_width_capacity<char_type> &&
		::fast_io::details::compiler_constant_width_child_eligible<char_type>(
			value.reference);
#else
	(void)value;
	return false;
#endif
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::compiler_constant_width_child<char_type, T>
// The matching GCC 13/15 and Clang 23 -O3 A/B proved this compiler-constant-protocol boundary independently for
// width_ch_t: without it the constant caller rejoins the generic formatter; the unknown-double caller is identical.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr auto
print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_ch_t<placement, T, width_char_type>>,
	::fast_io::manipulators::width_ch_t<placement, T, width_char_type> const &value) noexcept
{
	return ::fast_io::details::compiler_constant_width_materialize_checked<placement>(
		value.reference, value.width, value.ch);
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::compiler_constant_width_child<char_type, T>
#if (defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize_gate_proven(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_ch_t<placement, T, width_char_type>>,
	::fast_io::manipulators::width_ch_t<placement, T, width_char_type> const &value) noexcept
{
	return ::fast_io::details::compiler_constant_width_materialize<
		placement>(value.reference, value.width, value.ch);
}

template <::std::integral char_type, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_t<T>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_t<T>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T> &&
		::fast_io::compiler_constant_materialization_graph_proven_source_shape<
			char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_t<T>>) noexcept
{
	// Runtime placement and width are independently covered negative fields; consumer profitability is still separate.
	return {};
}

template <::std::integral char_type, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_t<T>>,
	::fast_io::manipulators::width_runtime_t<T> const &value) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	return __builtin_constant_p(value.placement) &&
		__builtin_constant_p(value.width) &&
		static_cast<::std::size_t>(value.placement) - 1u < 4u &&
		value.width <= ::fast_io::details::compiler_constant_width_capacity<char_type> &&
		::fast_io::details::compiler_constant_width_child_eligible<char_type>(
			value.reference);
#else
	(void)value;
	return false;
#endif
}

template <::std::integral char_type, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T>
// Run-time placement can itself be optimizer-proven constant. GCC 13/15 and Clang 23 -O3 require this constant-protocol
// boundary to finish materialization instead of reconnecting to the generic formatter; the fully unknown caller is identical.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr auto
print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_t<T>>,
	::fast_io::manipulators::width_runtime_t<T> const &value) noexcept
{
	return ::fast_io::details::compiler_constant_width_materialize_checked<
		::fast_io::manipulators::scalar_placement::none>(
		value.reference, value.width,
		::fast_io::char_literal_v<u8' ', char_type>, value.placement);
}

template <::std::integral char_type, typename T>
	requires ::fast_io::details::compiler_constant_width_child<char_type, T>
#if (defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize_gate_proven(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_t<T>>,
	::fast_io::manipulators::width_runtime_t<T> const &value) noexcept
{
	return ::fast_io::details::compiler_constant_width_materialize<
		::fast_io::manipulators::scalar_placement::none>(
		value.reference, value.width,
		::fast_io::char_literal_v<u8' ', char_type>, value.placement);
}

template <::std::integral char_type, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::compiler_constant_width_child<char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_ch_t<T, width_char_type>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::compiler_constant_width_child<char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_ch_t<T, width_char_type>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::compiler_constant_width_child<char_type, T> &&
		::fast_io::compiler_constant_materialization_graph_proven_source_shape<
			char_type, T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_ch_t<
			T, width_char_type>>) noexcept
{
	// The complete placement/width/fill/child field product is classified before any IO consumer may query it.
	return {};
}

template <::std::integral char_type, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::compiler_constant_width_child<char_type, T>
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_ch_t<T, width_char_type>>,
	::fast_io::manipulators::width_runtime_ch_t<T, width_char_type> const &value) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_constant_p)
	return __builtin_constant_p(value.placement) &&
		__builtin_constant_p(value.width) && __builtin_constant_p(value.ch) &&
		static_cast<::std::size_t>(value.placement) - 1u < 4u &&
		value.width <= ::fast_io::details::compiler_constant_width_capacity<char_type> &&
		::fast_io::details::compiler_constant_width_child_eligible<char_type>(
			value.reference);
#else
	(void)value;
	return false;
#endif
}

template <::std::integral char_type, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::compiler_constant_width_child<char_type, T>
// The placement-plus-fill variant has the same independently audited constant-only requirement: without this boundary
// GCC 13/15 and Clang 23 reconnect its constant caller to the generic formatter; the fully unknown caller is identical.
[[nodiscard]] FAST_IO_GNU_ALWAYS_INLINE inline constexpr auto
print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_ch_t<T, width_char_type>>,
	::fast_io::manipulators::width_runtime_ch_t<T, width_char_type> const &value) noexcept
{
	return ::fast_io::details::compiler_constant_width_materialize_checked<
		::fast_io::manipulators::scalar_placement::none>(
		value.reference, value.width, value.ch, value.placement);
}

template <::std::integral char_type, typename T,
	::std::integral width_char_type>
	requires ::std::same_as<char_type, width_char_type> &&
		::fast_io::details::compiler_constant_width_child<char_type, T>
#if (defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize_gate_proven(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::width_runtime_ch_t<T, width_char_type>>,
	::fast_io::manipulators::width_runtime_ch_t<T, width_char_type> const &value) noexcept
{
	return ::fast_io::details::compiler_constant_width_materialize<
		::fast_io::manipulators::scalar_placement::none>(
		value.reference, value.width, value.ch, value.placement);
}

namespace details
{

template <::std::integral char_type>
inline constexpr void
compiler_constant_width_fill(char_type *first, ::std::size_t count,
	char_type fill) noexcept
{
	for (; count != 0u; --count)
	{
		*first++ = fill;
	}
}

template <::std::integral char_type>
inline constexpr void
compiler_constant_width_move_right(char_type *first, ::std::size_t size,
	::std::size_t displacement) noexcept
{
	for (auto current{size}; current != 0u; --current)
	{
		first[current - 1u + displacement] = first[current - 1u];
	}
}

template <::fast_io::manipulators::scalar_placement placement,
	::std::integral char_type, typename T>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *
compiler_constant_width_define(
	char_type *first,
	::fast_io::manipulators::compiler_constant_width_t<
		placement, T, char_type> const &value) noexcept
{
	char_type *const child_end{print_reserve_precise_define(
		::fast_io::io_reserve_type<char_type, T>, first, value.child_size,
		value.reference)};
	if (child_end != first + value.child_size) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	auto const width{static_cast<::std::size_t>(value.width)};
	if (width <= value.child_size)
	{
		return child_end;
	}
	auto actual_placement{placement};
	if constexpr (placement ==
		::fast_io::manipulators::scalar_placement::none)
	{
		actual_placement = value.runtime_placement;
	}
	auto const padding{width - value.child_size};
	if (actual_placement ==
		::fast_io::manipulators::scalar_placement::left)
	{
		::fast_io::details::compiler_constant_width_fill(
			child_end, padding, value.fill);
		return first + width;
	}
	if (actual_placement ==
		::fast_io::manipulators::scalar_placement::middle)
	{
		auto const left_padding{padding >> 1u};
		auto const right_padding{padding - left_padding};
		::fast_io::details::compiler_constant_width_move_right(
			first, value.child_size, left_padding);
		::fast_io::details::compiler_constant_width_fill(
			first, left_padding, value.fill);
		::fast_io::details::compiler_constant_width_fill(
			first + left_padding + value.child_size, right_padding, value.fill);
		return first + width;
	}
	if (actual_placement ==
		::fast_io::manipulators::scalar_placement::internal)
	{
		if constexpr (::fast_io::printable_internal_shift<char_type, T>)
		{
			auto const internal_shift{print_define_internal_shift(
				::fast_io::io_reserve_type<char_type, T>, value.reference)};
			if (internal_shift <= value.child_size)
			{
				::fast_io::details::compiler_constant_width_move_right(
					first + internal_shift, value.child_size - internal_shift,
					padding);
				::fast_io::details::compiler_constant_width_fill(
					first + internal_shift, padding, value.fill);
				return first + width;
			}
			return child_end;
		}
	}
	// Right placement, invalid run-time placement, and internal placement without a child shift all share the mature
	// width fallback: pad before the already-produced child.
	::fast_io::details::compiler_constant_width_move_right(
		first, value.child_size, padding);
	::fast_io::details::compiler_constant_width_fill(
		first, padding, value.fill);
	return first + width;
}

} // namespace details

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_width_t<
			placement, T, char_type>>) noexcept
{
	return ::fast_io::details::compiler_constant_width_capacity<char_type>;
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_width_t<
			placement, T, char_type>>,
	char_type *iter,
	::fast_io::manipulators::compiler_constant_width_t<
		placement, T, char_type> const &value) noexcept
{
	return ::fast_io::details::compiler_constant_width_define(iter, value);
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
[[nodiscard]] inline constexpr ::std::size_t
print_reserve_precise_size(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_width_t<
			placement, T, char_type>>,
	::fast_io::manipulators::compiler_constant_width_t<
		placement, T, char_type> const &value) noexcept
{
	auto const width{static_cast<::std::size_t>(value.width)};
	return value.child_size < width ? width : value.child_size;
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
// Constant width-forwarding placement audit covers this proxy leaf and format's
// compiler-constant precision forwarding overload. With carrier sizing already
// available, exposing this leaf changes the focused GCC 15 O3 caller from
// 0x13a/one width call to 0x9a/one constant-carrier call;
// GCC 16 shows the same reduction. GCC 11--14 and Clang 17--23 leave both constant and unknown-value wrappers
// instruction-identical, so they deliberately retain ordinary placement even though older GCC suppresses 146--188
// bytes of incidental template emission. The positive GCC policy remains open until a newer compiler measures a
// reversal.
#if defined(__GNUC__) && !defined(__clang__) && 15 <= __GNUC__
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr char_type *print_reserve_precise_define(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_width_t<
			placement, T, char_type>> tag,
	char_type *iter, ::std::size_t precise_size,
	::fast_io::manipulators::compiler_constant_width_t<
		placement, T, char_type> const &value) noexcept
{
	(void)precise_size;
	return print_reserve_define(tag, iter, value);
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_placement placement, typename T>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_prefer_precise_compact(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::compiler_constant_width_t<
			placement, T, char_type>>) noexcept
{
	return {};
}

#if 0
namespace details
{

template <::std::integral char_type, typename T>
inline constexpr ::std::size_t print_reserve_size_width_impl(T t, ::std::size_t wid)
{
	if constexpr (reserve_printable<char_type, ::std::remove_cvref_t<T>>)
	{
		constexpr ::std::size_t sz{print_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>)};
		if (wid < sz)
		{
			return sz;
		}
	}
	else if constexpr (dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>>)
	{
		::std::size_t sz{print_reserve_size(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t)};
		if (wid < sz)
		{
			return sz;
		}
	}
	else if constexpr (scatter_printable<char_type, ::std::remove_cvref_t<T>>)
	{
		auto sz{print_scatter_define(io_reserve_type<char_type, ::std::remove_cvref_t<T>>, t).len};
		if (wid < sz)
		{
			return sz;
		}
	}
	return wid;
}

template <typename char_type, typename T>
concept print_reserve_static_stack_size_width_ok =
	::std::integral<char_type> && (reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
								   dynamic_reserve_with_possible_static_stack_size<char_type, ::std::remove_cvref_t<T>>);

template <::std::integral char_type, typename T>
	requires print_reserve_static_stack_size_width_ok<char_type, T>
inline constexpr ::std::size_t print_reserve_static_stack_size_width_impl() noexcept
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (reserve_printable<char_type, value_type>)
	{
		return print_reserve_size(io_reserve_type<char_type, value_type>);
	}
	else
	{
		return print_reserve_static_stack_size(io_reserve_type<char_type, value_type>);
	}
}

template <::fast_io::manipulators::scalar_placement placement, ::std::integral char_type>
inline constexpr char_type *handle_common_ch(char_type *first, char_type *last, ::std::size_t wd, char_type fillch)
{
	::std::size_t const diff{static_cast<::std::size_t>(last - first)};
	if (wd <= diff)
	{
		return last;
	}
	::std::size_t const to_fill_chs{wd - diff};
	if constexpr (placement == ::fast_io::manipulators::scalar_placement::left)
	{
		my_fill_n(last, to_fill_chs, fillch);
	}
	else if constexpr (placement == ::fast_io::manipulators::scalar_placement::middle)
	{
		constexpr ::std::size_t one{1};
		::std::size_t const left_indent{static_cast<::std::size_t>(to_fill_chs >> one)};
		::std::size_t const right_indent{to_fill_chs - left_indent};
		my_copy_right_shift(first, last, left_indent);
		my_fill_n(first, left_indent, fillch);
		my_fill_n(first + wd - right_indent, right_indent, fillch);
	}
	else
	{
		my_copy_right_shift(first, last, to_fill_chs);
		my_fill_n(first, to_fill_chs, fillch);
	}
	return first + wd;
}

template <::std::integral char_type>
inline constexpr char_type *handle_common_internal_ch(char_type *first, char_type *last, ::std::size_t wd,
													  char_type fillch, ::std::size_t internal_len)
{
	::std::size_t const diff1{static_cast<::std::size_t>(last - first)};
	if (wd <= diff1 || diff1 < internal_len)
	{
		return last;
	}
	first += internal_len;
	wd -= internal_len;
	::std::size_t const diff{static_cast<::std::size_t>(last - first)};
	::std::size_t const to_fill_chs{wd - diff};
	my_copy_right_shift(first, last, to_fill_chs);
	my_fill_n(first, to_fill_chs, fillch);
	return first + wd;
}

template <::fast_io::manipulators::scalar_placement placement, ::std::integral char_type, typename T>
inline constexpr char_type *print_reserve_define_width_ch_impl(char_type *iter, T t, ::std::size_t wdt,
															   char_type fillch)
{
	using value_type = ::std::remove_cvref_t<T>;
	if constexpr (placement == ::fast_io::manipulators::scalar_placement::internal)
	{
		if constexpr (printable_internal_shift<char_type, value_type>)
		{
			if constexpr (scatter_printable<char_type, value_type>)
			{
				auto sc{print_scatter_define(io_reserve_type<char_type, value_type>, t)};
				auto it{copy_scatter(sc, iter)};
				return handle_common_internal_ch(
					iter, it, wdt, fillch, print_define_internal_shift(io_reserve_type<char_type, value_type>, t));
			}
			else
			{
				char_type *it{print_reserve_define(io_reserve_type<char_type, value_type>, iter, t)};
				return handle_common_internal_ch(
					iter, it, wdt, fillch, print_define_internal_shift(io_reserve_type<char_type, value_type>, t));
			}
		}
		else
		{
			return print_reserve_define_width_ch_impl<::fast_io::manipulators::scalar_placement::right>(iter, t, wdt,
																										fillch);
		}
	}
	else
	{
		if constexpr (scatter_printable<char_type, value_type>)
		{
			auto sc{print_scatter_define(io_reserve_type<char_type, value_type>, t)};
			auto it{copy_scatter(sc, iter)};
			return handle_common_ch<placement>(iter, it, wdt, fillch);
		}
		else
		{
			char_type *it{print_reserve_define(io_reserve_type<char_type, value_type>, iter, t)};
			return handle_common_ch<placement>(iter, it, wdt, fillch);
		}
	}
}

template <::fast_io::manipulators::scalar_placement placement, ::std::integral char_type, typename T>
	requires ::std::is_trivially_copyable_v<T>
inline constexpr char_type *print_reserve_define_width_impl(char_type *iter, T t, ::std::size_t wdt)
{
	return print_reserve_define_width_ch_impl<placement>(iter, t, wdt, char_literal_v<u8' ', char_type>);
}

template <::std::integral char_type>
inline constexpr char_type *handle_common_rt_ch(::fast_io::manipulators::scalar_placement placement, char_type *first,
												char_type *last, ::std::size_t wd, char_type fillch)
{
	::std::size_t const diff{static_cast<::std::size_t>(last - first)};
	if (wd <= diff)
	{
		return last;
	}
	::std::size_t const to_fill_chs{wd - diff};
	if (placement == ::fast_io::manipulators::scalar_placement::left)
	{
		my_fill_n(last, to_fill_chs, fillch);
		return first + wd;
	}
	else if (placement == ::fast_io::manipulators::scalar_placement::middle)
	{
		::std::size_t one{1};
		::std::size_t const left_indent{static_cast<::std::size_t>(to_fill_chs >> one)};
		::std::size_t const right_indent{to_fill_chs - left_indent};
		my_copy_right_shift(first, last, left_indent);
		my_fill_n(first, left_indent, fillch);
		my_fill_n(first + wd - right_indent, right_indent, fillch);
		return first + wd;
	}
	else if (placement == ::fast_io::manipulators::scalar_placement::right)
	{
		my_copy_right_shift(first, last, to_fill_chs);
		my_fill_n(first, to_fill_chs, fillch);
		return first + wd;
	}
	else
	{
		return last;
	}
}

template <::std::integral char_type, typename T>
inline constexpr char_type *print_reserve_define_width_rt_ch_impl(char_type *iter,
																  ::fast_io::manipulators::scalar_placement placement,
																  T t, ::std::size_t wdt, char_type fillch)
{
	using value_type = ::std::remove_cvref_t<T>;
	if (placement == ::fast_io::manipulators::scalar_placement::internal)
	{
		if constexpr (printable_internal_shift<char_type, value_type>)
		{
			if constexpr (scatter_printable<char_type, value_type>)
			{
				auto sc{print_scatter_define(io_reserve_type<char_type, value_type>, t)};
				auto it{copy_scatter(sc, iter)};
				return handle_common_internal_ch(
					iter, it, wdt, fillch, print_define_internal_shift(io_reserve_type<char_type, value_type>, t));
			}
			else
			{
				char_type *it{print_reserve_define(io_reserve_type<char_type, value_type>, iter, t)};
				return handle_common_internal_ch(
					iter, it, wdt, fillch, print_define_internal_shift(io_reserve_type<char_type, value_type>, t));
			}
		}
		else
		{
			placement = ::fast_io::manipulators::scalar_placement::right;
		}
	}
	if constexpr (scatter_printable<char_type, value_type>)
	{
		auto sc{print_scatter_define(io_reserve_type<char_type, value_type>, t)};
		auto it{copy_scatter(sc, iter)};
		return handle_common_rt_ch(placement, iter, it, wdt, fillch);
	}
	else
	{
		char_type *it{print_reserve_define(io_reserve_type<char_type, value_type>, iter, t)};
		return handle_common_rt_ch(placement, iter, it, wdt, fillch);
	}
}

template <::std::integral char_type, typename T>
	requires ::std::is_trivially_copyable_v<T>
inline constexpr char_type *print_reserve_define_rt_width_impl(char_type *iter,
															   ::fast_io::manipulators::scalar_placement placement, T t,
															   ::std::size_t wdt)
{
	return print_reserve_define_width_rt_ch_impl(iter, placement, t, wdt, char_literal_v<u8' ', char_type>);
}

} // namespace details

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
	requires((reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  scatter_printable<char_type, ::std::remove_cvref_t<T>>) &&
			 (static_cast<::std::size_t>(static_cast<::std::size_t>(placement) - static_cast<::std::size_t>(1u)) <
			  static_cast<::std::size_t>(4u)))
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::width_t<placement, T>>,
										   ::fast_io::manipulators::width_t<placement, T> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(parameter<T>{t.reference}, t.width);
	}
	else
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(t.reference, t.width);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
	requires(::fast_io::details::print_reserve_static_stack_size_width_ok<char_type, T> &&
			 (static_cast<::std::size_t>(static_cast<::std::size_t>(placement) - static_cast<::std::size_t>(1u)) <
			  static_cast<::std::size_t>(4u)))
inline constexpr ::std::size_t
print_reserve_static_stack_size(io_reserve_type_t<char_type, ::fast_io::manipulators::width_t<placement, T>>) noexcept
{
	return ::fast_io::details::print_reserve_static_stack_size_width_impl<char_type, T>();
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
	requires((reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  scatter_printable<char_type, ::std::remove_cvref_t<T>>) &&
			 (static_cast<::std::size_t>(static_cast<::std::size_t>(placement) - static_cast<::std::size_t>(1u)) <
			  static_cast<::std::size_t>(4u)))
inline constexpr char_type *print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::width_t<placement, T>>,
										  char_type *iter, ::fast_io::manipulators::width_t<placement, T> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_define_width_impl<placement>(iter, parameter<T>{t.reference}, t.width);
	}
	else
	{
		return ::fast_io::details::print_reserve_define_width_impl<placement>(iter, t.reference, t.width);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
	requires(::fast_io::details::print_reserve_static_stack_size_width_ok<char_type, T> &&
			 (static_cast<::std::size_t>(static_cast<::std::size_t>(placement) - static_cast<::std::size_t>(1u)) <
			  static_cast<::std::size_t>(4u)))
inline constexpr ::std::size_t print_reserve_static_stack_size(
	io_reserve_type_t<char_type, ::fast_io::manipulators::width_ch_t<placement, T, char_type>>) noexcept
{
	return ::fast_io::details::print_reserve_static_stack_size_width_impl<char_type, T>();
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
	requires((reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  scatter_printable<char_type, ::std::remove_cvref_t<T>>) &&
			 (static_cast<::std::size_t>(static_cast<::std::size_t>(placement) - static_cast<::std::size_t>(1u)) <
			  static_cast<::std::size_t>(4u)))
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::width_ch_t<placement, T, char_type>>,
				   ::fast_io::manipulators::width_ch_t<placement, T, char_type> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(parameter<T>{t.reference}, t.width);
	}
	else
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(t.reference, t.width);
	}
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_placement placement, typename T>
	requires((reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			  scatter_printable<char_type, ::std::remove_cvref_t<T>>) &&
			 (static_cast<::std::size_t>(static_cast<::std::size_t>(placement) - static_cast<::std::size_t>(1u)) <
			  static_cast<::std::size_t>(4u)))
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::width_ch_t<placement, T, char_type>>,
					 char_type *iter, ::fast_io::manipulators::width_ch_t<placement, T, char_type> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_define_width_ch_impl<placement>(iter, parameter<T>{t.reference},
																				 t.width, t.ch);
	}
	else
	{
		return ::fast_io::details::print_reserve_define_width_ch_impl<placement>(iter, t.reference, t.width, t.ch);
	}
}

template <::std::integral char_type, typename T>
	requires(::fast_io::details::print_reserve_static_stack_size_width_ok<char_type, T>)
inline constexpr ::std::size_t print_reserve_static_stack_size(
	io_reserve_type_t<char_type, ::fast_io::manipulators::width_runtime_t<T>>) noexcept
{
	return ::fast_io::details::print_reserve_static_stack_size_width_impl<char_type, T>();
}

template <::std::integral char_type, typename T>
	requires(reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 scatter_printable<char_type, ::std::remove_cvref_t<T>>)
inline constexpr ::std::size_t print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::width_runtime_t<T>>,
										   ::fast_io::manipulators::width_runtime_t<T> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(parameter<T>{t.reference}, t.width);
	}
	else
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(t.reference, t.width);
	}
}

template <::std::integral char_type, typename T>
	requires(::fast_io::details::print_reserve_static_stack_size_width_ok<char_type, T>)
inline constexpr ::std::size_t print_reserve_static_stack_size(
	io_reserve_type_t<char_type, ::fast_io::manipulators::width_runtime_ch_t<T, char_type>>) noexcept
{
	return ::fast_io::details::print_reserve_static_stack_size_width_impl<char_type, T>();
}

template <::std::integral char_type, typename T>
	requires(reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 scatter_printable<char_type, ::std::remove_cvref_t<T>>)
inline constexpr char_type *print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::width_runtime_t<T>>,
										  char_type *iter, ::fast_io::manipulators::width_runtime_t<T> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_define_rt_width_impl(iter, t.placement, parameter<T>{t.reference},
																	  t.width);
	}
	else
	{
		return ::fast_io::details::print_reserve_define_rt_width_impl(iter, t.placement, t.reference, t.width);
	}
}

template <::std::integral char_type, typename T>
	requires(reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 scatter_printable<char_type, ::std::remove_cvref_t<T>>)
inline constexpr ::std::size_t
print_reserve_size(io_reserve_type_t<char_type, ::fast_io::manipulators::width_runtime_ch_t<T, char_type>>,
				   ::fast_io::manipulators::width_runtime_ch_t<T, char_type> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(parameter<T>{t.reference}, t.width);
	}
	else
	{
		return ::fast_io::details::print_reserve_size_width_impl<char_type>(t.reference, t.width);
	}
}

template <::std::integral char_type, typename T>
	requires(reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 dynamic_reserve_printable<char_type, ::std::remove_cvref_t<T>> ||
			 scatter_printable<char_type, ::std::remove_cvref_t<T>>)
inline constexpr char_type *
print_reserve_define(io_reserve_type_t<char_type, ::fast_io::manipulators::width_runtime_ch_t<T, char_type>>,
					 char_type *iter, ::fast_io::manipulators::width_runtime_ch_t<T, char_type> t) noexcept
{
	if constexpr (::std::is_reference_v<T>)
	{
		return ::fast_io::details::print_reserve_define_width_rt_ch_impl(iter, t.placement, parameter<T>{t.reference},
																		 t.width, t.ch);
	}
	else
	{
		return ::fast_io::details::print_reserve_define_width_rt_ch_impl(iter, t.placement, t.reference, t.width, t.ch);
	}
}

#endif
} // namespace fast_io

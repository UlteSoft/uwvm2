#pragma once

/*
 * Ordered semantic record node (`pack`) for the CPO/semantic level.
 *
 * A pack groups zero or more print objects so the IO planner can flatten them
 * into one logical operation, preserving order and one mutex/status boundary.
 * Construction applies print aliasing and selects lifetime-safe owned or
 * borrowed storage for every child. The pack itself performs no formatting;
 * active children are later interpreted by print or concat.
 */

#include "traits.h"

namespace fast_io
{

namespace details
{

template <typename T>
concept pack_value_transferable =
	::fast_io::details::io_print_forward_transport_by_value<T>;

// A pack reuses the normalized print transport policy, so it must not maintain a second architecture table. The shared
// admission first rejects incomplete objects without instantiating completeness-sensitive traits, then applies the
// target's conservative direct-value envelope and the C++ non-trivial-for-calls rules. This prevents semantic nesting
// from changing an lvalue from reference to value merely because the old two-word shortcut happened to accept its size.

template <typename T>
using pack_alias_result = decltype(::fast_io::io_print_alias(::std::declval<T>()));

/// @brief Selects lifetime-safe storage for one pack argument after print aliasing.
/// @details A custom alias returned from an lvalue may be a noncopyable lvalue proxy, so the pack preserves that exact
///          reference. When this branch is selected, that element is borrowed; any `pack_t` containing such an element
///          must not outlive the referenced source. A pack whose selected elements are all stored by value owns those
///          elements instead. The type system preserves the reference category but cannot prove run-time lifetime. A
///          result derived from an rvalue is always stored by value, even if a questionable customization returns an
///          lvalue reference, because retaining a subobject reference past the full-expression would dangle. Ordinary
///          small lvalues keep the historical value-copy optimization, while larger ordinary lvalues preserve their
///          actual cv-qualified reference instead of inventing constness.
template <typename T>
using pack_alias_type = ::std::conditional_t<
	::std::is_lvalue_reference_v<T &&> && ::fast_io::alias_printable<T> && ::std::is_lvalue_reference_v<::fast_io::details::pack_alias_result<T>>,
	::fast_io::details::pack_alias_result<T>,
	::std::conditional_t<
		::std::is_lvalue_reference_v<T &&> && !::fast_io::alias_printable<T>,
		::std::conditional_t<::fast_io::details::pack_value_transferable<T>, ::std::remove_cvref_t<T>, T>,
		::std::remove_cvref_t<::fast_io::details::pack_alias_result<T>>>>;

/// @brief Tests whether constructing one stored pack element is non-throwing.
template <typename T>
concept pack_alias_storable =
	requires {
		static_cast<::fast_io::details::pack_alias_type<T>>(
			::fast_io::io_print_alias(::std::declval<T>()));
	} &&
	(::std::is_lvalue_reference_v<::fast_io::details::pack_alias_type<T>> ||
	 ::std::constructible_from<
		 ::std::remove_cvref_t<::fast_io::details::pack_alias_type<T>>,
		 ::std::remove_cvref_t<::fast_io::details::pack_alias_type<T>> &&>);

/// @brief Computes the exception contract of the exact alias-and-storage expression used by `pack`.
/// @details The exact expression first rejects an rvalue alias that exposes only a noncopyable borrowed subobject. The
///          independent move requirement above models the next by-value decay boundary: guaranteed copy elision can
///          construct an immovable member here, but such a pack cannot participate in the normalized print/concat
///          pipeline and would only multiply unusable semantic instantiations.
template <typename T>
inline constexpr bool pack_alias_nothrow_constructible = []() constexpr {
	if constexpr (::fast_io::details::pack_alias_storable<T>)
	{
		return noexcept(static_cast<::fast_io::details::pack_alias_type<T>>(
			::fast_io::io_print_alias(::std::declval<T>())));
	}
	else
	{
		return false;
	}
}();

/// @brief Performs the single alias evaluation and materialization selected by `pack_alias_type`.
template <typename T>
	requires ::fast_io::details::pack_alias_storable<T>
inline constexpr ::fast_io::details::pack_alias_type<T> pack_store(T &&t) noexcept(::fast_io::details::pack_alias_nothrow_constructible<T>)
{
	return static_cast<::fast_io::details::pack_alias_type<T>>(
		::fast_io::io_print_alias(::std::forward<T>(t)));
}

} // namespace details

namespace manipulators
{

/// @brief Identifies a semantic node whose stored children are emitted sequentially.
/// @details This hidden tag lets print/concat planning distinguish pack expansion from an ordinary tuple payload.
struct pack_manip_tag_t
{};

/// @brief Stores a heterogeneous sequence of normalized printable children.
/// @details Children are emitted in source order without separators. Storage types may be values or stable references
///          as selected by `pack`; direct construction must preserve those reference lifetimes.
template <typename... Args>
struct pack_t
{
	using manip_tag = ::fast_io::manip_tag_t;
	using semantic_tag = pack_manip_tag_t;
	using storage_type = ::fast_io::containers::tuple<Args...>;
	inline static constexpr ::std::size_t size = sizeof...(Args);

	storage_type storage;
};

/// @brief Combines zero or more printable arguments into one separator-free semantic sequence.
/// @details Each argument is aliased exactly once and rvalue-derived aliases are materialized when necessary to avoid
///          dangling references. An empty pack emits nothing; no delimiter, whitespace, or terminator is inserted.
template <typename... Args>
	requires(::fast_io::details::pack_alias_storable<Args> && ...)
inline constexpr auto pack(Args &&...args) noexcept((::fast_io::details::pack_alias_nothrow_constructible<Args> && ...))
{
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-braces"
#endif
	return pack_t<::fast_io::details::pack_alias_type<Args>...>{
		::fast_io::containers::tuple<::fast_io::details::pack_alias_type<Args>...>{
			::fast_io::details::pack_store(::std::forward<Args>(args))...}};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}

} // namespace manipulators

/// @brief Propagates read-prefetch provenance through a semantic pack only when every emitted child is safe.
/// @details A pack expands all stored elements at run time, so one unproved non-empty child would invalidate a marker
///          on the composite even when every sibling were safe. `io_null_t` children contribute no range and therefore
///          satisfy the proof vacuously. The empty-pack fold is likewise true because it cannot supply a hint target.
template <typename... Args>
	requires(::fast_io::prfch_cacheable_read_or_no_external_range<Args> && ...)
inline constexpr ::std::true_type prfch_cacheable_read_provenance_define(
	io_type_t<manipulators::pack_t<Args...>>) noexcept
{
	return {};
}

} // namespace fast_io

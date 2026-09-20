#pragma once

/*
 * Format-specific semantic wrappers translated to IO protocols (FMT level).
 *
 * Some source-language rules cannot be represented by a bare scalar adapter,
 * for example internal padding around a sign/base prefix or precision-driven
 * presentation. The wrappers in this file retain that meaning while delegating
 * conversion to existing printable leaves. They are declarative IO objects:
 * orchestration, allocation, buffering, and device transfer remain outside
 * the FMT level.
 */

// These semantic leaves model reserve-print protocols, not output devices.
// Keeping the dependency freestanding preserves that distinction for both
// concat front doors and for non-ASCII execution character sets.
#include "../../fast_io_freestanding.h"

// Format details are parsed after the freestanding umbrella has restored the
// caller's macros. Re-enter fast_io's internal macro scope for capability probes.
#include "../../fast_io_dsal/impl/misc/push_macros.h"

#if !FAST_IO_HAS_BUILTIN(__builtin_signbit) || \
	!FAST_IO_HAS_BUILTIN(__builtin_isfinite)
#include <cmath>
#endif
#include <concepts>
#include <cstddef>
#include <type_traits>

namespace fast_io::manipulators
{

/// @brief Preserves format-specific sign and base-prefix semantics while delegating scalar conversion.
/// @details `base_prefix_size` extends the internal-padding shift so zero fill follows a sign and radix prefix—for
///          example, `#06x` produces `0x` before its padding. When `space_sign` is true, a generated positive `+` is
///          replaced by one space after emission; negative zero, NaN sign policy, digit generation, and sizing remain
///          those of the wrapped scalar.
template <typename scalar_type, ::std::size_t base_prefix_size, bool space_sign>
struct format_scalar_t
{
	using manip_tag = ::fast_io::manip_tag_t;
	scalar_type scalar;
};

} // namespace fast_io::manipulators

namespace fast_io
{

/**
 * Proves only the reserve semantics of an integral format scalar to IO.
 *
 * `format_scalar_t` changes sign and prefix spelling around a core integral scalar, while its
 * reserve extent remains type-static and its reserve definition remains non-throwing. This ADL
 * capability does not select an output strategy: IO core alone decides whether a sufficiently
 * large semantic run should consume the proof for contiguous bounded materialization. Percentage
 * output is intentionally excluded because it selects a separate suffix/ratio protocol rather
 * than the ordinary integral reserve definition proved here.
 */
template <::std::integral char_type,
		  ::fast_io::manipulators::scalar_flags flags, typename value_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires(::fast_io::details::my_integral<::std::remove_cvref_t<value_type>> &&
			 flags.percentage == ::fast_io::manipulators::percentage_flag::none)
inline constexpr ::std::true_type
	print_extended_bounded_passive_companion_safe(
		::fast_io::io_reserve_type_t<
			char_type,
			::fast_io::manipulators::format_scalar_t<
				::fast_io::manipulators::scalar_manip_t<flags, value_type>,
				base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// @brief Propagates the destination-neutral one-pass bounded marker through the format-specific scalar wrapper.
/// @details The wrapper changes sign/prefix spelling but delegates conversion and its dynamic reserve bound to the
///          wrapped scalar. Only a child with the complete source protocol can select a consumer's bounded path.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::single_pass_bounded_materialization_source<
		char_type, scalar_type>
inline constexpr ::std::true_type single_pass_bounded_materialization_preferred(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// @brief Propagates print's independent direct-put-area authorization through the format scalar wrapper.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires requires {
		{
			print_single_pass_bounded_direct_put_area_safe(
				::fast_io::io_reserve_type<char_type, scalar_type>)
		} -> ::std::same_as<::std::true_type>;
	}
inline constexpr ::std::true_type print_single_pass_bounded_direct_put_area_safe(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// @brief Forwards the non-fatal candidate bound through the format scalar wrapper.
/// @details Prefix placement and space-sign spelling do not add code units; they only reinterpret bytes already
///          covered by the child scalar's reserve bound. Preserving the caller's limit lets an extreme dynamic
///          precision reject speculative materialization before the ordinary fatal-overflow reserve protocol runs.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::single_pass_bounded_materialization_source<
		char_type, scalar_type>
inline constexpr ::std::size_t single_pass_bounded_materialization_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		scalar_type, base_prefix_size, space_sign> value,
	::std::size_t maximum_size) noexcept
{
	return ::fast_io::single_pass_bounded_materialization_size_invoke<char_type>(
		value.scalar, maximum_size);
}

template <::std::integral char_type, typename scalar_type, ::std::size_t base_prefix_size, bool space_sign>
	requires requires {
		print_reserve_size(::fast_io::io_reserve_type<char_type, scalar_type>);
	}
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign>>) noexcept
{
	// A static reserve size is a type policy, not a run-time call edge. Materializing the value here proves successful
	// constant evaluation even when an experimental effect annotation is present on the provider declaration.
	constexpr ::std::size_t result{
		print_reserve_size(::fast_io::io_reserve_type<char_type, scalar_type>)};
	return result;
}

#if defined(__HERBCEPTIONS__)
namespace details
{

/// @brief Classifies the deterministic effect of format-scalar protocol forwarding.
/// @details Each query names the child expression used by the corresponding wrapper body. Keeping this channel
///          separate from traditional `noexcept` expresses the language's two independent failure properties. Matching
///          named operand categories makes the declaration and its delegated protocol edge formally identical.
template <::std::integral char_type, typename scalar_type>
inline constexpr bool format_scalar_dynamic_reserve_size_herbceptions_may_fail =
	throws((print_reserve_size(
		::fast_io::io_reserve_type<char_type, scalar_type>,
		::std::declval<scalar_type &>())));

template <::std::integral char_type, typename scalar_type>
inline constexpr bool format_scalar_reserve_define_herbceptions_may_fail =
	throws((print_reserve_define(
		::fast_io::io_reserve_type<char_type, scalar_type>,
		::std::declval<char_type *&>(), ::std::declval<scalar_type &>())));

template <::std::integral char_type, typename scalar_type>
inline constexpr bool format_scalar_precise_size_herbceptions_may_fail =
	throws((print_reserve_precise_size(
		::fast_io::io_reserve_type<char_type, scalar_type>,
		::std::declval<scalar_type &>())));

template <::std::integral char_type, ::std::random_access_iterator iterator,
		  typename scalar_type>
inline constexpr bool format_scalar_precise_define_herbceptions_may_fail =
	throws((print_reserve_precise_define(
		::fast_io::io_reserve_type<char_type, scalar_type>,
		// Both by-value wrapper parameters are named lvalues at the child call. Reproducing those categories prevents
		// a char/iterator/size overload set from being classified through a different prvalue-only declaration.
		::std::declval<iterator &>(), ::std::declval<::std::size_t &>(),
		::std::declval<scalar_type &>())));

} // namespace details
#endif

/// Forwards an object-dependent reserve bound without pretending that it is static.
///
/// Runtime-precision floating manipulators deliberately expose the two-argument reserve
/// protocol: their requested precision participates in the upper bound.  Providing only
/// the static overload above would make the format wrapper cease to model
/// `dynamic_reserve_printable`, even though emission itself can still be delegated.  The
/// constrained overload keeps that capability distinction intact and therefore preserves
/// the core dispatcher's existing allocation strategy.
template <::std::integral char_type, typename scalar_type, ::std::size_t base_prefix_size, bool space_sign>
	requires requires(scalar_type value) {
		print_reserve_size(::fast_io::io_reserve_type<char_type, scalar_type>, value);
	}
inline constexpr ::std::size_t print_reserve_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign> value)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::format_scalar_dynamic_reserve_size_herbceptions_may_fail<
			char_type, scalar_type>),
		noexcept(print_reserve_size(
			::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar)))
{
	// The wrapper adds no run-time sizing work. Its deterministic effect and ordinary exception property are therefore
	// exactly those of the child call made with the stored scalar as a named lvalue.
	return print_reserve_size(
		::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar);
}

template <::std::integral char_type, typename scalar_type, ::std::size_t base_prefix_size, bool space_sign>
	requires requires(scalar_type value, char_type *iter) {
		print_reserve_define(::fast_io::io_reserve_type<char_type, scalar_type>, iter, value);
	}
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign>>,
	char_type *iter,
	::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign> value)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::format_scalar_reserve_define_herbceptions_may_fail<
			char_type, scalar_type>),
		noexcept(print_reserve_define(
			::fast_io::io_reserve_type<char_type, scalar_type>, iter, value.scalar)))
{
	// Sign spelling mutates only already-produced bytes and cannot fail. The wrapper consequently forwards the complete
	// failure contract of the named child emission without changing phase-1 scalar ownership.
	auto const begin{iter};
	auto const end{print_reserve_define(
		::fast_io::io_reserve_type<char_type, scalar_type>, iter, value.scalar)};
	if constexpr (space_sign)
	{
		if (begin != end && *begin == ::fast_io::char_literal_v<u8'+', char_type>)
		{
			*begin = ::fast_io::char_literal_v<u8' ', char_type>;
		}
	}
	return end;
}

/// Keeps the optional compiler-constant floating proxy visible through format's spelling wrapper.
///
/// This overload remains ordinary inline: independent GCC 13/15 and Clang 23
/// `#a` assembly probes were byte-for-byte unchanged after removing its former
/// forced attribute. The later precise-define leaf is the measured boundary
/// that actually requires intervention.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type,
	::std::size_t base_prefix_size, bool space_sign>
		requires requires(
			::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
				char_type, flags, floating_type> scalar,
		char_type *iter) {
		print_reserve_define(
			::fast_io::io_reserve_type<
				char_type,
				::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
					char_type, flags, floating_type>>,
			iter, scalar);
	}
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
				char_type, flags, floating_type>,
			base_prefix_size, space_sign>>,
	char_type *iter,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			char_type, flags, floating_type>,
		base_prefix_size, space_sign> value) noexcept
{
	auto const begin{iter};
	auto const end{print_reserve_define(
		::fast_io::io_reserve_type<
			char_type,
			::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
				char_type, flags, floating_type>>,
		iter, value.scalar)};
	if constexpr (space_sign)
	{
		if (begin != end && *begin == ::fast_io::char_literal_v<u8'+', char_type>)
		{
			*begin = ::fast_io::char_literal_v<u8' ', char_type>;
		}
	}
	return end;
}

/// Keeps the integer-fields precision proxy visible through the same format spelling wrapper.
/// @details A former print-only check found ordinary placement neutral, but the complete concat-level symbol audit
///          exposes the missing consumer: GCC 15 retains this exact wrapper call after every numeric field is already
///          constant. The overload's argument type can be formed only in a proven compiler-constant true arm, so
///          forcing this spelling-only leaf cannot expand the native runtime formatter. Format still translates syntax
///          only; print/concat owns recognition, materialization, allocation, and final strategy selection.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type,
	::std::size_t base_prefix_size, bool space_sign>
		 requires requires(
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				char_type, flags, floating_type> scalar,
			char_type *iter) {
		print_reserve_define(
			::fast_io::io_reserve_type<
				char_type,
				::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
					char_type, flags, floating_type>>,
			iter, scalar);
	}
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 13 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr char_type *print_reserve_define(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				char_type, flags, floating_type>,
			base_prefix_size, space_sign>>,
	char_type *iter,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			char_type, flags, floating_type>,
		base_prefix_size, space_sign> value) noexcept
{
	auto const begin{iter};
	auto const end{print_reserve_define(
		::fast_io::io_reserve_type<
			char_type,
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				char_type, flags, floating_type>>,
		iter, value.scalar)};
	if constexpr (space_sign)
	{
		if (begin != end && *begin == ::fast_io::char_literal_v<u8'+', char_type>)
		{
			*begin = ::fast_io::char_literal_v<u8' ', char_type>;
		}
	}
	return end;
}

/// Propagates the core compiler-constant protocol through format's sign/prefix spelling wrapper.
///
/// Format lowering only translates the parsed field to this semantic scalar.  The value-level decision and the
/// replacement formatter remain owned by the shared print/concat protocol below it.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::compiler_constant_query_inline_safe<
		char_type, scalar_type>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_query_inline_safe(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// Format lowering contributes only the spelling wrapper. The wrapped scalar's core opt-in is the proof that the
/// complete lowered leaf may cross print's pre-normalization replacement boundary.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::compiler_constant_pre_normalization_safe<
		char_type, scalar_type>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_pre_normalization_safe(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// @brief Propagates an already-classified provider graph through format's bounded spelling wrapper.
/// @details The wrapper's query is exactly the child's query, its materializer owns exactly one child proxy, and its
///          additional work is the type-bounded prefix/space-sign spelling covered by the permanent format-scalar
///          matrix. This marker does not turn an unclassified child into a candidate and does not authorize any format
///          policy: the lowered value still reaches the independent IO consumer proof. A novel child graph must first
///          satisfy its own field-complete query and recursive deletion contract.
template <::std::integral char_type, typename scalar_type,
	::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::compiler_constant_materialization_graph_proven_source_shape<
		char_type, scalar_type>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_materialization_graph_proven(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// @brief Preserves the child's flat-scalar source classification through a type-owned format spelling.
/// @details Prefix width and space-sign policy are compile-time members and introduce no condition, dynamic
///          width/precision owner, or nested value query. Materialization still produces one wrapper around one child
///          proxy. Clang 21--23 assembly A/B tests use this exact shape to reduce a literal printf integer from a
///          formatter call graph to one immediate byte store while leaving every unknown-value root byte-identical.
///          Older Clang versions remain closed by the IO-level compiler gate; this type-only CPO selects no strategy.
template <::std::integral char_type, typename scalar_type,
	::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::compiler_constant_simple_scalar_source_shape<
		char_type, scalar_type>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_simple_scalar_source(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// @brief Preserves a borrowed-text leaf classification through the format spelling wrapper.
/// @details Only the identity spelling (`base_prefix_size == 0` and no sign rewrite) can preserve the child's proof that
///          ordinary output is exactly its borrowed `[data, data + size)` range. A prefix or space-sign policy adds
///          formatting state and must not satisfy that contract merely because its child is borrowed. This type-only
///          CPO reads no characters and grants no replacement permission; it lets each IO-level consumer fail closed
///          before forming a redundant compiler-constant text proxy.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires(base_prefix_size == 0u && !space_sign) && requires {
		{
			print_compiler_constant_borrowed_text_leaf(
				::fast_io::io_reserve_type<char_type, scalar_type>)
		} -> ::std::same_as<::std::true_type>;
	}
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_borrowed_text_leaf(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// @brief Preserves the child's dynamic-precision floating classification through the spelling-only wrapper.
/// @details This forwarding CPO exposes no value and chooses no materialization strategy. It lets the IO-level
///          condition consumer apply its compiler-specific fail-closed proof to the original floating leaf before a
///          replacement type or optimizer query exists.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires requires {
		{
			print_compiler_constant_dynamic_precision_floating_leaf(
				::fast_io::io_reserve_type<char_type, scalar_type>)
		} -> ::std::same_as<::std::true_type>;
	}
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_dynamic_precision_floating_leaf(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// @brief Forwards the child's proof that a successful eligibility query already enforces the compact byte budget.
/// @details Width consumes this type-only fact to avoid materializing and exactly sizing an expensive precision float a
///          second time merely to repeat the same bound. Format itself makes no value-level decision here.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires requires {
		{
			print_compiler_constant_eligible_implies_compact_size(
				::fast_io::io_reserve_type<char_type, scalar_type>)
		} -> ::std::same_as<::std::true_type>;
	}
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_eligible_implies_compact_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	 requires ::fast_io::compiler_constant_printable<char_type, scalar_type>
// Tested GCC 11--16 otherwise outline this three-byte spelling query. That is not just
// a call-cost difference: `__builtin_constant_p` then observes the callee's
// parameter instead of the caller's expression, and the focused constant-fixed
// benchmark regresses from 11.8 ns to 21.6 ns. Keep the query attached to the
// source frame while leaving the underlying floating algorithm untouched. The
// active-condition recursive audit supersedes the former GCC 13 lower bound:
// GCC 11/12 otherwise emit an out-of-line query in both literal and unknown
// callers, retaining the mutually exclusive proxy and native graphs. The
// positive GCC policy remains open until a newer compiler measures a reversal.
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr bool
print_compiler_constant_materialization_eligible(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		scalar_type, base_prefix_size, space_sign> const &value) noexcept
{
	return print_compiler_constant_materialization_eligible(
		::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar);
}

template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	 requires ::fast_io::compiler_constant_printable<char_type, scalar_type>
// This construction leaf must share the source frame with the final materializer on tested GCC 13--16 and Clang 21--23.
// Representative GCC 15 and Clang 23 A/B probes substantially reduce the constant caller and total text while every
// audited dynamic object remains byte-identical. GCC 11--12 and Clang 17--20 produce byte-identical objects for this
// leaf, establishing the lower boundaries. The eligibility query above has no corresponding Clang benefit and therefore
// remains GCC-only. The positive policies remain open for newer frontends until a measured reversal; MSVC retains
// ordinary placement.
#if (defined(__GNUC__) && !defined(__clang__) && 13 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		scalar_type, base_prefix_size, space_sign> const &value) noexcept
{
	using materialized_scalar =
		::fast_io::details::compiler_constant_materialized_t<char_type, scalar_type>;
	return ::fast_io::manipulators::format_scalar_t<
		materialized_scalar, base_prefix_size, space_sign>{
		print_compiler_constant_materialize(
			::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar)};
}

/// Propagates core's already-observed eligibility proof through the spelling-only format wrapper.
///
/// The wrapper's eligibility query is exactly the child's query, so its true arm may use the child's optional
/// gate-proven materializer. The ordinary ADL CPO above deliberately remains the independently callable checked path.
/// Tested GCC 11--16 and Clang 21--23 need this bridge in the caller. In particular, the exact active-condition audit
/// shows GCC 11/12 otherwise retaining this true-arm call beside the native false arm even after all syntax lowering
/// has completed. Clang 17--20 retains its independently measured ordinary boundary. The positive policies remain open
/// for newer frontends until a measured reversal; MSVC retains ordinary placement.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::compiler_constant_printable<char_type, scalar_type>
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
[[nodiscard]] inline constexpr auto
print_compiler_constant_materialize_gate_proven(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		scalar_type, base_prefix_size, space_sign> const &value) noexcept
{
	using materialized_scalar =
		::fast_io::details::compiler_constant_materialized_t<char_type, scalar_type>;
	return ::fast_io::manipulators::format_scalar_t<
		materialized_scalar, base_prefix_size, space_sign>{
		print_compiler_constant_materialize_gate_proven(
			::fast_io::io_reserve_type<char_type, scalar_type>,
			value.scalar)};
}

/// Propagates the immutable-fragment representation through format's spelling-only scalar wrapper.
/// Format contributes no character buffer here: the core scalar owns every digit/punctuation table and the print
/// destination decides whether descriptors or ordinary reserve output are appropriate.  The sole spelling adjustment
/// made by this wrapper is printf/brace's space-sign rule, which substitutes a static space descriptor for a leading
/// plus without modifying any payload storage.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::compiler_constant_static_fragment_printable<
		char_type, scalar_type>
inline constexpr ::std::size_t print_compiler_constant_static_fragments_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	constexpr ::std::size_t result{print_compiler_constant_static_fragments_size(
		::fast_io::io_reserve_type<char_type, scalar_type>)};
	return result;
}

template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::compiler_constant_static_fragment_printable<
		char_type, scalar_type>
inline constexpr ::fast_io::basic_io_scatter_t<char_type> *
print_compiler_constant_static_fragments_define(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>,
	::fast_io::basic_io_scatter_t<char_type> *first,
	::fast_io::manipulators::format_scalar_t<
		scalar_type, base_prefix_size, space_sign> const &value) noexcept
{
	auto const last{print_compiler_constant_static_fragments_define(
		::fast_io::io_reserve_type<char_type, scalar_type>, first,
		value.scalar)};
	if constexpr (space_sign)
	{
		if (first != last && first->len != 0u &&
			*first->base == ::fast_io::char_literal_v<u8'+', char_type>)
		{
			first->base = __builtin_addressof(
				::fast_io::char_literal_v<u8' ', char_type>);
			first->len = 1u;
		}
	}
	return last;
}

/// @brief Preserves the child's expanded-fragment source classification through format's spelling wrapper.
/// @details A successful wrapper materialization contains exactly the child's materialized proxy and retains the same
///          type-level sign/prefix policy. The companion proxy marker below proves that this complete wrapped replacement
///          still expands to the wrapper's ordinary reserve spelling. This source CPO deliberately inspects only the
///          child's primitive type-only marker; it forms neither the replacement type nor a value query, so an IO
///          consumer can reject an unsupported source/compiler pair before either graph exists.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires requires {
		{
			print_compiler_constant_source_prefer_expanded_fragments(
				::fast_io::io_reserve_type<char_type, scalar_type>)
		} -> ::std::same_as<::std::true_type>;
	}
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_source_prefer_expanded_fragments(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// @brief Preserves the child's expanded immutable-fragment preference through format's spelling wrapper.
/// @details The wrapper forwards the child's descriptor bound and fragment order without creating payload storage.
///          Its only payload change, a leading plus-to-space substitution, is applied identically by the ordinary
///          reserve writer and by the fragment writer above; the replacement descriptor names static storage. The
///          `base_prefix_size` member affects only internal-padding placement and neither spelling. Consequently copying
///          the wrapped fragment sequence is exactly equivalent to the wrapped reserve spelling, while allocation and
///          destination selection remain exclusively owned by the consuming IO level.
template <::std::integral char_type, typename scalar_type,
		  ::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::compiler_constant_expanded_fragment_preferred<
		char_type, scalar_type>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_prefer_expanded_fragments(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

template <::std::integral char_type, typename scalar_type, ::std::size_t base_prefix_size, bool space_sign>
	requires requires {
		print_reserve_static_stack_size(::fast_io::io_reserve_type<char_type, scalar_type>);
	}
inline constexpr ::std::size_t print_reserve_static_stack_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign>>) noexcept
{
	// This policy is consumed as a compile-time bound. Caching it prevents an effect-annotated declaration from becoming
	// a run-time call hidden behind this wrapper's plain `noexcept` ABI.
	constexpr ::std::size_t result{
		print_reserve_static_stack_size(::fast_io::io_reserve_type<char_type, scalar_type>)};
	return result;
}

/// @brief Forwards exact-size queries for the already-materialized constant precision-float proxy.
/// @details Tested GCC 13--16 otherwise outline this spelling-only wrapper after the IO gate has built the integer-field
///          proxy. In the focused `{:.6f}` caller, retaining the boundary left 232 instructions and four calls on GCC
///          16; forcing only this proxy overload reduced it to 44 instructions and one error-only call, while the
///          unknown floating formatter cannot select this type. Clang 21--23 also require this forwarding leaf after
///          the prepared-record dispatcher structurally isolates its bounded direct branch; otherwise the retained
///          size call pulls the complete constant-precision fallback graph into a short literal record. Both positive
///          policies remain open until a newer compiler measures a reversal. Format contributes no sizing policy: the
///          core proxy owns the exact result.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename floating_type,
	::std::size_t base_prefix_size, bool space_sign>
	requires requires(
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			char_type, flags, floating_type> scalar) {
		print_reserve_precise_size(
			::fast_io::io_reserve_type<char_type, decltype(scalar)>, scalar);
	}
// Clang 21--23 otherwise retain a precise-size call and two stack-pointer adjustments in the static-precision caller;
// exposing this carrier leaf reduces it to 21 instructions, no calls, and no stack frame. Clang 17--20 produce the
// same object with ordinary or forced placement, establishing the lower boundary. The recursive active-record audit
// finds the equivalent retained proxy-size edge on GCC 11--16 after the source gate has succeeded. Unknown values
// cannot construct this proxy type. Both latest tested compilers are positive, so their policies remain open until a
// measured reversal.
#if defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__
FAST_IO_GNU_ALWAYS_INLINE
#elif defined(__clang__) && 21 <= __clang_major__
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				char_type, flags, floating_type>,
			base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			char_type, flags, floating_type>,
		base_prefix_size, space_sign> value) noexcept
{
	return print_reserve_precise_size(
		::fast_io::io_reserve_type<
			char_type,
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				char_type, flags, floating_type>>,
		value.scalar);
}

template <::std::integral char_type, typename scalar_type, ::std::size_t base_prefix_size, bool space_sign>
	requires requires(scalar_type value) {
		print_reserve_precise_size(::fast_io::io_reserve_type<char_type, scalar_type>, value);
	}
inline constexpr ::std::size_t print_reserve_precise_size(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign> value)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::format_scalar_precise_size_herbceptions_may_fail<
			char_type, scalar_type>),
		noexcept(print_reserve_precise_size(
			::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar)))
{
	// Exact sizing is transparently delegated through the named scalar member, so both failure properties equal those
	// of that one child protocol edge.
	return print_reserve_precise_size(
		::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar);
}

template <::std::integral char_type, ::std::random_access_iterator iterator,
		  typename scalar_type, ::std::size_t base_prefix_size, bool space_sign>
	requires requires(scalar_type value, iterator iter, ::std::size_t size) {
		print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, scalar_type>, iter, size, value);
	}
inline constexpr decltype(auto) print_reserve_precise_define(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign>>,
	iterator iter, ::std::size_t size,
	::fast_io::manipulators::format_scalar_t<scalar_type, base_prefix_size, space_sign> value)
	FAST_IO_HERBCEPTIONS_THROWS_OR_NOEXCEPT(
		(::fast_io::details::format_scalar_precise_define_herbceptions_may_fail<
			char_type, iterator, scalar_type>),
		noexcept(print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, scalar_type>, iter, size, value.scalar)))
{
	// `iter`, `size`, and `value.scalar` are all named lvalues in the executed child call; the constraint and both
	// effect proofs intentionally use those same categories. `decltype(auto)` retains either void or the exact endpoint
	// result rather than adding a transport conversion at this transparent layer.
	using define_result = decltype(print_reserve_precise_define(
		::fast_io::io_reserve_type<char_type, scalar_type>, iter, size, value.scalar));
	if constexpr (::std::same_as<define_result, void>)
	{
		print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, scalar_type>, iter, size, value.scalar);
		if constexpr (space_sign)
		{
			if (size != 0u && *iter == ::fast_io::char_literal_v<u8'+', char_type>)
			{
				*iter = ::fast_io::char_literal_v<u8' ', char_type>;
			}
		}
	}
	else
	{
		// Preserve an endpoint-reporting producer.  Returning `void` unconditionally
		// would remain semantically correct, but it would erase the stronger
		// `nothrow_precise_reserve_printable` proof used by overwrite-capable concat
		// destinations to avoid an initialization pass.
		auto result{print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, scalar_type>, iter, size, value.scalar)};
		if constexpr (space_sign)
		{
			if (iter != result && *iter == ::fast_io::char_literal_v<u8'+', char_type>)
			{
				*iter = ::fast_io::char_literal_v<u8' ', char_type>;
			}
		}
		return result;
	}
}

/// @brief Keeps a compiler-constant precision-float proxy visible across format's final precise-define forwarding leaf.
/// @details Clang 23 otherwise outlined only this spelling wrapper for `fmt::print<"v={:.3a}">(out(), 1.25)`, leaving
///          a 120-byte frame and a call after the core proxy had already become fully constant.  The overload is
///          intentionally limited to that replacement type; ordinary run-time scalar and format lowering paths keep
///          the generic compiler-selected inlining policy above. It is also the format half of width.h's measured
///          constant width-forwarding pair: GCC 15 reduced 0x13a/one width call to 0x9a/one carrier call, while the
///          GCC 13/15 and Clang 23 unknown-value normalized instruction hashes stayed identical.
template <::std::integral char_type, ::std::random_access_iterator iterator,
	::fast_io::manipulators::scalar_flags flags,
	::std::integral proxy_char_type, typename floating_type,
	::std::size_t base_prefix_size, bool space_sign>
	requires ::std::same_as<char_type, proxy_char_type> &&
		requires(
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				proxy_char_type, flags, floating_type> scalar,
			iterator iter, ::std::size_t size) {
			print_reserve_precise_define(
				::fast_io::io_reserve_type<
					char_type,
					::fast_io::manipulators::
						compiler_constant_floating_precision_manip_t<
							proxy_char_type, flags, floating_type>>,
				iter, size, scalar);
		}
// The active-record paired audit supersedes the former flat-source boundary. GCC 11/12 retain this exact proxy writer
// once the narrowly measured format source bridge is exposed; GCC 13--16 require the same leaf. The corresponding
// unknown and volatile symbols cannot select this replacement type and remain on their native formatter with no proxy
// reference. Clang 21--23 retain their independently measured positive range; Clang 17--20 remain ordinary. Both
// positive endpoints stay future-open until a measured reversal.
#if (defined(__GNUC__) && !defined(__clang__) && 11 <= __GNUC__) || \
	(defined(__clang__) && 21 <= __clang_major__)
FAST_IO_GNU_ALWAYS_INLINE
#endif
inline constexpr decltype(auto)
print_reserve_precise_define(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				proxy_char_type, flags, floating_type>,
			base_prefix_size, space_sign>>,
	iterator iter, ::std::size_t size,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			proxy_char_type, flags, floating_type>,
		base_prefix_size, space_sign> value) noexcept
{
	using scalar_type =
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			proxy_char_type, flags, floating_type>;
	using define_result = decltype(print_reserve_precise_define(
		::fast_io::io_reserve_type<char_type, scalar_type>, iter, size,
		value.scalar));
	if constexpr (::std::same_as<define_result, void>)
	{
		print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, scalar_type>, iter, size,
			value.scalar);
		if constexpr (space_sign)
		{
			if (size != 0u &&
				*iter == ::fast_io::char_literal_v<u8'+', char_type>)
			{
				*iter = ::fast_io::char_literal_v<u8' ', char_type>;
			}
		}
	}
	else
	{
		auto result{print_reserve_precise_define(
			::fast_io::io_reserve_type<char_type, scalar_type>, iter, size,
			value.scalar)};
		if constexpr (space_sign)
		{
			if (iter != result &&
				*iter == ::fast_io::char_literal_v<u8'+', char_type>)
			{
				*iter = ::fast_io::char_literal_v<u8' ', char_type>;
			}
		}
		return result;
	}
}

/// Propagates the child's flat integer replacement proof through format's spelling-only wrapper.
/// Format adds only compile-time prefix and space-sign spelling. It introduces no value field, loop, planner, dynamic
/// width, semantic branch, allocation, or native fallback, so the child's bounded integer graph remains complete.
template <::std::integral char_type, typename scalar_type,
	::std::size_t base_prefix_size, bool space_sign>
	requires ::fast_io::compiler_constant_flat_integer_replacement<
		char_type, scalar_type>
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_flat_integer_replacement(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// Propagates print's compact-before-fragments profitability marker through format's spelling-only wrapper.
/// Format contributes no strategy decision here: it merely preserves a type-level promise made by the lowered
/// scalar.  The print layer remains responsible for its output-device and size thresholds.
template <::std::integral char_type, typename scalar_type,
	::std::size_t base_prefix_size, bool space_sign>
	requires requires {
		{
			print_compiler_constant_prefer_precise_compact(
				::fast_io::io_reserve_type<char_type, scalar_type>)
		} -> ::std::same_as<::std::true_type>;
	}
[[nodiscard]] inline constexpr ::std::true_type
print_compiler_constant_prefer_precise_compact(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>) noexcept
{
	return {};
}

/// Propagates a provider-owned single immutable spelling through format's scalar wrapper.
template <::std::integral char_type, typename scalar_type,
	::std::size_t base_prefix_size, bool space_sign>
	requires requires(scalar_type const &value) {
		{
			print_compiler_constant_single_static_fragment(
				::fast_io::io_reserve_type<char_type, scalar_type>, value)
		} noexcept -> ::std::same_as<
			::fast_io::basic_io_scatter_t<char_type>>;
	}
[[nodiscard]] inline constexpr
	::fast_io::basic_io_scatter_t<char_type>
print_compiler_constant_single_static_fragment(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			scalar_type, base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		scalar_type, base_prefix_size, space_sign> const &value) noexcept
{
	return print_compiler_constant_single_static_fragment(
		::fast_io::io_reserve_type<char_type, scalar_type>, value.scalar);
}

namespace fmt::details
{

/**
 * Emits repetitions of one encoded format fill scalar.
 *
 * A fill scalar occupies one to four destination code units.  The common
 * one-unit case deliberately reaches core's constexpr-aware fill primitive;
 * at run time that has the memset shape expected by GCC and Clang.  Wider
 * encodings seed one scalar and then double the initialized prefix with
 * non-overlapping copies, avoiding a branch and store for every repetition.
 */
template <::std::integral char_type>
inline constexpr char_type *emit_repeated_code_unit_pattern(
	char_type *output, char_type const *pattern,
	::std::size_t pattern_size, ::std::size_t repetitions) noexcept
{
	if (repetitions == 0u || pattern_size == 0u)
	{
		return output;
	}
	if (4u < pattern_size) [[unlikely]]
	{
		::fast_io::fast_terminate();
	}
	if constexpr (::std::is_volatile_v<char_type>)
	{
		for (::std::size_t repetition{}; repetition != repetitions;
			 ++repetition)
		{
			for (::std::size_t index{}; index != pattern_size; ++index)
			{
				*output++ = pattern[index];
			}
		}
		return output;
	}
	else if (pattern_size == 1u)
	{
		return ::fast_io::details::my_fill_n(
			output, repetitions, pattern[0u]);
	}
	else
	{
		auto const total_size{
			::fast_io::details::intrinsics::mul_or_overflow_die(
				pattern_size, repetitions)};
		auto *const first{output};
		output = ::fast_io::details::non_overlapped_copy_n(
			pattern, pattern_size, output);
		::std::size_t produced{pattern_size};
		while (produced != total_size)
		{
			auto const remaining{total_size - produced};
			auto const copy_size{
				produced < remaining ? produced : remaining};
			output = ::fast_io::details::non_overlapped_copy_n(
				first, copy_size, output);
			produced += copy_size;
		}
		return output;
	}
}

/**
 * Classifies a scalar sign without changing the runtime floating ABI.
 *
 * Clang 17--19 do not accept `__builtin_signbit` while constant-evaluating the
 * IO-owned static provider.  Only that evaluation reads the IEC 60559 sign
 * field, preserving negative zero and signed NaNs.  Runtime calls deliberately
 * retain the established builtin path and its target-specific code generation.
 * The argument stays by value so native floating scalars use their register ABI.
 */
template <typename value_type>
[[nodiscard]] inline constexpr bool scalar_negative(value_type value) noexcept
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	if constexpr (::fast_io::details::floating_scalar_requires_integer_proxy<
		clean_type>)
	{
		return (::fast_io::bit_cast<::std::uint_least16_t>(value) & 0x8000u) != 0u;
	}
	else if constexpr (::fast_io::details::my_floating_point<clean_type>)
	{
		FAST_IO_IF_CONSTEVAL
		{
			return static_cast<bool>(
				::fast_io::details::compiler_constant_floating_capture_fields<
					clean_type>(value).sign);
		}
		else
		{
#if FAST_IO_HAS_BUILTIN(__builtin_signbit)
			return __builtin_signbit(value);
#else
			return ::std::signbit(value);
#endif
		}
	}
	else if constexpr (::fast_io::details::my_signed_integral<clean_type>)
	{
		return value < 0;
	}
	else
	{
		return false;
	}
}

/**
 * Classifies constant-evaluated finiteness from the stored exponent field.
 *
 * The representation branch lets old Clang initialize a static formatted
 * provider without evaluating or quieting a signaling NaN.  Runtime calls keep
 * the pre-existing builtin implementation byte-for-byte, including its native
 * floating argument ABI and target-specific classification instruction choice.
 */
template <typename value_type>
[[nodiscard]] inline constexpr bool scalar_finite(value_type value) noexcept
{
	using clean_type = ::std::remove_cvref_t<value_type>;
	if constexpr (::fast_io::details::floating_scalar_requires_integer_proxy<
		clean_type>)
	{
		auto const representation{
			::fast_io::bit_cast<::std::uint_least16_t>(value)};
		return ((representation >> 7u) & 0xffu) != 0xffu;
	}
	else if constexpr (::fast_io::details::my_floating_point<clean_type>)
	{
		FAST_IO_IF_CONSTEVAL
		{
			using trait = ::fast_io::details::iec559_traits<clean_type>;
			constexpr auto maximum_exponent{
				(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
			return ::fast_io::details::compiler_constant_floating_capture_fields<
				clean_type>(value).exponent != maximum_exponent;
		}
		else
		{
#if defined(FAST_IO_CLANG_HAS_BFLOAT16_TYPE) && defined(__clang__) && \
	__clang_major__ == 18 &&                                           \
	(defined(__aarch64__) || defined(_M_ARM64))
			if constexpr (::std::same_as<clean_type, __bf16>)
			{
				// Clang 18/AArch64 cannot select __builtin_isfinite(__bf16),
				// with or without +bf16. Clang 17 and Clang 19--23 were
				// audited successfully, so keep this backend workaround closed.
				using trait = ::fast_io::details::iec559_traits<clean_type>;
				constexpr auto maximum_exponent{
					(static_cast<::std::uint_least32_t>(1u) << trait::ebits) - 1u};
				return ::fast_io::details::compiler_constant_floating_capture_fields<
					clean_type>(value).exponent != maximum_exponent;
			}
			else
#endif
			{
#if FAST_IO_HAS_BUILTIN(__builtin_isfinite)
				return __builtin_isfinite(value);
#else
				return ::std::isfinite(value);
#endif
			}
		}
	}
	else
	{
		return true;
	}
}

template <::fast_io::manipulators::scalar_flags flags, typename value_type>
[[nodiscard]] inline constexpr ::std::size_t formatted_scalar_internal_shift(
	value_type value, ::std::size_t base_prefix_size) noexcept
{
	auto const sign_size{static_cast<::std::size_t>(
		flags.showpos || ::fast_io::fmt::details::scalar_negative(value))};
	auto const prefix_size{
		::fast_io::fmt::details::scalar_finite(value) ? base_prefix_size : 0u};
	return sign_size + prefix_size;
}

} // namespace fmt::details

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  typename value_type, ::std::size_t base_prefix_size, bool space_sign>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::scalar_manip_t<flags, value_type>,
			base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::scalar_manip_t<flags, value_type>,
		base_prefix_size, space_sign> value) noexcept
{
	return ::fast_io::fmt::details::formatted_scalar_internal_shift<flags>(
		value.scalar.reference, base_prefix_size);
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename value_type,
	::std::size_t base_prefix_size, bool space_sign>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::floating_scalar_field_manip_t<
				flags, value_type>,
			base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::floating_scalar_field_manip_t<
			flags, value_type>,
		base_prefix_size, space_sign> value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<value_type>;
	auto const fields{
		::fast_io::details::floating_scalar_proxy_fields<value_type>(
			value.scalar.representation)};
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
	return static_cast<::std::size_t>(flags.showpos || fields.sign) +
		(fields.exponent == exponent_mask ? 0u : base_prefix_size);
}

template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags, typename value_type,
	::std::size_t base_prefix_size, bool space_sign>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::floating_scalar_field_manip_precision_t<
				flags, value_type>,
			base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::floating_scalar_field_manip_precision_t<
			flags, value_type>,
		base_prefix_size, space_sign> value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<value_type>;
	auto const fields{
		::fast_io::details::floating_scalar_proxy_fields<value_type>(
			value.scalar.representation)};
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<typename trait::mantissa_type>(1u) << trait::ebits) - 1u)};
	return static_cast<::std::size_t>(flags.showpos || fields.sign) +
		(fields.exponent == exponent_mask ? 0u : base_prefix_size);
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  typename value_type, ::std::size_t base_prefix_size, bool space_sign>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>,
			base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::scalar_manip_precision_t<flags, value_type>,
		base_prefix_size, space_sign> value) noexcept
{
	return ::fast_io::fmt::details::formatted_scalar_internal_shift<flags>(
		value.scalar.reference, base_prefix_size);
}

/// @brief Preserves format-internal padding after an integer scalar is replaced by its compiler-constant proxy.
template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  typename value_type, ::std::size_t base_prefix_size, bool space_sign>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::compiler_constant_scalar_manip_t<
				flags, value_type>,
			base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::compiler_constant_scalar_manip_t<
			flags, value_type>,
		base_prefix_size, space_sign> value) noexcept
{
	return ::fast_io::fmt::details::formatted_scalar_internal_shift<flags>(
		value.scalar.reference, base_prefix_size);
}

/// @brief Preserves sign/prefix placement for an optimizer-proven default/scalar floating replacement.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags,
	::std::integral proxy_char_type, typename value_type,
	::std::size_t base_prefix_size, bool space_sign>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
				proxy_char_type, flags, value_type>,
			base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::compiler_constant_floating_scalar_manip_t<
			proxy_char_type, flags, value_type>,
		base_prefix_size, space_sign> const &value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<value_type>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u)};
	auto const sign_size{static_cast<::std::size_t>(
		flags.showpos || value.scalar.negative)};
	auto const prefix_size{value.scalar.binary_exponent == exponent_mask
		? 0u
		: base_prefix_size};
	return sign_size + prefix_size;
}

/// @brief Preserves sign/prefix placement for a compiler-constant explicit-precision floating replacement.
template <::std::integral char_type,
	::fast_io::manipulators::scalar_flags flags,
	::std::integral proxy_char_type, typename value_type,
	::std::size_t base_prefix_size, bool space_sign>
inline constexpr ::std::size_t print_define_internal_shift(
	::fast_io::io_reserve_type_t<
		char_type,
		::fast_io::manipulators::format_scalar_t<
			::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
				proxy_char_type, flags, value_type>,
			base_prefix_size, space_sign>>,
	::fast_io::manipulators::format_scalar_t<
		::fast_io::manipulators::compiler_constant_floating_precision_manip_t<
			proxy_char_type, flags, value_type>,
		base_prefix_size, space_sign> const &value) noexcept
{
	using trait = ::fast_io::details::iec559_traits<value_type>;
	using mantissa_type = typename trait::mantissa_type;
	constexpr auto exponent_mask{static_cast<::std::uint_least32_t>(
		(static_cast<mantissa_type>(1u) << trait::ebits) - 1u)};
	auto const sign_size{static_cast<::std::size_t>(
		flags.showpos || static_cast<bool>(value.scalar.fields.sign))};
	auto const prefix_size{value.scalar.fields.exponent == exponent_mask
		? 0u
		: base_prefix_size};
	return sign_size + prefix_size;
}

} // namespace fast_io

#include "../../fast_io_dsal/impl/misc/pop_macros.h"

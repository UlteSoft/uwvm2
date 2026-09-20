#pragma once

/*
 * Shared scatter-copy policy for IO materialization.
 *
 * Print, concat, and related materializers use these helpers only after a
 * caller has proved source extent, destination extent, stability, and
 * non-overlap. This file chooses code shape for copying static or small
 * run-time fragments; it is not a scatter-printable CPO and does not establish
 * lifetime or capacity by itself. Larger transfers remain delegated to the
 * core non-overlapping copy implementation.
 */

namespace fast_io::details::decay
{

template <typename value_type, ::std::size_t... index>
inline constexpr value_type *static_scatter_copy_indices(
	value_type const *source, value_type *destination,
	::std::index_sequence<index...>) noexcept
{
	((destination[index] = source[index]), ...);
	return destination + sizeof...(index);
}

/// @brief Copies a type-level fixed scatter without losing its extent to a run-time function parameter.
/// @details GCC recognizes a short loop whose count arrives as a function argument as an independent `memcpy`, even
///          after constant propagation. Adjacent literal fragments then remain separate stores and cannot be combined
///          with neighboring one-character manipulators. Keeping `count` in the template exposes every small assignment
///          before loop-distribution and lets the back end merge, for example, `"a", "bbb", chvw('c')` into the same
///          payload stores as `"abbbc"`. Only `static_scatter_t<count>` reserve CPOs call this helper, so their type
///          supplies the exact readable extent; a run-time descriptor does not acquire that proof from its pointer.
///          Extents above sixteen retain the memcpy-shaped non-overlapping path to avoid multiplying instructions at
///          large literal call sites.
/// @tparam count      exact number of source and destination elements
/// @tparam value_type the trivially addressable element type carried by the scatter
/// @param first       first source element of an extent containing at least `count` elements
/// @param result      first destination element of an extent containing at least `count` elements
/// @return value_type* one past the copied destination
template <::std::size_t count, typename value_type>
inline constexpr value_type *static_scatter_copy_n(
	value_type const *first, value_type *result) noexcept
{
	if constexpr (count == 0u)
	{
		// An empty scatter need not carry an array pointer. Return the representation unchanged: even adding zero to a
		// null pointer is not a defined pointer-arithmetic operation and is rejected during constant evaluation.
		return result;
	}
	else if constexpr (count <= 16u)
	{
		// An index expansion, rather than a counted loop, prevents GCC from recreating a separate memcpy before the
		// static-scatter source pointer itself has propagated to the literal object.
		return ::fast_io::details::decay::static_scatter_copy_indices(
			first, result, ::std::make_index_sequence<count>{});
	}
	else
	{
		return ::fast_io::details::non_overlapped_copy_n(first, count, result);
	}
}

/// @brief Copies a run-time scatter after its complete destination extent has been proved to fit a stable put area.
/// @details The SIMD-aware definition is provided after the core vector layer has been declared.  Keeping this
///          declaration beside the print materializers avoids moving the complete SIMD implementation ahead of the
///          operation concepts merely to make the put-area strategy target-aware.
/// @tparam value_type the trivially addressable element type carried by the scatter
/// @param first       first source element
/// @param count       number of elements to copy
/// @param result      first destination element
/// @return value_type* one past the copied destination
template <::std::integral value_type>
inline constexpr value_type *put_area_scatter_copy_n(
	value_type const *first, ::std::size_t count, value_type *result) noexcept;

/// @brief Copies a run-time scatter payload into an already-proved contiguous destination.
/// @details Repeated tiny descriptors, especially range separators, otherwise become out-of-line `memcpy` calls on
///          GCC even when a payload is only a few characters. The 16-element cutoff is shared by print coalescing,
///          concat, and range materialization so those strategy layers cannot silently acquire different lowering
///          policies. It is a measured cost threshold, not a capacity or lifetime proof; payloads above it continue
///          through the general non-overlapping copy routine. Increasing the cutoff requires both throughput and code-
///          size evidence because a compiler may emit one branch for every possible run-time length. GCC 15 measurements
///          showed the current cutoff removing per-separator calls, while also growing one N=128 range hot symbol from
///          roughly 526 to 1,150 bytes; this explicit trade-off must remain visible at the shared policy boundary.
/// @tparam value_type the trivially addressable element type carried by the scatter
/// @param first       first source element
/// @param count       number of elements to copy
/// @param result      first destination element
/// @return value_type* one past the copied destination
template <typename value_type>
inline constexpr value_type *small_scatter_copy_n(
	value_type const *first, ::std::size_t count, value_type *result) noexcept
{
	if (count <= 16u)
	{
		for (::std::size_t i{}; i != count; ++i)
		{
			result[i] = first[i];
		}
		return result + count;
	}
	return ::fast_io::details::non_overlapped_copy_n(first, count, result);
}

} // namespace fast_io::details::decay

#pragma once

/*
 * Forward declarations for the semantic print-object vocabulary.
 *
 * Keeping these structural node names independent of their implementations
 * lets CPO recognition and operation planning classify nested semantic records
 * without include cycles. A declaration here conveys no printable capability;
 * the node definitions and their child/storage contracts live in the
 * corresponding semantic headers.
 */

namespace fast_io
{

namespace manipulators
{

/// @brief Forward declaration of the heterogeneous semantic print pack.
/// @details The complete type stores independently aliased children and emits them in source order.
template <typename... Args>
struct pack_t;

/// @brief Forward declaration of the run-time conditional semantic print node.
/// @details The complete type stores both normalized arms and selects exactly one when formatted.
template <typename T1, typename T2>
struct condition;

/// @brief Forward declaration of the scalar padding-placement policy.
/// @details The complete enumeration is defined by the scalar manipulator layer.
enum class scalar_placement : char8_t;

/// @brief Forward declaration of a default-fill width node with type-level placement.
/// @details `flags` determines placement and `T` is the normalized child storage type.
template <scalar_placement flags, typename T>
struct width_t;

/// @brief Forward declaration of an explicit-fill width node with type-level placement.
/// @details `ch_type` is the fill-character code-unit type.
template <scalar_placement flags, typename T, ::std::integral ch_type>
struct width_ch_t;

/// @brief Forward declaration of a default-fill width node with run-time placement.
/// @details The complete type stores placement, child, and minimum width.
template <typename T>
struct width_runtime_t;

/// @brief Forward declaration of an explicit-fill width node with run-time placement.
/// @details The complete type stores placement, child, minimum width, and fill code unit.
template <typename T, ::std::integral ch_type>
struct width_runtime_ch_t;

} // namespace manipulators

} // namespace fast_io

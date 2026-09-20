#pragma once

namespace fast_io
{

namespace manipulators
{

/// @brief Hidden carrier requesting a diagnostic view of an object's internal representation.
/// @details The payload is normally a const reference; output format is implementation-specific and intended for
///          debugging rather than stable serialization.
template <typename T>
struct debug_view_t
{
	using manip_tag = manip_tag_t;
	T reference;
};

/// @brief Wraps an object in its library-defined diagnostic view.
/// @details The result borrows `v`; it does not copy, validate, or stabilize implementation-specific internals.
template <typename T>
inline constexpr debug_view_t<T const &> debug_view(T const &v) noexcept
{
	return ::fast_io::manipulators::debug_view_t<T const &>{v};
}

} // namespace manipulators

} // namespace fast_io

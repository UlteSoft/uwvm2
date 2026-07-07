#pragma once

namespace fast_io
{

namespace manipulators
{

template <typename T>
struct debug_view_t
{
	using manip_tag = manip_tag_t;
	T reference;
};

template <typename T>
inline constexpr debug_view_t<T const &> debug_view(T const &v) noexcept
{
	return ::fast_io::manipulators::debug_view_t<T const &>{v};
}

} // namespace manipulators

} // namespace fast_io

#pragma once

#include "custom.h"

namespace fast_io::details
{

inline constexpr ::std::size_t print_stack_buffer_default_max_bytes{
	::fast_io::custom::print_stack_buffer_default_max_bytes<>::value};

template <::std::size_t requested_max_bytes>
struct print_stack_policy
{
	static inline constexpr ::std::size_t max_bytes{requested_max_bytes};
};

using default_print_stack_policy = ::fast_io::details::print_stack_policy<
	::fast_io::details::print_stack_buffer_default_max_bytes>;

template <typename stack_policy = ::fast_io::details::default_print_stack_policy>
inline constexpr ::std::size_t print_stack_buffer_max_bytes() noexcept
{
	return static_cast<::std::size_t>(stack_policy::max_bytes);
}

} // namespace fast_io::details

namespace fast_io
{

using default_print_stack_policy = ::fast_io::details::default_print_stack_policy;

template <::std::size_t requested_max_bytes>
using print_stack_policy = ::fast_io::details::print_stack_policy<requested_max_bytes>;

} // namespace fast_io

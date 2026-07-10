#pragma once

namespace fast_io
{

namespace manipulators
{

template <typename... Args>
struct pack_t;

template <typename T1, typename T2>
struct condition;

enum class scalar_placement : char8_t;

template <scalar_placement flags, typename T>
struct width_t;

template <scalar_placement flags, typename T, ::std::integral ch_type>
struct width_ch_t;

template <typename T>
struct width_runtime_t;

template <typename T, ::std::integral ch_type>
struct width_runtime_ch_t;

} // namespace manipulators

} // namespace fast_io

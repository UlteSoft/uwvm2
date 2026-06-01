#pragma once

#ifndef FAST_IO_NOT_USE_FAST_FLOAT
#include <fast_float.h>

namespace fast_io
{

namespace details
{

template <typename T>
concept fast_float_supported_floating_point =
	::fast_io::details::my_floating_point<T> && ::fast_float::is_supported_float_type<::std::remove_cv_t<T>>::value;

[[nodiscard]] inline constexpr ::fast_float::chars_format fast_float_scan_format(::fast_io::manipulators::floating_format fmt) noexcept
{
	using floating_format = ::fast_io::manipulators::floating_format;
	switch (fmt)
	{
	case floating_format::fixed:
		return ::fast_float::chars_format::fixed;
	case floating_format::scientific:
		return ::fast_float::chars_format::scientific;
	case floating_format::general:
	case floating_format::decimal:
	default:
		return ::fast_float::chars_format::general;
	}
}

[[nodiscard]] inline constexpr ::fast_io::parse_code fast_float_scan_parse_code(::std::errc ec) noexcept
{
	if (ec == ::std::errc{}) [[likely]]
	{
		return ::fast_io::parse_code::ok;
	}
	if (ec == ::std::errc::result_out_of_range)
	{
		return ::fast_io::parse_code::overflow;
	}
	return ::fast_io::parse_code::invalid;
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  fast_float_supported_floating_point T>
inline constexpr ::fast_io::parse_result<char_type const *>
scan_fast_float_contiguous_define_impl(char_type const *begin, char_type const *end, T &value) noexcept
{
	if constexpr (!flags.noskipws)
	{
		begin = ::fast_io::details::find_space_common_impl<false, true>(begin, end);
		if (begin == end)
		{
			return {begin, ::fast_io::parse_code::end_of_file};
		}
	}

	auto fmt{::fast_io::details::fast_float_scan_format(flags.floating)};
	auto const res{::fast_float::from_chars(begin, end, value, fmt)};
	return {res.ptr, ::fast_io::details::fast_float_scan_parse_code(res.ec)};
}

} // namespace details

template <details::fast_float_supported_floating_point T>
inline constexpr ::fast_io::manipulators::scalar_manip_t<::fast_io::manipulators::floating_point_default_scalar_flags,
														 T &>
scan_alias_define(io_alias_t, T &value) noexcept
{
	return {value};
}

template <::std::integral char_type, ::fast_io::manipulators::scalar_flags flags,
		  details::fast_float_supported_floating_point T>
	requires(flags.floating != ::fast_io::manipulators::floating_format::hexfloat)
inline constexpr ::fast_io::parse_result<char_type const *>
scan_contiguous_define(io_reserve_type_t<char_type, ::fast_io::manipulators::scalar_manip_t<flags, T &>>,
					   char_type const *begin, char_type const *end,
					   ::fast_io::manipulators::scalar_manip_t<flags, T &> value) noexcept
{
	return ::fast_io::details::scan_fast_float_contiguous_define_impl<char_type, flags>(begin, end, value.reference);
}

} // namespace fast_io

#endif

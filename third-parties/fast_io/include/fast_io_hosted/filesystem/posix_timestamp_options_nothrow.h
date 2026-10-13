#pragma once

// Include after posix_timestamps_nothrow.h. This is the error-returning
// conversion layer for the existing fast_io unix_timestamp_option interface.
// Only a validated, caller-owned NUL-terminated path is borrowed, through one
// synchronous native operation. No path allocation or native capability lookup.
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#include <cerrno>
#include <cstddef>
#include <concepts>
#include <limits>
#include <type_traits>
#include <utility>

namespace fast_io::details::posix_timestamp_options_nothrow
{
template<typename Native>
concept native_timestamp_fields = requires(Native const& value)
{
	value.tv_sec;
	value.tv_nsec;
} && ::std::signed_integral<::std::remove_cvref_t<decltype(::std::declval<Native const&>().tv_sec)>> &&
	::std::signed_integral<::std::remove_cvref_t<decltype(::std::declval<Native const&>().tv_nsec)>>;

template<native_timestamp_fields Native>
[[nodiscard]] inline constexpr int convert_one(unix_timestamp_option option, Native& output) noexcept
{
#if defined(UTIME_NOW) && defined(UTIME_OMIT)
	using seconds_type = ::std::remove_cvref_t<decltype(output.tv_sec)>;
	using nanoseconds_type = ::std::remove_cvref_t<decltype(output.tv_nsec)>;
	switch (option.flags)
	{
	case utime_flags::now:
		output.tv_sec = 0;
		output.tv_nsec = static_cast<nanoseconds_type>(UTIME_NOW);
		return 0;
	case utime_flags::omit:
		output.tv_sec = 0;
		output.tv_nsec = static_cast<nanoseconds_type>(UTIME_OMIT);
		return 0;
	default:
		break; // Preserve the existing option interface's explicit-time default.
	}
	// NOW/OMIT never consult the unused numerical operand. An explicit value
	// must be representable by the selected native ABI before any narrowing;
	// this fixes the old path interface's silent 32-bit seconds truncation.
	if (!::std::in_range<seconds_type>(option.timestamp.seconds)) return EOVERFLOW;
	constexpr auto factor{::fast_io::uint_least64_subseconds_per_second / 1000000000u};
	auto const nanoseconds{option.timestamp.subseconds / factor};
	if (!::std::in_range<nanoseconds_type>(nanoseconds)) return EOVERFLOW;
	output.tv_sec = static_cast<seconds_type>(option.timestamp.seconds);
	output.tv_nsec = static_cast<nanoseconds_type>(nanoseconds);
	// A representable nanosecond >=1e9 remains a native EINVAL, rather than
	// introducing a new validation order for native descriptor/path errors.
	return 0;
#else
	return EINVAL;
#endif
}

template<native_timestamp_fields Native>
[[nodiscard]] inline constexpr int convert_pair(unix_timestamp_option access,
	unix_timestamp_option modification, Native (&output)[2]) noexcept
{
	unix_timestamp_option const options[2]{access, modification};
	for (::std::size_t i{}; i != 2u; ++i)
	{
		// [two complete owned options] end [two complete owned native cells] end
		// [safe                      ]     [safe                          ] i<2.
		// A failed conversion only modifies private local storage: no native
		// operation or externally observable timestamp mutation has occurred.
		if (int const error{convert_one(options[i], output[i])}; error != 0) return error;
	}
	return 0;
}

inline constexpr bool available{
#if defined(UTIME_NOW) && defined(UTIME_OMIT)
# if defined(__linux__) && __has_include(<linux/time_types.h>) && defined(__NR_utimensat_time64)
	::fast_io::details::posix_timestamps_nothrow_abi::wide_kernel_source_available<::__kernel_timespec>
# elif defined(__linux__) && __has_include(<linux/time_types.h>) && defined(__NR_utimensat)
	::fast_io::details::posix_timestamps_nothrow_abi::native_kernel_source_available<::__kernel_old_timespec>
# else
	::fast_io::details::posix_timestamps_nothrow_abi::sdk_available
# endif
#else
	false
#endif
};
} // namespace fast_io::details::posix_timestamp_options_nothrow

namespace fast_io
{
template<typename = void>
	requires details::posix_timestamp_options_nothrow::available
[[nodiscard]] inline posix_operation_result posix_utime_options_nothrow(posix_at_entry directory,
	char const* validated_path, unix_timestamp_option creation, unix_timestamp_option access,
	unix_timestamp_option modification, int raw_flags) noexcept
{
	// POSIX cannot set creation time, exactly as the existing throwing API.
	if (creation.flags != utime_flags::omit) return {EINVAL};
#if defined(__linux__) && __has_include(<linux/time_types.h>) && defined(__NR_utimensat_time64)
	struct ::__kernel_timespec owned[2]{};
	if (int const error{details::posix_timestamp_options_nothrow::convert_pair(access, modification, owned)}; error != 0)
		return {error};
	// Preserve the existing raw time64 selection and its real ENOSYS. Never
	// substitute glibc's fallback or retry a different native operation.
	return ::fast_io::linux_utimensat_time64_nothrow(directory, validated_path, owned, raw_flags);
#elif defined(__linux__) && __has_include(<linux/time_types.h>) && defined(__NR_utimensat)
	struct ::__kernel_old_timespec owned[2]{};
	if (int const error{details::posix_timestamp_options_nothrow::convert_pair(access, modification, owned)}; error != 0)
		return {error};
	return ::fast_io::linux_utimensat_nothrow(directory, validated_path, owned, raw_flags);
#else
# if (defined(__APPLE__) && defined(__MACH__) && defined(UTIME_NOW) && defined(UTIME_OMIT)) || \
     (defined(__linux__) && defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__BIONIC__) && \
      defined(__USE_ATFILE) && defined(__USE_XOPEN2K8) && \
      ((__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 6)) && \
      (!defined(__USE_TIME_BITS64) || defined(__USE_TIME64_REDIRECTS) || defined(__TIMESIZE)))
	struct ::timespec owned[2]{};
	if (int const error{details::posix_timestamp_options_nothrow::convert_pair(access, modification, owned)}; error != 0)
		return {error};
	return ::fast_io::posix_utimensat_nothrow(directory, validated_path, owned, raw_flags);
# else
	return {ENOSYS}; // Unavailable templates are constrained out, not providers.
# endif
#endif
}
} // namespace fast_io
#endif

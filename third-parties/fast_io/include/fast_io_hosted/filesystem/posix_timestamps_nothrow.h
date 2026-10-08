#pragma once

// Include after fast_io's POSIX file and existing error-returning provider.
// Native paths borrow validated NUL-terminated storage through ONE synchronous
// call. Typed references below require two complete initialized time values.
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#include <cstddef>
#include <cerrno>
#include <ctime>
#include <concepts>
#include <limits>
#include <type_traits>
#include <utility>
#include <sys/stat.h>
#if defined(__APPLE__) && defined(__MACH__)
#include <sys/time.h>
#elif __has_include(<linux/time_types.h>)
#include <linux/time_types.h>
#define FAST_IO_POSIX_TIMESTAMP_HAS_KERNEL_TYPES 1
#endif

namespace fast_io::details::posix_timestamps_nothrow_abi
{
// Match actual GNU SDK redirections, rather than inferring a libc symbol from
// sizeof(time_t). Unknown libcs retain their prior provider until a separate
// SDK/source/binding qualification establishes their contract.
#if defined(__APPLE__) && defined(__MACH__) && defined(UTIME_NOW) && defined(UTIME_OMIT)
inline constexpr bool sdk_available{true};
[[clang::availability(macos, introduced = 10.13), clang::availability(ios, introduced = 11.0),
  clang::availability(tvos, introduced = 11.0), clang::availability(watchos, introduced = 4.0)]]
extern int fd_times(int, struct ::timespec const*) noexcept __asm__("_futimens");
[[clang::availability(macos, introduced = 10.13), clang::availability(ios, introduced = 11.0),
  clang::availability(tvos, introduced = 11.0), clang::availability(watchos, introduced = 4.0)]]
extern int at_times(int, char const*, struct ::timespec const*, int) noexcept __asm__("_utimensat");
extern int fd_microseconds(int, struct ::timeval const*) noexcept __asm__("_futimes");
extern int path_microseconds(char const*, struct ::timeval const*) noexcept __asm__("_utimes");

inline int legacy_microseconds(struct ::timespec const (&times)[2], struct ::timeval (&converted)[2]) noexcept
{
	for (::std::size_t i{}; i != 2u; ++i)
	{
		// [two caller-owned complete time values] end
		// [safe                                ] i<2 before member access.
		// Preserve the existing older-Darwin fallback's validation precedence:
		// NOW/OMIT are unsupported before any native descriptor/path lookup.
		if (times[i].tv_nsec == UTIME_NOW || times[i].tv_nsec == UTIME_OMIT) return ENOSYS;
		if (times[i].tv_nsec < 0 || times[i].tv_nsec >= 1000000000L) return EINVAL;
		converted[i].tv_sec = times[i].tv_sec;
		converted[i].tv_usec = static_cast<decltype(converted[i].tv_usec)>(times[i].tv_nsec / 1000L);
	}
	return 0;
}
inline ::fast_io::posix_operation_result checked_fd_times(int descriptor, struct ::timespec const (&times)[2]) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_available)
	if (__builtin_available(macOS 10.13, iOS 11.0, tvOS 11.0, watchOS 4.0, *)) [[likely]]
	{
		if (fd_times(descriptor, times) < 0) return {::fast_io::details::posix_nothrow_abi::last_error()};
		return {};
	}
	struct ::timeval converted[2]{};
	if (int const error{legacy_microseconds(times, converted)}; error != 0) return {error};
	if (fd_microseconds(descriptor, converted) < 0)
		return {::fast_io::details::posix_nothrow_abi::last_error()};
	return {};
#else
	if (fd_times(descriptor, times) < 0) return {::fast_io::details::posix_nothrow_abi::last_error()};
	return {};
#endif
}
inline ::fast_io::posix_operation_result checked_at_times(int descriptor, char const* path,
	struct ::timespec const (&times)[2], int flags) noexcept
{
#if FAST_IO_HAS_BUILTIN(__builtin_available)
	if (__builtin_available(macOS 10.13, iOS 11.0, tvOS 11.0, watchOS 4.0, *)) [[likely]]
	{
		if (at_times(descriptor, path, times, flags) < 0) return {::fast_io::details::posix_nothrow_abi::last_error()};
		return {};
	}
	// This is exactly the existing fast_io fallback: a relative directory
	// capability or NOFOLLOW cannot be emulated by path-based utimes.
	if (descriptor != AT_FDCWD || flags != 0) return {ENOSYS};
	struct ::timeval converted[2]{};
	if (int const error{legacy_microseconds(times, converted)}; error != 0) return {error};
	if (path_microseconds(path, converted) < 0)
		return {::fast_io::details::posix_nothrow_abi::last_error()};
	return {};
#else
	if (at_times(descriptor, path, times, flags) < 0) return {::fast_io::details::posix_nothrow_abi::last_error()};
	return {};
#endif
}
#elif defined(__linux__) && defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__BIONIC__) && \
      defined(__USE_ATFILE) && defined(__USE_XOPEN2K8) && \
      ((__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 6)) && (!defined(__USE_TIME_BITS64) || defined(__USE_TIME64_REDIRECTS) || defined(__TIMESIZE))
inline constexpr bool sdk_available{true};
// Old GNU SDKs advertise TIME_BITS64 only for a native32 redirect; new
// SDKs also advertise it for native64. The actual SDK TIMESIZE therefore
// prevents selecting a private time64 symbol on a native64 ABI.
#if defined(__USE_TIME64_REDIRECTS) || (defined(__USE_TIME_BITS64) && defined(__TIMESIZE) && __TIMESIZE == 32)
extern int fd_times(int, struct ::timespec const*) noexcept __asm__("__futimens64");
extern int at_times(int, char const*, struct ::timespec const*, int) noexcept __asm__("__utimensat64");
#else
extern int fd_times(int, struct ::timespec const*) noexcept __asm__("futimens");
extern int at_times(int, char const*, struct ::timespec const*, int) noexcept __asm__("utimensat");
#endif
inline ::fast_io::posix_operation_result checked_fd_times(int descriptor, struct ::timespec const (&times)[2]) noexcept
{
	if (fd_times(descriptor, times) < 0) return {::fast_io::details::posix_nothrow_abi::last_error()};
	return {};
}
inline ::fast_io::posix_operation_result checked_at_times(int descriptor, char const* path,
	struct ::timespec const (&times)[2], int flags) noexcept
{
	if (at_times(descriptor, path, times, flags) < 0) return {::fast_io::details::posix_nothrow_abi::last_error()};
	return {};
}
#else
inline constexpr bool sdk_available{false};
#endif

#if defined(__linux__)
template<typename Source>
concept signed_timestamp_source = requires(Source const& value)
{
	value.tv_sec;
	value.tv_nsec;
} && ::std::signed_integral<::std::remove_cvref_t<decltype(::std::declval<Source const&>().tv_sec)>> &&
	::std::signed_integral<::std::remove_cvref_t<decltype(::std::declval<Source const&>().tv_nsec)>>;

template<typename Source>
inline constexpr bool native_kernel_source_available{
#if defined(FAST_IO_POSIX_TIMESTAMP_HAS_KERNEL_TYPES) && defined(__NR_utimensat)
	[]() constexpr
	{
		if constexpr (signed_timestamp_source<Source>)
			return ::std::numeric_limits<::std::remove_cvref_t<decltype(::std::declval<Source const&>().tv_sec)>>::digits <=
				::std::numeric_limits<decltype(::__kernel_old_timespec::tv_sec)>::digits &&
				::std::numeric_limits<::std::remove_cvref_t<decltype(::std::declval<Source const&>().tv_nsec)>>::digits <=
				::std::numeric_limits<decltype(::__kernel_old_timespec::tv_nsec)>::digits;
		else return false;
	}()
#else
	false
#endif
};
template<typename Source>
inline constexpr bool wide_kernel_source_available{
#if defined(FAST_IO_POSIX_TIMESTAMP_HAS_KERNEL_TYPES) && defined(__NR_utimensat_time64)
	[]() constexpr
	{
		if constexpr (signed_timestamp_source<Source>)
			return ::std::numeric_limits<::std::remove_cvref_t<decltype(::std::declval<Source const&>().tv_sec)>>::digits <=
				::std::numeric_limits<decltype(::__kernel_timespec::tv_sec)>::digits &&
				::std::numeric_limits<::std::remove_cvref_t<decltype(::std::declval<Source const&>().tv_nsec)>>::digits <=
				::std::numeric_limits<decltype(::__kernel_timespec::tv_nsec)>::digits;
		else return false;
	}()
#else
	false
#endif
};
#endif
} // namespace fast_io::details::posix_timestamps_nothrow_abi

namespace fast_io
{
// These native SDK functions make one call and return its original POSIX
// error. No FD, flags, seconds or nanoseconds are prevalidated or retried.
// They deliberately do not substitute for Linux's explicit raw AT_EMPTY_PATH
// operation: glibc futimens/time64 fallback has different error precedence.
template<typename = void>
	requires details::posix_timestamps_nothrow_abi::sdk_available
[[nodiscard]] inline posix_operation_result posix_futimens_nothrow(posix_io_observer file,
	struct ::timespec const (&times)[2]) noexcept
{
#if (defined(__APPLE__) && defined(__MACH__) && defined(UTIME_NOW) && defined(UTIME_OMIT)) || (defined(__linux__) && defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__BIONIC__) && defined(__USE_ATFILE) && defined(__USE_XOPEN2K8) && ((__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 6)) && (!defined(__USE_TIME_BITS64) || defined(__USE_TIME64_REDIRECTS) || defined(__TIMESIZE)))
	return details::posix_timestamps_nothrow_abi::checked_fd_times(file.native_handle(), times);
#else
	return {ENOSYS}; // The constrained-out body is never a callable provider.
#endif
}
template<typename = void>
	requires details::posix_timestamps_nothrow_abi::sdk_available
[[nodiscard]] inline posix_operation_result posix_utimensat_nothrow(posix_at_entry directory,
	char const* validated_path, struct ::timespec const (&times)[2], int raw_flags = 0) noexcept
{
#if (defined(__APPLE__) && defined(__MACH__) && defined(UTIME_NOW) && defined(UTIME_OMIT)) || (defined(__linux__) && defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__BIONIC__) && defined(__USE_ATFILE) && defined(__USE_XOPEN2K8) && ((__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 6)) && (!defined(__USE_TIME_BITS64) || defined(__USE_TIME64_REDIRECTS) || defined(__TIMESIZE)))
	return details::posix_timestamps_nothrow_abi::checked_at_times(directory.fd, validated_path, times, raw_flags);
#else
	return {ENOSYS};
#endif
}

#if defined(__linux__)
// Source fields are logical signed values, never a borrowed guessed ABI.
// Construct the real target UAPI payload in owned storage; the width constraint
// proves each cast lossless before any syscall. No numeric range check can
// change FD/flags/nsec error precedence. Unknown/incomplete SDKs are unavailable.
template<typename Source>
	requires details::posix_timestamps_nothrow_abi::native_kernel_source_available<Source>
[[nodiscard]] inline posix_operation_result linux_utimensat_nothrow(posix_at_entry directory,
	char const* validated_path, Source const (&times)[2], int raw_flags) noexcept
{
#if defined(FAST_IO_POSIX_TIMESTAMP_HAS_KERNEL_TYPES) && defined(__NR_utimensat)
	struct ::__kernel_old_timespec owned[2]{};
	for (::std::size_t i{}; i != 2u; ++i)
	{
		// [two complete source elements] [two complete owned UAPI elements]
		// [safe                        ] [safe                          ] i<2.
		owned[i].tv_sec = static_cast<decltype(owned[i].tv_sec)>(times[i].tv_sec);
		owned[i].tv_nsec = static_cast<decltype(owned[i].tv_nsec)>(times[i].tv_nsec);
	}
	auto const result{::fast_io::system_call<__NR_utimensat, int>(directory.fd, validated_path, owned, raw_flags)};
	return {result < 0 ? -result : 0};
#else
	return {ENOSYS};
#endif
}
template<typename Source>
	requires details::posix_timestamps_nothrow_abi::wide_kernel_source_available<Source>
[[nodiscard]] inline posix_operation_result linux_utimensat_time64_nothrow(posix_at_entry directory,
	char const* validated_path, Source const (&times)[2], int raw_flags) noexcept
{
#if defined(FAST_IO_POSIX_TIMESTAMP_HAS_KERNEL_TYPES) && defined(__NR_utimensat_time64)
	struct ::__kernel_timespec owned[2]{};
	for (::std::size_t i{}; i != 2u; ++i)
	{
		// [two complete source elements] [two complete owned UAPI elements]
		// [safe                        ] [safe                          ] i<2.
		owned[i].tv_sec = static_cast<decltype(owned[i].tv_sec)>(times[i].tv_sec);
		owned[i].tv_nsec = static_cast<decltype(owned[i].tv_nsec)>(times[i].tv_nsec);
	}
	auto const result{::fast_io::system_call<__NR_utimensat_time64, int>(directory.fd, validated_path, owned, raw_flags)};
	return {result < 0 ? -result : 0};
#else
	return {ENOSYS};
#endif
}
#endif
} // namespace fast_io
#if defined(FAST_IO_POSIX_TIMESTAMP_HAS_KERNEL_TYPES)
#undef FAST_IO_POSIX_TIMESTAMP_HAS_KERNEL_TYPES
#endif
#endif

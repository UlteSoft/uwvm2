#pragma once

// Include after fast_io's POSIX provider and posix_nothrow.h. This primitive
// borrows a validated NUL-terminated native path and caller-owned output bytes;
// it returns ONE native readlinkat result, without allocating or appending NUL.
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#include <cstddef>
#include <cerrno>
#include <type_traits>
#include <unistd.h>
#include <fcntl.h>

namespace fast_io::details::posix_readlink_nothrow_abi
{
#if defined(__linux__) && defined(__NR_readlinkat)
inline constexpr bool available{true};
#elif defined(__APPLE__) && defined(__MACH__)
inline constexpr bool available{true};
// Both selected Darwin SDKs declare these functions without an asm redirection.
// This is a native ssize_t/size_t ABI, independent of FILE_OFFSET_BITS/time64.
extern ::ssize_t path_link(char const*, char*, ::std::size_t) noexcept __asm__("_readlink");
[[clang::availability(macos, introduced = 10.10), clang::availability(ios, introduced = 8.0)]]
extern ::ssize_t at_link(int, char const*, char*, ::std::size_t) noexcept __asm__("_readlinkat");
#elif defined(__linux__) && defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__BIONIC__) && \
      defined(__USE_ATFILE) && ((__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 4))
inline constexpr bool available{true};
using native_signature = ::ssize_t (*)(int, char const*, char*, ::std::size_t);
static_assert(::std::is_convertible_v<decltype(&::readlinkat), native_signature>);
extern ::ssize_t at_link(int, char const*, char*, ::std::size_t) noexcept __asm__("readlinkat");
#else
// Preserve an unqualified libc's existing caller fallback rather than claiming
// a symbol merely from pointer/offset widths. The public name still exists so
// a dependent requires-expression can choose that original fallback.
inline constexpr bool available{false};
#endif
}

namespace fast_io
{
template <typename = void>
requires(::fast_io::details::posix_readlink_nothrow_abi::available)
inline posix_read_result posix_readlinkat_nothrow(posix_at_entry directory, char const* path,
                                                char* buffer, ::std::size_t count) noexcept
{
	// path: [validated caller-owned native characters ... NUL] end
	//       [safe] no pointer advance or retained pathname occurs here.
	// buffer: [caller-owned writable bytes, complete extent count] end
	//         [safe] the native call alone may write that proved extent.
	// No count/path/fd prevalidation changes the native error precedence.
	// transferred==count can mean truncation; this API never treats it as EOF,
	// adds NUL, retries, stats the path, follows the link or assumes immutability.
#if defined(__linux__) && defined(__NR_readlinkat)
	auto const transferred{::fast_io::system_call<__NR_readlinkat, ::ssize_t>(directory.fd, path, buffer, count)};
	if (::fast_io::linux_system_call_fails(transferred)) return {0, static_cast<int>(-transferred)};
#elif defined(__APPLE__) && defined(__MACH__)
	::ssize_t transferred;
#if FAST_IO_HAS_BUILTIN(__builtin_available)
	if (__builtin_available(macOS 10.10, iOS 8.0, *)) [[likely]]
	{
		transferred = ::fast_io::details::posix_readlink_nothrow_abi::at_link(directory.fd, path, buffer, count);
	}
	else
	{
		// Match the existing older-Darwin readlinkat fallback. A relative
		// directory capability cannot be implemented by a path-based operation.
		if (directory.fd != AT_FDCWD) return {0, ENOSYS};
		transferred = ::fast_io::details::posix_readlink_nothrow_abi::path_link(path, buffer, count);
	}
#else
	transferred = ::fast_io::details::posix_readlink_nothrow_abi::at_link(directory.fd, path, buffer, count);
#endif
	if (transferred < 0) return {0, ::fast_io::details::posix_nothrow_abi::last_error()};
#elif defined(__linux__) && defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__BIONIC__) && \
      defined(__USE_ATFILE) && ((__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 4))
	auto const transferred{::fast_io::details::posix_readlink_nothrow_abi::at_link(directory.fd, path, buffer, count)};
	if (transferred < 0) return {0, ::fast_io::details::posix_nothrow_abi::last_error()};
#else
	// The constraint rejects this branch; it cannot replace a caller fallback.
	(void)directory; (void)path; (void)buffer; (void)count;
	return {0, ENOSYS};
#endif
#if (defined(__linux__) && (defined(__NR_readlinkat) || (defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__BIONIC__) && defined(__USE_ATFILE) && ((__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 4))))) || (defined(__APPLE__) && defined(__MACH__))
	if (static_cast<::std::size_t>(transferred) > count) return {0, EIO};
	return {static_cast<::std::size_t>(transferred), 0};
#endif
}
}
#endif

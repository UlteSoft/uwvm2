#pragma once
// An independent native SDK oracle, not the production error normalizer.
#include <cstddef>
#include <cerrno>
#include <type_traits>
#include <unistd.h>
#include <fcntl.h>
#if defined(__linux__)
#include <sys/syscall.h>
#if !defined(__NR_readlinkat) || !defined(__USE_MISC) || !defined(__GLIBC__) || defined(__UCLIBC__) || defined(__BIONIC__)
#error This fixture needs an actual GNU Linux readlinkat/syscall SDK target.
#endif
#elif !(defined(__APPLE__) && defined(__MACH__))
#error This fixture does not qualify unknown readlinkat SDK ABIs.
#endif
namespace readlink_sdk_oracle
{
struct result { ::std::size_t transferred; int error; };
#if defined(__linux__)
static_assert(noexcept(::syscall(static_cast<long>(__NR_readlinkat), 0, static_cast<char const*>(nullptr), static_cast<char*>(nullptr), ::std::size_t{})));
#elif defined(__APPLE__) && defined(__MACH__)
using signature = ::ssize_t (*)(int, char const*, char*, ::std::size_t);
static_assert(::std::is_convertible_v<decltype(&::readlinkat), signature>);
[[clang::availability(macos, introduced = 10.10), clang::availability(ios, introduced = 8.0)]]
extern ::ssize_t sdk_readlinkat(int, char const*, char*, ::std::size_t) noexcept __asm__("_readlinkat");
#endif
inline result invoke(int descriptor, char const* path, char* buffer, ::std::size_t extent) noexcept
{
#if defined(__linux__)
	long const bytes{::syscall(static_cast<long>(__NR_readlinkat), descriptor, path, buffer, extent)};
#else
	::ssize_t bytes;
#if defined(__clang__)
	if (__builtin_available(macOS 10.10, iOS 8.0, *))
	{
		bytes = sdk_readlinkat(descriptor, path, buffer, extent);
	}
	else return {0, ENOSYS};
#else
	bytes = sdk_readlinkat(descriptor, path, buffer, extent);
#endif
#endif
	if (bytes == -1) { int const native_error{errno}; return {0, native_error}; }
	return {static_cast<::std::size_t>(bytes), 0};
}
}

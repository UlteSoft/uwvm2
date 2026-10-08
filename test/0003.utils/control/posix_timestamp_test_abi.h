#pragma once
#include <cerrno>
#include <sys/stat.h>
#if defined(__linux__)
#include <sys/syscall.h>
#if __has_include(<linux/time_types.h>)
#include <linux/time_types.h>
#define UWVM_TIMESTAMP_ORACLE_HAS_LINUX_UAPI 1
#if __has_include(<linux/stat.h>)
#include <linux/stat.h>
#define UWVM_TIMESTAMP_ORACLE_HAS_LINUX_STATX 1
#endif
#endif
#endif
namespace timestamp_test::abi
{
#if defined(__APPLE__) && defined(__MACH__)
[[clang::availability(macos, introduced = 10.13), clang::availability(ios, introduced = 11.0),
  clang::availability(tvos, introduced = 11.0), clang::availability(watchos, introduced = 4.0)]]
extern "C" int native_futimens(int, struct ::timespec const*) noexcept __asm__("_futimens");
[[clang::availability(macos, introduced = 10.13), clang::availability(ios, introduced = 11.0),
  clang::availability(tvos, introduced = 11.0), clang::availability(watchos, introduced = 4.0)]]
extern "C" int native_utimensat(int, char const*, struct ::timespec const*, int) noexcept __asm__("_utimensat");
#elif defined(__linux__) && defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__BIONIC__) && defined(__USE_ATFILE) && defined(__USE_XOPEN2K8) && ((__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 6))
// Use the actual SDK declaration as an independent oracle: __REDIRECT_NTH
// owns both the native symbol and the noexcept contract. Do not repeat the
// production provider's TIME_BITS64/REDIRECTS decision in the test oracle.
inline int native_futimens(int descriptor, struct ::timespec const* times) noexcept
{
    static_assert(noexcept(::futimens(descriptor, times)));
    return ::futimens(descriptor, times);
}
inline int native_utimensat(int descriptor, char const* path, struct ::timespec const* times, int flags) noexcept
{
    static_assert(noexcept(::utimensat(descriptor, path, times, flags)));
    return ::utimensat(descriptor, path, times, flags);
}
// Independent libc syscall bridge; its actual target SDK owns register packing
// and errno conversion. Payloads below are real UAPI tags, not product layouts.
extern "C" long native_syscall(long, ...) noexcept __asm__("syscall");
#endif
} // namespace timestamp_test::abi

#pragma once
#include <cstddef>
#include <type_traits>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>

#if !defined(__linux__)
# error "This oracle requires the real Linux SDK"
#endif
// Exactly the current fast_io system_call.h selector. Never force generic.h
// into an x64/direct-backend process and publish that as i386/PPC acceptance.
#if (defined(__x86_64__) && !(defined(__arm64ec__) || defined(_M_ARM64EC))) || \
    defined(__arm64__) || defined(__aarch64__) || \
    (defined(__riscv) && __SIZEOF_SIZE_T__ == 8) || \
    (defined(__loongarch__) && __SIZEOF_SIZE_T__ == 8) || defined(__s390x__)
# error "This target selects a direct backend, not the actual generic SDK bridge"
#endif
#if !defined(__GLIBC__) || defined(__UCLIBC__) || defined(__BIONIC__) || !defined(__USE_MISC)
# error "This independent oracle needs the bound genuine GNU SDK; other libcs remain pending"
#endif
namespace linux_generic_test::abi
{
using sdk_signature = long (*)(long, ...);
static_assert(::std::is_convertible_v<decltype(&::syscall), sdk_signature>);
template<typename... Args>
inline long syscall(long number, Args... args) noexcept
{
    // The actual SDK declaration, including its native C symbol and __THROW,
    // is the independent oracle. No private production alias participates.
    static_assert(noexcept(::syscall(number, args...)));
    return ::syscall(number, args...);
}
}

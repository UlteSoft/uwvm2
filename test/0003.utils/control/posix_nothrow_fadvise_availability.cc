#ifndef _LARGEFILE64_SOURCE
# define _LARGEFILE64_SOURCE 1
#endif
#include <fcntl.h>
#include <fast_io.h>

#if !defined(__linux__)
# error "This source fixture describes the actual Linux SDK only"
#endif

template<typename Observer>
concept public_advisory_available = requires(Observer observer, ::fast_io::intfpos_t offset)
{
    { ::fast_io::posix_fadvise_nothrow(observer, offset, offset, int{}) } noexcept;
};

#if defined(__UCLIBC__)
using sdk_offset = ::off_t;
#elif defined(__GLIBC__) || defined(__BIONIC__)
using sdk_offset = ::off64_t;
#else
using sdk_offset = ::off_t;
#endif
#if defined(__GLIBC__) && !defined(__UCLIBC__) && !defined(__BIONIC__) && \
    (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 4))
inline constexpr bool sdk_symbol_available = true;
#elif defined(__BIONIC__) && defined(__ANDROID_API__) && __ANDROID_API__ >= 21
inline constexpr bool sdk_symbol_available = true;
#else
inline constexpr bool sdk_symbol_available = false;
#endif
inline constexpr bool sdk_wide{sdk_symbol_available && ::std::numeric_limits<sdk_offset>::is_signed &&
    ::std::numeric_limits<sdk_offset>::digits >= ::std::numeric_limits<::fast_io::intfpos_t>::digits};
static_assert(public_advisory_available<::fast_io::posix_io_observer> == sdk_wide);

int main()
{
    // No success claim about a dynamic libc symbol or native filesystem. This
    // fixture checks only public overload viability against the actual SDK;
    // the independent real kernel tests remain required for acceptance.
    ::fast_io::io::println("PASS public Linux advisory SDK availability contract: ", sdk_wide);
}

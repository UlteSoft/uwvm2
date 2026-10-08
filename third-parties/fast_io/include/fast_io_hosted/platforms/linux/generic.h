#pragma once

#include <cstddef>
#include <cstdint>
#include <concepts>
#include <type_traits>
#include <cerrno>
#include <unistd.h>
#include <sys/syscall.h>

#if defined(__linux__)
namespace fast_io
{
namespace details::linux_generic_syscall_abi
{
// The actual Linux SDK owns the variadic signature and native long width.
// Accept its C++ noexcept and potentially-throwing declarations equally;
// exception specification is not evidence of a different C ABI.
// A macro-redirected or differently typed SDK needs its own symbol binding.
#if defined(syscall)
#error "fast_io generic syscall needs the actual unredirected Linux SDK syscall declaration"
#endif
using native_signature = long (*)(long, ...);
static_assert(::std::is_convertible_v<decltype(&::syscall), native_signature>,
    "fast_io generic syscall needs the actual Linux SDK long syscall(long, ...) ABI");
extern long sdk_syscall(long, ...) noexcept __asm__("syscall");
}

template <::std::size_t syscall_number, ::std::signed_integral return_value_type, typename... Args>
	requires(::std::is_trivially_copyable_v<Args> && ...)
inline return_value_type system_call(Args... args) noexcept
{
	// [safe one complete native SDK thread-local errno int cell]
	//       ^ borrow the SDK lvalue once; no pointer advances or escapes.
	int* const error_cell{__builtin_addressof(errno)};
	int const interrupted_error{*error_cell};
	long const result{details::linux_generic_syscall_abi::sdk_syscall(static_cast<long>(syscall_number), args...)};
	if (result == -1)
	{
		// libc returns -1, not negative errno. Capture its real error BEFORE
		// restoring the caller's cell. No retry, allocation or exception path.
		int const native_error{*error_cell};
		*error_cell = interrupted_error;
		// A failure with errno==0 violates the SDK contract. Fail closed with
		// EIO instead of returning zero as if the failed operation succeeded.
		return static_cast<return_value_type>(-static_cast<long>(native_error == 0 ? EIO : native_error));
	}
	*error_cell = interrupted_error;
	// Preserve the complete successful native long value; the caller's
	// existing return_value_type conversion is unchanged.
	return static_cast<return_value_type>(result);
}

template <::std::uint_least64_t syscall_number, ::std::signed_integral return_value_type>
	requires(1 < sizeof(return_value_type))
[[__gnu__::__always_inline__]]
inline return_value_type inline_syscall(auto, auto) noexcept
{
	static_assert(false, "force inline for generic syscall is not supported");
}

template <::std::size_t syscall_number>
[[noreturn]]
inline void system_call_no_return(auto p1) noexcept
{
	details::linux_generic_syscall_abi::sdk_syscall(static_cast<long>(syscall_number), p1);
	// A denied or otherwise unexpectedly returning exit-like syscall must
	// terminate on this cold path rather than reach undefined behavior.
	__builtin_trap();
}

template <::std::integral I>
[[noreturn]]
inline void fast_exit(I ret) noexcept
{
	system_call_no_return<__NR_exit>(ret);
}
} // namespace fast_io
#endif // __linux__

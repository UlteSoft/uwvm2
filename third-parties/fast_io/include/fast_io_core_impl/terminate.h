#pragma once
#if defined(_MSC_VER) && !defined(__clang__)
#include <cstdlib>
#endif

namespace fast_io
{

#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[noreturn]] inline void
fast_terminate() noexcept
{
// https://llvm.org/doxygen/Compiler_8h_source.html
#if defined(__loongarch__) && (defined(__GNUC__) || defined(__clang__))
	// Clang may lower __builtin_trap to an atomic store at address zero on
	// LoongArch. Use the ISA's breakpoint so intentional termination does
	// not masquerade as an invalid memory access on Linux.
	__asm__ __volatile__("break 0");
	__builtin_unreachable();
#elif FAST_IO_HAS_BUILTIN(__builtin_trap)
	__builtin_trap();
#elif FAST_IO_HAS_BUILTIN(__builtin_abort)
	__builtin_abort();
#else
	::std::abort();
#endif
}

#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
[[noreturn]] inline void
unreachable() noexcept
{
#if defined(_MSC_VER) && !defined(__clang__) // MSVC
	__assume(false);
#else // GCC, Clang
	__builtin_unreachable();
#endif
}

} // namespace fast_io

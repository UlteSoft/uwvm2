#pragma once

#include <cstddef>
#include <cstdint>
#include <concepts>

namespace fast_io
{

template <::std::uint_least64_t syscall_number, ::std::signed_integral return_value_type>
	requires(1 < sizeof(return_value_type))
inline return_value_type system_call() noexcept
{
	register ::std::intptr_t r1 __asm__("r1") = static_cast<::std::intptr_t>(syscall_number);
	register ::std::intptr_t r2 __asm__("r2");
	__asm__ __volatile__("svc 0" : "=r"(r2) : "r"(r1) : "memory");
	return static_cast<return_value_type>(r2);
}

template <::std::uint_least64_t syscall_number, ::std::signed_integral return_value_type>
	requires(1 < sizeof(return_value_type))
inline return_value_type system_call(auto p1) noexcept
{
	register ::std::intptr_t r1 __asm__("r1") = static_cast<::std::intptr_t>(syscall_number);
	register ::std::intptr_t r2 __asm__("r2") = (::std::intptr_t)p1;
	__asm__ __volatile__("svc 0" : "+r"(r2) : "r"(r1) : "memory");
	return static_cast<return_value_type>(r2);
}

template <::std::uint_least64_t syscall_number>
[[noreturn]]
inline void system_call_no_return(auto p1) noexcept
{
	register ::std::intptr_t r1 __asm__("r1") = static_cast<::std::intptr_t>(syscall_number);
	register ::std::intptr_t r2 __asm__("r2") = (::std::intptr_t)p1;
	__asm__ __volatile__("svc 0" : "+r"(r2) : "r"(r1) : "memory");
	__builtin_unreachable();
}

template <::std::uint_least64_t syscall_number, ::std::signed_integral return_value_type>
	requires(1 < sizeof(return_value_type))
inline return_value_type system_call(auto p1, auto p2) noexcept
{
	register ::std::intptr_t r1 __asm__("r1") = static_cast<::std::intptr_t>(syscall_number);
	register ::std::intptr_t r2 __asm__("r2") = (::std::intptr_t)p1;
	register ::std::intptr_t r3 __asm__("r3") = (::std::intptr_t)p2;
	__asm__ __volatile__("svc 0" : "+r"(r2) : "r"(r1), "r"(r3) : "memory");
	return static_cast<return_value_type>(r2);
}

template <::std::uint_least64_t syscall_number, ::std::signed_integral return_value_type>
	requires(1 < sizeof(return_value_type))
[[__gnu__::__always_inline__]]
inline return_value_type inline_syscall(auto p1, auto p2) noexcept
{
	register ::std::intptr_t r1 __asm__("r1") = static_cast<::std::intptr_t>(syscall_number);
	register ::std::intptr_t r2 __asm__("r2") = (::std::intptr_t)p1;
	register ::std::intptr_t r3 __asm__("r3") = (::std::intptr_t)p2;
	__asm__ __volatile__("svc 0" : "+r"(r2) : "r"(r1), "r"(r3) : "memory");
	return static_cast<return_value_type>(r2);
}

template <::std::uint_least64_t syscall_number, ::std::signed_integral return_value_type>
	requires(1 < sizeof(return_value_type))
inline return_value_type system_call(auto p1, auto p2, auto p3) noexcept
{
	register ::std::intptr_t r1 __asm__("r1") = static_cast<::std::intptr_t>(syscall_number);
	register ::std::intptr_t r2 __asm__("r2") = (::std::intptr_t)p1;
	register ::std::intptr_t r3 __asm__("r3") = (::std::intptr_t)p2;
	register ::std::intptr_t r4 __asm__("r4") = (::std::intptr_t)p3;
	__asm__ __volatile__("svc 0" : "+r"(r2) : "r"(r1), "r"(r3), "r"(r4) : "memory");
	return static_cast<return_value_type>(r2);
}

template <::std::uint_least64_t syscall_number, ::std::signed_integral return_value_type>
	requires(1 < sizeof(return_value_type))
inline return_value_type system_call(auto p1, auto p2, auto p3, auto p4) noexcept
{
	register ::std::intptr_t r1 __asm__("r1") = static_cast<::std::intptr_t>(syscall_number);
	register ::std::intptr_t r2 __asm__("r2") = (::std::intptr_t)p1;
	register ::std::intptr_t r3 __asm__("r3") = (::std::intptr_t)p2;
	register ::std::intptr_t r4 __asm__("r4") = (::std::intptr_t)p3;
	register ::std::intptr_t r5 __asm__("r5") = (::std::intptr_t)p4;
	__asm__ __volatile__("svc 0" : "+r"(r2) : "r"(r1), "r"(r3), "r"(r4), "r"(r5) : "memory");
	return static_cast<return_value_type>(r2);
}

template <::std::uint_least64_t syscall_number, ::std::signed_integral return_value_type>
	requires(1 < sizeof(return_value_type))
inline return_value_type system_call(auto p1, auto p2, auto p3, auto p4, auto p5) noexcept
{
	register ::std::intptr_t r1 __asm__("r1") = static_cast<::std::intptr_t>(syscall_number);
	register ::std::intptr_t r2 __asm__("r2") = (::std::intptr_t)p1;
	register ::std::intptr_t r3 __asm__("r3") = (::std::intptr_t)p2;
	register ::std::intptr_t r4 __asm__("r4") = (::std::intptr_t)p3;
	register ::std::intptr_t r5 __asm__("r5") = (::std::intptr_t)p4;
	register ::std::intptr_t r6 __asm__("r6") = (::std::intptr_t)p5;
	__asm__ __volatile__("svc 0" : "+r"(r2) : "r"(r1), "r"(r3), "r"(r4), "r"(r5), "r"(r6) : "memory");
	return static_cast<return_value_type>(r2);
}

template <::std::uint_least64_t syscall_number, ::std::signed_integral return_value_type>
	requires(1 < sizeof(return_value_type))
inline return_value_type system_call(auto p1, auto p2, auto p3, auto p4, auto p5, auto p6) noexcept
{
	register ::std::intptr_t r1 __asm__("r1") = static_cast<::std::intptr_t>(syscall_number);
	register ::std::intptr_t r2 __asm__("r2") = (::std::intptr_t)p1;
	register ::std::intptr_t r3 __asm__("r3") = (::std::intptr_t)p2;
	register ::std::intptr_t r4 __asm__("r4") = (::std::intptr_t)p3;
	register ::std::intptr_t r5 __asm__("r5") = (::std::intptr_t)p4;
	register ::std::intptr_t r6 __asm__("r6") = (::std::intptr_t)p5;
	register ::std::intptr_t r7 __asm__("r7") = (::std::intptr_t)p6;
	__asm__ __volatile__("svc 0"
						 : "+r"(r2)
						 : "r"(r1), "r"(r3), "r"(r4), "r"(r5), "r"(r6), "r"(r7)
						 : "memory");
	return static_cast<return_value_type>(r2);
}

template <::std::integral I>
[[noreturn]]
inline void fast_exit(I ret) noexcept
{
	system_call_no_return<__NR_exit>(ret);
}

} // namespace fast_io

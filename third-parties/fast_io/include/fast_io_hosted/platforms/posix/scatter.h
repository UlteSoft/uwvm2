#pragma once

namespace fast_io
{

namespace details
{

#if defined(__linux__)
/// Handles a failed writev outside the successful synchronous scatter path.
/// EINTR and every other kernel error retain the existing POSIX exception/termination semantics; only their placement
/// changes, so a static-fragment caller keeps one compare and an unlikely edge beside its inlined first syscall.
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
[[noreturn]] inline void posix_scatter_write_error(::std::ptrdiff_t result)
{
	::fast_io::throw_posix_error(static_cast<int>(-result));
}
#elif defined(__wasi__)
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
[[noreturn]] inline void posix_scatter_write_error(int error)
{
	::fast_io::throw_posix_error(error);
}
#else
#if __has_cpp_attribute(__gnu__::__cold__)
[[__gnu__::__cold__]]
#endif
#if __has_cpp_attribute(__gnu__::__noinline__)
[[__gnu__::__noinline__]]
#endif
[[noreturn]] inline void posix_scatter_write_error()
{
	::fast_io::throw_posix_error();
}
#endif

inline ::fast_io::io_scatter_status_t posix_scatter_read_bytes_impl(int fd, ::fast_io::io_scatter_t const *pscatter,
																		::std::size_t n)
{
	// Linux's raw readv syscall is not exempt from the POSIX vector-count limit: the kernel rejects iovcnt greater than
	// UIO_MAXIOV (1024) with EINVAL. Clamp at this trust boundary even though raw Linux and WASI wrappers accept size_t;
	// libc implementations additionally expose an int iovcnt, for which the same bound makes conversion lossless.
	// This function implements a "some" operation, so its status intentionally describes only the admitted prefix.
	// The generic read-all loop advances by that status and invokes us again for the unconsumed descriptors.
	// scatter_size_to_status subtracts every completed zero length even when readv returns zero: an all-empty admitted
	// prefix therefore becomes {n,0}, while {0,0} is reserved for zero bytes before the first positive-length entry.
	n = ::std::min(n, ::fast_io::details::posix_scatter_maximum_count);
#if defined(__linux__) && defined(__NR_readv)
	auto ret{system_call<__NR_readv, ::std::ptrdiff_t>(fd, pscatter, n)};
	::fast_io::linux_system_call_throw_error(ret);
#elif defined(__wasi__)
	using iovec_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= __wasi_iovec_t const *;
	::std::size_t ret;
	auto val{noexcept_call(::__wasi_fd_read, fd, reinterpret_cast<iovec_may_alias_const_ptr>(pscatter), n,
						   __builtin_addressof(ret))};
	if (val)
	{
		::fast_io::throw_posix_error(val);
	}
#else
	using iovec_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= struct iovec const *;

	auto ret{::fast_io::noexcept_call(::readv, fd, reinterpret_cast<iovec_may_alias_const_ptr>(pscatter),
									 static_cast<int>(n))};
	if (ret == -1)
	{
		::fast_io::throw_posix_error();
	}
#endif
	return scatter_size_to_status(static_cast<::std::size_t>(ret), pscatter, n);
}

inline ::fast_io::io_scatter_status_t
posix_static_scatter_write_bytes_first_impl(int fd,
	::fast_io::io_scatter_t const *pscatter, ::std::size_t n)
{
	// The generic scatter layer normally admits only a legal prefix, but this is the final syscall boundary and must
	// remain safe when reached through a lower-level adapter or a future direct call. Clamp again before any ABI
	// conversion of iovcnt: it prevents kernel-limit violations (typically EINVAL) and narrowing surprises on POSIX
	// interfaces whose public iovcnt type is int. The returned some-status is consequently relative to this prefix.
	n = ::std::min(n, ::fast_io::details::posix_scatter_maximum_count);
#if defined(__linux__) && defined(__NR_writev)
	auto ret{system_call<__NR_writev, ::std::ptrdiff_t>(fd, pscatter, n)};
	if (::fast_io::linux_system_call_fails(ret)) [[unlikely]]
	{
		::fast_io::details::posix_scatter_write_error(ret);
	}
#elif defined(__wasi__)
	using iovec_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= __wasi_ciovec_t const *;
	::std::size_t ret;
	auto val{noexcept_call(::__wasi_fd_write, fd, reinterpret_cast<iovec_may_alias_const_ptr>(pscatter), n,
						   __builtin_addressof(ret))};
	if (val)
	{
		::fast_io::details::posix_scatter_write_error(val);
	}
#else
	using iovec_may_alias_const_ptr
#if __has_cpp_attribute(__gnu__::__may_alias__)
		[[__gnu__::__may_alias__]]
#endif
		= struct iovec const *;

	auto ret{::fast_io::noexcept_call(::writev, fd, reinterpret_cast<iovec_may_alias_const_ptr>(pscatter),
									 static_cast<int>(n))};
	if (ret == -1)
	{
		::fast_io::details::posix_scatter_write_error();
	}
#endif
	return scatter_size_to_status(static_cast<::std::size_t>(ret), pscatter, n);
}

/// Ordinary POSIX scatter entry.  The syscall/error helper above is shared with
/// the print-level static-scatter path, while this wrapper remains an ordinary
/// inline function so callers follow the compiler's normal text-size heuristic.
inline ::fast_io::io_scatter_status_t
posix_scatter_write_bytes_impl(int fd,
	::fast_io::io_scatter_t const *pscatter, ::std::size_t n)
{
	return ::fast_io::details::posix_static_scatter_write_bytes_first_impl(
		fd, pscatter, n);
}

} // namespace details

template <::std::integral char_type>
inline ::fast_io::io_scatter_status_t
scatter_read_some_bytes_underflow_define(::fast_io::basic_posix_io_observer<char_type> piob,
										 ::fast_io::io_scatter_t const *pscatters, ::std::size_t n)
{
	return ::fast_io::details::posix_scatter_read_bytes_impl(piob.fd, pscatters, n);
}

template <::std::integral char_type>
inline ::fast_io::io_scatter_status_t
scatter_write_some_bytes_overflow_define(::fast_io::basic_posix_io_observer<char_type> piob,
										 ::fast_io::io_scatter_t const *pscatters, ::std::size_t n)
{
	return ::fast_io::details::posix_scatter_write_bytes_impl(piob.fd, pscatters, n);
}

/// Dedicated first-attempt entry for a proven synchronous static-fragment plan.
/// It remains separate from the public scatter CPO so the print dispatcher can
/// select this protocol without changing ordinary/runtime scatter semantics.
template <::std::integral char_type>
inline ::fast_io::io_scatter_status_t
print_static_scatter_write_some_bytes_overflow_define(
	::fast_io::basic_posix_io_observer<char_type> piob,
	::fast_io::io_scatter_t const *pscatters, ::std::size_t n)
{
	return ::fast_io::details::posix_static_scatter_write_bytes_first_impl(
		piob.fd, pscatters, n);
}

template <::std::integral char_type>
inline constexpr ::std::true_type print_synchronous_direct_scalar_output(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::basic_posix_io_observer<char_type>>) noexcept
{
	// A successful write copies the admitted scalar range before the syscall
	// returns; a partial write is completed synchronously by the core all-write
	// loop. The observer has no put area and never retains the source pointer.
	return {};
}

template <::std::integral char_type>
inline constexpr ::std::true_type print_synchronous_direct_scatter_output(
	::fast_io::io_reserve_type_t<char_type,
		::fast_io::basic_posix_io_observer<char_type>>) noexcept
{
	// readv/writev (and the corresponding raw syscalls) consume every admitted
	// iovec during this synchronous operation.  The observer has no put area and
	// the implementation neither transforms nor retains the payload pointers.
	return {};
}

} // namespace fast_io

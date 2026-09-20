#pragma once

/*
 * Normalized-observer entry points for primitive output (`operations::decay`).
 *
 * These functions expose the full contiguous/scatter, typed/byte, sequential/
 * positioned, some/all matrix after `output_stream_ref` has already selected an
 * owned or borrowed observer representation. Public pointer/span/range wrappers
 * sit above; raw provider `_define` CPOs sit below.
 */

namespace fast_io::operations::decay
{

template <typename outstmtype>
using write_decay_stream_t = ::std::remove_cvref_t<outstmtype>;

/*
 * The unsuffixed primitive is an explicit value-decay boundary, matching the
 * established print owner: a normalized prvalue retains its target ABI class
 * and owns its lifetime for the complete call. `_borrowed` instead preserves an
 * already-owned observer's exact identity, which is required for move-only
 * references and for small cursor objects whose mutable state lives inline.
 *
 * A named lvalue reaches `_dispatch`. This mandatory-inline policy bridge calls
 * the value primitive only after the observer author supplies the ADL semantic
 * substitution proof and the target ABI admits the small trivial argument.
 * Otherwise it calls the separately named borrowed primitive. Therefore neither
 * object size nor trivial special members can silently turn cursor identity into
 * copy semantics, while an outlined safe specialization still receives values
 * in registers.
 */

template <typename outstmtype>
inline constexpr typename write_decay_stream_t<outstmtype>::output_char_type const *
write_some_decay_borrowed(outstmtype &outsm,
						  typename write_decay_stream_t<outstmtype>::output_char_type const *first,
						  typename write_decay_stream_t<outstmtype>::output_char_type const *last)
{
	return ::fast_io::details::write_some_impl(outsm, first, last);
}

template <typename outstmtype>
inline constexpr typename write_decay_stream_t<outstmtype>::output_char_type const *
write_some_decay(outstmtype outsm,
				 typename write_decay_stream_t<outstmtype>::output_char_type const *first,
				 typename write_decay_stream_t<outstmtype>::output_char_type const *last)
{
	return ::fast_io::operations::decay::write_some_decay_borrowed(outsm, first, last);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr typename write_decay_stream_t<outstmtype>::output_char_type const *
write_some_decay_dispatch(outstmtype &outsm,
						  typename write_decay_stream_t<outstmtype>::output_char_type const *first,
						  typename write_decay_stream_t<outstmtype>::output_char_type const *last)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		return ::fast_io::operations::decay::write_some_decay(outsm, first, last);
	}
	else
	{
		return ::fast_io::operations::decay::write_some_decay_borrowed(outsm, first, last);
	}
}

template <typename outstmtype>
inline constexpr void write_all_decay_borrowed(
	outstmtype &outsm, typename write_decay_stream_t<outstmtype>::output_char_type const *first,
	typename write_decay_stream_t<outstmtype>::output_char_type const *last)
{
	::fast_io::details::write_all_impl(outsm, first, last);
}

template <typename outstmtype>
inline constexpr void write_all_decay(
	outstmtype outsm, typename write_decay_stream_t<outstmtype>::output_char_type const *first,
	typename write_decay_stream_t<outstmtype>::output_char_type const *last)
{
	::fast_io::operations::decay::write_all_decay_borrowed(outsm, first, last);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void write_all_decay_dispatch(
	outstmtype &outsm, typename write_decay_stream_t<outstmtype>::output_char_type const *first,
	typename write_decay_stream_t<outstmtype>::output_char_type const *last)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		::fast_io::operations::decay::write_all_decay(outsm, first, last);
	}
	else
	{
		::fast_io::operations::decay::write_all_decay_borrowed(outsm, first, last);
	}
}

template <typename outstmtype>
inline constexpr ::std::byte const *write_some_bytes_decay_borrowed(
	outstmtype &outsm, ::std::byte const *first, ::std::byte const *last)
{
	return ::fast_io::details::write_some_bytes_impl(outsm, first, last);
}

template <typename outstmtype>
inline constexpr ::std::byte const *write_some_bytes_decay(
	outstmtype outsm, ::std::byte const *first, ::std::byte const *last)
{
	return ::fast_io::operations::decay::write_some_bytes_decay_borrowed(outsm, first, last);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::byte const *write_some_bytes_decay_dispatch(
	outstmtype &outsm, ::std::byte const *first, ::std::byte const *last)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		return ::fast_io::operations::decay::write_some_bytes_decay(outsm, first, last);
	}
	else
	{
		return ::fast_io::operations::decay::write_some_bytes_decay_borrowed(outsm, first, last);
	}
}

template <typename outstmtype>
inline constexpr void write_all_bytes_decay_borrowed(
	outstmtype &outsm, ::std::byte const *first, ::std::byte const *last)
{
	::fast_io::details::write_all_bytes_impl(outsm, first, last);
}

template <typename outstmtype>
inline constexpr void write_all_bytes_decay(
	outstmtype outsm, ::std::byte const *first, ::std::byte const *last)
{
	::fast_io::operations::decay::write_all_bytes_decay_borrowed(outsm, first, last);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void write_all_bytes_decay_dispatch(
	outstmtype &outsm, ::std::byte const *first, ::std::byte const *last)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		::fast_io::operations::decay::write_all_bytes_decay(outsm, first, last);
	}
	else
	{
		::fast_io::operations::decay::write_all_bytes_decay_borrowed(outsm, first, last);
	}
}

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_write_some_decay_borrowed(
	outstmtype &outsm,
	basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
	::std::size_t n)
{
	return ::fast_io::details::scatter_write_some_impl(outsm, pscatters, n);
}

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_write_some_decay(
	outstmtype outsm,
	basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
	::std::size_t n)
{
	return ::fast_io::operations::decay::scatter_write_some_decay_borrowed(outsm, pscatters, n);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr io_scatter_status_t scatter_write_some_decay_dispatch(
	outstmtype &outsm,
	basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
	::std::size_t n)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		return ::fast_io::operations::decay::scatter_write_some_decay(outsm, pscatters, n);
	}
	else
	{
		return ::fast_io::operations::decay::scatter_write_some_decay_borrowed(outsm, pscatters, n);
	}
}

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_write_some_bytes_decay_borrowed(
	outstmtype &outsm, io_scatter_t const *pscatters, ::std::size_t n)
{
	return ::fast_io::details::scatter_write_some_bytes_impl(outsm, pscatters, n);
}

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_write_some_bytes_decay(
	outstmtype outsm, io_scatter_t const *pscatters, ::std::size_t n)
{
	return ::fast_io::operations::decay::scatter_write_some_bytes_decay_borrowed(outsm, pscatters, n);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr io_scatter_status_t scatter_write_some_bytes_decay_dispatch(
	outstmtype &outsm, io_scatter_t const *pscatters, ::std::size_t n)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		return ::fast_io::operations::decay::scatter_write_some_bytes_decay(outsm, pscatters, n);
	}
	else
	{
		return ::fast_io::operations::decay::scatter_write_some_bytes_decay_borrowed(outsm, pscatters, n);
	}
}

template <typename outstmtype>
inline constexpr void scatter_write_all_decay_borrowed(
	outstmtype &outsm,
	basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
	::std::size_t n)
{
	::fast_io::details::scatter_write_all_impl(outsm, pscatters, n);
}

template <typename outstmtype>
inline constexpr void scatter_write_all_decay(
	outstmtype outsm,
	basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
	::std::size_t n)
{
	::fast_io::operations::decay::scatter_write_all_decay_borrowed(outsm, pscatters, n);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void scatter_write_all_decay_dispatch(
	outstmtype &outsm,
	basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
	::std::size_t n)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		::fast_io::operations::decay::scatter_write_all_decay(outsm, pscatters, n);
	}
	else
	{
		::fast_io::operations::decay::scatter_write_all_decay_borrowed(outsm, pscatters, n);
	}
}

template <typename outstmtype>
inline constexpr void scatter_write_all_bytes_decay_borrowed(
	outstmtype &outsm, io_scatter_t const *pscatters, ::std::size_t n)
{
	::fast_io::details::scatter_write_all_bytes_impl(outsm, pscatters, n);
}

template <typename outstmtype>
inline constexpr void scatter_write_all_bytes_decay(
	outstmtype outsm, io_scatter_t const *pscatters, ::std::size_t n)
{
	::fast_io::operations::decay::scatter_write_all_bytes_decay_borrowed(outsm, pscatters, n);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void scatter_write_all_bytes_decay_dispatch(
	outstmtype &outsm, io_scatter_t const *pscatters, ::std::size_t n)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		::fast_io::operations::decay::scatter_write_all_bytes_decay(outsm, pscatters, n);
	}
	else
	{
		::fast_io::operations::decay::scatter_write_all_bytes_decay_borrowed(outsm, pscatters, n);
	}
}

template <typename outstmtype>
inline constexpr typename write_decay_stream_t<outstmtype>::output_char_type const *
pwrite_some_decay_borrowed(outstmtype &outsm,
						   typename write_decay_stream_t<outstmtype>::output_char_type const *first,
						   typename write_decay_stream_t<outstmtype>::output_char_type const *last,
						   ::fast_io::intfpos_t off)
{
	return ::fast_io::details::pwrite_some_impl(outsm, first, last, off);
}

template <typename outstmtype>
inline constexpr typename write_decay_stream_t<outstmtype>::output_char_type const *
pwrite_some_decay(outstmtype outsm,
				  typename write_decay_stream_t<outstmtype>::output_char_type const *first,
				  typename write_decay_stream_t<outstmtype>::output_char_type const *last,
				  ::fast_io::intfpos_t off)
{
	return ::fast_io::operations::decay::pwrite_some_decay_borrowed(outsm, first, last, off);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr typename write_decay_stream_t<outstmtype>::output_char_type const *
pwrite_some_decay_dispatch(outstmtype &outsm,
						   typename write_decay_stream_t<outstmtype>::output_char_type const *first,
						   typename write_decay_stream_t<outstmtype>::output_char_type const *last,
						   ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		return ::fast_io::operations::decay::pwrite_some_decay(outsm, first, last, off);
	}
	else
	{
		return ::fast_io::operations::decay::pwrite_some_decay_borrowed(outsm, first, last, off);
	}
}

template <typename outstmtype>
inline constexpr void pwrite_all_decay_borrowed(
	outstmtype &outsm, typename write_decay_stream_t<outstmtype>::output_char_type const *first,
	typename write_decay_stream_t<outstmtype>::output_char_type const *last, ::fast_io::intfpos_t off)
{
	::fast_io::details::pwrite_all_impl(outsm, first, last, off);
}

template <typename outstmtype>
inline constexpr void pwrite_all_decay(
	outstmtype outsm, typename write_decay_stream_t<outstmtype>::output_char_type const *first,
	typename write_decay_stream_t<outstmtype>::output_char_type const *last, ::fast_io::intfpos_t off)
{
	::fast_io::operations::decay::pwrite_all_decay_borrowed(outsm, first, last, off);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void pwrite_all_decay_dispatch(
	outstmtype &outsm, typename write_decay_stream_t<outstmtype>::output_char_type const *first,
	typename write_decay_stream_t<outstmtype>::output_char_type const *last, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		::fast_io::operations::decay::pwrite_all_decay(outsm, first, last, off);
	}
	else
	{
		::fast_io::operations::decay::pwrite_all_decay_borrowed(outsm, first, last, off);
	}
}

template <typename outstmtype>
inline constexpr ::std::byte const *pwrite_some_bytes_decay_borrowed(
	outstmtype &outsm, ::std::byte const *first, ::std::byte const *last, ::fast_io::intfpos_t off)
{
	return ::fast_io::details::pwrite_some_bytes_impl(outsm, first, last, off);
}

template <typename outstmtype>
inline constexpr ::std::byte const *pwrite_some_bytes_decay(
	outstmtype outsm, ::std::byte const *first, ::std::byte const *last, ::fast_io::intfpos_t off)
{
	return ::fast_io::operations::decay::pwrite_some_bytes_decay_borrowed(outsm, first, last, off);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::byte const *pwrite_some_bytes_decay_dispatch(
	outstmtype &outsm, ::std::byte const *first, ::std::byte const *last, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		return ::fast_io::operations::decay::pwrite_some_bytes_decay(outsm, first, last, off);
	}
	else
	{
		return ::fast_io::operations::decay::pwrite_some_bytes_decay_borrowed(outsm, first, last, off);
	}
}

template <typename outstmtype>
inline constexpr void pwrite_all_bytes_decay_borrowed(
	outstmtype &outsm, ::std::byte const *first, ::std::byte const *last, ::fast_io::intfpos_t off)
{
	::fast_io::details::pwrite_all_bytes_impl(outsm, first, last, off);
}

template <typename outstmtype>
inline constexpr void pwrite_all_bytes_decay(
	outstmtype outsm, ::std::byte const *first, ::std::byte const *last, ::fast_io::intfpos_t off)
{
	::fast_io::operations::decay::pwrite_all_bytes_decay_borrowed(outsm, first, last, off);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void pwrite_all_bytes_decay_dispatch(
	outstmtype &outsm, ::std::byte const *first, ::std::byte const *last, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		::fast_io::operations::decay::pwrite_all_bytes_decay(outsm, first, last, off);
	}
	else
	{
		::fast_io::operations::decay::pwrite_all_bytes_decay_borrowed(outsm, first, last, off);
	}
}

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_pwrite_some_decay_borrowed(
	outstmtype &outsm,
	basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t off)
{
	return ::fast_io::details::scatter_pwrite_some_impl(outsm, pscatters, n, off);
}

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_pwrite_some_decay(
	outstmtype outsm,
	basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t off)
{
	return ::fast_io::operations::decay::scatter_pwrite_some_decay_borrowed(outsm, pscatters, n, off);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr io_scatter_status_t scatter_pwrite_some_decay_dispatch(
	outstmtype &outsm,
	basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		return ::fast_io::operations::decay::scatter_pwrite_some_decay(outsm, pscatters, n, off);
	}
	else
	{
		return ::fast_io::operations::decay::scatter_pwrite_some_decay_borrowed(outsm, pscatters, n, off);
	}
}

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_pwrite_some_bytes_decay_borrowed(
	outstmtype &outsm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t off)
{
	return ::fast_io::details::scatter_pwrite_some_bytes_impl(outsm, pscatters, n, off);
}

template <typename outstmtype>
inline constexpr io_scatter_status_t scatter_pwrite_some_bytes_decay(
	outstmtype outsm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t off)
{
	return ::fast_io::operations::decay::scatter_pwrite_some_bytes_decay_borrowed(outsm, pscatters, n, off);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr io_scatter_status_t scatter_pwrite_some_bytes_decay_dispatch(
	outstmtype &outsm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		return ::fast_io::operations::decay::scatter_pwrite_some_bytes_decay(outsm, pscatters, n, off);
	}
	else
	{
		return ::fast_io::operations::decay::scatter_pwrite_some_bytes_decay_borrowed(outsm, pscatters, n, off);
	}
}

template <typename outstmtype>
inline constexpr void scatter_pwrite_all_decay_borrowed(
	outstmtype &outsm,
	basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t off)
{
	::fast_io::details::scatter_pwrite_all_impl(outsm, pscatters, n, off);
}

template <typename outstmtype>
inline constexpr void scatter_pwrite_all_decay(
	outstmtype outsm,
	basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t off)
{
	::fast_io::operations::decay::scatter_pwrite_all_decay_borrowed(outsm, pscatters, n, off);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void scatter_pwrite_all_decay_dispatch(
	outstmtype &outsm,
	basic_io_scatter_t<typename write_decay_stream_t<outstmtype>::output_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		::fast_io::operations::decay::scatter_pwrite_all_decay(outsm, pscatters, n, off);
	}
	else
	{
		::fast_io::operations::decay::scatter_pwrite_all_decay_borrowed(outsm, pscatters, n, off);
	}
}

template <typename outstmtype>
inline constexpr void scatter_pwrite_all_bytes_decay_borrowed(
	outstmtype &outsm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t off)
{
	::fast_io::details::scatter_pwrite_all_bytes_impl(outsm, pscatters, n, off);
}

template <typename outstmtype>
inline constexpr void scatter_pwrite_all_bytes_decay(
	outstmtype outsm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t off)
{
	::fast_io::operations::decay::scatter_pwrite_all_bytes_decay_borrowed(outsm, pscatters, n, off);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void scatter_pwrite_all_bytes_decay_dispatch(
	outstmtype &outsm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		::fast_io::operations::decay::scatter_pwrite_all_bytes_decay(outsm, pscatters, n, off);
	}
	else
	{
		::fast_io::operations::decay::scatter_pwrite_all_bytes_decay_borrowed(outsm, pscatters, n, off);
	}
}

template <typename outstmtype>
inline constexpr void char_put_decay_borrowed(
	outstmtype &outstm, typename write_decay_stream_t<outstmtype>::output_char_type ch)
{
	::fast_io::details::char_put_impl(outstm, ch);
}

template <typename outstmtype>
#if __has_cpp_attribute(__gnu__::__always_inline__)
[[__gnu__::__always_inline__]]
#elif __has_cpp_attribute(msvc::forceinline)
[[msvc::forceinline]]
#endif
inline constexpr void char_put_decay(
	outstmtype outstm, typename write_decay_stream_t<outstmtype>::output_char_type ch)
{
	::fast_io::operations::decay::char_put_decay_borrowed(outstm, ch);
}

template <typename outstmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void char_put_decay_dispatch(
	outstmtype &outstm, typename write_decay_stream_t<outstmtype>::output_char_type ch)
{
	if constexpr (::fast_io::operations::defines::abi_value_output_stream_ref_result<outstmtype &>)
	{
		::fast_io::operations::decay::char_put_decay(outstm, ch);
	}
	else
	{
		::fast_io::operations::decay::char_put_decay_borrowed(outstm, ch);
	}
}

} // namespace fast_io::operations::decay

#pragma once

/*
 * Normalized-observer entry points for primitive input (`operations::decay`).
 *
 * These functions expose the contiguous/scatter, typed/byte, sequential/
 * positioned, some/all matrix after `input_stream_ref` has already selected an
 * owned or borrowed observer representation. Public pointer/span wrappers sit
 * above; raw provider `_define` CPOs sit below.
 */

namespace fast_io::operations::decay
{

template <typename instmtype>
using read_decay_stream_t = ::std::remove_cvref_t<instmtype>;

/*
 * Primitive input has two deliberately distinct transport contracts. The
 * historical `*_decay` spelling is the explicit value owner: a normalized
 * prvalue enters with the platform aggregate ABI and remains alive for the
 * complete primitive. The `_borrowed` spelling preserves the identity of an
 * already-owned cursor, including move-only observers and compact observers
 * whose mutable position is stored in the proxy itself.
 *
 * `_dispatch` is the mandatory-inline bridge for a named normalized observer.
 * It may reopen value transport only when the observer author supplied the ADL
 * `stream_ref_value_transport_safe_define` semantic proof and the target ABI
 * admits its small trivial representation. Size/triviality alone can never
 * authorize the copy. Keeping that decision in the inlined bridge means an
 * outlined value specialization still receives register-class arguments,
 * while every unproved observer reaches the separately named borrowed entry.
 */

template <typename instmtype>
inline constexpr typename read_decay_stream_t<instmtype>::input_char_type *
read_some_decay_borrowed(instmtype &insm, typename read_decay_stream_t<instmtype>::input_char_type *first,
						 typename read_decay_stream_t<instmtype>::input_char_type *last)
{
	return ::fast_io::details::read_some_impl(insm, first, last);
}

template <typename instmtype>
inline constexpr typename read_decay_stream_t<instmtype>::input_char_type *
read_some_decay(instmtype insm, typename read_decay_stream_t<instmtype>::input_char_type *first,
				typename read_decay_stream_t<instmtype>::input_char_type *last)
{
	return ::fast_io::operations::decay::read_some_decay_borrowed(insm, first, last);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr typename read_decay_stream_t<instmtype>::input_char_type *
read_some_decay_dispatch(instmtype &insm, typename read_decay_stream_t<instmtype>::input_char_type *first,
						 typename read_decay_stream_t<instmtype>::input_char_type *last)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		return ::fast_io::operations::decay::read_some_decay(insm, first, last);
	}
	else
	{
		return ::fast_io::operations::decay::read_some_decay_borrowed(insm, first, last);
	}
}

template <typename instmtype>
inline constexpr void read_all_decay_borrowed(instmtype &insm,
											  typename read_decay_stream_t<instmtype>::input_char_type *first,
											  typename read_decay_stream_t<instmtype>::input_char_type *last)
{
	::fast_io::details::read_all_impl(insm, first, last);
}

template <typename instmtype>
inline constexpr void read_all_decay(instmtype insm,
									 typename read_decay_stream_t<instmtype>::input_char_type *first,
									 typename read_decay_stream_t<instmtype>::input_char_type *last)
{
	::fast_io::operations::decay::read_all_decay_borrowed(insm, first, last);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void read_all_decay_dispatch(
	instmtype &insm, typename read_decay_stream_t<instmtype>::input_char_type *first,
	typename read_decay_stream_t<instmtype>::input_char_type *last)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		::fast_io::operations::decay::read_all_decay(insm, first, last);
	}
	else
	{
		::fast_io::operations::decay::read_all_decay_borrowed(insm, first, last);
	}
}

template <typename instmtype>
inline constexpr ::std::byte *read_some_bytes_decay_borrowed(instmtype &insm, ::std::byte *first,
															 ::std::byte *last)
{
	return ::fast_io::details::read_some_bytes_impl(insm, first, last);
}

template <typename instmtype>
inline constexpr ::std::byte *read_some_bytes_decay(instmtype insm, ::std::byte *first, ::std::byte *last)
{
	return ::fast_io::operations::decay::read_some_bytes_decay_borrowed(insm, first, last);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::byte *
read_some_bytes_decay_dispatch(instmtype &insm, ::std::byte *first, ::std::byte *last)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		return ::fast_io::operations::decay::read_some_bytes_decay(insm, first, last);
	}
	else
	{
		return ::fast_io::operations::decay::read_some_bytes_decay_borrowed(insm, first, last);
	}
}

template <typename instmtype>
inline constexpr void read_all_bytes_decay_borrowed(instmtype &insm, ::std::byte *first, ::std::byte *last)
{
	::fast_io::details::read_all_bytes_impl(insm, first, last);
}

template <typename instmtype>
inline constexpr void read_all_bytes_decay(instmtype insm, ::std::byte *first, ::std::byte *last)
{
	::fast_io::operations::decay::read_all_bytes_decay_borrowed(insm, first, last);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void
read_all_bytes_decay_dispatch(instmtype &insm, ::std::byte *first, ::std::byte *last)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		::fast_io::operations::decay::read_all_bytes_decay(insm, first, last);
	}
	else
	{
		::fast_io::operations::decay::read_all_bytes_decay_borrowed(insm, first, last);
	}
}

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_read_some_decay_borrowed(
	instmtype &insm,
	basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
	::std::size_t n)
{
	return ::fast_io::details::scatter_read_some_impl(insm, pscatters, n);
}

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_read_some_decay(
	instmtype insm,
	basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
	::std::size_t n)
{
	return ::fast_io::operations::decay::scatter_read_some_decay_borrowed(insm, pscatters, n);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr io_scatter_status_t scatter_read_some_decay_dispatch(
	instmtype &insm,
	basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
	::std::size_t n)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		return ::fast_io::operations::decay::scatter_read_some_decay(insm, pscatters, n);
	}
	else
	{
		return ::fast_io::operations::decay::scatter_read_some_decay_borrowed(insm, pscatters, n);
	}
}

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_read_some_bytes_decay_borrowed(
	instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n)
{
	return ::fast_io::details::scatter_read_some_bytes_impl(insm, pscatters, n);
}

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_read_some_bytes_decay(
	instmtype insm, io_scatter_t const *pscatters, ::std::size_t n)
{
	return ::fast_io::operations::decay::scatter_read_some_bytes_decay_borrowed(insm, pscatters, n);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr io_scatter_status_t scatter_read_some_bytes_decay_dispatch(
	instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		return ::fast_io::operations::decay::scatter_read_some_bytes_decay(insm, pscatters, n);
	}
	else
	{
		return ::fast_io::operations::decay::scatter_read_some_bytes_decay_borrowed(insm, pscatters, n);
	}
}

template <typename instmtype>
inline constexpr void scatter_read_all_decay_borrowed(
	instmtype &insm,
	basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
	::std::size_t n)
{
	::fast_io::details::scatter_read_all_impl(insm, pscatters, n);
}

template <typename instmtype>
inline constexpr void scatter_read_all_decay(
	instmtype insm,
	basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
	::std::size_t n)
{
	::fast_io::operations::decay::scatter_read_all_decay_borrowed(insm, pscatters, n);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void scatter_read_all_decay_dispatch(
	instmtype &insm,
	basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
	::std::size_t n)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		::fast_io::operations::decay::scatter_read_all_decay(insm, pscatters, n);
	}
	else
	{
		::fast_io::operations::decay::scatter_read_all_decay_borrowed(insm, pscatters, n);
	}
}

template <typename instmtype>
inline constexpr void scatter_read_all_bytes_decay_borrowed(
	instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n)
{
	::fast_io::details::scatter_read_all_bytes_impl(insm, pscatters, n);
}

template <typename instmtype>
inline constexpr void scatter_read_all_bytes_decay(
	instmtype insm, io_scatter_t const *pscatters, ::std::size_t n)
{
	::fast_io::operations::decay::scatter_read_all_bytes_decay_borrowed(insm, pscatters, n);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void scatter_read_all_bytes_decay_dispatch(
	instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		::fast_io::operations::decay::scatter_read_all_bytes_decay(insm, pscatters, n);
	}
	else
	{
		::fast_io::operations::decay::scatter_read_all_bytes_decay_borrowed(insm, pscatters, n);
	}
}

template <typename instmtype>
inline constexpr typename read_decay_stream_t<instmtype>::input_char_type *
pread_some_decay_borrowed(instmtype &insm, typename read_decay_stream_t<instmtype>::input_char_type *first,
						  typename read_decay_stream_t<instmtype>::input_char_type *last,
						  ::fast_io::intfpos_t off)
{
	return ::fast_io::details::pread_some_impl(insm, first, last, off);
}

template <typename instmtype>
inline constexpr typename read_decay_stream_t<instmtype>::input_char_type *
pread_some_decay(instmtype insm, typename read_decay_stream_t<instmtype>::input_char_type *first,
				 typename read_decay_stream_t<instmtype>::input_char_type *last,
				 ::fast_io::intfpos_t off)
{
	return ::fast_io::operations::decay::pread_some_decay_borrowed(insm, first, last, off);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr typename read_decay_stream_t<instmtype>::input_char_type *
pread_some_decay_dispatch(instmtype &insm, typename read_decay_stream_t<instmtype>::input_char_type *first,
						  typename read_decay_stream_t<instmtype>::input_char_type *last,
						  ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		return ::fast_io::operations::decay::pread_some_decay(insm, first, last, off);
	}
	else
	{
		return ::fast_io::operations::decay::pread_some_decay_borrowed(insm, first, last, off);
	}
}

template <typename instmtype>
inline constexpr void pread_all_decay_borrowed(
	instmtype &insm, typename read_decay_stream_t<instmtype>::input_char_type *first,
	typename read_decay_stream_t<instmtype>::input_char_type *last, ::fast_io::intfpos_t off)
{
	::fast_io::details::pread_all_impl(insm, first, last, off);
}

template <typename instmtype>
inline constexpr void pread_all_decay(
	instmtype insm, typename read_decay_stream_t<instmtype>::input_char_type *first,
	typename read_decay_stream_t<instmtype>::input_char_type *last, ::fast_io::intfpos_t off)
{
	::fast_io::operations::decay::pread_all_decay_borrowed(insm, first, last, off);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void pread_all_decay_dispatch(
	instmtype &insm, typename read_decay_stream_t<instmtype>::input_char_type *first,
	typename read_decay_stream_t<instmtype>::input_char_type *last, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		::fast_io::operations::decay::pread_all_decay(insm, first, last, off);
	}
	else
	{
		::fast_io::operations::decay::pread_all_decay_borrowed(insm, first, last, off);
	}
}

template <typename instmtype>
inline constexpr ::std::byte *pread_some_bytes_decay_borrowed(
	instmtype &insm, ::std::byte *first, ::std::byte *last, ::fast_io::intfpos_t off)
{
	return ::fast_io::details::pread_some_bytes_impl(insm, first, last, off);
}

template <typename instmtype>
inline constexpr ::std::byte *pread_some_bytes_decay(
	instmtype insm, ::std::byte *first, ::std::byte *last, ::fast_io::intfpos_t off)
{
	return ::fast_io::operations::decay::pread_some_bytes_decay_borrowed(insm, first, last, off);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr ::std::byte *pread_some_bytes_decay_dispatch(
	instmtype &insm, ::std::byte *first, ::std::byte *last, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		return ::fast_io::operations::decay::pread_some_bytes_decay(insm, first, last, off);
	}
	else
	{
		return ::fast_io::operations::decay::pread_some_bytes_decay_borrowed(insm, first, last, off);
	}
}

template <typename instmtype>
inline constexpr void pread_all_bytes_decay_borrowed(
	instmtype &insm, ::std::byte *first, ::std::byte *last, ::fast_io::intfpos_t off)
{
	::fast_io::details::pread_all_bytes_impl(insm, first, last, off);
}

template <typename instmtype>
inline constexpr void pread_all_bytes_decay(
	instmtype insm, ::std::byte *first, ::std::byte *last, ::fast_io::intfpos_t off)
{
	::fast_io::operations::decay::pread_all_bytes_decay_borrowed(insm, first, last, off);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void pread_all_bytes_decay_dispatch(
	instmtype &insm, ::std::byte *first, ::std::byte *last, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		::fast_io::operations::decay::pread_all_bytes_decay(insm, first, last, off);
	}
	else
	{
		::fast_io::operations::decay::pread_all_bytes_decay_borrowed(insm, first, last, off);
	}
}

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_pread_some_decay_borrowed(
	instmtype &insm,
	basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t off)
{
	return ::fast_io::details::scatter_pread_some_impl(insm, pscatters, n, off);
}

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_pread_some_decay(
	instmtype insm,
	basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t off)
{
	return ::fast_io::operations::decay::scatter_pread_some_decay_borrowed(insm, pscatters, n, off);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr io_scatter_status_t scatter_pread_some_decay_dispatch(
	instmtype &insm,
	basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		return ::fast_io::operations::decay::scatter_pread_some_decay(insm, pscatters, n, off);
	}
	else
	{
		return ::fast_io::operations::decay::scatter_pread_some_decay_borrowed(insm, pscatters, n, off);
	}
}

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_pread_some_bytes_decay_borrowed(
	instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t off)
{
	return ::fast_io::details::scatter_pread_some_bytes_impl(insm, pscatters, n, off);
}

template <typename instmtype>
inline constexpr io_scatter_status_t scatter_pread_some_bytes_decay(
	instmtype insm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t off)
{
	return ::fast_io::operations::decay::scatter_pread_some_bytes_decay_borrowed(insm, pscatters, n, off);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr io_scatter_status_t scatter_pread_some_bytes_decay_dispatch(
	instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		return ::fast_io::operations::decay::scatter_pread_some_bytes_decay(insm, pscatters, n, off);
	}
	else
	{
		return ::fast_io::operations::decay::scatter_pread_some_bytes_decay_borrowed(insm, pscatters, n, off);
	}
}

template <typename instmtype>
inline constexpr void scatter_pread_all_decay_borrowed(
	instmtype &insm,
	basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t off)
{
	::fast_io::details::scatter_pread_all_impl(insm, pscatters, n, off);
}

template <typename instmtype>
inline constexpr void scatter_pread_all_decay(
	instmtype insm,
	basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t off)
{
	::fast_io::operations::decay::scatter_pread_all_decay_borrowed(insm, pscatters, n, off);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void scatter_pread_all_decay_dispatch(
	instmtype &insm,
	basic_io_scatter_t<typename read_decay_stream_t<instmtype>::input_char_type> const *pscatters,
	::std::size_t n, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		::fast_io::operations::decay::scatter_pread_all_decay(insm, pscatters, n, off);
	}
	else
	{
		::fast_io::operations::decay::scatter_pread_all_decay_borrowed(insm, pscatters, n, off);
	}
}

template <typename instmtype>
inline constexpr void scatter_pread_all_bytes_decay_borrowed(
	instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t off)
{
	::fast_io::details::scatter_pread_all_bytes_impl(insm, pscatters, n, off);
}

template <typename instmtype>
inline constexpr void scatter_pread_all_bytes_decay(
	instmtype insm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t off)
{
	::fast_io::operations::decay::scatter_pread_all_bytes_decay_borrowed(insm, pscatters, n, off);
}

template <typename instmtype>
FAST_IO_GNU_ALWAYS_INLINE inline constexpr void scatter_pread_all_bytes_decay_dispatch(
	instmtype &insm, io_scatter_t const *pscatters, ::std::size_t n, ::fast_io::intfpos_t off)
{
	if constexpr (::fast_io::operations::defines::abi_value_input_stream_ref_result<instmtype &>)
	{
		::fast_io::operations::decay::scatter_pread_all_bytes_decay(insm, pscatters, n, off);
	}
	else
	{
		::fast_io::operations::decay::scatter_pread_all_bytes_decay_borrowed(insm, pscatters, n, off);
	}
}

} // namespace fast_io::operations::decay
